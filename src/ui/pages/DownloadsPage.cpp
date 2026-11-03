#include "DownloadsPage.h"

#include "net/TorrentEngine.h"
#include "net/ExternalDownloader.h"
#include "net/DownloaderRpc.h"
#include "core/AppConfig.h"
#include "core/LocalLibrary.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Overlays.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>

namespace {
class ProgressDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->fillRect(option.rect,QColor(option.state & QStyle::State_Selected ? Theme::ACCENT_SOFT : Theme::SURFACE));
        p->setPen(QColor(Theme::BORDER)); p->drawLine(option.rect.bottomLeft(),option.rect.bottomRight());
        const double value=index.data(Qt::UserRole+1).toDouble();
        if(value<0){p->setPen(QColor(Theme::TEXT3));p->drawText(option.rect.adjusted(12,0,-10,0),Qt::AlignVCenter,QStringLiteral("由外部管理"));return;}
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const QRectF track=QRectF(option.rect).adjusted(12,32,-12,-14);
        p->setPen(Qt::NoPen); p->setBrush(QColor(Theme::SURFACE3)); p->drawRoundedRect(track,2,2);
        QRectF fill=track; fill.setWidth(track.width()*qBound(0.0,value,100.0)/100.0);
        p->setBrush(QColor(Theme::ACCENT)); p->drawRoundedRect(fill,2,2);
        p->setPen(QColor(Theme::TEXT2));
        p->drawText(option.rect.adjusted(12,0,-10,-12),Qt::AlignVCenter,QString::number(value,'f',1)+"%");
        p->restore();
    }
};

QString fmtSpeed(qint64 bytes)
{
    if (bytes >= (1LL << 20))
        return QStringLiteral("%1 MB/s").arg(bytes / 1048576.0, 0, 'f', 1);
    if (bytes >= (1LL << 10))
        return QStringLiteral("%1 KB/s").arg(bytes / 1024.0, 0, 'f', 0);
    return QStringLiteral("%1 B/s").arg(bytes);
}

QString fmtEta(qint64 seconds)
{
    if (seconds < 0)
        return QStringLiteral("—");
    if (seconds >= 86400)
        return QStringLiteral("%1d").arg(seconds / 86400);
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

} // namespace

DownloadsPage::DownloadsPage(TorrentEngine *engine,LocalLibrary *library,AppConfig *config,QWidget *parent)
    : QWidget(parent), m_library(library),m_config(config),m_engine(engine)
{
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(32,24,32,20); layout->setSpacing(18);
    auto *header=new QHBoxLayout;
    header->addWidget(pageHeading(QStringLiteral("下载任务"),QStringLiteral("所有传输，进度一目了然。")),1);
    m_backendButton=new QPushButton(this);m_backendButton->setObjectName("ChooseDownloader");m_backendButton->setIcon(Theme::icon("download"));
    connect(m_backendButton,&QPushButton::clicked,this,[this]{emit switchDownloaderRequested(m_backendButton);});
    header->addWidget(m_backendButton);updateDownloader();
    auto *openDirectory=new QPushButton(QStringLiteral("打开下载目录"),this);
    connect(openDirectory,&QPushButton::clicked,this,&DownloadsPage::openDirectoryRequested);
    header->addWidget(openDirectory); layout->addLayout(header);
    auto *metrics=new QHBoxLayout; metrics->setSpacing(16);
    m_taskCount=metricCard(metrics,QStringLiteral("任务总数"));
    m_downSpeed=metricCard(metrics,QStringLiteral("下载速度"));
    m_upSpeed=metricCard(metrics,QStringLiteral("上传速度")); layout->addLayout(metrics);
    auto *toolbar=new QHBoxLayout; toolbar->setSpacing(8);
    m_status=label(QString(),"Faint",false,this); toolbar->addWidget(m_status,1);
    m_openClient=new QPushButton(QStringLiteral("打开下载器"),this);m_openClient->setEnabled(false);toolbar->addWidget(m_openClient);
    connect(m_openClient,&QPushButton::clicked,this,[this]{
        const auto item=m_library->item(selectedHash().mid(9));if(!item)return;
        QString error;ExternalCommand command;command.program=item->externalProgram;command.directory=item->savePath;command.console=item->downloader=="aria2";
        if(item->downloader=="aria2"){Ui::message(this,QStringLiteral("aria2 后台任务"),QStringLiteral("aria2 作为后台进程运行。可在任务保存目录查看文件及 .aria2 进度文件。"));return;}
        if(!ExternalDownloader::start(command,&error))Ui::message(this,QStringLiteral("无法打开下载器"),error,Ui::Notice::Warning);
    });
    const QStringList names={QStringLiteral("暂停"),QStringLiteral("恢复"),QStringLiteral("移除任务")};
    for(int i=0;i<names.size();++i) {
        auto *b=new QPushButton(names.at(i),this);
        if(i==2)b->setObjectName("Danger");
        toolbar->addWidget(b); m_actions<<b; b->setEnabled(false);
    }
    connect(m_actions[0],&QPushButton::clicked,this,&DownloadsPage::pauseSelected);
    connect(m_actions[1],&QPushButton::clicked,this,&DownloadsPage::resumeSelected);
    connect(m_actions[2],&QPushButton::clicked,this,&DownloadsPage::deleteSelected);
    layout->addLayout(toolbar);
    auto *results=new QGridLayout; results->setContentsMargins(0,0,0,0);
    m_table=new QTableWidget(this); m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("任务名称"),QStringLiteral("状态"),QStringLiteral("进度"),
        QStringLiteral("下载"),QStringLiteral("上传"),QStringLiteral("大小"),QStringLiteral("做种 / 连接"),QStringLiteral("剩余时间")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection); m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    m_table->setColumnWidth(2,125); prepareTable(m_table);
    m_table->setItemDelegateForColumn(2,new ProgressDelegate(m_table));
    connect(m_table,&QTableWidget::itemSelectionChanged,this,[this]{
        const QString key=selectedHash();const bool external=key.startsWith("external:");
        const auto item=external?m_library->item(key.mid(9)):std::nullopt;
        const bool managed=item&&m_rpc&&m_rpc->manageable(*item);
        m_actions[0]->setEnabled(!key.isEmpty()&&(!external||managed));m_actions[1]->setEnabled(!key.isEmpty()&&(!external||managed));
        m_actions[2]->setEnabled(!key.isEmpty());m_actions[2]->setText(external&&!managed?QStringLiteral("移除记录"):QStringLiteral("移除任务"));m_openClient->setEnabled(external);
    });
    results->addWidget(m_table,0,0);
    m_empty=new EmptyState(Icons::DOWNLOAD,QStringLiteral("还没有下载任务"),QStringLiteral("在番剧详情中选择资源，点击「下载」开始传输。"),this);
    results->addWidget(m_empty,0,0); layout->addLayout(results,1);
    layout->addWidget(label(QStringLiteral("选中任务后可以暂停、恢复或移除；完成后的传输行为可在偏好设置中调整。"),"Faint"));
    m_timer=new QTimer(this); m_timer->setInterval(1500);
    connect(m_timer,&QTimer::timeout,this,&DownloadsPage::refresh);
}

void DownloadsPage::setRpc(DownloaderRpc *rpc)
{
    m_rpc=rpc;connect(rpc,&DownloaderRpc::changed,this,[this]{if(isVisible())refresh();});
}

void DownloadsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_timer->start();
    updateDownloader();refresh();
}

void DownloadsPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();
}

void DownloadsPage::refresh()
{
    const QString previous = selectedHash();
    auto tasks = m_engine->listTorrents();
    const auto remote=m_rpc?m_rpc->tasks():QHash<QString,DownloadTask>{};
    for(const auto &i:m_library->items())if(i.downloader!="builtin"&&(i.phase=="external"||i.phase=="external_paused"||i.phase=="external_pending"||i.phase=="error"||i.phase=="complete")) {
        if(remote.contains(i.key)){auto t=remote.value(i.key);t.title=i.resource.title;t.qbtHash="external:"+i.key;tasks.append(t);continue;}
        DownloadTask t;t.title=i.resource.title;t.qbtHash="external:"+i.key;t.progress=i.complete?100:-1;
        t.statusMessage=(i.phase=="complete"?QStringLiteral("已校验完成 · "):i.phase=="error"?QStringLiteral("发送失败 · "):i.phase=="external_pending"?QStringLiteral("正在发送 · "):QStringLiteral("已发送 · "))+DownloaderSettings::name(i.downloader);
        t.status=DownloadStatus::Pending;t.size=i.resource.fileSize;tasks.append(t);
    }
    qint64 down=0, up=0;
    m_empty->setVisible(tasks.isEmpty());
    m_table->setRowCount(tasks.size());
    for (int row = 0; row < tasks.size(); ++row) {
        const auto &task = tasks.at(row);
        down+=task.dlSpeed; up+=task.upSpeed;
        auto set = [&](int column, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setData(Qt::UserRole, task.qbtHash);
            m_table->setItem(row, column, item);
        };
        set(0, task.title);
        set(1, task.statusMessage.isEmpty() ? downloadStatusName(task.status) : task.statusMessage);
        m_table->item(row, 1)->setToolTip(task.statusMessage);
        set(2, QString());
        m_table->item(row,2)->setData(Qt::UserRole+1,task.progress);
        const bool external=task.qbtHash.startsWith("external:")&&!remote.contains(task.qbtHash.mid(9));
        set(3,external?QStringLiteral("—"):fmtSpeed(task.dlSpeed));
        set(4,external?QStringLiteral("—"):fmtSpeed(task.upSpeed));
        set(5, fmtSize(task.size));
        set(6,external?QStringLiteral("—"):QStringLiteral("%1/%2").arg(task.seeders).arg(task.leechers));
        set(7, fmtEta(task.eta));
        if(task.qbtHash==previous)m_table->selectRow(row);
    }
    m_taskCount->setText(QString::number(tasks.size()));
    m_downSpeed->setText(fmtSpeed(down)); m_upSpeed->setText(fmtSpeed(up));
    m_status->setText(QStringLiteral("内置与已连接的外部任务可在此管理；未连接的外部任务请在对应软件中操作。"));
    m_status->setToolTip(m_engine->version());

}

QString DownloadsPage::selectedHash() const
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return {};
    const auto *item = m_table->item(rows.first().row(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void DownloadsPage::pauseSelected()
{
    if(selectedHash().startsWith("external:")){if(m_rpc)m_rpc->control(selectedHash().mid(9),"pause");return;}
    if (const auto hash = selectedHash(); !hash.isEmpty())
        m_engine->pause(hash);
}

void DownloadsPage::resumeSelected()
{
    if(selectedHash().startsWith("external:")){if(m_rpc)m_rpc->control(selectedHash().mid(9),"resume");return;}
    if (const auto hash = selectedHash(); !hash.isEmpty())
        m_engine->resume(hash);
}

void DownloadsPage::deleteSelected()
{
    const auto hash = selectedHash();
    if (hash.isEmpty())
        return;
    if(hash.startsWith("external:")) {
        auto item=m_library->item(hash.mid(9));if(!item)return;
        if(m_rpc&&m_rpc->manageable(*item)){m_rpc->control(item->key,"remove");return;}
        item->phase="cancelled";QString error;
        if(!m_library->put(*item,&error))Ui::message(this,QStringLiteral("无法移除记录"),error,Ui::Notice::Warning);
        else Ui::message(this,QStringLiteral("记录已移除"),QStringLiteral("对应下载器中的任务仍会继续；需要取消时请在该软件中操作。"));
        refresh();return;
    }
    const auto choice = Ui::confirmRemoval(this);
    if (choice == Ui::Removal::Cancel)
        return;
    m_engine->remove(hash, choice == Ui::Removal::DeleteFiles);
}

void DownloadsPage::updateDownloader()
{m_backendButton->setText(DownloaderSettings::name(m_config->downloader.selected)+QStringLiteral(" ▾"));}

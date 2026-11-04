#include "DownloadsPage.h"

#include "net/TorrentEngine.h"
#include "net/ExternalDownloader.h"
#include "net/DownloaderRpc.h"
#include "core/AppConfig.h"
#include "core/LocalLibrary.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Controls.h"
#include "ui/Overlays.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QItemSelection>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
enum Column { TaskColumn, ProgressColumn, SpeedColumn, SizeColumn, PeersColumn, EtaColumn, ColumnCount };
enum Role { KeyRole = Qt::UserRole, ProgressRole, DetailRole, ToneRole, PathRole };

QColor toneColor(const QString &tone)
{
    if (tone == "active") return Theme::c().accentText;
    if (tone == "done") return Theme::c().ok;
    if (tone == "error") return Theme::c().err;
    if (tone == "wait") return Theme::c().warn;
    return Theme::c().text3;
}

QString statusTone(const DownloadTask &task)
{
    switch (task.status) {
    case DownloadStatus::Downloading: return QStringLiteral("active");
    case DownloadStatus::Seeding:
    case DownloadStatus::Completed: return QStringLiteral("done");
    case DownloadStatus::Error: return QStringLiteral("error");
    case DownloadStatus::Pending: return QStringLiteral("wait");
    case DownloadStatus::Paused: break;
    }
    return QStringLiteral("muted");
}

// Background, selection and the bottom rule shared by the custom cells.
void paintCellBase(QPainter *p, const QStyleOptionViewItem &option)
{
    p->fillRect(option.rect, QColor(option.state & QStyle::State_Selected ? Theme::c().accentSoft : Theme::c().surface));
    p->setPen(QColor(Theme::c().surface3));
    p->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
}

// Primary text with a smaller, tinted second line (status or upload speed).
class TwoLineDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save();
        paintCellBase(p, option);
        const QRect area = option.rect.adjusted(12, 7, -10, -7);
        QFont primary = option.font;
        if (index.column() == TaskColumn) primary.setWeight(QFont::DemiBold);
        p->setFont(primary);
        p->setPen(QColor(Theme::c().text));
        const QFontMetrics fm(primary);
        p->drawText(QRect(area.left(), area.top(), area.width(), area.height() / 2), Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(index.data().toString(), Qt::ElideMiddle, area.width()));
        QFont secondary = option.font; secondary.setPointSizeF(option.font.pointSizeF() - 1);
        p->setFont(secondary);
        p->setPen(QColor(toneColor(index.data(ToneRole).toString())));
        p->drawText(QRect(area.left(), area.center().y(), area.width(), area.height() / 2), Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(secondary).elidedText(index.data(DetailRole).toString(), Qt::ElideRight, area.width()));
        p->restore();
    }
};

class ProgressDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save();
        paintCellBase(p, option);
        const double value = index.data(ProgressRole).toDouble();
        const QRect area = option.rect.adjusted(12, 0, -14, 0);
        if (value < 0) {
            p->setPen(QColor(Theme::c().text3));
            p->drawText(area, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("由外部管理"));
            p->restore();
            return;
        }
        p->setRenderHint(QPainter::Antialiasing);
        const QString text = QString::number(value, 'f', value >= 100 ? 0 : 1) + "%";
        const int textWidth = option.fontMetrics.horizontalAdvance(QStringLiteral("100.0%")) + 6;
        const QRectF track(area.left(), option.rect.center().y() - 2.5, area.width() - textWidth, 5);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(Theme::c().surface3)); p->drawRoundedRect(track, 2.5, 2.5);
        QRectF fill = track; fill.setWidth(track.width() * qBound(0.0, value, 100.0) / 100.0);
        p->setBrush(QColor(value >= 100 ? Theme::c().ok : toneColor(index.data(ToneRole).toString()) == Theme::c().text3 ? Theme::c().borderStrong : Theme::c().accent));
        if (fill.width() > 0) p->drawRoundedRect(fill, 2.5, 2.5);
        p->setPen(QColor(Theme::c().text2));
        p->drawText(QRect(int(track.right()) + 6, option.rect.top(), textWidth, option.rect.height()),
                    Qt::AlignVCenter | Qt::AlignRight, text);
        p->restore();
    }
};

QString fmtEta(qint64 seconds)
{
    if (seconds < 0)
        return QStringLiteral("—");
    if (seconds >= 86400)
        return QStringLiteral("%1 天").arg(seconds / 86400);
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (hours > 0)
        return QStringLiteral("%1 时 %2 分").arg(hours).arg(minutes);
    if (minutes > 0)
        return QStringLiteral("%1 分 %2 秒").arg(minutes).arg(seconds % 60);
    return QStringLiteral("%1 秒").arg(seconds);
}

} // namespace

DownloadsPage::DownloadsPage(TorrentEngine *engine,LocalLibrary *library,AppConfig *config,QWidget *parent)
    : QWidget(parent), m_library(library),m_config(config),m_engine(engine)
{
    auto *layout=pageLayout(this);
    auto header=pageHeader(QStringLiteral("下载任务"),this);
    m_summary=header.meta;
    m_backendButton=new QPushButton(this);m_backendButton->setObjectName("ChooseDownloader");m_backendButton->setIcon(Theme::icon("download"));
    m_backendButton->setCursor(Qt::PointingHandCursor);m_backendButton->setToolTip(QStringLiteral("切换新任务使用的下载器"));
    connect(m_backendButton,&QPushButton::clicked,this,[this]{emit switchDownloaderRequested(m_backendButton);});
    header.actions->addWidget(m_backendButton);updateDownloader();
    auto *openDirectory=new QPushButton(QStringLiteral("打开下载目录"),this);openDirectory->setIcon(Theme::icon("folder"));
    connect(openDirectory,&QPushButton::clicked,this,&DownloadsPage::openDirectoryRequested);
    header.actions->addWidget(openDirectory); layout->addWidget(header.widget);

    auto *metrics=new QHBoxLayout; metrics->setSpacing(12);
    m_taskCount=statTile(metrics,"pulse",QStringLiteral("进行中"));
    m_downSpeed=statTile(metrics,"download",QStringLiteral("下载速度"));
    m_upSpeed=statTile(metrics,"export",QStringLiteral("上传速度")); layout->addLayout(metrics);

    auto *toolbar=new QHBoxLayout; toolbar->setSpacing(8);
    m_status=label(QStringLiteral("按住 Ctrl 或 Shift 可多选，右键查看更多选项"),"Faint",false,this); toolbar->addWidget(m_status,1);
    m_openClient=new QPushButton(QStringLiteral("打开下载器"),this);m_openClient->setIcon(Theme::icon("external"));m_openClient->hide();toolbar->addWidget(m_openClient);
    connect(m_openClient,&QPushButton::clicked,this,&DownloadsPage::openExternalClient);
    const QStringList names={QStringLiteral("暂停"),QStringLiteral("恢复"),QStringLiteral("移除任务")};
    const QStringList icons={"pause","play","trash"};
    for(int i=0;i<names.size();++i) {
        auto *b=new QPushButton(names.at(i),this); b->setIcon(Theme::icon(icons.at(i)));
        if(i==2)b->setObjectName("Danger");
        toolbar->addWidget(b); m_actions<<b; b->setEnabled(false);
    }
    connect(m_actions[0],&QPushButton::clicked,this,&DownloadsPage::pauseSelected);
    connect(m_actions[1],&QPushButton::clicked,this,&DownloadsPage::resumeSelected);
    connect(m_actions[2],&QPushButton::clicked,this,&DownloadsPage::deleteSelected);
    layout->addLayout(toolbar);

    m_table=new QTableWidget(this); m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("任务"),QStringLiteral("进度"),QStringLiteral("速度"),
        QStringLiteral("大小"),QStringLiteral("做种 / 连接"),QStringLiteral("剩余时间")});
    prepareTable(m_table);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->verticalHeader()->setDefaultSectionSize(58);
    m_table->horizontalHeader()->setSectionResizeMode(TaskColumn,QHeaderView::Stretch);
    m_table->setColumnWidth(ProgressColumn,190); m_table->setColumnWidth(SpeedColumn,120);
    m_table->setColumnWidth(SizeColumn,96); m_table->setColumnWidth(PeersColumn,100); m_table->setColumnWidth(EtaColumn,104);
    auto *twoLine=new TwoLineDelegate(m_table);
    m_table->setItemDelegateForColumn(TaskColumn,twoLine);
    m_table->setItemDelegateForColumn(SpeedColumn,twoLine);
    m_table->setItemDelegateForColumn(ProgressColumn,new ProgressDelegate(m_table));
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table,&QTableWidget::customContextMenuRequested,this,&DownloadsPage::showContextMenu);
    connect(m_table,&QTableWidget::itemSelectionChanged,this,&DownloadsPage::updateActions);
    m_empty=new EmptyState("download",QStringLiteral("还没有下载任务"),QStringLiteral("在番剧详情中选择资源，点击「下载」开始传输。"),this);
    layout->addWidget(overlayStack(m_table,m_empty,this),1);

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
    const QStringList previous = selectedHashes();
    auto tasks = m_engine->listTorrents();
    const auto remote=m_rpc?m_rpc->tasks():QHash<QString,DownloadTask>{};
    static const QSet<QString> externalPhases={"external","external_paused","external_pending","error","complete"};
    for(const auto &i:m_library->items())if(i.downloader!="builtin"&&externalPhases.contains(i.phase)) {
        if(remote.contains(i.key)){auto t=remote.value(i.key);t.title=i.resource.title;t.qbtHash="external:"+i.key;t.savePath=i.savePath;tasks.append(t);continue;}
        DownloadTask t;t.title=i.resource.title;t.qbtHash="external:"+i.key;t.progress=i.complete?100:-1;t.savePath=i.savePath;
        t.statusMessage=(i.phase=="complete"?QStringLiteral("已校验完成 · "):i.phase=="error"?QStringLiteral("发送失败 · "):i.phase=="external_pending"?QStringLiteral("正在发送 · "):QStringLiteral("已发送 · "))+DownloaderSettings::name(i.downloader);
        t.status=i.phase=="complete"?DownloadStatus::Completed:i.phase=="error"?DownloadStatus::Error:DownloadStatus::Pending;
        t.size=i.resource.fileSize;tasks.append(t);
    }

    qint64 down=0, up=0; int active=0;
    m_empty->setVisible(tasks.isEmpty());
    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(tasks.size());
    QList<int> keepSelected;
    for (int row = 0; row < tasks.size(); ++row) {
        const auto &task = tasks.at(row);
        down+=task.dlSpeed; up+=task.upSpeed;
        if(task.status==DownloadStatus::Downloading||task.status==DownloadStatus::Pending)++active;
        const bool external=task.qbtHash.startsWith("external:")&&!remote.contains(task.qbtHash.mid(9));
        const QString tone=statusTone(task);
        const QString status=task.statusMessage.isEmpty()?downloadStatusName(task.status):task.statusMessage;
        // Items are reused across refreshes so the table does not churn every 1.5 seconds.
        auto cell=[&](int column)->QTableWidgetItem * {
            auto *item=m_table->item(row,column);
            if(!item){item=new QTableWidgetItem;m_table->setItem(row,column,item);}
            item->setData(KeyRole,task.qbtHash);
            return item;
        };
        auto *name=cell(TaskColumn);
        name->setText(task.title); name->setData(DetailRole,status); name->setData(ToneRole,tone);
        name->setData(PathRole,task.savePath); name->setToolTip(task.title+"\n"+status);
        auto *progress=cell(ProgressColumn);
        progress->setData(ProgressRole,task.progress); progress->setData(ToneRole,tone);
        auto *speed=cell(SpeedColumn);
        speed->setText(external?QStringLiteral("—"):QStringLiteral("↓ ")+fmtSpeed(task.dlSpeed));
        speed->setData(DetailRole,external?QString():QStringLiteral("↑ ")+fmtSpeed(task.upSpeed));
        speed->setData(ToneRole,QStringLiteral("muted"));
        cell(SizeColumn)->setText(fmtSize(task.size));
        cell(PeersColumn)->setText(external?QStringLiteral("—"):QStringLiteral("%1 / %2").arg(task.seeders).arg(task.leechers));
        cell(EtaColumn)->setText(task.status==DownloadStatus::Downloading?fmtEta(task.eta):QStringLiteral("—"));
        if(previous.contains(task.qbtHash))keepSelected.append(row);
    }
    m_table->setUpdatesEnabled(true);
    // Rows can move between refreshes; restore the selection by task identity.
    if(selectedHashes()!=previous) {
        QItemSelection selection;
        for(int row:keepSelected)selection.select(m_table->model()->index(row,0),m_table->model()->index(row,ColumnCount-1));
        m_table->selectionModel()->select(selection,QItemSelectionModel::ClearAndSelect);
    }
    m_taskCount->setText(QString::number(active));
    m_downSpeed->setText(fmtSpeed(down)); m_upSpeed->setText(fmtSpeed(up));
    m_summary->setText(tasks.isEmpty()?QString():QStringLiteral("共 %1 个任务").arg(tasks.size()));
    m_summary->setToolTip(m_engine->version());
    updateActions();
}

void DownloadsPage::updateActions()
{
    const auto keys=selectedHashes();
    int controllable=0,unmanaged=0;
    for(const auto &key:keys) {
        if(!key.startsWith("external:")){++controllable;continue;}
        const auto item=m_library->item(key.mid(9));
        if(item&&m_rpc&&m_rpc->manageable(*item))++controllable;else ++unmanaged;
    }
    m_actions[0]->setEnabled(controllable>0);m_actions[1]->setEnabled(controllable>0);
    m_actions[2]->setEnabled(!keys.isEmpty());
    if(keys.size()>1)m_actions[2]->setText(QStringLiteral("移除 %1 项").arg(keys.size()));
    else m_actions[2]->setText(unmanaged?QStringLiteral("移除记录"):QStringLiteral("移除任务"));
    m_openClient->setVisible(keys.size()==1&&keys.first().startsWith("external:"));
    m_status->setText(keys.size()>1?QStringLiteral("已选择 %1 个任务").arg(keys.size())
                                    :QStringLiteral("按住 Ctrl 或 Shift 可多选，右键查看更多选项"));
}

QStringList DownloadsPage::selectedHashes() const
{
    QStringList keys;
    for(const auto &index:m_table->selectionModel()->selectedRows())
        if(const auto *item=m_table->item(index.row(),TaskColumn))keys.append(item->data(KeyRole).toString());
    return keys;
}

QString DownloadsPage::selectedHash() const
{
    return selectedHashes().value(0);
}

void DownloadsPage::showContextMenu(const QPoint &position)
{
    const int row=m_table->rowAt(position.y());
    if(row<0)return;
    // Right-clicking outside the selection acts on that row alone.
    if(!m_table->selectionModel()->isRowSelected(row,{}))m_table->selectRow(row);
    const auto *item=m_table->item(row,TaskColumn);
    QMenu menu(this);
    menu.addAction(Theme::icon("pause"),m_actions[0]->text(),this,&DownloadsPage::pauseSelected)->setEnabled(m_actions[0]->isEnabled());
    menu.addAction(Theme::icon("play"),m_actions[1]->text(),this,&DownloadsPage::resumeSelected)->setEnabled(m_actions[1]->isEnabled());
    menu.addSeparator();
    const QString path=item?item->data(PathRole).toString():QString();
    menu.addAction(Theme::icon("folder"),QStringLiteral("打开所在文件夹"),this,[path]{
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    })->setEnabled(!path.isEmpty()&&QDir(path).exists());
    const QString title=item?item->text():QString();
    menu.addAction(Theme::icon("copy"),QStringLiteral("复制任务名称"),this,[title]{QApplication::clipboard()->setText(title);});
    menu.addSeparator();
    menu.addAction(Theme::icon("trash",&Theme::Palette::err),m_actions[2]->text(),this,&DownloadsPage::deleteSelected);
    menu.exec(m_table->viewport()->mapToGlobal(position));
}

void DownloadsPage::openExternalClient()
{
    const auto item=m_library->item(selectedHash().mid(9));if(!item)return;
    if(item->downloader=="aria2"){Ui::message(this,QStringLiteral("aria2 后台任务"),QStringLiteral("aria2 作为后台进程运行。可在任务保存目录查看文件及 .aria2 进度文件。"));return;}
    QString error;ExternalCommand command;command.program=item->externalProgram;command.directory=item->savePath;command.console=false;
    if(!ExternalDownloader::start(command,&error))Ui::message(this,QStringLiteral("无法打开下载器"),error,Ui::Notice::Warning);
}

void DownloadsPage::control(const QString &action)
{
    for(const auto &key:selectedHashes()) {
        if(key.startsWith("external:")){if(m_rpc)m_rpc->control(key.mid(9),action);continue;}
        if(action=="pause")m_engine->pause(key);else m_engine->resume(key);
    }
}

void DownloadsPage::pauseSelected() { control(QStringLiteral("pause")); }
void DownloadsPage::resumeSelected() { control(QStringLiteral("resume")); }

void DownloadsPage::deleteSelected()
{
    const auto keys=selectedHashes();
    if(keys.isEmpty())return;
    QStringList builtin;QVector<LibraryItem> records;
    for(const auto &key:keys) {
        if(!key.startsWith("external:")){builtin.append(key);continue;}
        auto item=m_library->item(key.mid(9));if(!item)continue;
        if(m_rpc&&m_rpc->manageable(*item))m_rpc->control(item->key,"remove");
        else records.append(*item);
    }
    // Built-in tasks: one confirmation covers the whole selection.
    if(!builtin.isEmpty()) {
        const auto choice=Ui::confirmRemoval(this);
        if(choice!=Ui::Removal::Cancel)
            for(const auto &hash:builtin)m_engine->remove(hash,choice==Ui::Removal::DeleteFiles);
    }
    if(!records.isEmpty()) {
        QStringList errors;
        for(auto item:records){item.phase="cancelled";QString error;if(!m_library->put(item,&error))errors.append(error);}
        if(!errors.isEmpty())Ui::message(this,QStringLiteral("无法移除记录"),errors.join("\n"),Ui::Notice::Warning);
        else Ui::message(this,QStringLiteral("记录已移除"),QStringLiteral("对应下载器中的任务仍会继续；需要取消时请在该软件中操作。"));
    }
    refresh();
}

void DownloadsPage::updateDownloader()
{
    m_backendButton->setText(QStringLiteral("下载器：")+DownloaderSettings::name(m_config->downloader.selected));
}

#include "Overlays.h"
#include "Theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTranslator>
#include <QListWidget>
#include <QCollator>
#include <algorithm>
#include <QVBoxLayout>
#include <QWindow>

namespace {
class DialogTranslator final : public QTranslator {
public:
    using QTranslator::QTranslator;
    bool isEmpty() const override { return false; }
    QString translate(const char *,const char *source,const char *,int) const override {
        static const QHash<QString,QString> labels={
            {"Name",QStringLiteral("名称")},{"Size",QStringLiteral("大小")},
            {"Type",QStringLiteral("类型")},{"Date Modified",QStringLiteral("修改日期")},
            {"My Computer",QStringLiteral("此电脑")},{"Computer",QStringLiteral("此电脑")},
            {"Directories",QStringLiteral("文件夹")},{"Directory",QStringLiteral("文件夹")},
            {"Back",QStringLiteral("返回")},{"Forward",QStringLiteral("前进")},
            {"Parent Directory",QStringLiteral("上一级")},{"Create New Folder",QStringLiteral("新建文件夹")},
            {"List View",QStringLiteral("列表视图")},{"Detail View",QStringLiteral("详细视图")},
            {"&Undo",QStringLiteral("撤销")},{"&Redo",QStringLiteral("重做")},
            {"Cu&t",QStringLiteral("剪切")},{"&Copy",QStringLiteral("复制")},
            {"&Paste",QStringLiteral("粘贴")},{"Delete",QStringLiteral("删除")},
            {"Select All",QStringLiteral("全选")},{"&Select All",QStringLiteral("全选")}
        };
        return labels.value(QString::fromUtf8(source));
    }
};

class TooltipBubble final : public QWidget {
public:
    TooltipBubble() : QWidget(nullptr, Qt::ToolTip|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint) {
        setObjectName("TooltipBubble");
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        auto *layout=new QVBoxLayout(this); layout->setContentsMargins(16,13,16,13);
        m_text=new QLabel(this); m_text->setObjectName("TooltipText");
        m_text->setTextFormat(Qt::PlainText); m_text->setWordWrap(true);
        layout->addWidget(m_text);
    }
    void present(const QString &text, QWidget *owner, QPoint position) {
        QScreen *screen=QGuiApplication::screenAt(position);
        if(!screen)screen=QGuiApplication::primaryScreen();
        QRect bounds=screen->availableGeometry().adjusted(6,6,-6,-6);
        const QRect windowRect(owner->window()->mapToGlobal(QPoint()),owner->window()->size());
        const QRect inside=bounds.intersected(windowRect.adjusted(6,6,-6,-6));
        if(inside.width()>=180 && inside.height()>=90)bounds=inside;
        m_text->setMaximumWidth(qMin(320,qMax(100,bounds.width()-32)));
        m_text->setMaximumHeight(qMin(240,qMax(40,bounds.height()-26)));
        m_text->setText(text); adjustSize();
        QPoint target=position+QPoint(12,20);
        if(target.y()+height()>bounds.bottom())target.setY(position.y()-height()-12);
        target.setX(qBound(bounds.left(),target.x(),qMax(bounds.left(),bounds.right()-width()+1)));
        target.setY(qBound(bounds.top(),target.y(),qMax(bounds.top(),bounds.bottom()-height()+1)));
        move(target); show();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRectF body=QRectF(rect()).adjusted(6,6,-6,-6);
        p.setPen(Qt::NoPen);
        for(int i=5;i>=1;--i){p.setBrush(QColor(28,70,65,3));p.drawRoundedRect(body.adjusted(-i,-i+2,i,i+2),8+i,8+i);}
        p.setBrush(QColor("#fcfffe"));p.setPen(QPen(QColor(Theme::BORDER),1));p.drawRoundedRect(body,8,8);
    }
private:
    QLabel *m_text;
};

class OverlayController final : public QObject {
public:
    explicit OverlayController(QApplication *app):QObject(app) {
        m_expiry.setSingleShot(true);
        connect(&m_expiry,&QTimer::timeout,this,[this]{dismiss();});
        app->installEventFilter(this);
    }
    ~OverlayController() override { delete m_bubble.data(); }
    void present(QWidget *owner,const QString &text,const QPoint &position) {
        if(!owner||text.isEmpty()){dismiss();return;}
        QToolTip::hideText();
        if(!m_bubble)m_bubble=new TooltipBubble;
        m_owner=owner;m_bubble->present(text,owner,position);
        m_expiry.start(qBound(2500,text.size()*60,10000));
    }
    void dismiss(){if(m_bubble)m_bubble->hide();m_owner=nullptr;m_expiry.stop();}
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        auto *widget=qobject_cast<QWidget *>(object);
        if(event->type()==QEvent::ToolTip && widget) {
            auto *help=static_cast<QHelpEvent *>(event);
            QString text=widget->toolTip();
            if(text.isEmpty()) {
                if(auto *view=qobject_cast<QAbstractItemView *>(widget->parentWidget()))
                    text=view->indexAt(help->pos()).data(Qt::ToolTipRole).toString();
            }
            for(QWidget *ancestor=widget->parentWidget();text.isEmpty()&&ancestor;ancestor=ancestor->parentWidget()) {
                text=ancestor->toolTip();
                if(ancestor->isWindow())break;
            }
            if(text.trimmed().isEmpty()){dismiss();return false;}
            present(widget,text,help->globalPos());
            event->accept();return true;
        }
        switch(event->type()) {
        case QEvent::MouseButtonPress: case QEvent::Wheel: case QEvent::KeyPress:
        case QEvent::WindowDeactivate: case QEvent::ApplicationDeactivate:
            dismiss();break;
        case QEvent::Leave: case QEvent::Hide: case QEvent::Destroy:
            if(widget && widget==m_owner)dismiss();break;
        default:break;
        }
        return QObject::eventFilter(object,event);
    }
private:
    QPointer<TooltipBubble> m_bubble;
    QPointer<QWidget> m_owner;
    QTimer m_expiry;
};
QPointer<OverlayController> overlayController;

class DialogSurface final : public QDialog {
public:
    DialogSurface(QWidget *parent,const QString &title):QDialog(parent,Qt::Dialog|Qt::FramelessWindowHint) {
        setObjectName("DialogSurface");setWindowTitle(title);setWindowModality(parent?Qt::WindowModal:Qt::ApplicationModal);
        setAttribute(Qt::WA_TranslucentBackground);
        auto *outer=new QVBoxLayout(this);outer->setContentsMargins(14,14,14,14);
        auto *panel=new QFrame(this);panel->setObjectName("DialogPanel");outer->addWidget(panel);
        auto *shadow=new QGraphicsDropShadowEffect(panel);shadow->setBlurRadius(24);shadow->setOffset(0,5);
        shadow->setColor(QColor(24,65,60,30));panel->setGraphicsEffect(shadow);
        auto *body=new QVBoxLayout(panel);body->setContentsMargins(24,18,24,22);body->setSpacing(18);
        m_header=new QWidget(panel);m_header->setObjectName("DialogHeader");m_header->installEventFilter(this);
        auto *heading=new QHBoxLayout(m_header);heading->setContentsMargins(0,0,0,0);
        auto *name=new QLabel(title,m_header);name->setObjectName("DialogTitle");
        name->setAttribute(Qt::WA_TransparentForMouseEvents);heading->addWidget(name,1);
        auto *close=new QToolButton(m_header);close->setObjectName("DialogClose");close->setIcon(Theme::icon("close"));
        close->setFocusPolicy(Qt::NoFocus);
        close->setFixedSize(30,30);close->setAccessibleName(QStringLiteral("关闭"));
        connect(close,&QToolButton::clicked,this,&QDialog::reject);heading->addWidget(close);body->addWidget(m_header);
        content=new QVBoxLayout;content->setSpacing(16);body->addLayout(content);
    }
    QVBoxLayout *content;
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_header&&event->type()==QEvent::MouseButtonPress) {
            auto *mouse=static_cast<QMouseEvent *>(event);
            if(mouse->button()==Qt::LeftButton&&windowHandle())return windowHandle()->startSystemMove();
        }
        return QDialog::eventFilter(object,event);
    }
    void showEvent(QShowEvent *event) override {
        QDialog::showEvent(event);
        QScreen *screen=parentWidget()?parentWidget()->screen():QGuiApplication::primaryScreen();
        const QRect available=screen->availableGeometry().adjusted(10,10,-10,-10);
        const QPoint center=parentWidget()?parentWidget()->window()->geometry().center():available.center();
        move(qBound(available.left(),center.x()-width()/2,qMax(available.left(),available.right()-width()+1)),
             qBound(available.top(),center.y()-height()/2,qMax(available.top(),available.bottom()-height()+1)));
    }
private:
    QWidget *m_header;
};

QString pickFile(QWidget *parent,const QString &title,const QString &path,QFileDialog::FileMode mode,QFileDialog::AcceptMode acceptMode,bool executable=false)
{
    DialogSurface dialog(parent,title);dialog.resize(820,570);
    auto *picker=new QFileDialog(&dialog,QString(),path);
    picker->setOption(QFileDialog::DontUseNativeDialog);
    picker->setWindowFlags(Qt::Widget);picker->setObjectName("FilePicker");
    picker->setSizeGripEnabled(false);picker->setFileMode(mode);picker->setAcceptMode(acceptMode);
    if(acceptMode==QFileDialog::AcceptSave)picker->setOption(QFileDialog::DontConfirmOverwrite);
    picker->setViewMode(QFileDialog::Detail);
    auto places=picker->sidebarUrls();
    for(const auto location:{QStandardPaths::DownloadLocation,QStandardPaths::DesktopLocation}) {
        const QString folder=QStandardPaths::writableLocation(location);
        const QUrl url=QUrl::fromLocalFile(folder);
        if(QFileInfo(folder).isDir()&&!places.contains(url))places.append(url);
    }
    picker->setSidebarUrls(places);
    struct PickerAction { const char *name; const char *icon; const char16_t *label; };
    const PickerAction actions[]={
        {"backButton","back",u"返回"},{"forwardButton","forward",u"前进"},
        {"toParentButton","up",u"上一级文件夹"},{"newFolderButton","folder-plus",u"新建文件夹"},
        {"listModeButton","grid",u"图标视图"},{"detailModeButton","list",u"详细列表"}
    };
    for(const auto &action:actions) {
        if(auto *button=picker->findChild<QToolButton *>(action.name)) {
            button->setProperty("pickerTool",true);button->setIcon(Theme::icon(action.icon,QColor(Theme::ACCENT_TEXT)));
            button->setIconSize(QSize(19,19));button->setFixedSize(34,34);button->setAutoRaise(true);
            button->setToolTip(QString::fromUtf16(action.label));button->setAccessibleName(QString::fromUtf16(action.label));
        }
    }
    picker->setLabelText(QFileDialog::LookIn,QStringLiteral("当前位置"));
    picker->setLabelText(QFileDialog::FileName,mode==QFileDialog::Directory?QStringLiteral("文件夹"):QStringLiteral("文件名"));
    picker->setLabelText(QFileDialog::FileType,QStringLiteral("文件类型"));
    picker->setLabelText(QFileDialog::Accept,mode==QFileDialog::Directory?QStringLiteral("选择此目录"):
        acceptMode==QFileDialog::AcceptSave?QStringLiteral("保存"):QStringLiteral("打开"));
    picker->setLabelText(QFileDialog::Reject,QStringLiteral("取消"));
    if(mode==QFileDialog::Directory) {
        picker->setOption(QFileDialog::ShowDirsOnly);
        if(auto *types=picker->findChild<QWidget *>("fileTypeCombo"))types->hide();
        if(auto *types=picker->findChild<QWidget *>("fileTypeLabel"))types->hide();
    }
    else if(executable){picker->setNameFilter(QStringLiteral("Windows 应用程序 (*.exe)"));picker->setDefaultSuffix("exe");picker->setLabelText(QFileDialog::Accept,QStringLiteral("选择此程序"));}
    else {picker->setNameFilter(QStringLiteral("JSON 配置文件 (*.json)"));picker->setDefaultSuffix("json");}
    // File dialogs use the same typography, spacing and focus treatment as pages.
    if(picker->layout()){picker->layout()->setContentsMargins(0,0,0,0);picker->layout()->setSpacing(10);}
    for(auto *view:picker->findChildren<QAbstractItemView *>())view->setAlternatingRowColors(false);
    if(auto *buttons=picker->findChild<QDialogButtonBox *>()) {
        for(auto *button:buttons->buttons()) {
            if(buttons->buttonRole(button)==QDialogButtonBox::AcceptRole)button->setObjectName("Primary");
        }
    }
    dialog.content->addWidget(picker);
    QObject::connect(picker,&QFileDialog::accepted,&dialog,&QDialog::accept);
    QObject::connect(picker,&QFileDialog::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return {};
    const QString selected=picker->selectedFiles().value(0);
    if(acceptMode==QFileDialog::AcceptSave && QFileInfo::exists(selected)) {
        DialogSurface confirm(parent,QStringLiteral("替换已有配置？"));confirm.setFixedWidth(480);
        auto *text=new QLabel(QStringLiteral("%1 已存在。替换后将覆盖原有配置内容。")
            .arg(QFileInfo(selected).fileName()),&confirm);
        text->setWordWrap(true);text->setTextFormat(Qt::PlainText);text->setObjectName("DialogBody");
        confirm.content->addWidget(text);
        auto *buttons=new QHBoxLayout;buttons->addStretch();
        auto *cancel=new QPushButton(QStringLiteral("取消"),&confirm);cancel->setDefault(true);
        auto *replace=new QPushButton(QStringLiteral("替换配置"),&confirm);replace->setObjectName("Danger");
        QObject::connect(cancel,&QPushButton::clicked,&confirm,&QDialog::reject);
        QObject::connect(replace,&QPushButton::clicked,&confirm,&QDialog::accept);
        buttons->addWidget(cancel);buttons->addWidget(replace);confirm.content->addLayout(buttons);
        confirm.adjustSize();
        if(confirm.exec()!=QDialog::Accepted)return {};
    }
    return selected;
}

QLabel *bodyText(const QString &text,QWidget *parent)
{
    auto *label=new QLabel(text,parent);label->setObjectName("DialogBody");
    label->setTextFormat(Qt::PlainText);label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);return label;
}
}

namespace Ui {
void installOverlays(QApplication *app)
{
    if(!app || app->property("nexusOverlaysInstalled").toBool())return;
    app->setProperty("nexusOverlaysInstalled",true);
    app->installTranslator(new DialogTranslator(app));
    overlayController=new OverlayController(app);
}

void showTooltip(QWidget *owner,const QString &text,const QPoint &position)
{if(overlayController)overlayController->present(owner,text,position);}
void hideTooltip()
{if(overlayController)overlayController->dismiss();}

void message(QWidget *parent,const QString &title,const QString &text,Notice notice)
{
    DialogSurface dialog(parent,title);dialog.setFixedWidth(500);
    auto *row=new QHBoxLayout;row->setSpacing(14);
    auto *icon=new QLabel(&dialog);icon->setObjectName(notice==Notice::Error?"NoticeError":notice==Notice::Warning?"NoticeWarning":"NoticeInfo");
    icon->setFixedSize(40,40);icon->setAlignment(Qt::AlignCenter);
    const QString kind=notice==Notice::Error?"error":notice==Notice::Warning?"warning":"info";
    const QColor color(notice==Notice::Error?Theme::ERR:notice==Notice::Warning?Theme::WARN:Theme::ACCENT_TEXT);
    icon->setPixmap(Theme::icon(kind,color).pixmap(24,24));row->addWidget(icon,0,Qt::AlignTop);
    auto *textLabel=bodyText(text,&dialog);textLabel->setFixedWidth(360);
    auto *scroll=new QScrollArea(&dialog);scroll->setWidgetResizable(true);scroll->setWidget(textLabel);
    const int height=textLabel->heightForWidth(360);
    scroll->setFixedHeight(qBound(48,height+8,260));row->addWidget(scroll,1);dialog.content->addLayout(row);
    auto *buttons=new QHBoxLayout;buttons->addStretch();
    auto *ok=new QPushButton(QStringLiteral("知道了"),&dialog);ok->setObjectName("Primary");ok->setDefault(true);
    QObject::connect(ok,&QPushButton::clicked,&dialog,&QDialog::accept);buttons->addWidget(ok);dialog.content->addLayout(buttons);
    dialog.adjustSize();dialog.exec();
}

Removal confirmRemoval(QWidget *parent)
{
    DialogSurface dialog(parent,QStringLiteral("移除下载任务"));dialog.setFixedWidth(550);
    dialog.content->addWidget(bodyText(QStringLiteral("请选择是否保留已下载的文件。同时删除文件后无法撤销。"),&dialog));
    auto *buttons=new QHBoxLayout;buttons->setSpacing(8);
    auto *cancel=new QPushButton(QStringLiteral("取消"),&dialog);cancel->setDefault(true);
    auto *keep=new QPushButton(QStringLiteral("仅移除任务"),&dialog);keep->setObjectName("Primary");
    auto *remove=new QPushButton(QStringLiteral("同时删除文件"),&dialog);remove->setObjectName("Danger");
    Removal choice=Removal::Cancel;
    QObject::connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
    QObject::connect(keep,&QPushButton::clicked,&dialog,[&]{choice=Removal::KeepFiles;dialog.accept();});
    QObject::connect(remove,&QPushButton::clicked,&dialog,[&]{choice=Removal::DeleteFiles;dialog.accept();});
    buttons->addWidget(cancel);buttons->addStretch();buttons->addWidget(remove);buttons->addWidget(keep);dialog.content->addLayout(buttons);
    dialog.adjustSize();dialog.exec();return choice;
}

QString chooseDirectory(QWidget *parent,const QString &title,const QString &path)
{return pickFile(parent,title,path,QFileDialog::Directory,QFileDialog::AcceptOpen);}
QString openExecutable(QWidget *parent,const QString &path)
{return pickFile(parent,QStringLiteral("选择下载器程序"),path,QFileDialog::ExistingFile,QFileDialog::AcceptOpen,true);}
QString openJson(QWidget *parent,const QString &title)
{return pickFile(parent,title,QString(),QFileDialog::ExistingFile,QFileDialog::AcceptOpen);}
QString saveJson(QWidget *parent,const QString &title,const QString &fileName)
{return pickFile(parent,title,fileName,QFileDialog::AnyFile,QFileDialog::AcceptSave);}

QString replaceDownloadDirectory(QWidget *parent,const QString &path,const QString &reason)
{
    DialogSurface dialog(parent,QStringLiteral("需要更换下载目录"));dialog.setFixedWidth(560);
    dialog.content->addWidget(bodyText(reason,&dialog));
    auto *location=bodyText(QDir::toNativeSeparators(path),&dialog);location->setObjectName("DirectoryNotice");
    dialog.content->addWidget(location);
    auto *buttons=new QHBoxLayout;buttons->addStretch();
    auto *quit=new QPushButton(QStringLiteral("退出软件"),&dialog);
    auto *choose=new QPushButton(QStringLiteral("选择新目录"),&dialog);choose->setObjectName("Primary");choose->setDefault(true);
    QString selected;
    QObject::connect(quit,&QPushButton::clicked,&dialog,&QDialog::reject);
    QObject::connect(choose,&QPushButton::clicked,&dialog,[&] {
        QString initial=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if(!QFileInfo(initial).isDir())initial=QDir::homePath();
        const QString candidate=chooseDirectory(&dialog,QStringLiteral("选择新的下载目录"),initial);
        if(candidate.isEmpty())return;
        if(!QFileInfo(candidate).isDir()) {
            message(&dialog,QStringLiteral("目录不可用"),QStringLiteral("所选文件夹已经不存在，请重新选择。"),Notice::Warning);return;
        }
        selected=candidate;dialog.accept();
    });
    buttons->addWidget(quit);buttons->addWidget(choose);dialog.content->addLayout(buttons);
    dialog.adjustSize();dialog.exec();return selected;
}

QString chooseVideo(QWidget *parent,const QStringList &paths)
{
    if(paths.isEmpty())return {};
    if(paths.size()==1)return paths.first();
    DialogSurface dialog(parent,QStringLiteral("选择要播放的视频"));dialog.resize(650,460);
    auto *list=new QListWidget(&dialog);list->setObjectName("VideoList");
    QStringList sorted=paths;QCollator collator;collator.setNumericMode(true);
    std::sort(sorted.begin(),sorted.end(),[&](const QString &a,const QString &b){return collator.compare(QFileInfo(a).fileName(),QFileInfo(b).fileName())<0;});
    for(const auto &path:sorted){auto *item=new QListWidgetItem(QFileInfo(path).fileName(),list);item->setData(Qt::UserRole,path);}
    list->setCurrentRow(0);dialog.content->addWidget(list);
    auto *buttons=new QHBoxLayout;buttons->addStretch();
    auto *cancel=new QPushButton(QStringLiteral("取消"),&dialog);
    auto *play=new QPushButton(QStringLiteral("播放"),&dialog);play->setObjectName("Primary");play->setDefault(true);
    QObject::connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
    QObject::connect(play,&QPushButton::clicked,&dialog,&QDialog::accept);
    QObject::connect(list,&QListWidget::itemDoubleClicked,&dialog,&QDialog::accept);
    buttons->addWidget(cancel);buttons->addWidget(play);dialog.content->addLayout(buttons);
    if(dialog.exec()!=QDialog::Accepted||!list->currentItem())return {};
    return list->currentItem()->data(Qt::UserRole).toString();
}
}

// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Overlays.h"
#include "Theme.h"
#include "Motion.h"
#include <QButtonGroup>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QStackedWidget>
#include <tuple>

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
        p.setBrush(QColor(Theme::c().surface));p.setPen(QPen(QColor(Theme::c().border),1));p.drawRoundedRect(body,8,8);
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
        // Scrolling a page must not change a dropdown or number box that merely
        // passes under the pointer. Qt's default wheel focus would grab focus first,
        // so they take focus by click or Tab only, and unfocused ones hand the
        // wheel back to the page.
        if(event->type()==QEvent::Polish && (qobject_cast<QComboBox *>(widget)||qobject_cast<QAbstractSpinBox *>(widget))
           && widget->focusPolicy()==Qt::WheelFocus)
            widget->setFocusPolicy(Qt::StrongFocus);
        if(event->type()==QEvent::Wheel && widget && !widget->hasFocus()
           && (qobject_cast<QComboBox *>(widget)||qobject_cast<QAbstractSpinBox *>(widget))) {
            event->ignore();
            return true;
        }
        switch(event->type()) {
        case QEvent::Show:
            // Menus, dropdowns, dialogs and tooltips ease in instead of popping.
            if(widget && widget->isWindow() && !widget->testAttribute(Qt::WA_TransparentForMouseEvents)) {
                const auto type=widget->windowType();
                if(type==Qt::Popup||type==Qt::Dialog||type==Qt::ToolTip||type==Qt::Tool)Motion::fadeIn(widget);
            }
            break;
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
        shadow->setColor(Theme::c().shadow);panel->setGraphicsEffect(shadow);
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
            button->setProperty("pickerTool",true);button->setIcon(Theme::icon(action.icon,&Theme::Palette::accentText));
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
    const Theme::Role role=notice==Notice::Error?&Theme::Palette::err:notice==Notice::Warning?&Theme::Palette::warn:&Theme::Palette::accentText;
    icon->setPixmap(Theme::icon(kind,role).pixmap(24,24));row->addWidget(icon,0,Qt::AlignTop);
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

bool ask(QWidget *parent,const QString &title,const QString &text,const QString &acceptText,const QString &rejectText)
{
    DialogSurface dialog(parent,title);dialog.setFixedWidth(520);
    dialog.content->addWidget(bodyText(text,&dialog));
    auto *buttons=new QHBoxLayout;buttons->setSpacing(8);
    auto *reject=new QPushButton(rejectText,&dialog);
    auto *accept=new QPushButton(acceptText,&dialog);accept->setObjectName("Primary");accept->setDefault(true);
    QObject::connect(reject,&QPushButton::clicked,&dialog,&QDialog::reject);
    QObject::connect(accept,&QPushButton::clicked,&dialog,&QDialog::accept);
    buttons->addStretch();buttons->addWidget(reject);buttons->addWidget(accept);dialog.content->addLayout(buttons);
    dialog.adjustSize();
    return dialog.exec()==QDialog::Accepted;
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

QString playerNote()
{
    return QStringLiteral("本软件不含播放器，下载完成的视频会交给系统默认播放器打开。番剧资源多为 HEVC / 10bit 编码，"
                          "并常带内封或外挂的 ASS 特效字幕，建议安装 PotPlayer、mpv 或 MPC-HC 等能完整显示这些字幕的播放器，"
                          "并将其设为 .mkv / .mp4 的默认打开方式。");
}

QString freeSoftwareNote()
{
    return QStringLiteral("Anime Nexus 是免费的开源项目，从不以任何形式收费。如果你是付费购买、"
                          "或通过付费下载渠道获得本软件的，请立即申请退款并向平台举报售卖者。");
}

QStringList disclaimerItems()
{
    return {
        QStringLiteral("本软件仅是番剧资源的检索与下载管理工具，不提供、不存储、不上传任何影视内容。检索结果全部来自第三方公开站点，其内容与本软件无关。"),
        QStringLiteral("通过本软件下载的番剧仅供个人学习与交流使用，请在下载后 24 小时内删除。"),
        QStringLiteral("请支持正版。喜欢的作品请通过正版平台观看或购买，以支持创作者。"),
        QStringLiteral("请遵守所在地法律法规。不得将下载内容用于商业用途、公开放映、上传分享、二次分发或其他侵犯著作权的行为；由此产生的一切法律责任由使用者自行承担。"),
        QStringLiteral("BitTorrent 下载时会同时向其他用户上传已下载的数据（做种）。请了解所在地关于 P2P 传输的规定；可在「偏好设置 → 传输与队列」限制上传，或开启下载完成后自动停止。"),
        QStringLiteral("如权利人认为某些资源侵犯其合法权益，请联系提供该资源的站点处理。本软件不对第三方站点的内容、可用性及合法性负责。"),
        QStringLiteral("本软件按「现状」提供，不对下载内容的完整性、安全性及因使用本软件造成的任何损失承担责任。"),
        QStringLiteral("继续使用即表示您已阅读、理解并同意以上全部声明。"),
    };
}

namespace {
// Icon tile for the welcome guide; paints at paint time so it follows theme changes.
class GuideTile final : public QLabel {
public:
    GuideTile(const QString &icon, Theme::Role role, const char *tone, QWidget *parent)
        : QLabel(parent), m_icon(Theme::icon(icon, role))
    {
        setObjectName("GuideTile");
        setProperty("tone", tone);
        setFixedSize(36, 36);
    }
protected:
    void paintEvent(QPaintEvent *event) override
    {
        QLabel::paintEvent(event);
        QPainter painter(this);
        m_icon.paint(&painter, QRect((width() - 20) / 2, (height() - 20) / 2, 20, 20));
    }
private:
    QIcon m_icon;
};

// One guide row: tile, title, explanation, and an optional control on the right.
QWidget *guideRow(QWidget *parent, const QString &icon, Theme::Role role, const char *tone,
                  const QString &title, const QString &text, QWidget *trailing = nullptr)
{
    auto *row = new QWidget(parent);
    auto *line = new QHBoxLayout(row); line->setContentsMargins(0, 0, 0, 0); line->setSpacing(12);
    line->addWidget(new GuideTile(icon, role, tone, row), 0, Qt::AlignTop);
    auto *textCol = new QVBoxLayout; textCol->setSpacing(2);
    auto *name = new QLabel(title, row); name->setObjectName("GuideNoteTitle"); textCol->addWidget(name);
    auto *body = new QLabel(text, row); body->setObjectName("GuideNoteText"); body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse); textCol->addWidget(body);
    line->addLayout(textCol, 1);
    if (trailing) { trailing->setParent(row); line->addWidget(trailing, 0, Qt::AlignVCenter); }
    return row;
}

// A titled card holding rows separated by hairlines; every block of the guide uses it.
QWidget *guideSection(QWidget *parent, const QString &heading, const QList<QWidget *> &rows)
{
    auto *section = new QWidget(parent);
    auto *col = new QVBoxLayout(section); col->setContentsMargins(0, 0, 0, 0); col->setSpacing(6);
    auto *label = new QLabel(heading, section); label->setObjectName("GuideSection"); col->addWidget(label);
    auto *card = new QFrame(section); card->setObjectName("GuideNote");
    auto *cardCol = new QVBoxLayout(card); cardCol->setContentsMargins(14, 12, 14, 12); cardCol->setSpacing(10);
    for (int i = 0; i < rows.size(); ++i) {
        if (i > 0) { auto *line = new QFrame(card); line->setObjectName("SettingDivider"); line->setFixedHeight(1); cardCol->addWidget(line); }
        rows.at(i)->setParent(card);
        cardCol->addWidget(rows.at(i));
    }
    col->addWidget(card);
    return section;
}

QWidget *disclaimerList(QWidget *parent)
{
    auto *body=new QWidget(parent);body->setObjectName("ScrollBody");
    auto *col=new QVBoxLayout(body);col->setContentsMargins(0,0,8,0);col->setSpacing(10);
    const auto items=disclaimerItems();
    for(int i=0;i<items.size();++i) {
        auto *row=new QHBoxLayout;row->setSpacing(10);
        auto *number=new QLabel(QString::number(i+1),body);number->setObjectName("GuideNumber");
        number->setFixedSize(22,22);number->setAlignment(Qt::AlignCenter);row->addWidget(number,0,Qt::AlignTop);
        auto *text=new QLabel(items.at(i),body);text->setObjectName(i==1?"GuideEmphasis":"GuideNoteText");
        text->setWordWrap(true);text->setTextInteractionFlags(Qt::TextSelectableByMouse);row->addWidget(text,1);
        col->addLayout(row);
    }
    col->addStretch(1);
    auto *scroll=new QScrollArea(parent);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setWidget(body);scroll->setFixedHeight(320);
    return scroll;
}

// 跟随系统 / 浅色 / 深色, applied immediately so the guide itself previews the choice.
QWidget *themeChooser(QWidget *parent)
{
    auto *bar = new QFrame(parent); bar->setObjectName("Segmented");
    auto *line = new QHBoxLayout(bar); line->setContentsMargins(4, 4, 4, 4); line->setSpacing(2);
    auto *group = new QButtonGroup(bar);
    for (const auto &[mode, icon] : {std::pair{Theme::Mode::System, "monitor"}, {Theme::Mode::Light, "sun"}, {Theme::Mode::Dark, "moon"}}) {
        auto *button = new QPushButton(Theme::icon(icon), Theme::modeName(mode), bar);
        button->setObjectName("SegmentButton"); button->setCheckable(true); button->setCursor(Qt::PointingHandCursor);
        button->setChecked(Theme::mode() == mode);
        group->addButton(button, int(mode)); line->addWidget(button);
    }
    new Motion::SlidingIndicator(bar, group->buttons(), Motion::SlidingIndicator::Look::Outlined, 8);
    QObject::connect(group, &QButtonGroup::idClicked, bar, [](int id) { Theme::setMode(Theme::Mode(id)); });
    return bar;
}
}

bool firstRunGuide(QWidget *parent,bool review,QString *downloadDir)
{
    DialogSurface dialog(parent,review?QStringLiteral("使用声明"):QStringLiteral("欢迎使用 Anime Nexus"));
    dialog.setObjectName("FirstRunGuide");dialog.setFixedWidth(620);
    auto *pages=new QStackedWidget(&dialog);dialog.content->addWidget(pages);
    using P=Theme::Palette;

    // Page 1: quick start, two things to know, then where to save and how it looks.
    auto *start=new QWidget(pages);auto *startCol=new QVBoxLayout(start);startCol->setContentsMargins(0,0,0,0);startCol->setSpacing(14);
    startCol->addWidget(bodyText(QStringLiteral("聚合多个资源站，按字幕组整理番剧资源，并直接管理下载与本地视频。"),start));
    startCol->addWidget(guideSection(start,QStringLiteral("三步上手"),{
        guideRow(start,"calendar",&P::accentText,"accent",QStringLiteral("找番"),QStringLiteral("在「本季放送」按星期浏览新番，或按 Ctrl+K 用中文、日文或英文番名搜索。")),
        guideRow(start,"filter",&P::accentText,"accent",QStringLiteral("选字幕组"),QStringLiteral("番剧详情优先显示人气高、资源多的字幕组，可按季度、清晰度、字幕和关键词筛选。")),
        guideRow(start,"download",&P::accentText,"accent",QStringLiteral("下载与观看"),QStringLiteral("点「下载」获取单集，或用「一键下载」补齐缺集，旁边的 ▾ 可选字幕、清晰度和集数范围；完成后在「我的追番」点「播放」。"))}));
    startCol->addWidget(guideSection(start,QStringLiteral("使用须知"),{
        guideRow(start,"play",&P::accentText,"accent",QStringLiteral("关于播放"),playerNote()),
        guideRow(start,"info",&P::warn,"warn",QStringLiteral("免费开源"),freeSoftwareNote())}));
    QList<QWidget *> setup;
    if(downloadDir&&!review) {
        auto *change=new QPushButton(Theme::icon("folder"),QStringLiteral("更改…"),start);
        auto *row=guideRow(start,"folder",&P::accentText,"accent",QStringLiteral("下载位置"),QString(),change);
        auto *where=row->findChild<QLabel *>("GuideNoteText");
        auto show=[where,downloadDir] {
            // Long paths are shortened in the middle; the tooltip keeps the full one.
            where->ensurePolished();
            const QString path=QDir::toNativeSeparators(*downloadDir);
            where->setText(QStringLiteral("番剧按「番剧名 / 字幕组 / 第 N 季」整理到：\n")
                +where->fontMetrics().elidedText(path,Qt::ElideMiddle,360));
            where->setToolTip(path);
        };
        show();
        QObject::connect(change,&QPushButton::clicked,&dialog,[&dialog,downloadDir,show] {
            // The default folder may not exist yet; start the picker from its parent.
            QString from=*downloadDir;
            if(!QFileInfo(from).isDir())from=QFileInfo(from).absolutePath();
            const QString chosen=chooseDirectory(&dialog,QStringLiteral("选择下载目录"),from);
            if(!chosen.isEmpty()){*downloadDir=chosen;show();}
        });
        setup<<row;
    }
    setup<<guideRow(start,"sun",&P::accentText,"accent",QStringLiteral("界面配色"),QStringLiteral("立即生效，之后可在右上角或「偏好设置 → 外观」随时修改。"),themeChooser(start));
    startCol->addWidget(guideSection(start,setup.size()>1?QStringLiteral("开始之前"):QStringLiteral("界面主题"),setup));
    startCol->addStretch(1);
    pages->addWidget(start);

    // Page 2: the declaration, which must be accepted on first run.
    auto *terms=new QWidget(pages);auto *termsCol=new QVBoxLayout(terms);termsCol->setContentsMargins(0,0,0,0);termsCol->setSpacing(12);
    auto *heading=new QLabel(QStringLiteral("使用声明"),terms);heading->setObjectName("GuideSection");termsCol->addWidget(heading);
    auto *termsCard=new QFrame(terms);termsCard->setObjectName("GuideNote");
    auto *termsCardCol=new QVBoxLayout(termsCard);termsCardCol->setContentsMargins(14,12,8,12);
    termsCardCol->addWidget(disclaimerList(termsCard));termsCol->addWidget(termsCard);
    auto *agree=new QCheckBox(QStringLiteral("我已阅读并同意以上声明"),terms);agree->setObjectName("GuideAgree");
    agree->setVisible(!review);termsCol->addWidget(agree);
    termsCol->addStretch(1);
    pages->addWidget(terms);

    auto *buttons=new QHBoxLayout;buttons->setSpacing(8);
    auto *step=new QLabel(terms);step->setObjectName("Faint");
    auto *back=new QPushButton(QStringLiteral("上一步"),&dialog);back->setObjectName("Ghost");
    auto *quit=new QPushButton(QStringLiteral("不同意并退出"),&dialog);quit->setObjectName("Ghost");
    auto *next=new QPushButton(QStringLiteral("下一步"),&dialog);next->setObjectName("Primary");next->setDefault(true);
    buttons->addWidget(step);buttons->addStretch(1);buttons->addWidget(back);buttons->addWidget(quit);buttons->addWidget(next);
    dialog.content->addLayout(buttons);

    auto update=[&] {
        const bool last=pages->currentIndex()==1;
        step->setText(review?QString():QStringLiteral("%1 / 2").arg(pages->currentIndex()+1));
        back->setVisible(pages->currentIndex()>0&&!review);
        quit->setVisible(last&&!review);
        next->setText(review?QStringLiteral("关闭"):last?QStringLiteral("同意并开始使用"):QStringLiteral("下一步"));
        next->setEnabled(review||!last||agree->isChecked());
    };
    QObject::connect(agree,&QCheckBox::toggled,&dialog,update);
    QObject::connect(back,&QPushButton::clicked,&dialog,[&]{Motion::crossfade(pages,Motion::Fast);pages->setCurrentIndex(0);update();});
    QObject::connect(quit,&QPushButton::clicked,&dialog,&QDialog::reject);
    QObject::connect(next,&QPushButton::clicked,&dialog,[&] {
        if(review||pages->currentIndex()==1){dialog.accept();return;}
        Motion::crossfade(pages,Motion::Fast);pages->setCurrentIndex(1);update();
    });
    if(review)pages->setCurrentIndex(1);
    update();
    next->setFocus(Qt::OtherFocusReason);   // not the first theme button
    dialog.adjustSize();
    // Wrapped labels report heights for a narrower default width; size the dialog
    // from the layout's real height at its fixed width instead.
    if(auto *layout=dialog.layout();layout&&layout->hasHeightForWidth())
        dialog.resize(dialog.width(),layout->totalHeightForWidth(dialog.width()));
    return dialog.exec()==QDialog::Accepted&&(review||agree->isChecked());
}
}

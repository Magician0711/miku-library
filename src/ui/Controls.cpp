#include "Controls.h"
#include "Theme.h"

#include <QApplication>
#include <QFrame>
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QLayout>
#include <QFocusEvent>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTimer>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
class OptionDelegate : public QStyledItemDelegate {
public:
    explicit OptionDelegate(SelectBox *combo) : QStyledItemDelegate(combo), m_combo(combo) {}
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return QSize(QStyledItemDelegate::sizeHint(option,index).width()+48,38);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const bool checked=index.row()==m_combo->currentIndex();
        const bool hover=option.state & QStyle::State_MouseOver;
        const bool enabled=option.state & QStyle::State_Enabled;
        const bool focused=option.state & QStyle::State_Selected;
        const QRectF rect=QRectF(option.rect).adjusted(3,2,-3,-2);
        if(checked || hover || focused) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(checked ? Theme::ACCENT_SOFT : QStringLiteral("#f0f7f6")));
            p->drawRoundedRect(rect,7,7);
        }
        p->setFont(option.font);
        p->setPen(QColor(!enabled ? Theme::TEXT3 : checked ? Theme::ACCENT_TEXT : Theme::TEXT));
        const QRect textRect=option.rect.adjusted(16,0,-36,0);
        p->drawText(textRect,Qt::AlignLeft|Qt::AlignVCenter,
            option.fontMetrics.elidedText(index.data().toString(),Qt::ElideRight,textRect.width()));
        if(checked) {
            const QPointF c(option.rect.right()-20,option.rect.center().y());
            p->setPen(QPen(QColor(Theme::ACCENT_TEXT),1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            QPainterPath path; path.moveTo(c+QPointF(-4,0)); path.lineTo(c+QPointF(-1,3)); path.lineTo(c+QPointF(5,-4));
            p->drawPath(path);
        }
        p->restore();
    }
private:
    SelectBox *m_combo;
};
}

namespace {
class DownloaderPopup final : public QDialog {
public:
    DownloaderPopup(QWidget *anchor,const DownloaderSettings &settings)
        :QDialog(anchor->window(),Qt::Popup|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint),m_anchor(anchor)
    {
        setObjectName("DownloaderPopup");setAttribute(Qt::WA_TranslucentBackground);
        auto *outer=new QVBoxLayout(this);outer->setContentsMargins(10,8,10,14);
        auto *panel=new QFrame(this);panel->setObjectName("DownloaderPopupPanel");outer->addWidget(panel);
        auto *shadow=new QGraphicsDropShadowEffect(panel);shadow->setBlurRadius(18);shadow->setOffset(0,3);shadow->setColor(QColor(25,70,65,32));panel->setGraphicsEffect(shadow);
        auto *body=new QVBoxLayout(panel);body->setContentsMargins(8,12,8,8);body->setSpacing(4);
        auto *heading=new QLabel(QStringLiteral("选择下载器"),panel);heading->setObjectName("DownloaderPopupHeading");body->addWidget(heading);
        const QMap<QString,QString> descriptions={
            {"builtin",QStringLiteral("管理进度与本地视频")},{"qbittorrent",QStringLiteral("自动传入番剧保存目录")},
            {"transmission",QStringLiteral("在软件内确认保存位置")},{"aria2",QStringLiteral("支持本地合集的缺集选择")},
            {"bitcomet",QStringLiteral("静默添加种子任务")},{"custom",QStringLiteral("选择其他程序与启动参数")}};
        for(const auto &type:DownloaderSettings::types()) {
            auto *row=new QPushButton(panel);row->setObjectName("DownloaderOption");row->setProperty("downloaderType",type);
            row->setAccessibleName(DownloaderSettings::name(type));row->setCheckable(true);row->setChecked(settings.selected==type);
            row->setAutoDefault(false);row->setFixedHeight(56);row->setCursor(Qt::PointingHandCursor);row->installEventFilter(this);
            auto *layout=new QHBoxLayout(row);layout->setContentsMargins(10,7,10,7);layout->setSpacing(10);
            auto *icon=new QLabel(row);icon->setObjectName("DownloaderOptionIcon");icon->setFixedSize(30,30);icon->setAlignment(Qt::AlignCenter);
            icon->setPixmap(Theme::icon(type=="builtin"?"download":type=="custom"?"settings":"external",QColor(Theme::ACCENT_TEXT)).pixmap(18,18));layout->addWidget(icon);
            auto *text=new QVBoxLayout;text->setSpacing(2);auto *name=new QLabel(DownloaderSettings::name(type),row);name->setObjectName("DownloaderOptionName");
            auto *description=new QLabel(descriptions.value(type),row);description->setObjectName("DownloaderOptionDescription");text->addWidget(name);text->addWidget(description);layout->addLayout(text,1);
            const bool configured=type=="builtin"||QFileInfo(settings.programs.value(type)).isFile();
            auto *status=new QLabel(settings.selected==type?QStringLiteral("当前"):configured?QString():QStringLiteral("未配置"),row);
            status->setObjectName(settings.selected==type?"DownloaderCurrent":"DownloaderUnconfigured");layout->addWidget(status,0,Qt::AlignVCenter);
            for(auto *child:row->findChildren<QWidget*>())child->setAttribute(Qt::WA_TransparentForMouseEvents);
            connect(row,&QPushButton::clicked,this,[this,type]{choice=type;accept();});body->addWidget(row);m_rows.append(row);
            if(settings.selected==type)m_initial=row;
        }
        auto *line=new QFrame(panel);line->setObjectName("DownloaderPopupDivider");line->setFixedHeight(1);body->addSpacing(4);body->addWidget(line);body->addSpacing(3);
        auto *configure=new QPushButton(QStringLiteral("配置下载器…"),panel);configure->setObjectName("DownloaderConfigure");configure->setIcon(Theme::icon("settings"));
        configure->setAutoDefault(false);configure->installEventFilter(this);connect(configure,&QPushButton::clicked,this,[this]{choice="configure";accept();});body->addWidget(configure);m_rows.append(configure);
        setFixedWidth(360);
    }
    QString choice;
protected:
    void showEvent(QShowEvent *event) override {
        QDialog::showEvent(event);adjustSize();
        const QRect anchor(m_anchor->mapToGlobal(QPoint(0,0)),m_anchor->size());
        const QRect available=m_anchor->screen()->availableGeometry().adjusted(4,4,-4,-4);
        const int x=qBound(available.left(),anchor.left()-10,qMax(available.left(),available.right()-width()+1));
        const int below=available.bottom()-anchor.bottom();
        const int y=below>=height()?anchor.bottom()+4:anchor.top()-height()-4;
        move(x,qBound(available.top(),y,qMax(available.top(),available.bottom()-height()+1)));
        if(m_initial)m_initial->setFocus(Qt::PopupFocusReason);
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::KeyPress) {
            auto *key=static_cast<QKeyEvent*>(event);const int index=m_rows.indexOf(qobject_cast<QPushButton*>(object));
            if(index>=0) {
                if(key->key()==Qt::Key_Up||key->key()==Qt::Key_Down||key->key()==Qt::Key_Home||key->key()==Qt::Key_End) {
                    const int target=key->key()==Qt::Key_Home?0:key->key()==Qt::Key_End?m_rows.size()-1:(index+(key->key()==Qt::Key_Down?1:m_rows.size()-1))%m_rows.size();
                    m_rows[target]->setFocus(Qt::TabFocusReason);return true;
                }
                if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){m_rows[index]->click();return true;}
            }
        }
        return QDialog::eventFilter(object,event);
    }
private:
    QWidget *m_anchor;QVector<QPushButton*> m_rows;QPushButton *m_initial=nullptr;
};
}
QString Ui::chooseDownloader(QWidget *anchor,const DownloaderSettings &settings)
{
    DownloaderPopup popup(anchor,settings);return popup.exec()==QDialog::Accepted?popup.choice:QString();
}

SelectBox::SelectBox(QWidget *parent) : QComboBox(parent)
{
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(40);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    setMaxVisibleItems(8);
    auto *list=new QListView;
    list->setObjectName("SelectOptions");
    list->setFrameShape(QFrame::NoFrame);
    list->setMouseTracking(true);
    list->setUniformItemSizes(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list->setStyleSheet(QStringLiteral(
        "QListView#SelectOptions { background:white; color:#243b3a; border:1px solid #dce9e7;"
        "border-radius:10px; padding:6px; outline:0; }"
        "QListView#SelectOptions::item { border:none; padding:0; }"));
    setView(list);
    setItemDelegate(new OptionDelegate(this));
    auto *shadow=new QGraphicsDropShadowEffect(list);
    shadow->setBlurRadius(18); shadow->setOffset(0,3);
    shadow->setColor(QColor(25,70,65,32));
    list->setGraphicsEffect(shadow);
    QWidget *popup=list->parentWidget();
    popup->setWindowFlags(Qt::Popup|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint);
    popup->setAttribute(Qt::WA_TranslucentBackground);
    popup->setObjectName("SelectPopup");
    popup->setStyleSheet("#SelectPopup { background:transparent; border:none; }");
    if(auto *frame=qobject_cast<QFrame *>(popup))frame->setFrameShape(QFrame::NoFrame);
}

void SelectBox::showPopup()
{
    if(!count() || !isEnabled())return;
    QComboBox::showPopup();
    auto *popup=view()->parentWidget();
    if(auto *layout=popup->layout()) {
        layout->setContentsMargins(10,8,10,14);
        layout->setSizeConstraint(QLayout::SetNoConstraint);
    }
    const QRect anchor(mapToGlobal(QPoint(0,0)),size());
    QScreen *screen=QGuiApplication::screenAt(anchor.center());
    if(!screen)screen=QGuiApplication::primaryScreen();
    const QRect available=screen->availableGeometry().adjusted(2,2,-2,-2);
    const int desired=qMin(count(),maxVisibleItems())*38+14+22;
    const int below=available.bottom()-anchor.bottom()-4;
    const int above=anchor.top()-available.top()-4;
    const bool openBelow=below>=qMin(desired,150) || below>=above;
    const int height=qMin(desired,qMax(0,openBelow ? below : above));
    const int width=qMin(anchor.width()+20,available.width());
    const int x=qBound(available.left(),anchor.left()-10,available.right()-width+1);
    const int y=openBelow ? anchor.bottom()+4 : anchor.top()-height-4;
    popup->setGeometry(x,y,width,height);
    view()->scrollTo(view()->currentIndex(),QAbstractItemView::EnsureVisible);
}

void SoftFocusDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem clean=option;
    clean.state &= ~QStyle::State_HasFocus;
    QStyledItemDelegate::paint(p,clean,index);
}

SearchInput::SearchInput(QWidget *parent) : QLineEdit(parent)
{
    setAttribute(Qt::WA_InputMethodEnabled);
#ifdef Q_OS_WIN
    // Some IMEs consume the matching key-up. Release the guard when Windows
    // reports the key is up, even if QLineEdit never receives that event.
    m_candidateRelease=new QTimer(this);m_candidateRelease->setInterval(10);
    connect(m_candidateRelease,&QTimer::timeout,this,[this] {
        if(!(GetKeyState(VK_RETURN)&0x8000)) {
            m_confirmingCandidate=false;m_candidateRelease->stop();
        }
    });
#endif
}

void SearchInput::guardCandidateConfirmation()
{
    m_confirmingCandidate=true;
    if(m_candidateRelease)m_candidateRelease->start();
}

bool SearchInput::event(QEvent *event)
{
    if(event->type()==QEvent::ShortcutOverride && m_composing) {
        auto *key=static_cast<QKeyEvent *>(event);
        if(key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter) {
            guardCandidateConfirmation();
            event->accept();return true;
        }
    }
    return QLineEdit::event(event);
}

void SearchInput::inputMethodEvent(QInputMethodEvent *event)
{
    const bool wasComposing=m_composing;
    m_composing=!event->preeditString().isEmpty();
    QLineEdit::inputMethodEvent(event);
#ifdef Q_OS_WIN
    // Windows IMEs may deliver the commit before the Return key event. The
    // message-queue key state distinguishes Enter confirmation from Space/click.
    if(wasComposing && !event->commitString().isEmpty() && (GetKeyState(VK_RETURN)&0x8000))
        guardCandidateConfirmation();
#endif
    if(wasComposing!=m_composing)emit compositionChanged(m_composing);
}

void SearchInput::keyPressEvent(QKeyEvent *event)
{
    const bool enter=event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter;
    if(enter && (m_composing || m_confirmingCandidate || event->isAutoRepeat())) {
        guardCandidateConfirmation();
        // Do not force commit/reset here: Windows may still be delivering the
        // IME result, and interrupting it can discard the candidate text.
        event->accept();return;
    }
    if(!enter) {
        m_confirmingCandidate=false;
        if(m_candidateRelease)m_candidateRelease->stop();
    }
    QLineEdit::keyPressEvent(event);
}

void SearchInput::keyReleaseEvent(QKeyEvent *event)
{
    if(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter) {
        m_confirmingCandidate=false;
        if(m_candidateRelease)m_candidateRelease->stop();
    }
    QLineEdit::keyReleaseEvent(event);
}

void SearchInput::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    m_composing=false;m_confirmingCandidate=false;
    if(m_candidateRelease)m_candidateRelease->stop();
}

#include "Motion.h"
#include "Theme.h"

#include <QAbstractButton>
#include <QEvent>
#include <QLayout>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariantAnimation>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace Motion {

bool enabled()
{
#ifdef Q_OS_WIN
    BOOL animate = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0))
        return animate != FALSE;
#endif
    return true;
}

int duration(int ms) { return enabled() ? ms : 0; }

SnapshotFade::SnapshotFade(QWidget *target, int ms)
    : QWidget(target)
{
    // Capture first: an older fade still on screen becomes part of this snapshot,
    // so rapid changes blend instead of jumping.
    m_snapshot = target->grab();
    for (auto *older : target->findChildren<SnapshotFade *>(Qt::FindDirectChildrenOnly))
        if (older != this) older->deleteLater();
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    setGeometry(target->rect());
    target->installEventFilter(this);
    raise();
    show();

    // The change that follows (a restyle, a page's first layout) can block the UI
    // thread for a while. Start the clock when the new content is first painted,
    // otherwise the fade would be over before anyone sees it.
    m_animation = new QVariantAnimation(this);
    auto *animation = m_animation;
    animation->setDuration(ms);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QTimer::singleShot(600, this, &SnapshotFade::begin);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        // QStackedWidget raises the page it switches to; stay above it.
        if (parent()->children().constLast() != this) raise();
        m_opacity = value.toReal();
        update();
    });
    connect(animation, &QVariantAnimation::finished, this, &QObject::deleteLater);
}

void SnapshotFade::begin()
{
    if (m_animation->state() == QAbstractAnimation::Stopped && m_opacity >= 1.0)
        m_animation->start();
}

bool SnapshotFade::eventFilter(QObject *object, QEvent *event)
{
    // Keep covering the target if it is resized mid-fade.
    if (object == parent() && event->type() == QEvent::Resize) setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(object, event);
}

void SnapshotFade::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setOpacity(m_opacity);
    painter.drawPixmap(0, 0, m_snapshot);
    if (m_animation->state() == QAbstractAnimation::Stopped && m_opacity >= 1.0)
        QTimer::singleShot(0, this, &SnapshotFade::begin);
}

void crossfade(QWidget *target, int ms)
{
    ms = duration(ms);
    if (!target || ms <= 0 || !target->isVisible() || target->width() <= 0 || target->height() <= 0)
        return;
    new SnapshotFade(target, ms);
}

namespace {
class WindowFade final : public QWidget {
public:
    WindowFade(QWidget *window, int ms)
        : QWidget(window, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::NoDropShadowWindowHint)
    {
        m_snapshot = window->grab();
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_DeleteOnClose);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(QRect(window->mapToGlobal(QPoint(0, 0)), window->size()));
        m_animation = new QPropertyAnimation(this, "windowOpacity", this);
        m_animation->setDuration(ms);
        m_animation->setStartValue(1.0);
        m_animation->setEndValue(0.0);
        m_animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_animation, &QPropertyAnimation::finished, this, &QWidget::close);
        QTimer::singleShot(800, this, [this] { begin(); });
        show();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawPixmap(0, 0, m_snapshot);
        // The app underneath repaints in the new theme during the same pass.
        QTimer::singleShot(0, this, [this] { begin(); });
    }

private:
    void begin() { if (m_animation->state() == QAbstractAnimation::Stopped && windowOpacity() >= 1.0) m_animation->start(); }
    QPixmap m_snapshot;
    QPropertyAnimation *m_animation;
};
}

void crossfadeWindow(QWidget *window, int ms)
{
    ms = duration(ms);
    if (!window || ms <= 0 || !window->isVisible() || window->isMinimized()) return;
    new WindowFade(window, ms);
}

void setExpanded(QWidget *section, bool expanded, int ms)
{
    if (!section) return;
    auto *previous = section->findChild<QPropertyAnimation *>(QStringLiteral("ExpandAnimation"), Qt::FindDirectChildrenOnly);
    const int from = previous ? section->maximumHeight() : (section->isVisible() ? section->height() : 0);
    delete previous;
    ms = duration(ms);
    if (ms <= 0 || !section->parentWidget() || !section->parentWidget()->isVisible()) {
        section->setMaximumHeight(QWIDGETSIZE_MAX);
        section->setVisible(expanded);
        return;
    }
    int to = 0;
    if (expanded) {
        section->setMaximumHeight(from);
        section->show();
        const int width = section->width() > 0 ? section->width() : section->parentWidget()->width();
        to = section->hasHeightForWidth() ? section->heightForWidth(width) : section->sizeHint().height();
        if (section->layout() && section->layout()->hasHeightForWidth())
            to = qMax(to, section->layout()->totalHeightForWidth(width));
    }
    auto *animation = new QPropertyAnimation(section, "maximumHeight", section);
    animation->setObjectName(QStringLiteral("ExpandAnimation"));
    animation->setDuration(ms);
    animation->setStartValue(from);
    animation->setEndValue(to);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animation, &QPropertyAnimation::finished, section, [section, expanded, animation] {
        section->setMaximumHeight(QWIDGETSIZE_MAX);
        section->setVisible(expanded);
        animation->deleteLater();
    });
    animation->start();
}

void fadeIn(QWidget *window, int ms)
{
    ms = duration(ms);
    if (!window || ms <= 0) return;
    window->setWindowOpacity(0.0);
    auto *animation = new QPropertyAnimation(window, "windowOpacity", window);
    animation->setDuration(ms);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

SlidingIndicator::SlidingIndicator(QWidget *container, const QList<QAbstractButton *> &buttons, Look look, qreal radius)
    : QWidget(container), m_look(look), m_radius(radius)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    m_animation = new QPropertyAnimation(this, "geometry", this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    container->installEventFilter(this);
    for (auto *button : buttons) {
        m_buttons.append(button);
        button->installEventFilter(this);
        // A checkable button turns itself on before its group clears the old one,
        // so follow the button that just switched on rather than scanning the group.
        connect(button, &QAbstractButton::toggled, this, [this, button](bool on) {
            if (on) { m_current = button; follow(true); }
        });
    }
    lower();
    follow(false);
}

QAbstractButton *SlidingIndicator::checkedButton() const
{
    if (m_current && m_current->isChecked() && m_current->isVisibleTo(parentWidget())) return m_current;
    for (const auto &button : m_buttons)
        if (button && button->isChecked() && button->isVisibleTo(parentWidget())) return button;
    return nullptr;
}

void SlidingIndicator::follow(bool animate)
{
    auto *button = checkedButton();
    if (!button) { hide(); return; }
    const QRect target = button->geometry();
    const int ms = animate && isVisible() && parentWidget()->isVisible() ? duration(Normal) : 0;
    m_animation->stop();
    if (ms <= 0 || geometry().isEmpty()) {
        setGeometry(target);
    } else {
        m_animation->setDuration(ms);
        m_animation->setStartValue(geometry());
        m_animation->setEndValue(target);
        m_animation->start();
    }
    show();
    lower();
}

bool SlidingIndicator::eventFilter(QObject *object, QEvent *event)
{
    // Layout passes move the buttons without toggling them; snap along silently.
    switch (event->type()) {
    case QEvent::Move: case QEvent::Resize: case QEvent::Show: case QEvent::LayoutRequest:
        if (m_animation->state() != QAbstractAnimation::Running || object == parent()) follow(false);
        break;
    default: break;
    }
    return QWidget::eventFilter(object, event);
}

void SlidingIndicator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto &c = Theme::c();
    painter.setPen(m_look == Look::Outlined ? QPen(c.border, 1) : Qt::NoPen);
    painter.setBrush(c.surface);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
}

} // namespace Motion

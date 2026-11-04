// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Motion.h"
#include "Theme.h"

#include <QAbstractAnimation>
#include <QAbstractButton>
#include <QBasicTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QLayout>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>
#include <QTimerEvent>
#include <QVariantAnimation>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace Motion {

struct SnapshotFade::Trace {
    QElapsedTimer clock;
    qint64 grabMs = 0, startedAt = -1, last = -1, worst = 0;
    int frames = 0;
    QString name;
};

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

namespace {
class DisplayClock final : public QAnimationDriver {
public:
    explicit DisplayClock(QObject *parent) : QAnimationDriver(parent) {}
    void setRefreshRate(qreal hz)
    {
        // 4 ms floor keeps a 240 Hz panel fed without spinning; 16 ms is Qt's default.
        m_interval = std::chrono::milliseconds(qBound(4, qRound(1000.0 / qMax<qreal>(hz, 30.0)), 16));
        if (m_timer.isActive()) m_timer.start(m_interval, Qt::PreciseTimer, this);
    }

protected:
    void start() override { QAnimationDriver::start(); m_timer.start(m_interval, Qt::PreciseTimer, this); }
    void stop() override { m_timer.stop(); QAnimationDriver::stop(); }
    void timerEvent(QTimerEvent *event) override
    {
        if (event->timerId() == m_timer.timerId()) advance();
        else QAnimationDriver::timerEvent(event);
    }

private:
    QBasicTimer m_timer;
    std::chrono::milliseconds m_interval{16};
};

// What the window shows over target's rectangle, parents' backgrounds included,
// so the fade layer can paint every pixel itself.
QPixmap grabArea(QWidget *target)
{
    QWidget *window = target->window();
    return window->grab(QRect(target->mapTo(window, QPoint(0, 0)), target->size()));
}
} // namespace

void installDisplayClock()
{
    auto *clock = new DisplayClock(qApp);
    // The fastest screen sets the pace; the timer only runs while something animates.
    auto update = [clock] {
        qreal hz = 60;
        for (const auto *screen : QGuiApplication::screens()) hz = qMax(hz, screen->refreshRate());
        clock->setRefreshRate(hz);
    };
    auto watch = [update](QScreen *screen) { QObject::connect(screen, &QScreen::refreshRateChanged, qApp, update); };
    for (auto *screen : QGuiApplication::screens()) watch(screen);
    QObject::connect(qApp, &QGuiApplication::screenAdded, qApp, [update, watch](QScreen *screen) { watch(screen); update(); });
    QObject::connect(qApp, &QGuiApplication::screenRemoved, qApp, update);
    update();
    clock->install();
}

SnapshotFade::SnapshotFade(QWidget *target, int ms)
    : QWidget(target->parentWidget() ? target->parentWidget() : target)
    , m_target(target)
{
    // Capture first: an older fade still on screen becomes part of this snapshot,
    // so rapid changes blend instead of jumping.
    if (qEnvironmentVariableIsSet("ANIME_NEXUS_MOTION_TRACE")) {
        m_trace = new Trace;
        m_trace->clock.start();
        m_trace->name = target->objectName().isEmpty() ? QString::fromLatin1(target->metaObject()->className()) : target->objectName();
    }
    m_before = grabArea(target);
    if (m_trace) m_trace->grabMs = m_trace->clock.elapsed();
    for (auto *older : parentWidget()->findChildren<SnapshotFade *>(Qt::FindDirectChildrenOnly))
        if (older != this && older->m_target == target) { older->hide(); older->deleteLater(); }
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::NoFocus);
    setGeometry(parentWidget() == target ? target->rect() : target->geometry());
    target->installEventFilter(this);
    connect(target, &QObject::destroyed, this, &SnapshotFade::finish);
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
        // Stay above anything raised beside the target since the fade began.
        if (parent()->children().constLast() != this) raise();
        m_opacity = value.toReal();
        update();
    });
    connect(animation, &QVariantAnimation::finished, this, &SnapshotFade::finish);
}

SnapshotFade::~SnapshotFade()
{
    if (!m_trace) return;
    qInfo().noquote() << QStringLiteral("motion %1: grab %2 ms, wait %3 ms, %4 frames in %5 ms, worst gap %6 ms")
        .arg(m_trace->name).arg(m_trace->grabMs).arg(m_trace->startedAt).arg(m_trace->frames)
        .arg(m_trace->startedAt < 0 ? 0 : m_trace->clock.elapsed() - m_trace->startedAt).arg(m_trace->worst);
    delete m_trace;
}

void SnapshotFade::finish()
{
    // Hidden at once: deletion is deferred to the event loop, and a finished layer
    // must not keep covering live content until then.
    hide();
    deleteLater();
}

void SnapshotFade::begin()
{
    if (m_animation->state() != QAbstractAnimation::Stopped || m_opacity < 1.0)
        return;
    // Freeze the new look as well. Hidden for the capture only; both changes land
    // before the next paint, so nothing flickers.
    if (!m_target) return;
    hide();
    m_after = grabArea(m_target);
    show();
    if (m_trace) m_trace->startedAt = m_trace->last = m_trace->clock.elapsed();
    m_animation->start();
}

bool SnapshotFade::eventFilter(QObject *object, QEvent *event)
{
    // The stills no longer match a moved, resized or hidden target; finish at once.
    if (object == m_target && (event->type() == QEvent::Resize || event->type() == QEvent::Move
                               || event->type() == QEvent::Hide)) finish();
    return QWidget::eventFilter(object, event);
}

void SnapshotFade::paintEvent(QPaintEvent *)
{
    if (m_trace && m_trace->startedAt >= 0) {
        const qint64 now = m_trace->clock.elapsed();
        m_trace->worst = qMax(m_trace->worst, now - m_trace->last);
        m_trace->last = now; ++m_trace->frames;
    }
    QPainter painter(this);
    // Source copies alpha too, which keeps translucent dialog corners correct.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    if (!m_after.isNull()) {
        painter.drawPixmap(0, 0, m_after);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setOpacity(m_opacity);
    }
    painter.drawPixmap(0, 0, m_before);
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
        // The caller restyles the app right after this returns, and the window
        // repaints in the new theme on the next pass. Paint the snapshot now so it
        // already covers the window when that happens.
        repaint();
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
    // Outlined thumbs sit on a segmented track and must read as raised in both themes.
    const bool outlined = m_look == Look::Outlined;
    painter.setPen(outlined ? QPen(c.segmentThumbBorder, 1) : Qt::NoPen);
    painter.setBrush(outlined ? c.segmentThumb : c.surface);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
}

} // namespace Motion

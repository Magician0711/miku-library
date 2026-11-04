// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

class QAbstractButton;
class QPropertyAnimation;
class QVariantAnimation;

// Small, consistent motion vocabulary. Every helper degrades to an instant change
// when Windows "Show animations" is off or the widget is not on screen.
namespace Motion {

constexpr int Fast = 140;    // hover, popups
constexpr int Normal = 200;  // selection, expand / collapse, page changes
constexpr int Slow = 320;    // whole-window theme change

bool enabled();
// Steps Qt animations at the display's refresh rate instead of Qt's fixed 16 ms
// timer, which judders on 120 / 144 Hz screens. Call once after QApplication.
void installDisplayClock();
// Scales a duration to 0 when the user has turned animations off.
int duration(int ms);

// Freezes the current look of target and fades it out over whatever is drawn next.
// Call immediately before changing the content (switching a page, applying a theme).
void crossfade(QWidget *target, int ms = Normal);

// Whole-window variant for theme changes: the snapshot lives in its own
// click-through overlay window, so the compositor blends it over the app no matter
// how long the restyled window takes to repaint underneath.
void crossfadeWindow(QWidget *window, int ms = Slow);

// Animates a collapsible section open or closed by its height, then restores
// normal layout (visible + unbounded height, or hidden).
void setExpanded(QWidget *section, bool expanded, int ms = Normal);

// Fades a freshly shown top-level popup or dialog in from transparent.
void fadeIn(QWidget *window, int ms = Fast);

// The layer used by crossfade(); exposed for tests. It is fully opaque: it blends a
// still of the old look over a still of the new one, so nothing underneath has to
// repaint while the fade runs. It sits beside the target (a sibling stacked just
// above it), never inside it: a QStackedWidget raises the page it switches to above
// its other children, which would otherwise flash the new page before the fade.
class SnapshotFade final : public QWidget {
    Q_OBJECT
public:
    SnapshotFade(QWidget *target, int ms);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void begin();
    void finish();
    QPointer<QWidget> m_target;
    QPixmap m_before;
    QPixmap m_after;
    qreal m_opacity = 1.0;
    QVariantAnimation *m_animation;
    struct Trace;
    Trace *m_trace = nullptr;   // frame log, only with ANIME_NEXUS_MOTION_TRACE set
public:
    ~SnapshotFade() override;
};

// A rounded highlight that glides between the checked buttons of a group, so a
// selection change reads as movement instead of two backgrounds swapping.
// The buttons keep their text styling; their checked background must be transparent.
class SlidingIndicator final : public QWidget {
public:
    enum class Look { Plain, Outlined };
    SlidingIndicator(QWidget *container, const QList<QAbstractButton *> &buttons, Look look, qreal radius);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QAbstractButton *checkedButton() const;
    void follow(bool animate);
    QList<QPointer<QAbstractButton>> m_buttons;
    QPointer<QAbstractButton> m_current;
    QPropertyAnimation *m_animation;
    Look m_look;
    qreal m_radius;
};

} // namespace Motion

// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractNativeEventFilter>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

class QQuickWindow;

// What the window frame does on each system.
//
// Windows: a frameless window that still behaves like a Windows one: edge resizing, snap,
// the native shadow, the work area when maximised, and no unpainted first frame.
// macOS: the system title bar, made transparent so the interface runs under it; the
// traffic-light buttons, corners, shadow and resizing are the system's own.
// Linux: a frameless window; the interface draws its own buttons and resize edges.
class WindowChrome final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    // macOS: the system draws the window buttons at the top left (see leadingInset).
    Q_PROPERTY(bool systemButtons READ systemButtons CONSTANT)
    // Linux: the interface provides the resize edges itself.
    Q_PROPERTY(bool resizeEdges READ resizeEdges CONSTANT)
    // Height kept clear at the top left for the system's window buttons.
    Q_PROPERTY(int buttonInset READ buttonInset CONSTANT)
    // The desktop shows through the window, blurred by the system (Acrylic on Windows 11,
    // the behind-window material on macOS); the interface then paints its background as a
    // tint. False where the system has no such material, or transparency is turned off.
    Q_PROPERTY(bool translucent READ translucent NOTIFY translucentChanged)
public:
    explicit WindowChrome(QObject *parent = nullptr);
    ~WindowChrome() override;

    bool systemButtons() const;
    bool resizeEdges() const;
    int buttonInset() const { return systemButtons() ? 28 : 0; }

    bool translucent() const { return m_translucent; }

    // Call before the window is shown.
    Q_INVOKABLE void attach(QQuickWindow *window);
    Q_INVOKABLE void setDark(bool dark);
    // Asks for the see-through background; applies at attach() or at once if attached.
    Q_INVOKABLE void setBackdrop(bool wanted);
    // With MIKU_LIBRARY_MOTION_TRACE set, logs the frames presented between these calls.
    Q_INVOKABLE void beginTrace(const QString &name);
    Q_INVOKABLE void endTrace();

    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override;

signals:
    void translucentChanged();

private:
    void applyFrame();
    void applyBackdrop();
    void clearSurfaceLater();
    void reveal();

    QPointer<QQuickWindow> m_window;
    quintptr m_hwnd = 0;
    bool m_cloaked = false;
    bool m_dark = false;
    bool m_backdropWanted = false;
    bool m_translucent = false;
    bool m_clearQueued = false;
    QString m_traceName;
    QElapsedTimer m_traceClock;
    qint64 m_traceLast = 0, m_traceWorst = 0;
    int m_traceFrames = 0;
    QMetaObject::Connection m_traceConnection;
};

#ifdef Q_OS_MACOS
class QWindow;
// WindowChrome_mac.mm
namespace MacWindow {
void prepare(QWindow *window);
void setDark(QWindow *window, bool dark);
// True when the behind-window material is in place.
bool setBackdrop(QWindow *window, bool on);
}
#endif

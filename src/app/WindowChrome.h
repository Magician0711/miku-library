// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractNativeEventFilter>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

class QQuickWindow;

// A frameless window that still behaves like a Windows one: edge resizing, snap,
// the native shadow, the work area when maximised, and no unpainted first frame.
class WindowChrome final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit WindowChrome(QObject *parent = nullptr);
    ~WindowChrome() override;

    // Call before the window is shown.
    Q_INVOKABLE void attach(QQuickWindow *window);
    Q_INVOKABLE void setDark(bool dark);
    // With MIKU_LIBRARY_MOTION_TRACE set, logs the frames presented between these calls.
    Q_INVOKABLE void beginTrace(const QString &name);
    Q_INVOKABLE void endTrace();

    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override;

private:
    void applyFrame();
    void reveal();

    QPointer<QQuickWindow> m_window;
    quintptr m_hwnd = 0;
    bool m_cloaked = false;
    bool m_dark = false;
    QString m_traceName;
    QElapsedTimer m_traceClock;
    qint64 m_traceLast = 0, m_traceWorst = 0;
    int m_traceFrames = 0;
    QMetaObject::Connection m_traceConnection;
};

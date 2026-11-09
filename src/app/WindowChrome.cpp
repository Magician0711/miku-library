// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "WindowChrome.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

namespace {
constexpr DWORD kCloak = 13;      // DWMWA_CLOAK
constexpr DWORD kDarkMode = 20;   // DWMWA_USE_IMMERSIVE_DARK_MODE
constexpr DWORD kBorderColor = 34;   // DWMWA_BORDER_COLOR
constexpr COLORREF kNoBorder = 0xFFFFFFFE;   // DWMWA_COLOR_NONE
constexpr int kEdge = 7;
}

WindowChrome::WindowChrome(QObject *parent) : QObject(parent)
{
    qApp->installNativeEventFilter(this);
}

WindowChrome::~WindowChrome()
{
    qApp->removeNativeEventFilter(this);
}

void WindowChrome::attach(QQuickWindow *window)
{
    m_window = window;
    m_hwnd = quintptr(window->winId());
    applyFrame();
    // Keep the window invisible until its first frame is drawn; otherwise Windows
    // shows an unpainted client area for a moment on launch.
    const BOOL cloak = TRUE;
    if (SUCCEEDED(DwmSetWindowAttribute(HWND(m_hwnd), kCloak, &cloak, sizeof(cloak)))) {
        m_cloaked = true;
        connect(window, &QQuickWindow::frameSwapped, this, &WindowChrome::reveal, Qt::QueuedConnection);
        QTimer::singleShot(1500, this, &WindowChrome::reveal);   // never stay hidden
    }
}

void WindowChrome::applyFrame()
{
    const auto hwnd = HWND(m_hwnd);
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    const LONG_PTR desired = style | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    if (style != desired) {
        SetWindowLongPtr(hwnd, GWL_STYLE, desired);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
    // DWM supplies the native shadow without a translucent window.
    const MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    // Its radius is not the window's, so the interface draws the border itself.
    DwmSetWindowAttribute(hwnd, kBorderColor, &kNoBorder, sizeof(kNoBorder));
    setDark(m_dark);
}

void WindowChrome::setDark(bool dark)
{
    m_dark = dark;
    if (!m_hwnd)
        return;
    const BOOL value = dark;
    DwmSetWindowAttribute(HWND(m_hwnd), kDarkMode, &value, sizeof(value));
}

void WindowChrome::beginTrace(const QString &name)
{
    if (!m_window || !qEnvironmentVariableIsSet("MIKU_LIBRARY_MOTION_TRACE"))
        return;
    endTrace();
    m_traceName = name;
    m_traceFrames = 0;
    m_traceWorst = 0;
    m_traceLast = 0;
    m_traceClock.start();
    m_traceConnection = connect(m_window, &QQuickWindow::frameSwapped, this, [this] {
        const qint64 now = m_traceClock.elapsed();
        m_traceWorst = qMax(m_traceWorst, now - m_traceLast);
        m_traceLast = now;
        ++m_traceFrames;
    }, Qt::DirectConnection);
}

void WindowChrome::endTrace()
{
    if (!m_traceConnection)
        return;
    disconnect(m_traceConnection);
    qInfo().noquote() << QStringLiteral("motion %1: %2 frames in %3 ms, worst gap %4 ms")
        .arg(m_traceName).arg(m_traceFrames).arg(m_traceClock.elapsed()).arg(m_traceWorst);
}

void WindowChrome::reveal()
{
    if (!m_cloaked)
        return;
    m_cloaked = false;
    if (m_window)
        disconnect(m_window, &QQuickWindow::frameSwapped, this, &WindowChrome::reveal);
    const BOOL cloak = FALSE;
    DwmSetWindowAttribute(HWND(m_hwnd), kCloak, &cloak, sizeof(cloak));
}

bool WindowChrome::nativeEventFilter(const QByteArray &, void *message, qintptr *result)
{
    const auto *msg = static_cast<MSG *>(message);
    if (!m_hwnd || msg->hwnd != HWND(m_hwnd))
        return false;
    if (msg->message == WM_NCCALCSIZE && msg->wParam) {
        if (IsZoomed(msg->hwnd)) {
            auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
            MONITORINFO monitor = {sizeof(MONITORINFO)};
            if (GetMonitorInfo(MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                params->rgrc[0] = monitor.rcWork;
        }
        *result = 0;
        return true;
    }
    if (msg->message == WM_NCHITTEST && m_window && !IsZoomed(msg->hwnd)) {
        // Actual message coordinates through the native client rect, so negative monitor
        // origins and per-monitor DPI work. Do not use unsigned LOWORD here.
        POINT origin = {0, 0};
        ClientToScreen(msg->hwnd, &origin);
        const qreal scale = m_window->devicePixelRatio();
        const QPoint p(qRound((GET_X_LPARAM(msg->lParam) - origin.x) / scale),
                       qRound((GET_Y_LPARAM(msg->lParam) - origin.y) / scale));
        const bool left = p.x() < kEdge, right = p.x() >= m_window->width() - kEdge;
        const bool top = p.y() < kEdge, bottom = p.y() >= m_window->height() - kEdge;
        if (top || bottom || left || right) {
            *result = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP)
                      : bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                      : left ? HTLEFT : HTRIGHT;
            return true;
        }
        *result = HTCLIENT;
        return true;
    }
    return false;
}

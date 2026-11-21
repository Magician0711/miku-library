// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "WindowChrome.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

namespace {
constexpr DWORD kCloak = 13;      // DWMWA_CLOAK
constexpr DWORD kDarkMode = 20;   // DWMWA_USE_IMMERSIVE_DARK_MODE
constexpr DWORD kBorderColor = 34;   // DWMWA_BORDER_COLOR
constexpr COLORREF kNoBorder = 0xFFFFFFFE;   // DWMWA_COLOR_NONE
constexpr DWORD kCornerPreference = 33;   // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD kRoundCorners = 2;        // DWMWCP_ROUND
constexpr DWORD kSystemBackdrop = 38;     // DWMWA_SYSTEMBACKDROP_TYPE, Windows 11 22H2
constexpr DWORD kNoBackdrop = 1;          // DWMSBT_NONE
constexpr DWORD kAcrylic = 3;             // DWMSBT_TRANSIENTWINDOW
constexpr int kEdge = 7;
}
#endif

WindowChrome::WindowChrome(QObject *parent) : QObject(parent)
{
    qApp->installNativeEventFilter(this);
}

WindowChrome::~WindowChrome()
{
    qApp->removeNativeEventFilter(this);
}

bool WindowChrome::systemButtons() const
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

bool WindowChrome::resizeEdges() const
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return false;
#else
    return true;
#endif
}

void WindowChrome::attach(QQuickWindow *window)
{
    m_window = window;
#ifdef Q_OS_MACOS
    MacWindow::prepare(window);
    setDark(m_dark);
    applyBackdrop();
#endif
#ifdef Q_OS_WIN
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
#endif
}

void WindowChrome::applyFrame()
{
#ifdef Q_OS_WIN
    const auto hwnd = HWND(m_hwnd);
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    // The system menu style is applyBackdrop's to decide.
    const LONG_PTR desired = style | WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
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
#endif
    applyBackdrop();
}

void WindowChrome::setBackdrop(bool wanted)
{
    m_backdropWanted = wanted;
    if (m_window)
        applyBackdrop();
}

void WindowChrome::applyBackdrop()
{
    bool applied = false;
#ifdef Q_OS_WIN
    if (!m_hwnd)
        return;
    const auto hwnd = HWND(m_hwnd);
    // The whole client area becomes the frame, so the material shows wherever the
    // interface leaves it uncovered. Windows before 11 22H2 refuses the attribute.
    if (m_backdropWanted)
        applied = SUCCEEDED(DwmSetWindowAttribute(hwnd, kSystemBackdrop, &kAcrylic, sizeof(kAcrylic)));
    else if (m_translucent)
        DwmSetWindowAttribute(hwnd, kSystemBackdrop, &kNoBackdrop, sizeof(kNoBackdrop));
    const MARGINS margins = applied ? MARGINS{-1, -1, -1, -1} : MARGINS{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    // With the desktop showing through, the window has no title bar at all: no caption and
    // no system menu, only the sizing frame and the minimize and maximize boxes. Whatever
    // Windows paints for a title bar showed through the see-through interface: DWM's own
    // caption buttons beside the interface's, and at times the unthemed Windows 7 caption
    // and frame. Without a title bar there is nothing for either to draw. The sizing frame
    // keeps edge resizing and snapping; the taskbar still minimizes and restores the
    // window. What goes is Alt+Space and Windows' own minimize and maximize animation.
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    const LONG_PTR titleBar = WS_CAPTION | WS_SYSMENU;
    const LONG_PTR desired = applied ? style & ~titleBar : style | titleBar;
    if (style != desired) {
        SetWindowLongPtr(hwnd, GWL_STYLE, desired);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
    // The material fills the window's rectangle, so the window takes the system's rounded
    // corners and the interface follows their radius (Main.qml).
    if (applied) {
        DwmSetWindowAttribute(hwnd, kCornerPreference, &kRoundCorners, sizeof(kRoundCorners));
        DefWindowProcW(hwnd, WM_NCACTIVATE, TRUE, -1);
    }
#elif defined(Q_OS_MACOS)
    if (m_window)
        applied = MacWindow::setBackdrop(m_window, m_backdropWanted);
#endif
    if (applied != m_translucent) {
        m_translucent = applied;
        emit translucentChanged();
    }
    clearSurfaceLater();
}

void WindowChrome::setDark(bool dark)
{
    m_dark = dark;
#ifdef Q_OS_WIN
    if (!m_hwnd)
        return;
    const BOOL value = dark;
    DwmSetWindowAttribute(HWND(m_hwnd), kDarkMode, &value, sizeof(value));
#elif defined(Q_OS_MACOS)
    // The traffic lights follow the window's appearance, not the app's palette.
    if (m_window)
        MacWindow::setDark(m_window, dark);
#endif
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
#ifdef Q_OS_WIN
    if (!m_cloaked)
        return;
    m_cloaked = false;
    if (m_window)
        disconnect(m_window, &QQuickWindow::frameSwapped, this, &WindowChrome::reveal);
    const BOOL cloak = FALSE;
    DwmSetWindowAttribute(HWND(m_hwnd), kCloak, &cloak, sizeof(cloak));
    clearSurfaceLater();
#endif
}

#ifdef Q_OS_WIN
namespace {
// Under the interface's own surface each window keeps the surface Windows draws on with
// GDI. Whatever lands there stays: the old unthemed caption and frame, painted before the
// window's own handling was in place or by a message this filter does not see, showed
// through the see-through background for as long as the window lived. Over the system's
// blur, black there counts as clear, so the whole of it is painted black.
void clearSystemSurface(HWND hwnd)
{
    RECT window;
    if (!GetWindowRect(hwnd, &window))
        return;
    const RECT all = {0, 0, window.right - window.left, window.bottom - window.top};
    if (HDC dc = GetWindowDC(hwnd)) {
        FillRect(dc, &all, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        ReleaseDC(hwnd, dc);
    }
}
}
#endif

void WindowChrome::clearSurfaceLater()
{
#ifdef Q_OS_WIN
    // After Windows has finished with the message that may have drawn there.
    if (m_clearQueued || !m_translucent)
        return;
    m_clearQueued = true;
    QMetaObject::invokeMethod(this, [this] {
        m_clearQueued = false;
        if (m_hwnd && m_translucent)
            clearSystemSurface(HWND(m_hwnd));
    }, Qt::QueuedConnection);
#endif
}

bool WindowChrome::nativeEventFilter(const QByteArray &, void *message, qintptr *result)
{
#ifndef Q_OS_WIN
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
#else
    const auto *msg = static_cast<MSG *>(message);
    if (!m_hwnd || msg->hwnd != HWND(m_hwnd))
        return false;
    switch (msg->message) {
    case WM_NCACTIVATE: case WM_SETTEXT: case WM_SETICON: case WM_NCPAINT:
    case WM_SIZE: case WM_SHOWWINDOW: case WM_THEMECHANGED: case WM_DWMCOMPOSITIONCHANGED:
    case WM_SYSCOLORCHANGE: case WM_EXITSIZEMOVE: case WM_SETTINGCHANGE:
        clearSurfaceLater();
        break;
    }
    // These paint the unthemed Windows 7 caption straight over the window: activation
    // redraws the frame, and a new title or icon redraws the caption. lParam -1 tells
    // DefWindowProc to update the activation state without painting.
    // DWM swaps the blurred backdrop for a flat colour while the frame looks inactive. The
    // interface draws its own frame, so with the desktop showing through the frame always
    // stays active and the blur stays when another window takes the focus.
    if (msg->message == WM_NCACTIVATE) {
        const WPARAM active = m_translucent ? TRUE : msg->wParam;
        *result = DefWindowProcW(msg->hwnd, msg->message, active, -1);
        return true;
    }
    // The theme engine draws the caption and frame itself through these two undocumented
    // messages, in the old unthemed style when the window has no themed caption to draw;
    // Chromium blocks them for the same reason. Through the see-through background the
    // frame Windows paints would show inside the window, and there is no frame area to
    // paint anyway: the client area covers the whole window.
    constexpr UINT kNcUahDrawCaption = 0x00AE;
    constexpr UINT kNcUahDrawFrame = 0x00AF;
    if (msg->message == kNcUahDrawCaption || msg->message == kNcUahDrawFrame
        || (msg->message == WM_NCPAINT && m_translucent)) {
        *result = 0;
        return true;
    }
    if (msg->message == WM_SETTEXT || msg->message == WM_SETICON) {
        // Hidden for the moment of the change, so there is nothing to paint on.
        const LONG_PTR style = GetWindowLongPtr(msg->hwnd, GWL_STYLE);
        if (style & WS_VISIBLE)
            SetWindowLongPtr(msg->hwnd, GWL_STYLE, style & ~LONG_PTR(WS_VISIBLE));
        *result = DefWindowProcW(msg->hwnd, msg->message, msg->wParam, msg->lParam);
        if (style & WS_VISIBLE)
            SetWindowLongPtr(msg->hwnd, GWL_STYLE, style);
        return true;
    }
    // Without wParam Windows asks for the client rectangle of a proposed window rectangle;
    // left to DefWindowProc it would answer with a frame, which then gets painted.
    if (msg->message == WM_NCCALCSIZE && !msg->wParam) {
        *result = 0;
        return true;
    }
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
    // Without a title bar Qt maximizes the window by its geometry, and Windows does not
    // report it as zoomed: the window's own state says whether the edges resize.
    if (msg->message == WM_NCHITTEST && m_window && !IsZoomed(msg->hwnd)
        && m_window->visibility() != QWindow::Maximized && m_window->visibility() != QWindow::FullScreen) {
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
#endif
}

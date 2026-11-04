#include "FramelessWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

namespace {
// Draw caption glyphs ourselves so they remain crisp at any display scale.
QPixmap captionGlyph(int kind, const QColor &color)
{
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pixmap(QSize(16, 16) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.3));
    if (kind == 0) painter.drawLine(QPointF(3, 10), QPointF(13, 10));
    if (kind == 1) painter.drawRect(QRectF(3.5, 3.5, 9, 9));
    if (kind == 2) {
        painter.drawRect(QRectF(5.5, 2.5, 7, 7));
        painter.fillRect(QRectF(2.5, 5.5, 7, 7), QColor(Theme::c().chrome));
        painter.drawRect(QRectF(2.5, 5.5, 7, 7));
    }
    if (kind == 3) {
        painter.drawLine(QPointF(4, 4), QPointF(12, 12));
        painter.drawLine(QPointF(12, 4), QPointF(4, 12));
    }
    return pixmap;
}

// The close glyph turns white on its red hover fill; QToolButton uses the Active
// icon mode for hovered auto-raise buttons.
QIcon captionIcon(int kind)
{
    QIcon icon(captionGlyph(kind, QColor(Theme::c().text2)));
    if (kind == 3)
        icon.addPixmap(captionGlyph(kind, Qt::white), QIcon::Active);
    return icon;
}
}

FramelessWindow::FramelessWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("TitleBar");
    m_titleBar->setAttribute(Qt::WA_StyledBackground, true);
    m_titleBar->setFixedHeight(44);
    auto *row = new QHBoxLayout(m_titleBar);
    row->setContentsMargins(16, 0, 0, 0);
    row->setSpacing(0);
    auto *mark = new QLabel(QStringLiteral("N"), m_titleBar);
    mark->setObjectName("AppMark");
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(24, 24);
    auto *name = new QLabel(QStringLiteral("Anime Nexus"), m_titleBar);
    name->setObjectName("TitleBrand");
    row->addWidget(mark);
    row->addSpacing(10);
    row->addWidget(name);
    row->addSpacing(10);
    auto *separator = new QLabel(QStringLiteral("/"), m_titleBar);
    separator->setObjectName("TitleSeparator");
    row->addWidget(separator);
    row->addSpacing(10);
    m_context = new QLabel(QStringLiteral("本季放送"), m_titleBar);
    m_context->setObjectName("TitleContext");
    row->addWidget(m_context);
    row->addStretch(1);

    auto makeButton = [this, row](const char *id, const QString &tip, int glyph) {
        auto *button = new QToolButton(m_titleBar);
        button->setObjectName(id);
        button->setAccessibleName(tip);
        button->setToolTip(tip);
        button->setFixedSize(46, 44);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIcon(captionIcon(glyph));
        row->addWidget(button);
        return button;
    };
    m_minimize = makeButton("WindowMinimize", QStringLiteral("最小化"), 0);
    m_maximize = makeButton("WindowMaximize", QStringLiteral("最大化"), 1);
    m_close = makeButton("WindowClose", QStringLiteral("关闭"), 3);
    auto *minimize = m_minimize, *close = m_close;
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximize, &QToolButton::clicked, this, [this] { toggleMaximized(); });
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    m_titleBar->installEventFilter(this);
    connect(Theme::notifier(), &Theme::Notifier::changed, this, &FramelessWindow::applyThemeToChrome);
    for (auto *label : m_titleBar->findChildren<QLabel *>()) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
}

void FramelessWindow::setContextTitle(const QString &title)
{
    m_context->setText(title);
}

void FramelessWindow::addTitleWidget(QWidget *widget)
{
    auto *row = static_cast<QHBoxLayout *>(m_titleBar->layout());
    row->insertWidget(row->indexOf(m_minimize), widget, 0, Qt::AlignVCenter);
    row->insertSpacing(row->indexOf(m_minimize), 8);
}

void FramelessWindow::toggleMaximized()
{
    isMaximized() ? showNormal() : showMaximized();
}

void FramelessWindow::updateControls()
{
    const QString tip = isMaximized() ? QStringLiteral("还原") : QStringLiteral("最大化");
    m_maximize->setToolTip(tip);
    m_maximize->setAccessibleName(tip);
    m_maximize->setIcon(captionIcon(isMaximized() ? 2 : 1));
}

void FramelessWindow::applyThemeToChrome()
{
    // Caption glyphs are pixmaps; redraw them in the new palette.
    m_minimize->setIcon(captionIcon(0));
    m_close->setIcon(captionIcon(3));
    updateControls();
#ifdef Q_OS_WIN
    if (testAttribute(Qt::WA_WState_Created)) {
        const BOOL dark = Theme::c().dark;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    }
#endif
}

void FramelessWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        updateControls();
}

void FramelessWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
#ifdef Q_OS_WIN
    const auto hwnd = reinterpret_cast<HWND>(winId());
    // Keep the window invisible until its first frame is drawn; otherwise Windows
    // shows an unpainted client area for a moment on launch.
    if (!m_revealed && !m_cloaked && windowHandle()) {
        const BOOL cloak = TRUE;
        if (SUCCEEDED(DwmSetWindowAttribute(hwnd, 13 /* DWMWA_CLOAK */, &cloak, sizeof(cloak)))) {
            m_cloaked = true;
            windowHandle()->installEventFilter(this);
            QTimer::singleShot(600, this, &FramelessWindow::reveal);   // never stay hidden
        }
    }
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    const LONG_PTR desired = style | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU
                                  | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    if (style != desired) {
        SetWindowLongPtr(hwnd, GWL_STYLE, desired);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
    // DWM supplies the native shadow without a translucent full-window backing store.
    const MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    const BOOL dark = Theme::c().dark;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
#endif
    updateControls();
}

bool FramelessWindow::isDragArea(const QPoint &position) const
{
    const QPoint local = m_titleBar->mapFrom(this, position);
    if (!m_titleBar->rect().contains(local)) return false;
    const QWidget *child = m_titleBar->childAt(local);
    return !child || child == m_titleBar;
}

void FramelessWindow::reveal()
{
    m_revealed = true;
    if (!m_cloaked) return;
    m_cloaked = false;
    if (windowHandle()) windowHandle()->removeEventFilter(this);
#ifdef Q_OS_WIN
    const BOOL cloak = FALSE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 13 /* DWMWA_CLOAK */, &cloak, sizeof(cloak));
#endif
}

bool FramelessWindow::eventFilter(QObject *object, QEvent *event)
{
    // The first expose paints and flushes synchronously; uncloak right after it.
    if (m_cloaked && object == windowHandle() && event->type() == QEvent::Expose)
        QTimer::singleShot(0, this, &FramelessWindow::reveal);
    if (object == m_titleBar) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) { toggleMaximized(); return true; }
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && windowHandle())
                return windowHandle()->startSystemMove();
        }
    }
    return QMainWindow::eventFilter(object, event);
}

bool FramelessWindow::nativeEvent(const QByteArray &type, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG *>(message);
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
    if (msg->message == WM_NCHITTEST && !isFullScreen()) {
        // Map actual message coordinates through Qt's native client rect, including
        // negative monitor origins and per-monitor DPI. Do not use unsigned LOWORD.
        POINT origin = {0, 0};
        ClientToScreen(msg->hwnd, &origin);
        const qreal scale = devicePixelRatioF();
        const QPoint p(qRound((GET_X_LPARAM(msg->lParam) - origin.x) / scale),
                       qRound((GET_Y_LPARAM(msg->lParam) - origin.y) / scale));
        const int edge = 7;
        if (!isMaximized()) {
            const bool left = p.x() < edge, right = p.x() >= width() - edge;
            const bool top = p.y() < edge, bottom = p.y() >= height() - edge;
            if (top || bottom || left || right) {
                *result = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP)
                          : bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                          : left ? HTLEFT : HTRIGHT;
                return true;
            }
        }
        if (isDragArea(p)) { *result = HTCAPTION; return true; }
        *result = HTCLIENT;
        return true;
    }
#endif
    return QMainWindow::nativeEvent(type, message, result);
}

// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "TrayIcon.h"

#include "ImageProviders.h"

#include <QImage>
#include <QPixmap>

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

namespace {
constexpr UINT kCallback = WM_APP + 41;
constexpr UINT kIconId = 1;

void copyText(wchar_t *target, size_t capacity, const QString &text)
{
    const auto length = qMin<qsizetype>(text.size(), qsizetype(capacity) - 1);
    text.left(length).toWCharArray(target);
    target[length] = 0;
}

NOTIFYICONDATAW iconData(void *window)
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = static_cast<HWND>(window);
    data.uID = kIconId;
    return data;
}
}

qintptr TrayIcon::proc(void *hwnd, unsigned message, quintptr wParam, qintptr lParam)
{
    auto *self = reinterpret_cast<TrayIcon *>(GetWindowLongPtrW(static_cast<HWND>(hwnd), GWLP_USERDATA));
    if (self && message == kCallback) {
        self->handle(wParam, lParam);
        return 0;
    }
    // Explorer restarted: every icon must be added again.
    if (self && message == self->m_taskbarCreated && self->m_shown) {
        self->m_shown = false;
        self->setToolTip(QStringLiteral("Miku Library"));
        return 0;
    }
    return DefWindowProcW(static_cast<HWND>(hwnd), message, wParam, lParam);
}

TrayIcon::TrayIcon(const QStringList &items, QObject *parent)
    : QObject(parent), m_items(items)
{
    static const wchar_t *const className = L"MikuLibraryTray";
    WNDCLASSW type = {};
    type.lpfnWndProc = [](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
        return TrayIcon::proc(hwnd, message, wParam, lParam);
    };
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = className;
    RegisterClassW(&type);
    m_window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, nullptr, nullptr, type.hInstance, nullptr);
    SetWindowLongPtrW(static_cast<HWND>(m_window), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    m_icon = appIcon().pixmap(32, 32).toImage().toHICON();
    setToolTip(QStringLiteral("Miku Library"));
}

TrayIcon::~TrayIcon()
{
    hide();
    if (m_icon) DestroyIcon(static_cast<HICON>(m_icon));
    if (m_window) DestroyWindow(static_cast<HWND>(m_window));
}

void TrayIcon::setToolTip(const QString &text)
{
    auto data = iconData(m_window);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kCallback;
    data.hIcon = static_cast<HICON>(m_icon);
    copyText(data.szTip, std::size(data.szTip), text);
    Shell_NotifyIconW(m_shown ? NIM_MODIFY : NIM_ADD, &data);
    m_shown = true;
}

void TrayIcon::showMessage(const QString &title, const QString &text)
{
    auto data = iconData(m_window);
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    data.hBalloonIcon = static_cast<HICON>(m_icon);
    copyText(data.szInfoTitle, std::size(data.szInfoTitle), title);
    copyText(data.szInfo, std::size(data.szInfo), text);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayIcon::hide()
{
    if (!m_shown)
        return;
    auto data = iconData(m_window);
    Shell_NotifyIconW(NIM_DELETE, &data);
    m_shown = false;
}

void TrayIcon::handle(quintptr, qintptr lParam)
{
    switch (LOWORD(lParam)) {
    case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: emit activated(); break;
    case WM_RBUTTONUP: case WM_CONTEXTMENU: showMenu(); break;
    case NIN_BALLOONUSERCLICK: emit messageClicked(); break;
    default: break;
    }
}

void TrayIcon::showMenu()
{
    HMENU menu = CreatePopupMenu();
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).isEmpty())
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        else
            AppendMenuW(menu, MF_STRING, UINT_PTR(i + 1), reinterpret_cast<const wchar_t *>(m_items.at(i).utf16()));
    }
    POINT cursor;
    GetCursorPos(&cursor);
    // Without this the menu stays open after a click elsewhere.
    SetForegroundWindow(static_cast<HWND>(m_window));
    const int choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0,
                                      static_cast<HWND>(m_window), nullptr);
    DestroyMenu(menu);
    if (choice > 0)
        emit menuTriggered(choice - 1);
}

// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QStringList>

class QMenu;
class QSystemTrayIcon;

// The notification-area icon. On Windows it talks to the shell directly: QSystemTrayIcon
// would bring Qt Widgets into the release for this one feature. macOS (the menu bar) and
// Linux (StatusNotifierItem) use QSystemTrayIcon, in TrayIcon_qt.cpp.
class TrayIcon final : public QObject {
    Q_OBJECT
public:
    // items are the context menu entries; an empty string is a separator.
    TrayIcon(const QStringList &items, QObject *parent = nullptr);
    ~TrayIcon() override;

    void setToolTip(const QString &text);
    // The menu is built each time it opens, so new entries take effect at once.
    void setItems(const QStringList &items) { m_items = items; }
    void showMessage(const QString &title, const QString &text);
    void hide();

signals:
    void activated();
    void messageClicked();
    void menuTriggered(int index);

private:
    void handle(quintptr wParam, qintptr lParam);
    void showMenu();
    static qintptr proc(void *hwnd, unsigned message, quintptr wParam, qintptr lParam);

    void *m_window = nullptr;
    void *m_icon = nullptr;
    bool m_shown = false;
    unsigned m_taskbarCreated = 0;
    QStringList m_items;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
};

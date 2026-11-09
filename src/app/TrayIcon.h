// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QStringList>

// The notification-area icon, on the Windows shell directly: QSystemTrayIcon would
// bring Qt Widgets into the release for this one feature.
class TrayIcon final : public QObject {
    Q_OBJECT
public:
    // items are the context menu entries; an empty string is a separator.
    TrayIcon(const QStringList &items, QObject *parent = nullptr);
    ~TrayIcon() override;

    void setToolTip(const QString &text);
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
};

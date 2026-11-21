// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

// macOS and Linux: the tray through Qt. The Windows version is TrayIcon.cpp.

#include "TrayIcon.h"

#include "ImageProviders.h"

#include <QMenu>
#include <QSystemTrayIcon>

TrayIcon::TrayIcon(const QStringList &items, QObject *parent)
    : QObject(parent), m_items(items)
{
    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_menu = new QMenu;
    // Built each time it opens, so new entries take effect at once.
    connect(m_menu, &QMenu::aboutToShow, this, &TrayIcon::showMenu);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit activated();
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &TrayIcon::messageClicked);
    setToolTip(QStringLiteral("Miku Library"));
}

TrayIcon::~TrayIcon()
{
    hide();
    delete m_menu;
}

void TrayIcon::setToolTip(const QString &text)
{
    m_tray->setToolTip(text);
    // Some Linux desktops have no tray at all; the window then simply stays the way out.
    if (QSystemTrayIcon::isSystemTrayAvailable())
        m_tray->show();
    m_shown = m_tray->isVisible();
}

void TrayIcon::showMessage(const QString &title, const QString &text)
{
    m_tray->showMessage(title, text, appIcon());
}

void TrayIcon::hide()
{
    m_tray->hide();
    m_shown = false;
}

void TrayIcon::handle(quintptr, qintptr)
{
}

void TrayIcon::showMenu()
{
    m_menu->clear();
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).isEmpty())
            m_menu->addSeparator();
        else
            connect(m_menu->addAction(m_items.at(i)), &QAction::triggered, this, [this, i] { emit menuTriggered(i); });
    }
}

qintptr TrayIcon::proc(void *, unsigned, quintptr, qintptr)
{
    return 0;
}

// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

// What differs between Windows, macOS and Linux before the first window exists.
//
// Windows draws with Direct3D 11 and talks to the shell directly, so the release carries
// no Qt Widgets. On macOS and Linux the scene graph uses OpenGL, which is what libmpv can
// draw into there, and the tray and file pickers come from Qt Widgets.
#ifdef Q_OS_WIN
#include <QGuiApplication>
using PlatformApplication = QGuiApplication;
#else
#include <QApplication>
using PlatformApplication = QApplication;
#endif

namespace Platform {

// Before the application object: the graphics API cannot change once a window is up.
inline void prepareGraphics()
{
#ifndef Q_OS_WIN
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
#ifdef Q_OS_MACOS
    // macOS only offers modern OpenGL as a core profile; the default is 2.1, too old for
    // mpv's shaders and Anime4K.
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
#endif
}

}

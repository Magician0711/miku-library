// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "EngineSettings.h"
#include "DownloaderSettings.h"

#include <QString>

struct PathConfig {
    QString downloadDir;
    QString libraryDir;
    bool useHardlink = true;

    PathConfig();
};

struct AppConfig {
    enum class DownloadDirectoryState { Ready, CreatedDefault, MissingSaved, Error };
    PathConfig paths;
    EngineSettings engine;
    DownloaderSettings downloader;
    QString theme = QStringLiteral("dark");     // legacy key, kept for older builds
    QString appearance = QStringLiteral("system"); // system | light | dark
    int disclaimerAccepted = 0;                    // accepted declaration version, 0 = never
    bool checkAppUpdates = true;                   // look for a newer release at startup
    QString closeAction = QStringLiteral("ask");   // closing the window: ask | tray | quit
    QString removeAction = QStringLiteral("ask");  // removing built-in tasks: ask | keep | delete
    bool notifyDownloads = true;                   // desktop notice when a download finishes
    QString proxyMode = QStringLiteral("system");  // system | none | http | socks5
    QString proxyHost;
    int proxyPort = 0;
    int updateIntervalHours = 0;                   // repeat the update check while running; 0 = startup only
    QString language = QStringLiteral("zh-CN");
    bool hasSavedDownloadDirectory = false;

    static QString configPath();
    // "library" inside the system Downloads folder; created on first use.
    static QString defaultDownloadDirectory();
    static QString dataDir();
    static AppConfig load();
    bool save() const;
    DownloadDirectoryState prepareDownloadDirectory(QString *error = nullptr,
                                                     const QString &downloadsRoot = {});
};

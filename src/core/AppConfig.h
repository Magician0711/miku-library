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
    QString language = QStringLiteral("zh-CN");
    bool hasSavedDownloadDirectory = false;

    static QString configPath();
    // "AnimeNexus" inside the system Downloads folder; created on first use.
    static QString defaultDownloadDirectory();
    static QString dataDir();
    static AppConfig load();
    bool save() const;
    DownloadDirectoryState prepareDownloadDirectory(QString *error = nullptr,
                                                     const QString &downloadsRoot = {});
};

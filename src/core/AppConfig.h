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
    QString language = QStringLiteral("zh-CN");
    bool hasSavedDownloadDirectory = false;

    static QString configPath();
    static QString dataDir();
    static AppConfig load();
    bool save() const;
    DownloadDirectoryState prepareDownloadDirectory(QString *error = nullptr,
                                                     const QString &downloadsRoot = {});
};

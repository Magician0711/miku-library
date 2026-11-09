// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "AppConfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString systemDownloads()
{
    const QString path=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return path.isEmpty()?QDir::homePath()+QStringLiteral("/Downloads"):path;
}
}

PathConfig::PathConfig()
    : downloadDir(QDir(systemDownloads()).filePath(QStringLiteral("library")))
    , libraryDir(QDir::homePath() + QStringLiteral("/AnimeLibrary"))
{
}

QString AppConfig::dataDir()
{
    const QString overridePath=qEnvironmentVariable("MIKU_LIBRARY_DATA_DIR").trimmed();
    if(!overridePath.isEmpty())return QDir(overridePath).absolutePath();
    return QDir::homePath() + QStringLiteral("/.miku-library");
}

QString AppConfig::configPath()
{
    return dataDir() + QStringLiteral("/config.json");
}

AppConfig AppConfig::load()
{
    AppConfig cfg;
    cfg.engine.savePath = cfg.paths.downloadDir;
    QFile file(configPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return cfg;

    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return cfg;

    const auto root = doc.object();
    const auto paths = root.value(QStringLiteral("paths")).toObject();
    if (!paths.isEmpty()) {
        cfg.paths.downloadDir = paths.value(QStringLiteral("download_dir")).toString(cfg.paths.downloadDir);
        cfg.paths.libraryDir = paths.value(QStringLiteral("library_dir")).toString(cfg.paths.libraryDir);
        cfg.paths.useHardlink = paths.value(QStringLiteral("use_hardlink")).toBool(cfg.paths.useHardlink);
    }
    cfg.theme = root.value(QStringLiteral("theme")).toString(cfg.theme);
    cfg.appearance = root.value(QStringLiteral("appearance")).toString(cfg.appearance);
    cfg.disclaimerAccepted = root.value(QStringLiteral("disclaimer_accepted")).toInt(cfg.disclaimerAccepted);
    cfg.checkAppUpdates = root.value(QStringLiteral("check_app_updates")).toBool(cfg.checkAppUpdates);
    cfg.closeAction = root.value(QStringLiteral("close_action")).toString(cfg.closeAction);
    if (cfg.closeAction != QStringLiteral("tray") && cfg.closeAction != QStringLiteral("quit"))
        cfg.closeAction = QStringLiteral("ask");
    cfg.notifyDownloads = root.value(QStringLiteral("notify_downloads")).toBool(cfg.notifyDownloads);
    cfg.removeAction = root.value(QStringLiteral("remove_action")).toString(cfg.removeAction);
    if (cfg.removeAction != QStringLiteral("keep") && cfg.removeAction != QStringLiteral("delete"))
        cfg.removeAction = QStringLiteral("ask");
    const auto proxy = root.value(QStringLiteral("proxy")).toObject();
    cfg.proxyMode = proxy.value(QStringLiteral("mode")).toString(cfg.proxyMode);
    cfg.proxyHost = proxy.value(QStringLiteral("host")).toString();
    cfg.proxyPort = proxy.value(QStringLiteral("port")).toInt();
    cfg.updateIntervalHours = qBound(0, root.value(QStringLiteral("update_interval_hours")).toInt(), 24);
    cfg.language = root.value(QStringLiteral("language")).toString(cfg.language);

    cfg.downloader=DownloaderSettings::fromJson(root.value("downloader").toObject());
    cfg.engine = EngineSettings::fromJson(root.value(QStringLiteral("engine")).toObject());
    cfg.hasSavedDownloadDirectory = !cfg.engine.savePath.trimmed().isEmpty()
        || !paths.value(QStringLiteral("download_dir")).toString().trimmed().isEmpty();
    if (cfg.engine.savePath.isEmpty())
        cfg.engine.savePath = cfg.paths.downloadDir;
    return cfg;
}

bool AppConfig::save() const
{
    QDir().mkpath(dataDir());
    QJsonObject pathsObj;
    pathsObj.insert(QStringLiteral("download_dir"), paths.downloadDir);
    pathsObj.insert(QStringLiteral("library_dir"), paths.libraryDir);
    pathsObj.insert(QStringLiteral("use_hardlink"), paths.useHardlink);

    QJsonObject root;
    root.insert(QStringLiteral("paths"), pathsObj);
    root.insert(QStringLiteral("engine"), engine.toJson());
    root.insert("downloader",downloader.toJson());
    root.insert(QStringLiteral("theme"), theme);
    root.insert(QStringLiteral("appearance"), appearance);
    root.insert(QStringLiteral("disclaimer_accepted"), disclaimerAccepted);
    root.insert(QStringLiteral("check_app_updates"), checkAppUpdates);
    root.insert(QStringLiteral("close_action"), closeAction);
    root.insert(QStringLiteral("notify_downloads"), notifyDownloads);
    root.insert(QStringLiteral("remove_action"), removeAction);
    root.insert(QStringLiteral("proxy"), QJsonObject{{QStringLiteral("mode"), proxyMode},
                                                     {QStringLiteral("host"), proxyHost},
                                                     {QStringLiteral("port"), proxyPort}});
    root.insert(QStringLiteral("update_interval_hours"), updateIntervalHours);
    root.insert(QStringLiteral("language"), language);

    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray data=QJsonDocument(root).toJson(QJsonDocument::Indented);
    return file.write(data)==data.size() && file.commit();
}

QString AppConfig::defaultDownloadDirectory()
{
    return QDir(systemDownloads()).absoluteFilePath(QStringLiteral("library"));
}

AppConfig::DownloadDirectoryState AppConfig::prepareDownloadDirectory(QString *error,const QString &downloadsRoot)
{
    if(error)error->clear();
    if(hasSavedDownloadDirectory) {
        if(QFileInfo(engine.savePath).isDir())return DownloadDirectoryState::Ready;
        if(error)*error=QStringLiteral("保存的下载目录不存在，或该位置已不再是文件夹。请更换目标下载目录。");
        return DownloadDirectoryState::MissingSaved;
    }
    const QString root=downloadsRoot.isEmpty()?systemDownloads():downloadsRoot;
    const QString target=QDir(root).absoluteFilePath(QStringLiteral("library"));
    if(!QDir().mkpath(target) || !QFileInfo(target).isDir()) {
        if(error)*error=QStringLiteral("无法在系统下载文件夹中创建 library，请选择其他下载目录。");
        return DownloadDirectoryState::Error;
    }
    engine.savePath=target;paths.downloadDir=target;hasSavedDownloadDirectory=true;
    return DownloadDirectoryState::CreatedDefault;
}

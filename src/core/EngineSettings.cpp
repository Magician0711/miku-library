// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "EngineSettings.h"
#include <QJsonArray>

EngineSettings EngineSettings::fromJson(const QJsonObject &object)
{
    EngineSettings settings;
    settings.savePath = object.value(QStringLiteral("save_path")).toString();
    settings.bindAddress = object.value(QStringLiteral("bind_address")).toString();
    settings.listenPort = object.value(QStringLiteral("listen_port")).toInt(settings.listenPort);
    settings.dht = object.value(QStringLiteral("dht")).toBool(settings.dht);
    settings.lsd = object.value(QStringLiteral("lsd")).toBool(settings.lsd);
    settings.upnp = object.value(QStringLiteral("upnp")).toBool(settings.upnp);
    settings.publicTrackerFallback=object.value("public_tracker_fallback").toBool(true);
    if(object.value("fallback_trackers").isArray()){settings.fallbackTrackers.clear();for(const auto &v:object.value("fallback_trackers").toArray())if(v.isString())settings.fallbackTrackers.append(v.toString());}
    settings.encryption = object.value(QStringLiteral("encryption")).toInt(settings.encryption);
    settings.downloadLimit = object.value(QStringLiteral("download_limit")).toInt(settings.downloadLimit);
    settings.uploadLimit = object.value(QStringLiteral("upload_limit")).toInt(settings.uploadLimit);
    settings.maxConnections = object.value(QStringLiteral("max_connections")).toInt(settings.maxConnections);
    settings.maxUploads = object.value(QStringLiteral("max_uploads")).toInt(settings.maxUploads);
    settings.activeDownloads = object.value(QStringLiteral("active_downloads")).toInt(settings.activeDownloads);
    settings.activeSeeds = object.value(QStringLiteral("active_seeds")).toInt(settings.activeSeeds);
    settings.activeLimit = object.value(QStringLiteral("active_limit")).toInt(settings.activeLimit);
    settings.queueing = object.value(QStringLiteral("queueing")).toBool(settings.queueing);
    settings.stopOnCompletion = object.value(QStringLiteral("stop_on_completion")).toBool(settings.stopOnCompletion);
    settings.sourceStallSeconds=qBound(0,object.value("source_stall_seconds").toInt(90),3600);
    return settings;
}

QJsonObject EngineSettings::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("save_path"), savePath);
    object.insert(QStringLiteral("bind_address"), bindAddress);
    object.insert(QStringLiteral("listen_port"), listenPort);
    object.insert(QStringLiteral("dht"), dht);
    object.insert(QStringLiteral("lsd"), lsd);
    object.insert(QStringLiteral("upnp"), upnp);
    object.insert("public_tracker_fallback",publicTrackerFallback);
    QJsonArray trackers;for(const auto &url:fallbackTrackers)trackers.append(url);object.insert("fallback_trackers",trackers);
    object.insert(QStringLiteral("encryption"), encryption);
    object.insert(QStringLiteral("download_limit"), downloadLimit);
    object.insert(QStringLiteral("upload_limit"), uploadLimit);
    object.insert(QStringLiteral("max_connections"), maxConnections);
    object.insert(QStringLiteral("max_uploads"), maxUploads);
    object.insert(QStringLiteral("active_downloads"), activeDownloads);
    object.insert(QStringLiteral("active_seeds"), activeSeeds);
    object.insert(QStringLiteral("active_limit"), activeLimit);
    object.insert(QStringLiteral("queueing"), queueing);
    object.insert(QStringLiteral("stop_on_completion"), stopOnCompletion);
    object.insert("source_stall_seconds",sourceStallSeconds);
    return object;
}

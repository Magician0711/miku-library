#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct EngineSettings {
    QString savePath;
    QString bindAddress;
    int listenPort = 6881;
    bool dht = true;
    bool lsd = true;
    bool upnp = true;
    bool publicTrackerFallback = true;
    QStringList fallbackTrackers={"https://tracker.pmman.tech:443/announce","https://tracker.nekomi.cn:443/announce",
        "udp://tracker.opentrackr.org:1337/announce","udp://tracker.torrent.eu.org:451/announce"};
    int encryption = 0;          // 0 preferred, 1 forced, 2 disabled
    int downloadLimit = 0;       // KiB/s, 0 = unlimited
    int uploadLimit = 1024;         // KiB/s, 0 = unlimited
    int maxConnections = 0;      // 0 = unlimited (OS resources still apply)
    int maxUploads = 0;          // 0 = unlimited
    int activeDownloads = 0;     // 0 = unlimited
    int activeSeeds = 0;         // 0 = unlimited
    int activeLimit = 0;         // 0 = unlimited
    bool queueing = true;
    bool stopOnCompletion = true;
    int sourceStallSeconds = 90; // Only switch before any payload is received; 0 disables.

    QJsonObject toJson() const;
    static EngineSettings fromJson(const QJsonObject &object);
};

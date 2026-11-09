// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

enum class Confidence { Unknown, Guessed, Metadata, Probed };
enum class AdapterType { None, Rss, JsonApi };
enum class DownloadStatus { Pending, Downloading, Paused, Completed, Error, Seeding };

struct SiteEntry {
    QString id;
    QString name;
    QString url;
    QString category = QStringLiteral("public");
    QString kind = QStringLiteral("public_index");
    QString language = QStringLiteral("multi");
    bool enabled = true;
    int priority = 50;
    AdapterType adapter = AdapterType::None;
    QString searchUrl;
    QString rssUrl;
    double timeout = 20.0;
    bool rssFlag = false;
    bool searchAvailable = false;
    QString lastError;
    QDateTime lastSuccess;
    QJsonObject raw;


    bool isSearchable() const { return enabled && adapter != AdapterType::None; }
    QString adapterName() const;
};

struct SiteCatalog {
    QVector<SiteEntry> torrentSites;
    QVector<SiteEntry> fansubPages;
    QVector<SiteEntry> supplementarySites;
    QVector<SiteEntry> historicalSites;

    QVector<SiteEntry> allSites() const;
    SiteEntry *byId(const QString &id);
    const SiteEntry *byId(const QString &id) const;
};

struct ResourceTags {
    QString resolution;
    QString videoCodec;
    int bitDepth = 0;
    QString hdrType;
    QString hdrFallback;
    QString dvProfile;
    QString audioCodec;
    QString audioChannels;
    QString subtitleLang;
    QString subtitleType;
    QString sourceType;
    QString releaseGroup;

    Confidence resolutionConf = Confidence::Unknown;
    Confidence videoConf = Confidence::Unknown;
    Confidence hdrConf = Confidence::Unknown;
    Confidence audioConf = Confidence::Unknown;
    Confidence subtitleConf = Confidence::Unknown;
    Confidence confidence = Confidence::Unknown;
};

struct ResourceSource {
    QString siteId, siteName, torrentUrl, magnetUri, infoHash, sourcePage;
    int seeders = -1;
    QJsonObject toJson() const;
    static ResourceSource fromJson(const QJsonObject &object);
};

struct ResourceResult {
    QString title;
    QString animeId;
    QString siteId;
    QString siteName;
    QString releaseGroup;
    QString episode;
    QDateTime publishTime;
    qint64 fileSize = 0;
    QString magnetUri;
    QString torrentUrl;
    QString infoHash;
    int seeders = -1;
    int leechers = -1;
    ResourceTags tags;
    QString sourcePage;
    QStringList seenOn;
    QVector<ResourceSource> sources;
    QJsonObject raw;
    // Local media rows never use a remote URI for playback.
    QString localPath;

    QString dedupKey() const;
    QString rowKey() const;
};

void mergeResource(ResourceResult &target, const ResourceResult &incoming);
QVector<ResourceResult> downloadCandidates(const ResourceResult &resource);

struct AnimeSubject {
    int bangumiId = 0;
    QString nameCn;
    QString nameJp;
    QString nameEn;
    QString nameRomaji;
    QStringList aliases;
    QString summary;
    int year = 0;
    QString airDate;
    int episodeCount = 0;
    QStringList tags;
    QString coverUrl;
    QString bangumiUrl;
    double rating = 0.0;
    int rank = 0;
    QString platform;
    int airWeekday = 0;
    QString studio;

    QString displayName() const;
};

struct SiteStatus {
    QString siteId;
    QString siteName;
    bool ok = false;
    int count = 0;
    double elapsed = 0.0;
    QString error;
};


struct DownloadTask {
    QString title;
    QString savePath;
    QString statusMessage;
    DownloadStatus status = DownloadStatus::Pending;
    double progress = 0.0;
    qint64 dlSpeed = 0;
    qint64 upSpeed = 0;
    qint64 downloaded = 0;
    qint64 receivedPayload = 0;
    qint64 size = 0;
    int seeders = 0;
    int leechers = 0;
    qint64 eta = -1;
    QString infoHash;
    QString qbtHash;
    QString animeId;
    QString siteId;
};

struct DownloadFile {
    QString path;
    qint64 size = 0;
    qint64 downloaded = 0;
    bool wanted = true;
};

QString confidenceName(Confidence c);
QString downloadStatusName(DownloadStatus s);

std::optional<QString> normalizeBtih(const QString &value);
std::optional<QString> btihFromMagnet(const QString &magnet);

Q_DECLARE_METATYPE(ResourceResult)
Q_DECLARE_METATYPE(AnimeSubject)
Q_DECLARE_METATYPE(SiteStatus)
Q_DECLARE_METATYPE(QVector<ResourceResult>)

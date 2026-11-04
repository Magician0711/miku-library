#pragma once

#include "Models.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

QStringList subjectKeys(const AnimeSubject &subject);
QString releaseGroupKey(const QString &name);
bool isRelevant(const QString &title, const QStringList &keys);

struct Release {
    ResourceResult result;
    QString episode;
    bool isBatch = false;
    std::optional<int> season;

    double sortEpisode() const;
};

struct ReleaseGroup {
    QString name;
    QVector<Release> releases;

    QDateTime latest() const;
    int totalSeeders() const;
    QStringList resolutions() const;
    QSet<QString> subtitleLangs() const;
};

// Inclusive episode window chosen for a group download; 0 leaves that end open.
struct EpisodeRange {
    int first = 0;
    int last = 0;
    bool active() const { return first > 0 || last > 0; }
    bool contains(const QString &episode) const;
};

struct Filters {
    std::optional<int> season;
    QString resolution;
    QString subtitle;
    bool hdrOnly = false;
    QString kind = QStringLiteral("all");
    int minSeeders = 0;
    QString text;
};

QVector<Release> annotate(const QVector<ResourceResult> &results);
bool passes(const Release &release, const Filters &filters);
// sortKey: "popular" (seeders and resource count combined), "latest", "seeders",
// "count" or "name". Releases without a recognisable group, or whose bracket
// prefix is just the anime's own name, are merged into kUnknownGroup, always last.
QVector<ReleaseGroup> groupReleases(const QVector<Release> &releases, const QString &sortKey = QStringLiteral("popular"),
                                    const QStringList &subjectKeys = {});
QVector<int> seasonsPresent(const QVector<Release> &releases);

extern const QString kUnknownGroup;

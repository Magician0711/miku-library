// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

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

// Another season, arc or film of the same anime, as Bangumi lists it.
struct SeriesEntry {
    AnimeSubject subject;
    QString label;      // what sets it apart: 游郭篇, 第二季, 剧场版 无限列车篇
    bool main = false;  // carries the bare title with nothing after it
};
// The anime's title without its season or arc, in each language: what to search for.
QStringList seriesNames(const AnimeSubject &subject);
// The entries of `found` sharing current's title, oldest first; always includes current.
QVector<SeriesEntry> seriesEntries(const AnimeSubject &current, const QVector<AnimeSubject> &found);
// Words that place a release title in one of the entries instead of the main one.
QStringList seriesMarks(const QVector<SeriesEntry> &entries);
bool mentionsAny(const QString &title, const QStringList &marks);

extern const QString kUnknownGroup;

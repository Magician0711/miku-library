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
QVector<ReleaseGroup> groupReleases(const QVector<Release> &releases, const QString &sortKey = QStringLiteral("latest"));
QVector<int> seasonsPresent(const QVector<Release> &releases);

extern const QString kUnknownGroup;

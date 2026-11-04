// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "Grouping.h"
#include <QHash>
#include <QObject>

struct LibraryFile {
    QString path;
    qint64 size = 0;
    bool complete = false;
};

struct LibraryItem {
    QString key;
    AnimeSubject subject;
    ResourceResult resource;
    QString groupKey;
    int season = 1;
    QStringList episodes;
    QStringList onlyEpisodes;
    QString hash;
    QString savePath;
    QString phase = QStringLiteral("queued");
    QString downloader=QStringLiteral("builtin");
    QString externalProgram;
    QString externalArguments;
    QString torrentFile;
    QString remoteId;
    QStringList attemptedSources;
    bool complete = false;
    QVector<LibraryFile> files;
};

// What the user last chose for an anime: the release group and the kind of
// release. One-click downloads start from it and the update check follows it.
struct DownloadPreference {
    QString group;            // release group display name; empty = none
    QString resolution;       // e.g. "1080p"; empty = any
    QString language;         // subtitle language tag, "bilingual", or empty
    QString subtitleType;     // "internal", "hardcoded", ...; empty = any
    int ignoredThrough = 0;   // update notices up to this episode were dismissed
    bool follow = true;       // include in the update check
    bool matches(const ResourceResult &resource) const;
};

// A followed release group to check for episodes after the newest one held.
struct UpdateTarget {
    AnimeSubject subject;
    DownloadPreference preference;
    int season = 1;
    int after = 0;            // newest episode already downloaded, queued or ignored
};

struct PlannedDownload {
    ResourceResult resource;
    int season = 1;
    QStringList episodes;
    QStringList onlyEpisodes;
};

class LocalLibrary : public QObject {
    Q_OBJECT
public:
    explicit LocalLibrary(const QString &path,QObject *parent=nullptr);
    bool load(QString *error=nullptr);
    bool save(QString *error=nullptr) const;
    QString path() const { return m_path; }
    QVector<LibraryItem> items() const { return m_items; }
    std::optional<LibraryItem> item(const QString &key) const;
    QVector<AnimeSubject> subjects() const;
    QString itemKey(int subjectId,const ResourceResult &resource) const;
    QStringList playableFiles(const LibraryItem &item) const;
    QStringList playableFiles(int subjectId,const ResourceResult &resource) const;
    bool downloaded(int subjectId,const ResourceResult &resource) const;
    bool busy(int subjectId,const ResourceResult &resource) const;
    bool external(int subjectId,const ResourceResult &resource) const;
    bool groupHasHistory(int subjectId,const QString &group) const;
    QSet<QString> claimedEpisodes(int subjectId,const QString &group,int season) const;
    QVector<Release> localReleases(int subjectId) const;
    // Stored preference, or one derived from the anime's most recent download.
    DownloadPreference preference(int subjectId) const;
    bool setPreference(int subjectId,const DownloadPreference &preference,QString *error=nullptr);
    // Anime whose preferred group may have new episodes: has numbered downloads in
    // that group and is not complete according to its episode count.
    QVector<UpdateTarget> updateTargets() const;
    // Episodes after target.after that the given search results would add.
    QVector<PlannedDownload> newEpisodes(const UpdateTarget &target,const QVector<ResourceResult> &results,
                                         QVector<ResourceResult> *candidates=nullptr) const;
    QVector<PlannedDownload> plan(const AnimeSubject &subject,const QVector<ResourceResult> &resources,
                                  QString *reason=nullptr,bool preferSingles=false,const EpisodeRange &range={}) const;
    bool put(const LibraryItem &item,QString *error=nullptr);
    void reconcile(const QString &hash,const QVector<DownloadFile> &files,bool completed,const QString &phase);
    void markRemoved(const QString &hash);
    static QString safeFolderName(const QString &name);
    QString downloadDirectory(const QString &root,const AnimeSubject &subject,const QString &group,int season) const;
    static int subjectSeason(const AnimeSubject &subject);

signals:
    void changed();
    void storageError(const QString &message);

private:
    QSet<QString> availableEpisodes(const LibraryItem &item) const;
    void rebuildIndex();
    static QString identity(int subjectId,const ResourceResult &resource);
    QString m_path;
    QVector<LibraryItem> m_items;
    // Lookups run for every resource row; keep them O(1). Rebuilt whenever
    // records are added, replaced or reloaded (status updates keep keys stable).
    QHash<int,DownloadPreference> m_preferences;
    QHash<QString,qsizetype> m_byKey;
    QHash<QString,QString> m_byIdentity;
    bool m_ready = false;
};

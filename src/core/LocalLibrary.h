#pragma once

#include "Grouping.h"
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
    QVector<PlannedDownload> plan(const AnimeSubject &subject,const QVector<ResourceResult> &resources,
                                  QString *reason=nullptr,bool preferSingles=false) const;
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
    QString m_path;
    QVector<LibraryItem> m_items;
    bool m_ready = false;
};

#pragma once

#include "core/LocalLibrary.h"
#include <QObject>
#include <QQueue>
#include <QElapsedTimer>

class TorrentEngine;
class ExternalDownloader;
class DownloaderRpc;
struct AppConfig;
class QTimer;

class AnimeDownloads : public QObject {
    Q_OBJECT
public:
    AnimeDownloads(TorrentEngine *engine,LocalLibrary *library,AppConfig *config,QObject *parent=nullptr);
    void downloadOne(const AnimeSubject &subject,const ResourceResult &resource);
    void downloadGroup(const AnimeSubject &subject,const QVector<ResourceResult> &resources,const EpisodeRange &range={});
    void adoptExisting(const AnimeSubject &subject,const QVector<ResourceResult> &resources);
    void synchronize();
    void adoptCachedSubjects(const QVector<AnimeSubject> &subjects);
    DownloaderRpc *rpc() const {return m_rpc;}

signals:
    void queued();
    void information(const QString &message);
    void error(const QString &message);

private:
    bool reserve(const AnimeSubject &subject,const PlannedDownload &plan);
    void processNext();
    void startCandidate(const QString &key,const QVector<ResourceResult> &candidates,int index=0);
    void restorePending();
    void scheduleSynchronize();
    void recoverStalled(const QVector<DownloadTask> &tasks);
    void waitForRemoval(const QString &key,const QString &hash,QVector<ResourceResult> candidates,int attempt=0);
    QHash<QString,QElapsedTimer> m_waiting;
    QSet<QString> m_payloadReceived;
    ExternalDownloader *m_external;
    DownloaderRpc *m_rpc;
    TorrentEngine *m_engine;
    LocalLibrary *m_library;
    AppConfig *m_config;
    QTimer *m_timer;
    QQueue<QString> m_pending;
    QTimer *m_syncTimer;
    bool m_dispatching=false;
    QSet<QString> m_activeKeys;
    QStringList m_errors;
};

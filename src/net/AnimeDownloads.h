// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

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

    // Changing the download folder: what lives under the old one, and moving it.
    struct MoveSummary { int anime=0; int items=0; int external=0; };
    MoveSummary planMove(const QString &oldRoot) const;
    // Built-in tasks move through the engine, finished files on a worker thread;
    // external downloaders' tasks stay where they are. Reports via moveFinished.
    void moveDownloads(const QString &oldRoot,const QString &newRoot);
    bool moving() const {return !m_pendingMoves.isEmpty()||m_fileMoves>0;}

signals:
    void queued();
    void information(const QString &message);
    void error(const QString &message);
    void moveFinished(int moved,const QStringList &errors);

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
    void finishMoveStep();
    QHash<QString,QPair<QString,QString>> m_pendingMoves;   // hash -> old, new save path
    int m_fileMoves=0;
    int m_moved=0;
    QStringList m_moveErrors;
    QString m_moveRoot;
};

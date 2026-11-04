#pragma once

#include "core/EngineSettings.h"
#include "core/Models.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>
#include <string>

#include <libtorrent/fwd.hpp>

class QNetworkAccessManager;
class QTimer;

class TorrentEngine : public QObject {
    Q_OBJECT
public:
    explicit TorrentEngine(const EngineSettings &settings, QObject *parent = nullptr,
                           const QString &stateDir = {});
    ~TorrentEngine() override;

    bool start(QString *error = nullptr);
    void stop();
    bool running() const;
    QString version() const;

    const EngineSettings &settings() const { return m_settings; }
    QJsonObject networkStatus() const;
    void applySettings(const EngineSettings &settings);

    QVector<DownloadTask> listTorrents() const;
    QVector<DownloadFile> filesFor(const QString &infoHash) const;

    void addTorrent(const QString &uri, const QString &savePath,
                    std::function<void(bool, QString)> callback = {});
    using AddCallback = std::function<void(bool, QString, QString)>;
    void addTorrentTracked(const QString &uri,const QString &savePath,AddCallback callback,
                           const QStringList &onlyEpisodes = {},const QStringList &trackers = {},const QString &expectedHash = {});
    static QString saveTorrentFile(const QByteArray &data,const QString &path,QString *error=nullptr);
    bool selectEpisodes(const QString &infoHash,const QStringList &episodes,QString *error=nullptr);
    bool pause(const QString &infoHash);
    bool resume(const QString &infoHash);
    bool remove(const QString &infoHash, bool deleteFiles = false);

signals:
    void changed();
    void engineError(const QString &message);
    void torrentRemoved(const QString &infoHash);

private:
    struct Entry {
        QString uri;
        QString savePath;
        QString infoHash;
        bool paused = false;
        QStringList onlyEpisodes;
        bool autoStopped = false;
        QStringList trackers;
    };

    bool addMagnet(const QString &magnet, const QString &savePath, bool paused, QString *error,
                   bool persist = true,const QStringList &onlyEpisodes = {},QString *addedHash = nullptr);
    bool addTorrentFile(const QString &path, const QString &savePath, bool paused, QString *error,
                        bool persist = true,const QStringList &onlyEpisodes = {},QString *addedHash = nullptr,
                        const QStringList &trackers = {},const QString &expectedHash = {});
    void fetchTorrent(const QString &url, const QString &savePath,
                      AddCallback callback, bool paused = false,const QStringList &onlyEpisodes = {},
                      const QStringList &trackers = {},const QString &expectedHash = {});
    bool saveMagnetTorrent(const lt::torrent_handle &handle, QString *error);
    void appendEntry(const Entry &entry);
    void removeEntry(const QString &infoHash);
    void updateEntryPaused(const QString &infoHash, bool paused);
    void saveState() const;
    void loadState();
    void pollAlerts();
    QString hashString(const lt::torrent_handle &handle) const;
    void stopCompleted(const lt::torrent_handle &handle);

    EngineSettings m_settings;
    std::unique_ptr<lt::session> m_session;
    QNetworkAccessManager *m_net = nullptr;
    QTimer *m_alertTimer = nullptr;
    QString m_stateDir;
    QVector<Entry> m_entries;
    // File list of one torrent; stopped marks a task the engine paused on completion.
    QVector<DownloadFile> filesOf(const lt::torrent_handle &handle, const QString &savePath, bool stopped) const;
    QSet<QString> m_metadataPending;
    QHash<QString,Entry> m_readds;
    QHash<QString, QString> m_saveErrors;
    QHash<QString,QString> m_trackerWarnings;
    QString m_networkWarning;
    bool m_started = false;
};

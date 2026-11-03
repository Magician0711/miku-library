#include "TorrentEngine.h"

#include "core/AppConfig.h"
#include "core/TitleParser.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/info_hash.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_status.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/version.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/download_priority.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>

namespace {

QString hashToQString(const lt::info_hash_t &hashes)
{
    return hashes.has_v1()
        ? QString::fromLatin1(QByteArray(hashes.v1.data(), 20).toHex())
        : QString::fromLatin1(QByteArray(hashes.v2.data(), 32).toHex());
}

// Validate before writing so HTML/error responses can never become .torrent files.
QString saveTorrentData(const QByteArray &data, const QString &savePath, QString *error)
{
    lt::add_torrent_params params;
    try {
        params = lt::load_torrent_buffer(lt::span<char const>(data.constData(), data.size()));
    } catch (const std::exception &ex) {
        if (error) *error = QStringLiteral("无效的种子文件：%1").arg(QString::fromUtf8(ex.what()));
        return {};
    }
    QString name = QString::fromStdString(params.ti->name());
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x{0000}-\\x{001f}]")), QStringLiteral("_"));
    name = name.trimmed().left(64);
    if (!name.isEmpty() && name.back().isHighSurrogate()) name.chop(1);
    // Prefix also avoids Windows reserved device names; hash separates equal titles.
    const QString stem = QStringLiteral("torrent-%1-%2").arg(name, hashToQString(params.ti->info_hashes()));
    QDir dir(savePath);
    if (!dir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("无法创建下载目录：%1").arg(savePath);
        return {};
    }
    QString path;
    for (int suffix = 0; ; ++suffix) {
        path = dir.absoluteFilePath(stem + (suffix ? QStringLiteral("-%1").arg(suffix) : QString())
                                   + QStringLiteral(".torrent"));
        if (!QFile::exists(path)) break;
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == data) return path;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = QStringLiteral("无法保存种子文件：%1\n%2").arg(path, file.errorString());
        return {};
    }
    return path;
}

void setStartFlags(lt::add_torrent_params &params, bool paused, bool queueing)
{
    params.flags &= ~(lt::torrent_flags::paused | lt::torrent_flags::auto_managed);
    if (paused) params.flags |= lt::torrent_flags::paused;
    else if (queueing) params.flags |= lt::torrent_flags::auto_managed;
    params.flags |= lt::torrent_flags::duplicate_is_error;
}

std::vector<lt::download_priority_t> episodePriorities(const lt::torrent_info &info,const QStringList &wanted,QString *error)
{
    const auto &files=info.layout();
    std::vector<lt::download_priority_t> priorities(files.num_files(),lt::dont_download);
    const QSet<QString> selected(wanted.begin(),wanted.end());
    int videos=0;
    QSet<QString> assigned;
    for(lt::file_index_t n{0};n<files.end_file();++n) {
        const QString name=QString::fromStdString(files.file_path(n));
        static const QRegularExpression extra(QStringLiteral("(?:^|[\\s\\[\\]_.-])(?:NCOP|NCED|OP|ED|PV|CM|SP|OVA|OAD)(?:[\\s\\[\\]_.\\d-]|$)"),QRegularExpression::CaseInsensitiveOption);
        if(extra.match(QFileInfo(name).fileName()).hasMatch())continue;
        const auto episodes=fileEpisodeNumbers(name);
        bool matches=false;for(const auto &ep:episodes)if(selected.contains(ep)){matches=true;break;}
        if(matches) {
            if(isVideoFile(name)) {
                bool overlap=false;
                for(const auto &ep:episodes)if(!selected.contains(ep)||assigned.contains(ep))overlap=true;
                if(overlap)continue;
                for(const auto &ep:episodes)assigned.insert(ep);
                ++videos;
            }
            priorities[int(n)]=lt::default_priority;
        }
    }
    if(!videos && error)*error=QStringLiteral("合集中没有可确认的缺集视频，已阻止可能的重复下载。");
    return videos?priorities:std::vector<lt::download_priority_t>{};
}

} // namespace

TorrentEngine::TorrentEngine(const EngineSettings &settings, QObject *parent, const QString &stateDir)
    : QObject(parent)
    , m_settings(settings)
    , m_net(new QNetworkAccessManager(this))
    , m_stateDir(stateDir.isEmpty() ? AppConfig::dataDir() + QStringLiteral("/engine") : stateDir)
{
    QDir().mkpath(m_stateDir);
}

TorrentEngine::~TorrentEngine()
{
    stop();
}

bool TorrentEngine::start(QString *error)
{
    if (m_started)
        return true;
    try {
        lt::settings_pack pack;
        pack.set_str(lt::settings_pack::user_agent, "AnimeNexus/0.4 libtorrent");
        pack.set_int(lt::settings_pack::alert_mask,
                     static_cast<int>(static_cast<std::uint32_t>(
                         lt::alert_category::error | lt::alert_category::status | lt::alert_category::storage | lt::alert_category::tracker)));
        m_session = std::make_unique<lt::session>(pack);
        m_session->set_alert_notify([this] {
            QMetaObject::invokeMethod(this,&TorrentEngine::pollAlerts,Qt::QueuedConnection);
        });
    } catch (const std::exception &ex) {
        if (error)
            *error = QString::fromUtf8(ex.what());
        return false;
    }
    applySettings(m_settings);
    if (!m_alertTimer) {
        m_alertTimer = new QTimer(this);
        m_alertTimer->setInterval(1000);
        connect(m_alertTimer, &QTimer::timeout, this, &TorrentEngine::pollAlerts);
    }
    m_alertTimer->start();
    m_started = true;
    m_entries.clear();
    loadState();
    const auto entries = m_entries;
    for (const auto &entry : entries) {
        QString ignored;
        if (entry.uri.startsWith(QStringLiteral("magnet:")))
            addMagnet(entry.uri, entry.savePath, entry.paused, &ignored, false,entry.onlyEpisodes);
        else if (entry.uri.startsWith(QStringLiteral("http")))
            fetchTorrent(entry.uri, entry.savePath, {}, entry.paused,entry.onlyEpisodes,entry.trackers,entry.infoHash);
        else
            addTorrentFile(entry.uri, entry.savePath, entry.paused, &ignored, false,entry.onlyEpisodes,nullptr,entry.trackers,entry.infoHash);
    }
    emit changed();
    return true;
}

void TorrentEngine::stop()
{
    if (!m_started && !m_session)
        return;
    if (m_alertTimer)
        m_alertTimer->stop();
    saveState();
    m_started = false;
    for (auto *reply : m_net->findChildren<QNetworkReply *>()) reply->abort();
    m_session.reset();
    m_metadataPending.clear();m_readds.clear();
    m_saveErrors.clear();m_trackerWarnings.clear();m_networkWarning.clear();
    emit changed();
}

bool TorrentEngine::running() const
{
    return m_started && m_session != nullptr;
}

QString TorrentEngine::version() const
{
    return QStringLiteral("libtorrent ") + QString::fromUtf8(lt::version());
}

void TorrentEngine::applySettings(const EngineSettings &settings)
{
    m_settings = settings;
    if (!m_session)
        return;
    lt::settings_pack pack;
    QString bindAddress = m_settings.bindAddress;
    if (bindAddress.isEmpty())
        bindAddress = qEnvironmentVariable("ANIME_NEXUS_BIND_IP");
    const QString listen = bindAddress.isEmpty()
        ? QStringLiteral("0.0.0.0:%1,[::]:%1").arg(settings.listenPort)
        : QStringLiteral("%1:%2").arg(bindAddress.contains(':')?"["+bindAddress+"]":bindAddress).arg(settings.listenPort);
    pack.set_str(lt::settings_pack::listen_interfaces, listen.toStdString());
    // Empty means the OS routes each peer; also clears a previous manual bind.
    pack.set_str(lt::settings_pack::outgoing_interfaces, bindAddress.toStdString());
    pack.set_bool(lt::settings_pack::enable_dht, settings.dht);
    pack.set_str(lt::settings_pack::dht_bootstrap_nodes,
                 "dht.libtorrent.org:25401,dht.transmissionbt.com:6881,router.bittorrent.com:6881");
    pack.set_bool(lt::settings_pack::enable_lsd, settings.lsd);
    pack.set_bool(lt::settings_pack::enable_upnp, settings.upnp);
    pack.set_bool(lt::settings_pack::enable_natpmp, settings.upnp);
    // A dead first tier must not postpone every usable tracker behind it.
    pack.set_bool(lt::settings_pack::announce_to_all_tiers,true);
    pack.set_int(lt::settings_pack::download_rate_limit, settings.downloadLimit * 1024);
    pack.set_int(lt::settings_pack::upload_rate_limit, settings.uploadLimit * 1024);
    pack.set_int(lt::settings_pack::connections_limit, qMax(0,settings.maxConnections));
    pack.set_int(lt::settings_pack::unchoke_slots_limit, settings.maxUploads>0?settings.maxUploads:-1);
    pack.set_int(lt::settings_pack::active_downloads, settings.queueing&&settings.activeDownloads>0?settings.activeDownloads:-1);
    pack.set_int(lt::settings_pack::active_seeds, settings.queueing&&settings.activeSeeds>0?settings.activeSeeds:-1);
    pack.set_int(lt::settings_pack::active_limit, settings.queueing&&settings.activeLimit>0?settings.activeLimit:-1);
    pack.set_int(lt::settings_pack::active_checking,-1);
    pack.set_int(lt::settings_pack::active_dht_limit,-1);
    pack.set_int(lt::settings_pack::active_tracker_limit,-1);
    pack.set_int(lt::settings_pack::active_lsd_limit,-1);
    pack.set_bool(lt::settings_pack::allow_multiple_connections_per_ip, false);
    const int encPolicy = settings.encryption == 1
        ? lt::settings_pack::pe_forced
        : (settings.encryption == 2 ? lt::settings_pack::pe_disabled : lt::settings_pack::pe_enabled);
    pack.set_int(lt::settings_pack::out_enc_policy, encPolicy);
    pack.set_int(lt::settings_pack::in_enc_policy, encPolicy);
    m_session->apply_settings(pack);
    if(settings.stopOnCompletion)for(const auto &handle:m_session->get_torrents())
        if(handle.status().is_finished)stopCompleted(handle);
    emit changed();
}

QJsonObject TorrentEngine::networkStatus() const
{
    if(!m_session)return {{"running",false}};
    const auto settings=m_session->get_settings();
    return {{"running",running()},{"listen_interfaces",QString::fromStdString(settings.get_str(lt::settings_pack::listen_interfaces))},
        {"outgoing_interfaces",QString::fromStdString(settings.get_str(lt::settings_pack::outgoing_interfaces))},
        {"warning",m_networkWarning}};
}

QVector<DownloadTask> TorrentEngine::listTorrents() const
{
    QVector<DownloadTask> tasks;
    if (!m_session)
        return tasks;
    const auto handles = m_session->get_torrents();
    tasks.reserve(static_cast<int>(handles.size()));
    for (const auto &handle : handles) {
        const auto status = handle.status();
        DownloadTask task;
        task.title = QString::fromStdString(status.name);
        task.savePath = QString::fromStdString(status.save_path);
        task.progress = status.progress * 100.0;
        task.dlSpeed = status.download_payload_rate;
        task.upSpeed = status.upload_payload_rate;
        task.downloaded = status.total_wanted_done;
        task.receivedPayload = status.all_time_download;
        task.size = status.total_wanted;
        task.seeders = status.num_seeds;
        task.leechers = qMax(0, status.num_peers - status.num_seeds);
        if (status.download_payload_rate > 0) {
            const qint64 remaining = status.total_wanted - status.total_wanted_done;
            task.eta = remaining > 0 ? remaining / status.download_payload_rate : -1;
        } else {
            task.eta = -1;
        }
        task.infoHash = hashString(handle);
        task.qbtHash = task.infoHash;

        const bool paused = static_cast<bool>(handle.flags() & lt::torrent_flags::paused);
        bool cachedComplete=false;
        for(const auto &entry:m_entries)if(entry.infoHash==task.infoHash&&entry.autoStopped&&paused) {
            cachedComplete=true;bool hasFile=false;
            for(const auto &f:filesFor(task.infoHash))if(f.wanted) {
                hasFile=true;if(f.size>0&&f.downloaded<f.size)cachedComplete=false;
            }
            cachedComplete=cachedComplete&&hasFile;
        }
        if(cachedComplete){task.progress=100;task.downloaded=task.size;}
        if (m_saveErrors.contains(task.infoHash) || status.errc) {
            task.status = DownloadStatus::Error;
            task.statusMessage = m_saveErrors.value(task.infoHash,
                QString::fromUtf8(status.errc.message().c_str()));
        } else if ((status.is_finished||cachedComplete) && m_settings.stopOnCompletion) {
            task.status = DownloadStatus::Completed;
            task.statusMessage=QStringLiteral("已完成 · 传输已停止");
        } else if (paused) {
            task.status = DownloadStatus::Paused;
        } else if (m_metadataPending.contains(task.infoHash)) {
            task.status = DownloadStatus::Downloading;
            task.statusMessage = QStringLiteral("获取种子中");
        } else if (status.state == lt::torrent_status::seeding
                   || status.state == lt::torrent_status::finished) {
            task.status = DownloadStatus::Seeding;
        } else if (status.state == lt::torrent_status::downloading
                   || status.state == lt::torrent_status::downloading_metadata) {
            task.status = DownloadStatus::Downloading;
            if(status.num_peers==0)task.statusMessage=m_trackerWarnings.contains(task.infoHash)
                ?QStringLiteral("等待节点 · ")+m_trackerWarnings.value(task.infoHash):QStringLiteral("正在发现下载节点");
            else if(status.download_payload_rate==0)task.statusMessage=QStringLiteral("已连接 %1 个节点 · 等待可用分片").arg(status.num_peers);
        } else {
            task.status = DownloadStatus::Pending;
        }
        tasks.push_back(std::move(task));
    }
    return tasks;
}

QVector<DownloadFile> TorrentEngine::filesFor(const QString &infoHash) const
{
    QVector<DownloadFile> files;
    if(!m_session)return files;
    for(const auto &handle:m_session->get_torrents()) {
        if(hashString(handle).compare(infoHash,Qt::CaseInsensitive)!=0)continue;
        const auto info=handle.torrent_file();if(!info)return files;
        std::vector<std::int64_t> progress;handle.file_progress(progress);
        const auto priorities=handle.get_file_priorities();
        const auto &storage=info->layout();
        bool stopped=false;
        for(const auto &entry:m_entries)if(entry.infoHash.compare(infoHash,Qt::CaseInsensitive)==0&&entry.autoStopped
            &&bool(handle.flags()&lt::torrent_flags::paused))stopped=true;
        const QString root=QDir(QString::fromStdString(handle.status().save_path)).absolutePath();
        for(lt::file_index_t n{0};n<storage.end_file();++n) {
            const QString path=QDir::cleanPath(QDir(root).absoluteFilePath(QString::fromStdString(storage.file_path(n))));
            if(!path.startsWith(root+"/",Qt::CaseInsensitive))continue;
            qint64 downloaded=int(n)<progress.size()?progress[int(n)]:0;
            const QFileInfo disk(path);
            if(stopped&&disk.isFile()&&disk.size()==storage.file_size(n))downloaded=storage.file_size(n);
            files.append({path,storage.file_size(n),downloaded,
                int(n)>=priorities.size()||priorities[int(n)]!=lt::dont_download});
        }
        break;
    }
    return files;
}

QString TorrentEngine::saveTorrentFile(const QByteArray &data,const QString &path,QString *error)
{return saveTorrentData(data,path,error);}

void TorrentEngine::addTorrent(const QString &uri, const QString &savePath,
                               std::function<void(bool, QString)> callback)
{
    addTorrentTracked(uri,savePath,[callback=std::move(callback)](bool ok,QString message,QString) {
        if(callback)callback(ok,message);
    });
}

void TorrentEngine::addTorrentTracked(const QString &uri,const QString &savePath,AddCallback callback,const QStringList &onlyEpisodes,const QStringList &trackers,const QString &expectedHash)
{
    if (!running()) {
        if (callback)
            callback(false, QStringLiteral("下载引擎未启动"),{});
        return;
    }
    if (!QDir().mkpath(savePath)) {
        if (callback) callback(false, QStringLiteral("无法创建下载目录"),{});
        return;
    }
    QTemporaryFile probe(QDir(savePath).filePath(QStringLiteral(".nexus-write-XXXXXX")));
    if (!probe.open()) {
        if (callback) callback(false, QStringLiteral("下载目录不可写：%1").arg(probe.errorString()),{});
        return;
    }
    probe.close();
    if (uri.startsWith(QStringLiteral("magnet:"))) {
        QString error;
        QString hash;
        const bool ok = addMagnet(uri, savePath, false, &error,true,onlyEpisodes,&hash);
        if (callback)
            callback(ok, error,hash);
        return;
    }
    if (uri.startsWith(QStringLiteral("http"))) {
        fetchTorrent(uri, savePath, std::move(callback),false,onlyEpisodes,trackers,expectedHash);
        return;
    }
    QString error;
    const QUrl sourceUrl(uri);QFile source(sourceUrl.isLocalFile()?sourceUrl.toLocalFile():uri);
    QString path;
    if (!source.open(QIODevice::ReadOnly)) error = source.errorString();
    else path = saveTorrentData(source.readAll(), savePath, &error);
    QString hash;
    const bool ok = !path.isEmpty() && addTorrentFile(path, savePath, false, &error,true,onlyEpisodes,&hash,trackers,expectedHash);
    if (callback)
        callback(ok, error,hash);
}

bool TorrentEngine::addMagnet(const QString &magnet, const QString &savePath, bool paused,
                              QString *error, bool persist,const QStringList &onlyEpisodes,QString *addedHash)
{
    lt::error_code ec;
    auto params = lt::parse_magnet_uri(magnet.toStdString(), ec);
    if (ec) {
        if (error)
            *error = QString::fromUtf8(ec.message().c_str());
        return false;
    }
    params.save_path = savePath.toStdString();
    setStartFlags(params, paused, false);
    // Fetch metadata without issuing payload requests. Only start payload after
    // the .torrent has been safely committed to the selected directory.
    params.flags |= lt::torrent_flags::upload_mode;
    const auto handle = m_session->add_torrent(params, ec);
    if (ec || !handle.is_valid()) {
        if (error)
            *error = QString::fromUtf8(ec.message().c_str());
        return false;
    }
    if (persist) {
        Entry entry;
        entry.uri = magnet;
        entry.savePath = savePath;
        entry.infoHash = hashString(handle);
        entry.paused = paused;
        entry.onlyEpisodes=onlyEpisodes;
        appendEntry(entry);
    }
    if(addedHash)*addedHash=hashString(handle);
    m_metadataPending.insert(hashString(handle));
    if (handle.torrent_file() && !saveMagnetTorrent(handle, error)) {
        handle.pause();
        m_saveErrors.insert(hashString(handle), error ? *error : QStringLiteral("种子保存失败"));
        emit changed();
        return false;
    }
    emit changed();
    return true;
}

bool TorrentEngine::addTorrentFile(const QString &path, const QString &savePath, bool paused,
                                   QString *error, bool persist,const QStringList &onlyEpisodes,QString *addedHash,const QStringList &trackers,const QString &expectedHash)
{
    lt::error_code ec;
    lt::add_torrent_params params;
    try {
        params = lt::load_torrent_file(path.toStdString());
    } catch (const std::exception &ex) {
        if (error)
            *error = QString::fromUtf8(ex.what());
        return false;
    }
    params.save_path = savePath.toStdString();
    const QString actualHash=hashToQString(params.ti->info_hashes());
    const auto hashes=params.ti->info_hashes();
    const QString v2=hashes.has_v2()?QString::fromLatin1(QByteArray(hashes.v2.data(),32).toHex()):QString();
    if(!expectedHash.isEmpty()&&actualHash.compare(expectedHash,Qt::CaseInsensitive)!=0&&v2.compare(expectedHash,Qt::CaseInsensitive)!=0){if(error)*error=QStringLiteral("种子内容与资源哈希不一致，已阻止错误下载。");return false;}
    QStringList extraTrackers;
    params.tracker_tiers.resize(params.trackers.size(),0);
    auto candidates=trackers;
    if(!params.ti->priv()&&params.trackers.empty()&&candidates.isEmpty()&&m_settings.publicTrackerFallback)candidates=m_settings.fallbackTrackers;
    if(!params.ti->priv())for(const auto &tracker:candidates){
        const QUrl url(tracker);if(!url.isValid()||url.host().isEmpty()||!QStringList{"http","https","udp"}.contains(url.scheme())||tracker.size()>2048)continue;
        if(extraTrackers.size()>=64)break;
        if(!extraTrackers.contains(tracker)){extraTrackers.append(tracker);if(std::find(params.trackers.begin(),params.trackers.end(),tracker.toStdString())==params.trackers.end()){
            const int tier=params.tracker_tiers.empty()?0:*std::max_element(params.tracker_tiers.begin(),params.tracker_tiers.end())+1;
            params.trackers.push_back(tracker.toStdString());params.tracker_tiers.push_back(tier);
        }}
    }
    if(!onlyEpisodes.isEmpty()) {
        params.file_priorities=episodePriorities(*params.ti,onlyEpisodes,error);
        if(params.file_priorities.empty())return false;
    }
    setStartFlags(params, paused, m_settings.queueing);
    const auto handle = m_session->add_torrent(params, ec);
    if (ec || !handle.is_valid()) {
        if (error)
            *error = QString::fromUtf8(ec.message().c_str());
        return false;
    }
    if (persist) {
        Entry entry;
        entry.uri = path;
        entry.savePath = savePath;
        entry.infoHash = hashString(handle);
        entry.paused = paused;
        entry.onlyEpisodes=onlyEpisodes;
        entry.trackers=extraTrackers;
        appendEntry(entry);
    }
    if(addedHash)*addedHash=hashString(handle);
    emit changed();
    return true;
}

void TorrentEngine::fetchTorrent(const QString &url, const QString &savePath,
                                 AddCallback callback, bool paused,const QStringList &onlyEpisodes,const QStringList &trackers,const QString &expectedHash)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AnimeNexus/0.4"));
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, savePath, paused, onlyEpisodes,trackers,expectedHash,callback = std::move(callback)]() mutable {
        const auto data = reply->readAll();
        const QString error = reply->error() == QNetworkReply::NoError
            ? QString() : reply->errorString();
        reply->deleteLater();
        if (!running()) {
            if (callback) callback(false, QStringLiteral("下载引擎已停止"),{});
            return;
        }
        if (!error.isEmpty() || data.isEmpty()) {
            if (callback)
                callback(false, error.isEmpty() ? QStringLiteral("种子下载失败") : error,{});
            return;
        }
        QString addError;
        const QString path = saveTorrentData(data, savePath, &addError);
        QString hash;
        const bool ok = !path.isEmpty() && addTorrentFile(path, savePath, paused, &addError,true,onlyEpisodes,&hash,trackers,expectedHash);
        if (callback)
            callback(ok, addError,hash);
    });
}

bool TorrentEngine::saveMagnetTorrent(const lt::torrent_handle &handle, QString *error)
{
    const QString hash = hashString(handle);
    try {
        const auto params = handle.get_resume_data(lt::torrent_handle::save_info_dict);
        const auto buffer = lt::write_torrent_file_buf(params, lt::write_flags::allow_missing_piece_layer);
        const QString savePath = QString::fromStdString(params.save_path);
        const QString path = saveTorrentData(QByteArray(buffer.data(), buffer.size()), savePath, error);
        if (path.isEmpty()) return false;
        bool paused = false;
        for (auto &entry : m_entries) {
            if (entry.infoHash == hash) {
                if(!entry.onlyEpisodes.isEmpty()) {
                    const auto priorities=episodePriorities(*params.ti,entry.onlyEpisodes,error);
                    if(priorities.empty())return false;
                    handle.prioritize_files(priorities);
                }
                entry.uri = path;
                paused = entry.paused;
                break;
            }
        }
        saveState();
        m_metadataPending.remove(hash);
        m_saveErrors.remove(hash);
        handle.unset_flags(lt::torrent_flags::upload_mode);
        if (!paused) {
            if (m_settings.queueing) handle.set_flags(lt::torrent_flags::auto_managed);
            handle.resume();
        }
        return true;
    } catch (const std::exception &ex) {
        if (error) *error = QStringLiteral("保存磁力种子失败：%1").arg(QString::fromUtf8(ex.what()));
        return false;
    }
}

bool TorrentEngine::pause(const QString &infoHash)
{
    if (!m_session)
        return false;
    for (const auto &handle : m_session->get_torrents()) {
        if (hashString(handle).compare(infoHash, Qt::CaseInsensitive) == 0) {
            handle.unset_flags(lt::torrent_flags::auto_managed);
            handle.pause();
            updateEntryPaused(infoHash, true);
            emit changed();
            return true;
        }
    }
    return false;
}

bool TorrentEngine::selectEpisodes(const QString &infoHash,const QStringList &episodes,QString *error)
{
    if(!m_session||m_readds.contains(infoHash))return false;
    for(const auto &handle:m_session->get_torrents())if(hashString(handle).compare(infoHash,Qt::CaseInsensitive)==0) {
        const auto info=handle.torrent_file();
        if(!info){if(error)*error=QStringLiteral("种子元数据尚未读取完成，请稍后更新。");return false;}
        if(!episodes.isEmpty()&&episodePriorities(*info,episodes,error).empty())return false;
        for(auto &entry:m_entries)if(entry.infoHash.compare(infoHash,Qt::CaseInsensitive)==0) {
            entry.onlyEpisodes=episodes;entry.paused=false;entry.autoStopped=false;
            m_readds.insert(infoHash,entry);saveState();
            // Re-open the same task after removal is acknowledged. Rechecking
            // existing data reuses cached pieces and avoids a second live hash.
            m_session->remove_torrent(handle);return true;
        }
    }
    if(error)*error=QStringLiteral("下载任务已不存在。");return false;
}

bool TorrentEngine::resume(const QString &infoHash)
{
    if (!m_session)
        return false;
    for (const auto &handle : m_session->get_torrents()) {
        if (hashString(handle).compare(infoHash, Qt::CaseInsensitive) == 0) {
            if(m_settings.stopOnCompletion&&handle.status().is_finished){stopCompleted(handle);emit changed();return true;}
            updateEntryPaused(infoHash, false);
            if (m_metadataPending.contains(infoHash) && handle.torrent_file()) {
                QString error;
                if (!saveMagnetTorrent(handle, &error)) {
                    m_saveErrors.insert(infoHash, error);
                    emit engineError(error);
                    emit changed();
                    return false;
                }
            } else {
                if (!m_metadataPending.contains(infoHash) && m_settings.queueing)
                    handle.set_flags(lt::torrent_flags::auto_managed);
                handle.resume();
                m_trackerWarnings.remove(infoHash);handle.force_reannounce();handle.force_dht_announce();
            }
            emit changed();
            return true;
        }
    }
    return false;
}

bool TorrentEngine::remove(const QString &infoHash, bool deleteFiles)
{
    if (!m_session)
        return false;
    for (const auto &handle : m_session->get_torrents()) {
        if (hashString(handle).compare(infoHash, Qt::CaseInsensitive) == 0) {
            lt::remove_flags_t flags{};
            if (deleteFiles)
                flags = lt::session::delete_files;
            m_session->remove_torrent(handle, flags);
            removeEntry(infoHash);
            m_metadataPending.remove(infoHash);m_readds.remove(infoHash);
            m_saveErrors.remove(infoHash);
            m_trackerWarnings.remove(infoHash);
            emit torrentRemoved(infoHash);
            emit changed();
            return true;
        }
    }
    return false;
}

void TorrentEngine::appendEntry(const Entry &entry)
{
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&](const Entry &existing) {
                                       return existing.infoHash == entry.infoHash
                                           && !entry.infoHash.isEmpty();
                                   }),
                    m_entries.end());
    m_entries.push_back(entry);
    saveState();
}

void TorrentEngine::removeEntry(const QString &infoHash)
{
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&](const Entry &entry) {
                                       return entry.infoHash.compare(infoHash, Qt::CaseInsensitive) == 0;
                                   }),
                    m_entries.end());
    saveState();
}

void TorrentEngine::updateEntryPaused(const QString &infoHash, bool paused)
{
    for (auto &entry : m_entries) {
        if (entry.infoHash.compare(infoHash, Qt::CaseInsensitive) == 0)
            entry.paused = paused;
    }
    saveState();
}

void TorrentEngine::saveState() const
{
    QJsonArray array;
    for (const auto &entry : m_entries) {
        QJsonObject object;
        object.insert(QStringLiteral("uri"), entry.uri);
        object.insert(QStringLiteral("save_path"), entry.savePath);
        object.insert(QStringLiteral("info_hash"), entry.infoHash);
        object.insert(QStringLiteral("paused"), entry.paused);
        QJsonArray episodes;for(const auto &ep:entry.onlyEpisodes)episodes.append(ep);
        object.insert(QStringLiteral("only_episodes"),episodes);
        object.insert(QStringLiteral("auto_stopped"),entry.autoStopped);
        QJsonArray trackers;for(const auto &tracker:entry.trackers)trackers.append(tracker);object.insert("trackers",trackers);
        array.append(object);
    }
    QSaveFile file(m_stateDir + QStringLiteral("/state.json"));const auto data=QJsonDocument(array).toJson(QJsonDocument::Indented);
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit())qWarning()<<"Cannot persist download state:"<<file.errorString();
}

void TorrentEngine::loadState()
{
    QFile file(m_stateDir + QStringLiteral("/state.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return;
    for (const auto &value : doc.array()) {
        const auto object = value.toObject();
        Entry entry;
        entry.uri = object.value(QStringLiteral("uri")).toString();
        entry.savePath = object.value(QStringLiteral("save_path")).toString();
        entry.infoHash = object.value(QStringLiteral("info_hash")).toString();
        entry.paused = object.value(QStringLiteral("paused")).toBool();
        for(const auto &ep:object.value(QStringLiteral("only_episodes")).toArray())entry.onlyEpisodes.append(ep.toString());
        entry.autoStopped=object.value(QStringLiteral("auto_stopped")).toBool();
        for(const auto &tracker:object.value("trackers").toArray())entry.trackers.append(tracker.toString());
        if (!entry.uri.isEmpty())
            m_entries.push_back(entry);
    }
}

QString TorrentEngine::hashString(const lt::torrent_handle &handle) const
{
    return hashToQString(handle.info_hashes());
}

void TorrentEngine::pollAlerts()
{
    if (!m_session)
        return;
    bool stateChanged = false;
    std::vector<lt::alert *> alerts;
    m_session->pop_alerts(&alerts);
    for (const auto *alert : alerts) {
        if (const auto *error = lt::alert_cast<lt::torrent_error_alert>(alert)) {
            const QString message = QStringLiteral("torrent error: ")
                + QString::fromStdString(error->error.message());
            qWarning().noquote() << message;
            emit engineError(message);
        } else if(const auto *tracker=lt::alert_cast<lt::tracker_error_alert>(alert)) {
            const QString hash=hashString(tracker->handle);
            const QString message=QUrl(QString::fromUtf8(tracker->tracker_url())).host()+QStringLiteral("：")+QString::fromStdString(tracker->error.message());
            if(m_trackerWarnings.value(hash)!=message)qWarning().noquote()<<"Tracker"<<hash<<message;
            m_trackerWarnings.insert(hash,message);stateChanged=true;
        } else if(const auto *tracker=lt::alert_cast<lt::tracker_reply_alert>(alert)) {
            m_trackerWarnings.remove(hashString(tracker->handle));stateChanged=true;
        } else if(const auto *listen=lt::alert_cast<lt::listen_failed_alert>(alert)) {
            m_networkWarning=QStringLiteral("网络监听失败：")+QString::fromStdString(listen->error.message());qWarning().noquote()<<m_networkWarning;stateChanged=true;
        } else if (const auto *metadata = lt::alert_cast<lt::metadata_received_alert>(alert)) {
            const QString hash = hashString(metadata->handle);
            if (m_metadataPending.contains(hash)) {
                QString error;
                if (!saveMagnetTorrent(metadata->handle, &error)) {
                    metadata->handle.pause();
                    m_saveErrors.insert(hash, error);
                    emit engineError(error);
                }
            }
            stateChanged = true;
        } else if(const auto *removed=lt::alert_cast<lt::torrent_removed_alert>(alert)) {
            const QString hash=hashToQString(removed->info_hashes);
            if(m_readds.contains(hash)) {
                const auto entry=m_readds.take(hash);QString error;
                if(!addTorrentFile(entry.uri,entry.savePath,false,&error,true,entry.onlyEpisodes,nullptr,entry.trackers,entry.infoHash)) {
                    m_saveErrors.insert(hash,error);emit engineError(error);
                }
            }
            stateChanged=true;
        } else if (const auto *finished=lt::alert_cast<lt::torrent_finished_alert>(alert)) {
            if(m_settings.stopOnCompletion)stopCompleted(finished->handle);
            stateChanged = true;
        }
    }
    if (stateChanged)
        emit changed();
}

void TorrentEngine::stopCompleted(const lt::torrent_handle &handle)
{
    if(!handle.is_valid())return;
    const QString pendingHash=hashString(handle);
    if(m_readds.contains(pendingHash)||!handle.status().is_finished)return;
    handle.unset_flags(lt::torrent_flags::auto_managed);
    handle.pause();
    const QString hash=hashString(handle);
    bool dirty=false;
    for(auto &entry:m_entries)if(entry.infoHash==hash) {
        if(!entry.paused||!entry.autoStopped)dirty=true;
        entry.paused=true;entry.autoStopped=true;
    }
    if(dirty)saveState();
}

// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "net/TorrentEngine.h"
#include "net/AnimeDownloads.h"
#include "core/LocalLibrary.h"
#include "core/AppConfig.h"
#include "core/TitleParser.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include <QDebug>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

#include <functional>

namespace {
bool waitUntil(const std::function<bool()> &condition, int timeout = 15000)
{
    QElapsedTimer timer; timer.start();
    do {
        QCoreApplication::processEvents();
        if (condition()) return true;
        QThread::msleep(10);
    } while (timer.elapsed() < timeout);
    return false;
}
QByteArray encode(const lt::entry &entry)
{
    std::string buffer; lt::bencode(std::back_inserter(buffer), entry);
    return QByteArray::fromStdString(buffer);
}
QByteArray readFile(const QString &path)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir sandbox;
    if (!sandbox.isValid()) return 2;
    QFile report(QCoreApplication::applicationDirPath() + "/download-smoke.log");
    if (!report.open(QIODevice::WriteOnly|QIODevice::Text)) return 3;
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
        report.write(QByteArray(label) + (ok ? " PASS\n" : " FAIL\n")); report.flush();
        qInfo() << label << (ok ? "PASS" : "FAIL"); if (!ok) ++failures;
    };

    const EngineSettings defaults;
    check(defaults.uploadLimit==1024&&defaults.downloadLimit==0&&defaults.maxConnections==0&&defaults.maxUploads==0
        &&defaults.activeDownloads==0&&defaults.activeSeeds==0&&defaults.activeLimit==0,"defaults only limit combined upload to 1 MiB per second");
    const auto restoredDefaults=EngineSettings::fromJson(defaults.toJson());
    check(restoredDefaults.maxConnections==0&&restoredDefaults.activeDownloads==0&&restoredDefaults.uploadLimit==1024,
        "unlimited values survive settings serialization");
    auto customJson=defaults.toJson();customJson["active_downloads"]=7;customJson["max_connections"]=500;customJson["upload_limit"]=256;
    const auto custom=EngineSettings::fromJson(customJson);
    check(custom.activeDownloads==7&&custom.maxConnections==500&&custom.uploadLimit==256,"explicit custom limits remain configurable");

    // Own generated payload. All HTTP, tracker, metadata and payload traffic stays
    // on loopback; engine persistence is isolated from the user's real tasks.
    QByteArray payload(128*1024, '\0');
    for (int i=0; i<payload.size(); ++i) payload[i] = char((i*37+i/251)%251);
    const QString fileName = QStringLiteral("[Group A] 测试番剧 S01E01.mkv");
    const QString seedPath = sandbox.path()+"/seed";
    QDir().mkpath(seedPath);
    QFile seedFile(seedPath+"/"+fileName);
    if (!seedFile.open(QIODevice::WriteOnly) || seedFile.write(payload)!=payload.size()) return 4;
    seedFile.close();

    lt::settings_pack seedSettings;
    seedSettings.set_str(lt::settings_pack::listen_interfaces,"127.0.0.2:0");
    seedSettings.set_str(lt::settings_pack::outgoing_interfaces,"127.0.0.2");
    seedSettings.set_bool(lt::settings_pack::enable_dht,false);
    seedSettings.set_bool(lt::settings_pack::enable_lsd,false);
    seedSettings.set_bool(lt::settings_pack::enable_upnp,false);
    seedSettings.set_bool(lt::settings_pack::enable_natpmp,false);
    seedSettings.set_int(lt::settings_pack::alert_mask,lt::alert_category::error|lt::alert_category::status);
    lt::session seed(seedSettings);
    if (!waitUntil([&]{ return seed.listen_port()>0; })) return 5;

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) return 6;
    const QByteArray base="http://127.0.0.1:"+QByteArray::number(server.serverPort());
    lt::entry metainfo;
    metainfo["announce"] = (base+"/announce").toStdString();
    auto &info = metainfo["info"];
    info["name"] = fileName.toUtf8().toStdString();
    info["length"] = payload.size();
    info["piece length"] = 16384;
    QByteArray pieces;
    for (int i=0;i<payload.size();i+=16384)
        pieces+=QCryptographicHash::hash(payload.mid(i,16384),QCryptographicHash::Sha1);
    info["pieces"] = pieces.toStdString();
    const QByteArray torrent = encode(metainfo);
    auto seedParams=lt::load_torrent_buffer(lt::span<char const>(torrent.constData(),torrent.size()));
    seedParams.save_path=seedPath.toStdString();
    seedParams.trackers.clear();
    seedParams.flags &= ~(lt::torrent_flags::paused|lt::torrent_flags::auto_managed);
    seedParams.flags |= lt::torrent_flags::seed_mode;
    const auto seedHandle=seed.add_torrent(seedParams);
    const QString hash=QString::fromLatin1(QByteArray(seedHandle.info_hashes().v1.data(),20).toHex());
    auto trackerlessMeta=metainfo;trackerlessMeta.dict().erase("announce");const auto trackerlessTorrent=encode(trackerlessMeta);
    int foreignTrackerRequests=0;

    const QString batchRoot="TestCollection";
    const QString ep1="Test S01E01.mkv",ep2="Test S01E02.mkv";
    QDir().mkpath(seedPath+"/"+batchRoot);
    for(const auto &name:{ep1,ep2}) {
        QFile media(seedPath+"/"+batchRoot+"/"+name);media.open(QIODevice::WriteOnly);media.write(payload);media.close();
    }
    lt::entry batchMeta;batchMeta["announce"]=(base+"/announce").toStdString();
    auto &batchInfo=batchMeta["info"];batchInfo["name"]=batchRoot.toStdString();batchInfo["piece length"]=16384;
    batchInfo["pieces"]=(pieces+pieces).toStdString();
    lt::entry::list_type batchFiles;
    for(const auto &name:{ep1,ep2}) {
        lt::entry f;f["length"]=payload.size();f["path"]=lt::entry::list_type{lt::entry(name.toStdString())};batchFiles.push_back(f);
    }
    batchInfo["files"]=batchFiles;
    const QByteArray batchTorrent=encode(batchMeta);
    auto batchParams=lt::load_torrent_buffer(lt::span<char const>(batchTorrent.constData(),batchTorrent.size()));
    batchParams.save_path=seedPath.toStdString();batchParams.trackers.clear();
    batchParams.flags&=~(lt::torrent_flags::paused|lt::torrent_flags::auto_managed);batchParams.flags|=lt::torrent_flags::seed_mode;
    const auto batchSeed=seed.add_torrent(batchParams);
    const QString batchHash=QString::fromLatin1(QByteArray(batchSeed.info_hashes().v1.data(),20).toHex());

    QVector<QByteArray> parallelTorrents;QStringList parallelHashes,parallelNames;
    for(int n=1;n<=5;++n) {
        const QString name=QString("[Parallel] Parallel Anime S01E%1.mkv").arg(n,2,10,QChar('0'));
        QFile media(seedPath+"/"+name);media.open(QIODevice::WriteOnly);media.write(payload);media.close();
        lt::entry meta=metainfo;meta["info"]["name"]=name.toStdString();
        const QByteArray encoded=encode(meta);parallelTorrents.append(encoded);parallelNames.append(name);
        auto params=lt::load_torrent_buffer(lt::span<char const>(encoded.constData(),encoded.size()));
        params.save_path=seedPath.toStdString();params.trackers.clear();
        params.flags&=~(lt::torrent_flags::paused|lt::torrent_flags::auto_managed);params.flags|=lt::torrent_flags::seed_mode;
        const auto handle=seed.add_torrent(params);
        parallelHashes.append(QString::fromLatin1(QByteArray(handle.info_hashes().v1.data(),20).toHex()));
    }
    bool holdFirst=false;QPointer<QTcpSocket> heldReply;int parallelRequests=0;
    auto sendTorrent=[](QTcpSocket *socket,const QByteArray &body) {
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/x-bittorrent\r\nContent-Length: "
            +QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
        socket->disconnectFromHost();
    };

    QObject::connect(&server,&QTcpServer::newConnection,&app,[&] {
        while (auto *socket=server.nextPendingConnection()) {
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
                QByteArray request=socket->property("request").toByteArray()+socket->readAll();
                socket->setProperty("request",request);
                if (!request.contains("\r\n\r\n")) return;
                if(socket->property("handled").toBool())return;socket->setProperty("handled",true);
                if(request.startsWith("GET /unresponsive-tracker"))return;
                if(request.startsWith("GET /parallel")) {
                    ++parallelRequests;
                    const int index=request.mid(13).split(' ').value(0).toInt()-1;
                    if(index==0&&holdFirst){heldReply=socket;return;}
                    sendTorrent(socket,parallelTorrents.value(index));return;
                }
                QByteArray body=request.startsWith("GET /batch")?batchTorrent:torrent;
                if(request.startsWith("GET /trackerless"))body=trackerlessTorrent;
                if(request.startsWith("GET /foreign-tracker"))++foreignTrackerRequests;
                if (request.startsWith("GET /bad")) body="<html>not a torrent</html>";
                if (request.startsWith("GET /announce")) {
                    lt::entry response;
                    response["interval"]=1;
                    QByteArray peers; peers.append(char(127)); peers.append(char(0));
                    peers.append(char(0)); peers.append(char(2));
                    peers.append(char(seed.listen_port()>>8)); peers.append(char(seed.listen_port()&255));
                    response["peers"]=peers.toStdString(); body=encode(response);
                }
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/x-bittorrent\r\nContent-Length: "
                    +QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
                socket->disconnectFromHost();
            });
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QTcpSocket::deleteLater);
        }
    });

    EngineSettings settings;
    settings.stopOnCompletion=false;
    settings.bindAddress="127.0.0.1"; settings.listenPort=0;
    settings.dht=false; settings.lsd=false; settings.upnp=false; settings.queueing=true;
    settings.publicTrackerFallback=false;
    const QString stateDir=sandbox.path()+"/state";
    TorrentEngine engine(settings,nullptr,stateDir);
    QObject::connect(&engine,&TorrentEngine::engineError,&app,[&](const QString &message){report.write(message.toUtf8()+"\n");report.flush();});
    auto diagnose=[&] {
        std::vector<lt::alert *> alerts;seed.pop_alerts(&alerts);
        for(const auto *a:alerts)report.write(QByteArray::fromStdString(a->message())+"\n");
        for(const auto &task:engine.listTorrents())report.write(QStringLiteral("Task: %1 state=%2 peers=%3/%4 %5\n")
            .arg(task.title).arg(int(task.status)).arg(task.seeders).arg(task.leechers).arg(task.statusMessage).toUtf8());
        report.flush();
    };
    QString error;
    check(engine.start(&error),"isolated engine starts");
    auto automaticNetwork=settings;automaticNetwork.bindAddress.clear();engine.applySettings(automaticNetwork);
    const auto autoNetwork=engine.networkStatus();
    check(autoNetwork.value("outgoing_interfaces").toString().isEmpty()&&autoNetwork.value("listen_interfaces").toString().contains("0.0.0.0:")&&autoNetwork.value("listen_interfaces").toString().contains("[::]:"),
        "automatic networking clears fixed interface binding and listens on IPv4 and IPv6");
    engine.applySettings(settings);
    check(engine.networkStatus().value("outgoing_interfaces").toString()=="127.0.0.1","explicit manual interface remains supported");
    {
        // Held exclusively, so the bind is refused the way a port reserved by Windows is.
        QTcpServer holder;
        check(holder.listen(QHostAddress::LocalHost),"a port can be held for the test");
        const int held=holder.serverPort();
        auto occupied=settings;occupied.listenPort=held;
        engine.applySettings(occupied);
        const auto moved=[&]{const int port=engine.networkStatus().value("listen_port").toInt();return port>0&&port!=held;};
        check(waitUntil(moved),"a refused listen port moves to one the system allows");
        check(engine.networkStatus().value("warning").toString().isEmpty(),"moving off a refused port is not reported as a failure");
        engine.applySettings(settings);
        check(waitUntil([&]{return engine.networkStatus().value("listen_port").toInt()>0;}),"the engine keeps listening after the port setting changes back");

        // A port Windows has reserved answers differently from one in use. Which ports
        // are reserved depends on the machine, so this only runs where there are some.
        QProcess netsh;
        netsh.start("netsh",{"interface","ipv4","show","excludedportrange","protocol=tcp"});
        netsh.waitForFinished(5000);
        const auto range=QRegularExpression("(?m)^\\s*(\\d+)\\s+(\\d+)\\s*$").match(QString::fromLocal8Bit(netsh.readAllStandardOutput()));
        if(range.hasMatch()) {
            const int reserved=range.captured(1).toInt();
            auto refused=settings;refused.listenPort=reserved;
            engine.applySettings(refused);
            check(waitUntil([&]{const int port=engine.networkStatus().value("listen_port").toInt();return port>0&&port!=reserved;}),"a port reserved by Windows moves to one the system allows");
            check(!engine.networkStatus().value("listen_interfaces").toString().endsWith(QString(":%1").arg(reserved)),"the reserved port is given up");
            check(engine.networkStatus().value("warning").toString().isEmpty(),"a reserved port is not reported as a failure");
            engine.applySettings(settings);
        }
    }
    const QString destination=sandbox.path()+QStringLiteral("/下载目录");
    bool done=false,ok=false,savedBeforeAdd=false;
    engine.addTorrent(QString::fromLatin1(base+"/file.torrent"),destination,[&](bool success,QString message) {
        ok=success; error=message; done=true;
        savedBeforeAdd=!QDir(destination).entryList({"*.torrent"},QDir::Files|QDir::Hidden).isEmpty();
    });
    check(waitUntil([&]{return done;})&&ok,"HTTP torrent is added");
    if (!ok) qWarning().noquote()<<error;
    check(savedBeforeAdd,"torrent saved in chosen folder before success");
    const auto files=QDir(destination).entryList({"*.torrent"},QDir::Files|QDir::Hidden);
    check(files.size()==1 && readFile(QDir(destination).filePath(files.value(0)))==torrent,
        "downloaded torrent preserves original bytes");
    check(QFileInfo(QDir(destination).filePath(files.value(0))).isHidden()&&QDir(destination).entryList({"*.torrent"},QDir::Files).isEmpty(),
        "saved torrent is a hidden file");
    const bool httpComplete=waitUntil([&]{return readFile(destination+"/"+fileName)==payload;});
    check(httpComplete,"HTTP task automatically downloads payload from local seeder");
    if(!httpComplete)diagnose();
    engine.pause(hash);
    check(waitUntil([&]{const auto t=engine.listTorrents();return t.size()==1&&t[0].status==DownloadStatus::Paused;}),
        "task can be paused");
    engine.stop();
    check(engine.start(&error),"engine restarts from saved local torrent");
    check(waitUntil([&]{const auto t=engine.listTorrents();return t.size()==1&&t[0].status==DownloadStatus::Paused;}),
        "restored task preserves pause state");
    const auto entries=QJsonDocument::fromJson(readFile(stateDir+"/state.json")).array();
    check(entries.size()==1 && entries[0].toObject()["uri"].toString().startsWith(destination)
        && entries[0].toObject()["uri"].toString().endsWith(".torrent"),"state uses local torrent instead of refetch URL");
    engine.remove(hash);
    waitUntil([&]{return engine.listTorrents().isEmpty();});

    const QString magnetDestination=sandbox.path()+QStringLiteral("/磁力下载");
    const QString magnet="magnet:?xt=urn:btih:"+hash+"&dn=Local-test&x.pe=127.0.0.2:"
        +QString::number(seed.listen_port());
    done=false;ok=false;
    engine.addTorrent(magnet,magnetDestination,[&](bool success,QString message){done=true;ok=success;error=message;});
    check(done&&ok,"magnet task is added");
    bool payloadBeforeTorrent=false;
    const bool magnetComplete=waitUntil([&] {
        const bool saved=!QDir(magnetDestination).entryList({"*.torrent"},QDir::Files|QDir::Hidden).isEmpty();
        const auto tasks=engine.listTorrents();
        if (!tasks.isEmpty()&&tasks[0].downloaded>0&&!saved) payloadBeforeTorrent=true;
        return saved&&readFile(magnetDestination+"/"+fileName)==payload;
    },25000);
    check(magnetComplete,"magnet metadata is saved and payload auto starts");
    if(!magnetComplete)diagnose();
    check(!payloadBeforeTorrent,"magnet payload waits until torrent file is saved");
    const auto magnetFiles=QDir(magnetDestination).entryList({"*.torrent"},QDir::Files|QDir::Hidden);
    bool matches=false;
    if (!magnetFiles.isEmpty()) {
        try { auto p=lt::load_torrent_file(QDir(magnetDestination).filePath(magnetFiles[0]).toStdString());
            matches=p.ti->info_hashes()==seedHandle.info_hashes(); } catch (...) {}
    }
    check(matches,"exported magnet torrent keeps original info hash");
    engine.addTorrent(magnet,magnetDestination,[&](bool success,QString){ok=success;});
    check(!ok&&engine.listTorrents().size()==1,"duplicate download does not add a second task");
    {
        LocalLibrary migrated(sandbox.path()+"/migrated.json");migrated.load();AppConfig cfg;cfg.engine=settings;
        AnimeDownloads migration(&engine,&migrated,&cfg);AnimeSubject cached;cached.bangumiId=77;cached.nameCn=QStringLiteral("测试番剧");
        migration.adoptCachedSubjects({cached});migration.synchronize();
        check(migrated.items().size()==1&&migrated.items()[0].resource.releaseGroup=="Group A"
            &&migrated.localReleases(77).size()==1,"completed legacy task maps uniquely to cached anime and fansub");
    }
    settings.stopOnCompletion=true;engine.applySettings(settings);
    check(waitUntil([&]{const auto tasks=engine.listTorrents();
        const auto state=QJsonDocument::fromJson(readFile(stateDir+"/state.json")).array();
        return tasks.size()==1&&tasks[0].status==DownloadStatus::Completed&&state.size()==1&&state[0].toObject()["paused"].toBool();
    }),"completion stops transfer and persists pause");
    engine.resume(hash);
    check(waitUntil([&]{const auto t=engine.listTorrents();return t.size()==1&&t[0].status==DownloadStatus::Completed&&t[0].upSpeed==0;}),
        "completed task stays stopped when resume is clicked");
    engine.stop();engine.start(&error);
    check(waitUntil([&]{const auto t=engine.listTorrents();return t.size()==1&&t[0].status==DownloadStatus::Completed;}),
        "automatic completion stop survives restart");
    settings.stopOnCompletion=false;engine.applySettings(settings);
    check(waitUntil([&]{const auto t=engine.listTorrents();return t.size()==1&&t[0].status==DownloadStatus::Seeding;}),
        "turning the stop off returns stopped tasks to seeding");
    engine.remove(hash);waitUntil([&]{return engine.listTorrents().isEmpty();});

    {
        // Deleting with files clears what the task created, and nothing else.
        const QString libraryRoot=sandbox.path()+QStringLiteral("/番剧库");
        auto rooted=settings;rooted.savePath=libraryRoot;engine.applySettings(rooted);
        QStringList engineErrors;
        const auto errorWatch=QObject::connect(&engine,&TorrentEngine::engineError,&app,[&](const QString &message){engineErrors<<message;});
        auto download=[&](const QString &folder) {
            done=false;ok=false;
            engine.addTorrent(QString::fromLatin1(base+"/file.torrent"),folder,[&](bool success,QString message){done=true;ok=success;error=message;});
            return waitUntil([&]{return done&&ok&&readFile(folder+"/"+fileName)==payload;},25000);
        };
        auto torrents=[](const QString &folder){return QDir(folder).entryList({"*.torrent"},QDir::Files|QDir::Hidden);};
        auto removed=[&](const QString &folder) {
            return waitUntil([&]{return engine.listTorrents().isEmpty()&&torrents(folder).isEmpty()&&!QFile::exists(folder+"/"+fileName);});
        };
        auto write=[](const QString &path){QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write("x")==1;};

        const QString nested=libraryRoot+QStringLiteral("/番剧甲/字幕组/第 1 季");
        check(download(nested),"nested task downloads");
        engine.remove(hash,true);
        check(removed(nested)&&waitUntil([&]{return !QDir(libraryRoot+QStringLiteral("/番剧甲")).exists();})&&QDir(libraryRoot).exists(),
            "deleting removes video, torrent and empty folders but keeps the download root");

        const QString shared=libraryRoot+QStringLiteral("/番剧乙/字幕组/第 1 季");
        check(write(libraryRoot+QStringLiteral("/番剧乙/笔记.txt")),"user file sits beside the task folders");
        check(download(shared),"task beside a user file downloads");
        engine.remove(hash,true);
        check(removed(shared)&&waitUntil([&]{return !QDir(libraryRoot+QStringLiteral("/番剧乙/字幕组")).exists();})
            &&QFile::exists(libraryRoot+QStringLiteral("/番剧乙/笔记.txt")),"folder walk stops at a folder holding the user's file");

        const QString occupied=libraryRoot+QStringLiteral("/番剧丙/第 1 季");
        check(write(occupied+QStringLiteral("/字幕.ass")),"user file sits in the task folder");
        check(download(occupied),"task in an occupied folder downloads");
        engine.remove(hash,true);
        check(removed(occupied)&&QFile::exists(occupied+QStringLiteral("/字幕.ass")),"other files in the task folder survive");

        const QString outside=sandbox.path()+QStringLiteral("/自选目录/番剧丁");
        check(download(outside),"task outside the download root downloads");
        engine.remove(hash,true);
        check(removed(outside)&&QDir(outside).exists(),"folders outside the download root are left in place");

        const QString kept=libraryRoot+QStringLiteral("/番剧戊");
        check(download(kept),"task to keep downloads");
        engine.remove(hash,false);
        check(waitUntil([&]{return engine.listTorrents().isEmpty();})&&readFile(kept+"/"+fileName)==payload&&torrents(kept).size()==1,
            "removing the task alone keeps video and torrent");

        const QString locked=libraryRoot+QStringLiteral("/番剧己");
        check(download(locked),"task with a file in use downloads");
        {
            engine.pause(hash);
            waitUntil([&]{const auto t=engine.listTorrents();return t.size()==1&&t[0].status==DownloadStatus::Paused;});
            QFile inUse(locked+"/"+fileName);inUse.open(QIODevice::ReadOnly);
            engine.remove(hash,true);
            check(waitUntil([&]{return engineErrors.join("\n").contains(QStringLiteral("部分文件没有删除"));})&&QFile::exists(locked+"/"+fileName),
                "a file that cannot be deleted is reported");
        }
        waitUntil([&]{return engine.listTorrents().isEmpty();});
        QObject::disconnect(errorWatch);
        engine.applySettings(settings);
    }

    const QString selective=sandbox.path()+"/selective";done=false;ok=false;
    engine.addTorrentTracked(QString::fromLatin1(base+"/batch"),selective,[&](bool success,QString,QString){done=true;ok=success;},{"2"});
    check(waitUntil([&]{return done;})&&ok,"overlap batch is accepted with episode selection");
    check(waitUntil([&]{return readFile(selective+"/"+batchRoot+"/"+ep2)==payload;})
        && !QFile::exists(selective+"/"+batchRoot+"/"+ep1),"only missing video downloads, earlier episode is skipped");
    const auto selectedFiles=engine.filesFor(batchHash);int wanted=0;for(const auto &f:selectedFiles)if(f.wanted)++wanted;
    check(selectedFiles.size()==2&&wanted==1,"engine exposes selected per-file completion");
    check(engine.selectEpisodes(batchHash,{"1"},&error)&&waitUntil([&]{return readFile(selective+"/"+batchRoot+"/"+ep1)==payload;}),
        "existing batch can select another missing episode without duplicate task");
    if(readFile(selective+"/"+batchRoot+"/"+ep1)!=payload)diagnose();
    engine.remove(batchHash);waitUntil([&]{return engine.listTorrents().isEmpty();});

    LocalLibrary library(sandbox.path()+"/library.json");library.load();AppConfig config;config.engine=settings;config.engine.savePath=sandbox.path()+"/organized";
    AnimeDownloads downloads(&engine,&library,&config);QStringList downloadErrors;
    QObject::connect(&downloads,&AnimeDownloads::error,&app,[&](const QString &e){downloadErrors.append(e);});
    AnimeSubject subject;subject.bangumiId=42;subject.nameCn=QStringLiteral("本地测试番剧");subject.aliases={"Test"};subject.episodeCount=2;
    ResourceResult one;one.title="[Group A] Test S01E01 [1080p]";one.releaseGroup="Group A";one.infoHash=hash;one.tags=parseTitle(one.title);
    one.torrentUrl=QString::fromLatin1(base+"/file.torrent");
    auto failedSource=one;failedSource.siteId="failed";failedSource.siteName="Failed Source";failedSource.torrentUrl=QString::fromLatin1(base+"/bad");
    one.siteId="working";one.siteName="Working Source";mergeResource(failedSource,one);one=failedSource;
    downloads.downloadOne(subject,one);downloads.downloadOne(subject,one);
    const bool firstReady=waitUntil([&]{downloads.synchronize();return library.downloaded(42,one);});
    check(firstReady&&engine.listTorrents().size()==1&&library.items().size()==1,"rapid repeated clicks create one task and record");
    check(firstReady&&library.items().first().resource.siteId=="working"&&library.items().first().resource.sources.size()==2,
        "failed torrent source automatically falls back to working source and creates only one transfer");
    const QString automatic=library.downloadDirectory(config.engine.savePath,subject,"Group A",1);
    check(library.items().value(0).savePath==automatic&&readFile(automatic+"/"+fileName)==payload,
        "coordinator creates anime group folder and automatically downloads");
    ResourceResult batch=one;batch.title="[Group A] Test [01-02] [1080p]";batch.infoHash=batchHash;
    batch.sources.clear();batch.seenOn.clear();
    batch.torrentUrl=QString::fromLatin1(base+"/batch");
    downloads.downloadGroup(subject,{one,batch});downloads.downloadGroup(subject,{one,batch});
    check(waitUntil([&]{downloads.synchronize();return library.localReleases(42).size()==2;})
        &&engine.listTorrents().size()==2&&!QFile::exists(automatic+"/"+batchRoot+"/"+ep1),"group update skips cached first episode and fills second once");
    LocalLibrary persisted(library.path());persisted.load();
    check(persisted.localReleases(42).size()==2&&persisted.subjects().size()==1&&downloadErrors.isEmpty(),
        "completed local collection and playback records survive reload");
    for(const auto &message:downloadErrors){report.write(message.toUtf8()+"\n");report.flush();}
    {
        // Changing the download folder: engine tasks move inside libtorrent, a record
        // without a task moves its files directly, and every stored path follows.
        auto clean=[](const QString &path){return QDir::cleanPath(QDir::fromNativeSeparators(path));};
        const QString oldRoot=config.engine.savePath,newRoot=sandbox.path()+"/moved";
        LibraryItem leftover;leftover.key="leftover";leftover.subject=subject;leftover.subject.bangumiId=43;
        leftover.resource.title="[Group B] Leftover - 01";leftover.savePath=oldRoot+"/Leftover/Group B/Season 1";
        QDir().mkpath(leftover.savePath);const QString leftoverFile=leftover.savePath+"/Leftover - 01.mkv";
        {QFile f(leftoverFile);f.open(QIODevice::WriteOnly);f.write(payload);}
        leftover.files={{leftoverFile,qint64(payload.size()),true}};leftover.phase="complete";leftover.complete=true;
        check(library.put(leftover),"record without a task is stored");
        const auto plan=downloads.planMove(oldRoot);
        check(plan.items==3&&plan.anime==2&&plan.external==0,"folder change counts the tasks and records under the old folder");
        int moved=-1;QStringList moveErrors;
        QObject::connect(&downloads,&AnimeDownloads::moveFinished,&app,[&](int n,const QStringList &e){moved=n;moveErrors=e;});
        downloads.moveDownloads(oldRoot,newRoot);
        check(waitUntil([&]{return moved>=0;})&&moveErrors.isEmpty()&&moved==3,"folder change moves everything and reports once");
        for(const auto &message:moveErrors){report.write(message.toUtf8()+"\n");report.flush();}
        const QString newAutomatic=library.downloadDirectory(newRoot,subject,"Group A",1);
        check(readFile(newAutomatic+"/"+fileName)==payload&&!QFile::exists(automatic+"/"+fileName),"engine task data moves with its task");
        check(readFile(newRoot+"/Leftover/Group B/Season 1/Leftover - 01.mkv")==payload&&!QFile::exists(leftoverFile),"finished files without a task are moved");
        bool pathsMoved=true;
        for(const auto &item:library.items()) {
            pathsMoved=pathsMoved&&clean(item.savePath).startsWith(newRoot);
            for(const auto &file:item.files)pathsMoved=pathsMoved&&clean(file.path).startsWith(newRoot);
        }
        check(pathsMoved,"library records point at the new folder");
        bool tasksMoved=engine.listTorrents().size()==2;
        for(const auto &task:engine.listTorrents())tasksMoved=tasksMoved&&clean(task.savePath).startsWith(newRoot);
        const auto state=QJsonDocument::fromJson(readFile(stateDir+"/state.json")).array();
        bool torrentsMoved=state.size()==2;
        for(const auto &entry:state) {
            const QString uri=entry.toObject()["uri"].toString();
            torrentsMoved=torrentsMoved&&clean(uri).startsWith(newRoot)&&QFile::exists(uri);
        }
        check(tasksMoved&&torrentsMoved,"engine tasks and their saved torrents now live in the new folder");
        check(!QDir(oldRoot+"/Leftover").exists(),"folders emptied by the move are removed");
        check(waitUntil([&]{downloads.synchronize();return library.localReleases(42).size()==2;}),"moved videos stay playable");
    }
    {
        // A download the drive clearly cannot hold is refused before anything is reserved.
        QStringList spaceErrors;
        auto connection=QObject::connect(&downloads,&AnimeDownloads::error,&app,[&](const QString &e){spaceErrors<<e;});
        ResourceResult huge;huge.title="[Group C] Huge - 01 [1080p]";huge.releaseGroup="Group C";huge.tags=parseTitle(huge.title);
        huge.infoHash=QString(40,QLatin1Char('c'));huge.torrentUrl=QString::fromLatin1(base+"/huge.torrent");huge.fileSize=1LL<<55;
        AnimeSubject other=subject;other.bangumiId=44;
        const auto before=library.items().size();
        downloads.downloadOne(other,huge);
        check(spaceErrors.size()==1&&spaceErrors.first().contains(QStringLiteral("磁盘空间不足"))&&library.items().size()==before,
              "downloads that cannot fit are refused with a reason");
        QObject::disconnect(connection);
    }
    engine.remove(hash);engine.remove(batchHash);waitUntil([&]{return engine.listTorrents().isEmpty();});

    {
        LocalLibrary multiLibrary(sandbox.path()+"/multi-library.json");multiLibrary.load();AppConfig multiConfig;multiConfig.engine=settings;
        multiConfig.engine.savePath=sandbox.path()+"/parallel";AnimeDownloads multi(&engine,&multiLibrary,&multiConfig);
        AnimeSubject anime;anime.bangumiId=88;anime.nameCn="Parallel Anime";anime.aliases={"Parallel Anime"};anime.episodeCount=5;
        QVector<ResourceResult> releases;
        for(int n=1;n<=5;++n){ResourceResult r;r.title=parallelNames[n-1];r.releaseGroup="Parallel";r.infoHash=parallelHashes[n-1];
            r.torrentUrl=QString::fromLatin1(base+"/parallel"+QByteArray::number(n));releases.append(r);}
        QStringList errors;QObject::connect(&multi,&AnimeDownloads::error,&app,[&](QString message){errors.append(message);});
        holdFirst=true;parallelRequests=0;multi.downloadGroup(anime,releases);multi.downloadGroup(anime,releases);
        check(waitUntil([&]{return parallelRequests==5&&heldReply&&engine.listTorrents().size()==4;},5000),
            "five torrent requests dispatch without waiting for the stalled first request");
        check(waitUntil([&]{multi.synchronize();return multiLibrary.localReleases(88).size()==4;},10000),
            "later tasks download while first torrent request is stalled");
        if(heldReply)sendTorrent(heldReply,parallelTorrents.first());holdFirst=false;
        check(waitUntil([&]{multi.synchronize();return multiLibrary.localReleases(88).size()==5;},10000)
            &&multiLibrary.items().size()==5&&engine.listTorrents().size()==5&&errors.isEmpty(),
            "all parallel tasks finish once with unlimited activity settings");
        for(const auto &h:parallelHashes)engine.remove(h);
        waitUntil([&]{return engine.listTorrents().isEmpty();});
    }

    {
        LocalLibrary stalledLibrary(sandbox.path()+"/stall-library.json");stalledLibrary.load();AppConfig stallConfig;stallConfig.engine=settings;
        stallConfig.engine.savePath=sandbox.path()+"/stalled";stallConfig.engine.sourceStallSeconds=1;
        AnimeDownloads stalled(&engine,&stalledLibrary,&stallConfig);AnimeSubject anime;anime.bangumiId=99;anime.nameCn="Stalled Anime";
        lt::entry dormant;dormant["info"]["name"]=fileName.toStdString();dormant["info"]["length"]=16384;dormant["info"]["piece length"]=16384;dormant["info"]["pieces"]=std::string(20,'z');
        const QString dormantFile=sandbox.path()+"/dormant.torrent";QFile dormantOutput(dormantFile);dormantOutput.open(QIODevice::WriteOnly);dormantOutput.write(encode(dormant));dormantOutput.close();
        ResourceResult r;r.title="[Group] Stalled Anime S01E01";r.releaseGroup="Group";r.siteId="stalled";r.siteName="Stalled";r.torrentUrl=dormantFile;r.seeders=100;
        const auto dormantInfo=lt::load_torrent_file(dormantFile.toStdString());r.infoHash=QString::fromLatin1(QByteArray(dormantInfo.ti->info_hashes().v1.data(),20).toHex());
        auto alternate=r;alternate.infoHash=hash;alternate.siteId="available";alternate.siteName="Available";alternate.seeders=1;alternate.torrentUrl=QString::fromLatin1(base+"/file.torrent");mergeResource(r,alternate);
        stalled.downloadOne(anime,r);
        check(waitUntil([&]{stalled.synchronize();return stalledLibrary.downloaded(99,r);})&&engine.listTorrents().size()==1,"zero-payload stalled transfer switches to an available source without parallel duplicates");
        check(stalledLibrary.items().value(0).attemptedSources.size()==2&&stalledLibrary.items().value(0).resource.siteId=="available","automatic source attempts persist in local inventory");
        engine.remove(hash);waitUntil([&]{return engine.listTorrents().isEmpty();});
    }
    {
        engine.applySettings(automaticNetwork);
        LocalLibrary hintsLibrary(sandbox.path()+"/hints-library.json");hintsLibrary.load();AppConfig hintsConfig;hintsConfig.engine=settings;hintsConfig.engine.savePath=sandbox.path()+"/hints";
        AnimeDownloads hints(&engine,&hintsLibrary,&hintsConfig);AnimeSubject anime;anime.bangumiId=101;anime.nameCn="Trackerless Test";
        ResourceResult r;r.title="[Group] Trackerless Test S01E01";r.releaseGroup="Group";r.siteId="file";r.infoHash=hash;r.torrentUrl=QString::fromLatin1(base+"/trackerless");
        auto same=r;same.siteId="magnet";same.torrentUrl.clear();same.magnetUri="magnet:?xt=urn:btih:"+hash+"&tr="+QString::fromLatin1(QUrl::toPercentEncoding(QString::fromLatin1(base+"/announce")));mergeResource(r,same);
        auto foreign=same;foreign.siteId="other-hash";foreign.infoHash=QString(40,'9');foreign.magnetUri="magnet:?xt=urn:btih:"+foreign.infoHash+"&tr="+QString::fromLatin1(QUrl::toPercentEncoding(QString::fromLatin1(base+"/foreign-tracker")));mergeResource(r,foreign);
        hints.downloadOne(anime,r);
        check(waitUntil([&]{hints.synchronize();return hintsLibrary.downloaded(101,r);}),"trackerless torrent downloads using matching source magnet trackers with DHT disabled");
        const auto states=QJsonDocument::fromJson(readFile(stateDir+"/state.json")).array();const auto hinted=states.first().toObject();
        check(hinted.value("trackers").toArray()==QJsonArray{QString::fromLatin1(base+"/announce")}&&foreignTrackerRequests==0,"trackers from unrelated hashes are excluded");
        check(readFile(hinted.value("uri").toString())==trackerlessTorrent,"tracker repair preserves the original torrent file bytes");
        engine.pause(hash);engine.stop();check(engine.start(&error)&&engine.listTorrents().size()==1&&QJsonDocument::fromJson(readFile(stateDir+"/state.json")).array().first().toObject().value("trackers").toArray().size()==1,"tracker hints survive engine restart");
        engine.remove(hash);waitUntil([&]{return engine.listTorrents().isEmpty();});
        done=false;ok=true;
        engine.addTorrentTracked(QString::fromLatin1(base+"/file.torrent"),sandbox.path()+"/wrong-hash",[&](bool success,QString,QString){done=true;ok=success;},{},{},QString(40,'f'));
        check(waitUntil([&]{return done;})&&!ok&&engine.listTorrents().isEmpty(),"mismatched torrent hash cannot start the wrong swarm");
    }
    {
        auto fallback=settings;fallback.publicTrackerFallback=true;fallback.fallbackTrackers={QString::fromLatin1(base+"/unresponsive-tracker"),QString::fromLatin1(base+"/announce")};engine.applySettings(fallback);
        done=false;ok=false;const auto destination=sandbox.path()+"/fallback";
        engine.addTorrentTracked(QString::fromLatin1(base+"/trackerless"),destination,[&](bool success,QString,QString){done=true;ok=success;});
        check(waitUntil([&]{return done;})&&ok&&waitUntil([&]{return readFile(destination+"/"+fileName)==payload;},5000),"public tracker fallback finds a seeder despite a stalled first tier and disabled DHT");
        engine.remove(hash);waitUntil([&]{return engine.listTorrents().isEmpty();});
        auto privateMeta=trackerlessMeta;privateMeta["info"]["private"]=1;const auto privateTorrent=encode(privateMeta);const auto source=sandbox.path()+"/private.torrent";QFile f(source);f.open(QIODevice::WriteOnly);f.write(privateTorrent);f.close();
        done=false;ok=false;QString privateHash;
        engine.addTorrentTracked(source,sandbox.path()+"/private",[&](bool success,QString,QString h){done=true;ok=success;privateHash=h;},{},{QString::fromLatin1(base+"/foreign-tracker")});
        const auto state=QJsonDocument::fromJson(readFile(stateDir+"/state.json")).array();
        check(done&&ok&&state.size()==1&&state.first().toObject().value("trackers").toArray().isEmpty()&&foreignTrackerRequests==0,"private torrents never receive source hints or public fallback trackers");
        engine.remove(privateHash);waitUntil([&]{return engine.listTorrents().isEmpty();});engine.applySettings(settings);
    }
    done=false;ok=true;
    const QString badDestination=sandbox.path()+"/invalid";
    engine.addTorrent(QString::fromLatin1(base+"/bad"),badDestination,[&](bool success,QString){ok=success;done=true;});
    check(waitUntil([&]{return done;})&&!ok && engine.listTorrents().isEmpty()
        &&QDir(badDestination).entryList({"*.torrent"},QDir::Files|QDir::Hidden).isEmpty(),"invalid response creates neither torrent nor task");
    const QString blocked=sandbox.path()+"/blocked";
    QFile block(blocked);block.open(QIODevice::WriteOnly);block.write("occupied");block.close();
    done=false;ok=true;
    engine.addTorrent(magnet,blocked,[&](bool success,QString){ok=success;done=true;});
    check(done&&!ok&&engine.listTorrents().isEmpty(),"unwritable destination rejects download");
    engine.stop();
    return failures ? 1 : 0;
}

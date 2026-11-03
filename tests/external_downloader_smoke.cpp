#include "net/ExternalDownloader.h"
#include "net/TorrentEngine.h"
#include "net/AnimeDownloads.h"
#include "core/AppConfig.h"
#include "core/TitleParser.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QDateTime>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <functional>

bool waitUntil(std::function<bool()> test){QElapsedTimer t;t.start();do{QCoreApplication::processEvents();if(test())return true;QThread::msleep(10);}while(t.elapsed()<5000);return false;}
QByteArray read(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{};}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir dir;QFile report(QCoreApplication::applicationDirPath()+"/external-downloader-smoke.log");report.open(QIODevice::WriteOnly);int failed=0;
    auto check=[&](bool ok,const char *name){report.write(QByteArray(name)+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failed;};
    DownloaderSettings settings;check(settings.selected=="builtin","builtin remains the default");
    settings.selected="custom";settings.programs["custom"]=QCoreApplication::applicationDirPath()+"/ExternalClientProbe.exe";
    settings.programs["qbittorrent"]=QStringLiteral("C:/程序 文件/qbittorrent.exe");settings.customArguments="\"{input}\" \"{saveDir}\"";
    const auto reloaded=DownloaderSettings::fromJson(settings.toJson());
    check(reloaded.programs==settings.programs&&reloaded.customArguments==settings.customArguments&&reloaded.selected=="custom","per-type paths and arguments survive JSON roundtrip");
    QString error;check(ExternalDownloader::validate(settings,&error),"custom executable path validates");
    auto invalid=settings;invalid.programs["custom"]=QCoreApplication::applicationFilePath();
    check(!ExternalDownloader::validate(invalid,&error),"self-launch is rejected");invalid.programs["custom"]=dir.filePath("missing.exe");
    check(!ExternalDownloader::validate(invalid,&error),"missing executable is rejected");invalid=settings;invalid.customArguments="--no-input";
    check(!ExternalDownloader::validate(invalid,&error),"missing input placeholder is rejected");invalid.customArguments="\"{input}\" {bad}";
    check(!ExternalDownloader::validate(invalid,&error),"unknown argument placeholder is rejected");
    LocalLibrary library(dir.filePath("library.json"));library.load();
    LibraryItem item;item.key="sample";item.subject.bangumiId=42;item.subject.nameCn="Test Anime";
    item.resource.title="[Fansub] Test S01E01 [1080p]";item.resource.releaseGroup="Fansub";item.groupKey=releaseGroupKey("Fansub");item.episodes={"1"};
    item.savePath=dir.filePath(QStringLiteral("番剧 空格 & 测试"));QDir().mkpath(item.savePath);item.externalProgram=settings.programs["custom"];
    const QString magnet="magnet:?xt=urn:btih:"+QString(40,'1')+"&dn=literal%20%26%20name&tr=http%3A%2F%2Flocalhost";
    item.downloader="qbittorrent";auto cmd=ExternalDownloader::command(item,magnet,{},true);
    check(cmd.arguments==QStringList({"--save-path="+item.savePath,"--skip-dialog=true",magnet}),"qBittorrent receives save folder and one intact magnet argument");
    item.downloader="transmission";check(ExternalDownloader::command(item,magnet,{},true).arguments==QStringList{magnet},"Transmission uses its torrent opening interface");
    item.downloader="bitcomet";cmd=ExternalDownloader::command(item,"D:/seed file.torrent",{},true);
    check(cmd.arguments.contains("--output="+item.savePath)&&cmd.arguments.contains("D:/seed file.torrent"),"BitComet receives the local torrent and output folder");
    item.downloader="aria2";cmd=ExternalDownloader::command(item,"D:/seed file.torrent",{"2","4"},true);
    check(cmd.console&&cmd.arguments.contains("--select-file=2,4")&&cmd.arguments.contains("--seed-time=0"),"aria2 preserves selected files and completion stop");
    item.downloader="custom";item.externalArguments="\"{input}\" \"{name}\" \"{saveDir}\"";item.resource.title="literal {saveDir} & title";
    cmd=ExternalDownloader::command(item,magnet,{},true);
    check(cmd.arguments==QStringList({magnet,item.resource.title,item.savePath}),"custom replacement is single-pass and preserves literal title content");
    item.resource.title="[Fansub] Test S01E01 [1080p]";
    const QByteArray payload(32*1024,'V');lt::entry meta;meta["info"]["name"]="Test S01E01.mkv";meta["info"]["length"]=payload.size();meta["info"]["piece length"]=16384;
    QByteArray pieces;for(int i=0;i<payload.size();i+=16384)pieces+=QCryptographicHash::hash(payload.mid(i,16384),QCryptographicHash::Sha1);
    meta["info"]["pieces"]=pieces.toStdString();std::string bytes;lt::bencode(std::back_inserter(bytes),meta);const QByteArray torrent=QByteArray::fromStdString(bytes);
    const QString source=dir.filePath("source.torrent");QFile sourceFile(source);sourceFile.open(QIODevice::WriteOnly);sourceFile.write(torrent);sourceFile.close();
    item.resource.torrentUrl=source;item.externalArguments="\"{input}\"";
    int launches=0;ExternalCommand captured;ExternalDownloader service(&library,nullptr,[&](const ExternalCommand &c,QString*){++launches;captured=c;return true;});
    bool done=false,ok=false;ExternalResult sent;
    service.send(item,true,[&](bool success,QString,ExternalResult result){done=true;ok=success;sent=result;});
    check(done&&ok&&launches==1&&QFile::exists(sent.torrentFile)&&read(sent.torrentFile)==torrent,"local torrent is validated and saved before launching external client");
    check(sent.files.size()==1&&!sent.files[0].complete&&sent.episodes==QStringList{"1"},"handoff stores expected videos without falsely completing them");
    auto wrong=item;wrong.resource.infoHash=QString(40,'0');done=false;ok=true;
    service.send(wrong,true,[&](bool success,QString,ExternalResult){done=true;ok=success;});
    check(done&&!ok&&launches==1,"mismatched torrent hash never launches another task");
    auto collection=item;collection.resource.torrentUrl.clear();collection.resource.magnetUri=magnet;collection.onlyEpisodes={"2"};done=false;ok=true;
    service.send(collection,true,[&](bool success,QString,ExternalResult){done=true;ok=success;});
    check(done&&!ok&&launches==1,"magnet collection without file selection cannot duplicate cached episodes");
    item.key=library.itemKey(42,item.resource);item.hash=sent.hash;item.torrentFile=sent.torrentFile;item.files=sent.files;item.phase="external";library.put(item);
    check(library.busy(42,item.resource)&&!library.downloaded(42,item.resource)&&library.groupHasHistory(42,"Fansub"),"external handoff reserves episodes and changes group action without faking download");
    auto duplicate=item;duplicate.key="another-key";done=false;ok=true;service.send(duplicate,true,[&](bool success,QString,ExternalResult){done=true;ok=success;});
    check(done&&!ok&&launches==1,"same torrent hash is not sent twice across aliases");
    LocalLibrary reload(library.path());reload.load();check(reload.items().value(0).downloader=="custom"&&reload.busy(42,item.resource),"external reservations survive restart");
    QFile media(sent.files.value(0).path);media.open(QIODevice::WriteOnly);media.write(QByteArray(payload.size(),'0'));media.close();
    check(ExternalDownloader::verifyFiles(item).isEmpty(),"preallocated wrong content never becomes playable");
    media.open(QIODevice::WriteOnly);media.write(payload);media.close();
    const auto verified=ExternalDownloader::verifyFiles(item);check(verified.size()==1&&verified[0].complete,"external completed payload passes actual piece hash verification");
    media.open(QIODevice::ReadWrite);media.setFileTime(QDateTime::currentDateTime().addSecs(-30),QFileDevice::FileModificationTime);media.close();
    service.checkLocalFiles();
    check(waitUntil([&]{return library.downloaded(42,item.resource);}),"background verifier marks completed external payload automatically");
    check(library.downloaded(42,item.resource)&&library.localReleases(42).size()==1,"verified external video joins local collection and playback");
    QTcpServer server;server.listen(QHostAddress::LocalHost);
    QObject::connect(&server,&QTcpServer::newConnection,&app,[&]{while(auto *s=server.nextPendingConnection()){
        QObject::connect(s,&QTcpSocket::readyRead,s,[s,torrent]{if(!s->readAll().contains("\r\n\r\n"))return;s->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(torrent.size())+"\r\nConnection: close\r\n\r\n"+torrent);s->disconnectFromHost();});
        QObject::connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
    }});
    auto http=item;http.key="http";http.hash.clear();http.resource.torrentUrl="http://127.0.0.1:"+QString::number(server.serverPort())+"/seed";http.savePath=dir.filePath("http");QDir().mkpath(http.savePath);
    LocalLibrary fresh(dir.filePath("fresh.json"));fresh.load();ExternalDownloader httpService(&fresh,nullptr,[&](const ExternalCommand &c,QString*){captured=c;return true;});
    done=false;ok=false;httpService.send(http,true,[&](bool success,QString,ExternalResult r){done=true;ok=success;sent=r;});
    check(waitUntil([&]{return done;})&&ok&&QFile::exists(sent.torrentFile)&&captured.arguments.contains(sent.torrentFile),"HTTP torrent handoff saves the original seed first");
    LibraryItem probe=item;probe.key="probe";probe.resource.infoHash.clear();probe.resource.torrentUrl.clear();probe.resource.magnetUri=magnet;probe.savePath=dir.filePath(QStringLiteral("真实参数 空格 & 测试"));QDir().mkpath(probe.savePath);
    probe.externalArguments="\"{saveDir}/captured.json\" \"{input}\" \"{name}\" \"{saveDir}\"";
    ExternalDownloader native(&fresh);done=false;ok=false;native.send(probe,true,[&](bool success,QString,ExternalResult){done=true;ok=success;});
    const QString output=probe.savePath+"/captured.json";
    check(done&&ok&&waitUntil([&]{return QFile::exists(output);}),"custom executable actually launches without a shell");
    const auto arguments=QJsonDocument::fromJson(read(output)).array();
    check(arguments.size()==3&&arguments[0].toString()==magnet&&arguments[1].toString()==probe.resource.title&&arguments[2].toString()==probe.savePath,"real process receives intact Unicode paths ampersands and magnet parameters");
    {
        LocalLibrary records(dir.filePath("routing.json"));records.load();AppConfig config;config.engine.savePath=dir.filePath("organized");
        config.downloader.selected="custom";config.downloader.programs["custom"]=settings.programs["custom"];
        config.downloader.customArguments="\"{saveDir}/{name}.json\" \"{input}\" \"{saveDir}\"";
        TorrentEngine internal(config.engine,nullptr,dir.filePath("engine-state"));AnimeDownloads coordinator(&internal,&records,&config);
        AnimeSubject anime;anime.bangumiId=66;anime.nameCn="Routing Anime";anime.episodeCount=2;
        QVector<ResourceResult> resources;
        for(int ep=1;ep<=2;++ep) {
            auto data=meta;data["info"]["name"]=QString("Routing S01E%1.mkv").arg(ep,2,10,QChar('0')).toStdString();
            std::string raw;lt::bencode(std::back_inserter(raw),data);
            QFile seed(dir.filePath(QString("routing%1.torrent").arg(ep)));seed.open(QIODevice::WriteOnly);seed.write(QByteArray::fromStdString(raw));seed.close();
            ResourceResult r;r.title=QString("[Fansub] Routing S01E%1").arg(ep,2,10,QChar('0'));r.releaseGroup="Fansub";r.torrentUrl=seed.fileName();resources.append(r);
        }
        auto failedSource=resources.first();failedSource.siteId="failed";failedSource.siteName="Failed Source";failedSource.torrentUrl=dir.filePath("missing.torrent");
        resources[0].siteId="working";resources[0].siteName="Working Source";mergeResource(failedSource,resources.first());resources[0]=failedSource;
        QStringList errors;QObject::connect(&coordinator,&AnimeDownloads::error,&app,[&](QString e){errors.append(e);});
        coordinator.downloadGroup(anime,resources);coordinator.downloadGroup(anime,resources);config.downloader.selected="builtin";
        check(waitUntil([&]{auto items=records.items();return items.size()==2&&items[0].phase=="external"&&items[1].phase=="external";})
            &&errors.isEmpty()&&!internal.running()&&internal.listTorrents().isEmpty(),"one-click uses external client without starting builtin transfer and rejects repeats");
        bool capturedBoth=true;for(const auto &i:records.items())capturedBoth=capturedBoth&&i.downloader=="custom"&&waitUntil([&]{return QFile::exists(i.savePath+"/"+i.resource.title+".json");});
        check(capturedBoth,"switching backend affects only new tasks and preserves reserved external profiles");
        bool fallback=false;for(const auto &i:records.items())if(i.resource.title==resources.first().title)fallback=i.resource.siteId=="working"&&i.resource.sources.size()==2;
        check(fallback,"external handoff also falls back from unavailable source before launching exactly once");
    }
    return failed?1:0;
}

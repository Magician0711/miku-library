// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "HttpFixture.h"
#include "net/DownloaderRpc.h"
#include "core/TitleParser.h"
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>

int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir dir;QFile report(app.applicationDirPath()+"/rpc-smoke.log");report.open(QIODevice::WriteOnly);int failed=0;
    auto check=[&](bool ok,QString message){report.write(message.toUtf8()+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failed;};
    for(const auto &type:QStringList{"qbittorrent","transmission","aria2"}){
        LocalLibrary library(dir.filePath(type+".json"));library.load();AppConfig config;config.engine.stopOnCompletion=true;config.downloader.selected=type;
        HttpFixture server;config.downloader.rpc[type]={true,server.url(type=="qbittorrent"?"":"/rpc"),"test","secret"};
        LibraryItem item;item.key=type;item.downloader=type;item.subject.bangumiId=42;item.subject.nameCn="Test Anime";item.groupKey="group";
        item.resource.title="[Group] Test Anime - 01";item.resource.releaseGroup="Group";item.resource.infoHash=QString(40,'a');item.resource.magnetUri="magnet:?xt=urn:btih:"+item.resource.infoHash;
        item.hash=item.resource.infoHash;item.savePath=dir.filePath(type);QDir().mkpath(item.savePath);item.episodes={"1"};item.phase="external_pending";library.put(item);
        bool paused=false,complete=false,removed=false,login=false,token=false,savePath=false,stopOption=false;QStringList methods;
        const QString videoName="Test Anime - 01.mkv";
        server.handler=[&](QByteArray header,QByteArray data){HttpFixture::Response out;const auto path=header.split(' ').value(1);QJsonValue response;
            if(type=="qbittorrent") {
                if(path.contains("auth/login")){login=data.contains("username=test")&&data.contains("password=secret");out.data="Ok.";out.headers="Set-Cookie: SID=fixture; Path=/\r\n";return out;}
                if(!header.contains("SID=fixture")){out.code=403;return out;}
                if(path.contains("app/version")){out.data="v5.0.0";return out;}
                if(path.contains("torrents/add")){savePath=data.contains(item.savePath.toUtf8());stopOption=data.contains("stopped");out.data="Ok.";return out;}
                if(path.contains("torrents/start")){paused=false;methods<<"start";}
                if(path.contains("torrents/stop")){paused=true;methods<<"stop";}
                if(path.contains("torrents/delete")){removed=data.contains("deleteFiles=false");methods<<"remove";}
                if(path.contains("torrents/info"))response=QJsonArray{QJsonObject{{"progress",complete?1.0:.25},{"size",4},{"dlspeed",complete?0:1024},{"state",paused?"stoppedDL":complete?"uploading":"downloading"}}};
                if(path.contains("torrents/files"))response=QJsonArray{QJsonObject{{"name",videoName},{"size",4},{"priority",1},{"progress",1}}};
            } else {
                const auto body=QJsonDocument::fromJson(data).object();const auto method=body.value("method").toString();methods<<method;
                if(type=="transmission") {
                    if(!header.toLower().contains("x-transmission-session-id: fixture")){out.code=409;out.headers="X-Transmission-Session-Id: fixture\r\n";return out;}
                    token=true;const auto args=body.value("arguments").toObject();QJsonObject result;
                    if(method=="torrent-add"){savePath=args.value("download-dir").toString()==item.savePath;stopOption=args.value("paused").toBool();result={{"torrent-added",QJsonObject{{"hashString",item.hash}}}};}
                    if(method=="torrent-start")paused=false;if(method=="torrent-stop")paused=true;
                    if(method=="torrent-remove")removed=!args.value("delete-local-data").toBool(true);
                    if(method=="torrent-get")result={{"torrents",QJsonArray{QJsonObject{{"percentDone",complete?1.0:.25},{"totalSize",4},{"rateDownload",complete?0:1024},{"status",paused?0:complete?6:4},
                        {"files",QJsonArray{QJsonObject{{"name",videoName},{"length",4},{"bytesCompleted",4}}}}}}}};
                    response=QJsonObject{{"result","success"},{"arguments",result}};
                } else {
                    auto params=body.value("params").toArray();token=params.first().toString()=="token:secret";QJsonValue result;
                    if(method=="aria2.addUri"){auto options=params.last().toObject();savePath=options.value("dir").toString()==item.savePath;stopOption=options.value("seed-time").toString()=="0";result="gid123";}
                    if(method=="aria2.pause")paused=true;if(method=="aria2.unpause")paused=false;if(method=="aria2.remove")removed=true;
                    if(method=="aria2.tellStatus")result=QJsonObject{{"totalLength","4"},{"completedLength",complete?"4":"1"},{"downloadSpeed",complete?"0":"1024"},{"status",paused?"paused":"active"},{"seeder",complete?"true":"false"}};
                    if(method=="aria2.getFiles")result=QJsonArray{QJsonObject{{"path",QDir(item.savePath).filePath(videoName)},{"length","4"},{"selected","true"}}};
                    response=QJsonObject{{"jsonrpc","2.0"},{"id",1},{"result",result}};
                }
            }
            out.data=response.isArray()?QJsonDocument(response.toArray()).toJson():QJsonDocument(response.toObject()).toJson();return out;
        };
        DownloaderRpc rpc(&library,&config);bool added=false,ok=false;ExternalResult result;
        rpc.add(item,true,[&](bool success,QString error,ExternalResult r){added=true;ok=success;result=r;if(!error.isEmpty())report.write(error.toUtf8()+"\n");});
        check(spinUntil([&]{return added;})&&ok&&!result.remoteId.isEmpty(),type+" adds managed task");
        check(savePath&&stopOption&&(type=="qbittorrent"?login:token),type+" authenticates and applies directory and completion settings");
        item.remoteId=result.remoteId;item.phase="external";library.put(item);rpc.poll();
        check(spinUntil([&]{return rpc.tasks().value(item.key).progress==25;}),type+" synchronizes real progress");
        bool ack=false;rpc.control(item.key,"pause",[&](bool success,QString){ack=success;});check(spinUntil([&]{return ack;})&&paused&&library.item(item.key)->phase=="external_paused",type+" pauses without losing reservation");
        ack=false;rpc.control(item.key,"resume",[&](bool success,QString){ack=success;});check(spinUntil([&]{return ack;})&&!paused,type+" resumes task");
        QFile video(QDir(item.savePath).filePath(videoName));video.open(QIODevice::WriteOnly);video.write("data");video.close();complete=true;rpc.poll();
        check(spinUntil([&]{return library.item(item.key)->complete&&paused;})&&library.playableFiles(*library.item(item.key)).size()==1,type+" completes playable inventory and automatically stops");
        ack=false;rpc.control(item.key,"remove",[&](bool success,QString){ack=success;});check(spinUntil([&]{return ack;})&&removed&&QFile::exists(video.fileName()),type+" removes remote task while keeping files");
    }
    DownloaderSettings invalid;invalid.selected="aria2";invalid.rpc["aria2"]={true,"http://remote.example/rpc",{}, {}};QString error;
    check(!ExternalDownloader::validate(invalid,&error),"RPC rejects non-local endpoint");
    return failed?1:0;
}

#include "DownloaderRpc.h"
#include "core/TitleParser.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QPointer>
#include <QHttpMultiPart>
#include <QJsonDocument>

namespace {
QByteArray form(const QMap<QString,QString> &values){QUrlQuery q;for(auto i=values.begin();i!=values.end();++i)q.addQueryItem(i.key(),i.value());return q.query(QUrl::FullyEncoded).toUtf8();}
QJsonArray indices(const QStringList &indexes,bool inverse,int count){QJsonArray out;for(int i=1;i<=count;++i)if(indexes.contains(QString::number(i))!=inverse)out.append(i-1);return out;}
QJsonValue first(const QJsonArray &a){return a.isEmpty()?QJsonValue():a.at(0);}
QString baseUrl(QString url){while(url.endsWith('/'))url.chop(1);return url;}
QJsonValue transmissionKeys(QJsonValue value,bool legacy){
    static const QMap<QString,QString> keys={{"download_dir","download-dir"},{"files_wanted","files-wanted"},{"files_unwanted","files-unwanted"},{"delete_local_data","delete-local-data"},
        {"upload_limited","uploadLimited"},{"upload_limit","uploadLimit"},{"download_limited","downloadLimited"},{"download_limit","downloadLimit"},
        {"torrent_added","torrent-added"},{"torrent_duplicate","torrent-duplicate"},{"hash_string","hashString"},{"percent_done","percentDone"},
        {"rate_download","rateDownload"},{"rate_upload","rateUpload"},{"total_size","totalSize"},{"error_string","errorString"},{"bytes_completed","bytesCompleted"}};
    auto key=[&](QString k){return legacy?keys.value(k,k):keys.key(k,k);};
    if(value.isObject()){QJsonObject out;const auto object=value.toObject();for(auto i=object.begin();i!=object.end();++i)out.insert(key(i.key()),transmissionKeys(i.value(),legacy));return out;}
    if(value.isArray()){QJsonArray out;for(auto v:value.toArray())out.append(transmissionKeys(v,legacy));return out;}
    if(value.isString())return key(value.toString());return value;
}
}

DownloaderRpc::DownloaderRpc(LocalLibrary *library,AppConfig *config,QObject *parent)
    :QObject(parent),m_library(library),m_config(config),m_prepare(library,this,{},true)
{
    auto *timer=new QTimer(this);timer->setInterval(3000);connect(timer,&QTimer::timeout,this,&DownloaderRpc::poll);timer->start();
}
bool DownloaderRpc::enabled(const QString &type)const{return DownloaderSettings::supportsRpc(type)&&m_config->downloader.rpcProfile(type).enabled;}
bool DownloaderRpc::manageable(const LibraryItem &i)const{return enabled(i.downloader)&&(!i.hash.isEmpty()||!i.remoteId.isEmpty());}

void DownloaderRpc::wire(const QString &type,const QString &path,const QByteArray &body,const QByteArray &contentType,Wire callback,bool get)
{
    auto settings=m_config->downloader;settings.selected=type;QString validation;
    if(!ExternalDownloader::validate(settings,&validation)){callback(0,{},validation,{});return;}
    const auto profile=settings.rpcProfile(type);QUrl url(baseUrl(profile.url)+path);
    QNetworkRequest request(url);request.setTransferTimeout(10000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader,QString::fromLatin1(contentType));
    if(type=="qbittorrent") {request.setRawHeader("Referer",profile.url.toUtf8()+"/");request.setRawHeader("Origin",profile.url.toUtf8());}
    if(type=="transmission") {
        request.setRawHeader("X-Transmission-Session-Id",m_transmissionSession);
        if(!profile.username.isEmpty())request.setRawHeader("Authorization","Basic "+(profile.username+":"+profile.password).toUtf8().toBase64());
    }
    auto *reply=get?m_net.get(request):m_net.post(request,body);
    connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 n,qint64){if(n>32*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,callback=std::move(callback)]{
        const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto data=reply->readAll();const auto token=reply->rawHeader("X-Transmission-Session-Id");
        const QString error=reply->error()==QNetworkReply::NoError?QString():QStringLiteral("连接失败（HTTP %1）：%2").arg(code).arg(reply->errorString());reply->deleteLater();callback(code,data,error,token);
    });
}

void DownloaderRpc::qb(const QString &method,const QMap<QString,QString> &values,Reply callback,bool get)
{
    const auto p=m_config->downloader.rpcProfile("qbittorrent");const QString identity=p.url+"|"+p.username+"|"+p.password;
    if(!p.username.isEmpty()&&m_qbLogin!=identity) {
        wire("qbittorrent","/api/v2/auth/login",form({{"username",p.username},{"password",p.password}}),"application/x-www-form-urlencoded",[this,identity,method,values,callback,get](int code,QByteArray data,QString error,QByteArray){
            if(code!=200||data.trimmed()!="Ok."){callback(false,{},error.isEmpty()?QStringLiteral("qBittorrent 登录失败，请核对 Web UI 凭据。"):error);return;}
            m_qbLogin=identity;qb(method,values,callback,get);
        });return;
    }
    const auto body=form(values);const QString path="/api/v2/"+method+(get&&!body.isEmpty()?"?"+QString::fromUtf8(body):QString());
    wire("qbittorrent",path,get?QByteArray():body,"application/x-www-form-urlencoded",[this,callback](int code,QByteArray data,QString error,QByteArray){
        if(code==403)m_qbLogin.clear();const auto doc=QJsonDocument::fromJson(data);QJsonValue value=doc.isArray()?QJsonValue(doc.array()):QJsonValue(doc.object());
        callback(code>=200&&code<300,value,error);
    },get);
}

void DownloaderRpc::call(const QString &type,const QString &method,QJsonValue params,Reply callback,int attempt)
{
    const auto profile=m_config->downloader.rpcProfile(type);QJsonObject body;
    if(type=="aria2") {
        auto values=params.toArray();if(!profile.password.isEmpty())values.prepend("token:"+profile.password);
        body={{"jsonrpc","2.0"},{"id",1},{"method","aria2."+method},{"params",values}};
    } else {QString action=method;action.replace('_','-');body={{"method",action},{"arguments",transmissionKeys(params,true)},{"tag",1}};}
    wire(type,{},QJsonDocument(body).toJson(QJsonDocument::Compact),"application/json",[this,type,method,params,callback,attempt](int code,QByteArray data,QString error,QByteArray token){
        if(type=="transmission"&&code==409&&!token.isEmpty()&&attempt<1){m_transmissionSession=token;call(type,method,params,callback,attempt+1);return;}
        const auto doc=QJsonDocument::fromJson(data);const auto object=doc.object();
        if(code<200||code>=300||!doc.isObject()||object.contains("error")){callback(false,{},error.isEmpty()?QStringLiteral("下载器接口拒绝请求，请核对连接设置和接口版本。"):error);return;}
        if(type=="transmission") {
            if(object.value("result").toString()!="success"){callback(false,{},QStringLiteral("Transmission 拒绝请求：")+object.value("result").toString());return;}
            callback(true,transmissionKeys(object.value("arguments"),false),{});
        } else callback(true,object.value("result"),{});
    });
}

void DownloaderRpc::add(const LibraryItem &item,bool stop,ExternalDownloader::Callback callback)
{
    m_prepare.send(item,stop,[this,item,stop,callback](bool valid,QString message,ExternalResult prepared)mutable {
        if(!valid){callback(false,message,prepared);return;}
        if(item.downloader!="aria2"&&prepared.hash.isEmpty()){callback(false,QStringLiteral("此磁力链接缺少可管理的哈希标识，请选择对应的种子文件。"),prepared);return;}
        QFile file(prepared.torrentFile);QByteArray data;if(!prepared.torrentFile.isEmpty()&&file.open(QIODevice::ReadOnly))data=file.readAll();
        Reply finish=[this,item,prepared,callback](bool ok,QJsonValue value,QString error)mutable {
            if(item.downloader=="aria2")prepared.remoteId=value.toString();else prepared.remoteId=prepared.hash;
            if(ok&&prepared.remoteId.isEmpty()){callback(false,QStringLiteral("下载器未返回可管理的任务标识。"),prepared);return;}
            callback(ok,error,prepared);QTimer::singleShot(0,this,&DownloaderRpc::poll);
        };
        const auto &limits=m_config->engine;
        if(item.downloader=="aria2") {
            QJsonObject options{{"dir",item.savePath},{"max-download-limit",QString::number(qint64(limits.downloadLimit)*1024)},{"max-upload-limit",QString::number(qint64(limits.uploadLimit)*1024)}};
            if(stop)options.insert("seed-time","0");if(!prepared.selectedIndexes.isEmpty())options.insert("select-file",prepared.selectedIndexes.join(','));
            if(data.isEmpty())call("aria2","addUri",QJsonArray{QJsonArray{item.resource.magnetUri},options},finish);
            else call("aria2","addTorrent",QJsonArray{QString::fromLatin1(data.toBase64()),QJsonArray{},options},finish);
        } else if(item.downloader=="transmission") {
            QJsonObject params{{"download_dir",item.savePath},{"paused",true}};if(data.isEmpty())params.insert("filename",item.resource.magnetUri);else params.insert("metainfo",QString::fromLatin1(data.toBase64()));
            if(!prepared.selectedIndexes.isEmpty()){params.insert("files_wanted",indices(prepared.selectedIndexes,false,prepared.fileCount));params.insert("files_unwanted",indices(prepared.selectedIndexes,true,prepared.fileCount));}
            call("transmission","torrent_add",params,[this,prepared,finish,limits](bool ok,QJsonValue value,QString error)mutable {
                if(!ok){finish(false,{},error);return;}auto result=value.toObject().value("torrent_added").toObject();if(result.isEmpty())result=value.toObject().value("torrent_duplicate").toObject();
                const QString hash=result.value("hash_string").toString(prepared.hash);
                call("transmission","torrent_set",QJsonObject{{"ids",QJsonArray{hash}},{"upload_limited",limits.uploadLimit>0},{"upload_limit",limits.uploadLimit},{"download_limited",limits.downloadLimit>0},{"download_limit",limits.downloadLimit}},[this,hash,finish](bool set,QJsonValue,QString e)mutable {
                    if(!set){finish(false,{},e);return;}call("transmission","torrent_start",QJsonObject{{"ids",QJsonArray{hash}}},finish);
                });
            });
        } else {
            // Authenticate using the regular API before posting multipart data.
            qb("app/version",{},[this,item,data,prepared,finish,limits](bool ok,QJsonValue,QString error)mutable {
                if(!ok){finish(false,{},error);return;}
                auto *parts=new QHttpMultiPart(QHttpMultiPart::FormDataType);
                auto text=[&](QString name,QByteArray value){QHttpPart part;part.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\""+name+"\"");part.setBody(value);parts->append(part);};
                if(data.isEmpty())text("urls",item.resource.magnetUri.toUtf8());else {QHttpPart seed;seed.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"torrents\"; filename=\"resource.torrent\"");seed.setBody(data);parts->append(seed);}
                text("savepath",item.savePath.toUtf8());text("paused","true");text("stopped","true");text("upLimit",QByteArray::number(qint64(limits.uploadLimit)*1024));text("dlLimit",QByteArray::number(qint64(limits.downloadLimit)*1024));
                const auto profile=m_config->downloader.rpcProfile("qbittorrent");QNetworkRequest request(QUrl(baseUrl(profile.url)+"/api/v2/torrents/add"));request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setRawHeader("Referer",profile.url.toUtf8()+"/");
                auto *reply=m_net.post(request,parts);parts->setParent(reply);
                connect(reply,&QNetworkReply::finished,this,[this,reply,prepared,finish]()mutable {
                    const bool ok=reply->error()==QNetworkReply::NoError&&reply->readAll().trimmed()=="Ok.";reply->deleteLater();
                    if(!ok){finish(false,{},QStringLiteral("qBittorrent 添加任务失败。"));return;}
                    auto start=[this,prepared,finish]{qb("torrents/start",{{"hashes",prepared.hash}},[this,prepared,finish](bool ok,QJsonValue v,QString e){if(ok)finish(ok,v,e);else qb("torrents/resume",{{"hashes",prepared.hash}},finish);});};
                    if(prepared.selectedIndexes.isEmpty()){start();return;}
                    QStringList all,selected;for(int i=0;i<prepared.fileCount;++i)all.append(QString::number(i));for(const auto &index:prepared.selectedIndexes)selected.append(QString::number(index.toInt()-1));
                    qb("torrents/filePrio",{{"hash",prepared.hash},{"id",all.join('|')},{"priority","0"}},[this,prepared,selected,start,finish](bool ok,QJsonValue,QString e){if(!ok){finish(false,{},e);return;}qb("torrents/filePrio",{{"hash",prepared.hash},{"id",selected.join('|')},{"priority","1"}},[start,finish](bool ok,QJsonValue,QString e){if(ok)start();else finish(false,{},e);});});
                });
            },true);
        }
    });
}

void DownloaderRpc::control(const QString &key,const QString &action,std::function<void(bool,QString)> callback)
{
    const auto item=m_library->item(key);if(!item||!manageable(*item)){if(callback)callback(false,QStringLiteral("请先启用此下载器的本机连接管理。"));return;}
    auto done=[this,key,action,callback](bool ok,QJsonValue,QString e){
        if(ok)if(auto record=m_library->item(key)){record->phase=action=="remove"?"cancelled":record->complete?"complete":action=="pause"?"external_paused":"external";QString error;if(!m_library->put(*record,&error)){ok=false;e=error;}}
        if(!ok&&e.isEmpty())e=QStringLiteral("下载器未接受操作。");if(callback)callback(ok,e);else if(!ok)emit error(e);emit changed();
    };
    const QString id=item->remoteId.isEmpty()?item->hash:item->remoteId;
    if(item->downloader=="qbittorrent") {
        const QString method=action=="remove"?"delete":action=="pause"?"stop":"start";
        qb("torrents/"+method,{{"hashes",item->hash},{"deleteFiles","false"}},[this,action,item,done](bool ok,QJsonValue v,QString e){
            if(ok||action=="remove"){done(ok,v,e);return;}qb(action=="pause"?"torrents/pause":"torrents/resume",{{"hashes",item->hash}},done);
        });
    } else if(item->downloader=="transmission") {
        QJsonObject args{{"ids",QJsonArray{item->hash}}};if(action=="remove")args.insert("delete_local_data",false);
        call("transmission",action=="remove"?"torrent_remove":action=="pause"?"torrent_stop":"torrent_start",args,done);
    } else call("aria2",action=="remove"?"remove":action=="pause"?"pause":"unpause",QJsonArray{id},[this,id,action,done](bool ok,QJsonValue v,QString e){
        if(!ok&&action=="remove")call("aria2","removeDownloadResult",QJsonArray{id},done);else done(ok,v,e);
    });
}

void DownloaderRpc::poll()
{
    for(const auto &item:m_library->items()) {
        if(!manageable(item)||m_busy.contains(item.key)||(item.phase!="external"&&item.phase!="external_paused"&&item.phase!="complete"))continue;
        m_busy.insert(item.key);
        auto done=[this,item](bool ok,QJsonValue value,QString error){
            m_busy.remove(item.key);
            if(!ok){auto t=m_tasks.value(item.key);t.statusMessage=QStringLiteral("连接中断 · ")+error;m_tasks.insert(item.key,t);emit changed();return;}
            QJsonObject status;
            if(item.downloader=="qbittorrent")status=first(value.toArray()).toObject();
            else if(item.downloader=="transmission")status=first(value.toObject().value("torrents").toArray()).toObject();
            else status=value.toObject();
            if(!status.isEmpty())synchronize(item,status);
        };
        if(item.downloader=="qbittorrent")qb("torrents/info",{{"hashes",item.hash}},done,true);
        else if(item.downloader=="transmission")call("transmission","torrent_get",QJsonObject{{"ids",QJsonArray{item.hash}},{"fields",QJsonArray{"hash_string","name","status","percent_done","rate_download","rate_upload","total_size","eta","error_string"}}},done);
        else call("aria2","tellStatus",QJsonArray{item.remoteId.isEmpty()?item.hash:item.remoteId},done);
    }
    const auto items=m_library->items();for(auto it=m_tasks.begin();it!=m_tasks.end();) {bool exists=false;for(const auto &item:items)if(item.key==it.key()&&item.phase!="cancelled")exists=true;if(!exists)it=m_tasks.erase(it);else ++it;}
}

void DownloaderRpc::synchronize(const LibraryItem &item,const QJsonObject &s)
{
    auto current=m_library->item(item.key);if(!current||current->phase=="cancelled")return;
    DownloadTask t;t.title=item.resource.title;t.qbtHash="external:"+item.key;t.infoHash=item.hash;bool complete=false,paused=false;
    if(item.downloader=="qbittorrent") {
        t.progress=s.value("progress").toDouble()*100;t.size=qint64(s.value("size").toDouble());t.dlSpeed=qint64(s.value("dlspeed").toDouble());t.upSpeed=qint64(s.value("upspeed").toDouble());t.eta=qint64(s.value("eta").toDouble());const auto state=s.value("state").toString();paused=state.startsWith("paused")||state.startsWith("stopped");complete=t.progress>=99.999&&t.size>0&&!state.contains("checking",Qt::CaseInsensitive);
    } else if(item.downloader=="transmission") {
        t.progress=s.value("percent_done").toDouble()*100;t.size=qint64(s.value("total_size").toDouble());t.dlSpeed=qint64(s.value("rate_download").toDouble());t.upSpeed=qint64(s.value("rate_upload").toDouble());t.eta=qint64(s.value("eta").toDouble());const int state=s.value("status").toInt();paused=state==0;complete=t.progress>=99.999&&t.size>0&&state!=1&&state!=2;
    } else {
        const auto children=s.value("followedBy").toArray();if(!children.isEmpty()){current->remoteId=children.first().toString();m_library->put(*current);return;}
        t.size=s.value("totalLength").toString().toLongLong();const qint64 bytes=s.value("completedLength").toString().toLongLong();t.progress=t.size?double(bytes)*100/t.size:0;t.dlSpeed=s.value("downloadSpeed").toString().toLongLong();t.upSpeed=s.value("uploadSpeed").toString().toLongLong();paused=s.value("status").toString()=="paused";complete=t.size>0&&bytes==t.size&&(s.value("status").toString()=="complete"||s.value("seeder").toString()=="true");
    }
    t.status=complete?DownloadStatus::Completed:paused?DownloadStatus::Paused:DownloadStatus::Downloading;
    t.statusMessage=(complete?QStringLiteral("已完成"):paused?QStringLiteral("已暂停"):QStringLiteral("下载中"))+QStringLiteral(" · ")+DownloaderSettings::name(item.downloader);m_tasks.insert(item.key,t);emit changed();
    if(!complete&&current->phase!=(paused?"external_paused":"external")){current->phase=paused?"external_paused":"external";m_library->put(*current);}
    if(complete&&!current->complete)files(*current,true);
    const bool alreadyStopped=item.downloader=="aria2"&&s.value("status").toString()=="complete";
    if(complete&&m_config->engine.stopOnCompletion&&!paused&&!alreadyStopped&&!m_stopping.contains(item.key)) {
        m_stopping.insert(item.key);control(item.key,"pause",[this,key=item.key](bool ok,QString message){
            if(!ok){emit error(message);QTimer::singleShot(60000,this,[this,key]{m_stopping.remove(key);});}else m_stopping.remove(key);
        });
    }
}

void DownloaderRpc::files(const LibraryItem &item,bool complete)
{
    auto done=[this,item,complete](bool ok,QJsonValue v,QString){
        if(!ok)return;auto record=m_library->item(item.key);if(!record||record->phase=="cancelled")return;
        QJsonArray values=item.downloader=="qbittorrent"?v.toArray():item.downloader=="transmission"?first(v.toObject().value("torrents").toArray()).toObject().value("files").toArray():v.toArray();
        QVector<LibraryFile> files;const QString root=QDir(item.savePath).absolutePath();
        for(const auto &value:values){const auto f=value.toObject();QString name=f.value(item.downloader=="aria2"?"path":"name").toString();
            const QString path=QDir::cleanPath(QFileInfo(name).isAbsolute()?name:QDir(root).absoluteFilePath(name));if(!path.startsWith(root+"/",Qt::CaseInsensitive)||!isVideoFile(path))continue;
            if(item.downloader=="qbittorrent"&&(f.value("priority").toInt()==0||f.value("progress").toDouble()<1))continue;
            if(item.downloader=="aria2"&&f.value("selected").toString()!="true")continue;
            if(item.downloader=="transmission"&&f.value("bytes_completed").toDouble()<f.value("length").toDouble())continue;
            const qint64 size=item.downloader=="aria2"?f.value("length").toString().toLongLong():qint64(f.value(item.downloader=="qbittorrent"?"size":"length").toDouble());
            const QFileInfo info(path);if(size>0&&info.isFile()&&info.size()==size)files.append({path,size,complete});
        }
        if(!files.isEmpty()){record->files=files;record->complete=complete;record->phase="complete";QString e;if(!m_library->put(*record,&e))emit error(e);}
    };
    if(item.downloader=="qbittorrent")qb("torrents/files",{{"hash",item.hash}},done,true);
    else if(item.downloader=="transmission")call("transmission","torrent_get",QJsonObject{{"ids",QJsonArray{item.hash}},{"fields",QJsonArray{"files"}}},done);
    else call("aria2","getFiles",QJsonArray{item.remoteId},done);
}

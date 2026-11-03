#include "ExternalDownloader.h"
#include "TorrentEngine.h"
#include "core/TitleParser.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QTimer>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/file_storage.hpp>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

ExternalDownloader::ExternalDownloader(LocalLibrary *library,QObject *parent,Launcher launcher,bool prepareOnly)
    :QObject(parent),m_library(library),m_net(new QNetworkAccessManager(this)),m_launcher(launcher?launcher:start),m_prepareOnly(prepareOnly)
{
    auto *timer=new QTimer(this);timer->setInterval(10000);connect(timer,&QTimer::timeout,this,&ExternalDownloader::checkLocalFiles);timer->start();
    connect(library,&LocalLibrary::changed,this,[this]{
        for(const auto &i:m_library->items())if(i.phase=="cancelled")m_sentHashes.remove(i.hash);
    });
}

bool ExternalDownloader::validate(const DownloaderSettings &settings,QString *error)
{
    auto fail=[&](const QString &message){if(error)*error=message;return false;};
    if(settings.selected=="builtin")return true;
    if(settings.supportsRpc(settings.selected)&&settings.rpcProfile(settings.selected).enabled) {
        const QUrl url(settings.rpcProfile(settings.selected).url);
        if((url.scheme()!="http"&&url.scheme()!="https")||!(url.host()=="127.0.0.1"||url.host()=="localhost"||url.host()=="::1")||!url.userInfo().isEmpty())
            return fail(QStringLiteral("下载器连接地址必须是本机 HTTP(S) 地址，凭据请填在独立输入框。"));
        return true;
    }
    if(!DownloaderSettings::types().contains(settings.selected))return fail(QStringLiteral("请选择有效的下载器类型。"));
    const QFileInfo exe(settings.programs.value(settings.selected));
    if(!exe.isFile()||exe.suffix().compare("exe",Qt::CaseInsensitive)!=0)
        return fail(QStringLiteral("请在设置中选择已安装下载器的 .exe 程序。"));
    if(exe.canonicalFilePath().compare(QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath(),Qt::CaseInsensitive)==0)
        return fail(QStringLiteral("请选择外部下载器，不能选择 Anime Nexus 自身。"));
    if(settings.selected=="custom") {
        if(!settings.customArguments.contains("{input}"))return fail(QStringLiteral("自定义参数必须包含 {input}，用于传入磁力链接或种子文件。"));
        QString rest=settings.customArguments;rest.remove("{input}");rest.remove("{saveDir}");rest.remove("{name}");
        if(QRegularExpression("\\{[^{}]+\\}").match(rest).hasMatch())return fail(QStringLiteral("参数仅支持 {input}、{saveDir}、{name}。"));
        if(settings.customArguments.count('"')%2!=0)return fail(QStringLiteral("自定义参数的双引号没有成对闭合。"));
    }
    return true;
}

ExternalCommand ExternalDownloader::command(const LibraryItem &item,const QString &input,const QStringList &indexes,bool stopOnCompletion,QString *error)
{
    ExternalCommand c;c.program=item.externalProgram;c.directory=item.savePath;
    if(item.downloader=="qbittorrent")c.arguments={"--save-path="+item.savePath,"--skip-dialog=true",input};
    else if(item.downloader=="transmission")c.arguments={input};
    else if(item.downloader=="aria2") {
        c.console=true;c.arguments={"--dir="+item.savePath,"--bt-save-metadata=true","--continue=true"};
        if(stopOnCompletion)c.arguments.append("--seed-time=0");
        if(!indexes.isEmpty())c.arguments.append("--select-file="+indexes.join(','));
        c.arguments.append(input);
    } else if(item.downloader=="bitcomet") {
        c.arguments=input.startsWith("magnet:")?QStringList{"--url="+input,"--silent"}:QStringList{input,"--output="+item.savePath,"--silent"};
    } else if(item.downloader=="custom") {
        c.arguments=QProcess::splitCommand(item.externalArguments);
        for(auto &arg:c.arguments) {
            // Replace the template once. A title or URI containing placeholder
            // text must remain literal and must never be parsed by a shell.
            QString result;const QRegularExpression token("\\{(input|saveDir|name)\\}");int end=0;
            auto matches=token.globalMatch(arg);while(matches.hasNext()) {
                const auto m=matches.next();result+=arg.mid(end,m.capturedStart()-end);
                result+=m.captured(1)=="input"?input:m.captured(1)=="saveDir"?item.savePath:item.resource.title;
                end=m.capturedEnd();
            }
            result+=arg.mid(end);arg=result;
        }
    } else if(error)*error=QStringLiteral("未知的外部下载器类型。");
    return c;
}

bool ExternalDownloader::start(const ExternalCommand &command,QString *error)
{
    QProcess process;process.setProgram(command.program);process.setArguments(command.arguments);process.setWorkingDirectory(command.directory);
#ifdef Q_OS_WIN
    if(command.console)process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    if(!process.startDetached()){if(error)*error=process.errorString();return false;}return true;
}

void ExternalDownloader::send(const LibraryItem &item,bool stopOnCompletion,Callback callback)
{
    DownloaderSettings settings;settings.selected=item.downloader;settings.programs[item.downloader]=item.externalProgram;settings.customArguments=item.externalArguments;
    QString error;if(!m_prepareOnly&&!validate(settings,&error)){callback(false,error,{});return;}
    const QString input=item.resource.torrentUrl.isEmpty()?item.resource.magnetUri:item.resource.torrentUrl;
    const QUrl url(input);
    if(url.scheme()=="http"||url.scheme()=="https") {
        QNetworkRequest request(url);request.setTransferTimeout(20000);auto *reply=m_net->get(request);
        connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64){if(received>32*1024*1024)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,item,stopOnCompletion,callback=std::move(callback)]()mutable {
            const auto data=reply->readAll();const bool ok=reply->error()==QNetworkReply::NoError;const auto message=reply->errorString();reply->deleteLater();
            if(!ok){ExternalResult failure;failure.retryable=true;callback(false,QStringLiteral("种子获取失败：")+message,failure);return;}
            QString error;const QString path=TorrentEngine::saveTorrentFile(data,item.savePath,&error);
            if(path.isEmpty()){ExternalResult failure;failure.retryable=true;callback(false,error,failure);return;}
            launch(item,path,stopOnCompletion,std::move(callback));
        });
    } else if(input.startsWith("magnet:"))launch(item,input,stopOnCompletion,std::move(callback));
    else {
        QFile file(url.isLocalFile()?url.toLocalFile():input);
        if(!file.open(QIODevice::ReadOnly)){ExternalResult failure;failure.retryable=true;callback(false,QStringLiteral("找不到可用的磁力链接或种子文件。"),failure);return;}
        const QString path=TorrentEngine::saveTorrentFile(file.readAll(),item.savePath,&error);
        if(path.isEmpty()){ExternalResult failure;failure.retryable=true;callback(false,error,failure);return;}
        launch(item,path,stopOnCompletion,std::move(callback));
    }
}

void ExternalDownloader::launch(const LibraryItem &item,const QString &input,bool stopOnCompletion,Callback callback)
{
    ExternalResult result;result.hash=item.resource.infoHash.isEmpty()?btihFromMagnet(item.resource.magnetUri).value_or(QString()):item.resource.infoHash.toLower();
    QStringList indexes;bool requiresSelection=false;
    if(!input.startsWith("magnet:")) {
        try {
            const auto params=lt::load_torrent_file(input.toStdString());const auto &hash=params.ti->info_hashes();
            const QString actual=QString::fromLatin1(hash.has_v1()?QByteArray(hash.v1.data(),20).toHex():QByteArray(hash.v2.data(),32).toHex());
            if(!result.hash.isEmpty()&&actual!=result.hash){callback(false,QStringLiteral("种子内容与资源 hash 不一致，已停止发送。"),{});return;}
            result.hash=actual;result.torrentFile=input;const auto &files=params.ti->layout();QSet<QString> assigned;
            result.fileCount=files.num_files();
            const QSet<QString> wanted(item.onlyEpisodes.begin(),item.onlyEpisodes.end());int videos=0;
            for(lt::file_index_t n{0};n<files.end_file();++n) {
                const QString relative=QString::fromStdString(files.file_path(n));const QString path=QDir::cleanPath(QDir(item.savePath).absoluteFilePath(relative));
                if(!path.startsWith(QDir(item.savePath).absolutePath()+"/",Qt::CaseInsensitive))continue;
                const auto episodes=fileEpisodeNumbers(relative);bool selected=wanted.isEmpty();
                if(!wanted.isEmpty())for(const auto &ep:episodes)if(wanted.contains(ep))selected=true;
                if(isVideoFile(path)) {
                    bool duplicate=false;for(const auto &ep:episodes)if(assigned.contains(ep)||(!wanted.isEmpty()&&!wanted.contains(ep)))duplicate=true;
                    if(duplicate)selected=false;
                    if(!selected)requiresSelection=true;
                    if(selected){++videos;for(const auto &ep:episodes)assigned.insert(ep);result.files.append({path,files.file_size(n),false});}
                }
                if(selected)indexes.append(QString::number(int(n)+1));
            }
            if(videos==0){callback(false,QStringLiteral("种子中没有可确认的缺集视频，已阻止可能的重复下载。"),{});return;}
            result.episodes=QStringList(assigned.begin(),assigned.end());
        } catch(const std::exception &e){callback(false,QString::fromUtf8(e.what()),{});return;}
    } else if(!item.onlyEpisodes.isEmpty()) {
        // A magnet alone has no file indexes yet. Keep automatic collection
        // selection in the builtin engine until a local torrent is available.
        requiresSelection=true;
    }
    if(requiresSelection&&((!m_prepareOnly&&item.downloader!="aria2")||result.torrentFile.isEmpty())) {
        callback(false,QStringLiteral("此合集需要选择缺集文件，当前外部下载器的启动方式无法保证去重。请改选单集资源，或使用内置下载器；本地种子也可使用 aria2。"),{});return;
    }
    if(!result.hash.isEmpty()) {
        bool exists=m_sentHashes.contains(result.hash);
        for(const auto &i:m_library->items())if(i.key!=item.key&&i.hash==result.hash
            &&(i.phase=="external"||i.phase=="external_paused"||i.phase=="external_pending"||i.phase=="downloading"||i.phase=="paused"||i.complete))exists=true;
        if(exists){callback(false,QStringLiteral("相同种子已经提交过，已跳过重复任务。"),{});return;}
    }
    if(m_prepareOnly){result.selectedIndexes=requiresSelection?indexes:QStringList{};callback(true,{},result);return;}
    QString error;const auto cmd=command(item,input,requiresSelection?indexes:QStringList{},stopOnCompletion,&error);
    if(!error.isEmpty()||!m_launcher(cmd,&error)){callback(false,QStringLiteral("无法启动外部下载器：")+error,{});return;}
    if(!result.hash.isEmpty())m_sentHashes.insert(result.hash);
    callback(true,{},result);
}

QVector<LibraryFile> ExternalDownloader::verifyFiles(const LibraryItem &item)
{
    // Verify the actual v1/hybrid pieces before making an external video
    // playable. File size alone is insufficient for preallocated downloads.
    try {
        const auto params=lt::load_torrent_file(item.torrentFile.toStdString());const auto info=params.ti;
        if(!info->info_hashes().has_v1()||info->num_pieces()==0)return {};
        const QString hashId=QString::fromLatin1(QByteArray(info->info_hashes().v1.data(),20).toHex());
        if(!item.hash.isEmpty()&&item.hash.compare(hashId,Qt::CaseInsensitive)!=0)return {};
        const auto &files=info->layout();const QString root=QDir(item.savePath).absolutePath();
        QStringList paths;QVector<qint64> modified;
        for(lt::file_index_t n{0};n<files.end_file();++n) {
            const QString path=QDir::cleanPath(QDir(root).absoluteFilePath(QString::fromStdString(files.file_path(n))));
            if(!path.startsWith(root+"/",Qt::CaseInsensitive))return {};
            paths.append(path);const QFileInfo f(path);modified.append(f.lastModified().toMSecsSinceEpoch());
            if(!files.pad_file_at(n)&&files.file_size(n)>0&&(!f.isFile()||f.size()!=files.file_size(n)))return {};
        }
        QCryptographicHash hash(QCryptographicHash::Sha1);lt::piece_index_t piece{0};int remaining=info->piece_size(piece);
        for(lt::file_index_t n{0};n<files.end_file();++n) {
            QFile input(paths[int(n)]);const bool pad=files.pad_file_at(n);
            if(!pad&&files.file_size(n)>0&&!input.open(QIODevice::ReadOnly))return {};
            qint64 left=files.file_size(n);
            while(left>0) {
                const int size=int(qMin<qint64>(qMin(remaining,1024*1024),left));
                const QByteArray data=pad?QByteArray(size,'\0'):input.read(size);
                if(data.size()!=size)return {};hash.addData(data);left-=size;remaining-=size;
                if(remaining==0) {
                    const auto expected=info->hash_for_piece(piece);
                    if(hash.result()!=QByteArray(expected.data(),20))return {};
                    ++piece;hash.reset();if(int(piece)<info->num_pieces())remaining=info->piece_size(piece);
                }
            }
        }
        if(int(piece)!=info->num_pieces())return {};
        for(lt::file_index_t n{0};n<files.end_file();++n)if(!files.pad_file_at(n)&&files.file_size(n)>0) {
            const QFileInfo f(paths[int(n)]);
            if(f.size()!=files.file_size(n)||f.lastModified().toMSecsSinceEpoch()!=modified[int(n)])return {};
        }
        QVector<LibraryFile> verified;
        for(const auto &media:item.files)for(lt::file_index_t n{0};n<files.end_file();++n)
            if(paths[int(n)]==media.path&&isVideoFile(media.path))verified.append({media.path,files.file_size(n),true});
        return verified;
    } catch(...) {return {};}
}

void ExternalDownloader::checkLocalFiles()
{
    if(m_verifying)return;
    for(const auto &item:m_library->items()) {
        if(item.phase!="external"||item.torrentFile.isEmpty()||item.files.isEmpty())continue;
        bool ready=true;QString stamp;
        try {
            const auto meta=lt::load_torrent_file(item.torrentFile.toStdString());const auto &layout=meta.ti->layout();
            const QString root=QDir(item.savePath).absolutePath();
            for(lt::file_index_t n{0};n<layout.end_file();++n) {
                if(layout.pad_file_at(n)||layout.file_size(n)==0)continue;
                const QString path=QDir::cleanPath(QDir(root).absoluteFilePath(QString::fromStdString(layout.file_path(n))));
                const QFileInfo f(path);const qint64 age=f.lastModified().msecsTo(QDateTime::currentDateTime());
                if(!path.startsWith(root+"/",Qt::CaseInsensitive)||!f.isFile()||f.size()!=layout.file_size(n)||age<10000){ready=false;break;}
                stamp+=path+":"+QString::number(f.size())+":"+QString::number(f.lastModified().toMSecsSinceEpoch())+";";
            }
        } catch(...) {continue;}
        if(!ready||m_checkedFiles.value(item.key)==stamp)continue;
        m_checkedFiles[item.key]=stamp;m_verifying=true;
        auto *watcher=new QFutureWatcher<QVector<LibraryFile>>(this);
        connect(watcher,&QFutureWatcher<QVector<LibraryFile>>::finished,this,[this,watcher,key=item.key]{
            const auto files=watcher->result();watcher->deleteLater();m_verifying=false;
            QTimer::singleShot(0,this,&ExternalDownloader::checkLocalFiles);
            auto record=m_library->item(key);if(!record||record->phase!="external"||files.isEmpty())return;
            record->files=files;record->complete=true;record->phase="complete";QString error;
            if(!m_library->put(*record,&error))emit m_library->storageError(error);
        });
        watcher->setFuture(QtConcurrent::run([item]{return verifyFiles(item);}));break;
    }
}

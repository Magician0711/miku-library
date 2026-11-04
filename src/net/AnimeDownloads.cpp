#include "AnimeDownloads.h"
#include "TorrentEngine.h"
#include "ExternalDownloader.h"
#include "DownloaderRpc.h"
#include "core/AppConfig.h"
#include "core/TitleParser.h"

#include <QDir>
#include <QPointer>
#include <QTimer>
#include <QUrlQuery>
#include <QRegularExpression>

namespace {
QString hashOf(const ResourceResult &r)
{
    if(!r.infoHash.isEmpty())return r.infoHash.toLower();
    if(const auto hash=btihFromMagnet(r.magnetUri))return *hash;
    const auto match=QRegularExpression("(?:^|/)([0-9a-fA-F]{40})\\.torrent(?:[?#]|$)").match(r.torrentUrl);
    return match.captured(1).toLower();
}
QStringList trackersFor(const ResourceResult &r)
{
    const QString hash=hashOf(r);QStringList trackers;if(hash.isEmpty())return trackers;
    auto add=[&](QString magnet,QString knownHash){
        const auto parsed=btihFromMagnet(magnet);if(!parsed||*parsed!=hash||(!knownHash.isEmpty()&&knownHash.compare(hash,Qt::CaseInsensitive)!=0))return;
        for(const auto &url:QUrlQuery(QUrl(magnet)).allQueryItemValues("tr",QUrl::FullyDecoded))if(!trackers.contains(url)&&trackers.size()<64)trackers.append(url);
    };
    add(r.magnetUri,r.infoHash);for(const auto &source:r.sources)add(source.magnetUri,source.infoHash);
    return trackers;
}
QString phaseOf(const DownloadTask &task)
{
    if(task.progress>=99.999&&task.size>0)return "complete";
    if(task.status==DownloadStatus::Paused)return "paused";
    if(task.status==DownloadStatus::Error)return "error";
    return "downloading";
}
}

AnimeDownloads::AnimeDownloads(TorrentEngine *engine,LocalLibrary *library,AppConfig *config,QObject *parent)
    :QObject(parent),m_engine(engine),m_library(library),m_config(config),m_timer(new QTimer(this)),m_syncTimer(new QTimer(this))
{
    m_external=new ExternalDownloader(library,this);
    m_rpc=new DownloaderRpc(library,config,this);connect(m_rpc,&DownloaderRpc::error,this,&AnimeDownloads::error);
    connect(engine,&TorrentEngine::changed,this,&AnimeDownloads::scheduleSynchronize);
    m_syncTimer->setSingleShot(true);m_syncTimer->setInterval(100);
    connect(m_syncTimer,&QTimer::timeout,this,&AnimeDownloads::synchronize);
    connect(engine,&TorrentEngine::torrentRemoved,library,&LocalLibrary::markRemoved);
    connect(library,&LocalLibrary::storageError,this,&AnimeDownloads::error);
    m_timer->setInterval(1500);connect(m_timer,&QTimer::timeout,this,&AnimeDownloads::synchronize);m_timer->start();
    QTimer::singleShot(0,this,&AnimeDownloads::restorePending);
}

void AnimeDownloads::synchronize()
{
    if(!m_engine->running())return;
    const auto tasks=m_engine->listTorrents();
    for(const auto &task:tasks) {
        const bool completed=task.progress>=99.999&&task.size>0;
        m_library->reconcile(task.infoHash,m_engine->filesFor(task.infoHash),completed,phaseOf(task));
    }
    recoverStalled(tasks);
}

void AnimeDownloads::recoverStalled(const QVector<DownloadTask> &tasks)
{
    if(m_config->engine.sourceStallSeconds<=0){m_waiting.clear();return;}
    QSet<QString> live;
    for(const auto &item:m_library->items()) {
        if(item.downloader!="builtin"||item.phase!="downloading"||m_activeKeys.contains(item.key))continue;
        for(const auto &task:tasks)if(task.infoHash==item.hash) {
            if(task.downloaded>0||task.receivedPayload>0||task.dlSpeed>0)m_payloadReceived.insert(item.key);
            if(task.status!=DownloadStatus::Downloading||m_payloadReceived.contains(item.key))break;
            live.insert(item.key);
            auto &clock=m_waiting[item.key];if(!clock.isValid()){clock.start();break;}
            if(clock.elapsed()<qint64(m_config->engine.sourceStallSeconds)*1000)break;
            QVector<ResourceResult> alternatives;
            for(const auto &candidate:downloadCandidates(item.resource)) {
                const QString uri=candidate.torrentUrl.isEmpty()?candidate.magnetUri:candidate.torrentUrl;
                // Another URL for the same swarm cannot solve an unavailable swarm.
                if(!item.attemptedSources.contains(uri)&&hashOf(candidate)!=item.hash)alternatives.append(candidate);
            }
            if(alternatives.isEmpty())break;
            m_activeKeys.insert(item.key);m_engine->remove(item.hash,false);
            auto pending=item;pending.phase="queued";QString error;
            if(!m_library->put(pending,&error)){m_activeKeys.remove(item.key);emit this->error(error);break;}
            emit information(QStringLiteral("%1 暂无可用连接，正在尝试其他来源。").arg(item.resource.title));
            waitForRemoval(item.key,item.hash,alternatives);break;
        }
    }
    for(auto it=m_waiting.begin();it!=m_waiting.end();)if(!live.contains(it.key()))it=m_waiting.erase(it);else ++it;
}

void AnimeDownloads::waitForRemoval(const QString &key,const QString &hash,QVector<ResourceResult> candidates,int attempt)
{
    QTimer::singleShot(100,this,[this,key,hash,candidates=std::move(candidates),attempt]{
        auto item=m_library->item(key);
        if(!item||item->phase!="queued"){m_activeKeys.remove(key);return;}
        bool present=false;for(const auto &task:m_engine->listTorrents())if(task.infoHash==hash)present=true;
        if(present&&attempt<50){waitForRemoval(key,hash,candidates,attempt+1);return;}
        if(present){item->phase="error";m_library->put(*item);m_activeKeys.remove(key);emit error(QStringLiteral("原任务尚未停止，已取消切换以避免重复下载。"));return;}
        m_waiting.remove(key);startCandidate(key,candidates);
    });
}

void AnimeDownloads::adoptExisting(const AnimeSubject &subject,const QVector<ResourceResult> &resources)
{
    const auto tasks=m_engine->listTorrents();
    for(const auto &r:resources) {
        const QString hash=hashOf(r);if(hash.isEmpty())continue;
        if(m_library->item(m_library->itemKey(subject.bangumiId,r)))continue;
        bool otherGroup=false;
        for(const auto &i:m_library->items())if(i.hash==hash
            &&(i.subject.bangumiId!=subject.bangumiId||i.groupKey!=releaseGroupKey(r.releaseGroup)))otherGroup=true;
        if(otherGroup)continue;
        for(const auto &task:tasks)if(task.infoHash.compare(hash,Qt::CaseInsensitive)==0) {
            const auto release=annotate({r}).value(0);
            LibraryItem item;item.key=m_library->itemKey(subject.bangumiId,r);item.subject=subject;item.resource=r;
            item.groupKey=releaseGroupKey(r.releaseGroup);item.season=release.season.value_or(LocalLibrary::subjectSeason(subject));
            item.episodes=episodeNumbers(release.episode);item.hash=hash;item.savePath=task.savePath;item.phase=phaseOf(task);
            QString message;if(!m_library->put(item,&message))emit error(message);
            break;
        }
    }
    synchronize();
}

void AnimeDownloads::adoptCachedSubjects(const QVector<AnimeSubject> &subjects)
{
    for(const auto &task:m_engine->listTorrents()) {
        bool known=false;for(const auto &i:m_library->items())if(i.hash==task.infoHash){known=true;break;}
        if(known)continue;
        const auto tags=parseTitle(task.title);if(tags.releaseGroup.isEmpty())continue;
        const auto season=parseSeason(task.title);QVector<AnimeSubject> matches;
        for(const auto &subject:subjects) {
            if(season&&*season!=LocalLibrary::subjectSeason(subject))continue;
            QStringList keys;for(const auto &key:subjectKeys(subject))if(key.size()>=4)keys.append(key);
            if(!keys.isEmpty()&&isRelevant(task.title,keys))matches.append(subject);
        }
        // Ambiguous old tasks stay unassigned until a resource hash confirms
        // the title. Never infer a different anime or fansub from its folder.
        if(matches.size()!=1)continue;
        ResourceResult r;r.title=task.title;r.infoHash=task.infoHash;r.tags=tags;r.releaseGroup=tags.releaseGroup;
        r.magnetUri="magnet:?xt=urn:btih:"+task.infoHash;r.episode=parseEpisode(task.title).label;
        adoptExisting(matches.first(),{r});
    }
}

bool AnimeDownloads::reserve(const AnimeSubject &subject,const PlannedDownload &plan)
{
    const auto &r=plan.resource;
    const QString key=m_library->itemKey(subject.bangumiId,r);
    if(m_library->downloaded(subject.bangumiId,r)||m_library->busy(subject.bangumiId,r))return false;
    const QString hash=hashOf(r);
    if(!hash.isEmpty())for(const auto &task:m_engine->listTorrents())if(task.infoHash.compare(hash,Qt::CaseInsensitive)==0) {
        adoptExisting(subject,{r});
        auto existing=m_library->item(key);if(!existing)return false;
        existing->episodes=plan.episodes;existing->onlyEpisodes=plan.onlyEpisodes;
        existing->phase="queued";existing->complete=false;
        QString message;
        if(!m_library->put(*existing,&message)){emit error(message);return false;}
        if(!m_engine->selectEpisodes(hash,plan.onlyEpisodes,&message)) {
            existing->phase="error";m_library->put(*existing);emit error(message);return false;
        }
        return true;
    }
    LibraryItem item=m_library->item(key).value_or(LibraryItem{});
    item.key=key;item.subject=subject;item.resource=r;item.hash.clear();item.phase="queued";item.complete=false;
    item.attemptedSources.clear();item.remoteId.clear();m_payloadReceived.remove(key);
    item.downloader=m_config->downloader.selected;item.externalProgram=m_config->downloader.programs.value(item.downloader);
    item.externalArguments=m_config->downloader.customArguments;
    item.resource.animeId=QString::number(subject.bangumiId);
    item.groupKey=releaseGroupKey(r.releaseGroup);item.season=plan.season;item.episodes=plan.episodes;
    item.onlyEpisodes=plan.onlyEpisodes;
    item.savePath=m_library->downloadDirectory(m_config->engine.savePath,subject,r.releaseGroup,plan.season);
    QString message;
    if(!QDir().mkpath(item.savePath)){emit error(QStringLiteral("无法创建番剧目录：%1").arg(item.savePath));return false;}
    if(!m_library->put(item,&message)){emit error(QStringLiteral("无法记录下载任务：%1").arg(message));return false;}
    m_pending.enqueue(key);return true;
}

void AnimeDownloads::downloadOne(const AnimeSubject &subject,const ResourceResult &resource)
{
    if(m_config->downloader.selected=="builtin"&&!m_engine->running()){emit error(QStringLiteral("下载引擎未启动。"));return;}
    QString setupError;if(!ExternalDownloader::validate(m_config->downloader,&setupError)){emit error(setupError);return;}
    adoptExisting(subject,{resource});
    if(m_library->downloaded(subject.bangumiId,resource)){emit information(QStringLiteral("该资源已经下载，可直接播放。"));return;}
    if(m_library->busy(subject.bangumiId,resource)) {
        const auto item=m_library->item(m_library->itemKey(subject.bangumiId,resource));
        if(item&&item->downloader=="builtin"&&!item->hash.isEmpty())m_engine->resume(item->hash);
        emit queued();return;
    }
    const auto release=annotate({resource}).value(0);
    PlannedDownload plan{resource,release.season.value_or(LocalLibrary::subjectSeason(subject)),episodeNumbers(release.episode),{}};
    if(release.isBatch&&plan.episodes.isEmpty()&&m_library->groupHasHistory(subject.bangumiId,resource.releaseGroup)) {
        QString reason;const auto plans=m_library->plan(subject,{resource},&reason);
        if(plans.isEmpty()){emit information(reason.isEmpty()?QStringLiteral("当前没有可确认的缺集，已跳过重复合集。"):reason);return;}
        plan=plans.first();
    }
    for(const auto &i:m_library->items())if(i.subject.bangumiId==subject.bangumiId
        &&i.groupKey==releaseGroupKey(resource.releaseGroup)&&i.season==plan.season&&i.episodes.isEmpty()
        &&(i.phase=="queued"||i.phase=="downloading"||i.phase=="paused"||!m_library->playableFiles(i).isEmpty())) {
        emit information(QStringLiteral("已有合集的集数尚未确认，请等待元数据读取后更新，以免重复下载。"));return;
    }
    const auto covered=m_library->claimedEpisodes(subject.bangumiId,resource.releaseGroup,plan.season);
    QStringList missing;for(const auto &ep:plan.episodes)if(!covered.contains(ep))missing.append(ep);
    if(!plan.episodes.isEmpty()&&missing.isEmpty()) {
        emit information(QStringLiteral("同一字幕组的这些话数已下载或正在下载，已跳过重复任务。"));return;
    }
    if(missing.size()!=plan.episodes.size()){plan.onlyEpisodes=missing;plan.episodes=missing;}
    if(reserve(subject,plan)){emit queued();processNext();}
}

void AnimeDownloads::downloadGroup(const AnimeSubject &subject,const QVector<ResourceResult> &resources,const EpisodeRange &range)
{
    if(m_config->downloader.selected=="builtin"&&!m_engine->running()){emit error(QStringLiteral("下载引擎未启动。"));return;}
    QString setupError;if(!ExternalDownloader::validate(m_config->downloader,&setupError)){emit error(setupError);return;}
    if(resources.isEmpty()){emit information(QStringLiteral("暂时没有可更新的资源，请等待检索完成后重试。"));return;}
    const QString group=releaseGroupKey(resources.first().releaseGroup);
    if(group.isEmpty()||resources.first().releaseGroup==kUnknownGroup){emit error(QStringLiteral("无法确认字幕组，不能自动批量下载。"));return;}
    for(const auto &r:resources)if(releaseGroupKey(r.releaseGroup)!=group) {
        emit error(QStringLiteral("资源来自不同字幕组，已取消批量任务。"));return;
    }
    adoptExisting(subject,resources);
    for(const auto &i:m_library->items())if(i.subject.bangumiId==subject.bangumiId&&i.groupKey==group&&i.downloader=="builtin"&&i.phase=="paused")
        m_engine->resume(i.hash);
    QString reason;const auto plans=m_library->plan(subject,resources,&reason,m_config->downloader.selected!="builtin"&&m_config->downloader.selected!="aria2"&&!m_rpc->enabled(m_config->downloader.selected),range);
    int added=0;for(const auto &plan:plans)if(reserve(subject,plan))++added;
    if(added){emit queued();processNext();}
    else if(!reason.isEmpty())emit information(reason);
    else if(range.active())emit information(QStringLiteral("所选集数范围内没有需要下载的集数：已下载、已加入任务，或当前检索结果中没有对应资源。"));
    else emit information(QStringLiteral("该字幕组当前可识别的集数已下载或已加入任务，无需重复下载。"));
}

void AnimeDownloads::scheduleSynchronize()
{
    if(!m_syncTimer->isActive())m_syncTimer->start();
}

void AnimeDownloads::processNext()
{
    if(m_dispatching)return;
    m_dispatching=true;
    while(!m_pending.isEmpty()) {
        const QString key=m_pending.dequeue();const auto found=m_library->item(key);
        if(!found||m_activeKeys.contains(key))continue;
        m_activeKeys.insert(key);
        startCandidate(key,downloadCandidates(found->resource));
    }
    m_dispatching=false;
    if(m_activeKeys.isEmpty()&&!m_errors.isEmpty()) {
        const QString message=m_errors.mid(0,5).join("\n");m_errors.clear();emit error(message);
    }
}

void AnimeDownloads::startCandidate(const QString &key,const QVector<ResourceResult> &candidates,int index)
{
    auto found=m_library->item(key);
    if(!found){m_activeKeys.remove(key);return;}
    if(index>=candidates.size()) {
        found->phase="error";QString message;m_library->put(*found,&message);
        m_errors.append(found->resource.title+QStringLiteral(": 没有可用的下载来源。"));
        m_activeKeys.remove(key);QTimer::singleShot(0,this,&AnimeDownloads::processNext);return;
    }
    auto pending=*found;pending.resource=candidates.at(index);
    const QString attempted=pending.resource.torrentUrl.isEmpty()?pending.resource.magnetUri:pending.resource.torrentUrl;
    if(!found->attemptedSources.contains(attempted))found->attemptedSources.append(attempted);
    QString persistenceError;if(!m_library->put(*found,&persistenceError)){m_activeKeys.remove(key);emit error(persistenceError);return;}
    pending.resource.sources=found->resource.sources;pending.resource.seenOn=found->resource.seenOn;
    const bool external=pending.downloader!="builtin";
    if(external) {
        auto record=*found;record.phase="external_pending";QString message;
        if(!m_library->put(record,&message)){m_errors.append(message);m_activeKeys.remove(key);return;}
    }
    QPointer<AnimeDownloads> self(this);
    auto complete=[self,key,candidates,index,resource=pending.resource,external](bool ok,QString message,ExternalResult result) {
        if(!self)return;
        auto item=self->m_library->item(key);
        if(!item){self->m_activeKeys.remove(key);return;}
        // Retry acquisition failures only, before a task has been accepted.
        // Never launch several alternatives to an already running transfer.
        if(!ok&&result.retryable&&index+1<candidates.size()) {
            QTimer::singleShot(0,self,[self,key,candidates,index]{if(self)self->startCandidate(key,candidates,index+1);});return;
        }
        if(ok)item->resource=resource;
        item->hash=result.hash;item->phase=ok?(external?"external":"downloading"):"error";
        if(external) {
            item->torrentFile=result.torrentFile;item->files=result.files;
            item->remoteId=result.remoteId;
            if(!result.episodes.isEmpty())item->episodes=result.episodes;
        }
        QString failure;if(!self->m_library->put(*item,&failure))self->m_errors.append(failure);
        if(!ok)self->m_errors.append(item->resource.title+": "+message);
        self->m_activeKeys.remove(key);self->scheduleSynchronize();
        QTimer::singleShot(0,self,&AnimeDownloads::processNext);
    };
    if(external&&m_rpc->enabled(pending.downloader))m_rpc->add(pending,m_config->engine.stopOnCompletion,std::move(complete));
    else if(external)m_external->send(pending,m_config->engine.stopOnCompletion,std::move(complete));
    else {
        const auto &resource=candidates.at(index);
        const QString uri=resource.torrentUrl.isEmpty()?resource.magnetUri:resource.torrentUrl;
        m_engine->addTorrentTracked(uri,pending.savePath,[complete=std::move(complete)](bool ok,QString message,QString hash) {
            ExternalResult result;result.hash=hash;result.retryable=!ok&&hash.isEmpty();
            complete(ok,message,result);
        },pending.onlyEpisodes,trackersFor(pending.resource),hashOf(resource));
    }
}

void AnimeDownloads::restorePending()
{
    const auto tasks=m_engine->listTorrents();
    for(auto item:m_library->items()) {
        if(m_activeKeys.contains(item.key)||m_pending.contains(item.key))continue;
        if(item.downloader!="builtin") {
            if(item.phase=="queued"||item.phase=="external_pending"){item.phase="error";QString message;if(!m_library->put(item,&message))emit error(message);}
            continue;
        }
        if(item.phase!="queued"&&item.phase!="downloading"&&item.phase!="paused")continue;
        bool present=false;for(const auto &task:tasks)if(task.infoHash==item.hash||(!hashOf(item.resource).isEmpty()&&task.infoHash==hashOf(item.resource))) {
            item.hash=task.infoHash;present=true;break;
        }
        if(present){QString message;if(!m_library->put(item,&message))emit error(message);continue;}
        if(item.complete)continue;
        // Paused records without an engine handle are not restarted implicitly.
        if(item.phase=="paused")continue;
        item.phase="queued";QString message;
        if(m_library->put(item,&message))m_pending.enqueue(item.key);else emit error(message);
    }
    synchronize();processNext();
}

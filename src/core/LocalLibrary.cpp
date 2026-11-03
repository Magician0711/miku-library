#include "LocalLibrary.h"
#include "TitleParser.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>

namespace {
QJsonArray strings(const QStringList &list){QJsonArray a;for(const auto &s:list)a.append(s);return a;}
QStringList strings(const QJsonValue &value){QStringList a;for(const auto &s:value.toArray())a.append(s.toString());return a;}
bool fileReady(const LibraryFile &file)
{const QFileInfo info(file.path);return file.complete&&file.size>0&&info.isFile()&&info.size()==file.size;}
QString fingerprint(const ResourceResult &r)
{return r.infoHash.isEmpty()?btihFromMagnet(r.magnetUri).value_or(r.dedupKey()):r.infoHash.toLower();}
bool active(const QString &phase){return phase=="queued"||phase=="downloading"||phase=="paused"||phase=="external"||phase=="external_paused"||phase=="external_pending";}
QStringList sortedNumbers(const QSet<QString> &set)
{QStringList list(set.begin(),set.end());std::sort(list.begin(),list.end(),[](const QString &a,const QString &b){return a.toDouble()<b.toDouble();});return list;}
int quality(const ResourceResult &r)
{return r.tags.resolution.left(r.tags.resolution.size()-1).toInt();}
QStringList recordedEpisodes(const LibraryItem &item)
{
    // Do not revive an old filename guess (for example E528 -> 528) when
    // current parsing can no longer confirm it. Actual filenames take priority.
    QSet<QString> files;
    for(const auto &file:item.files)for(const auto &ep:fileEpisodeNumbers(file.path))files.insert(ep);
    if(!files.isEmpty())return sortedNumbers(files);
    const auto parsed=parseEpisode(item.resource.title);
    // A collection's title can list all episodes while its reservation only
    // selects missing files. Do not expand that reservation to the full range.
    if(parsed.batch)return item.episodes;
    const auto titleEpisodes=episodeNumbers(parsed.label);
    if(!titleEpisodes.isEmpty())return titleEpisodes;
    return episodeNumbers(item.resource.episode);
}
}

LocalLibrary::LocalLibrary(const QString &path,QObject *parent):QObject(parent),m_path(path){}

bool LocalLibrary::load(QString *error)
{
    m_ready=false;
    m_items.clear();
    if(!QFile::exists(m_path)){m_ready=save(error);return m_ready;}
    QFile file(m_path);
    if(!file.open(QIODevice::ReadOnly)){if(error)*error=file.errorString();return false;}
    QJsonParseError parseError;
    const auto document=QJsonDocument::fromJson(file.readAll(),&parseError);
    if(parseError.error!=QJsonParseError::NoError||!document.isObject()
        ||document.object().value("schema_version").toInt()!=1){if(error)*error=QStringLiteral("本地番剧目录格式无效，原文件已保留。");return false;}
    for(const auto &value:document.object().value("downloads").toArray()) {
        const auto o=value.toObject(),s=o.value("subject").toObject(),r=o.value("resource").toObject();
        LibraryItem item;item.key=o.value("key").toString();item.subject.bangumiId=s.value("id").toInt();
        item.subject.nameCn=s.value("name").toString();item.subject.nameJp=s.value("name_jp").toString();
        item.subject.nameEn=s.value("name_en").toString();item.subject.summary=s.value("summary").toString();
        item.subject.coverUrl=s.value("cover").toString();item.subject.bangumiUrl=s.value("url").toString();
        item.subject.episodeCount=s.value("episode_count").toInt();item.subject.year=s.value("year").toInt();
        item.subject.rating=s.value("rating").toDouble();item.subject.aliases=strings(s.value("aliases"));
        item.resource.title=r.value("title").toString();item.resource.releaseGroup=r.value("group").toString();
        item.resource.siteId=r.value("site_id").toString();item.resource.siteName=r.value("site_name").toString();
        item.resource.magnetUri=r.value("magnet").toString();item.resource.torrentUrl=r.value("torrent").toString();
        item.resource.infoHash=r.value("info_hash").toString();item.resource.episode=r.value("episode").toString();
        item.resource.sourcePage=r.value("source_page").toString();item.resource.tags=parseTitle(item.resource.title);
        item.resource.seenOn=strings(r.value("seen_on"));
        for(const auto &source:r.value("sources").toArray())item.resource.sources.append(ResourceSource::fromJson(source.toObject()));
        item.resource.animeId=QString::number(item.subject.bangumiId);
        item.groupKey=o.value("group_key").toString();item.season=o.value("season").toInt(1);
        item.episodes=strings(o.value("episodes"));item.onlyEpisodes=strings(o.value("only_episodes"));
        item.hash=o.value("hash").toString();item.savePath=o.value("save_path").toString();
        item.phase=o.value("phase").toString();item.complete=o.value("complete").toBool();
        item.downloader=o.value("downloader").toString("builtin");item.externalProgram=o.value("external_program").toString();
        item.externalArguments=o.value("external_arguments").toString();item.torrentFile=o.value("torrent_file").toString();
        item.remoteId=o.value("remote_id").toString();
        item.attemptedSources=strings(o.value("attempted_sources"));
        for(const auto &f:o.value("files").toArray()) {
            const auto media=f.toObject();item.files.append({media.value("path").toString(),
                qint64(media.value("size").toDouble()),media.value("complete").toBool()});
        }
        item.episodes=recordedEpisodes(item);
        if(!item.key.isEmpty()&&item.subject.bangumiId>0)m_items.append(item);
    }
    m_ready=true;return true;
}

bool LocalLibrary::save(QString *error) const
{
    QJsonArray entries;
    for(const auto &i:m_items) {
        const auto &s=i.subject;const auto &r=i.resource;
        QJsonObject subject{{"id",s.bangumiId},{"name",s.displayName()},{"name_jp",s.nameJp},{"name_en",s.nameEn},
            {"summary",s.summary},{"cover",s.coverUrl},{"url",s.bangumiUrl},{"year",s.year},{"rating",s.rating},
            {"episode_count",s.episodeCount},{"aliases",strings(s.aliases)}};
        QJsonObject resource{{"title",r.title},{"group",r.releaseGroup},{"site_id",r.siteId},{"site_name",r.siteName},
            {"magnet",r.magnetUri},{"torrent",r.torrentUrl},{"info_hash",r.infoHash},{"episode",r.episode},{"source_page",r.sourcePage}};
        QJsonArray sources;for(const auto &source:r.sources)sources.append(source.toJson());
        resource.insert("sources",sources);resource.insert("seen_on",strings(r.seenOn));
        QJsonArray files;for(const auto &f:i.files)files.append(QJsonObject{{"path",f.path},{"size",double(f.size)},{"complete",f.complete}});
        entries.append(QJsonObject{{"key",i.key},{"subject",subject},{"resource",resource},{"group_key",i.groupKey},
            {"season",i.season},{"episodes",strings(i.episodes)},{"only_episodes",strings(i.onlyEpisodes)},
            {"hash",i.hash},{"save_path",i.savePath},{"phase",i.phase},{"complete",i.complete},{"files",files},
            {"downloader",i.downloader},{"external_program",i.externalProgram},{"external_arguments",i.externalArguments},{"torrent_file",i.torrentFile},{"remote_id",i.remoteId},{"attempted_sources",strings(i.attemptedSources)}});
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);const auto data=QJsonDocument(QJsonObject{{"schema_version",1},{"downloads",entries}}).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit()) {
        if(error)*error=file.errorString();return false;
    }
    return true;
}

QString LocalLibrary::itemKey(int subjectId,const ResourceResult &r) const
{
    // A merged row may choose a different site's hash. Keep the reservation
    // stable across that selection and across later search arrival order.
    auto row=r;row.animeId.clear();
    for(const auto &item:m_items)if(item.subject.bangumiId==subjectId) {
        auto existing=item.resource;existing.animeId.clear();
        if(existing.rowKey()==row.rowKey())return item.key;
    }
    const QString raw=QString::number(subjectId)+"|"+releaseGroupKey(r.releaseGroup)+"|"+fingerprint(r);
    return QString::fromLatin1(QCryptographicHash::hash(raw.toUtf8(),QCryptographicHash::Sha256).toHex());
}

std::optional<LibraryItem> LocalLibrary::item(const QString &key) const
{for(const auto &i:m_items)if(i.key==key)return i;return std::nullopt;}

QStringList LocalLibrary::playableFiles(const LibraryItem &item) const
{QStringList out;for(const auto &f:item.files)if(fileReady(f)&&isVideoFile(f.path))out.append(f.path);return out;}
QStringList LocalLibrary::playableFiles(int id,const ResourceResult &resource) const
{
    if(!resource.localPath.isEmpty()) {
        for(const auto &i:m_items)if(i.subject.bangumiId==id&&i.groupKey==releaseGroupKey(resource.releaseGroup))
            for(const auto &f:i.files)if(f.path==resource.localPath&&fileReady(f)&&isVideoFile(f.path))return {f.path};
        return {};
    }
    const auto found=item(itemKey(id,resource));
    if(!found)return {};
    const auto expected=episodeNumbers(parseEpisode(resource.title).label);
    if(expected.size()<2)return playableFiles(*found);
    QStringList paths;
    for(const auto &i:m_items)if(i.subject.bangumiId==id&&i.groupKey==found->groupKey&&i.season==found->season)
        for(const auto &path:playableFiles(i)) {
            auto episodes=fileEpisodeNumbers(path);
            if(episodes.isEmpty()&&i.files.size()==1)episodes=i.episodes;
            bool relevant=false;for(const auto &ep:episodes)if(expected.contains(ep))relevant=true;
            if(relevant&&!paths.contains(path))paths.append(path);
        }
    return paths;

}
bool LocalLibrary::downloaded(int id,const ResourceResult &r) const
{
    if(!r.localPath.isEmpty())return !playableFiles(id,r).isEmpty();
    const auto found=item(itemKey(id,r));
    if(!found||!found->complete||found->files.isEmpty())return false;
    for(const auto &f:found->files)if(!fileReady(f))return false;
    const auto expected=episodeNumbers(parseEpisode(r.title).label);
    if(!expected.isEmpty()) {
        QSet<QString> present;
        for(const auto &i:m_items)if(i.subject.bangumiId==id&&i.groupKey==found->groupKey&&i.season==found->season)
            present.unite(availableEpisodes(i));
        for(const auto &ep:expected)if(!present.contains(ep))return false;
    }
    return !playableFiles(*found).isEmpty();
}
bool LocalLibrary::busy(int id,const ResourceResult &r) const
{const auto found=item(itemKey(id,r));return found&&active(found->phase)&&!downloaded(id,r);}
bool LocalLibrary::external(int id,const ResourceResult &r) const
{const auto found=item(itemKey(id,r));return found&&found->downloader!="builtin"&&(found->phase=="external"||found->phase=="external_paused"||found->phase=="external_pending");}

bool LocalLibrary::groupHasHistory(int id,const QString &group) const
{
    const auto key=releaseGroupKey(group);
    for(const auto &i:m_items)if(i.subject.bangumiId==id&&i.groupKey==key) {
        if(active(i.phase)||!playableFiles(i).isEmpty())return true;
        for(const auto &f:i.files)if(QFileInfo(f.path).isFile()&&QFileInfo(f.path).size()>0)return true;
    }
    return false;
}

QSet<QString> LocalLibrary::availableEpisodes(const LibraryItem &item) const
{
    bool all=item.complete&&!item.files.isEmpty();
    QSet<QString> available;
    for(const auto &f:item.files) {
        if(!fileReady(f)){all=false;continue;}
        for(const auto &ep:fileEpisodeNumbers(f.path))available.insert(ep);
    }
    if(all||(item.files.size()==1&&!playableFiles(item).isEmpty()))
        for(const auto &ep:recordedEpisodes(item))available.insert(ep);
    return available;
}

QSet<QString> LocalLibrary::claimedEpisodes(int id,const QString &group,int season) const
{
    QSet<QString> out;const auto key=releaseGroupKey(group);
    for(const auto &i:m_items)if(i.subject.bangumiId==id&&i.groupKey==key&&i.season==season) {
        out.unite(availableEpisodes(i));
        if(active(i.phase))for(const auto &ep:recordedEpisodes(i))out.insert(ep);
    }
    return out;
}

QVector<AnimeSubject> LocalLibrary::subjects() const
{
    QVector<AnimeSubject> out;QSet<int> ids;
    for(const auto &i:m_items)if(!ids.contains(i.subject.bangumiId)&&!playableFiles(i).isEmpty()) {
        out.append(i.subject);ids.insert(i.subject.bangumiId);
    }
    return out;
}

QVector<Release> LocalLibrary::localReleases(int id) const
{
    QVector<Release> out;QSet<QString> paths;
    for(const auto &i:m_items)if(i.subject.bangumiId==id)for(const auto &path:playableFiles(i)) {
        if(paths.contains(path))continue;paths.insert(path);
        Release release;release.result=i.resource;release.result.title=QFileInfo(path).fileName();
        release.result.localPath=path;release.result.fileSize=QFileInfo(path).size();
        release.result.tags=parseTitle(release.result.title);
        if(release.result.tags.resolution.isEmpty())release.result.tags=i.resource.tags;
        release.season=i.season;
        release.episode=parseEpisode(release.result.title).label;
        if(release.episode.isEmpty())release.episode=fileEpisodeNumbers(path).join("-");
        const auto fallback=recordedEpisodes(i);
        if(release.episode.isEmpty()&&fallback.size()==1)release.episode=fallback.first();
        out.append(release);
    }
    std::sort(out.begin(),out.end(),[](const Release &a,const Release &b){return a.sortEpisode()<b.sortEpisode();});
    return out;
}

int LocalLibrary::subjectSeason(const AnimeSubject &s)
{
    for(const auto &name:{s.nameCn,s.nameJp,s.nameEn})if(auto season=parseSeason(name))return *season;
    return 1;
}

QVector<PlannedDownload> LocalLibrary::plan(const AnimeSubject &subject,const QVector<ResourceResult> &resources,QString *reason,bool preferSingles) const
{
    QVector<Release> releases=annotate(resources);
    std::sort(releases.begin(),releases.end(),[preferSingles](const Release &a,const Release &b) {
        if(preferSingles&&a.isBatch!=b.isBatch)return !a.isBatch;
        const int countA=episodeNumbers(a.episode).size(),countB=episodeNumbers(b.episode).size();
        if(countA!=countB)return countA>countB;
        if(quality(a.result)!=quality(b.result))return quality(a.result)>quality(b.result);
        if(a.result.publishTime!=b.result.publishTime)return a.result.publishTime>b.result.publishTime;
        return a.result.dedupKey()<b.result.dedupKey();
    });
    QHash<QString,QSet<QString>> claimed;
    QSet<QString> identities;QVector<PlannedDownload> out;
    static const QRegularExpression extra(QStringLiteral("(?:^|[\\s\\[\\]_.-])(?:NCOP|NCED|OP|ED|PV|CM|SP|OVA|OAD)(?:[\\s\\[\\]_.\\d-]|$)"),QRegularExpression::CaseInsensitiveOption);
    for(const auto &release:releases) {
        auto resource=release.result;
        if(resource.releaseGroup.isEmpty()||resource.releaseGroup==kUnknownGroup)continue;
        if(resource.magnetUri.isEmpty()&&resource.torrentUrl.isEmpty())continue;
        if(extra.match(resource.title).hasMatch())continue;
        const int season=release.season.value_or(subjectSeason(subject));
        const QString group=releaseGroupKey(resource.releaseGroup);
        const QString scope=group+"|"+QString::number(season);
        if(!claimed.contains(scope))claimed.insert(scope,claimedEpisodes(subject.bangumiId,resource.releaseGroup,season));
        auto &covered=claimed[scope];
        const QString key=itemKey(subject.bangumiId,resource);
        if(identities.contains(key)||downloaded(subject.bangumiId,resource)||busy(subject.bangumiId,resource))continue;
        identities.insert(key);
        bool unknownExisting=false;
        for(const auto &i:m_items)if(i.subject.bangumiId==subject.bangumiId&&i.groupKey==group
            &&i.season==season&&i.episodes.isEmpty()&&(active(i.phase)||!playableFiles(i).isEmpty()))unknownExisting=true;
        if(unknownExisting){if(reason)*reason=QStringLiteral("已有合集尚未确认集数，请等待元数据读取完成后更新。");continue;}
        QStringList episodes=episodeNumbers(release.episode),only;
        if(episodes.isEmpty()&&!release.isBatch)continue;
        if(episodes.isEmpty()) {
            // A complete collection can be inspected by metadata. With prior
            // episodes, select only missing numbered files, never guess a group.
            if(!covered.isEmpty()||groupHasHistory(subject.bangumiId,resource.releaseGroup)) {
                bool unidentified=false;
                for(const auto &i:m_items)if(i.subject.bangumiId==subject.bangumiId&&i.groupKey==group
                    &&i.season==season&&i.episodes.isEmpty()&&(active(i.phase)||!playableFiles(i).isEmpty()))unidentified=true;
                if(unidentified){if(reason)*reason=QStringLiteral("已有合集的集数无法识别，已阻止重复下载。可播放现有视频或选择明确集数的资源。");continue;}
                const int last=subject.episodeCount>0?qMin(subject.episodeCount,1899):1899;
                for(int ep=1;ep<=last;++ep)if(!covered.contains(QString::number(ep)))only.append(QString::number(ep));
                if(only.isEmpty())continue;
                episodes=only;
            }
        } else {
            QStringList missing;for(const auto &ep:episodes)if(!covered.contains(ep))missing.append(ep);
            if(missing.isEmpty())continue;
            if(release.isBatch||missing.size()!=episodes.size())only=missing;
            episodes=missing;
        }
        if(release.isBatch&&only.isEmpty()) {
            const int last=subject.episodeCount>0?qMin(subject.episodeCount,1899):1899;
            for(int n=1;n<=last;++n)only.append(QString::number(n));
        }
        out.append({resource,season,episodes,only});
        for(const auto &ep:episodes)covered.insert(ep);
        if(release.isBatch&&episodes.isEmpty()) {
            // Wait for this collection's metadata before considering overlapping
            // versions whose coverage cannot yet be known.
            out.removeLast();out.prepend({resource,season,episodes,only});
            break;
        }
    }
    std::sort(out.begin(),out.end(),[](const PlannedDownload &a,const PlannedDownload &b) {
        if(a.season!=b.season)return a.season<b.season;
        return a.episodes.value(0).toDouble()<b.episodes.value(0).toDouble();
    });
    return out;
}

bool LocalLibrary::put(const LibraryItem &item,QString *error)
{
    if(!m_ready){if(error)*error=QStringLiteral("本地目录尚未成功读取，已停止写入以保护原有记录。");return false;}
    const auto before=m_items;bool replaced=false;
    for(auto &i:m_items)if(i.key==item.key){i=item;replaced=true;break;}
    if(!replaced)m_items.append(item);
    if(!save(error)){m_items=before;return false;}
    emit changed();return true;
}

void LocalLibrary::reconcile(const QString &hash,const QVector<DownloadFile> &files,bool completed,const QString &phase)
{
    if(files.isEmpty())return;
    bool changed=false;
    for(auto &i:m_items)if(i.hash==hash) {
        QVector<LibraryFile> media;QSet<QString> actualEpisodes;
        for(const auto &f:files)if(f.wanted&&isVideoFile(f.path)) {
            media.append({f.path,f.size,f.size>0&&f.downloaded>=f.size});
            for(const auto &ep:fileEpisodeNumbers(f.path))actualEpisodes.insert(ep);
        }
        for(const auto &old:i.files)if(fileReady(old)) {
            bool present=false;for(const auto &f:media)if(f.path==old.path){present=true;break;}
            if(!present)media.append(old);
        }
        bool same=media.size()==i.files.size();
        if(same)for(int n=0;n<media.size();++n)if(media[n].path!=i.files[n].path||media[n].size!=i.files[n].size||media[n].complete!=i.files[n].complete){same=false;break;}
        if(!media.isEmpty()&&!actualEpisodes.isEmpty()) {
            const auto numbers=sortedNumbers(actualEpisodes);
            if(numbers!=i.episodes){i.episodes=numbers;changed=true;}
        }
        if(!same||i.complete!=completed||i.phase!=phase){i.files=media;i.complete=completed;i.phase=phase;changed=true;}
    }
    if(changed){QString error;if(!save(&error))emit storageError(error);emit this->changed();}
}

void LocalLibrary::markRemoved(const QString &hash)
{
    bool dirty=false;for(auto &i:m_items)if(i.hash==hash){i.phase="cancelled";dirty=true;}
    if(dirty){QString error;if(!save(&error))emit storageError(error);emit changed();}
}

QString LocalLibrary::safeFolderName(const QString &value)
{
    QString name=value.normalized(QString::NormalizationForm_KC);
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x{0000}-\\x{001f}]")),"_");
    name=name.simplified();while(name.endsWith('.')||name.endsWith(' '))name.chop(1);
    if(name.isEmpty()||name=="."||name=="..")name=QStringLiteral("未命名");
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)",QRegularExpression::CaseInsensitiveOption);
    if(reserved.match(name).hasMatch())name.prepend('_');
    name=name.left(64);if(!name.isEmpty()&&name.back().isHighSurrogate())name.chop(1);return name;
}

QString LocalLibrary::downloadDirectory(const QString &root,const AnimeSubject &subject,const QString &group,int season) const
{
    QString name=safeFolderName(subject.displayName());
    const QString base=QDir(root).absoluteFilePath(name);
    for(const auto &i:m_items)if(i.subject.bangumiId!=subject.bangumiId&&i.savePath.startsWith(base+"/")) {
        name+=QStringLiteral(" [%1]").arg(subject.bangumiId);break;
    }
    QString groupName=safeFolderName(group.isEmpty()?kUnknownGroup:group);
    const QString groupRoot=QDir(QDir(root).absoluteFilePath(name)).filePath(groupName);
    for(const auto &i:m_items)if(i.subject.bangumiId==subject.bangumiId&&i.groupKey!=releaseGroupKey(group)&&i.savePath.startsWith(groupRoot+"/")) {
        groupName+="-"+QString::fromLatin1(QCryptographicHash::hash(group.toUtf8(),QCryptographicHash::Sha256).toHex().left(8));break;
    }
    return QDir(root).absoluteFilePath(name+"/"+groupName+QStringLiteral("/第%1季").arg(season));
}

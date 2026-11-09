// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "core/LocalLibrary.h"
#include "core/Backup.h"
#include "core/TitleParser.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QDebug>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QTemporaryDir dir;int failed=0,checks=0;
    QFile report(QCoreApplication::applicationDirPath()+"/local-library-smoke.log");report.open(QIODevice::WriteOnly);
    auto check=[&](bool ok,const char *name){++checks;report.write(QByteArray(name)+(ok?" PASS\n":" FAIL\n"));report.flush();qInfo()<<name<<(ok?"PASS":"FAIL");if(!ok)++failed;};
    const QString path=dir.filePath("local-library.json");LocalLibrary library(path);QString error;
    check(library.load(&error)&&QFile::exists(path),"creates local inventory");
    AnimeSubject subject;subject.bangumiId=123;subject.nameCn=QStringLiteral("测试番剧");subject.episodeCount=12;
    auto resource=[](int ep,QString group="Group A",int season=1,int quality=1080){
        ResourceResult r;r.title=QString("[%1] Test S%2E%3 [%4p]").arg(group).arg(season,2,10,QChar('0')).arg(ep,2,10,QChar('0')).arg(quality);
        r.releaseGroup=group;r.episode=QString::number(ep);r.tags=parseTitle(r.title);
        r.magnetUri="magnet:?xt=urn:btih:"+QString::number(ep+season*100+quality+(group=="Group A"?0:10000),16).rightJustified(40,'0');return r;
    };
    auto first=resource(1);auto second=resource(2);auto low=resource(1,"Group A",1,720);
    check(fileEpisodeNumbers("01.mkv")==QStringList{"1"}&&fileEpisodeNumbers("S02E03.mp4")==QStringList{"3"},"numbered media file names are recognized");
    const QVector<QPair<QString,QString>> episodeCases={
        {"[Fansub] Test E528 [1080p]",""},
        {"[Fansub] Test [E528] [1080p]",""},
        {"[Fansub] Test [E528] [05] [1080p]","05"},
        {"[Fansub] Test E528 - 06 [1080p]","06"},
        {"[Fansub] Test [03] [E528]","03"},
        {"[Fansub] Test [A1E528] [1080p]",""},
        {"[Fansub] Test ID_E528Z [1080p]",""},
        {"[Fansub] Test ABCS01E528 [1080p]",""},
        {"[Fansub] Test S01E528 [1080p]","528"},
        {"[Fansub] Test EP528 [1080p]","528"},
        {QStringLiteral("测试 第528话 [E528]"),"528"},
        {"[Fansub] Test S01E720 [1080p]","720"},
        {"[Fansub] Test E12 [1080p]","12"},
        {"[Fansub] Test [12.5] [ABC123EF]","12.5"},
        {"[Fansub] Test [720] [05]","05"},
        {"[Fansub] Test [1080p] [HEVC 10bit] [AAC 2.0]",""},
        {"[Fansub] Test [01-12] [E528]","01-12"},
        {QStringLiteral("[组] 测试 Ｓ０１Ｅ０３"),"03"},
        // long-running series
        {"[Erai-raws] One Piece - 1089 [1080p][Multiple Subtitle]","1089"},
        {"[Lilith-Raws] ONE PIECE [1089][Baha][WEB-DL][1080p]","1089"},
        {QStringLiteral("【喵萌奶茶屋】海贼王 第1089话 [1080p]"),"1089"},
        {"[GM-Team][One Piece][1001-1050][GB][1080p]","1001-1050"},
        {"[Group] Detective Conan [0995-1005] [1080p]","0995-1005"},
        {"[Group] One Piece EP1001-EP1010 [1080p]","1001-1010"},
        {"[Group] One Piece S01E1100 [1080p]","1100"},
        {"[Group] Test [2019-2020] [07] [1080p]","07"},
        {"[Group] Test [1080] [1089]","1089"},
        {"[Group] Test [2024] [1080p]",""}
    };
    for(const auto &[title,label]:episodeCases)check(parseEpisode(title).label==label,qPrintable("episode recognition: "+title));
    check(episodeNumbers("1001-1050").size()==50&&episodeNumbers("0995-1005").value(0)=="995"&&fileEpisodeNumbers("One Piece - 1089.mkv")==QStringList{"1089"},"four-digit episodes and long collections expand to individual episodes");
    check(fileEpisodeNumbers("Test E528.mkv").isEmpty()&&fileEpisodeNumbers("Test [E528] - 07.mkv")==QStringList{"7"},"media identifiers do not become downloaded episode claims");
    {
        LocalLibrary legacy(dir.filePath("legacy.json"));legacy.load();
        LibraryItem old;old.subject=subject;old.resource=first;old.resource.title="[Group A] Test [E528]";old.resource.episode.clear();
        old.groupKey=releaseGroupKey(first.releaseGroup);old.key=legacy.itemKey(subject.bangumiId,old.resource);old.episodes={"528"};old.phase="complete";old.complete=true;
        QFile media(dir.filePath("Test [E528].mkv"));media.open(QIODevice::WriteOnly);media.write("video");media.close();old.files={{media.fileName(),5,true}};legacy.put(old);
        check(legacy.localReleases(subject.bangumiId).first().episode.isEmpty()&&!legacy.claimedEpisodes(subject.bangumiId,first.releaseGroup,1).contains("528"),"old incorrect episode cache cannot override corrected filename parsing");
        LocalLibrary reopened(legacy.path());reopened.load();
        check(reopened.items().first().episodes.isEmpty()&&reopened.playableFiles(reopened.items().first()).size()==1,"legacy episode claims are re-read without removing existing video or playback");
    }
    auto mirror=first;first.siteId="a";first.siteName="Site A";first.torrentUrl="https://a.invalid/seed";
    mirror.siteId="b";mirror.siteName="Site B";mirror.infoHash=QString(40,'f');mirror.magnetUri="magnet:?xt=urn:btih:"+mirror.infoHash;
    mirror.torrentUrl="https://b.invalid/seed";
    mergeResource(first,mirror);mergeResource(first,mirror);
    check(first.sources.size()==2&&first.seenOn==QStringList({"Site A","Site B"}),"same row retains unique source names and paired download endpoints");
    auto different=mirror;different.releaseGroup="Group B";
    check(first.rowKey()==mirror.rowKey()&&first.rowKey()!=different.rowKey()&&first.rowKey()!=low.rowKey(),"merge identity separates groups and quality versions");
    const auto candidates=downloadCandidates(first);
    check(candidates.size()==4&&candidates[0].torrentUrl==first.torrentUrl&&candidates[1].infoHash==mirror.infoHash
        &&candidates[1].magnetUri==mirror.magnetUri,"automatic source candidates preserve each source hash and prefer torrent endpoints");
    auto plans=library.plan(subject,{low,first,second});
    check(plans.size()==2&&plans[0].resource.tags.resolution=="1080p","one quality per episode, episode order");
    auto freshBatch=first;freshBatch.title="[Group A] Test [01-02] Complete";
    check(library.plan(subject,{freshBatch}).value(0).onlyEpisodes==QStringList({"1","2"}),"fresh collection also selects one video per episode");
    check(library.plan(subject,{freshBatch},nullptr,false,{2,2}).value(0).onlyEpisodes==QStringList({"2"}),"episode window limits a collection to files inside it");
    {
        const auto windowed=library.plan(subject,{resource(1),resource(2)},nullptr,false,{2,0});
        check(windowed.size()==1&&windowed.value(0).episodes==QStringList({"2"}),"episode window skips single episodes outside it");
    }
    {
        LocalLibrary selected(dir.filePath("selected.json"));selected.load();LibraryItem pending;
        pending.subject=subject;pending.resource=freshBatch;pending.groupKey=releaseGroupKey(freshBatch.releaseGroup);
        pending.key=selected.itemKey(subject.bangumiId,freshBatch);pending.episodes={"2"};pending.onlyEpisodes={"2"};pending.phase="queued";selected.put(pending);
        LocalLibrary reopened(selected.path());reopened.load();
        check(reopened.claimedEpisodes(subject.bangumiId,freshBatch.releaseGroup,1)==QSet<QString>{"2"},"re-read collection reserves selected episodes only rather than its entire title range");
    }
    LibraryItem i;i.subject=subject;i.resource=first;i.groupKey=releaseGroupKey(first.releaseGroup);i.key=library.itemKey(subject.bangumiId,first);
    i.episodes={"1"};i.hash="first";i.savePath=library.downloadDirectory(dir.path(),subject,first.releaseGroup,1);
    check(i.savePath.contains(subject.nameCn)&&i.savePath.contains("Group A"),"automatic anime and group folder");
    check(library.put(i,&error),"reserve before network");
    check(library.plan(subject,{first,second,low}).size()==1,"pending task blocks mirror and quality duplicates");
    check(library.groupHasHistory(123,"Group A"),"pending cache changes group action to update");
    check(library.plan(subject,{resource(1,"Group B"),resource(1,"Group A",2)}).size()==2,"groups and seasons isolated");
    check(library.plan(subject,{first,second}).size()==1,"repeated group action only adds missing episodes");
    i.phase="complete";i.complete=true;QDir().mkpath(i.savePath);
    const QString video=QDir(i.savePath).filePath("Test S01E01.mkv");QFile file(video);file.open(QIODevice::WriteOnly);file.write("video");file.close();
    i.files={{video,5,true}};library.put(i);
    check(library.downloaded(123,first)&&library.localReleases(123).size()==1,"verified completed video is playable");
    check(library.subjects().size()==1,"local anime appears in collection");
    LocalLibrary reload(path);check(reload.load(&error)&&reload.downloaded(123,first),"inventory survives restart");
    check(reload.items().first().resource.sources.size()==2&&reload.items().first().resource.seenOn.size()==2,"alternate sources survive inventory reload");
    check(reload.itemKey(123,mirror)==i.key&&reload.downloaded(123,mirror),"source hash and arrival order changes reuse existing downloaded row");
    ResourceResult batch=first;batch.title="[Group A] Test [01-03] [1080p]";batch.episode="01-03";batch.magnetUri="magnet:?xt=urn:btih:"+QString(40,'f');
    plans=library.plan(subject,{batch,second});
    check(plans.size()==1&&plans[0].onlyEpisodes==QStringList({"2","3"}),"overlap collection selects missing episodes only");
    LibraryItem partial=i;partial.resource=batch;partial.key=library.itemKey(123,batch);partial.hash="partial";
    partial.episodes={"2","3"};partial.files.clear();partial.phase="cancelled";partial.complete=false;
    library.put(partial);
    check(library.playableFiles(123,batch)==QStringList{video},"collection playback finds matching cached episodes across releases");
    auto other=subject;other.bangumiId=124;
    check(library.plan(other,{first}).size()==1,"anime identity is isolated");
    auto local=library.localReleases(123).first().result;
    check(library.playableFiles(123,local)==QStringList{video},"local playback resolves actual video");
    file.setFileName(video);file.open(QIODevice::WriteOnly|QIODevice::Append);file.write("x");file.close();
    check(!library.downloaded(123,local)&&library.localReleases(123).isEmpty(),"changed size is not a completed video");
    QFile::remove(video);
    check(!library.downloaded(123,first)&&library.plan(subject,{first}).size()==1,"deleted file can be downloaded again");
    i.phase="downloading";i.episodes.clear();i.resource.title="[Group A] Test Complete Batch";i.key=library.itemKey(123,i.resource);i.files.clear();library.put(i);
    check(library.plan(subject,{second}).isEmpty(),"unknown active batch blocks overlapping singles");
    {
        // Download preferences and the startup update check.
        const QString followPath=dir.filePath("follow.json");
        LocalLibrary follow(followPath);follow.load();
        AnimeSubject show;show.bangumiId=500;show.nameCn=QStringLiteral("追番测试");show.nameEn="Test";show.episodeCount=24;
        AnimeSubject done=show;done.bangumiId=501;done.episodeCount=2;
        auto hold=[&](const AnimeSubject &s,int ep){
            LibraryItem item;item.subject=s;item.resource=resource(ep);item.resource.magnetUri+=QString::number(s.bangumiId);
            item.groupKey=releaseGroupKey("Group A");item.season=1;item.key=follow.itemKey(s.bangumiId,item.resource);
            item.episodes={QString::number(ep)};item.phase="downloading";follow.put(item);
        };
        hold(done,1);hold(done,2);hold(show,1);hold(show,2);
        auto pref=follow.preference(500);
        check(pref.group=="Group A"&&pref.resolution=="1080p","preference is derived from the latest download");
        const auto targets=follow.updateTargets();
        check(targets.size()==1&&targets[0].subject.bangumiId==500&&targets[0].after==2&&targets[0].season==1,
              "update check follows the newest claimed episode and skips completed anime");
        const auto found=follow.newEpisodes(targets.value(0),{resource(1),resource(3),resource(4),resource(3,"Group B"),resource(4,"Group A",1,720)});
        QStringList episodes;for(const auto &plan:found)episodes+=plan.episodes;
        check(episodes==QStringList({"3","4"}),"new episodes come only from the followed group and quality");
        pref.ignoredThrough=3;check(follow.setPreference(500,pref),"preference is saved");
        LocalLibrary reopened(followPath);reopened.load();
        check(reopened.preference(500).ignoredThrough==3&&reopened.updateTargets().value(0).after==3,
              "dismissed episodes persist and move the update window");
        check(reopened.item(reopened.itemKey(500,resource(2)))&&!reopened.item("missing"),"indexed lookups survive reload");
        // Watched and deleted: a finished task without files is still followed.
        auto watched=*reopened.item(reopened.itemKey(500,resource(2)));watched.phase="complete";watched.complete=true;watched.files.clear();
        reopened.put(watched);auto reset=reopened.preference(500);reset.ignoredThrough=0;reopened.setPreference(500,reset);
        check(reopened.updateTargets().value(0).after==2,"deleted episodes still mark how far the user has followed");
        hold(show,5);auto cancelled=*follow.item(follow.itemKey(500,resource(5)));cancelled.phase="cancelled";follow.put(cancelled);
        check(follow.updateTargets().value(0).after==3,"cancelled downloads do not advance the update window");
        auto quiet=follow.preference(500);quiet.follow=false;follow.setPreference(500,quiet);
        LocalLibrary muted(followPath);muted.load();
        check(!muted.preference(500).follow&&muted.updateTargets().isEmpty(),"anime with update notices turned off are skipped and stay off");
        auto automatic=muted.preference(500);automatic.follow=true;automatic.autoDownload=true;muted.setPreference(500,automatic);
        LocalLibrary reloaded(followPath);reloaded.load();
        check(reloaded.preference(500).autoDownload&&reloaded.preference(500).follow,"auto download choice is saved per anime");
    }
    {
        // Backup: settings, sites, records and follows survive a round trip; foreign files are refused.
        QTemporaryDir source,target;
        auto put=[](const QString &path,const QByteArray &data){QFile f(path);f.open(QIODevice::WriteOnly);f.write(data);};
        put(source.filePath("config.json"),R"({"appearance":"dark","close_action":"tray"})");
        put(source.filePath("anime_sites.json"),R"({"anime_torrent_sites":[]})");
        QFile::copy(dir.filePath("follow.json"),source.filePath("local-library.json"));
        const auto backup=Backup::create(source.path(),{11,22});
        const QString file=source.filePath("backup.json");
        check(Backup::save(backup,file),"backup is written");
        QJsonObject loaded;QString error;
        check(Backup::load(file,&loaded)&&Backup::summary(loaded).startsWith(QStringLiteral("2 部追番")),"backup reads back with a summary");
        QVector<int> followed;
        check(Backup::restore(loaded,target.path(),&followed)&&followed==QVector<int>({11,22}),"restore returns the followed anime");
        LocalLibrary restored(target.filePath("local-library.json"));
        check(restored.load()&&restored.preference(500).autoDownload&&!restored.items().isEmpty(),"restored records and preferences load");
        QFile config(target.filePath("config.json"));config.open(QIODevice::ReadOnly);
        check(QJsonDocument::fromJson(config.readAll()).object().value("close_action").toString()=="tray","restored settings keep their values");
        put(source.filePath("other.json"),R"({"downloads":[]})");
        check(!Backup::load(source.filePath("other.json"),nullptr,&error)&&!error.isEmpty(),"files that are not backups are refused");
        auto newer=backup;newer["version"]=Backup::kVersion+1;Backup::save(newer,file);
        check(!Backup::load(file,nullptr,&error),"backups from a newer version are refused");
    }
    check(LocalLibrary::safeFolderName("CON")=="_CON"&&LocalLibrary::safeFolderName("../A:B?*").contains("_")
        &&!LocalLibrary::safeFolderName("../A:B?*").contains('/'),"Windows folder names are safe");
    QFile corrupt(dir.filePath("broken.json"));corrupt.open(QIODevice::WriteOnly);corrupt.write("broken");corrupt.close();
    LocalLibrary broken(corrupt.fileName());check(!broken.load(&error)&&!broken.put(i,&error),"malformed inventory is preserved and blocks writes");
    qInfo()<<checks<<"checks,"<<failed<<"failures";return failed?1:0;
}

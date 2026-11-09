// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "HttpFixture.h"
#include "core/Catalog.h"
#include "core/Grouping.h"
#include "core/InstanceGuard.h"
#include "net/HttpClient.h"
#include "net/BangumiClient.h"
#include "net/FeedParser.h"
#include "net/AppUpdate.h"
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <QTimeZone>
#include "net/SearchAggregator.h"
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QUrlQuery>

int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--secondary")){InstanceGuard guard(app.arguments().at(2));return guard.acquire(app.arguments())==InstanceGuard::Forwarded?0:9;}
    QTemporaryDir dir;QFile report(app.applicationDirPath()+"/optimization-smoke.log");report.open(QIODevice::WriteOnly);int failed=0;
    auto check=[&](bool ok,const char *message){report.write(QByteArray(message)+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failed;};
    auto write=[&](QString name,QJsonObject data){QFile f(dir.filePath(name));f.open(QIODevice::WriteOnly);f.write(QJsonDocument(data).toJson());return f.fileName();};
    HttpFixture server;int gardenPages=0;
    server.handler=[&](QByteArray header,QByteArray){
        const auto target=header.split(' ').value(1);QUrl url("http://localhost"+target);HttpFixture::Response response;
        if(url.path()=="/garden"){
            ++gardenPages;const int page=QUrlQuery(url).queryItemValue("page").toInt();
            response.data=QJsonDocument(QJsonObject{{"resources",QJsonArray{QJsonObject{{"title",QString("[Group] Test Anime - %1").arg(page)},{"magnet","magnet:?xt=urn:btih:"+QString(39,'a')+QString::number(page)}}}},{"pagination",QJsonObject{{"complete",page>=3}}}}).toJson();
        } else {response.delay=url.path()=="/slow"?500:0;response.data="<rss xmlns:atom=\"http://www.w3.org/2005/Atom\"><channel>";
            if(url.path()=="/paged")response.data+="<atom:link rel=\"next\" href=\"/page2\"/>";
            if(url.path()=="/loop")response.data+="<atom:link rel=\"next\" href=\"/loop\"/>";
            response.data+="<item><title>[Group] Test Anime - "+QByteArray(url.path()=="/page2"?"05":"04")+" [1080p]</title><enclosure url=\"http://localhost/a.torrent\" /></item></channel></rss>";}
        return response;
    };
    QJsonArray sites;for(const auto &id:{"a","b","c","d","e","f","g","h","slow"})sites.append(QJsonObject{{"id",id},{"name",id},{"url",server.url()},{"rss_url",server.url("/"+QString(id)+"?q={q}")}});
    sites.append(QJsonObject{{"id","animegarden"},{"name","Garden"},{"url",server.url()},{"search_url",server.url("/garden")}});
    sites.append(QJsonObject{{"id","web-only"},{"name","Directory"},{"url",server.url()}});
    const QString path=write("catalog.json",{{"anime_torrent_sites",sites}});Catalog catalog(path);check(catalog.load()&&catalog.sites(true).size()==10,"custom RSS joins all supported enabled sources");
    QString error;check(!catalog.setEnabled("web-only",true,&error),"web directory cannot masquerade as a search adapter");
    check(catalog.setEnabled("b",false),"disable source persists");Catalog reloaded(path);reloaded.load();check(reloaded.sites(true).size()==9&&!reloaded.get("b")->enabled,"disabled state survives reload");
    catalog.setEnabled("b",true);HttpClient http;BangumiClient bangumi(&http);SearchAggregator search(&catalog,&http,&bangumi);
    AnimeSubject subject;subject.bangumiId=42;subject.nameEn="Test Anime";int finished=0,total=0;QSet<QString> visited;QHash<QString,ResourceResult> results;
    QObject::connect(&search,&SearchAggregator::started,&app,[&](int n){total=n;});QObject::connect(&search,&SearchAggregator::finished,&app,[&]{++finished;});
    QObject::connect(&search,&SearchAggregator::siteFinished,&app,[&](SiteStatus s){visited.insert(s.siteId);});
    QObject::connect(&search,&SearchAggregator::batchReady,&app,[&](QVector<ResourceResult> batch){for(auto r:batch)results.insert(r.rowKey(),r);});
    search.search(subject.nameEn,subject);check(spinUntil([&]{return finished==1;})&&total==10&&visited.size()==10,"every enabled source runs beyond concurrency limit");
    check(gardenPages==3&&results.size()==4,"Garden follows pagination to older episodes");
    check(search.retainedJobs()==0,"completed searches release network jobs");
    visited.clear();search.search("old",subject,{"slow"});search.search("new",subject,{"a"});
    check(spinUntil([&]{return finished==2;})&&visited==QSet<QString>{"a"},"replacement search cancels stale jobs and results");
    search.search("cancel",subject,{"slow"});search.cancel();check(search.retainedJobs()==0,"explicit cancellation releases active jobs");
    auto paged=sites.first().toObject();paged.insert("rss_url",server.url("/paged"));
    Catalog pageCatalog(write("paged.json",{{"anime_torrent_sites",QJsonArray{paged}}}));pageCatalog.load();SearchAggregator pageSearch(&pageCatalog,&http,&bangumi);
    bool pageDone=false,pageError=false;int pageCount=0;QObject::connect(&pageSearch,&SearchAggregator::finished,&app,[&]{pageDone=true;});
    QObject::connect(&pageSearch,&SearchAggregator::batchReady,&app,[&](QVector<ResourceResult> r){pageCount+=r.size();});
    QObject::connect(&pageSearch,&SearchAggregator::siteFinished,&app,[&](SiteStatus s){pageError=!s.ok;});
    pageSearch.search(subject.nameEn,subject);check(spinUntil([&]{return pageDone;})&&pageCount==2&&!pageError,"RSS follows explicit same-origin next link");
    pageCatalog.get("a")->rssUrl=server.url("/loop");pageDone=false;pageSearch.search(subject.nameEn,subject);
    check(spinUntil([&]{return pageDone;})&&pageError,"repeating RSS next link terminates with partial-result warning");
    const QString imported=write("import.json",{{"anime_torrent_sites",QJsonArray{sites.first()}}});check(catalog.importFrom(imported),"import writes current user catalog");
    Catalog importedReload(path);importedReload.load();check(importedReload.sites().size()==1,"import survives restart without remembering transient filename");
    write("bad.json",{});check(!catalog.importFrom(dir.filePath("bad.json"))&&catalog.sites().size()==1,"invalid import keeps prior catalog");
    check(!isRelevant("[Fansub] Fairy Tail - 01",{"air"})&&isRelevant("[Fansub] AIR - 01",{"air"}),"short Latin aliases require word boundaries");
    check(isRelevant("[Group] Test_Anime - 01",{"testanime"}),"separator variations still match anime name");
    {
        // Names as Bangumi lists them.
        auto anime=[](int id,const QString &cn,const QString &jp,const QString &date={},const QString &platform="TV") {
            AnimeSubject s;s.bangumiId=id;s.nameCn=cn;s.nameJp=jp;s.airDate=date;s.platform=platform;return s;
        };
        auto label=[](const QVector<SeriesEntry> &entries,int id) {
            for(const auto &e:entries)if(e.subject.bangumiId==id)return e.label;
            return QString();
        };
        const auto arc=anime(328195,QStringLiteral("鬼灭之刃 游郭篇"),QStringLiteral("鬼滅の刃 遊郭編"),"2021-12-05");
        check(seriesNames(arc)==QStringList({QStringLiteral("鬼灭之刃"),QStringLiteral("鬼滅の刃")}),"an arc's main name drops the arc");
        check(seriesNames(anime(1,QStringLiteral("Re：从零开始的异世界生活 第三季 袭击篇"),QStringLiteral("Re:ゼロから始める異世界生活 3rd season 襲擊編")))
              ==QStringList({QStringLiteral("Re：从零开始的异世界生活"),QStringLiteral("Re:ゼロから始める異世界生活")}),"a colon inside a Latin opening stays in the main name");
        check(seriesNames(anime(2,QStringLiteral("航海王：Z"),"ONE PIECE FILM Z")).first()==QStringLiteral("航海王"),"a subtitle after a colon is dropped");
        check(seriesNames(anime(3,QStringLiteral("进击的巨人 最终季 完结篇 前篇"),QStringLiteral("進撃の巨人 The Final Season 完結編 前編"))).first()==QStringLiteral("进击的巨人"),"named final seasons are dropped");
        check(seriesNames(anime(4,QStringLiteral("黑执事Ⅱ"),QStringLiteral("黒執事Ⅱ"))).first()==QStringLiteral("黑执事"),"a trailing numeral is dropped");
        check(seriesNames(anime(5,QStringLiteral("葬送的芙莉莲 第三季 黄金乡篇"),QStringLiteral("葬送のフリーレン 第3期【黄金郷編】"))).last()==QStringLiteral("葬送のフリーレン"),"Japanese season markers are dropped");

        const QVector<AnimeSubject> found{
            anime(245665,QStringLiteral("鬼灭之刃"),QStringLiteral("鬼滅の刃"),"2019-04-06"),
            arc,
            anime(369768,QStringLiteral("鬼灭之刃 刀匠村篇"),QStringLiteral("鬼滅の刃 刀鍛冶の里編"),"2023-04-09"),
            anime(291494,QStringLiteral("剧场版 鬼灭之刃 无限列车篇"),QStringLiteral("劇場版 鬼滅の刃 無限列車編"),"2020-10-16",QStringLiteral("剧场版")),
            anime(501960,QStringLiteral("剧场版 鬼灭之刃 无限城篇 第二部"),QStringLiteral("劇場版 鬼滅の刃 無限城編 第二部"),{},QStringLiteral("剧场版")),
            anime(549660,{},QStringLiteral("アニメ「鬼滅の刃」×コラボレーションムービー"),"2025-03-06",QStringLiteral("其他")),
            anime(18177,QStringLiteral("鬼"),QStringLiteral("鬼"),"1972-01-01",QStringLiteral("剧场版")),
        };
        const auto entries=seriesEntries(arc,found);
        check(entries.size()==5,"the series keeps its seasons and films and nothing else");
        check(entries.first().subject.bangumiId==245665&&entries.first().main&&entries.first().label==QStringLiteral("鬼灭之刃"),"the series starts with the bare title");
        check(entries.last().subject.bangumiId==501960,"entries that have not aired come last");
        check(label(entries,328195)==QStringLiteral("游郭篇"),"an entry is named after what sets it apart");
        check(label(entries,291494)==QStringLiteral("剧场版 无限列车篇"),"a film says so");
        const auto marks=seriesMarks(entries);
        check(mentionsAny(QStringLiteral("[Group] 鬼灭之刃 游郭篇 [01]"),marks)&&!mentionsAny(QStringLiteral("[Group] 鬼灭之刃 [01]"),marks),"a release title shows which entry it belongs to");

        const auto pirate=anime(975,QStringLiteral("海贼王"),QStringLiteral("海賊王"));
        check(seriesEntries(pirate,{anime(311310,QStringLiteral("海贼王女"),QStringLiteral("海賊王女"))}).size()==1,"a longer word is a different title");
        const auto zero=anime(140001,QStringLiteral("Re：从零开始的异世界生活"),QStringLiteral("Re:ゼロから始める異世界生活"),"2016-04-03");
        const auto second=seriesEntries(zero,{anime(278826,QStringLiteral("Re：从零开始的异世界生活 第二季"),QStringLiteral("Re:ゼロから始める異世界生活 2nd season"),"2020-07-08"),
                                              anime(177998,QStringLiteral("Re：从零开始的休息时间"),QStringLiteral("Re:ゼロから始める休憩時間"),"2016-04-08")});
        check(second.size()==2&&label(second,278826)==QStringLiteral("第二季"),"season names stay as Bangumi writes them");
        const auto ghoul=anime(115292,QStringLiteral("东京喰种√A"),QStringLiteral("東京喰種トーキョーグール √A"),"2015-01-08");
        const auto ghouls=seriesEntries(ghoul,{anime(93714,QStringLiteral("东京喰种"),QStringLiteral("東京喰種トーキョーグール"),"2014-07-03"),
                                              anime(148481,QStringLiteral("东京喰种:re"),QStringLiteral("東京喰種トーキョーグール：re"),"2018-04-03")});
        check(ghouls.size()==3&&label(ghouls,115292)==QStringLiteral("√A")&&label(ghouls,148481)==QStringLiteral("re"),"the other language finds seasons a title alone would miss");
        const auto butler=seriesEntries(anime(883,QStringLiteral("黑执事"),QStringLiteral("黒執事")),{anime(444049,QStringLiteral("黑执事 -寄宿学校篇-"),QStringLiteral("黒執事 -寄宿学校編-"))});
        check(label(butler,444049)==QStringLiteral("寄宿学校篇"),"the dashes around an arc name go with it");
    }
    InstanceGuard guard(dir.filePath("instance"));check(guard.acquire({"primary"})==InstanceGuard::Primary,"first instance owns data lock");
    QStringList forwarded;guard.onArguments=[&](QStringList args){forwarded=args;};QProcess secondary;secondary.start(app.applicationFilePath(),{"--secondary",dir.filePath("instance"),"--search",QStringLiteral("中文 查询")});
    check(spinUntil([&]{return secondary.state()==QProcess::NotRunning;})&&secondary.exitCode()==0&&forwarded.contains(QStringLiteral("中文 查询")),"secondary process forwards Unicode arguments without second writer");
    {
        SiteEntry site;site.id="nyaa";site.name="Nyaa";
        const QByteArray feed=R"(<rss><channel><item><title>[Group] Show - 01 [1080p]</title><link>https://example.com/1.torrent</link><enclosure url="https://example.com/1.torrent" length="1" type="application/x-bittorrent"/></item></channel></rss>)";
        const auto rows=parseRss(feed,site);
        check(rows.size()==1&&rows.first().fileSize==0,"placeholder enclosure length is treated as unknown size");
        ResourceResult target=rows.value(0),incoming=target;incoming.fileSize=700LL*1024*1024;mergeResource(target,incoming);
        check(target.fileSize==incoming.fileSize,"merge fills unknown size from another source");
        target.fileSize=1;mergeResource(target,incoming);
        check(target.fileSize==incoming.fileSize,"merge replaces placeholder 1 B size");
        incoming.fileSize=2048;mergeResource(target,incoming);
        check(target.fileSize==700LL*1024*1024,"merge keeps a real size over a smaller one");
    }
    check(AppUpdate::versionFromTag("v0.7.0-20261104")==QVersionNumber(0,7,0)&&AppUpdate::versionFromTag("0.6.1")==QVersionNumber(0,6,1)
          &&AppUpdate::versionFromTag("v1.2")==QVersionNumber(1,2),"release tags yield their version number");
    check(AppUpdate::versionFromTag("nightly").isNull()&&AppUpdate::versionFromTag("v2026").isNull()&&AppUpdate::versionFromTag("").isNull(),
          "tags without a version are ignored");
    {
        auto proxyFor=[](const QString &url){return QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(QUrl(url))).value(0);};
        HttpClient::applyProxy("socks5","127.0.0.1",1080);
        const auto remote=proxyFor("https://api.bgm.tv/calendar");
        check(remote.type()==QNetworkProxy::Socks5Proxy&&remote.hostName()=="127.0.0.1"&&remote.port()==1080,"manual proxy applies to site requests");
        check(proxyFor("http://127.0.0.1:8080/api/v2/app/version").type()==QNetworkProxy::NoProxy
              &&proxyFor("http://localhost:6800/jsonrpc").type()==QNetworkProxy::NoProxy,"local downloader connections never use the proxy");
        HttpClient::applyProxy("none",{},0);
        check(proxyFor("https://api.bgm.tv/calendar").type()==QNetworkProxy::NoProxy,"proxy can be turned off");
        HttpClient::applyProxy("http","",0);
        const auto system=QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl("https://api.bgm.tv/calendar"))).value(0);
        const auto fallback=proxyFor("https://api.bgm.tv/calendar");
        check(fallback.type()==system.type()&&fallback.hostName()==system.hostName()&&fallback.port()==system.port(),
              "incomplete manual proxy falls back to system settings");

        // A proxy on a closed local port makes Bangumi unreachable without touching the internet.
        HttpClient::applyProxy("http","127.0.0.1",1);
        QTemporaryDir cacheDir;BangumiClient offline(&http);offline.setCacheDir(cacheDir.path());
        BangumiClient::CalendarDays days;QString calendarError;QDateTime cachedAt;bool answered=false;
        auto ask=[&]{answered=false;offline.calendar([&](BangumiClient::CalendarDays d,QString e,QDateTime at){days=d;calendarError=e;cachedAt=at;answered=true;});return spinUntil([&]{return answered;},20000);};
        check(ask()&&days.isEmpty()&&!calendarError.isEmpty()&&!cachedAt.isValid(),"schedule without network or cache reports the error");
        QFile cache(cacheDir.filePath("calendar-cache.json"));cache.open(QIODevice::WriteOnly);
        cache.write(R"({"fetched_at":"2026-11-03T12:30:00Z","calendar":[{"weekday":{"id":2},"items":[{"id":7,"name":"Test","name_cn":"测试番剧"}]}]})");cache.close();
        check(ask()&&days.size()==1&&days.first().first==2&&days.first().second.value(0).bangumiId==7&&calendarError.isEmpty()
              &&cachedAt.isValid()&&cachedAt.toUTC()==QDateTime(QDate(2026,11,3),QTime(12,30),QTimeZone::UTC),"offline schedule falls back to the last saved one with its time");
        HttpClient::applyProxy("none",{},0);
    }
    {
        // Qt resends on its own a few times and keeps spare connections, so count requests
        // rather than connections, and find out first what one plain request uses up.
        QTcpServer server;server.listen(QHostAddress::LocalHost);int requests=0,dropFirst=0;
        QObject::connect(&server,&QTcpServer::newConnection,&app,[&] {
            while(auto *socket=server.nextPendingConnection()) {
                QObject::connect(socket,&QTcpSocket::readyRead,socket,[&requests,&dropFirst,socket] {
                    socket->readAll();
                    if(++requests>dropFirst)socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                    socket->disconnectFromHost();
                });
                QObject::connect(socket,&QTcpSocket::disconnected,socket,&QTcpSocket::deleteLater);
            }
        });
        const QUrl url(QStringLiteral("http://127.0.0.1:%1/calendar").arg(server.serverPort()));
        dropFirst=1000;
        {
            HttpClient client;auto *plain=client.get(url,5000);bool done=false;
            QObject::connect(plain,&QNetworkReply::finished,&app,[&]{done=true;});
            check(spinUntil([&]{return done;})&&plain->error()==QNetworkReply::RemoteHostClosedError,"dropped connection reaches the caller as an error");
        }
        const int perRequest=requests;
        auto ask=[&]{
            HttpClient client;requests=0;
            QNetworkReply::NetworkError result=QNetworkReply::UnknownNetworkError;QByteArray body;bool done=false;
            client.getWithRetry(url,5000,&app,[&](QNetworkReply *reply){result=reply->error();body=reply->readAll();reply->deleteLater();done=true;});
            spinUntil([&]{return done;});
            return qMakePair(result,body);
        };
        const bool gaveUp=ask().first==QNetworkReply::RemoteHostClosedError;
        const int attempts=requests;
        spinUntil([]{return false;},1200);   // longer than the retry delay: nothing may follow
        // Qt does not reset its own resends between requests, so the retry can use fewer.
        check(gaveUp&&attempts>perRequest&&attempts<=perRequest*2&&requests==attempts,"a connection that keeps dropping is retried once, not forever");
        dropFirst=perRequest;
        const auto recovered=ask();
        check(recovered.first==QNetworkReply::NoError&&recovered.second=="ok","a dropped connection recovers on the retry");
        dropFirst=0;
        check(ask().second=="ok"&&requests==1,"a working connection makes one request");
    }
    return failed?1:0;
}

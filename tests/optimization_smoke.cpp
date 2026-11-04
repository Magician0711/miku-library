#include "HttpFixture.h"
#include "core/Catalog.h"
#include "core/Grouping.h"
#include "core/InstanceGuard.h"
#include "net/HttpClient.h"
#include "net/BangumiClient.h"
#include "net/FeedParser.h"
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
    return failed?1:0;
}

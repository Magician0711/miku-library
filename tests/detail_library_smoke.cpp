// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "ui/pages/AnimeDetailPage.h"
#include "core/LocalLibrary.h"
#include "core/Catalog.h"
#include "core/TitleParser.h"
#include "net/SearchAggregator.h"
#include "net/HttpClient.h"
#include "net/BangumiClient.h"
#include "ui/ImageLoader.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QEventLoop>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonDocument>

int main(int argc,char **argv) {
    QApplication app(argc,argv);Theme::applyTheme(&app);QTemporaryDir dir;
    QFile report(QCoreApplication::applicationDirPath()+"/detail-library-smoke.log");report.open(QIODevice::WriteOnly);
    int failed=0;auto check=[&](bool ok,const char *s){report.write(QByteArray(s)+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failed;};
    QFile catalogFile(dir.filePath("catalog.json"));catalogFile.open(QIODevice::WriteOnly);catalogFile.write("{}");catalogFile.close();
    Catalog catalog(catalogFile.fileName());catalog.load();HttpClient http;BangumiClient bangumi(&http);
    SearchAggregator aggregator(&catalog,&http,&bangumi);ImageLoader images(dir.filePath("covers"));
    LocalLibrary library(dir.filePath("library.json"));library.load();
    AnimeSubject subject;subject.bangumiId=42;subject.nameCn=QStringLiteral("测试番剧");subject.nameJp="Test Anime";subject.aliases={"Test Anime"};
    subject.summary=QStringLiteral("测试本地视频、字幕组更新和资源筛选的展示效果。");subject.episodeCount=12;
    auto resource=[](int n,QString group="LoliHouse") {
        ResourceResult r;r.title=QString("[%1] Test Anime S01E%2 [1080p HEVC-10bit AAC][CHS]").arg(group).arg(n,2,10,QChar('0'));
        r.releaseGroup=group;r.magnetUri="magnet:?xt=urn:btih:"+QString::number(n+(group=="LoliHouse"?0:100),16).rightJustified(40,'0');
        r.tags=parseTitle(r.title);return r;
    };
    const auto first=resource(1),second=resource(2),other=resource(1,QStringLiteral("桜都字幕组"));
    const QString video=dir.filePath("Test Anime S01E01.mkv");QFile f(video);f.open(QIODevice::WriteOnly);f.write("video");f.close();
    LibraryItem item;item.subject=subject;item.resource=first;item.groupKey=releaseGroupKey(first.releaseGroup);item.key=library.itemKey(42,first);
    item.episodes={"1"};item.phase="complete";item.complete=true;item.files={{video,5,true}};library.put(item);
    AnimeDetailPage page(&aggregator,&bangumi,&catalog,&images,&library,[](int){return true;});page.resize(1120,900);page.setAttribute(Qt::WA_DontShowOnScreen);page.show();
    auto pump=[&]{QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QCoreApplication::processEvents();page.findChild<QTimer*>()->stop();QMetaObject::invokeMethod(page.findChild<QTimer*>(),"timeout");QCoreApplication::processEvents();QEventLoop loop;QTimer::singleShot(80,&loop,&QEventLoop::quit);loop.exec();};
    auto buttons=[&](QString text){QList<QPushButton*> out;for(auto *b:page.findChildren<QPushButton*>())if(b->isVisibleTo(&page)&&b->text()==text)out.append(b);return out;};
    auto feed=[&]{aggregator.batchReady({first,second,other});aggregator.finished();pump();};
    page.showSubject(subject);feed();
    check(buttons(QStringLiteral("播放")).size()==1&&buttons(QStringLiteral("下载")).size()==2,"resource rows switch downloaded item to play");
    check(buttons(QStringLiteral("更新")).size()==1&&buttons(QStringLiteral("一键下载")).size()==1,"group action reflects only its own history");
    int plays=0;QObject::connect(&page,&AnimeDetailPage::playRequested,&app,[&](AnimeSubject s,ResourceResult r){if(s.bangumiId==42&&r.releaseGroup=="LoliHouse")++plays;});
    if(!buttons(QStringLiteral("播放")).isEmpty())buttons(QStringLiteral("播放")).first()->click();
    check(plays==1,"play emits correct anime and group");
    QVector<ResourceResult> queued;QObject::connect(&page,&AnimeDetailPage::groupDownloadRequested,&app,[&](AnimeSubject,QVector<ResourceResult> r){queued=r;});
    if(!buttons(QStringLiteral("更新")).isEmpty())buttons(QStringLiteral("更新")).first()->click();
    bool isolated=queued.size()==2;for(const auto &r:queued)isolated=isolated&&r.releaseGroup=="LoliHouse";
    check(isolated,"group update only receives its own resources");
    auto *status=page.findChild<QComboBox*>("DownloadStatusFilter");
    check(status&&status->count()==3,"status filter contains all downloaded and missing");
    status->setCurrentIndex(1);pump();
    check(buttons(QStringLiteral("播放")).size()==1&&buttons(QStringLiteral("下载")).isEmpty(),"downloaded filter only shows actual completed video");
    status->setCurrentIndex(2);pump();
    check(buttons(QStringLiteral("播放")).isEmpty()&&buttons(QStringLiteral("下载")).size()==2,"missing filter excludes completed video");
    status->setCurrentIndex(0);pump();
    bool seedLabel=false;for(auto *l:page.findChildren<QLabel*>())if(l->text().startsWith(QStringLiteral("做种"))||l->text().startsWith(QStringLiteral("↑")))seedLabel=true;
    check(!seedLabel,"download action has no seeder count label");
    page.findChild<QScrollArea*>()->verticalScrollBar()->setValue(290);pump();
    for(auto *w:page.findChildren<QWidget*>("ResourceRow"))if(w->isVisibleTo(&page))report.write(QString("row height=%1 min=%2 hint=%3\n").arg(w->height()).arg(w->minimumSizeHint().height()).arg(w->sizeHint().height()).toUtf8());report.flush();
    page.grab().save(QCoreApplication::applicationDirPath()+"/detail-resources-preview.png");
    page.showSubject(subject,true);feed();
    check(buttons(QStringLiteral("播放")).size()==1&&buttons(QStringLiteral("下载")).isEmpty(),"following detail only shows local playable videos");
    check(buttons(QStringLiteral("一键下载")).isEmpty()&&buttons(QStringLiteral("更新")).size()==1,"following detail hides groups without downloaded media");
    check(!status->isEnabled()&&status->currentData().toString()=="downloaded","following detail stays in downloaded state");
    int resourceNavigation=0;QObject::connect(&page,&AnimeDetailPage::resourceSearchRequested,&app,[&](AnimeSubject s){if(s.bangumiId==42)++resourceNavigation;});
    auto *searchAction=page.findChild<QPushButton*>("ResourceSearchAction");searchAction->click();
    check(resourceNavigation==1&&searchAction->text()==QStringLiteral("查找可下载资源"),"following action navigates to the same anime search detail");
    page.findChild<QScrollArea*>()->verticalScrollBar()->setValue(220);pump();page.grab().save(QCoreApplication::applicationDirPath()+"/detail-local-preview.png");
    QFile::remove(video);status->setCurrentIndex(1);pump();
    check(buttons(QStringLiteral("播放")).isEmpty(),"deleted local video is removed from following detail");
    page.showSubject(subject);feed();
    check(status->isEnabled()&&status->currentIndex()==0,"search detail resets local-only filter and exposes downloadable resources");
    auto mirrorA=second;mirrorA.siteId="site-a";mirrorA.siteName="Site A";
    mirrorA.torrentUrl="https://a.invalid/seed";
    auto mirrorB=mirrorA;mirrorB.siteId="site-b";mirrorB.siteName="Site B";
    mirrorB.torrentUrl="https://b.invalid/seed";mirrorB.infoHash=QString(40,'b');mirrorB.magnetUri="magnet:?xt=urn:btih:"+mirrorB.infoHash;
    aggregator.batchReady({mirrorA});pump();aggregator.batchReady({mirrorB});pump();
    check(buttons(QStringLiteral("下载")).size()==3,"late duplicate filenames update existing rows instead of appending");
    bool bothSources=false;for(auto *l:page.findChildren<QLabel*>("ResourceSources"))if(l->isVisibleTo(&page)&&l->text().contains("Site A")&&l->text().contains("Site B"))bothSources=true;
    check(bothSources,"one resource row visibly lists both source sites");
    bool emittedSources=false;QObject::connect(&page,&AnimeDetailPage::downloadRequested,&app,[&](AnimeSubject,ResourceResult r){
        if(r.title==second.title&&r.seenOn.contains("Site A")&&r.seenOn.contains("Site B")&&downloadCandidates(r).size()>=3)emittedSources=true;
    });
    for(auto *b:buttons(QStringLiteral("下载")))b->click();
    check(emittedSources,"download action carries all alternate source endpoints");
    page.findChild<QScrollArea*>()->verticalScrollBar()->setValue(310);pump();
    page.grab().save(QCoreApplication::applicationDirPath()+"/detail-merged-sources-preview.png");
    QVector<ResourceResult> manyGroups;for(int i=0;i<10;++i)manyGroups.append(resource(1,"Group "+QString::number(i)));
    aggregator.batchReady(manyGroups);aggregator.finished();pump();
    auto visibleGroups=[&]{int count=0;for(auto *b:page.findChildren<QPushButton*>("GroupHeader"))if(b->isVisibleTo(&page))++count;return count;};
    check(visibleGroups()==3,"initial release group list remains compact");
    for(auto *b:page.findChildren<QPushButton*>())if(b->isVisibleTo(&page)&&b->text().startsWith(QStringLiteral("显示更多发布组"))){b->click();break;}
    pump();check(visibleGroups()==12,"one more-groups click reveals every release group");
    aggregator.batchReady({resource(1,"Late Group")});pump();
    check(visibleGroups()==13,"show-all choice also includes late arriving release groups");
    bool moreAgain=false;for(auto *b:page.findChildren<QPushButton*>())if(b->isVisibleTo(&page)&&b->text().startsWith(QStringLiteral("显示更多发布组")))moreAgain=true;
    check(!moreAgain,"no repeated more-groups action is needed");
    {
        // Ranking: the larger group leads; title-only "groups" and unattributed
        // releases are merged into one folded bucket at the bottom.
        page.showSubject(subject);
        QVector<ResourceResult> ranked;
        for(int n=21;n<=24;++n)ranked.append(resource(n,"Big Group"));
        ranked.append(resource(25,"Small Group"));
        auto pseudo=resource(26,"Test Anime");ranked.append(pseudo);
        auto bare=resource(27,"Nobody");bare.title="Test Anime S01E27 [1080p]";bare.releaseGroup.clear();bare.tags=parseTitle(bare.title);ranked.append(bare);
        aggregator.batchReady(ranked);aggregator.finished();pump();
        QStringList headers;QPushButton *last=nullptr;
        for(auto *b:page.findChildren<QPushButton*>("GroupHeader"))if(b->isVisibleTo(&page)){headers.append(b->text());last=b;}
        check(headers.size()==3&&headers.first().startsWith("Big Group")&&headers.last().startsWith(QStringLiteral("未归档资源")),"popular groups first, unattributed releases merged last");
        check(last&&!last->isChecked(),"unattributed bucket starts folded");
        bool bulkOnUnknown=false;
        if(last)for(auto *b:last->parentWidget()->findChildren<QPushButton*>())
            if((b->objectName()=="GroupDownload"||b->objectName()=="GroupDownloadOptions")&&b->isVisibleTo(&page))bulkOnUnknown=true;
        check(!bulkOnUnknown,"unattributed bucket offers no one-click download");
        int options=0;for(auto *b:page.findChildren<QPushButton*>("GroupDownloadOptions"))if(b->isVisibleTo(&page)&&b->isEnabled())++options;
        check(options==2,"each real group has a download options button");
    }
    {
        QTcpServer server;check(server.listen(QHostAddress::LocalHost),"local RSS fixture starts");
        const QString base="http://127.0.0.1:"+QString::number(server.serverPort());
        QObject::connect(&server,&QTcpServer::newConnection,&app,[&]{while(auto *socket=server.nextPendingConnection()) {
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket]{
                auto request=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",request);
                if(!request.contains("\r\n\r\n")||socket->property("done").toBool())return;socket->setProperty("done",true);
                const bool secondSite=request.startsWith("GET /b");
                const QByteArray hash(40,secondSite?'b':'a');
                const QByteArray xml="<rss><channel><item><title>[Fansub] Test Anime [03] [E528]</title><infoHash>"+hash
                    +"</infoHash><enclosure url=\"http://127.0.0.1/"+hash+".torrent\" /></item></channel></rss>";
                QTimer::singleShot(secondSite?50:0,socket,[socket,xml]{socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(xml.size())+"\r\nConnection: close\r\n\r\n"+xml);socket->disconnectFromHost();});
            });
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }});
        QJsonArray sites;for(const auto &id:{"a","b"})sites.append(QJsonObject{{"id",id},{"name",QString("Site ")+id},{"url",base},{"rss_url",base+"/"+id+"?q={q}"},{"adapter","rss"}});
        QFile file(dir.filePath("rss-catalog.json"));file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"anime_torrent_sites",sites}}).toJson());file.close();
        Catalog fixture(file.fileName());fixture.load();SearchAggregator search(&fixture,&http,&bangumi);bool finished=false;QHash<QString,ResourceResult> rows;bool incremental=false;
        QObject::connect(&search,&SearchAggregator::batchReady,&app,[&](QVector<ResourceResult> results){for(const auto &r:results){rows.insert(r.rowKey(),r);if(r.seenOn.size()==1)incremental=true;}});
        QEventLoop loop;QObject::connect(&search,&SearchAggregator::finished,&loop,[&]{finished=true;loop.quit();});QTimer::singleShot(5000,&loop,&QEventLoop::quit);
        search.search("Test Anime",subject);loop.exec();
        check(finished&&rows.size()==1,"RSS aggregation merges same filenames with different torrent hashes");
        check(incremental&&rows.value(rows.keys().value(0)).sources.size()==2&&rows.value(rows.keys().value(0)).seenOn.size()==2,"RSS emits updated row after later source arrives without duplicate endpoints");
    }
    return failed?1:0;
}

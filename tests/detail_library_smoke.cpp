// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "app/DetailController.h"
#include "core/Catalog.h"
#include "core/LocalLibrary.h"
#include "core/TitleParser.h"
#include "net/BangumiClient.h"
#include "net/HttpClient.h"
#include "net/SearchAggregator.h"

#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    QFile report(QCoreApplication::applicationDirPath() + "/detail-library-smoke.log");
    report.open(QIODevice::WriteOnly);
    int failed = 0;
    auto check = [&](bool ok, const char *s) { report.write(QByteArray(s) + (ok ? " PASS\n" : " FAIL\n")); report.flush(); if (!ok) ++failed; };

    QFile catalogFile(dir.filePath("catalog.json"));
    catalogFile.open(QIODevice::WriteOnly);
    catalogFile.write("{}");
    catalogFile.close();
    Catalog catalog(catalogFile.fileName());
    catalog.load();
    HttpClient http;
    BangumiClient bangumi(&http);
    SearchAggregator aggregator(&catalog, &http, &bangumi);
    LocalLibrary library(dir.filePath("library.json"));
    library.load();

    AnimeSubject subject;
    subject.bangumiId = 42;
    subject.nameCn = QStringLiteral("测试番剧");
    subject.nameJp = "Test Anime";
    subject.aliases = {"Test Anime"};
    subject.summary = QStringLiteral("测试本地视频、字幕组更新和资源筛选的展示效果。");
    subject.episodeCount = 12;
    auto resource = [](int n, QString group = "LoliHouse") {
        ResourceResult r;
        r.title = QString("[%1] Test Anime S01E%2 [1080p HEVC-10bit AAC][CHS]").arg(group).arg(n, 2, 10, QChar('0'));
        r.releaseGroup = group;
        r.magnetUri = "magnet:?xt=urn:btih:" + QString::number(n + (group == "LoliHouse" ? 0 : 100), 16).rightJustified(40, '0');
        r.tags = parseTitle(r.title);
        return r;
    };
    const auto first = resource(1), second = resource(2), other = resource(1, QStringLiteral("桜都字幕组"));
    const QString video = dir.filePath("Test Anime S01E01.mkv");
    QFile f(video);
    f.open(QIODevice::WriteOnly);
    f.write("video");
    f.close();
    LibraryItem item;
    item.subject = subject;
    item.resource = first;
    item.groupKey = releaseGroupKey(first.releaseGroup);
    item.key = library.itemKey(42, first);
    item.episodes = {"1"};
    item.phase = "complete";
    item.complete = true;
    item.files = {{video, 5, true}};
    library.put(item);

    DetailController page(&aggregator, &bangumi, &catalog, &library, [](int) { return true; });
    auto pump = [&] { QCoreApplication::processEvents(); page.renderNow(); QCoreApplication::processEvents(); };
    auto feed = [&] { aggregator.batchReady({first, second, other}); aggregator.finished(); pump(); };
    auto rows = [&](const QString &kind) {
        QVector<QVariantMap> out;
        for (const auto &row : page.rows()->rows())
            if (row.value("kind") == kind) out.append(row);
        return out;
    };
    auto actions = [&](const QString &text) {
        QVector<QVariantMap> out;
        for (const auto &row : rows("release"))
            if (row.value("actionText") == text) out.append(row);
        return out;
    };
    auto bulk = [&](const QString &text) {
        QVector<QVariantMap> out;
        for (const auto &row : rows("group"))
            if (row.value("bulkText") == text && row.value("bulkVisible").toBool()) out.append(row);
        return out;
    };

    page.showSubject(subject);
    feed();
    check(actions(QStringLiteral("播放")).size() == 1 && actions(QStringLiteral("下载")).size() == 2, "resource rows switch downloaded item to play");
    check(bulk(QStringLiteral("更新")).size() == 1 && bulk(QStringLiteral("一键下载")).size() == 1, "group action reflects only its own history");

    int plays = 0;
    QObject::connect(&page, &DetailController::playRequested, &app, [&](const AnimeSubject &s, const ResourceResult &r) { if (s.bangumiId == 42 && r.releaseGroup == "LoliHouse") ++plays; });
    page.releaseAction(actions(QStringLiteral("播放")).first().value("key").toString());
    check(plays == 1, "play emits correct anime and group");

    QVector<ResourceResult> queued;
    EpisodeRange queuedRange{-1, -1};
    int requests = 0;
    QObject::connect(&page, &DetailController::groupDownloadRequested, &app, [&](const AnimeSubject &, const QVector<ResourceResult> &r, const EpisodeRange &range) { queued = r; queuedRange = range; ++requests; });
    // The button only asks: nothing is queued until the dialog is answered.
    page.askGroupDownload("LoliHouse");
    const auto options = page.groupDialog();
    check(options.value("group") == "LoliHouse" && options.value("resolutions").toList().size() >= 2, "group button opens the options dialog");
    check(requests == 0, "opening the dialog queues nothing");
    page.dismissGroup();
    check(requests == 0 && page.groupDialog().isEmpty(), "cancelling the dialog queues nothing");
    page.askGroupDownload("LoliHouse");
    const int low = page.groupDialog().value("low").toInt(), high = page.groupDialog().value("high").toInt();
    const auto preview = page.previewGroup({}, {}, {}, low, high);
    check(preview.value("summary").toString().contains(QStringLiteral("符合条件 2 个资源")), "the dialog previews what the planner would queue");
    page.confirmGroup({}, {}, {}, low, high);
    bool isolated = requests == 1 && queued.size() == 2;
    for (const auto &r : queued)
        isolated = isolated && r.releaseGroup == "LoliHouse";
    check(isolated, "group update only receives its own resources");
    check(!queuedRange.active(), "the default episode span stays unlimited");
    page.askGroupDownload("LoliHouse");
    page.confirmGroup({}, {}, {}, low, low);
    check(queuedRange.first == low && queuedRange.last == low, "a narrower episode span reaches the download");
    page.askGroupDownload("LoliHouse");
    page.confirmGroup({}, {}, "2160p", low, high);
    check(queued.isEmpty(), "a resolution choice filters the group's resources");
    page.dismissGroup();

    check(page.filterValues().value("status") == "all", "status filter starts on everything");
    page.setFilter("status", "downloaded");
    pump();
    check(actions(QStringLiteral("播放")).size() == 1 && actions(QStringLiteral("下载")).isEmpty(), "downloaded filter only shows actual completed video");
    page.setFilter("status", "missing");
    pump();
    check(actions(QStringLiteral("播放")).isEmpty() && actions(QStringLiteral("下载")).size() == 2, "missing filter excludes completed video");
    page.setFilter("status", "all");
    pump();

    page.showSubject(subject, true);
    feed();
    check(actions(QStringLiteral("播放")).size() == 1 && actions(QStringLiteral("下载")).isEmpty(), "following detail only shows local playable videos");
    check(bulk(QStringLiteral("一键下载")).isEmpty() && bulk(QStringLiteral("更新")).size() == 1, "following detail hides groups without downloaded media");
    check(page.localOnly() && page.filterValues().value("status") == "downloaded", "following detail stays in downloaded state");
    int resourceNavigation = 0;
    QObject::connect(&page, &DetailController::resourceSearchRequested, &app, [&](const AnimeSubject &s) { if (s.bangumiId == 42) ++resourceNavigation; });
    page.researchClicked();
    check(resourceNavigation == 1, "following action navigates to the same anime search detail");
    QFile::remove(video);
    page.setFilter("status", "all");
    page.setFilter("status", "downloaded");
    pump();
    check(actions(QStringLiteral("播放")).isEmpty(), "deleted local video is removed from following detail");

    page.showSubject(subject);
    feed();
    check(!page.localOnly() && page.filterValues().value("status") == "all", "search detail resets local-only filter and exposes downloadable resources");
    auto mirrorA = second;
    mirrorA.siteId = "site-a";
    mirrorA.siteName = "Site A";
    mirrorA.torrentUrl = "https://a.invalid/seed";
    auto mirrorB = mirrorA;
    mirrorB.siteId = "site-b";
    mirrorB.siteName = "Site B";
    mirrorB.torrentUrl = "https://b.invalid/seed";
    mirrorB.infoHash = QString(40, 'b');
    mirrorB.magnetUri = "magnet:?xt=urn:btih:" + mirrorB.infoHash;
    aggregator.batchReady({mirrorA});
    pump();
    aggregator.batchReady({mirrorB});
    pump();
    check(actions(QStringLiteral("下载")).size() == 3, "late duplicate filenames update existing rows instead of appending");
    bool bothSources = false;
    for (const auto &row : rows("release"))
        bothSources = bothSources || (row.value("meta").toString().contains("Site A") && row.value("meta").toString().contains("Site B"));
    check(bothSources, "one resource row visibly lists both source sites");
    bool emittedSources = false;
    QObject::connect(&page, &DetailController::downloadRequested, &app, [&](const AnimeSubject &, const ResourceResult &r) {
        if (r.title == second.title && r.seenOn.contains("Site A") && r.seenOn.contains("Site B") && downloadCandidates(r).size() >= 3)
            emittedSources = true;
    });
    for (const auto &row : actions(QStringLiteral("下载")))
        page.releaseAction(row.value("key").toString());
    check(emittedSources, "download action carries all alternate source endpoints");

    // A new batch must keep the rows on screen, not rebuild them.
    {
        int resets = 0, removed = 0;
        QObject::connect(page.rows(), &QAbstractItemModel::modelReset, &app, [&] { ++resets; });
        QObject::connect(page.rows(), &QAbstractItemModel::rowsRemoved, &app, [&] { ++removed; });
        const auto before = page.rows()->count();
        aggregator.batchReady({resource(3)});
        pump();
        check(resets == 0 && removed == 0 && page.rows()->count() == before + 1, "a new batch inserts rows without rebuilding the list");
    }

    QVector<ResourceResult> manyGroups;
    for (int i = 0; i < 10; ++i)
        manyGroups.append(resource(1, "Group " + QString::number(i)));
    aggregator.batchReady(manyGroups);
    aggregator.finished();
    pump();
    check(rows("group").size() == 3, "initial release group list remains compact");
    check(rows("moreGroups").size() == 1, "further groups wait behind one action");
    page.showAllGroups();
    pump();
    check(rows("group").size() == 12, "one more-groups click reveals every release group");
    aggregator.batchReady({resource(1, "Late Group")});
    pump();
    check(rows("group").size() == 13, "show-all choice also includes late arriving release groups");
    check(rows("moreGroups").isEmpty(), "no repeated more-groups action is needed");

    {
        // Folding a group removes its rows and leaves the others alone.
        const QString group = rows("group").first().value("group").toString();
        const int before = rows("release").size();
        page.toggleGroup(group);
        check(rows("release").size() < before && !rows("group").first().value("expanded").toBool(), "folding a group hides its rows");
        page.toggleGroup(group);
        check(rows("release").size() == before, "unfolding brings them back");
    }
    {
        // Ranking: the larger group leads; title-only "groups" and unattributed
        // releases are merged into one folded bucket at the bottom.
        page.showSubject(subject);
        QVector<ResourceResult> ranked;
        for (int n = 21; n <= 24; ++n)
            ranked.append(resource(n, "Big Group"));
        ranked.append(resource(25, "Small Group"));
        ranked.append(resource(26, "Test Anime"));
        auto bare = resource(27, "Nobody");
        bare.title = "Test Anime S01E27 [1080p]";
        bare.releaseGroup.clear();
        bare.tags = parseTitle(bare.title);
        ranked.append(bare);
        aggregator.batchReady(ranked);
        aggregator.finished();
        pump();
        const auto headers = rows("group");
        check(headers.size() == 3 && headers.first().value("text").toString().startsWith("Big Group")
                  && headers.last().value("text").toString().startsWith(QStringLiteral("未归档资源")),
              "popular groups first, unattributed releases merged last");
        check(!headers.last().value("expanded").toBool(), "unattributed bucket starts folded");
        check(!headers.last().value("bulkVisible").toBool(), "unattributed bucket offers no one-click download");
        int enabled = 0;
        for (const auto &header : headers)
            enabled += header.value("bulkEnabled").toBool();
        check(enabled == 2, "each real group has one download button");
    }
    {
        // A site that never answers keeps the search running while batches arrive.
        QTcpServer silent;
        silent.listen(QHostAddress::LocalHost);
        const QString base = "http://127.0.0.1:" + QString::number(silent.serverPort());
        const QJsonArray sites{QJsonObject{{"id", "slow"}, {"name", "Slow"}, {"url", base}, {"rss_url", base + "/slow?q={q}"}, {"adapter", "rss"}}};
        QFile file(dir.filePath("slow-catalog.json"));
        file.open(QIODevice::WriteOnly);
        file.write(QJsonDocument(QJsonObject{{"anime_torrent_sites", sites}}).toJson());
        file.close();
        Catalog slow(file.fileName());
        slow.load();
        SearchAggregator running(&slow, &http, &bangumi);
        DetailController waiting(&running, &bangumi, &slow, &library, [](int) { return true; });
        auto spinning = [&] {
            int count = 0;
            for (const auto &row : waiting.rows()->rows())
                if (row.value("kind") == "group" && row.value("bulkBusy").toBool() && !row.value("bulkEnabled").toBool()) ++count;
            return count;
        };
        waiting.showSubject(subject);
        running.batchReady({first, second, other});
        waiting.renderNow();
        check(waiting.searching() && spinning() == 2, "group download buttons show a spinner during the search");
        running.finished();
        waiting.renderNow();
        check(spinning() == 0, "the spinner stops when the search ends");
        waiting.cancel();
    }
    {
        // RSS aggregation through a local server.
        QTcpServer server;
        check(server.listen(QHostAddress::LocalHost), "local RSS fixture starts");
        const QString base = "http://127.0.0.1:" + QString::number(server.serverPort());
        QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                    auto request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n") || socket->property("done").toBool())
                        return;
                    socket->setProperty("done", true);
                    const bool secondSite = request.startsWith("GET /b");
                    const QByteArray hash(40, secondSite ? 'b' : 'a');
                    const QByteArray xml = "<rss><channel><item><title>[Fansub] Test Anime [03] [E528]</title><infoHash>" + hash
                        + "</infoHash><enclosure url=\"http://127.0.0.1/" + hash + ".torrent\" /></item></channel></rss>";
                    QTimer::singleShot(secondSite ? 50 : 0, socket, [socket, xml] {
                        socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(xml.size()) + "\r\nConnection: close\r\n\r\n" + xml);
                        socket->disconnectFromHost();
                    });
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        QJsonArray sites;
        for (const auto &id : {"a", "b"})
            sites.append(QJsonObject{{"id", id}, {"name", QString("Site ") + id}, {"url", base}, {"rss_url", base + "/" + id + "?q={q}"}, {"adapter", "rss"}});
        QFile file(dir.filePath("rss-catalog.json"));
        file.open(QIODevice::WriteOnly);
        file.write(QJsonDocument(QJsonObject{{"anime_torrent_sites", sites}}).toJson());
        file.close();
        Catalog fixture(file.fileName());
        fixture.load();
        SearchAggregator search(&fixture, &http, &bangumi);
        bool finished = false, incremental = false;
        QHash<QString, ResourceResult> found;
        QObject::connect(&search, &SearchAggregator::batchReady, &app, [&](QVector<ResourceResult> results) {
            for (const auto &r : results) {
                found.insert(r.rowKey(), r);
                if (r.seenOn.size() == 1) incremental = true;
            }
        });
        QEventLoop loop;
        QObject::connect(&search, &SearchAggregator::finished, &loop, [&] { finished = true; loop.quit(); });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        search.search("Test Anime", subject);
        loop.exec();
        check(finished && found.size() == 1, "RSS aggregation merges same filenames with different torrent hashes");
        check(incremental && found.value(found.keys().value(0)).sources.size() == 2 && found.value(found.keys().value(0)).seenOn.size() == 2,
              "RSS emits updated row after later source arrives without duplicate endpoints");
    }
    {
        // Other seasons and arcs are one choice away, and each lists its own releases.
        AnimeSubject main;
        main.bangumiId = 900;
        main.nameCn = QStringLiteral("系列番");
        main.nameJp = QStringLiteral("シリーズ");
        main.aliases = {QStringLiteral("系列番")};
        AnimeSubject sequel = main, arc = main;
        sequel.bangumiId = 901;
        sequel.nameCn = QStringLiteral("系列番 第二季");
        sequel.nameJp = QStringLiteral("シリーズ 第2期");
        sequel.airDate = "2024-01-01";
        sequel.year = 2024;
        sequel.platform = "TV";
        arc.bangumiId = 902;
        arc.nameCn = QStringLiteral("系列番 黄昏篇");
        arc.nameJp = QStringLiteral("シリーズ 黄昏編");
        arc.aliases = {arc.nameCn};
        arc.airDate = "2025-01-01";
        main.airDate = "2023-01-01";
        auto titled = [&](int n, const QString &title) {
            auto r = resource(n, QStringLiteral("字幕组"));
            r.title = title;
            r.tags = parseTitle(title);
            return r;
        };
        const QVector<ResourceResult> releases{
            titled(901, QStringLiteral("[字幕组] 系列番 [01][1080p]")),
            titled(902, QStringLiteral("[字幕组] 系列番 第二季 [01][1080p]")),
            titled(903, QStringLiteral("[字幕组] 系列番 黄昏篇 [01][1080p]")),
        };
        auto feedSeries = [&] { aggregator.batchReady(releases); aggregator.finished(); pump(); };
        auto shownTitles = [&] {
            QStringList out;
            for (const auto &row : rows("release"))
                out << row.value("text").toString();
            return out;
        };
        QVector<QPair<int, bool>> requested;
        QObject::connect(&page, &DetailController::animeRequested, &app, [&](const AnimeSubject &s, bool local) {
            requested.append({s.bangumiId, local});
            page.showSubject(s, local);
        });

        page.showSubject(main);
        check(page.series().isEmpty(), "a new anime has no series until Bangumi answers");
        const QVector<AnimeSubject> listed{sequel, arc};
        page.applySeries(listed);
        const auto series = page.series();
        check(series.size() == 3 && series.first().toMap().value("value") == 900, "the series lists every entry, oldest first");
        check(series.at(1).toMap().value("text") == QStringLiteral("第二季") && series.at(1).toMap().value("note") == QStringLiteral("2024 · TV"),
              "an entry shows its own name, year and format");
        check(series.at(2).toMap().value("text") == QStringLiteral("黄昏篇"), "an arc is not forced into a season number");
        feedSeries();
        check(shownTitles() == QStringList{releases.at(0).title}, "the main entry leaves out its seasons and arcs");

        page.switchSeries(901);
        pump();
        check(requested.size() == 1 && requested.first().first == 901 && page.hero().value("subjectId") == 901, "choosing a season swaps the whole page");
        check(page.series().size() == 3, "the list stays while moving within the series");
        feedSeries();
        check(shownTitles() == QStringList{releases.at(1).title}, "a numbered season lists its own releases");

        page.switchSeries(902);
        pump();
        feedSeries();
        check(page.hero().value("title") == arc.nameCn && shownTitles() == QStringList{releases.at(2).title}, "an arc lists its own releases");
        check(page.filterValues().value("season") == -1, "an arc is not narrowed to a season number");

        page.switchSeries(902);
        check(requested.size() == 2, "choosing the current entry does nothing");
        page.switchSeries(12345);
        check(requested.size() == 2, "an unknown entry does nothing");

        page.showSubject(main, true);
        pump();
        page.switchSeries(901);
        pump();
        check(!page.localOnly(), "moving to a season with no videos leaves the local view");
        page.showSubject(subject);
        check(page.series().isEmpty(), "another anime drops the series");
    }
    return failed ? 1 : 0;
}

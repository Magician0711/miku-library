// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

// Loads the real interface and walks it: every page, every dialog, theme switches and
// the frameless window. A QML warning anywhere fails the run.

#include "app/AppController.h"
#include "app/LanguageController.h"
#include "app/Format.h"
#include "app/ImageProviders.h"
#include "app/MapListModel.h"
#include "app/Platform.h"
#include "app/WindowChrome.h"
#include "app/StartupFlow.h"
#include "core/AppConfig.h"
#include "core/Catalog.h"
#include "core/Database.h"
#include "core/LocalLibrary.h"
#include "net/BangumiClient.h"
#include "net/HttpClient.h"
#include "net/ImageLoader.h"
#include "net/SearchAggregator.h"
#include "net/TorrentEngine.h"

#include <QBuffer>
#include <QElapsedTimer>
#include <QSet>
#include <QFile>
#include <QGuiApplication>
#include <QTimer>
#include <QInputMethodEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTest>
#include <QQmlComponent>
#include <QOperatingSystemVersion>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtQml/qqmlextensionplugin.h>

#include <QWheelEvent>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

Q_IMPORT_QML_PLUGIN(MikuLibraryPlugin)

namespace {
QStringList g_warnings;

void collect(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type != QtDebugMsg && type != QtInfoMsg && (message.contains("qrc:/") || message.contains("QML")))
        g_warnings << message;
}

void settle(int ms)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

template <class F> bool waitFor(F ready, int ms = 3000)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms) {
        if (ready())
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return ready();
}

template <class Test> QQuickItem *findItem(QQuickItem *root, Test test)
{
    if (test(root))
        return root;
    for (auto *child : root->childItems())
        if (auto *hit = findItem(child, test))
            return hit;
    return nullptr;
}
}

int main(int argc, char **argv)
{
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    Platform::prepareGraphics();
    PlatformApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir dir;
    qputenv("MIKU_LIBRARY_DATA_DIR", dir.path().toUtf8());
    QFile report(QCoreApplication::applicationDirPath() + "/quick-ui-smoke.log");
    report.open(QIODevice::WriteOnly);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { report.write(QByteArray(name) + (ok ? " PASS\n" : " FAIL\n")); report.flush(); if (!ok) ++failures; };
    qInstallMessageHandler(collect);

    {
        // The list model applies changes as the smallest edit it can find.
        MapListModel model({"id", "text"}, "id");
        auto row = [](int id, const QString &text) { return QVariantMap{{"id", id}, {"text", text}}; };
        model.update({row(1, "a"), row(2, "b"), row(3, "c")});
        int resets = 0, inserted = 0, removed = 0, changed = 0;
        QObject::connect(&model, &QAbstractItemModel::modelReset, &app, [&] { ++resets; });
        QObject::connect(&model, &QAbstractItemModel::rowsInserted, &app, [&](const QModelIndex &, int a, int b) { inserted += b - a + 1; });
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &app, [&](const QModelIndex &, int a, int b) { removed += b - a + 1; });
        QObject::connect(&model, &QAbstractItemModel::dataChanged, &app, [&] { ++changed; });
        model.update({row(1, "a"), row(2, "B"), row(3, "c")});
        check(changed == 1 && inserted == 0 && removed == 0, "a changed value updates only its row");
        model.update({row(1, "a"), row(9, "x"), row(2, "B"), row(3, "c")});
        check(inserted == 1 && removed == 0 && model.count() == 4, "a new row in the middle is one insertion");
        model.update({row(1, "a"), row(3, "c")});
        check(removed == 2 && model.count() == 2 && model.get(1).value("id") == 3, "rows that left are removed around what stays");
        model.update({});
        check(model.count() == 0 && resets == 0, "the model never resets");
    }
    {
        check(Format::size(0) == QStringLiteral("—") && Format::size(1536) == "1.5 KiB", "sizes read naturally");
        check(Format::speed(2097152) == "2.0 MB/s", "speeds read naturally");
        check(Format::eta(3700) == QStringLiteral("1 时 1 分") && Format::eta(-1) == QStringLiteral("—"), "remaining time reads naturally");
        check(Format::episodeSummary({"13", "14"}) == QStringLiteral("第 13–14 话"), "episode runs are summarised");
        check(Notes::disclaimerItems().size() == 8, "the declaration is complete");
    }
    {
        IconProvider icons;
        QSize size;
        const QImage icon = icons.requestImage("download/ff0000", &size, QSize(48, 48));
        bool drawn = false;
        for (int y = 0; y < 48 && !drawn; ++y)
            for (int x = 0; x < 48 && !drawn; ++x)
                drawn = icon.pixelColor(x, y).alpha() > 200 && icon.pixelColor(x, y).red() > 200;
        check(size == QSize(48, 48) && drawn, "icons draw in the requested colour and size");
        const QImage settings = icons.requestImage("settings/00ff00", nullptr, QSize(48, 48));
        check(settings.pixelColor(18, 12).alpha() < 40, "the settings glyph punches holes without a background colour");
        check(!appIcon().isNull() && appIcon().availableSizes().size() >= 8, "the app icon comes in every size");
    }
    {
        // Workers post results back to the loader; none may outlive it.
        auto *loader = new ImageLoader(dir.filePath("covers-a"));
        loader->request(QStringLiteral("http://example.com/b.jpg"), [](QImage) {});
        delete loader;
        check(true, "cover loader destructor waits for pending workers");
    }
    {
        // Bangumi serves some covers thousands of pixels tall and several megabytes large.
        QImage tall(3000, 5400, QImage::Format_RGB32);
        quint32 seed = 7;
        for (int y = 0; y < tall.height(); ++y) {
            auto *line = reinterpret_cast<quint32 *>(tall.scanLine(y));
            for (int x = 0; x < tall.width(); ++x) { seed = seed * 1664525u + 1013904223u; line[x] = 0xff000000u | (seed >> 8); }
        }
        QByteArray encoded;
        QBuffer sink(&encoded);
        sink.open(QIODevice::WriteOnly);
        tall.save(&sink, "JPG", 95);
        const QImage cover = ImageLoader::decodeCover(encoded);
        check(encoded.size() > 4 * 1024 * 1024 && cover.size() == QSize(400, 600), "a cover of several megabytes still shows");
        check(ImageLoader::decodeCover("not an image").isNull(), "an unreadable cover is left empty");
    }

    {
        // The task list: selection, what the actions offer, and removal.
        AppConfig options;
        options.removeAction = "ask";
        LocalLibrary tasks(dir.filePath("tasks.json"));
        tasks.load();
        for (int i = 1; i <= 3; ++i) {
            LibraryItem record;
            record.subject.bangumiId = 90;
            record.resource.title = QStringLiteral("任务 %1").arg(i);
            record.key = QStringLiteral("k%1").arg(i);
            record.downloader = "aria2";
            record.phase = "external";
            tasks.put(record);
        }
        TorrentEngine idle(options.engine, nullptr, dir.filePath("idle"));
        DialogHub hub;
        DownloadsController list(&idle, &tasks, &options, nullptr, &hub);
        list.refresh();
        check(list.model()->count() == 3 && list.summary() == QStringLiteral("共 3 个任务"), "external tasks are listed");
        check(list.filters().size() == 3 && list.filters().first().toMap().value("text") == QStringLiteral("全部 3"),
              "the filters count tasks, and failures are only offered when there are some");
        list.setFilter(2);
        check(list.filter() == 0 && list.model()->count() == 3, "an empty filter falls back to every task");
        check(list.pauseText() == QStringLiteral("全部暂停") && list.resumeText() == QStringLiteral("全部开始"),
              "with nothing selected the buttons act on every task");
        const QString a = list.model()->get(0).value("key").toString(), c = list.model()->get(2).value("key").toString();
        list.selectOnly(a);
        check(list.selectedCount() == 1 && list.model()->get(0).value("selected").toBool(), "a click selects one task");
        check(list.pauseText() == QStringLiteral("暂停") && !list.canPlay(a), "a selection narrows the buttons; nothing to play yet");
        check(list.canOpenClient() && list.removeText() == QStringLiteral("移除记录"), "one external task offers its client and record removal");
        list.extendTo(c);
        check(list.selectedCount() == 3 && list.allSelected(), "shift extends the selection");
        check(list.removeText() == QStringLiteral("移除 3 项") && list.hint().contains(QStringLiteral("已选择 3")), "the actions name the selection");
        list.toggle(a);
        check(list.selectedCount() == 2 && !list.allSelected(), "ctrl or the checkbox toggles one task");
        list.prepareMenu(a);
        check(list.selectedCount() == 1, "a right click outside the selection acts on that row");
        list.toggleAll();
        check(list.allSelected(), "the header selects everything");
        list.toggleAll();
        check(list.selectedCount() == 0, "and clears it again");
        list.selectOnly(a);
        list.removeSelected();
        list.refresh();
        check(list.model()->count() == 2 && list.selectedCount() == 0, "removing drops the record and the selection");
        check(hub.current().value("title") == QStringLiteral("记录已移除"), "removing an external record explains what remains");
    }
    {
        // The site list: order, filters and enabling.
        const QJsonArray entries{
            QJsonObject{{"id", "rss"}, {"name", "Alpha"}, {"url", "https://www.alpha.example"}, {"rss_url", "https://alpha.example/rss?q={q}"}, {"adapter", "rss"}, {"language", "zh"}},
            QJsonObject{{"id", "web"}, {"name", "Beta"}, {"url", "https://beta.example"}, {"language", "en"}}};
        QFile file(dir.filePath("sites.json"));
        file.open(QIODevice::WriteOnly);
        file.write(QJsonDocument(QJsonObject{{"anime_torrent_sites", entries}}).toJson());
        file.close();
        Catalog sites(file.fileName());
        sites.load();
        HttpClient client;
        DialogHub hub;
        SitesController list(&sites, &client, &hub);
        check(list.model()->count() == 2 && list.model()->get(0).value("siteId") == "rss", "search sources come first");
        check(list.model()->get(0).value("host") == "alpha.example", "hosts drop the www prefix");
        list.setScope(1);
        check(list.model()->count() == 1, "the scope keeps searchable sources");
        list.setScope(2);
        check(list.model()->count() == 1 && list.model()->get(0).value("siteId") == "web", "or the web directories");
        list.setScope(0);
        list.setFilter(QStringLiteral("英文"));
        check(list.model()->count() == 1 && list.model()->get(0).value("name") == "Beta", "the filter searches every column");
        list.setFilter({});
        list.setEnabled("rss", false);
        check(!sites.get("rss")->enabled, "the checkbox switches a source");
    }

    AppConfig config;
    config.closeAction = "ask";
    config.appearance = "light";
    // The checks below read Chinese text; "system" would follow the test machine.
    config.language = "zh-CN";
    config.save();
    QFile catalogFile(dir.filePath("catalog.json"));
    catalogFile.open(QIODevice::WriteOnly);
    catalogFile.write("{}");
    catalogFile.close();
    Catalog catalog(catalogFile.fileName());
    catalog.load();
    Database db(dir.filePath("test.db"));
    QString dbError;
    db.init(&dbError);
    HttpClient::applyProxy("none", {}, 0);
    HttpClient http;
    BangumiClient bangumi(&http);
    bangumi.setCacheDir(dir.path());
    SearchAggregator aggregator(&catalog, &http, &bangumi);
    ImageLoader images(dir.filePath("covers"));
    TorrentEngine torrents(config.engine, nullptr, dir.path());
    ThemeController theme(config.appearance);
    WindowChrome chrome;
    LanguageController language(&config);
    AppController controller(&catalog, &db, &config, &http, &torrents, &aggregator, &bangumi, &theme, &chrome);
    QObject::connect(&language, &LanguageController::changed, &controller, &AppController::retranslate);

    QQmlApplicationEngine engine;
    QObject::connect(&language, &LanguageController::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("Lang", &language);
    engine.rootContext()->setContextProperty("Theme", &theme);
    engine.rootContext()->setContextProperty("Chrome", &chrome);
    engine.rootContext()->setContextProperty("App", &controller);
    engine.addImageProvider("icon", new IconProvider);
    engine.addImageProvider("cover", new CoverProvider(&images));
    {
        // A new profile answers the guide first; only then does the main window open.
        const auto guideWindow = [] {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->title() == QStringLiteral("欢迎使用 Miku Library"))
                    return candidate;
            return static_cast<QWindow *>(nullptr);
        };
        StartupFlow accepted(QStringLiteral("guide"), dir.path());
        QTimer::singleShot(300, &accepted, [&] { accepted.finish(true); });
        check(accepted.run(&engine), "accepting the guide lets the app start");
        check(guideWindow() == nullptr, "the guide window is gone once it is answered");
        check(engine.rootObjects().isEmpty(), "the guide leaves nothing behind for the main window");

        StartupFlow declined(QStringLiteral("guide"), dir.path());
        QTimer::singleShot(300, &declined, [&] { if (auto *open = guideWindow()) open->close(); });
        check(!declined.run(&engine), "closing the guide declines it");
        check(guideWindow() == nullptr, "the declined guide window is gone too");
    }
    engine.loadFromModule("MikuLibrary", "Main");
    check(!engine.rootObjects().isEmpty(), "the main window loads");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constLast());
    controller.setWindow(window);
    check(window && waitFor([&] { return window->isExposed(); }), "the window opens");
    check(window->flags() & Qt::FramelessWindowHint, "the window draws its own frame");
    {
        auto *logo = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "brandLogo"; });
        check(logo && logo->property("status").toInt() == 1, "the navigation bar shows the logo");
        // Right after start-up, before anything has moved: the glass already samples
        // what lies beneath it, not where the bar was before the window was laid out.
        auto *bar = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "navBar"; });
        auto *behind = bar ? findItem(bar, [](QQuickItem *item) { return item->objectName() == "glassBehind"; }) : nullptr;
        const QRectF sampled = behind ? behind->property("sourceRect").toRectF() : QRectF();
        const QPointF barAt = bar ? bar->mapToScene(QPointF(0, 0)) : QPointF();
        check(behind && qAbs(sampled.x() - barAt.x()) < 1 && qAbs(sampled.y() - barAt.y()) < 1,
              QStringLiteral("the glass samples beneath the bar from the start (sampled %1,%2, bar %3,%4)")
                  .arg(sampled.x()).arg(sampled.y()).arg(barAt.x()).arg(barAt.y()).toUtf8().constData());
        check(!appIcon().isNull() && !appIcon().pixmap(32, 32).isNull(), "the app has its icon");
        check(window->title() == QStringLiteral("Miku Library"), "the window carries the new name");
    }
    {
        // Every language applies at once and comes back, with no QML warning on the way.
        const auto hasText = [&](const QString &text) {
            return findItem(window->contentItem(), [&](QQuickItem *item) { return item->property("text").toString() == text; }) != nullptr;
        };
        check(LanguageController::resolve("system", {"zh-Hant-TW"}) == "zh-TW" && LanguageController::resolve("system", {"ja-JP"}) == "ja"
                  && LanguageController::resolve("system", {"de-DE", "fr"}) == "en" && LanguageController::resolve("system", {"zh-CN"}) == "zh-CN",
              "following the system picks the closest language");
        check(hasText(QStringLiteral("本季放送")), "the navigation starts in Simplified Chinese");
        for (const QString code : {"en", "ja", "zh-TW"}) {
            language.choose(code);
            settle(50);
            // Written differently in every language, unlike 本季放送 in Traditional Chinese.
            const QString downloads = QCoreApplication::translate("Miku", "下载任务");
            check(language.language() == code && downloads != QStringLiteral("下载任务") && hasText(downloads),
                  qPrintable(QStringLiteral("the interface switches to %1").arg(code)));
        }
        language.choose("en");
        settle(50);
        check(hasText(QStringLiteral("This Season")) && controller.contextTitle() == QStringLiteral("This Season"),
              "English replaces the navigation and the page title");
        check(QCoreApplication::translate("Miku", "共 %1 个任务").arg(3) == QStringLiteral("3 tasks"), "texts built in C++ follow too");
        language.choose("zh-CN");
        settle(50);
        check(hasText(QStringLiteral("本季放送")) && controller.contextTitle() == QStringLiteral("本季放送"), "Simplified Chinese comes back");
        config.language = "zh-CN";
    }
    {
        auto *circle = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "closeCircle"; });
        check(circle && qFuzzyCompare(circle->property("radius").toReal(), circle->width() / 2), "the close button is a circle");
        if (circle) {
            const QPointF centre = circle->mapToScene(QPointF(circle->width() / 2, circle->height() / 2));
            check(qAbs(centre.x() - (window->width() - 24)) < 1 && qAbs(centre.y() - 24) < 1, "the close circle shares its centre with the window corner");
        }
    }
    check(window->rendererInterface()->graphicsApi() != QSGRendererInterface::Software, "the interface renders on the GPU");
    settle(300);

    // Every page loads without a warning.
    for (int page : {1, 2, 3, 4, 5, 0}) {
        controller.selectPage(page);
        settle(280);
    }
    check(controller.page() == 0 && controller.contextTitle() == QStringLiteral("本季放送"), "navigation updates the title");
    AnimeSubject subject;
    subject.bangumiId = 7;
    subject.nameCn = QStringLiteral("测试");
    subject.aliases = {"Test"};
    subject.summary = QString(200, QChar('x'));
    controller.detail()->showSubject(subject);
    settle(100);

    {
        // Each dialog opens over the window and closes on its answer.
        auto *hub = controller.dialogs();
        QString answered;
        hub->message("t", "text");
        settle(220);
        check(!hub->current().isEmpty(), "a notice stays until it is answered");
        hub->message("second", "queued");
        hub->answer("");
        check(hub->current().value("title") == "second", "notices queue instead of stacking");
        hub->answer("");
        settle(220);
        hub->confirmRemoval([&](const QString &choice, bool remember) { answered = choice + (remember ? "+" : ""); });
        settle(220);
        hub->answer("keep", true);
        check(answered == "keep+", "the removal question reports the choice and remember");
        hub->askClose(2, [&](const QString &choice, bool) { answered = choice; });
        check(hub->current().value("text").toString().contains(QStringLiteral("2 个任务")), "the close prompt mentions running downloads");
        settle(220);
        hub->answer("cancel");
        hub->chooseVideo({"b/2.mkv", "b/10.mkv", "b/1.mkv"}, [&](const QString &path) { answered = path; });
        const auto videos = hub->current().value("videos").toList();
        check(videos.size() == 3 && videos.at(1).toMap().value("name") == "2.mkv", "videos sort by episode number");
        settle(220);
        hub->answer("b/1.mkv");
        check(answered == "b/1.mkv", "the chosen video comes back");
        answered.clear();
        hub->chooseVideo({"only.mkv"}, [&](const QString &path) { answered = path; });
        check(answered == "only.mkv" && hub->current().isEmpty(), "a single video needs no question");
        hub->toast("done");
        settle(120);
    }

    {
        // Closing asks through the dialog hub; the window stays until it is answered.
        controller.requestClose();
        check(controller.dialogs()->current().value("kind") == "close", "closing the window asks first");
        settle(220);
        controller.dialogs()->answer("cancel");
        check(window->isVisible(), "cancelling keeps the window open");
        settle(320);
        bool sheetShowing = false;
        for (auto *item : window->contentItem()->childItems())
            if (QByteArray(item->metaObject()->className()).contains("Overlay"))
                for (auto *child : item->childItems())
                    sheetShowing = sheetShowing || QByteArray(child->metaObject()->className()).contains("PopupItem");
        check(!sheetShowing, "an answered dialog leaves the screen");
    }

    {
        // Theme: the palette flips only after the old look is on screen as a still.
        // Sampled on a page without posters from the network, which may or may not have loaded.
        controller.selectPage(3);
        settle(400);
        QObject *fade = nullptr;
        for (auto *item : window->contentItem()->childItems())
            if (QByteArray(item->metaObject()->className()).contains("ThemeFade")) fade = item;
        check(fade != nullptr, "the theme fade layer exists");
        int flips = 0;
        QObject::connect(&theme, &ThemeController::darkChanged, &app, [&] { ++flips; });
        const bool before = theme.dark();
        theme.setMode(before ? "light" : "dark");
        if (theme.animated()) {
            check(theme.dark() == before, "the palette waits for the still of the old look");
            check(waitFor([&] { return theme.dark() != before; }, 1000), "the palette flips once the still is on screen");
            check(fade && fade->property("opacity").toReal() > 0.5, "the still covers the window when the palette flips");
        } else {
            // With Windows' animation effects off (as on CI machines) the switch is instant.
            check(theme.dark() != before && fade && !fade->property("visible").toBool(),
                  "without animation effects the palette flips at once, with no fade");
        }
        const QColor mid = window->grabWindow().pixelColor(window->width() / 2, window->height() - 30);
        settle(600);
        const QColor end = window->grabWindow().pixelColor(window->width() / 2, window->height() - 30);
        check(fade && !fade->property("visible").toBool(), "the fade removes itself");
        check(mid.isValid() && end.isValid() && (theme.dark() ? end.lightness() < 80 : end.lightness() > 180), "the window ends in the new palette");
        check(flips == 1, "one switch flips the palette once");

        const QStringList modes = {"light", "dark"};
        for (int i = 0; i < 12; ++i) {
            theme.setMode(modes.at(i % 2));
            settle(35);
        }
        settle(700);
        const QColor settled = window->grabWindow().pixelColor(window->width() / 2, window->height() - 30);
        check(theme.dark() == (theme.mode() == "dark") && (theme.dark() ? settled.lightness() < 80 : settled.lightness() > 180), "rapid switching ends consistent");
        check(fade && !fade->property("visible").toBool(), "rapid switching leaves nothing behind");
        controller.selectPage(0);
        settle(300);
    }

#ifdef Q_OS_WIN
    {
        // The window keeps native edges and stays inside the work area when maximised.
        const auto hwnd = HWND(window->winId());
        check(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_THICKFRAME, "the window keeps a resizable frame");
        RECT rect;
        GetWindowRect(hwnd, &rect);
        const auto hit = [&](int x, int y) { return SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(x, y)); };
        check(hit(rect.left + 2, (rect.top + rect.bottom) / 2) == HTLEFT, "the left edge resizes");
        check(hit(rect.right - 3, rect.bottom - 3) == HTBOTTOMRIGHT, "the corner resizes");
        check(hit((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2) == HTCLIENT, "the content is client area");
        window->showMaximized();
        settle(300);
        MONITORINFO monitor = {sizeof(MONITORINFO)};
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
        RECT client;
        GetClientRect(hwnd, &client);
        POINT origin = {0, 0};
        ClientToScreen(hwnd, &origin);
        check(origin.y >= monitor.rcWork.top && origin.y + client.bottom <= monitor.rcWork.bottom, "maximised, the window stays off the taskbar");
        window->showNormal();
        settle(300);
    }
#elif defined(Q_OS_MACOS)
    {
        // The system's title bar, transparent: its buttons, nothing drawn over them.
        check(window->flags().testFlag(Qt::NoTitleBarBackgroundHint) && !window->flags().testFlag(Qt::FramelessWindowHint),
              "the window keeps the system title bar for its buttons");
        auto *buttons = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("WindowButtons"); });
        check(buttons && !buttons->isVisible(), "the drawn window buttons give way to the system's");
    }
#else
    {
        // A frameless window that the interface lets the user resize from every edge.
        check(window->flags().testFlag(Qt::FramelessWindowHint), "the window draws its own frame");
        auto *left = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "resizeLeft"; });
        auto *corner = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "resizeCorner"; });
        check(left && left->isVisible() && left->mapToScene(QPointF(0, 0)).x() == 0, "the left edge resizes");
        check(corner && corner->isVisible() && qAbs(corner->mapToScene(QPointF(corner->width(), corner->height())).x() - window->width()) < 0.5,
              "the corner resizes");
        window->showMaximized();
        settle(300);
        check(!left || !left->isVisible(), "a maximised window has no resize edges");
        window->showNormal();
        settle(300);
    }
#endif

    {
        // Enter during IME composition is not a search.
        controller.selectPage(AppController::kSearchPage);
        settle(300);
        auto *field = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("NxField") && item->isVisible() && item->height() >= 40; });
        check(field != nullptr, "the search field exists");
        if (field) {
            field->forceActiveFocus();
            int queries = 0;
            QObject::connect(controller.search(), &SearchController::queryChanged, &app, [&] { ++queries; });
            QInputMethodEvent composing("zhong", {});
            QCoreApplication::sendEvent(field, &composing);
            check(field->property("inputMethodComposing").toBool(), "the field reports composition");
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(field, &press);
            check(queries == 0, "enter during composition does not search");
            QInputMethodEvent commit;
            commit.setCommitString(QStringLiteral("中"));
            QCoreApplication::sendEvent(field, &commit);
            check(field->property("text").toString() == QStringLiteral("中"), "the candidate is committed");
            QCoreApplication::sendEvent(field, &press);
            check(queries == 0, "the enter confirming a candidate does not search");
            settle(90);
            QCoreApplication::sendEvent(field, &press);
            check(controller.search()->query() == QStringLiteral("中"), "a later enter searches");
            controller.search()->reset();
        }
    }

    {
        // Whatever the window size, posters fill the width.
        controller.selectPage(0);
        settle(300);
        auto *grid = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PosterGrid") && item->isVisible(); });
        check(grid != nullptr, "the home page has a poster grid");
        auto fills = [&](int width) {
            window->resize(width, 760);
            settle(320);
            const qreal available = grid->property("available").toReal(), card = grid->property("cardWidth").toReal();
            const int columns = grid->property("columns").toInt();
            const qreal gap = grid->property("gap").toReal();
            return columns * card + (columns - 1) * gap > available - columns;
        };
        bool everyWidth = grid != nullptr;
        if (grid)
            for (const int width : {900, 1100, 1320, 1700, 2400})
                everyWidth = everyWidth && fills(width);
        check(everyWidth, "posters fill the width at every window size");
        if (grid) {
            // Narrowing the window never brings a column back.
            int previous = 1 << 20;
            bool fewer = true;
            for (int width = 1400; width >= 900; width -= 20) {
                window->resize(width, 760);
                settle(30);
                const int columns = grid->property("columns").toInt();
                fewer = fewer && columns <= previous;
                previous = columns;
            }
            check(fewer, "a narrowing window never gains a column");
        }
        // The home page holds whatever the network returned, sometimes nothing, so the
        // scrolling is tested on the search page with a grid of known length.
        QVector<AnimeSubject> many;
        for (int id = 1; id <= 24; ++id) {
            AnimeSubject item = subject;
            item.bangumiId = id;
            many.append(item);
        }
        controller.search()->setSuggestions(many);
        controller.selectPage(1);
        settle(500);
        auto *searchGrid = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PosterGrid") && item->isVisible(); });
        check(searchGrid != nullptr, "the search page shows its suggestions");
        if (searchGrid) {
            // Wheeling back up returns to the very top, past the toolbar band.
            auto *wheel = findItem(searchGrid, [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("SmoothWheel"); });
            auto *view = wheel ? wheel->property("view").value<QQuickItem *>() : nullptr;
            check(view != nullptr, "the poster grid scrolls with the wheel");
            if (view) {
                window->resize(1100, 620);
                settle(320);
                const qreal top = view->property("contentY").toReal();
                QMetaObject::invokeMethod(wheel, "scrollBy", Q_ARG(QVariant, 400));
                // The glide runs on rendered frames, which a busy machine delays.
                for (int waited = 0; waited < 2000 && view->property("contentY").toReal() <= top + 100; waited += 50)
                    settle(50);
                check(view->property("contentY").toReal() > top + 100, "the wheel scrolls the posters");
                QMetaObject::invokeMethod(wheel, "scrollBy", Q_ARG(QVariant, -5000));
                settle(320);
                check(qAbs(view->property("contentY").toReal() - top) < 1, "the wheel returns to the very top");
                {
                    auto *bar = findItem(searchGrid, [](QQuickItem *item) { return item->property("windowEdge").toBool(); });
                    auto *thumb = bar ? bar->property("contentItem").value<QQuickItem *>() : nullptr;
                    check(thumb != nullptr, "a list filling the window has its scroll bar");
                    if (thumb) {
                        check(thumb->mapToScene(QPointF(0, 0)).y() >= 48, "the scroll bar starts below the window controls");
                        check(thumb->mapToScene(QPointF(thumb->width(), 0)).x() <= window->width() - 3, "the scroll bar stays inside the window");
                    }
                }
                // A real wheel message from Windows, so the whole way to the handler counts:
                // calling scrollBy alone passed while no wheel event ever reached it.
#ifdef Q_OS_WIN
                const QPointF over = window->mapToGlobal(QPointF(700, 400)) * window->devicePixelRatio();
                PostMessageW(HWND(window->winId()), WM_MOUSEWHEEL, MAKEWPARAM(0, WORD(SHORT(-360))), MAKELPARAM(int(over.x()), int(over.y())));
#else
                {
                    // The window system's own wheel event, delivered the way Qt delivers one.
                    const QPointF over(700, 400);
                    QWheelEvent wheel(over, window->mapToGlobal(over), QPoint(), QPoint(0, -360), Qt::NoButton, Qt::NoModifier,
                                      Qt::NoScrollPhase, false);
                    QCoreApplication::sendEvent(window, &wheel);
                }
#endif
                for (int waited = 0; waited < 2000 && view->property("contentY").toReal() <= top + 100; waited += 50)
                    settle(50);
                check(view->property("contentY").toReal() > top + 100, "a wheel event over the posters scrolls them");
                {
                    settle(250);
                    auto *searchPage = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("SearchPage"); });
                    check(searchPage && searchPage->property("lift").toReal() > 100, "the title leaves with the content");
                    auto *backing = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "captionBacking"; });
                    check(backing && backing->isVisible(), "the window controls stay readable over the posters");
                }
                QMetaObject::invokeMethod(wheel, "scrollBy", Q_ARG(QVariant, -5000));
                settle(320);
                {
                    auto *backing = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "captionBacking"; });
                    check(backing && !backing->isVisible(), "back at the top the window controls need no backing");
                }
                window->resize(1100, 1200);
                settle(320);
                check(qAbs(view->property("contentY").toReal() - view->property("originY").toReal() + view->property("topMargin").toReal()) < 1,
                      "the posters stay at the top when the title band grows");
            }
        }
        controller.search()->setSuggestions({});
        controller.selectPage(0);
        settle(320);
        auto *rootItem = qobject_cast<QObject *>(window);
        auto *navBar = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "navBar"; });
        check(navBar && navBar->isVisible(), "navigation floats at the bottom of the window");
        if (navBar) {
            // Centred, clear of the edges, and the pages run on beneath it.
            const QPointF topLeft = navBar->mapToScene(QPointF(0, 0));
            check(qAbs(topLeft.x() + navBar->width() / 2 - window->width() / 2.0) < 1 && qAbs(window->height() - (topLeft.y() + navBar->height()) - 14) < 1,
                  "the navigation bar sits centred above the bottom edge");
            auto *grid = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PosterGrid") && item->isVisible(); });
            check(grid && grid->mapToScene(QPointF(0, grid->height())).y() >= window->height() - 1, "the posters run on beneath the navigation bar");
            {
                // The glass blurs exactly what lies beneath it.
                auto *behind = findItem(navBar, [](QQuickItem *item) { return item->objectName() == "glassBehind"; });
                const QRectF sampled = behind ? behind->property("sourceRect").toRectF() : QRectF();
                const QPointF at = navBar->mapToScene(QPointF(0, 0));
                const QString seen = QStringLiteral("sampled %1,%2 %3x%4 bar %5,%6 %7x%8 source %9")
                    .arg(sampled.x()).arg(sampled.y()).arg(sampled.width()).arg(sampled.height())
                    .arg(at.x()).arg(at.y()).arg(navBar->width()).arg(navBar->height())
                    .arg(behind && behind->property("sourceItem").value<QQuickItem *>() ? "set" : "none");
                check(behind && qAbs(sampled.x() - at.x()) < 1 && qAbs(sampled.y() - at.y()) < 1
                          && qAbs(sampled.width() - navBar->width()) < 1 && behind->property("sourceItem").value<QQuickItem *>(),
                      ("the navigation bar blurs what lies beneath it (" + seen + ")").toUtf8().constData());
                // And the blur is really drawn: with the buttons hidden, the bar is far
                // smoother than the same stretch of posters just above it.
                auto *blur = findItem(navBar, [](QQuickItem *item) { return item->objectName() == "glassBlur"; });
                auto *row = findItem(navBar, [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("QQuickRow"); });
                auto *pages = navBar->property("backdrop").value<QQuickItem *>();
                if (blur && blur->isVisible() && row && pages) {
                    // Fine black and white stripes in the page layer, under the bar and above it.
                    QQmlComponent stripes(&engine);
                    stripes.setData("import QtQuick\nRow { z: 1000; Repeater { model: 80; Rectangle { width: 12; height: 200; color: index % 2 ? 'black' : 'white' } } }", QUrl());
                    auto *pattern = qobject_cast<QQuickItem *>(stripes.create());
                    if (pattern) {
                        pattern->setParentItem(pages);
                        pattern->setPosition(QPointF(at.x(), at.y() + navBar->height() - 200));
                    }
                    row->setOpacity(0);
                    settle(400);
                    const QImage shot = window->grabWindow();
                    row->setOpacity(1);
                    delete pattern;
                    const qreal dpr = shot.devicePixelRatio();
                    const QRect bar = QRectF(at.x() + navBar->height() / 2, at.y() + 10, navBar->width() - navBar->height(), navBar->height() - 20).toRect();
                    const auto roughness = [&](QRect area) {
                        double total = 0;
                        const QRect px(QPoint(qRound(area.x() * dpr), qRound(area.y() * dpr)), QSize(qRound(area.width() * dpr), qRound(area.height() * dpr)));
                        for (int y = px.top(); y <= px.bottom(); ++y)
                            for (int x = px.left() + 1; x <= px.right(); ++x)
                                total += qAbs(qGray(shot.pixel(x, y)) - qGray(shot.pixel(x - 1, y)));
                        return total / qMax(1, px.width() * px.height());
                    };
                    const double inside = roughness(bar), above = roughness(bar.translated(0, -bar.height() - 40));
                    check(inside < above * 0.5, QStringLiteral("the navigation bar's blur is drawn (roughness %1 inside, %2 above)").arg(inside, 0, 'f', 2).arg(above, 0, 'f', 2).toUtf8().constData());
                }
            }
            auto *gridView = grid ? findItem(grid, [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("GridView"); }) : nullptr;
            check(gridView && gridView->property("bottomMargin").toReal() >= navBar->height() + 28, "the last row of posters ends above it");
            // At every width the bar fits, and it drops the names only when they would not.
            bool fits = true, namesWhenRoom = true;
            for (const int width : {900, 1100, 1320}) {
                window->resize(width, 760);
                settle(200);
                fits = fits && navBar->width() <= window->width() - 48;
                namesWhenRoom = namesWhenRoom && navBar->property("compact").toBool() == (navBar->property("fullWidth").toReal() > window->width() - 48);
            }
            check(fits, "the navigation bar fits the window at every width");
            check(namesWhenRoom, "the navigation bar keeps the names while they fit");
            window->resize(1320, 860);
            // Away from the bar: the real pointer may be resting where the window put it.
            QTest::mouseMove(window, QPoint(40, 200));
            settle(700);
            check(!navBar->property("compact").toBool(), "a wide window has room for the names");
            check(navBar->property("reveal").toReal() == 0 && navBar->width() < navBar->property("fullWidth").toReal() - 1,
                  "at rest the navigation bar shows only its icons");
            const auto pointAt = [&](QPointF scene) {
                QMouseEvent move(QEvent::MouseMove, scene, scene, window->mapToGlobal(scene), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &move);
            };
            auto *middleButton = findItem(navBar, [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("NavButton") && item->property("page").toInt() == 2; });
            const auto middleCentre = [&] { return middleButton ? middleButton->mapToScene(QPointF(middleButton->width() / 2, 0)).x() : -1.0; };
            const qreal middleAtRest = middleCentre();
            pointAt(navBar->mapToScene(QPointF(navBar->width() / 2, navBar->height() / 2)));
            settle(450);
            check(middleButton && qAbs(middleCentre() - middleAtRest) < 1,
                  QStringLiteral("the bar opens around its middle item (moved %1)").arg(middleCentre() - middleAtRest).toUtf8().constData());
            check(navBar->property("reveal").toReal() == 1 && qAbs(navBar->width() - navBar->property("fullWidth").toReal()) < 1,
                  "pointing at the navigation bar opens the names");
            pointAt(QPointF(window->width() / 2.0, 200));
            settle(800);
            check(navBar->property("reveal").toReal() == 0, "the names close again when the pointer leaves");

            // The logo leads home, from the keyboard as from the pointer.
            auto *home = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "homeButton"; });
            controller.selectPage(3);
            settle(300);
            if (home) {
                home->forceActiveFocus();
                settle(50);
                check(navBar->property("expanded").toBool(), "moving the keyboard into the bar opens the names");
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(home, &enter);
                settle(300);
            }
            check(home && controller.page() == 0, "the logo returns to the home page");
            {
                // A click on a button stays in the bar: the posters run on beneath it.
                window->contentItem()->forceActiveFocus();
                settle(400);
                auto *settingsButton = findItem(navBar, [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("NavButton") && item->property("page").toInt() == 5; });
                if (settingsButton) {
                    const QPointF centre = settingsButton->mapToScene(QPointF(settingsButton->width() / 2, settingsButton->height() / 2));
                    pointAt(centre);
                    settle(450);
                    const QPointF target = settingsButton->mapToScene(QPointF(settingsButton->width() / 2, settingsButton->height() / 2));
                    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, target.toPoint());

                    settle(600);
                }
                check(settingsButton && controller.page() == 5, QStringLiteral("clicking a button in the bar opens its page and nothing beneath it (page %1)").arg(controller.page()).toUtf8().constData());
                controller.selectPage(0);
                pointAt(QPointF(window->width() / 2.0, 200));
                settle(800);
            }
            window->contentItem()->forceActiveFocus();
            settle(700);
        }
        {
            // The see-through background, where the system offers it (Windows 11 22H2 on).
            auto *background = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "windowBackground"; });
            const QColor tint = background ? background->property("color").value<QColor>() : QColor();
#ifdef Q_OS_WIN
            const bool acrylic = QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0, 22621);
            check(chrome.translucent() == (acrylic && theme.transparency()), "the desktop shows through where Windows can blur it");
#endif
            check(background && (chrome.translucent() ? tint.alphaF() < 1 : tint.alphaF() == 1), "the background is a tint over the blur, or solid without one");
        }
        {
            // A detail page keeps the navigation, with nothing in it selected.
            emit controller.home()->animeSelected(subject);
            settle(500);
            auto *thumb = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "navThumb"; });
            check(thumb && !thumb->isVisible(), "a detail page selects nothing in the navigation");
            {
                // The artwork and the text under it share one right edge.
                auto *art = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("HeroPoster") && item->isVisible(); });
                auto *infoColumn = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("HeroInfo") && item->isVisible(); });
                auto *side = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "detailSide" && item->isVisible(); });
                const bool sideScrolls = side && side->property("scrolls").toBool();
                check(art && infoColumn && art->width() <= infoColumn->width() + 1
                          && (qAbs(art->width() - infoColumn->width()) < 1 || !sideScrolls),
                      "the poster is as wide as the text under it, or smaller so the column fits");
                check(side && !sideScrolls, QStringLiteral("the side column stays put while the summary is folded (window %1 high)").arg(window->height()).toUtf8().constData());

                // The summary's toggle is a button beside the others, and only when it is needed.
                AnimeSubject wordy = subject;
                wordy.summary = QString(1800, QChar('x'));
                emit controller.home()->animeSelected(wordy);
                settle(300);
                auto *more = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "summaryMore" && item->isVisible(); });
                check(more && more->property("text").toString() == QStringLiteral("更多"), "a long summary offers a button to read on");
                if (more) {
                    QMetaObject::invokeMethod(more, "clicked");
                    settle(250);
                    check(more->property("text").toString() == QStringLiteral("收起"), "the button folds the summary again");
                    auto *openSide = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "detailSide" && item->isVisible(); });
                    check(openSide && openSide->property("scrolls").toBool(), "an opened long summary lets the side column scroll");
                    QMetaObject::invokeMethod(more, "clicked");
                    settle(250);
                }
                AnimeSubject brief = subject;
                brief.summary = QStringLiteral("简短的简介。\n\n");
                emit controller.home()->animeSelected(brief);
                settle(300);
                check(findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "summaryMore" && item->isVisible(); }) == nullptr, "a short summary has no such button");
                emit controller.home()->animeSelected(subject);
                settle(300);
            }
            {
                // An anime with no videos offers the search on the empty page, and only there.
                const auto buttonsReading = [&](const QString &label) {
                    int found = 0;
                    std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
                        if (item->isVisible() && QByteArray(item->metaObject()->className()).contains("NxButton") && item->property("text").toString() == label)
                            ++found;
                        for (auto *child : item->childItems())
                            walk(child);
                    };
                    walk(window->contentItem());
                    return found;
                };
                controller.detail()->showSubject(subject, true);
                settle(350);
                auto *offer = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "emptyAction" && item->isVisible(); });
                check(offer && offer->property("text").toString() == QStringLiteral("查找可下载资源"), "the empty page offers to find resources");
                check(buttonsReading(QStringLiteral("查找可下载资源")) == 1, "the offer appears once, not in the toolbar too");
                check(!controller.detail()->emptyText().contains(QStringLiteral("右上角")), "the text no longer points at the toolbar");
                if (offer) {
                    QMetaObject::invokeMethod(offer, "clicked");
                    settle(350);
                    check(!controller.detail()->localOnly(), "the offer opens the downloadable resources");
                }
                // The offer goes by way of the search page; come back from there.
                controller.selectPage(0);
                controller.search()->reset();
                settle(200);
                emit controller.home()->animeSelected(subject);
                settle(300);
            }
            {
                // The built-in player, on a picture mpv generates itself.
                auto *player = controller.player();
                check(player->available(), "the built-in player is available");
                auto *playingNav = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("NavButton") && item->property("page").toInt() == 7; });
                check(playingNav != nullptr, "the navigation has a now-playing item");
                check(player->open(QStringLiteral("av://lavfi:testsrc=size=640x360:rate=25:duration=30"), QStringLiteral("测试视频"), QStringLiteral("第一集")), "a video opens");
                controller.selectPage(7);
                bool started = false;
                for (int waited = 0; waited < 8000 && !started; waited += 100) {
                    settle(100);
                    started = player->position() > 0.5;
                }
                check(started, "playback runs");
                {
                    auto *upscale = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "playerUpscale" && item->isVisible(); });
                    auto *label = upscale ? upscale->property("contentItem").value<QQuickItem *>() : nullptr;
                    check(label && label->property("implicitWidth").toReal() <= label->width() + 0.5, "the upscale choice is never cut short");
                }
                auto *video = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "playerVideo" && item->isVisible(); });
                check(video != nullptr, "the picture is on the page");
                {
                    auto *bar = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "navBar"; });
                    check(video && bar && video->mapToScene(QPointF(0, video->height())).y() <= bar->mapToScene(QPointF(0, 0)).y(),
                          "the picture and its controls end above the navigation bar");
                }
                if (video) {
                    settle(400);
                    const QImage shot = window->grabWindow();
                    const qreal ratio = shot.width() / qreal(window->width());
                    const QPointF middle = video->mapToScene(QPointF(video->width() * 0.3, video->height() * 0.3));
                    const QColor pixel = shot.pixelColor(int(middle.x() * ratio), int(middle.y() * ratio));
                    check(pixel.lightness() > 25, "the picture is drawn, not black");
                    // testsrc runs a rainbow gradient across the lower part of the frame; the
                    // colour bars mirrored there would show only a few hues. The picture is
                    // 16:9, fitted into the item.
                    const qreal pw = qMin(video->width(), video->height() * 16 / 9), ph = pw * 9 / 16;
                    const auto hues = [&](qreal row) {
                        QSet<int> seen;
                        for (int i = 0; i < 48; ++i) {
                            const QPointF at = video->mapToScene(QPointF((video->width() - pw) / 2 + pw * (i + 0.5) / 48,
                                                                         (video->height() - ph) / 2 + ph * row));
                            const int hue = shot.pixelColor(int(at.x() * ratio), int(at.y() * ratio)).hsvHue();
                            if (hue >= 0) seen.insert(hue / 15);
                        }
                        return seen.size();
                    };
                    check(hues(0.81) > hues(0.19) + 4, "the picture is the right way up");
                }
                player->setUpscale(1);
                check(player->shaderList().contains(QStringLiteral("Upscale_CNN_x2_M")), "fast upscaling loads its shaders");
                settle(900);
                const qreal upscaling = player->position();
                settle(900);
                check(player->position() > upscaling, "playback goes on with upscaling");
                player->setInterpolation(1);
                check(player->interpolation() == 1 && player->interpolationState().startsWith(QStringLiteral("display-resample yes")),
                      "smooth interpolation paces frames to the display and blends them");
                settle(900);
                const qreal interpolating = player->position();
                settle(900);
                check(player->position() > interpolating, "playback goes on with interpolation");
                // Asking is not enough: mpv only blends once it has locked onto the screen's
                // rate, which needs the refresh rate and the swap reports from the video item.
                check(player->interpolationState().endsWith(QStringLiteral(" yes")), "mpv is in step with the display while interpolating");
                if (!player->motionAvailable()) {
                    player->setInterpolation(2);
                    check(player->interpolation() == 1, "motion interpolation is only offered with VapourSynth");
                }
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
                // Windows and macOS programs carry VapourSynth and MVTools themselves, nothing to
                // install; on macOS from 15 on, which MVTools is built for, and once CI has put
                // the set into the build folder.
#ifdef Q_OS_MACOS
                const bool carried = QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 15)
                                     && QFile::exists(QCoreApplication::applicationDirPath() + "/vapoursynth/python/bin/python3");
#else
                const bool carried = true;
#endif
                if (carried)
                    check(player->motionAvailable(), "motion interpolation works out of the box");
                if (carried && player->motionAvailable()) {
                    player->setInterpolation(2);
                    check(player->interpolation() == 2 && player->interpolationState().contains(QStringLiteral("vapoursynth")),
                          "motion interpolation runs the VapourSynth filter");
                    settle(3000);
                    const qreal motion = player->position();
                    settle(900);
                    check(player->interpolation() == 2 && player->position() > motion, "playback goes on with motion interpolation");
                    // The 25 fps test picture leaves MVTools with in-between frames added.
                    check(player->filteredFps() > 25 * 1.2, "MVTools adds frames in between");
                    report.write("    motion interpolation output: " + QByteArray::number(player->filteredFps(), 'f', 1) + " fps\n");
                }
#endif
                player->setInterpolation(0);
                check(player->interpolationState().startsWith(QStringLiteral("audio no")), "turning interpolation off goes back to audio-timed playback");
                if (video) {
                    // On a held frame the only thing that can change the picture is the shaders.
                    player->togglePause();
                    settle(500);
                    const auto frame = [&] { return window->grabWindow(); };
                    const QImage upscaled = frame();
                    player->setUpscale(0);
                    settle(900);
                    const QImage plain = frame();
                    player->togglePause();
                    const qreal scale = plain.width() / qreal(window->width());
                    // Over the whole picture: the shaders work on edges, and a small patch can
                    // fall entirely inside one flat colour bar.
                    qint64 difference = 0;
                    for (int y = 0; y < 100; ++y)
                        for (int x = 0; x < 150; ++x) {
                            const QPointF scene = video->mapToScene(QPointF(video->width() * (x + 0.5) / 150, video->height() * (y + 0.5) / 100));
                            const QPoint at(int(scene.x() * scale), int(scene.y() * scale));
                            difference += qAbs(qGray(upscaled.pixel(at)) - qGray(plain.pixel(at)));
                        }
                    check(difference > 3000, "the upscaling shaders change the picture");
                    player->setUpscale(1);
                }
                player->setUpscale(2);
                check(player->shaderList().contains(QStringLiteral("Upscale_CNN_x2_VL")), "high quality upscaling loads its shaders");
                settle(600);
                player->setUpscale(0);
                check(player->shaderList().isEmpty(), "upscaling can be turned off");
                player->togglePause();
                settle(200);
                check(player->paused(), "playback pauses");
                player->togglePause();
                controller.selectPage(0);
                settle(600);
                check(!player->videoVisible() && player->active(), "leaving the page keeps it playing without the picture");
                controller.selectPage(7);
                settle(500);
                check(player->videoVisible(), "coming back brings the picture back");
                {
                    // Three seconds without moving the pointer hide both bars and the pointer;
                    // a movement brings them back. A shorter wait keeps the test quick.
                    auto *idle = rootItem->findChild<QObject *>(QStringLiteral("playerIdle"));
                    auto *controls = rootItem->findChild<QQuickItem *>(QStringLiteral("playerControls"));
                    auto *nav = rootItem->findChild<QQuickItem *>(QStringLiteral("navBar"));
                    auto *picture = rootItem->findChild<QQuickItem *>(QStringLiteral("playerVideo"));
                    check(idle && idle->property("interval").toInt() == 3000, "the player rests after three seconds");
                    if (idle && controls && nav && picture) {
                        idle->setProperty("interval", 400);
                        const QPoint middle = picture->mapToScene(QPointF(picture->width() / 2, picture->height() / 3)).toPoint();
                        QTest::mouseMove(window, middle);
                        settle(200);
                        check(!player->paused() && controls->opacity() > 0.9 && nav->opacity() > 0.9
                                  && window->cursor().shape() != Qt::BlankCursor,
                              "moving the pointer over the picture shows the controls, the navigation and the pointer");
                        // The video keeps drawing frames while the pointer stands still.
                        settle(1200);
                        check(controls->opacity() < 0.01 && !controls->isVisible(), "the controls hide while the pointer rests");
                        check(nav->opacity() < 0.01 && rootItem->property("playerResting").toBool(), "the navigation hides with them");
                        check(window->cursor().shape() == Qt::BlankCursor, "so does the pointer");
                        QTest::mouseMove(window, middle + QPoint(30, 10));
                        settle(250);
                        check(controls->opacity() > 0.9 && nav->opacity() > 0.9 && !rootItem->property("playerResting").toBool()
                                  && window->cursor().shape() != Qt::BlankCursor,
                              "a movement brings them all back");
                        // Moving onto the hidden navigation brings it back too.
                        settle(1200);
                        QTest::mouseMove(window, nav->mapToScene(QPointF(nav->width() / 2, nav->height() / 2)).toPoint());
                        settle(250);
                        check(nav->opacity() > 0.9 && controls->opacity() > 0.9, "pointing at the hidden navigation brings it back");
                        settle(1200);
                        check(nav->opacity() > 0.9, "and it stays while the pointer is on it");
                        QTest::mouseMove(window, middle);
                        idle->setProperty("interval", 3000);
                    }
                }
                {
                    // Full screen gives the whole window to the picture and gives it back.
                    auto *playerPage = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PlayerPage") && item->isVisible(); });
                    check(playerPage != nullptr, "the player page is on screen");
                    if (playerPage) {
                        QMetaObject::invokeMethod(playerPage, "toggleFull");
                        settle(600);
                        check(window->visibility() == QWindow::FullScreen && rootItem->property("immersive").toBool(), "full screen hides the navigation");
                        QMetaObject::invokeMethod(playerPage, "toggleFull");
                        settle(600);
                        check(window->visibility() != QWindow::FullScreen && !rootItem->property("immersive").toBool(), "leaving full screen brings it back");
                    }
                }
                player->stop();
                settle(300);
                check(!player->active(), "playback stops");
                controller.selectPage(0);
                settle(150);
                emit controller.home()->animeSelected(subject);
                settle(300);
            }
            check(controller.startedAgain(), "starting the app again runs the checks");
            check(!controller.startedAgain(), "starting it again at once does not repeat them");
            {
                auto *back = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "detailBack" && item->isVisible(); });
                check(back && back->property("text").toString() == QStringLiteral("本季放送"), "the back button names the page it returns to");
            }
            {
                // Opening it needs an active window, so the margin that keeps it on screen is checked.
                auto *sort = findItem(window->contentItem(), [](QQuickItem *item) { return item->property("tip").toString() == QStringLiteral("排序") && item->isVisible(); });
                auto *popup = sort ? sort->property("popup").value<QObject *>() : nullptr;
                check(popup && popup->property("rightMargin").toReal() >= 8, "a list at the window's right edge stays inside it");
            }
            {
                // The switcher sits with the filters and takes the whole page along.
                auto selector = [&] {
                    return findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "seriesSelect" && item->isVisible(); });
                };
                check(selector() == nullptr, "an anime without other seasons shows no switcher");
                AnimeSubject arc = subject;
                arc.bangumiId = 8;
                arc.nameCn = QStringLiteral("测试 黄昏篇");
                controller.detail()->applySeries({arc});
                settle(200);
                auto *select = selector();
                check(select && select->property("displayText").toString() == QStringLiteral("测试"), "the switcher shows the entry on screen");
                if (select) {
                    QMetaObject::invokeMethod(select, "chosen", Q_ARG(QVariant, QVariant(8)));
                    settle(300);
                    check(controller.detail()->hero().value("subjectId") == 8 && controller.page() == 6, "choosing an entry swaps the details");
                    check(select->property("displayText").toString() == QStringLiteral("黄昏篇"), "the switcher follows the page");
                }
            }
            controller.closeDetail();
            settle(500);
            {
                // The selection lens follows the page, and the keyboard can use the bar.
                auto *navItem = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("NavButton") && item->property("page").toInt() == 3; });
                auto *thumb = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "navThumb"; });
                check(navItem != nullptr && thumb != nullptr, "the navigation bar has its buttons");
                if (navItem && thumb) {
                    const int pageBefore = controller.page();
                    navItem->forceActiveFocus();
                    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    QCoreApplication::sendEvent(navItem, &enter);
                    settle(600);
                    check(controller.page() == 3, "the keyboard can use the navigation bar");
                    check(qAbs(thumb->mapToScene(QPointF(0, 0)).x() - navItem->mapToScene(QPointF(0, 0)).x()) < 1 && qAbs(thumb->width() - navItem->width()) < 1,
                          "the selection slides to the chosen page");
                    controller.selectPage(pageBefore);
                    window->contentItem()->forceActiveFocus();
                    settle(600);
                }
            }
            {
                auto *grid = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PosterGrid") && item->isVisible(); });
                check(grid != nullptr, "a poster grid is on screen for the size checks");
                if (grid) {
                    const qreal rest = grid->property("cardWidth").toReal();
                    check(qFuzzyCompare(rest, qRound(rest) + 0.0), "posters rest on whole pixels");

                    // Dragging the window leaves the decoded covers alone until it stops.
                    settle(400);
                    const int decoded = grid->property("decodeWidth").toInt();
                    bool steady = true;
                    for (int width = 1320; width <= 1560; width += 40) {
                        window->resize(width, 860);
                        settle(40);
                        steady = steady && grid->property("decodeWidth").toInt() == decoded;
                    }
                    check(steady, "dragging the window does not reload the covers");
                    settle(500);
                    check(grid->property("decodeWidth").toInt() % 40 == 0 && grid->property("decodeWidth").toInt() > 0, "covers settle on a step once the window stops");
                    window->resize(1320, 860);
                    settle(600);
                }
            }
            {
                // A detail page whose header gains a line keeps its heading where it is.
                emit controller.home()->animeSelected(subject);
                settle(600);
                AnimeSubject arc = subject;
                arc.bangumiId = 8;
                arc.nameCn = QStringLiteral("测试 黄昏篇");
                controller.detail()->applySeries({arc});
                settle(400);
                auto *heading = findItem(window->contentItem(), [](QQuickItem *item) { return item->property("text").toString() == QStringLiteral("可用资源") && item->isVisible(); });
                auto *kinds = findItem(window->contentItem(), [](QQuickItem *item) { return item->property("tip").toString() == QStringLiteral("下载状态") && item->isVisible(); });
                check(heading && kinds, "the detail page shows its headings");
                if (heading && kinds) {
                    const qreal headingY = heading->mapToScene(QPointF(0, 0)).y();
                    qreal pushed = 0;
                    const auto wrapFrames = QObject::connect(window, &QQuickWindow::beforeSynchronizing, window, [&] {
                        pushed = qMax(pushed, qAbs(heading->mapToScene(QPointF(0, 0)).y() - headingY));
                    }, Qt::DirectConnection);
                    const qreal lastFilterY = kinds->mapToScene(QPointF(0, 0)).y();
                    // As wide as the list was beside a folded sidebar: still the two-column layout.
                    window->resize(1052, 860);
                    settle(400);
                    const qreal wrappedBy = kinds->mapToScene(QPointF(0, 0)).y() - lastFilterY;
                    window->resize(1320, 860);
                    settle(400);
                    QObject::disconnect(wrapFrames);
                    check(wrappedBy > 20, "a narrower window wraps the filters");
                    check(pushed < 0.5, "a header gaining a line does not push the heading up");
                }
                controller.closeDetail();
                settle(500);
            }
        }
        auto *settingsPage = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("SettingsPage"); });
        check(settingsPage != nullptr && !settingsPage->property("overview").toBool(), "settings show one section in a normal window");
        if (settingsPage) {
            auto *switcher = findItem(settingsPage, [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("Segmented") && item->property("items").toList().size() == 5; });
            check(switcher != nullptr, "settings have a section switcher");
            if (switcher) {
                auto *settingsObject = settingsPage->property("s").value<QObject *>();
                settingsObject->setProperty("section", 2);
                settle(50);
                check(switcher->property("chosen").toInt() == 2, "the switcher's thumb follows the section");
                settingsObject->setProperty("section", 0);
                settle(50);
                switcher->forceActiveFocus();
                QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
                QCoreApplication::sendEvent(switcher, &right);
                settle(50);
                check(settingsObject->property("section").toInt() == 1, "the arrow keys move through a segmented control");
                settingsObject->setProperty("section", 0);
                settle(50);
            }
        }
        window->resize(2400, 860);
        settle(320);
        check(settingsPage != nullptr && settingsPage->property("overview").toBool(), "settings show every section in a wide window");
        if (settingsPage) {
            // The title band loosens with the window's height, not its width.
            auto band = [&](int width, int height) {
                window->resize(width, height);
                settle(120);
                return settingsPage->property("band").toReal();
            };
            const qreal shortWindow = band(1320, 620), normal = band(1320, 860), tall = band(1320, 1200), ultrawide = band(2400, 620);
            check(shortWindow >= 76 && normal > shortWindow && tall > normal && tall <= 104, "the title band grows with the window's height");
            check(ultrawide == shortWindow, "a short wide window keeps the compact band");
            // Rows under the band read its height from Shell, so this also catches pages
            // compiled against an older Shell.
            window->resize(1320, 860);
            settle(120);
            auto *switcher = findItem(settingsPage, [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("Segmented") && item->property("items").toList().size() == 5; });
            check(switcher != nullptr && qAbs(switcher->y() - settingsPage->property("band").toReal() - 2) < 1, "rows under the title band sit right below it");
        }
        window->resize(1320, 860);
        settle(320);
        controller.search()->setSuggestions({subject});
        check(controller.search()->model()->count() == 1 && !controller.search()->emptyVisible(), "the search page opens with suggestions");
        controller.search()->setSuggestions({});
    }

#ifdef Q_OS_WIN
    {
        // The tray icon brings a hidden window back.
        window->hide();
        settle(150);
        const HWND tray = FindWindowW(L"MikuLibraryTray", nullptr);
        check(tray != nullptr, "the tray icon has its message window");
        SendMessageW(tray, WM_APP + 41, 1, WM_LBUTTONUP);
        check(waitFor([&] { return window->isVisible(); }, 1000), "clicking the tray icon shows the window");
    }
#endif

    check(g_warnings.isEmpty(), "no QML warnings anywhere");
    for (const auto &warning : g_warnings)
        report.write(warning.toUtf8() + "\n");
    report.flush();
    return failures ? 1 : 0;
}

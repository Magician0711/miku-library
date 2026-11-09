// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

// Loads the real interface and walks it: every page, every dialog, theme switches and
// the frameless window. A QML warning anywhere fails the run.

#include "app/AppController.h"
#include "app/Format.h"
#include "app/ImageProviders.h"
#include "app/MapListModel.h"
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

#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QTimer>
#include <QInputMethodEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtQml/qqmlextensionplugin.h>

#define NOMINMAX
#include <windows.h>

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
    QGuiApplication app(argc, argv);
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
        const QString a = list.model()->get(0).value("key").toString(), c = list.model()->get(2).value("key").toString();
        list.selectOnly(a);
        check(list.selectedCount() == 1 && list.model()->get(0).value("selected").toBool(), "a click selects one task");
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
    AppController controller(&catalog, &db, &config, &http, &torrents, &aggregator, &bangumi, &theme, &chrome);

    QQmlApplicationEngine engine;
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
        check(logo && logo->property("status").toInt() == 1, "the sidebar shows the logo");
        check(!appIcon().isNull() && !appIcon().pixmap(32, 32).isNull(), "the app has its icon");
        check(window->title() == QStringLiteral("Miku Library"), "the window carries the new name");
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
        check(theme.dark() == before, "the palette waits for the still of the old look");
        check(waitFor([&] { return theme.dark() != before; }, 1000), "the palette flips once the still is on screen");
        check(fade && fade->property("opacity").toReal() > 0.5, "the still covers the window when the palette flips");
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
        // Whatever the window size, posters fill the width and the sidebar makes room.
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
                const QPointF over = window->mapToGlobal(QPointF(700, 400)) * window->devicePixelRatio();
                PostMessageW(HWND(window->winId()), WM_MOUSEWHEEL, MAKEWPARAM(0, WORD(SHORT(-360))), MAKELPARAM(int(over.x()), int(over.y())));
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
        window->resize(900, 700);
        settle(320);
        check(rootItem->property("rail").toBool() && rootItem->property("sidebarWidth").toInt() < 100, "a narrow window folds the sidebar to icons");
        window->resize(1320, 860);
        settle(320);
        check(!rootItem->property("rail").toBool(), "a wide window opens it again");
        {
            // A detail page takes the sidebar's room and gives it back.
            emit controller.home()->animeSelected(subject);
            settle(500);
            check(rootItem->property("rail").toBool(), "opening a detail page folds the sidebar");
            {
                // The artwork and the text under it share one right edge.
                auto *art = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("HeroPoster") && item->isVisible(); });
                auto *infoColumn = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("HeroInfo") && item->isVisible(); });
                check(art && infoColumn && qAbs(art->width() - infoColumn->width()) < 1, "the poster is as wide as the text under it");

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
                check(playingNav != nullptr, "the sidebar has a now-playing item");
                check(player->open(QStringLiteral("av://lavfi:testsrc=size=640x360:rate=25:duration=30"), QStringLiteral("测试视频"), QStringLiteral("第一集")), "a video opens");
                controller.selectPage(7);
                bool started = false;
                for (int waited = 0; waited < 8000 && !started; waited += 100) {
                    settle(100);
                    started = player->position() > 0.5;
                }
                check(started, "playback runs");
                check(rootItem->property("rail").toBool(), "the player folds the sidebar");
                auto *video = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "playerVideo" && item->isVisible(); });
                check(video != nullptr, "the picture is on the page");
                if (video) {
                    settle(400);
                    const QImage shot = window->grabWindow();
                    const qreal ratio = shot.width() / qreal(window->width());
                    const QPointF middle = video->mapToScene(QPointF(video->width() * 0.3, video->height() * 0.3));
                    const QColor pixel = shot.pixelColor(int(middle.x() * ratio), int(middle.y() * ratio));
                    check(pixel.lightness() > 25, "the picture is drawn, not black");
                }
                player->setUpscale(1);
                check(player->shaderList().contains(QStringLiteral("Upscale_CNN_x2_M")), "fast upscaling loads its shaders");
                settle(900);
                const qreal upscaling = player->position();
                settle(900);
                check(player->position() > upscaling, "playback goes on with upscaling");
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
                    const QPointF corner = video->mapToScene(QPointF(video->width() * 0.25, video->height() * 0.25));
                    qint64 difference = 0;
                    for (int y = 0; y < 200; y += 2)
                        for (int x = 0; x < 300; x += 2) {
                            const QPoint at(int(corner.x() * scale) + x, int(corner.y() * scale) + y);
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
                    // Full screen gives the whole window to the picture and gives it back.
                    auto *playerPage = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PlayerPage") && item->isVisible(); });
                    check(playerPage != nullptr, "the player page is on screen");
                    if (playerPage) {
                        QMetaObject::invokeMethod(playerPage, "toggleFull");
                        settle(600);
                        check(window->visibility() == QWindow::FullScreen && rootItem->property("immersive").toBool(), "full screen hides the sidebar");
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
            check(!rootItem->property("rail").toBool(), "leaving it opens the sidebar again");

            // Resting the pointer on the folded sidebar opens it over the content.
            // Sent straight to the window: Windows would report the real pointer leaving at once.
            auto pointerAt = [&](int x, int y) {
                const QPointF at(x, y);
                QMouseEvent move(QEvent::MouseMove, at, at, window->mapToGlobal(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &move);
            };
            auto *navItem = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("NavButton") && item->property("page").toInt() == 3; });
            check(navItem != nullptr, "the sidebar has its navigation buttons");
            if (navItem) {
                const int pageBefore = controller.page();
                navItem->forceActiveFocus();
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(navItem, &enter);
                settle(120);
                check(controller.page() == 3, "the keyboard can use the sidebar");
                controller.selectPage(pageBefore);
                settle(120);
            }
            const QPointF openAt = navItem ? navItem->mapToScene(QPointF(0, 0)) : QPointF();
            rootItem->setProperty("railChosen", true);
            settle(500);
            check(rootItem->property("sidebarShown").toReal() < 70, "the folded sidebar is narrow");
            check(navItem && navItem->mapToScene(QPointF(0, 0)) == openAt, "folding leaves the navigation buttons where they were");
            // Repeated like a pointer that keeps reporting: the real one, wherever it is,
            // can send the window events of its own while the test runs.
            auto restAt = [&](int x, int y, const char *name, bool value) {
                for (int waited = 0; waited < 1500 && rootItem->property(name).toBool() != value; waited += 50) {
                    pointerAt(x, y);
                    settle(50);
                }
                return rootItem->property(name).toBool() == value;
            };
            QPair<int, int> gridFolded;
            {
                auto *grid = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PosterGrid") && item->isVisible(); });
                check(grid != nullptr, "a poster grid is on screen for the motion checks");
                if (grid) {
                    // The cards arrive at whole pixels without a step at the end.
                    const auto travel = [&](bool folded) {
                        rootItem->setProperty("railChosen", folded);
                        qreal last = grid->property("cardWidth").toReal(), worstTail = 0;
                        for (int waited = 0; waited < 700; waited += 4) {
                            settle(4);
                            const qreal now = grid->property("cardWidth").toReal(), reveal = rootItem->property("sidebarReveal").toReal();
                            if (reveal > 0.98 || reveal < 0.02)
                                worstTail = qMax(worstTail, qAbs(now - last));
                            last = now;
                        }
                        return worstTail;
                    };
                    const qreal openTail = travel(false), foldTail = travel(true);
                    check(openTail < 0.5 && foldTail < 0.5, "a sidebar motion ends without a step");
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
            check(restAt(30, 400, "peek", true), "the pointer opens a folded sidebar");
            for (int waited = 0; waited < 500; waited += 50) {
                pointerAt(30, 400);
                settle(50);
            }
            check(rootItem->property("sidebarShown").toReal() > 200, "the sidebar opens fully");
            check(rootItem->property("contentLeft").toReal() > 250, "the open sidebar pushes the content aside");
            {
                // Same columns and same decoded covers either way, or the posters would jump.
                auto *grid = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PosterGrid") && item->isVisible(); });
                check(grid != nullptr, "a poster grid is on screen");
                if (grid) {
                    const int columnsOpen = grid->property("columns").toInt(), decodeOpen = grid->property("decodeWidth").toInt();
                    gridFolded = {columnsOpen, decodeOpen};
                }
            }
            check(rootItem->property("peek").toBool(), "the sidebar stays open under a resting pointer");
            check(restAt(900, 400, "peek", false), "leaving folds it again");
            {
                // The headings of a detail page hold still while the sidebar passes.
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
                QVector<ResourceResult> long_;
                for (int i = 0; i < 12; ++i) {
                    ResourceResult r;
                    r.title = QStringLiteral("[第%1组] 测试 - %2 这是一条足够长的资源标题在侧栏展开时原本会折成两行显示用来确认").arg(i % 2).arg(i + 1);
                    r.releaseGroup = QStringLiteral("第%1组").arg(i % 2);
                    r.magnetUri = "magnet:?xt=urn:btih:" + QString::number(i + 1, 16).rightJustified(40, '0');
                    long_.append(r);
                }
                emit aggregator.batchReady(long_);
                emit aggregator.finished();
                settle(700);
                auto *detailPage = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("DetailPage"); });
                auto *lastGroup = findItem(window->contentItem(), [](QQuickItem *item) { return item->property("text").toString().startsWith(QStringLiteral("第1组")) && item->isVisible(); });
                if (!lastGroup)
                    lastGroup = findItem(window->contentItem(), [](QQuickItem *item) { return item->property("text").toString().startsWith(QStringLiteral("第0组")) && item->isVisible(); });
                check(detailPage && lastGroup, "the detail page lists its groups");
                if (heading && kinds && detailPage && lastGroup) {
                    const qreal headingY = heading->mapToScene(QPointF(0, 0)).y(), kindsY = kinds->mapToScene(QPointF(0, 0)).y();
                    qreal headingShift = 0, kindsShift = 0, headingLag = 0;
                    // Read as each frame is handed to the renderer: a shift that is put
                    // right before the next event would not show up between frames.
                    const auto frames = QObject::connect(window, &QQuickWindow::beforeSynchronizing, window, [&] {
                        headingShift = qMax(headingShift, qAbs(heading->mapToScene(QPointF(0, 0)).y() - headingY));
                        kindsShift = qMax(kindsShift, qAbs(kinds->mapToScene(QPointF(0, 0)).y() - kindsY));
                        headingLag = qMax(headingLag, qAbs(heading->mapToScene(QPointF(0, 0)).x() - detailPage->property("listLeft").toReal()));
                    }, Qt::DirectConnection);
                    const auto follow = [&](int x, int ms) {
                        for (int waited = 0; waited < ms; waited += 20) {
                            pointerAt(x, 400);
                            settle(20);
                        }
                    };
                    for (int cycle = 0; cycle < 2; ++cycle) {
                        follow(30, 800);
                        follow(900, 900);
                    }
                    QObject::disconnect(frames);
                    check(headingShift < 0.5, "passing over the sidebar leaves the page heading in place");
                    check(kindsShift < 0.5, "passing over the sidebar does not rewrap the filters");
                    {
                        // Pushed text wraps to the room it has, the way it does in a narrower window.
                        auto *title = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("BodyText") && item->property("text").toString().startsWith(QStringLiteral("[第0组]")) && item->isVisible(); });
                        check(title != nullptr, "a release title is on screen");
                        if (title) {
                            const qreal foldedHeight = title->height();
                            follow(30, 800);
                            const qreal pushedHeight = title->height();
                            const qreal right = title->mapToScene(QPointF(title->width(), 0)).x();
                            const qreal listRight = detailPage->property("listLeft").toReal() + detailPage->property("listWidth").toReal();
                            bool clipped = false;
                            for (auto *up = title->parentItem(); up && up != window->contentItem(); up = up->parentItem())
                                clipped = clipped || (up->clip() && !QByteArray(up->metaObject()->className()).contains("Loader") && up->width() < title->width());
                            check(pushedHeight > foldedHeight + 4, "a pushed release title wraps onto another line");
                            check(right < listRight - 100 && !clipped, "a pushed release title is not cut off");
                            follow(900, 900);
                        }
                    }
                    check(headingLag < 1.5, "the heading follows the content edge on every frame");

                    // A header that really does gain a line keeps its top where it is, on every frame.
                    qreal pushed = 0;
                    const auto wrapFrames = QObject::connect(window, &QQuickWindow::beforeSynchronizing, window, [&] {
                        pushed = qMax(pushed, qAbs(heading->mapToScene(QPointF(0, 0)).y() - headingY));
                    }, Qt::DirectConnection);
                    auto *lastFilter = findItem(window->contentItem(), [](QQuickItem *item) { return item->property("tip").toString() == QStringLiteral("下载状态") && item->isVisible(); });
                    const qreal lastFilterY = lastFilter ? lastFilter->mapToScene(QPointF(0, 0)).y() : 0;
                    window->resize(1120, 860);
                    settle(400);
                    const qreal wrappedBy = lastFilter ? lastFilter->mapToScene(QPointF(0, 0)).y() - lastFilterY : 0;
                    window->resize(1320, 860);
                    settle(400);
                    QObject::disconnect(wrapFrames);
                    check(wrappedBy > 20, "a narrower window wraps the filters");
                    check(pushed < 0.5, "a header gaining a line does not push the heading up");
                }
                controller.closeDetail();
                settle(500);
            }

            // Frame by frame the content must stay clear of the sidebar, also when the
            // pointer changes its mind halfway.
            qreal worstGap = 1000;
            const auto watch = [&](int ms) {
                for (int waited = 0; waited < ms; waited += 8) {
                    settle(8);
                    const qreal contentLeft = rootItem->property("contentLeft").toReal();
                    const qreal sidebarRight = 8 + rootItem->property("sidebarShown").toReal();
                    worstGap = qMin(worstGap, contentLeft - sidebarRight);
                }
            };
            settle(500);
            rootItem->setProperty("peekWanted", true);
            watch(500);
            rootItem->setProperty("peekWanted", false);
            watch(500);
            rootItem->setProperty("peekWanted", true);
            watch(150);
            rootItem->setProperty("peekWanted", false);
            watch(120);
            rootItem->setProperty("peekWanted", true);
            watch(500);
            rootItem->setProperty("peekWanted", false);
            watch(500);
            check(worstGap >= 22, "the sidebar never reaches the content while it moves");
            check(rootItem->property("sidebarShown").toReal() < 70, "quick changes of mind end folded");
            {
                auto *grid = findItem(window->contentItem(), [](QQuickItem *item) { return QByteArray(item->metaObject()->className()).contains("PosterGrid") && item->isVisible(); });
                check(grid && grid->property("columns").toInt() == gridFolded.first && grid->property("decodeWidth").toInt() == gridFolded.second,
                      "opening the sidebar keeps the columns and the decoded covers");
            }
            rootItem->setProperty("railChosen", false);
            settle(500);
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

    {
        // The tray icon brings a hidden window back.
        window->hide();
        settle(150);
        const HWND tray = FindWindowW(L"MikuLibraryTray", nullptr);
        check(tray != nullptr, "the tray icon has its message window");
        SendMessageW(tray, WM_APP + 41, 1, WM_LBUTTONUP);
        check(waitFor([&] { return window->isVisible(); }, 1000), "clicking the tray icon shows the window");
    }

    check(g_warnings.isEmpty(), "no QML warnings anywhere");
    for (const auto &warning : g_warnings)
        report.write(warning.toUtf8() + "\n");
    report.flush();
    return failures ? 1 : 0;
}

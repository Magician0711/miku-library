// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

// Records the real interface frame by frame, for release videos. Animations run on a
// clock this tool advances by exactly one frame at a time, so a recording is smooth
// however long each frame takes to grab. The window is parked off the desktop and never
// takes the focus; input comes from a script.
//
//   ShowreelCapture <data-dir> <script.txt> <out-dir>
//
// Script, one command a line (# starts a comment, "quotes" keep spaces):
//   size W H              window size in logical pixels
//   settle MS             let the app run in real time, without recording
//   waittext TEXT MS      settle until an item showing TEXT is visible
//   waitgone TEXT MS      settle until no item shows TEXT
//   framesuntilgone TEXT N  advance frames, at most N, until no item shows TEXT
//   rec NAME / stop       record the frames that follow into <out-dir>/NAME
//   frames N              advance N frames
//   pace MS               real milliseconds to spend on each recorded frame (0 = none)
//   move X Y N            glide the pointer there over N frames
//   moveto TEXT N [DX DY] the same, to the centre of the item showing TEXT (@name = objectName, ^text = starts with)
//   press / release / click
//   wheel DELTA           one wheel step at the pointer (negative scrolls down)
//   type TEXT N           type into the focused field, N frames a character
//   key NAME [N]          Return, Escape, Space, Left, Right, Up, Down, F, M, Tab, Backspace; N times
//   page N                open a page directly
//   call NAME             run a function of the window, such as chooseDownloader
//   activate TEXT         click a button by its signal (title bar buttons; TEXT may be its tooltip)
//   mark NAME             note the current frame of the recording, saved in its manifest
//   theme MODE            light, dark or system
//   resize W H N          change the window size over N frames
//   play PATH TITLE SUB   open a video in the built-in player
//   upscale N             Anime4K preset
//   snap NAME             save one PNG
//   dump NAME             list the visible texts and where they are

#include "app/AppController.h"
#include "app/LanguageController.h"
#include "app/ImageProviders.h"
#include "app/PlayerController.h"
#include "app/ThemeController.h"
#include "app/WindowChrome.h"
#include "core/AppConfig.h"
#include "core/Catalog.h"
#include "core/Database.h"
#include "net/BangumiClient.h"
#include "net/HttpClient.h"
#include "net/ImageLoader.h"
#include "net/SearchAggregator.h"
#include "net/TorrentEngine.h"

#include <QAnimationDriver>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QFutureSynchronizer>
#include <QGuiApplication>
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
#include <QScreen>
#include <QSslSocket>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QWheelEvent>
#include <QtConcurrent>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(MikuLibraryPlugin)

namespace {
constexpr double kFrameMs = 1000.0 / 60.0;

// The animation clock: it moves only when told to.
class StepDriver final : public QAnimationDriver {
public:
    void step(double ms)
    {
        m_exact += ms;
        m_elapsed = qint64(m_exact);
        advanceAnimation();
    }
    qint64 elapsed() const override { return m_elapsed; }
    void advance() override {}

private:
    double m_exact = 0;
    qint64 m_elapsed = 0;
};

QStringList words(const QString &line)
{
    QStringList out;
    QString current;
    bool quoted = false, any = false;
    for (const QChar c : line) {
        if (c == '"') { quoted = !quoted; any = true; continue; }
        if (c.isSpace() && !quoted) {
            if (any || !current.isEmpty()) out << current;
            current.clear();
            any = false;
            continue;
        }
        current += c;
    }
    if (any || !current.isEmpty()) out << current;
    return out;
}

bool shown(const QQuickItem *item)
{
    for (auto *walk = item; walk; walk = walk->parentItem())
        if (!walk->isVisible() || walk->opacity() < 0.05)
            return false;
    return true;
}

template <class Visit> void walk(QQuickItem *root, Visit visit)
{
    visit(root);
    for (auto *child : root->childItems())
        walk(child, visit);
}

QFile g_log;
void logLine(const QString &text)
{
    g_log.write(text.toUtf8() + '\n');
    g_log.flush();
}
void handler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type != QtDebugMsg)
        logLine(QStringLiteral("qt: ") + message);
}
}

int main(int argc, char *argv[])
{
    if (argc < 4)
        return 2;
    qputenv("MIKU_LIBRARY_DATA_DIR", QByteArray(argv[1]));
    // One thread draws and reads back, so a grab is the frame the clock just produced.
    qputenv("QSG_RENDER_LOOP", "basic");
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QQuickWindow::setDefaultAlphaBuffer(true);
    // Windows slows the display of a window nobody can see; nothing here waits for it.
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc, argv);
    StepDriver clock;
    clock.install();
    QSslSocket::setActiveBackend(QStringLiteral("schannel"));
    app.setApplicationName(QStringLiteral("Miku Library"));
    app.setApplicationVersion(QStringLiteral(MIKU_LIBRARY_VERSION));
    app.setOrganizationName(QStringLiteral("miku-library"));
    QFont font;
    font.setFamilies({QStringLiteral("Segoe UI Variable"), QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI")});
    font.setPixelSize(13);
    app.setFont(font);
    app.setQuitOnLastWindowClosed(false);

    const QString outDir = QString::fromLocal8Bit(argv[3]);
    QDir().mkpath(outDir);
    g_log.setFileName(outDir + QStringLiteral("/capture.log"));
    g_log.open(QIODevice::WriteOnly);
    qInstallMessageHandler(handler);

    AppConfig config = AppConfig::load();
    HttpClient::applyProxy(config.proxyMode, config.proxyHost, config.proxyPort);
    ThemeController theme(config.appearance);
    WindowChrome chrome;
    Catalog catalog(QDir(AppConfig::dataDir()).filePath(QStringLiteral("anime_sites.json")));
    if (!catalog.load()) { logLine(QStringLiteral("no site catalog in the data folder")); return 1; }
    Database db(AppConfig::dataDir() + QStringLiteral("/miku_library.db"));
    QString error;
    if (!db.init(&error)) { logLine(error); return 1; }
    HttpClient http;
    BangumiClient bangumi(&http);
    bangumi.setCacheDir(AppConfig::dataDir());
    SearchAggregator aggregator(&catalog, &http, &bangumi);
    ImageLoader images(AppConfig::dataDir() + QStringLiteral("/covers"));
    // The engine is left stopped: a recording needs no network ports.
    TorrentEngine torrents(config.engine);
    LanguageController language(&config);
    AppController controller(&catalog, &db, &config, &http, &torrents, &aggregator, &bangumi, &theme, &chrome);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Lang"), &language);
    engine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("Chrome"), &chrome);
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("cover"), new CoverProvider(&images));
    engine.loadFromModule("MikuLibrary", "Main");
    if (engine.rootObjects().isEmpty()) { logLine(QStringLiteral("the window did not load")); return 1; }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constLast());
    controller.setWindow(window);

    // Beside the desktop, where nothing shows and nothing can be clicked by accident.
    window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
    window->setFlag(Qt::Tool, true);
    const QRect desktop = app.primaryScreen()->virtualGeometry();
    window->setPosition(desktop.right() + 400, desktop.top() + 80);
    window->show();

    QPointF pointer(-50, -50);
    bool down = false;
    QElapsedTimer real;
    real.start();

    bool recording = false;
    QString recName;
    QJsonArray recFrames;
    QByteArray lastHash;
    int lastFile = -1, fileCount = 0, paceMs = 0;
    QSize recSize;
    QFutureSynchronizer<void> saves;
    QElapsedTimer recClock;
    QJsonObject marks;

    auto pump = [&] { QCoreApplication::processEvents(QEventLoop::AllEvents, 2); };
    auto settle = [&](int ms) {
        QElapsedTimer t;
        t.start();
        qint64 before = 0;
        while (t.elapsed() < ms) {
            pump();
            const qint64 now = t.elapsed();
            clock.step(double(qMin<qint64>(now - before, 50)));
            before = now;
        }
        pump();
    };
    auto sendMouse = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, pointer, pointer, window->mapToGlobal(pointer), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &event);
    };
    auto frame = [&] {
        QElapsedTimer spent;
        spent.start();
        clock.step(kFrameMs);
        pump();
        if (recording) {
            QImage image = window->grabWindow();
            recSize = image.size();
            const QByteArray hash = QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()), QCryptographicHash::Md5);
            if (hash != lastHash) {
                lastHash = hash;
                lastFile = fileCount++;
                const QString path = QStringLiteral("%1/%2/%3.jpg").arg(outDir, recName).arg(lastFile, 5, 10, QChar('0'));
                saves.addFuture(QtConcurrent::run([image, path] { image.convertToFormat(QImage::Format_RGB888).save(path, "JPG", 93); }));
            }
            recFrames.append(QJsonArray{lastFile, qRound(pointer.x() * 10) / 10.0, qRound(pointer.y() * 10) / 10.0, down ? 1 : 0});
            while (paceMs > 0 && spent.elapsed() < paceMs)
                pump();
        }
    };
    auto finish = [&] {
        if (!recording) return;
        saves.waitForFinished();
        QFile manifest(QStringLiteral("%1/%2.json").arg(outDir, recName));
        manifest.open(QIODevice::WriteOnly);
        manifest.write(QJsonDocument(QJsonObject{{"name", recName}, {"fps", 60}, {"files", fileCount},
            {"width", recSize.width()}, {"height", recSize.height()}, {"logicalWidth", window->width()}, {"logicalHeight", window->height()},
            {"frames", recFrames}, {"marks", marks}}).toJson(QJsonDocument::Compact));
        logLine(QStringLiteral("recorded %1: %2 frames, %3 distinct, %4 ms a frame").arg(recName).arg(recFrames.size()).arg(fileCount).arg(recClock.elapsed() / qMax<qsizetype>(1, recFrames.size())));
        recording = false;
    };
    auto find = [&](const QString &what) -> QQuickItem * {
        QQuickItem *hit = nullptr;
        walk(window->contentItem(), [&](QQuickItem *item) {
            if (hit || !shown(item) || item->width() <= 0) return;
            // A text field may hold the very words being looked for; it is never the target.
            if (!what.startsWith('@') && item->metaObject()->indexOfProperty("inputMethodComposing") >= 0) return;
            // Icon buttons have no text: their tooltip names them.
            const QString text = item->property("text").toString(), tip = item->property("tip").toString();
            const bool match = what.startsWith('@') ? item->objectName() == what.mid(1)
                : what.startsWith('^') ? text.startsWith(what.mid(1)) || (!tip.isEmpty() && tip.startsWith(what.mid(1)))
                : text == what || (!tip.isEmpty() && tip == what);
            if (!match) return;
            const QPointF centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
            if (QRectF(0, 0, window->width(), window->height()).contains(centre))
                hit = item;
        });
        return hit;
    };
    auto glide = [&](QPointF to, int count) {
        const QPointF from = pointer;
        for (int i = 1; i <= qMax(1, count); ++i) {
            const double k = double(i) / qMax(1, count);
            const double eased = k * k * k * (k * (k * 6 - 15) + 10);
            pointer = from + (to - from) * eased;
            sendMouse(QEvent::MouseMove, Qt::NoButton, down ? Qt::LeftButton : Qt::NoButton);
            frame();
        }
    };

    QFile scriptFile(QString::fromLocal8Bit(argv[2]));
    if (!scriptFile.open(QIODevice::ReadOnly)) { logLine(QStringLiteral("cannot read the script")); return 1; }
    QTextStream script(&scriptFile);
    script.setEncoding(QStringConverter::Utf8);

    settle(600);
    {
        // The window believes it is the active one, so focus rings and carets behave.
        QFocusEvent focus(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
        QCoreApplication::sendEvent(window, &focus);
    }
    logLine(QStringLiteral("window %1x%2 dpr %3 exposed %4").arg(window->width()).arg(window->height()).arg(window->effectiveDevicePixelRatio()).arg(window->isExposed()));

    int lineNumber = 0;
    while (!script.atEnd()) {
        const QString line = script.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QStringList a = words(line);
        const QString &op = a.at(0);
        auto num = [&](int i, double fallback = 0) { return i < a.size() ? a.at(i).toDouble() : fallback; };
        if (op == "size") { window->resize(int(num(1)), int(num(2))); settle(300); }
        else if (op == "settle") settle(int(num(1)));
        else if (op == "waittext") {
            QElapsedTimer t;
            t.start();
            while (!find(a.value(1)) && t.elapsed() < num(2, 5000)) settle(100);
            logLine(QStringLiteral("line %1: %2 %3").arg(lineNumber).arg(a.value(1), find(a.value(1)) ? "found" : "MISSING"));
        }
        else if (op == "waitgone") {
            QElapsedTimer t;
            t.start();
            while (find(a.value(1)) && t.elapsed() < num(2, 5000)) settle(100);
            logLine(QStringLiteral("line %1: %2 %3").arg(lineNumber).arg(a.value(1), find(a.value(1)) ? "STILL THERE" : "gone"));
        }
        else if (op == "framesuntilgone") {
            // Keeps recording until no item shows TEXT, so a wait on the network stays on film.
            int count = 0;
            while (find(a.value(1)) && count < int(num(2, 3600))) { frame(); ++count; }
            logLine(QStringLiteral("line %1: %2 after %3 frames").arg(lineNumber).arg(find(a.value(1)) ? "STILL THERE" : "gone").arg(count));
        }
        else if (op == "rec") {
            finish();
            recName = a.value(1);
            QDir(outDir + '/' + recName).removeRecursively();
            QDir().mkpath(outDir + '/' + recName);
            recFrames = {};
            marks = {};
            lastHash.clear();
            lastFile = -1;
            fileCount = 0;
            recording = true;
            recClock.start();
        }
        else if (op == "stop") finish();
        else if (op == "frames") { for (int i = 0; i < int(num(1)); ++i) frame(); }
        else if (op == "pace") paceMs = int(num(1));
        else if (op == "move") glide(QPointF(num(1), num(2)), int(num(3, 30)));
        else if (op == "moveto") {
            if (auto *item = find(a.value(1)))
                glide(item->mapToScene(QPointF(item->width() / 2, item->height() / 2)) + QPointF(num(3), num(4)), int(num(2, 30)));
            else logLine(QStringLiteral("line %1: nothing shows %2").arg(lineNumber).arg(a.value(1)));
        }
        else if (op == "press") { down = true; sendMouse(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton); frame(); }
        else if (op == "release") { down = false; sendMouse(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton); frame(); }
        else if (op == "click") {
            down = true; sendMouse(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
            for (int i = 0; i < 5; ++i) frame();
            down = false; sendMouse(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
            frame();
        }
        else if (op == "wheel") {
            QWheelEvent event(pointer, window->mapToGlobal(pointer), QPoint(), QPoint(0, int(num(1))), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(window, &event);
            frame();
        }
        else if (op == "type") {
            for (const QChar c : a.value(1)) {
                if (auto *target = window->activeFocusItem()) {
                    QInputMethodEvent commit;
                    commit.setCommitString(QString(c));
                    QCoreApplication::sendEvent(target, &commit);
                }
                for (int i = 0; i < int(num(2, 6)); ++i) frame();
            }
        }
        else if (op == "key") {
            static const QHash<QString, int> keys{{"Return", Qt::Key_Return}, {"Escape", Qt::Key_Escape}, {"Space", Qt::Key_Space}, {"Left", Qt::Key_Left},
                {"Right", Qt::Key_Right}, {"Up", Qt::Key_Up}, {"Down", Qt::Key_Down}, {"F", Qt::Key_F}, {"M", Qt::Key_M}, {"Tab", Qt::Key_Tab},
                {"Backspace", Qt::Key_Backspace}, {"End", Qt::Key_End}};
            for (int i = 0; i < qMax(1, int(num(2, 1))); ++i) {
                QKeyEvent press(QEvent::KeyPress, keys.value(a.value(1)), Qt::NoModifier), release(QEvent::KeyRelease, keys.value(a.value(1)), Qt::NoModifier);
                QCoreApplication::sendEvent(window, &press);
                QCoreApplication::sendEvent(window, &release);
                frame();
            }
        }
        else if (op == "activate") {
            // Buttons in the title bar sit where Windows moves the window, so a scripted press
            // never reaches them; their click signal is what a real press ends in.
            if (auto *item = find(a.value(1))) QMetaObject::invokeMethod(item, "clicked");
            else logLine(QStringLiteral("line %1: nothing shows %2").arg(lineNumber).arg(a.value(1)));
            frame();
        }
        else if (op == "mark") {
            marks.insert(a.value(1), recording ? int(recFrames.size()) : -1);
        }
        else if (op == "call") { QMetaObject::invokeMethod(window, a.value(1).toLatin1().constData()); frame(); }
        else if (op == "page") { controller.selectPage(int(num(1))); frame(); }
        else if (op == "theme") {
            // The same call the appearance menu makes, so the change fades as it does in the app.
            QMetaObject::invokeMethod(controller.property("settings").value<QObject *>(), "setAppearance", Q_ARG(QString, a.value(1)));
            frame();
        }
        else if (op == "resize") {
            const QSizeF from = window->size(), to(num(1), num(2));
            const int count = qMax(1, int(num(3, 30)));
            for (int i = 1; i <= count; ++i) {
                const double k = double(i) / count, eased = k * k * (3 - 2 * k);
                window->resize(qRound(from.width() + (to.width() - from.width()) * eased), qRound(from.height() + (to.height() - from.height()) * eased));
                frame();
            }
        }
        else if (op == "play") logLine(QStringLiteral("line %1: play %2").arg(lineNumber).arg(controller.player()->open(a.value(1), a.value(2), a.value(3)) ? "opened" : "FAILED"));
        else if (op == "upscale") { controller.player()->setUpscale(int(num(1))); frame(); }
        else if (op == "snap") { pump(); window->grabWindow().save(QStringLiteral("%1/%2.png").arg(outDir, a.value(1))); }
        else if (op == "dump") {
            QFile out(QStringLiteral("%1/%2.txt").arg(outDir, a.value(1)));
            out.open(QIODevice::WriteOnly);
            walk(window->contentItem(), [&](QQuickItem *item) {
                const QString text = item->property("text").toString();
                if ((text.isEmpty() && item->objectName().isEmpty()) || !shown(item) || item->width() <= 0) return;
                const QRectF box = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
                if (!QRectF(0, 0, window->width(), window->height()).intersects(box)) return;
                out.write(QStringLiteral("%1,%2 %3x%4 | %5 | %6 | %7\n").arg(qRound(box.x())).arg(qRound(box.y())).arg(qRound(box.width())).arg(qRound(box.height()))
                    .arg(QString::fromLatin1(item->metaObject()->className()).section('_', 0, 0), item->objectName(), text.left(60).replace('\n', ' ')).toUtf8());
            });
        }
        else logLine(QStringLiteral("line %1: unknown command %2").arg(lineNumber).arg(op));
    }
    finish();
    logLine(QStringLiteral("done in %1 s").arg(real.elapsed() / 1000.0, 0, 'f', 1));
    controller.player()->shutdown(window);
    return 0;
}

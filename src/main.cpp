// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "app/AppController.h"
#include "app/DialogHub.h"
#include "app/Format.h"
#include "app/ImageProviders.h"
#include "app/NativeDialogs.h"
#include "app/PlayerController.h"
#include "app/StartupFlow.h"
#include "app/ThemeController.h"
#include "app/WindowChrome.h"
#include "core/AppConfig.h"
#include "core/Backup.h"
#include "core/Catalog.h"
#include "core/Database.h"
#include "core/InstanceGuard.h"
#include "net/BangumiClient.h"
#include "net/HttpClient.h"
#include "net/ImageLoader.h"
#include "net/SearchAggregator.h"
#include "net/TorrentEngine.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QMutex>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSslSocket>
#include <QTextStream>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>

#define NOMINMAX
#include <objbase.h>
#include <windows.h>

Q_IMPORT_QML_PLUGIN(MikuLibraryPlugin)

namespace {

QString logPath()
{
    return AppConfig::dataDir() + QStringLiteral("/miku_library.log");
}

// One open handle for the session; messages can arrive from worker threads.
QFile *logFile()
{
    static QFile *file = [] {
        const QString path = logPath();
        // Keep the log bounded: start a fresh file once it passes 1 MiB.
        if (QFileInfo(path).size() > (1 << 20)) {
            QFile::remove(path + QStringLiteral(".old"));
            QFile::rename(path, path + QStringLiteral(".old"));
        }
        auto *f = new QFile(path);
        if (!f->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) { delete f; return static_cast<QFile *>(nullptr); }
        return f;
    }();
    return file;
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const char *level = "INFO";
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }
    static QMutex mutex;
    const QMutexLocker lock(&mutex);
    QFile *file = logFile();
    if (!file)
        return;
    QTextStream stream(file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << " [" << level << "] "
           << (context.category ? context.category : "default") << ": " << message << '\n';
    stream.flush();
    file->flush();
}

QString catalogPath()
{
    const QString besideExe = QCoreApplication::applicationDirPath() + QStringLiteral("/config/anime_sites.json");
    if (QFile::exists(besideExe))
        return besideExe;
    const QString sourceTree = QDir::currentPath() + QStringLiteral("/config/anime_sites.json");
    if (QFile::exists(sourceTree))
        return sourceTree;
    return besideExe;
}

} // namespace

int main(int argc, char *argv[])
{
    // Plain text at small sizes: distance-field glyphs blur Chinese text.
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    // The window draws its own corners, at the radius of the rest of the interface.
    QQuickWindow::setDefaultAlphaBuffer(true);
    QGuiApplication app(argc, argv);
    // The system file pickers are COM objects.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    // Releases ship only this TLS backend. A Qt installation on the plugin path adds
    // OpenSSL, and through a local proxy about half of its first connections then
    // fail with "Connection closed", so every way of running the app must use this one.
    QSslSocket::setActiveBackend(QStringLiteral("schannel"));

    app.setApplicationName(QStringLiteral("Miku Library"));
    app.setApplicationVersion(QStringLiteral(MIKU_LIBRARY_VERSION));
    app.setOrganizationName(QStringLiteral("miku-library"));
    app.setWindowIcon(appIcon());
    QFont font;
    font.setFamilies({QStringLiteral("Segoe UI Variable"), QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI")});
    font.setPixelSize(13);
    app.setFont(font);
    // The window can close to the tray; quitting is explicit (tray menu or the close prompt).
    app.setQuitOnLastWindowClosed(false);

    QDir().mkpath(AppConfig::dataDir());
    InstanceGuard instance(AppConfig::dataDir());
    // Windows only lets the running copy come to the front if this one, which the user
    // has just started, gives it leave to.
    AllowSetForegroundWindow(ASFW_ANY);
    const auto instanceResult = instance.acquire(app.arguments());
    if (instanceResult == InstanceGuard::Forwarded)
        return 0;
    if (instanceResult == InstanceGuard::Busy) {
        NativeDialogs::alert(QStringLiteral("Miku Library 已在运行"), QStringLiteral("现有实例正在启动或忙碌，请稍后重试。"), false);
        return 0;
    }
    qInstallMessageHandler(messageHandler);

    AppConfig config = AppConfig::load();
    HttpClient::applyProxy(config.proxyMode, config.proxyHost, config.proxyPort);
    ThemeController theme(config.appearance);
    WindowChrome chrome;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("Chrome"), &chrome);
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);

    // Quick start and usage declaration, once per declaration version.
    if (config.disclaimerAccepted < Notes::kDisclaimerVersion) {
        const QString proposed = config.hasSavedDownloadDirectory ? config.engine.savePath : AppConfig::defaultDownloadDirectory();
        StartupFlow guide(QStringLiteral("guide"), proposed);
        if (!guide.run(&engine))
            return 0;
        // A different folder picked in the guide; the default is created further down.
        if (guide.downloadDir() != proposed) {
            QDir().mkpath(guide.downloadDir());
            config.engine.savePath = guide.downloadDir();
            config.paths.downloadDir = guide.downloadDir();
            config.hasSavedDownloadDirectory = true;
        }
        config.disclaimerAccepted = Notes::kDisclaimerVersion;
        // The guide's theme picker applies live; keep whatever was chosen there.
        config.appearance = theme.mode();
        if (!config.save())
            NativeDialogs::alert(QStringLiteral("设置未保存"), QStringLiteral("无法写入配置文件，下次启动会再次显示使用声明。"), false);
    }
    QString directoryError;
    const auto directoryState = config.prepareDownloadDirectory(&directoryError);
    if (directoryState == AppConfig::DownloadDirectoryState::MissingSaved || directoryState == AppConfig::DownloadDirectoryState::Error) {
        StartupFlow replace(QStringLiteral("directory"), config.engine.savePath, directoryError);
        if (!replace.run(&engine))
            return 0;
        config.engine.savePath = replace.downloadDir();
        config.paths.downloadDir = replace.downloadDir();
        config.hasSavedDownloadDirectory = true;
    }
    if (directoryState != AppConfig::DownloadDirectoryState::Ready && !config.save()) {
        NativeDialogs::alert(QStringLiteral("无法保存下载目录"), QStringLiteral("下载目录设置未能保存，请检查配置文件的写入权限后重新打开软件。"));
        return 1;
    }

    const QString userCatalog = QDir(AppConfig::dataDir()).filePath(QStringLiteral("anime_sites.json"));
    if (!QFile::exists(userCatalog) && !QFile::copy(catalogPath(), userCatalog)) {
        NativeDialogs::alert(QStringLiteral("站点配置"), QStringLiteral("无法创建用户站点配置。"));
        return 1;
    }
    Catalog catalog(userCatalog);
    if (!catalog.load()) {
        NativeDialogs::alert(QStringLiteral("Miku Library"), QStringLiteral("找不到站点目录：\n%1").arg(catalogPath()));
        return 1;
    }
    catalog.startWatching();

    Database db(AppConfig::dataDir() + QStringLiteral("/miku_library.db"));
    QString dbError;
    if (!db.init(&dbError)) {
        NativeDialogs::alert(QStringLiteral("Miku Library"), QStringLiteral("数据库初始化失败：\n%1").arg(dbError));
        return 1;
    }

    HttpClient http;
    BangumiClient bangumi(&http);
    bangumi.setCacheDir(AppConfig::dataDir());
    SearchAggregator aggregator(&catalog, &http, &bangumi);
    ImageLoader images(AppConfig::dataDir() + QStringLiteral("/covers"));
    TorrentEngine torrents(config.engine);
    QString engineError;
    const bool engineStarted = torrents.start(&engineError);

    AppController controller(&catalog, &db, &config, &http, &torrents, &aggregator, &bangumi, &theme, &chrome);
    if (!engineStarted)
        controller.dialogs()->message(QStringLiteral("下载引擎"), QStringLiteral("内部 libtorrent 引擎启动失败：\n%1").arg(engineError), QStringLiteral("warning"));

    engine.addImageProvider(QStringLiteral("cover"), new CoverProvider(&images));
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    engine.loadFromModule("MikuLibrary", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    controller.setWindow(qobject_cast<QQuickWindow *>(engine.rootObjects().constLast()));

    // Starting the app again brings this one back, and counts as asking for fresh news.
    instance.onArguments = [&controller](const QStringList &args) {
        controller.handleArguments(args);
        controller.startedAgain();
    };
    QTimer::singleShot(0, &controller, [&] {
        controller.handleArguments(app.arguments());
        instance.ready();
    });

    const int result = app.exec();
    const QJsonObject restore = controller.takePendingRestore();
    controller.player()->shutdown(qobject_cast<QQuickWindow *>(engine.rootObjects().constLast()));
    torrents.stop();
    if (!restore.isEmpty()) {
        QVector<int> followed;
        QString restoreError;
        if (!Backup::restore(restore, AppConfig::dataDir(), &followed, &restoreError)) {
            NativeDialogs::alert(QStringLiteral("导入失败"), restoreError);
            return result;
        }
        for (const int id : db.followedIds()) db.unfollow(id);
        for (const int id : followed) db.follow(id);
        instance.release();
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    }
    return result;
}

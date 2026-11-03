#include "core/AppConfig.h"
#include "core/Catalog.h"
#include "core/Database.h"
#include "core/InstanceGuard.h"
#include "net/BangumiClient.h"
#include "net/HttpClient.h"
#include "net/SearchAggregator.h"
#include "net/TorrentEngine.h"
#include "ui/ImageLoader.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/Overlays.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>


namespace {

QString logPath()
{
    return AppConfig::dataDir() + QStringLiteral("/anime_nexus_cpp.log");
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
    QFile file(logPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << " [" << level << "] "
           << (context.category ? context.category : "default") << ": " << message << '\n';
    if (type == QtFatalMsg)
        stream.flush();
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
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Anime Nexus"));
    app.setApplicationVersion(QStringLiteral("0.4.0"));
    app.setOrganizationName(QStringLiteral("anime-nexus"));
    Theme::applyTheme(&app);
    app.setQuitOnLastWindowClosed(false);

    QDir().mkpath(AppConfig::dataDir());
    InstanceGuard instance(AppConfig::dataDir());
    const auto instanceResult=instance.acquire(app.arguments());
    if(instanceResult==InstanceGuard::Forwarded)return 0;
    if(instanceResult==InstanceGuard::Busy){Ui::message(nullptr,QStringLiteral("Anime Nexus 已在运行"),QStringLiteral("现有实例正在启动或忙碌，请稍后重试。"));return 0;}
    qInstallMessageHandler(messageHandler);

    AppConfig config = AppConfig::load();
    if (const auto appearance = Theme::modeFromString(config.appearance); appearance != Theme::mode())
        Theme::setMode(appearance);
    QString directoryError;
    const auto directoryState=config.prepareDownloadDirectory(&directoryError);
    if(directoryState==AppConfig::DownloadDirectoryState::MissingSaved
        || directoryState==AppConfig::DownloadDirectoryState::Error) {
        const QString selected=Ui::replaceDownloadDirectory(nullptr,config.engine.savePath,directoryError);
        if(selected.isEmpty())return 0;
        config.engine.savePath=selected;config.paths.downloadDir=selected;
        config.hasSavedDownloadDirectory=true;
    }
    if(directoryState!=AppConfig::DownloadDirectoryState::Ready && !config.save()) {
        Ui::message(nullptr,QStringLiteral("无法保存下载目录"),
            QStringLiteral("下载目录设置未能保存，请检查配置文件的写入权限后重新打开软件。"),Ui::Notice::Error);
        return 1;
    }

    const QString userCatalog=QDir(AppConfig::dataDir()).filePath(QStringLiteral("anime_sites.json"));
    if(!QFile::exists(userCatalog)&&!QFile::copy(catalogPath(),userCatalog)) {
        Ui::message(nullptr,QStringLiteral("站点配置"),QStringLiteral("无法创建用户站点配置。"),Ui::Notice::Error);return 1;
    }
    Catalog catalog(userCatalog);
    if (!catalog.load()) {
        Ui::message(nullptr, QStringLiteral("Anime Nexus"),
                    QStringLiteral("找不到站点目录：\n%1").arg(catalogPath()), Ui::Notice::Error);
        return 1;
    }
    catalog.startWatching();

    Database db(AppConfig::dataDir() + QStringLiteral("/anime_nexus.db"));
    QString dbError;
    if (!db.init(&dbError)) {
        Ui::message(nullptr, QStringLiteral("Anime Nexus"),
                    QStringLiteral("数据库初始化失败：\n%1").arg(dbError), Ui::Notice::Error);
        return 1;
    }

    HttpClient http;
    BangumiClient bangumi(&http);
    SearchAggregator aggregator(&catalog, &http, &bangumi);
    ImageLoader images(AppConfig::dataDir() + QStringLiteral("/covers"));
    TorrentEngine engine(config.engine);
    QString engineError;
    if (!engine.start(&engineError)) {
        Ui::message(nullptr, QStringLiteral("下载引擎"),
                    QStringLiteral("内部 libtorrent 引擎启动失败：\n%1").arg(engineError), Ui::Notice::Warning);
    }

    MainWindow window(&catalog, &db, &config, &http, &engine,
                      &aggregator, &bangumi, &images);
    app.setQuitOnLastWindowClosed(true);
    window.show();

    instance.onArguments=[&window](const QStringList &args){window.handleArguments(args);};
    QTimer::singleShot(0,&window,[&]{window.handleArguments(app.arguments());instance.ready();});

    const int result = app.exec();
    engine.stop();
    return result;
}

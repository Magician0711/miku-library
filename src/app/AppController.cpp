// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "AppController.h"

#include "BrowseControllers.h"
#include "DetailController.h"
#include "DialogHub.h"
#include "DownloadsController.h"
#include "Format.h"
#include "NativeDialogs.h"
#include "SettingsController.h"
#include "SitesController.h"
#include "ThemeController.h"
#include "TrayIcon.h"
#include "WindowChrome.h"
#include "core/AppConfig.h"
#include "core/Backup.h"
#include "core/Catalog.h"
#include "core/Database.h"
#include "core/LocalLibrary.h"
#include "core/TitleParser.h"
#include "net/AnimeDownloads.h"
#include "app/MpvVideoItem.h"
#include "app/PlayerController.h"
#include "net/AppUpdate.h"
#include "net/BangumiClient.h"
#include "net/ExternalDownloader.h"
#include "net/HttpClient.h"
#include "net/SearchAggregator.h"
#include "net/TorrentEngine.h"

#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_info.hpp>

namespace {
QStringList trayItems()
{
    return {QCoreApplication::translate("Miku", "打开 Miku Library"), QCoreApplication::translate("Miku", "检查追更"), QString(), QCoreApplication::translate("Miku", "退出")};
}

QString detailTitle(const QString &name) { return QCoreApplication::translate("Miku", "番剧详情 · %1").arg(name); }

// Built when asked, so they follow the interface language.
QStringList pageNames()
{
    return {QCoreApplication::translate("Miku", "本季放送"), QCoreApplication::translate("Miku", "搜索番剧"), QCoreApplication::translate("Miku", "我的追番"),
                                QCoreApplication::translate("Miku", "下载任务"), QCoreApplication::translate("Miku", "资源站点"), QCoreApplication::translate("Miku", "偏好设置"),
                                QCoreApplication::translate("Miku", "番剧详情"), QCoreApplication::translate("Miku", "正在播放")};
}

// Display name of a magnet link or local .torrent handed over on the command line.
QString importedTitle(const QString &input)
{
    if (input.startsWith("magnet:"))
        return QUrlQuery(QUrl(input)).queryItemValue("dn");
    try {
        return QString::fromStdString(lt::load_torrent_file(input.toStdString()).ti->name());
    } catch (...) {
        return QFileInfo(input).completeBaseName();
    }
}
}

AppController::AppController(Catalog *catalog, Database *db, AppConfig *config, HttpClient *http, TorrentEngine *engine,
                             SearchAggregator *aggregator, BangumiClient *bangumi, ThemeController *theme, WindowChrome *chrome,
                             QObject *parent)
    : QObject(parent), m_catalog(catalog), m_db(db), m_config(config), m_http(http), m_engine(engine),
      m_aggregator(aggregator), m_bangumi(bangumi), m_theme(theme), m_chrome(chrome), m_contextTitle(pageNames().first())
{
    for (const int id : m_db->followedIds())
        m_following.insert(id);

    m_dialogs = new DialogHub(this);
    m_localLibrary = new LocalLibrary(QDir(AppConfig::dataDir()).filePath("local-library.json"), this);
    QString libraryError;
    m_localLibrary->load(&libraryError);
    m_animeDownloads = new AnimeDownloads(m_engine, m_localLibrary, m_config, this);
    m_home = new HomeController(m_bangumi, this);
    m_search = new SearchController(m_bangumi, this);
    m_library = new LibraryController(m_db, m_bangumi, m_localLibrary, this);
    m_downloads = new DownloadsController(m_engine, m_localLibrary, m_config, m_animeDownloads->rpc(), m_dialogs, this);
    m_sites = new SitesController(m_catalog, m_http, m_dialogs, this);
    m_settings = new SettingsController(m_engine, m_config, m_theme, m_dialogs, this);
    m_detail = new DetailController(m_aggregator, m_bangumi, m_catalog, m_localLibrary,
                                    [this](int id) { return m_following.contains(id); }, this);
    m_appUpdate = new AppUpdate(m_http, this);
    qmlRegisterType<MpvVideoItem>("MikuLibrary.Native", 1, 0, "MpvVideo");
    m_player = new PlayerController(this);
    connect(m_player, &PlayerController::failed, this, [this](const QString &message) {
        m_dialogs->message(QCoreApplication::translate("Miku", "无法播放"), message, QStringLiteral("warning"));
    });
    wire();
    wireSettings();
    buildTray();

    m_periodicCheck = new QTimer(this);
    connect(m_periodicCheck, &QTimer::timeout, this, [this] { checkForNewEpisodes(false); });
    applyGeneralSettings();
    if (!libraryError.isEmpty())
        m_dialogs->message(QCoreApplication::translate("Miku", "本地记录读取失败"), libraryError, QStringLiteral("warning"));
    QTimer::singleShot(0, this, [this] { m_animeDownloads->adoptCachedSubjects(m_db->cachedSubjects()); });
    updateStatus();
    // Check followed release groups once per launch, after the first screen has settled.
    QTimer::singleShot(10000, this, [this] { checkForNewEpisodes(false); });
    if (m_config->checkAppUpdates)
        QTimer::singleShot(15000, m_appUpdate, &AppUpdate::check);
}

void AppController::setWindow(QQuickWindow *window)
{
    m_window = window;
    m_settings->setWindow(window);
}

void AppController::wire()
{
    connect(m_animeDownloads, &AnimeDownloads::queued, this, [this] { if (!m_autoQueuing) selectPage(kDownloadsPage); });
    connect(m_animeDownloads, &AnimeDownloads::information, this, [this](const QString &text) {
        if (m_autoQueuing) {
            notify(QCoreApplication::translate("Miku", "自动下载"), text, kDownloadsPage);
            return;
        }
        m_dialogs->message(QCoreApplication::translate("Miku", "下载提示"), text);
    });
    connect(m_animeDownloads, &AnimeDownloads::error, this, [this](const QString &text) {
        if (m_autoQueuing || !m_window || !m_window->isVisible()) {
            notify(QCoreApplication::translate("Miku", "下载失败"), text, kDownloadsPage);
            return;
        }
        m_dialogs->message(QCoreApplication::translate("Miku", "下载失败"), text, QStringLiteral("warning"));
    });
    // Finished downloads arrive in bursts (a batch, several episodes); one notice per burst.
    m_finishedTimer = new QTimer(this);
    m_finishedTimer->setSingleShot(true);
    m_finishedTimer->setInterval(2500);
    connect(m_finishedTimer, &QTimer::timeout, this, [this] {
        const QStringList names = std::exchange(m_finished, {});
        if (names.isEmpty())
            return;
        const QString text = names.size() <= 3 ? names.join(QLatin1Char('\n'))
            : names.mid(0, 2).join(QLatin1Char('\n')) + QCoreApplication::translate("Miku", "\n等 %1 个下载").arg(names.size());
        notify(QCoreApplication::translate("Miku", "下载完成"), text, kLibraryPage);
    });
    connect(m_localLibrary, &LocalLibrary::downloadCompleted, this, [this](const LibraryItem &item) {
        if (!m_config->notifyDownloads)
            return;
        const QString episodes = item.episodes.isEmpty() ? QString()
            : item.episodes.size() == 1 ? QCoreApplication::translate("Miku", " 第 %1 话").arg(item.episodes.first())
            : QCoreApplication::translate("Miku", " 第 %1–%2 话").arg(item.episodes.first(), item.episodes.last());
        m_finished << item.subject.displayName() + episodes;
        m_finishedTimer->start();
    });
    connect(m_engine, &TorrentEngine::engineError, this, [this](const QString &message) {
        m_dialogs->message(QCoreApplication::translate("Miku", "下载任务异常"), message, QStringLiteral("warning"));
    });

    connect(m_home, &BrowseController::animeSelected, this, [this](const AnimeSubject &s) { openAnime(s); });
    connect(m_home, &HomeController::topRatedChanged, m_search, &SearchController::setSuggestions);
    connect(m_search, &BrowseController::animeSelected, this, [this](const AnimeSubject &s) { openAnime(s); });
    connect(m_library, &BrowseController::animeSelected, this, [this](const AnimeSubject &s) { openAnime(s, true); });
    connect(m_library, &LibraryController::checkUpdatesRequested, this, [this] { checkForNewEpisodes(true); });

    connect(m_detail, &DetailController::animeRequested, this, [this](const AnimeSubject &subject, bool localOnly) {
        openAnime(subject, localOnly);
    });
    connect(m_detail, &DetailController::resourceSearchRequested, this, [this](const AnimeSubject &subject) {
        selectPage(kSearchPage);
        m_search->search(subject.displayName());
        openAnime(subject, false);
    });
    connect(m_detail, &DetailController::downloadRequested, this, &AppController::onDownloadRequested);
    connect(m_detail, &DetailController::followChanged, this, &AppController::onFollowChanged);
    connect(m_detail, &DetailController::groupDownloadRequested, m_animeDownloads, &AnimeDownloads::downloadGroup);
    connect(m_detail, &DetailController::resourcesDiscovered, m_animeDownloads, &AnimeDownloads::adoptExisting);
    connect(m_detail, &DetailController::playRequested, this, [this](const AnimeSubject &subject, const ResourceResult &resource) {
        const QStringList files = m_localLibrary->playableFiles(subject.bangumiId, resource);
        if (files.isEmpty()) {
            m_dialogs->message(QCoreApplication::translate("Miku", "视频不可用"), QCoreApplication::translate("Miku", "本地视频已移动、删除或尚未下载完成。"), QStringLiteral("warning"));
            return;
        }
        playFiles(files, subject.displayName());
    });
    connect(m_downloads, &DownloadsController::playRequested, this, &AppController::playFiles);

    connect(m_sites, &SitesController::importRequested, this, &AppController::importCatalog);
    connect(m_sites, &SitesController::exportRequested, this, &AppController::exportCatalog);

    connect(m_appUpdate, &AppUpdate::available, this, [this](const QString &version, const QUrl &page) {
        m_newVersion = QCoreApplication::translate("Miku", "新版本 %1").arg(version);
        m_newVersionPage = page;
        emit newVersionChanged();
    });
    connect(m_appUpdate, &AppUpdate::checked, m_settings, &SettingsController::setUpdateResult);

    // Engine change notifications are frequent; the sidebar only needs a coarse refresh.
    // Throttle, not debounce: they must not keep postponing it while transfers run.
    m_badgeTimer = new QTimer(this);
    m_badgeTimer->setSingleShot(true);
    m_badgeTimer->setInterval(600);
    connect(m_badgeTimer, &QTimer::timeout, this, &AppController::updateStatus);
    connect(m_engine, &TorrentEngine::changed, this, [this] { if (!m_badgeTimer->isActive()) m_badgeTimer->start(); });
    m_badgeTimer->start();
}

void AppController::wireSettings()
{
    connect(m_settings, &SettingsController::downloaderChanged, this, &AppController::updateDownloader);
    connect(m_settings, &SettingsController::generalSettingsChanged, this, &AppController::applyGeneralSettings);
    connect(m_settings, &SettingsController::proxyChanged, this, [this] {
        HttpClient::applyProxy(m_config->proxyMode, m_config->proxyHost, m_config->proxyPort);
    });
    connect(m_settings, &SettingsController::checkAppUpdateRequested, m_appUpdate, &AppUpdate::check);
    connect(m_settings, &SettingsController::exportBackupRequested, this, [this] {
        const QString name = QCoreApplication::translate("Miku", "MikuLibrary-备份-%1.json").arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
        const QString path = NativeDialogs::saveJson(m_window, QCoreApplication::translate("Miku", "导出备份"),
            QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(name));
        if (path.isEmpty())
            return;
        const auto backup = Backup::create(AppConfig::dataDir(), m_db->followedIds());
        QString error;
        if (Backup::save(backup, path, &error))
            m_dialogs->message(QCoreApplication::translate("Miku", "备份已导出"), Backup::summary(backup) + QStringLiteral("\n") + QDir::toNativeSeparators(path));
        else
            m_dialogs->message(QCoreApplication::translate("Miku", "导出失败"), error, QStringLiteral("error"));
    });
    connect(m_settings, &SettingsController::importBackupRequested, this, [this] {
        const QString path = NativeDialogs::openJson(m_window, QCoreApplication::translate("Miku", "导入备份"));
        if (path.isEmpty())
            return;
        QJsonObject backup;
        QString error;
        if (!Backup::load(path, &backup, &error)) {
            m_dialogs->message(QCoreApplication::translate("Miku", "无法导入"), error, QStringLiteral("warning"));
            return;
        }
        const QString text = Backup::summary(backup)
            + QCoreApplication::translate("Miku", "\n\n导入后将替换当前的设置、资源站点、下载记录和追番列表。软件会关闭并自动重新打开，进行中的下载会在重新打开后继续。");
        m_dialogs->ask(QCoreApplication::translate("Miku", "导入备份？"), text, QCoreApplication::translate("Miku", "导入并重启"), QCoreApplication::translate("Miku", "取消"), [this, backup](bool yes) {
            if (!yes)
                return;
            // Written after the app has stopped, so nothing still running can overwrite it.
            m_pendingRestore = backup;
            quitApp();
        });
    });
    connect(m_settings, &SettingsController::downloadFolderChanged, this, [this](const QString &from, const QString &to) {
        const auto plan = m_animeDownloads->planMove(from);
        if (plan.items == 0)
            return;
        QString text = (plan.anime > 0 ? QCoreApplication::translate("Miku", "原下载目录中还有 %1 部番剧的 %2 个下载").arg(plan.anime).arg(plan.items)
                                       : QCoreApplication::translate("Miku", "原下载目录中还有 %1 个下载").arg(plan.items))
            + QCoreApplication::translate("Miku", "。要一起移到新目录吗？正在下载的任务会在新位置继续。");
        if (plan.external > 0)
            text += QCoreApplication::translate("Miku", "\n\n另有 %1 个由外部下载器处理的任务不会移动，请在对应软件中自行处理。").arg(plan.external);
        m_dialogs->ask(QCoreApplication::translate("Miku", "移动已下载的番剧？"), text, QCoreApplication::translate("Miku", "一起移动"), QCoreApplication::translate("Miku", "留在原处"), [this, from, to](bool yes) {
            if (yes)
                m_animeDownloads->moveDownloads(from, to);
        });
    });
    connect(m_animeDownloads, &AnimeDownloads::moveFinished, this, [this](int moved, const QStringList &errors) {
        if (errors.isEmpty()) {
            m_dialogs->message(QCoreApplication::translate("Miku", "移动完成"), QCoreApplication::translate("Miku", "已把 %1 个下载移到新目录。").arg(moved));
            return;
        }
        m_dialogs->message(QCoreApplication::translate("Miku", "部分内容未能移动"),
            QCoreApplication::translate("Miku", "已移动 %1 个下载，以下内容仍在原目录：\n%2").arg(moved).arg(errors.mid(0, 8).join(QLatin1Char('\n'))),
            QStringLiteral("warning"));
    });
}

void AppController::buildTray()
{
    m_tray = new TrayIcon(trayItems(), this);
    connect(m_tray, &TrayIcon::activated, this, &AppController::showWindow);
    connect(m_tray, &TrayIcon::menuTriggered, this, [this](int index) {
        if (index == 0) showWindow();
        else if (index == 1) checkForNewEpisodes(true);
        else quitApp();
    });
    connect(m_tray, &TrayIcon::messageClicked, this, [this] {
        showWindow();
        if (m_noticePage >= 0)
            selectPage(m_noticePage);
    });
}

void AppController::showWindow()
{
    if (!m_window)
        return;
    if (m_window->visibility() == QWindow::Minimized) m_window->showNormal();
    else m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void AppController::quitApp()
{
    m_quitting = true;
    if (m_tray)
        m_tray->hide();
    qApp->quit();
}

void AppController::notify(const QString &title, const QString &text, int page)
{
    m_noticePage = page;
    m_tray->showMessage(title, text);
}

void AppController::applyGeneralSettings()
{
    const int hours = m_config->updateIntervalHours;
    if (hours > 0) m_periodicCheck->start(hours * 3600 * 1000);
    else m_periodicCheck->stop();
}

void AppController::requestClose()
{
    auto finish = [this](const QString &action) {
        if (action == QStringLiteral("tray")) {
            m_window->hide();
            if (!std::exchange(m_trayHintShown, true))
                notify(QCoreApplication::translate("Miku", "Miku Library 仍在运行"), QCoreApplication::translate("Miku", "下载和追更检查会在后台继续。点击托盘图标打开窗口，右键可以退出。"));
            return;
        }
        quitApp();
    };
    if (m_config->closeAction != QStringLiteral("ask")) {
        finish(m_config->closeAction);
        return;
    }
    m_dialogs->askClose(m_activeDownloads, [this, finish](const QString &choice, bool remember) {
        if (choice == QLatin1String("cancel"))
            return;
        if (remember) {
            m_config->closeAction = choice;
            m_config->save();
            m_settings->reloadGeneral();
        }
        finish(choice);
    });
}

void AppController::addNotice(const UpdateNotice &notice)
{
    for (int i = 0; i < m_notices.size(); ++i)
        if (m_notices.at(i).target.subject.bangumiId == notice.target.subject.bangumiId) {
            m_notices.removeAt(i);
            break;
        }
    m_notices.append(notice);
    m_noticesOpen = true;
    emit noticesChanged();
}

void AppController::removeNotice(int subjectId)
{
    for (int i = 0; i < m_notices.size(); ++i)
        if (m_notices.at(i).target.subject.bangumiId == subjectId) {
            m_notices.removeAt(i);
            break;
        }
    if (m_notices.isEmpty())
        m_noticesOpen = false;
    emit noticesChanged();
}

QVariantList AppController::notices() const
{
    QVariantList list;
    for (const auto &notice : m_notices) {
        QStringList kind = {notice.target.preference.group};
        if (!notice.target.preference.resolution.isEmpty())
            kind << notice.target.preference.resolution;
        kind << Format::episodeSummary(notice.episodes);
        list << QVariantMap{{"subjectId", notice.target.subject.bangumiId}, {"title", notice.target.subject.displayName()},
                            {"detail", kind.join(QStringLiteral(" · "))}};
    }
    return list;
}

QString AppController::noticeSummary() const
{
    int episodes = 0;
    for (const auto &notice : m_notices)
        episodes += notice.episodes.size();
    return QCoreApplication::translate("Miku", "%1 部番剧 · 共 %2 集").arg(m_notices.size()).arg(episodes);
}

void AppController::downloadNotice(int subjectId)
{
    for (const auto &notice : std::as_const(m_notices))
        if (notice.target.subject.bangumiId == subjectId) {
            m_animeDownloads->downloadGroup(notice.target.subject, notice.candidates, {notice.target.after + 1, 0});
            break;
        }
    removeNotice(subjectId);
}

void AppController::ignoreNotice(int subjectId)
{
    for (const auto &notice : std::as_const(m_notices))
        if (notice.target.subject.bangumiId == subjectId) {
            auto preference = m_localLibrary->preference(subjectId);
            for (const auto &episode : notice.episodes)
                preference.ignoredThrough = qMax(preference.ignoredThrough, int(episode.toDouble()));
            m_localLibrary->setPreference(subjectId, preference);
            break;
        }
    removeNotice(subjectId);
}

void AppController::downloadAllNotices()
{
    const auto notices = m_notices;
    for (const auto &notice : notices)
        downloadNotice(notice.target.subject.bangumiId);
}

void AppController::closeNotices()
{
    m_noticesOpen = false;
    emit noticesChanged();
}

void AppController::checkForNewEpisodes(bool manual)
{
    if (!m_updateChecker) {
        m_updateChecker = new UpdateChecker(m_catalog, m_http, m_bangumi, m_localLibrary, this);
        connect(m_updateChecker, &UpdateChecker::found, this, [this](const UpdateNotice &notice) {
            const QString what = QStringLiteral("%1 · %2").arg(notice.target.subject.displayName(), Format::episodeSummary(notice.episodes));
            if (notice.target.preference.autoDownload) {
                // Same request as the panel's 下载 button, without asking.
                m_autoQueuing = true;
                m_animeDownloads->downloadGroup(notice.target.subject, notice.candidates, {notice.target.after + 1, 0});
                m_autoQueuing = false;
                notify(QCoreApplication::translate("Miku", "已自动下载新集"), what, kDownloadsPage);
                return;
            }
            addNotice(notice);
            if (m_manualCheck)
                m_manualFound.append(notice);
            if (!m_window || !m_window->isVisible() || m_window->visibility() == QWindow::Minimized)
                notify(QCoreApplication::translate("Miku", "追番有新集"), what);
        });
        connect(m_updateChecker, &UpdateChecker::finished, this, [this](int checked, int withUpdates) {
            qInfo().noquote() << QStringLiteral("update check: %1 anime checked, %2 with new episodes").arg(checked).arg(withUpdates);
            if (!std::exchange(m_manualCheck, false))
                return;
            m_library->setChecking(false);
            const auto found = std::exchange(m_manualFound, {});
            const QString quiet = checked == 0 ? QCoreApplication::translate("Miku", "还没有可追更的番剧：先用某个字幕组下载一集，之后就会检查它的新集")
                : withUpdates == 0 ? QCoreApplication::translate("Miku", "已检查 %1 部番剧，暂无新集").arg(checked)
                : QCoreApplication::translate("Miku", "%1 部番剧的新集已自动开始下载").arg(withUpdates);
            if (!m_window || !m_window->isVisible() || m_window->visibility() == QWindow::Minimized) {   // started from the tray menu
                if (found.isEmpty())
                    notify(QCoreApplication::translate("Miku", "追更检查"), quiet);
                return;
            }
            if (found.isEmpty()) {
                m_dialogs->toast(quiet, checked == 0 ? QStringLiteral("info") : QStringLiteral("check"));
                return;
            }
            QStringList lines;
            for (int i = 0; i < found.size() && i < 5; ++i)
                lines << QStringLiteral("· %1  %2").arg(found.at(i).target.subject.displayName(), Format::episodeSummary(found.at(i).episodes));
            if (found.size() > 5)
                lines << QCoreApplication::translate("Miku", "· 等 %1 部").arg(found.size());
            const QString text = QCoreApplication::translate("Miku", "%1 部番剧有新集：\n%2\n\n现在按上次的字幕组和清晰度下载吗？选「稍后」可在右下角的提示中逐部处理。")
                .arg(found.size()).arg(lines.join(QLatin1Char('\n')));
            m_dialogs->ask(QCoreApplication::translate("Miku", "发现新集"), text, QCoreApplication::translate("Miku", "全部下载"), QCoreApplication::translate("Miku", "稍后"), [this, found](bool yes) {
                if (!yes)
                    return;
                for (const auto &notice : found)
                    downloadNotice(notice.target.subject.bangumiId);
            });
        });
    }
    // A manual click during the startup run just waits for that run to report.
    m_manualCheck = m_manualCheck || manual;
    if (manual)
        m_library->setChecking(true);
    if (!m_updateChecker->running())
        m_updateChecker->start();
}

QString AppController::engineName() const
{
    return m_config->downloader.selected == "builtin" ? QCoreApplication::translate("Miku", "内置下载器") : DownloaderSettings::name(m_config->downloader.selected);
}

void AppController::updateStatus()
{
    const bool builtin = m_config->downloader.selected == "builtin";
    const bool running = m_engine->running();
    int active = 0;
    qint64 down = 0, up = 0;
    for (const auto &task : m_engine->listTorrents()) {
        if (task.status == DownloadStatus::Downloading || task.status == DownloadStatus::Pending)
            ++active;
        down += task.dlSpeed;
        up += task.upSpeed;
    }
    m_activeDownloads = active;
    m_tray->setToolTip(active > 0 ? QCoreApplication::translate("Miku", "Miku Library · %1 个下载中 · ↓ %2").arg(active).arg(Format::speed(down))
                                  : QStringLiteral("Miku Library"));
    m_engineState = builtin && !running ? QStringLiteral("err") : active > 0 ? QStringLiteral("busy") : QStringLiteral("ok");
    if (builtin && !running) m_transfer = QCoreApplication::translate("Miku", "引擎未启动");
    else if (down > 0 || up > 0) m_transfer = QStringLiteral("↓ %1 · ↑ %2").arg(Format::speed(down), Format::speed(up));
    else if (active > 0) m_transfer = QCoreApplication::translate("Miku", "%1 个任务等待数据").arg(active);
    else m_transfer = builtin ? QCoreApplication::translate("Miku", "空闲") : QCoreApplication::translate("Miku", "新任务交给外部下载器");
    emit statusChanged();
    // Keep the speeds current while anything is transferring.
    if ((active > 0 || down > 0 || up > 0) && !m_badgeTimer->isActive())
        m_badgeTimer->start(2000);
}

void AppController::selectPage(int index)
{
    if (index < 0 || index >= pageNames().size() || index == kDetailPage)
        return;
    if (m_page == kDetailPage)
        m_detail->cancel();
    m_contextTitle = pageNames().at(index);
    m_currentNav = index;
    m_page = index;
    emit pageChanged();
    if (index == kSearchPage)
        emit focusSearchRequested();
}

void AppController::searchQuery(const QString &query)
{
    selectPage(kSearchPage);
    m_search->search(query);
}

void AppController::openSubjectById(int bangumiId)
{
    m_bangumi->getSubject(bangumiId, [this](AnimeSubject subject, QString error) {
        if (!error.isEmpty()) {
            m_pendingInput.clear();
            m_dialogs->message(QCoreApplication::translate("Miku", "番剧查询失败"), error, QStringLiteral("warning"));
            return;
        }
        openAnime(subject);
    });
}

void AppController::openAnime(const AnimeSubject &subject, bool localOnly)
{
    m_detailName = subject.displayName();
    m_contextTitle = detailTitle(m_detailName);
    if (m_page != kDetailPage)
        m_currentNav = m_page;
    m_page = kDetailPage;
    m_detail->showSubject(subject, localOnly);
    emit pageChanged();
    if (m_pendingInput.isEmpty())
        return;
    ResourceResult resource;
    const QString input = std::exchange(m_pendingInput, QString());
    if (input.startsWith("magnet:")) {
        resource.magnetUri = input;
        resource.infoHash = btihFromMagnet(input).value_or(QString());
    } else {
        resource.torrentUrl = input;
    }
    resource.title = importedTitle(input);
    if (resource.title.isEmpty())
        resource.title = subject.displayName();
    resource.tags = parseTitle(resource.title);
    resource.releaseGroup = resource.tags.releaseGroup;
    if (resource.releaseGroup.isEmpty())
        resource.releaseGroup = kUnknownGroup;
    onDownloadRequested(subject, resource);
}

void AppController::handleArguments(const QStringList &args)
{
    showWindow();
    auto value = [&](const QString &key) {
        const int i = args.indexOf(key);
        return i >= 0 && i + 1 < args.size() ? args.at(i + 1) : QString();
    };
    QString input = value("--add-magnet");
    if (input.isEmpty())
        input = value("--add-torrent");
    if (!input.isEmpty()) {
        if (!m_pendingInput.isEmpty()) {
            m_dialogs->message(QCoreApplication::translate("Miku", "等待选择番剧"), QCoreApplication::translate("Miku", "请先选择上一份导入资源所属的番剧。"));
            return;
        }
        m_pendingInput = input;
        const int id = value("--subject").toInt();
        if (id > 0) {
            openSubjectById(id);
            return;
        }
        QString title = importedTitle(input);
        QVector<AnimeSubject> matches;
        for (const auto &subject : m_db->cachedSubjects())
            if (!title.isEmpty() && isRelevant(title, subjectKeys(subject)))
                matches.append(subject);
        if (matches.size() == 1) {
            openAnime(matches.first(), false);
            return;
        }
        title.remove(QRegularExpression("\\[[^\\]]*\\]|【[^】]*】"));
        title.remove(QRegularExpression("\\s*(?:S\\d+E\\d+|[-–]\\s*\\d+).*$", QRegularExpression::CaseInsensitiveOption));
        searchQuery(title.trimmed());
        m_dialogs->message(QCoreApplication::translate("Miku", "选择资源所属番剧"), QCoreApplication::translate("Miku", "请在搜索结果中选择对应番剧，随后将按当前下载器和番剧目录导入资源。"));
        return;
    }
    const int id = value("--subject").toInt();
    if (id > 0)
        openSubjectById(id);
    const QString query = value("--search");
    if (!query.isEmpty())
        searchQuery(query);
}

QString AppController::backTitle() const
{
    return pageNames().value(m_currentNav, pageNames().first());
}

bool AppController::startedAgain()
{
    constexpr qint64 kMinimumGap = 60 * 1000;
    if (m_lastRelaunchCheck.isValid() && m_lastRelaunchCheck.elapsed() < kMinimumGap)
        return false;
    m_lastRelaunchCheck.start();
    qInfo().noquote() << QStringLiteral("started again: showing this window and checking for updates");
    if (m_config->checkAppUpdates)
        m_appUpdate->check();
    checkForNewEpisodes(false);
    m_home->load(true);
    return true;
}

void AppController::closeDetail()
{
    m_detail->cancel();
    selectPage(m_currentNav);
}

void AppController::onFollowChanged(const AnimeSubject &subject, bool following)
{
    if (following) {
        m_db->follow(subject.bangumiId);
        m_db->upsertSubject(subject);
        m_following.insert(subject.bangumiId);
    } else {
        m_db->unfollow(subject.bangumiId);
        m_following.remove(subject.bangumiId);
    }
}

void AppController::onDownloadRequested(const AnimeSubject &subject, const ResourceResult &result)
{
    m_db->upsertSubject(subject);
    m_animeDownloads->downloadOne(subject, result);
}

void AppController::updateDownloader()
{
    updateStatus();
    m_downloads->updateBackend();
}

QVariantList AppController::downloaderOptions() const
{
    const QHash<QString, QString> descriptions = {
        {"builtin", QCoreApplication::translate("Miku", "管理进度与本地视频")}, {"qbittorrent", QCoreApplication::translate("Miku", "自动传入番剧保存目录")},
        {"transmission", QCoreApplication::translate("Miku", "在软件内确认保存位置")}, {"aria2", QCoreApplication::translate("Miku", "支持本地合集的缺集选择")},
        {"bitcomet", QCoreApplication::translate("Miku", "静默添加种子任务")}, {"custom", QCoreApplication::translate("Miku", "选择其他程序与启动参数")}};
    QVariantList list;
    for (const auto &type : DownloaderSettings::types())
        list << QVariantMap{{"type", type}, {"name", DownloaderSettings::name(type)}, {"description", descriptions.value(type)},
                            {"icon", type == "builtin" ? "download" : type == "custom" ? "settings" : "external"},
                            {"current", m_config->downloader.selected == type},
                            {"configured", type == "builtin" || QFileInfo(m_config->downloader.programs.value(type)).isFile()}};
    return list;
}

void AppController::chooseDownloader(const QString &type)
{
    if (type == "configure") {
        selectPage(kSettingsPage);
        m_settings->editDownloader();
        return;
    }
    auto candidate = m_config->downloader;
    candidate.selected = type;
    if (type != "builtin" && candidate.programs.value(type).isEmpty())
        candidate.programs[type] = DownloaderSettings::detect(type);
    QString message;
    if (!ExternalDownloader::validate(candidate, &message)) {
        selectPage(kSettingsPage);
        m_settings->editDownloader(type);
        return;
    }
    const auto before = m_config->downloader;
    m_config->downloader = candidate;
    if (!m_config->save()) {
        m_config->downloader = before;
        m_dialogs->message(QCoreApplication::translate("Miku", "切换失败"), QCoreApplication::translate("Miku", "无法保存下载器设置。"), QStringLiteral("error"));
        return;
    }
    updateDownloader();
}

void AppController::importCatalog()
{
    const QString path = NativeDialogs::openJson(m_window, QCoreApplication::translate("Miku", "导入站点配置"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog->importFrom(path, &error)) {
        m_dialogs->message(QCoreApplication::translate("Miku", "导入失败"), error, QStringLiteral("warning"));
        return;
    }
    m_sites->refresh();
}

void AppController::exportCatalog()
{
    const QString path = NativeDialogs::saveJson(m_window, QCoreApplication::translate("Miku", "导出站点配置"), QStringLiteral("anime_sites_export.json"));
    if (!path.isEmpty())
        m_catalog->save(path);
}

void AppController::openNewVersion()
{
    if (m_newVersionPage.isValid())
        QDesktopServices::openUrl(m_newVersionPage);
}

void AppController::openDownloadFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_config->engine.savePath));
}

void AppController::playFiles(const QStringList &files, const QString &title)
{
    m_dialogs->chooseVideo(files, [this, title](const QString &file) {
        if (m_player->open(file, title, QFileInfo(file).completeBaseName())) {
            selectPage(kPlayerPage);
            return;
        }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(file)))
            m_dialogs->message(QCoreApplication::translate("Miku", "无法播放"), QCoreApplication::translate("Miku", "请为此视频格式设置默认播放器。"), QStringLiteral("warning"));
    });
}

void AppController::retranslate()
{
    m_contextTitle = m_page == kDetailPage ? detailTitle(m_detailName) : pageNames().value(m_page, pageNames().first());
    emit pageChanged();
    if (m_tray)
        m_tray->setItems(trayItems());
    updateStatus();
    // Lists and status lines were written in the old language; quiet reloads keep what is on screen.
    m_home->load(true);
    m_library->refresh();
    m_downloads->retranslate();
    m_sites->refresh();
    m_settings->retranslate();
    m_detail->retranslate();
}

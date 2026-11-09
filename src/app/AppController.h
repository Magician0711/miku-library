// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "BrowseControllers.h"
#include "DetailController.h"
#include "DialogHub.h"
#include "DownloadsController.h"
#include "SettingsController.h"
#include "SitesController.h"
#include "ThemeController.h"
#include "WindowChrome.h"
#include "core/Models.h"
#include "net/UpdateChecker.h"

#include <QElapsedTimer>

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantList>

#include <utility>

class AnimeDownloads;
class AppUpdate;
class BangumiClient;
#include "PlayerController.h"
class Catalog;
class Database;
class HttpClient;
class LocalLibrary;
class QQuickWindow;
class QTimer;
class SearchAggregator;
class TorrentEngine;
class TrayIcon;
struct AppConfig;

// The application as QML sees it: navigation, the sidebar status, notices, and the
// controller of each page.
class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(ThemeController *theme READ theme CONSTANT)
    Q_PROPERTY(DialogHub *dialogs READ dialogs CONSTANT)
    Q_PROPERTY(WindowChrome *chrome READ chrome CONSTANT)
    Q_PROPERTY(HomeController *home READ home CONSTANT)
    Q_PROPERTY(SearchController *search READ search CONSTANT)
    Q_PROPERTY(LibraryController *library READ library CONSTANT)
    Q_PROPERTY(DownloadsController *downloads READ downloads CONSTANT)
    Q_PROPERTY(SitesController *sites READ sites CONSTANT)
    Q_PROPERTY(SettingsController *settings READ settings CONSTANT)
    Q_PROPERTY(DetailController *detail READ detail CONSTANT)
    Q_PROPERTY(PlayerController *player READ player CONSTANT)
    Q_PROPERTY(int page READ page NOTIFY pageChanged)
    Q_PROPERTY(QString contextTitle READ contextTitle NOTIFY pageChanged)
    // The page that closing the detail page returns to.
    Q_PROPERTY(QString backTitle READ backTitle NOTIFY pageChanged)
    Q_PROPERTY(int activeDownloads READ activeDownloads NOTIFY statusChanged)
    Q_PROPERTY(QString engineName READ engineName NOTIFY statusChanged)
    Q_PROPERTY(QString engineState READ engineState NOTIFY statusChanged)
    Q_PROPERTY(QString transfer READ transfer NOTIFY statusChanged)
    Q_PROPERTY(QString newVersion READ newVersion NOTIFY newVersionChanged)
    Q_PROPERTY(QVariantList notices READ notices NOTIFY noticesChanged)
    Q_PROPERTY(QString noticeSummary READ noticeSummary NOTIFY noticesChanged)
    Q_PROPERTY(bool noticesOpen READ noticesOpen NOTIFY noticesChanged)
public:
    static constexpr int kSearchPage = 1;
    static constexpr int kLibraryPage = 2;
    static constexpr int kDownloadsPage = 3;
    static constexpr int kSitesPage = 4;
    static constexpr int kSettingsPage = 5;
    static constexpr int kDetailPage = 6;
    static constexpr int kPlayerPage = 7;

    AppController(Catalog *catalog, Database *db, AppConfig *config, HttpClient *http, TorrentEngine *engine,
                  SearchAggregator *aggregator, BangumiClient *bangumi, ThemeController *theme, WindowChrome *chrome,
                  QObject *parent = nullptr);

    ThemeController *theme() const { return m_theme; }
    DialogHub *dialogs() const { return m_dialogs; }
    WindowChrome *chrome() const { return m_chrome; }
    HomeController *home() const { return m_home; }
    SearchController *search() const { return m_search; }
    LibraryController *library() const { return m_library; }
    DownloadsController *downloads() const { return m_downloads; }
    SitesController *sites() const { return m_sites; }
    SettingsController *settings() const { return m_settings; }
    DetailController *detail() const { return m_detail; }
    PlayerController *player() const { return m_player; }
    int page() const { return m_page; }
    QString contextTitle() const { return m_contextTitle; }
    int activeDownloads() const { return m_activeDownloads; }
    QString engineName() const;
    QString engineState() const { return m_engineState; }
    QString transfer() const { return m_transfer; }
    QString newVersion() const { return m_newVersion; }
    QVariantList notices() const;
    QString noticeSummary() const;
    bool noticesOpen() const { return m_noticesOpen; }

    void setWindow(QQuickWindow *window);
    void handleArguments(const QStringList &arguments);
    // A backup chosen for import; written by main() once the app has stopped.
    QJsonObject takePendingRestore() { return std::exchange(m_pendingRestore, {}); }

    Q_INVOKABLE void selectPage(int index);
    Q_INVOKABLE void closeDetail();
    // The window's close button; asks or follows the saved choice.
    Q_INVOKABLE void requestClose();
    Q_INVOKABLE void openNewVersion();
    Q_INVOKABLE void openDownloadFolder();
    Q_INVOKABLE void checkForNewEpisodes(bool manual);
    // Another launch reached this copy: checks the app, followed anime and the schedule.
    // Repeated launches within a minute check once; false when this one was skipped.
    bool startedAgain();
    Q_INVOKABLE void downloadNotice(int subjectId);
    Q_INVOKABLE void ignoreNotice(int subjectId);
    Q_INVOKABLE void downloadAllNotices();
    Q_INVOKABLE void closeNotices();
    // Entries of the downloader menu: {type, name, description, current, configured}.
    Q_INVOKABLE QVariantList downloaderOptions() const;
    Q_INVOKABLE void chooseDownloader(const QString &type);

signals:
    void pageChanged();
    void statusChanged();
    void newVersionChanged();
    void noticesChanged();
    void focusSearchRequested();

private:
    void wire();
    void wireSettings();
    void buildTray();
    void showWindow();
    void quitApp();
    void notify(const QString &title, const QString &text, int page = -1);
    void applyGeneralSettings();
    void updateStatus();
    void updateDownloader();
    void openAnime(const AnimeSubject &subject, bool localOnly = false);
    void openSubjectById(int bangumiId);
    void searchQuery(const QString &query);
    void onFollowChanged(const AnimeSubject &subject, bool following);
    void onDownloadRequested(const AnimeSubject &subject, const ResourceResult &result);
    void addNotice(const UpdateNotice &notice);
    void removeNotice(int subjectId);
    void importCatalog();
    void exportCatalog();

    Catalog *m_catalog;
    Database *m_db;
    AppConfig *m_config;
    HttpClient *m_http;
    TorrentEngine *m_engine;
    SearchAggregator *m_aggregator;
    BangumiClient *m_bangumi;
    ThemeController *m_theme;
    WindowChrome *m_chrome;

    QQuickWindow *m_window = nullptr;
    DialogHub *m_dialogs;
    LocalLibrary *m_localLibrary;
    AnimeDownloads *m_animeDownloads;
    HomeController *m_home;
    PlayerController *m_player;
    SearchController *m_search;
    LibraryController *m_library;
    DownloadsController *m_downloads;
    SitesController *m_sites;
    SettingsController *m_settings;
    DetailController *m_detail;
    AppUpdate *m_appUpdate;
    UpdateChecker *m_updateChecker = nullptr;
    TrayIcon *m_tray = nullptr;
    QTimer *m_badgeTimer;
    QTimer *m_periodicCheck;
    QTimer *m_finishedTimer;

    QSet<int> m_following;
    QVector<UpdateNotice> m_notices;
    QVector<UpdateNotice> m_manualFound;   // notices of a manual check, offered for download at the end
    QStringList m_finished;
    QString m_contextTitle;
    QString m_engineState = QStringLiteral("ok");
    QString m_transfer;
    QString m_newVersion;
    QUrl m_newVersionPage;
    QString m_pendingInput;
    QJsonObject m_pendingRestore;
    int m_page = 0;
    int m_currentNav = 0;
    QElapsedTimer m_lastRelaunchCheck;
public:
    QString backTitle() const;
private:
    int m_activeDownloads = 0;
    int m_noticePage = -1;
    bool m_noticesOpen = false;
    bool m_manualCheck = false;
    bool m_quitting = false;
    bool m_trayHintShown = false;
    bool m_autoQueuing = false;   // auto downloads stay quiet: no page switch, no dialogs
};

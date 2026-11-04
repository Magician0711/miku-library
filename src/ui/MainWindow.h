// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/Models.h"

#include "FramelessWindow.h"
#include "Theme.h"
#include <QSet>
#include <QVector>

class Catalog;
class Database;
struct AppConfig;
class HttpClient;
class TorrentEngine;
class SearchAggregator;
class BangumiClient;
class ImageLoader;
class HomePage;
class SearchPage;
class LibraryPage;
class DownloadsPage;
class SitesPage;
class LocalLibrary;
class AnimeDownloads;
class SettingsPage;
class AnimeDetailPage;
class QStackedWidget;
class QPushButton;
class QLabel;
class QTimer;
class QToolButton;
class StatusDot;
class UpdateChecker;
class AppUpdate;
class UpdatePanel;

class MainWindow : public FramelessWindow {
    Q_OBJECT
public:
    MainWindow(Catalog *catalog, Database *db, AppConfig *config, HttpClient *http,
               TorrentEngine *engine, SearchAggregator *aggregator, BangumiClient *bangumi,
               ImageLoader *images, QWidget *parent = nullptr);

    void openSubjectById(int bangumiId);
    void searchQuery(const QString &query);
    void handleArguments(const QStringList &arguments);

private:
    static constexpr int kSearchPage = 1;
    static constexpr int kDownloadsPage = 3;
    static constexpr int kSettingsPage = 5;
    void buildUi();
    void openAnime(const AnimeSubject &subject, bool localOnly=false);
    void closeDetail();
    void onFollowChanged(const AnimeSubject &subject, bool following);
    void onDownloadRequested(const AnimeSubject &subject,const ResourceResult &result);
    void selectPage(int index);
    // Sidebar status: downloader name, live transfer speeds and the active-task badge.
    void updateStatus();
    // Runs the followed-group update check; manual runs report their result.
    void checkForNewEpisodes(bool manual);
    void setAppearance(Theme::Mode mode);
    void updateThemeButton();
    void showDownloaderMenu(QWidget *anchor);
    void updateDownloader();
    void importCatalog();
    void exportCatalog();

    Catalog *m_catalog;
    Database *m_db;
    AppConfig *m_config;
    HttpClient *m_http;
    TorrentEngine *m_engine;
    SearchAggregator *m_aggregator;
    BangumiClient *m_bangumi;
    ImageLoader *m_images;

    QStackedWidget *m_stack;
    QVector<QPushButton *> m_navButtons;
    QLabel *m_engineStatus;
    QLabel *m_transferStatus;
    StatusDot *m_engineDot;
    QLabel *m_downloadBadge;
    QTimer *m_badgeTimer;
    QToolButton *m_themeButton;
    QPushButton *m_newVersion = nullptr;
    AppUpdate *m_appUpdate = nullptr;
    UpdatePanel *m_updatePanel = nullptr;
    UpdateChecker *m_updateChecker = nullptr;
    bool m_manualCheck = false;

    HomePage *m_home;
    SearchPage *m_search;
    LibraryPage *m_library;
    DownloadsPage *m_downloads;
    SitesPage *m_sites;
    LocalLibrary *m_localLibrary;
    AnimeDownloads *m_animeDownloads;
    SettingsPage *m_settings;
    AnimeDetailPage *m_detail = nullptr;
    QSet<int> m_following;
    int m_currentNav = 0;
    QString m_pendingInput;
};

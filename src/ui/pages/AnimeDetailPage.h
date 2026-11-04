#pragma once

#include "core/Grouping.h"
#include "core/Models.h"

#include <QHash>
#include <QWidget>

#include <array>
#include <functional>
#include <optional>

class LocalLibrary;
class SearchAggregator;
class BangumiClient;
class Catalog;
class ImageLoader;
class EmptyState;
class StatusDot;
class FlowLayout;
class QLabel;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QVBoxLayout;
class QHBoxLayout;
class QTimer;
class QPushButton;
class QKeyEvent;
class QScrollArea;

class AnimeDetailPage : public QWidget {
    Q_OBJECT
public:
    AnimeDetailPage(SearchAggregator *aggregator, BangumiClient *bangumi, Catalog *catalog,
                    ImageLoader *images, LocalLibrary *library, std::function<bool(int)> isFollowing, QWidget *parent = nullptr);

    void showSubject(const AnimeSubject &subject,bool localOnly=false);
    void cancel();

signals:
    void backRequested();
    void downloadRequested(AnimeSubject subject,ResourceResult result);
    void groupDownloadRequested(AnimeSubject subject,QVector<ResourceResult> resources,EpisodeRange range);
    void resourcesDiscovered(AnimeSubject subject,QVector<ResourceResult> resources);
    void playRequested(AnimeSubject subject,ResourceResult result);
    void followChanged(AnimeSubject subject, bool following);
    void resourceSearchRequested(AnimeSubject subject);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct RowState { bool ready = false; bool active = false; bool external = false; };
    void buildHero(QVBoxLayout *layout);
    QWidget *buildResourceRow(const Release &release, const RowState &state, QWidget *parent);
    void updateFollowButton(bool following);
    void buildFilters(QVBoxLayout *layout);
    void buildSourceStrip(QVBoxLayout *layout);
    void refreshSources();
    void startSearch();
    void searchWith(const AnimeSubject &subject);
    void onBatch(QVector<ResourceResult> results);
    void onSiteStatus(SiteStatus status);
    void onFinished();
    void scheduleRender();
    void render();
    void clearGroups();
    void showGroupOptions(QWidget *anchor, const QString &group, const QVector<ResourceResult> &candidates);
    // Saves the group (and, when given, language / subtitle type / resolution)
    // as this anime's download preference for later one-click and update checks.
    void rememberChoice(const QString &group, std::optional<std::array<QString, 3>> kind);
    void resetFilters();
    void updateHero();
    void setSeasons(const QVector<int> &seasons);
    Filters filters() const;
    QString sortKey() const;

    LocalLibrary *m_library;
    bool m_localOnly=false;
    bool m_searching=false;
    bool m_fadeNextRender=false;
    QWidget *m_sourcePanel=nullptr;
    QLabel *m_sectionTitle=nullptr;
    QComboBox *m_downloadStatus=nullptr;
    SearchAggregator *m_aggregator;
    BangumiClient *m_bangumi;
    Catalog *m_catalog;
    ImageLoader *m_images;
    std::function<bool(int)> m_isFollowing;

    AnimeSubject m_subject;
    QVector<Release> m_releases;
    QHash<QString, QPair<StatusDot *, QLabel *>> m_dots;
    QTimer *m_renderTimer;
    QScrollArea *m_scroll = nullptr;
    QPushButton *m_resourceSearch;
    int m_searchGeneration=0;

    QLabel *m_poster;
    QLabel *m_title;
    QLabel *m_subtitle;
    QWidget *m_stats;
    QLabel *m_summary;
    QLabel *m_progress;
    QLabel *m_countLabel;
    QWidget *m_tagsBox;
    FlowLayout *m_tagsLayout;
    QPushButton *m_followButton;
    QPushButton *m_moreButton;
    QWidget *m_dotsBox;
    FlowLayout *m_dotsLayout;
    QWidget *m_groupsBox;
    QVBoxLayout *m_groupsLayout;
    EmptyState *m_empty;
    int m_statusDone = 0;
    int m_statusTotal = 0;
    int m_groupLimit = 3;
    QHash<QString, bool> m_groupExpanded;
    QHash<QString, bool> m_groupShowAll;

    QComboBox *m_season;
    QComboBox *m_resolution;
    QComboBox *m_subtitleFilter;
    QComboBox *m_kind;
    QComboBox *m_sort;
    QCheckBox *m_hdrOnly;
    QLineEdit *m_text;
    QWidget *m_filterBar;
};

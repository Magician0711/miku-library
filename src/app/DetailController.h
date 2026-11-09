// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/Grouping.h"
#include "core/Models.h"
#include "MapListModel.h"

#include <QHash>
#include <QObject>
#include <QVariantList>

#include <array>
#include <functional>
#include <optional>

class BangumiClient;
class Catalog;
class LocalLibrary;
class QTimer;
class SearchAggregator;

// One anime: its details, and the releases found for it, grouped and filtered.
// rows is a flat list of the visible group headers, releases and "show more" entries,
// so the view only creates what is on screen.
class DetailController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(MapListModel *rows READ rows CONSTANT)
    Q_PROPERTY(QVariantMap hero READ hero NOTIFY heroChanged)
    Q_PROPERTY(bool localOnly READ localOnly NOTIFY modeChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(QString progress READ progress NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QString count READ count NOTIFY resultsChanged)
    Q_PROPERTY(QString emptyTitle READ emptyTitle NOTIFY resultsChanged)
    Q_PROPERTY(QString emptyText READ emptyText NOTIFY resultsChanged)
    Q_PROPERTY(bool emptyVisible READ emptyVisible NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList seasons READ seasons NOTIFY seasonsChanged)
    // The anime's other seasons, arcs and films as {text, value, note}; value is the
    // subject id. Empty when Bangumi knows of no others.
    Q_PROPERTY(QVariantList series READ series NOTIFY seriesChanged)
    // Filter values, read and written by name: season, kind, resolution, subtitle,
    // status, sort, hdrOnly, text.
    Q_PROPERTY(QVariantMap filters READ filterValues NOTIFY filtersChanged)
    // Non-empty while the 选择下载方式 dialog is open.
    Q_PROPERTY(QVariantMap groupDialog READ groupDialog NOTIFY groupDialogChanged)
public:
    DetailController(SearchAggregator *aggregator, BangumiClient *bangumi, Catalog *catalog, LocalLibrary *library,
                     std::function<bool(int)> isFollowing, QObject *parent = nullptr);

    MapListModel *rows() const { return m_rows; }
    QVariantMap hero() const { return m_hero; }
    bool localOnly() const { return m_localOnly; }
    bool searching() const { return m_searching; }
    QString progress() const { return m_progress; }
    QVariantList sources() const;
    QString count() const { return m_count; }
    QString emptyTitle() const { return m_emptyTitle; }
    QString emptyText() const { return m_emptyText; }
    bool emptyVisible() const { return m_emptyVisible; }
    QVariantList seasons() const;
    QVariantList series() const;
    QVariantMap filterValues() const { return m_filters; }
    QVariantMap groupDialog() const { return m_groupDialog; }
    const AnimeSubject &subject() const { return m_subject; }

    void showSubject(const AnimeSubject &subject, bool localOnly = false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void switchSeries(int subjectId);
    // Takes Bangumi's answer for the current anime; tests call it to skip the network.
    void applySeries(const QVector<AnimeSubject> &found);

    Q_INVOKABLE void setFilter(const QString &name, const QVariant &value);
    Q_INVOKABLE void resetFilters();
    Q_INVOKABLE void researchClicked();
    Q_INVOKABLE void setFollowing(bool following);
    Q_INVOKABLE void setUpdatesMode(int mode);
    Q_INVOKABLE void toggleGroup(const QString &group);
    Q_INVOKABLE void toggleShowAll(const QString &group);
    Q_INVOKABLE void showAllGroups();
    Q_INVOKABLE void releaseAction(const QString &key);
    Q_INVOKABLE void openPage(const QString &url);
    Q_INVOKABLE void copyLink(const QString &key);

    Q_INVOKABLE void askGroupDownload(const QString &group);
    // {summary, ready} for the dialog's current choices.
    Q_INVOKABLE QVariantMap previewGroup(const QString &language, const QString &subtitleType, const QString &resolution,
                                         int first, int last) const;
    Q_INVOKABLE void confirmGroup(const QString &language, const QString &subtitleType, const QString &resolution,
                                  int first, int last);
    Q_INVOKABLE void dismissGroup();

    // Runs the coalesced render at once; tests use it to skip the delay.
    void renderNow();

signals:
    void heroChanged();
    void modeChanged();
    void searchingChanged();
    void sourcesChanged();
    void resultsChanged();
    void seasonsChanged();
    void seriesChanged();
    void filtersChanged();
    void groupDialogChanged();
    void downloadRequested(const AnimeSubject &subject, const ResourceResult &result);
    void groupDownloadRequested(const AnimeSubject &subject, const QVector<ResourceResult> &resources, const EpisodeRange &range);
    void resourcesDiscovered(const AnimeSubject &subject, const QVector<ResourceResult> &resources);
    void playRequested(const AnimeSubject &subject, const ResourceResult &result);
    void followChanged(const AnimeSubject &subject, bool following);
    void resourceSearchRequested(const AnimeSubject &subject);
    void animeRequested(const AnimeSubject &subject, bool localOnly);

private:
    struct RowState { bool ready = false; bool active = false; bool external = false; };
    struct Source { QString id, name, state, tip; };
    struct GroupChoice { QString language, subtitleType, resolution; EpisodeRange range; };

    void startSearch();
    void searchWith(const AnimeSubject &subject);
    void onBatch(QVector<ResourceResult> results);
    void onSiteStatus(const SiteStatus &status);
    void onFinished();
    void render();
    void refreshSources();
    void updateHero();
    void setSeasons(const QVector<int> &seasons);
    void lookupSeries();
    bool onMainEntry() const;
    // Which season this page narrows the releases to, or -1 for all of them.
    int seasonFilter() const;
    void setSearching(bool searching);
    void setResults(const QString &count, const QString &title, const QString &text, bool visible);
    Filters filters() const;
    QVariantMap releaseRow(const Release &release, const RowState &state, const QString &group, bool last);
    // Saves the group (and, when given, language / subtitle type / resolution)
    // as this anime's download preference for later update checks.
    void rememberChoice(const QString &group, std::optional<std::array<QString, 3>> kind);
    QVector<ResourceResult> applyChoice(const GroupChoice &choice) const;
    GroupChoice choice(const QString &language, const QString &subtitleType, const QString &resolution, int first, int last) const;

    SearchAggregator *m_aggregator;
    BangumiClient *m_bangumi;
    Catalog *m_catalog;
    LocalLibrary *m_library;
    std::function<bool(int)> m_isFollowing;
    MapListModel *m_rows;
    QTimer *m_renderTimer;

    AnimeSubject m_subject;
    QVector<Release> m_releases;
    QVector<Source> m_sources;
    QVector<int> m_seasons;
    QVector<SeriesEntry> m_series;
    QStringList m_seriesMarks;
    QVariantMap m_hero, m_filters, m_groupDialog;
    QHash<QString, ResourceResult> m_shown;             // rows on screen, by key
    QHash<QString, QVector<ResourceResult>> m_candidates;   // per group, by group name
    QHash<QString, bool> m_groupExpanded, m_groupShowAll;
    QString m_progress, m_count, m_emptyTitle, m_emptyText;
    QString m_dialogGroup;
    QVector<ResourceResult> m_dialogResources;
    int m_dialogLow = 1, m_dialogHigh = 1;
    int m_statusDone = 0, m_statusTotal = 0;
    int m_groupLimit = 3;
    int m_searchGeneration = 0;
    int m_seriesGeneration = 0;
    bool m_localOnly = false;
    bool m_searching = false;
    bool m_emptyVisible = true;
};

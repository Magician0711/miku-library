// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "DetailController.h"

#include "Format.h"
#include "core/Catalog.h"
#include "core/LocalLibrary.h"
#include "core/TitleParser.h"
#include "net/BangumiClient.h"
#include "net/SearchAggregator.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <memory>

namespace {
constexpr int kRowsPerGroup = 6;
constexpr int kInitialGroups = 3;
constexpr int kSeriesSearchLimit = 50;

const QStringList kRowRoles = {
    "key", "kind", "group", "text", "first", "last",
    // group headers
    "expanded", "bulkVisible", "bulkEnabled", "bulkBusy", "bulkIcon", "bulkText", "bulkTip", "headerTip",
    // releases
    "episode", "batch", "chips", "meta", "metaTip", "sourcePage", "copyVisible", "copyEnabled", "copyTip",
    "actionText", "actionIcon", "actionPrimary", "actionEnabled", "actionTip",
};

QVariantMap chip(const QString &text, const QString &bg, const QString &fg, const QString &tip = {})
{
    return {{"text", text}, {"bg", bg}, {"fg", fg}, {"tip", tip}};
}

QString groupTitle(const QString &name, qsizetype count)
{
    if (name == kUnknownGroup)
        return QStringLiteral("未归档资源    %1 个").arg(count);
    return QStringLiteral("%1    %2 个资源").arg(name).arg(count);
}

QVariantMap option(const QString &text, const QString &value)
{
    return {{"text", text}, {"value", value}};
}

QString seriesNote(const AnimeSubject &subject)
{
    QStringList parts{subject.year > 0 ? QString::number(subject.year) : QStringLiteral("未定档")};
    if (!subject.platform.isEmpty()) parts << subject.platform;
    return parts.join(QStringLiteral(" · "));
}
}

DetailController::DetailController(SearchAggregator *aggregator, BangumiClient *bangumi, Catalog *catalog, LocalLibrary *library,
                                   std::function<bool(int)> isFollowing, QObject *parent)
    : QObject(parent)
    , m_aggregator(aggregator), m_bangumi(bangumi), m_catalog(catalog), m_library(library)
    , m_isFollowing(std::move(isFollowing))
    , m_rows(new MapListModel(kRowRoles, "key", this))
{
    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(180);
    connect(m_renderTimer, &QTimer::timeout, this, &DetailController::render);
    resetFilters();

    connect(m_library, &LocalLibrary::changed, m_renderTimer, qOverload<>(&QTimer::start));
    connect(m_aggregator, &SearchAggregator::batchReady, this, &DetailController::onBatch);
    connect(m_aggregator, &SearchAggregator::siteFinished, this, &DetailController::onSiteStatus);
    connect(m_aggregator, &SearchAggregator::finished, this, &DetailController::onFinished);
    connect(m_aggregator, &SearchAggregator::started, this, [this](int count) {
        m_statusTotal = count;
        m_statusDone = 0;
        m_progress = QStringLiteral("检索中 0/%1").arg(count);
        emit sourcesChanged();
    });
}

QVariantList DetailController::sources() const
{
    QVariantList list;
    for (const auto &source : m_sources)
        list << QVariantMap{{"name", source.name}, {"state", source.state}, {"tip", source.tip}};
    return list;
}

QVariantList DetailController::seasons() const
{
    QVariantList list{QVariantMap{{"text", QStringLiteral("全部季度")}, {"value", -1}}};
    for (const int season : m_seasons)
        list << QVariantMap{{"text", QStringLiteral("第 %1 季").arg(season)}, {"value", season}};
    return list;
}

QVariantList DetailController::series() const
{
    QVariantList list;
    for (const auto &entry : m_series)
        list << QVariantMap{{"text", entry.label}, {"value", entry.subject.bangumiId}, {"note", seriesNote(entry.subject)}};
    return list;
}

void DetailController::setSearching(bool searching)
{
    if (searching == m_searching)
        return;
    m_searching = searching;
    emit searchingChanged();
}

void DetailController::setResults(const QString &count, const QString &title, const QString &text, bool visible)
{
    m_count = count;
    m_emptyTitle = title;
    m_emptyText = text;
    m_emptyVisible = visible;
    emit resultsChanged();
}

void DetailController::showSubject(const AnimeSubject &subject, bool localOnly)
{
    m_groupExpanded.clear();
    m_groupShowAll.clear();
    m_subject = subject;
    // Moving within a series keeps its list, so the switcher stays put.
    const bool sameSeries = std::any_of(m_series.cbegin(), m_series.cend(), [&subject](const SeriesEntry &entry) {
        return entry.subject.bangumiId == subject.bangumiId;
    });
    if (!sameSeries && !m_series.isEmpty()) {
        m_series.clear();
        m_seriesMarks.clear();
        emit seriesChanged();
    }
    if (!m_seasons.isEmpty()) {
        m_seasons.clear();
        emit seasonsChanged();
    }
    if (localOnly != m_localOnly) {
        m_localOnly = localOnly;
        emit modeChanged();
    }
    m_releases.clear();
    m_rows->update({});
    m_filters.clear();
    resetFilters();
    updateHero();
    if (!sameSeries)
        lookupSeries();
    startSearch();
}

void DetailController::lookupSeries()
{
    const int generation = ++m_seriesGeneration;
    const QStringList names = seriesNames(m_subject);
    if (m_subject.bangumiId == 0 || names.isEmpty())
        return;
    auto found = std::make_shared<QVector<AnimeSubject>>();
    auto waiting = std::make_shared<qsizetype>(names.size());
    QPointer<DetailController> self(this);
    for (const auto &name : names) {
        m_bangumi->search(name, kSeriesSearchLimit, [self, generation, found, waiting](QVector<AnimeSubject> subjects, QString) {
            if (!self || generation != self->m_seriesGeneration)
                return;
            *found += subjects;
            if (--*waiting == 0)
                self->applySeries(*found);
        });
    }
}

void DetailController::applySeries(const QVector<AnimeSubject> &found)
{
    ++m_seriesGeneration;
    const auto entries = seriesEntries(m_subject, found);
    m_series = entries.size() > 1 ? entries : QVector<SeriesEntry>{};
    m_seriesMarks = seriesMarks(m_series);
    emit seriesChanged();
    m_filters.insert("season", seasonFilter());
    emit filtersChanged();
    m_renderTimer->start();
}

void DetailController::switchSeries(int subjectId)
{
    if (subjectId == m_subject.bangumiId)
        return;
    for (const auto &entry : m_series) {
        if (entry.subject.bangumiId == subjectId) {
            // The library's view of one season says nothing about the next: when that
            // has no videos yet, what the viewer wants is its resources.
            emit animeRequested(entry.subject, m_localOnly && !m_library->localReleases(subjectId).isEmpty());
            return;
        }
    }
}

bool DetailController::onMainEntry() const
{
    return std::any_of(m_series.cbegin(), m_series.cend(), [this](const SeriesEntry &entry) {
        return entry.main && entry.subject.bangumiId == m_subject.bangumiId;
    });
}

int DetailController::seasonFilter() const
{
    const int own = LocalLibrary::subjectSeason(m_subject);
    const int numbered = m_seasons.contains(own) ? own : -1;
    if (m_series.isEmpty() || parseSeason(m_subject.nameCn) || parseSeason(m_subject.nameJp) || parseSeason(m_subject.nameEn))
        return numbered;
    // An arc has no number of its own, whatever the groups call it. The main entry is
    // season one, which groups usually leave unnumbered.
    if (!onMainEntry())
        return -1;
    const bool unnumbered = std::any_of(m_releases.cbegin(), m_releases.cend(), [](const Release &release) { return !release.season; });
    return numbered > 0 || unnumbered ? own : -1;
}

void DetailController::cancel()
{
    ++m_searchGeneration;
    ++m_seriesGeneration;
    m_aggregator->cancel();
    setSearching(false);
    m_renderTimer->stop();
}

void DetailController::resetFilters()
{
    m_filters = {{"season", seasonFilter()}, {"kind", QStringLiteral("all")}, {"resolution", QString()},
                 {"subtitle", QString()}, {"status", m_localOnly ? QStringLiteral("downloaded") : QStringLiteral("all")},
                 {"sort", QStringLiteral("popular")}, {"hdrOnly", false}, {"text", QString()}};
    emit filtersChanged();
    render();
}

void DetailController::setFilter(const QString &name, const QVariant &value)
{
    if (m_filters.value(name) == value)
        return;
    m_filters.insert(name, value);
    emit filtersChanged();
    m_renderTimer->start();
}

Filters DetailController::filters() const
{
    Filters out;
    const int season = m_filters.value("season").toInt();
    if (season > 0)
        out.season = season;
    out.resolution = m_filters.value("resolution").toString();
    out.subtitle = m_filters.value("subtitle").toString();
    out.kind = m_filters.value("kind").toString();
    out.hdrOnly = m_filters.value("hdrOnly").toBool();
    out.text = m_filters.value("text").toString().trimmed();
    return out;
}

void DetailController::setSeasons(const QVector<int> &seasons)
{
    m_seasons = seasons;
    emit seasonsChanged();
    m_filters.insert("season", seasonFilter());
    emit filtersChanged();
}

void DetailController::updateHero()
{
    QStringList alt;
    if (!m_subject.nameJp.isEmpty() && m_subject.nameJp != m_subject.displayName()) alt << m_subject.nameJp;
    if (!m_subject.nameEn.isEmpty() && m_subject.nameEn != m_subject.displayName()) alt << m_subject.nameEn;

    QVariantList stats;
    if (m_subject.rating > 0)
        stats << chip(QStringLiteral("★ %1").arg(m_subject.rating, 0, 'f', 1)
                          + (m_subject.rank > 0 ? QStringLiteral("  #%1").arg(m_subject.rank) : QString()),
                      "guessedBg", "guessedFg", QStringLiteral("Bangumi 评分与排名"));
    QStringList facts;
    if (m_subject.episodeCount > 0) facts << QStringLiteral("%1 话").arg(m_subject.episodeCount);
    if (!m_subject.airDate.isEmpty()) facts << m_subject.airDate;
    if (!m_subject.platform.isEmpty()) facts << m_subject.platform;
    if (!m_subject.studio.isEmpty()) facts << m_subject.studio;
    for (const auto &fact : facts)
        stats << chip(fact, "surface2", "text2");
    QVariantList tags;
    for (int i = 0; i < m_subject.tags.size() && i < 10; ++i)
        tags << chip(m_subject.tags.at(i), "accentSoft", "accentText");

    const auto preference = m_library->preference(m_subject.bangumiId);
    const int mode = !preference.follow ? 0 : preference.autoDownload ? 2 : 1;
    static const char *const labels[] = {"追更：关闭", "追更：提醒", "追更：自动下载"};

    m_hero = {
        {"subjectId", m_subject.bangumiId},
        {"title", m_subject.displayName()},
        {"subtitle", alt.join(QStringLiteral("  /  "))},
        {"stats", stats},
        {"tags", tags},
        {"summary", m_subject.summary.isEmpty() ? QStringLiteral("暂无简介。") : m_subject.summary},
        {"following", m_isFollowing(m_subject.bangumiId)},
        {"cover", BangumiClient::coverUrl(m_subject.coverUrl, 400)},
        {"bangumiUrl", m_subject.bangumiUrl},
        {"updatesVisible", !preference.group.isEmpty()},
        {"updatesMode", mode},
        {"updatesText", QString::fromUtf8(labels[mode])},
        {"updatesIcon", mode == 0 ? "minus" : mode == 2 ? "download" : "refresh"},
        {"updatesTip", mode == 0 ? QStringLiteral("不检查这部番剧的新集")
             : QStringLiteral("检查「%1」%2的新集，%3").arg(preference.group,
                   preference.resolution.isEmpty() ? QString() : QStringLiteral(" %1 ").arg(preference.resolution),
                   mode == 2 ? QStringLiteral("发现后直接下载") : QStringLiteral("发现后在右下角提醒"))},
    };
    emit heroChanged();
}

void DetailController::setFollowing(bool following)
{
    emit followChanged(m_subject, following);
    updateHero();
}

void DetailController::setUpdatesMode(int mode)
{
    auto preference = m_library->preference(m_subject.bangumiId);
    preference.follow = mode > 0;
    preference.autoDownload = mode == 2;
    m_library->setPreference(m_subject.bangumiId, preference);
    updateHero();
}

void DetailController::researchClicked()
{
    if (m_localOnly) {
        emit resourceSearchRequested(m_subject);
        return;
    }
    if (m_series.isEmpty())
        lookupSeries();
    resetFilters();
    startSearch();
}

void DetailController::refreshSources()
{
    m_sources.clear();
    for (const auto &site : m_catalog->sites(true))
        m_sources.append({site.id, site.name, QStringLiteral("busy"), QString()});
}

void DetailController::startSearch()
{
    if (m_subject.bangumiId == 0)
        return;
    m_aggregator->cancel();
    const int generation = ++m_searchGeneration;
    refreshSources();
    setSearching(true);
    m_releases.clear();
    m_groupLimit = kInitialGroups;
    m_statusDone = 0;
    m_statusTotal = m_sources.size();
    m_progress = QStringLiteral("检索中 0/%1").arg(m_statusTotal);
    emit sourcesChanged();
    render();
    if (m_localOnly) {
        setSearching(false);
        onFinished();
        return;
    }
    if (m_subject.aliases.isEmpty()) {
        const int id = m_subject.bangumiId;
        QPointer<DetailController> self(this);
        m_bangumi->getSubject(id, [self, id, generation](AnimeSubject full, QString error) {
            if (!self || self->m_subject.bangumiId != id || generation != self->m_searchGeneration)
                return;
            if (!error.isEmpty() || full.bangumiId != id) {
                self->searchWith(self->m_subject);
                return;
            }
            full.airWeekday = full.airWeekday > 0 ? full.airWeekday : self->m_subject.airWeekday;
            self->m_subject = full;
            self->updateHero();
            self->searchWith(self->m_subject);
        });
    } else {
        searchWith(m_subject);
    }
}

void DetailController::searchWith(const AnimeSubject &subject)
{
    m_aggregator->search(subject.nameCn.isEmpty() ? subject.displayName() : subject.nameCn, subject);
}

void DetailController::onBatch(QVector<ResourceResult> results)
{
    const auto keys = subjectKeys(m_subject);
    QVector<ResourceResult> relevant;
    for (auto &result : results) {
        if (!result.animeId.isEmpty() && result.animeId != QString::number(m_subject.bangumiId))
            continue;
        if (isRelevant(result.title, keys))
            relevant.push_back(std::move(result));
    }
    if (relevant.isEmpty())
        return;
    emit resourcesDiscovered(m_subject, relevant);
    QHash<QString, qsizetype> index;
    for (qsizetype i = 0; i < m_releases.size(); ++i)
        index.insert(m_releases.at(i).result.rowKey(), i);
    for (const auto &release : annotate(relevant)) {
        const QString key = release.result.rowKey();
        const auto found = index.constFind(key);
        if (found == index.cend()) {
            index.insert(key, m_releases.size());
            m_releases.append(release);
        } else {
            mergeResource(m_releases[found.value()].result, release.result);
        }
    }
    m_renderTimer->start();
}

void DetailController::onSiteStatus(const SiteStatus &status)
{
    ++m_statusDone;
    for (auto &source : m_sources) {
        if (source.id != status.siteId)
            continue;
        source.state = !status.ok ? QStringLiteral("err") : status.count > 0 ? QStringLiteral("ok") : QStringLiteral("warn");
        source.tip = QStringLiteral("%1 条 · %2s").arg(status.count).arg(status.elapsed, 0, 'f', 1);
        if (!status.error.isEmpty())
            source.tip += QStringLiteral("\n") + status.error;
    }
    m_progress = QStringLiteral("检索中 %1/%2").arg(m_statusDone).arg(m_statusTotal);
    emit sourcesChanged();
}

void DetailController::onFinished()
{
    setSearching(false);
    m_progress = QStringLiteral("已检索 %1 个站点").arg(m_statusTotal);
    emit sourcesChanged();
    setSeasons(seasonsPresent(m_releases + m_library->localReleases(m_subject.bangumiId)));
    render();
}

void DetailController::renderNow()
{
    m_renderTimer->stop();
    render();
}

QVariantMap DetailController::releaseRow(const Release &release, const RowState &state, const QString &group, bool last)
{
    const auto &result = release.result;
    QString episodeText;
    if (release.isBatch)
        episodeText = release.episode.isEmpty() || release.episode == QStringLiteral("合集") ? QStringLiteral("合集") : release.episode;
    else if (!release.episode.isEmpty())
        episodeText = QStringLiteral("第 %1 话").arg(release.episode);
    else
        episodeText = QStringLiteral("—");

    QStringList meta;
    if (result.fileSize > 0) meta << Format::size(result.fileSize);
    if (const QString age = Format::age(result.publishTime); !age.isEmpty()) meta << age;
    QStringList sites = result.seenOn.isEmpty() ? QStringList{result.siteName} : result.seenOn;
    sites.removeAll(QString());
    if (!sites.isEmpty()) meta << sites.join(QStringLiteral(" · "));

    const QString link = result.magnetUri.isEmpty() ? result.torrentUrl : result.magnetUri;
    QString action = QStringLiteral("下载");
    if (state.ready) action = QStringLiteral("播放");
    else if (state.external) action = QStringLiteral("已发送");
    else if (state.active) action = QStringLiteral("下载中");

    const QString key = QStringLiteral("r|%1|%2|%3").arg(group, result.rowKey(), result.localPath);
    m_shown.insert(key, result);
    return {
        {"key", key}, {"kind", "release"}, {"group", group}, {"text", result.title}, {"first", false}, {"last", last},
        {"episode", episodeText}, {"batch", release.isBatch}, {"chips", Format::qualityChips(result.tags)},
        {"meta", meta.join(QStringLiteral("  ·  "))},
        {"metaTip", sites.isEmpty() ? QString() : QStringLiteral("来源：") + sites.join(QStringLiteral("、"))},
        {"sourcePage", result.sourcePage},
        {"copyVisible", result.localPath.isEmpty()}, {"copyEnabled", !link.isEmpty()},
        {"copyTip", result.magnetUri.isEmpty() ? QStringLiteral("复制种子链接") : QStringLiteral("复制磁力链接")},
        {"actionText", action}, {"actionIcon", state.ready ? "play" : state.external ? "external" : "download"},
        {"actionPrimary", state.ready}, {"actionEnabled", state.ready || (!state.active && !link.isEmpty() && !m_localOnly)},
        {"actionTip", state.external ? QStringLiteral("已交给外部下载器，请在该软件中查看任务。") : QString()},
    };
}

void DetailController::render()
{
    if (m_subject.bangumiId == 0)
        return;

    const Filters filter = filters();
    const QString state = m_filters.value("status").toString();
    const int id = m_subject.bangumiId;

    // LocalLibrary lookups hash keys and stat files; resolve each row once per render.
    QHash<QString, RowState> states;
    auto stateOf = [&](const ResourceResult &result) {
        const QString key = result.rowKey() + QLatin1Char('\n') + result.localPath;
        auto found = states.constFind(key);
        if (found != states.cend())
            return found.value();
        RowState s;
        s.ready = m_library->downloaded(id, result);
        s.external = !s.ready && m_library->external(id, result);
        s.active = !s.ready && m_library->busy(id, result);
        states.insert(key, s);
        return s;
    };

    const auto local = m_library->localReleases(id);
    QVector<Release> pool = (m_localOnly || state == "downloaded") ? local : m_releases;
    if (!m_localOnly && state == "all") {
        QSet<QString> representedFiles;
        for (const auto &online : m_releases)
            if (stateOf(online.result).ready)
                for (const auto &path : m_library->playableFiles(id, online.result))
                    representedFiles.insert(path);
        for (const auto &r : local)
            if (!representedFiles.contains(r.result.localPath))
                pool.append(r);
    }
    // Every title of a later arc contains the main title, so its releases are found here
    // too; they are listed on the arc's own page.
    const bool mainEntry = onMainEntry();
    auto elsewhere = [&](const Release &release) {
        return mainEntry && release.result.localPath.isEmpty() && mentionsAny(release.result.title, m_seriesMarks);
    };
    QVector<Release> shown;
    for (const auto &release : pool) {
        if (state == "missing" && stateOf(release.result).ready)
            continue;
        if (passes(release, filter) && !elsewhere(release))
            shown.append(release);
    }

    m_candidates.clear();
    for (const auto &r : m_releases)
        if (passes(r, filter) && !elsewhere(r))
            m_candidates[releaseGroupKey(r.result.releaseGroup)].append(r.result);

    const auto groups = groupReleases(shown, m_filters.value("sort").toString(), subjectKeys(m_subject));
    m_shown.clear();
    QVector<QVariantMap> rows;
    const int limit = qMin<qsizetype>(groups.size(), m_groupLimit);
    for (int i = 0; i < limit; ++i) {
        const auto &group = groups.at(i);
        const bool unarchived = group.name == kUnknownGroup;
        // The merged bucket of unattributed releases stays folded unless opened.
        const bool expanded = m_groupExpanded.value(group.name, i < kInitialGroups && !unarchived);
        const bool showAll = m_groupShowAll.value(group.name, false);
        const bool history = m_library->groupHasHistory(id, group.name);
        const bool hasCandidates = !m_candidates.value(releaseGroupKey(group.name)).isEmpty();
        const int rowCount = !expanded ? 0 : showAll ? int(group.releases.size()) : qMin(int(group.releases.size()), kRowsPerGroup);
        const bool hasMore = expanded && group.releases.size() > kRowsPerGroup;

        rows.append({{"key", QStringLiteral("g|") + group.name}, {"kind", "group"}, {"group", group.name},
                     {"text", groupTitle(group.name, group.releases.size())}, {"first", true}, {"last", rowCount == 0},
                     {"expanded", expanded}, {"bulkVisible", !unarchived}, {"bulkEnabled", !m_searching && hasCandidates && !unarchived},
                     {"bulkBusy", m_searching && !unarchived}, {"bulkIcon", history ? "refresh" : "download"},
                     {"bulkText", history ? QStringLiteral("更新") : QStringLiteral("一键下载")},
                     {"bulkTip", m_searching ? QStringLiteral("正在检索最新话数…") : QStringLiteral("选择字幕、清晰度和集数，下载本字幕组缺少的正片")},
                     {"headerTip", unarchived ? QStringLiteral("未能识别字幕组的资源合并显示在这里，可单独下载，不支持一键下载。") : QString()}});
        for (int r = 0; r < rowCount; ++r)
            rows.append(releaseRow(group.releases.at(r), stateOf(group.releases.at(r).result), group.name, r == rowCount - 1 && !hasMore));
        if (hasMore)
            rows.append({{"key", QStringLiteral("m|") + group.name}, {"kind", "moreRows"}, {"group", group.name}, {"first", false}, {"last", true},
                         {"expanded", showAll},
                         {"text", showAll ? QStringLiteral("收起") : QStringLiteral("显示其余 %1 个资源").arg(group.releases.size() - kRowsPerGroup)}});
    }
    if (groups.size() > limit)
        rows.append({{"key", QStringLiteral("more")}, {"kind", "moreGroups"}, {"first", true}, {"last", true},
                     {"text", QStringLiteral("显示更多发布组（剩余 %1 个）").arg(groups.size() - limit)}});
    m_rows->update(rows);

    const QString count = QStringLiteral("%1 个资源 · %2 个发布组").arg(shown.size()).arg(groups.size())
        + (!m_localOnly && state == "all" && shown.size() != m_releases.size() ? QStringLiteral("（共 %1）").arg(m_releases.size()) : QString());
    if (!groups.isEmpty())
        setResults(count, {}, {}, false);
    else if (m_localOnly)
        setResults(count, QStringLiteral("还没有下载完成的视频"), QStringLiteral("下载完成的视频会按字幕组显示在这里。"), true);
    else if (m_searching && m_releases.isEmpty())
        setResults(QString(), QStringLiteral("正在搜索资源…"), QStringLiteral("从已启用的资源站并发检索，结果会逐步出现。"), true);
    else if (m_releases.isEmpty())
        setResults(count, QStringLiteral("没有找到资源"), QStringLiteral("各资源站均未返回与本作匹配的结果。可在「资源站点」检查站点状态。"), true);
    else
        setResults(count, QStringLiteral("没有符合筛选条件的资源"), QStringLiteral("试试放宽画质或字幕条件，或点击「重置筛选」。"), true);
}

void DetailController::toggleGroup(const QString &group)
{
    bool expanded = false;
    for (const auto &row : m_rows->rows())
        if (row.value("kind") == "group" && row.value("group") == group)
            expanded = row.value("expanded").toBool();
    m_groupExpanded.insert(group, !expanded);
    render();
}

void DetailController::toggleShowAll(const QString &group)
{
    m_groupShowAll.insert(group, !m_groupShowAll.value(group, false));
    render();
}

void DetailController::showAllGroups()
{
    m_groupLimit = std::numeric_limits<int>::max();
    render();
}

void DetailController::releaseAction(const QString &key)
{
    const auto found = m_shown.constFind(key);
    if (found == m_shown.cend())
        return;
    const auto result = found.value();
    if (m_library->downloaded(m_subject.bangumiId, result)) {
        emit playRequested(m_subject, result);
        return;
    }
    rememberChoice(result.releaseGroup, std::array{result.tags.subtitleLang.section(QLatin1Char(','), 0, 0),
                                                   result.tags.subtitleType, result.tags.resolution});
    emit downloadRequested(m_subject, result);
}

void DetailController::openPage(const QString &url)
{
    if (!url.isEmpty())
        QDesktopServices::openUrl(QUrl(url));
}

void DetailController::copyLink(const QString &key)
{
    const auto found = m_shown.constFind(key);
    if (found != m_shown.cend())
        QGuiApplication::clipboard()->setText(found->magnetUri.isEmpty() ? found->torrentUrl : found->magnetUri);
}

void DetailController::rememberChoice(const QString &group, std::optional<std::array<QString, 3>> kind)
{
    if (group.isEmpty() || group == kUnknownGroup || m_subject.bangumiId == 0)
        return;
    auto preference = m_library->preference(m_subject.bangumiId);
    const bool sameGroup = releaseGroupKey(preference.group) == releaseGroupKey(group);
    if (!sameGroup) {                          // a new group starts with no filters or dismissals
        const bool follow = preference.follow;
        preference = {};
        preference.follow = follow;
    }
    preference.group = group;
    if (kind) {
        preference.language = (*kind)[0];
        preference.subtitleType = (*kind)[1];
        preference.resolution = (*kind)[2];
    }
    m_library->setPreference(m_subject.bangumiId, preference);
    updateHero();
}

void DetailController::askGroupDownload(const QString &group)
{
    m_dialogGroup = group;
    m_dialogResources = m_candidates.value(releaseGroupKey(group));

    QHash<QString, int> langs, types, resolutions;
    QSet<int> episodes;
    for (const auto &r : m_dialogResources) {
        const auto list = r.tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto &lang : list) ++langs[lang];
        if (list.contains(QStringLiteral("zh-Hans")) && list.contains(QStringLiteral("zh-Hant"))) ++langs[QStringLiteral("bilingual")];
        if (!r.tags.subtitleType.isEmpty()) ++types[r.tags.subtitleType];
        if (!r.tags.resolution.isEmpty()) ++resolutions[r.tags.resolution];
    }
    for (const auto &release : annotate(m_dialogResources))
        for (const auto &ep : episodeNumbers(release.episode))
            episodes.insert(ep.toInt());

    auto fill = [](const QString &any, const QVector<QPair<QString, QString>> &known, const QHash<QString, int> &counts) {
        QVariantList list{option(any, QString())};
        for (const auto &[value, name] : known)
            if (counts.value(value) > 0)
                list << option(QStringLiteral("%1（%2）").arg(name).arg(counts.value(value)), value);
        return list;
    };
    m_dialogLow = episodes.isEmpty() ? 1 : *std::min_element(episodes.cbegin(), episodes.cend());
    m_dialogHigh = episodes.isEmpty() ? 1 : *std::max_element(episodes.cbegin(), episodes.cend());
    const int low = m_dialogLow, high = m_dialogHigh;

    QVariantList quick;
    quick << QVariantMap{{"text", QStringLiteral("全部 %1–%2 话").arg(low).arg(high)}, {"from", low}, {"to", high}};
    if (high - low >= 12) quick << QVariantMap{{"text", QStringLiteral("最新 12 话")}, {"from", high - 11}, {"to", high}};
    if (high - low >= 50) quick << QVariantMap{{"text", QStringLiteral("最新 50 话")}, {"from", high - 49}, {"to", high}};

    // Start from the last choice made for this group, where it still applies.
    const auto saved = m_library->preference(m_subject.bangumiId);
    const bool sameGroup = releaseGroupKey(saved.group) == releaseGroupKey(group);
    m_groupDialog = {
        {"group", group},
        {"languages", fill(QStringLiteral("不限语言"), {{"zh-Hans", QStringLiteral("简体中文")}, {"zh-Hant", QStringLiteral("繁体中文")},
             {"bilingual", QStringLiteral("简繁都有")}, {"zh", QStringLiteral("中文")}, {"ja", QStringLiteral("日文")}, {"en", QStringLiteral("英文")}}, langs)},
        {"types", fill(QStringLiteral("不限形式"), {{"internal", QStringLiteral("内封字幕")}, {"hardcoded", QStringLiteral("内嵌字幕")},
             {"external", QStringLiteral("外挂字幕")}, {"ass", QStringLiteral("ASS")}, {"srt", QStringLiteral("SRT")}, {"pgs", QStringLiteral("PGS")}}, types)},
        {"resolutions", fill(QStringLiteral("不限（自动选最高）"), {{"2160p", QStringLiteral("4K")}, {"1440p", QStringLiteral("1440p")},
             {"1080p", QStringLiteral("1080p")}, {"720p", QStringLiteral("720p")}, {"480p", QStringLiteral("480p")}}, resolutions)},
        {"low", low}, {"high", high}, {"quick", quick}, {"hasEpisodes", !episodes.isEmpty()},
        {"language", sameGroup ? saved.language : QString()},
        {"subtitleType", sameGroup ? saved.subtitleType : QString()},
        {"resolution", sameGroup ? saved.resolution : QString()},
    };
    emit groupDialogChanged();
}

DetailController::GroupChoice DetailController::choice(const QString &language, const QString &subtitleType, const QString &resolution,
                                                       int first, int last) const
{
    GroupChoice c{language, subtitleType, resolution, {}};
    // The whole span means no limit, which also keeps releases of unknown episodes.
    if (m_groupDialog.value("hasEpisodes").toBool() && (first != m_dialogLow || last != m_dialogHigh))
        c.range = {first, last};
    return c;
}

QVector<ResourceResult> DetailController::applyChoice(const GroupChoice &choice) const
{
    QVector<ResourceResult> out;
    for (const auto &r : m_dialogResources) {
        const auto langs = r.tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (choice.language == QLatin1String("bilingual")) {
            if (!langs.contains(QStringLiteral("zh-Hans")) || !langs.contains(QStringLiteral("zh-Hant"))) continue;
        } else if (!choice.language.isEmpty() && !langs.contains(choice.language)) {
            continue;
        }
        if (!choice.subtitleType.isEmpty() && r.tags.subtitleType != choice.subtitleType) continue;
        if (!choice.resolution.isEmpty() && r.tags.resolution != choice.resolution) continue;
        out.append(r);
    }
    return out;
}

QVariantMap DetailController::previewGroup(const QString &language, const QString &subtitleType, const QString &resolution,
                                           int first, int last) const
{
    const auto c = choice(language, subtitleType, resolution, first, last);
    const auto filtered = applyChoice(c);
    QString reason;
    const auto plans = m_library->plan(m_subject, filtered, &reason, false, c.range);
    QString text;
    if (plans.isEmpty()) {
        text = reason.isEmpty() ? QStringLiteral("没有需要下载的集数（已下载、下载中或无对应资源）") : reason;
    } else {
        int episodes = 0;
        for (const auto &plan : plans)
            episodes += plan.episodes.isEmpty() ? plan.onlyEpisodes.size() : plan.episodes.size();
        text = episodes > 0 ? QStringLiteral("预计新建 %1 个任务，共 %2 集").arg(plans.size()).arg(episodes)
                            : QStringLiteral("预计新建 %1 个任务").arg(plans.size());
    }
    return {{"summary", QStringLiteral("符合条件 %1 个资源 · %2").arg(filtered.size()).arg(text)}, {"ready", !plans.isEmpty()}};
}

void DetailController::confirmGroup(const QString &language, const QString &subtitleType, const QString &resolution, int first, int last)
{
    const auto c = choice(language, subtitleType, resolution, first, last);
    const auto resources = applyChoice(c);
    const QString group = m_dialogGroup;
    dismissGroup();
    rememberChoice(group, std::array{c.language, c.subtitleType, c.resolution});
    emit groupDownloadRequested(m_subject, resources, c.range);
}

void DetailController::dismissGroup()
{
    m_groupDialog.clear();
    emit groupDialogChanged();
}

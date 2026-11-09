// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "BrowseControllers.h"

#include "core/Database.h"
#include "core/LocalLibrary.h"
#include "net/BangumiClient.h"

#include <QDate>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>

namespace {
const QStringList kDayNames = {QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
                               QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
                               QStringLiteral("周日")};
const QString kIdleStatus = QStringLiteral("支持原名与别名检索");

// Bangumi seasons start in January, April, July and October.
QString seasonLabel()
{
    const QDate today = QDate::currentDate();
    return QStringLiteral("%1 年 %2 月新番").arg(today.year()).arg((today.month() - 1) / 3 * 3 + 1);
}

// Newest year first, then the better rated within a year; undated anime go last.
QString dateKey(const AnimeSubject &s)
{
    return !s.airDate.isEmpty() ? s.airDate : s.year > 0 ? QString::number(s.year) : QString();
}
bool newer(const AnimeSubject &a, const AnimeSubject &b)
{
    const QString da = dateKey(a), db = dateKey(b);
    if (da.isEmpty() != db.isEmpty()) return !da.isEmpty();
    if (da.left(4) != db.left(4)) return da.left(4) > db.left(4);
    if (a.rating != b.rating) return a.rating > b.rating;
    return da > db;
}
bool betterRated(const AnimeSubject &a, const AnimeSubject &b)
{
    if ((a.rating > 0) != (b.rating > 0)) return a.rating > 0;
    if (a.rating != b.rating) return a.rating > b.rating;
    return newer(a, b);
}
}

BrowseController::BrowseController(QObject *parent)
    : QObject(parent)
    , m_model(new MapListModel({"subjectId", "title", "meta", "rating", "cover"}, "subjectId", this))
{
}

void BrowseController::setStatus(const QString &text, const QString &tip)
{
    m_status = text;
    m_statusTip = tip;
    emit statusChanged();
}

void BrowseController::setEmpty(const QString &title, const QString &text)
{
    m_emptyTitle = title;
    m_emptyText = text;
    m_emptyVisible = true;
    emit emptyChanged();
}

void BrowseController::hideEmpty()
{
    m_emptyVisible = false;
    emit emptyChanged();
}

void BrowseController::setSubjects(const QVector<AnimeSubject> &subjects)
{
    m_subjects.clear();
    QVector<QVariantMap> rows;
    rows.reserve(subjects.size());
    for (const auto &subject : subjects) {
        if (m_subjects.contains(subject.bangumiId))
            continue;
        m_subjects.insert(subject.bangumiId, subject);
        QStringList meta;
        if (subject.year > 0) meta << QString::number(subject.year);
        if (!subject.platform.isEmpty()) meta << subject.platform;
        if (subject.episodeCount > 0) meta << QStringLiteral("%1 话").arg(subject.episodeCount);
        rows.append({{"subjectId", subject.bangumiId}, {"title", subject.displayName()},
                     {"meta", meta.join(QStringLiteral(" · "))},
                     {"rating", subject.rating > 0 ? QStringLiteral("★ %1").arg(subject.rating, 0, 'f', 1) : QString()},
                     {"cover", BangumiClient::coverUrl(subject.coverUrl, 480)}});
    }
    m_model->update(rows);
}

void BrowseController::open(int subjectId)
{
    const auto found = m_subjects.constFind(subjectId);
    if (found != m_subjects.cend())
        emit animeSelected(found.value());
}

HomeController::HomeController(BangumiClient *bangumi, QObject *parent)
    : BrowseController(parent), m_bangumi(bangumi)
{
    setStatus(seasonLabel());
    setEmpty(QStringLiteral("正在准备放送表"), QStringLiteral("新一周的故事，即将开始。"));
}

QVariantList HomeController::days() const
{
    QVariantList list;
    for (int day = 1; day <= 7; ++day) {
        const auto count = m_calendar.value(day).size();
        const QString &name = kDayNames.at(day - 1);
        list << QVariantMap{{"day", day}, {"text", count > 0 ? QStringLiteral("%1  %2").arg(name).arg(count) : name},
                            {"tip", day == QDate::currentDate().dayOfWeek() ? QStringLiteral("今天") : QString()}};
    }
    return list;
}

void HomeController::load(bool quiet)
{
    if (!quiet) {
        setStatus(QStringLiteral("加载中…"));
        setEmpty(QStringLiteral("加载中…"), QStringLiteral("正在从 Bangumi 获取本季放送表。"));
        setSubjects({});
    }

    const int generation = ++m_generation;
    QPointer<HomeController> self(this);
    m_bangumi->calendar([this, self, generation, quiet](BangumiClient::CalendarDays days, QString error, QDateTime cachedAt) {
        if (!self || generation != m_generation)
            return;
        if (quiet && (!error.isEmpty() || cachedAt.isValid()))
            return;
        if (!error.isEmpty()) {
            setStatus(QStringLiteral("加载失败"));
            setEmpty(QStringLiteral("加载失败"), error + QStringLiteral("\n点击右上角刷新重试。"));
            return;
        }
        m_calendar.clear();
        int total = 0;
        for (const auto &[weekday, items] : days) {
            m_calendar.insert(weekday, items);
            total += items.size();
        }
        m_loaded = true;
        setStatus(cachedAt.isValid()
                      ? QStringLiteral("离线数据 · 更新于 %1 · 共 %2 部").arg(cachedAt.toString(QStringLiteral("MM-dd HH:mm"))).arg(total)
                      : QStringLiteral("%1 · 共 %2 部").arg(seasonLabel()).arg(total),
                  cachedAt.isValid() ? QStringLiteral("暂时无法连接 Bangumi，显示上次成功加载的放送表。点击右上角刷新重试。") : QString());
        emit daysChanged();
        QVector<AnimeSubject> all;
        for (const auto &[weekday, items] : days)
            all += items;
        std::stable_sort(all.begin(), all.end(), [](const AnimeSubject &a, const AnimeSubject &b) { return a.rating > b.rating; });
        emit topRatedChanged(all.mid(0, 24));
        const int today = QDate::currentDate().dayOfWeek();
        const int wanted = quiet && m_calendar.contains(m_day) ? m_day : today;
        showDay(m_calendar.contains(wanted) ? wanted : m_calendar.keys().value(0, 1));
    });
}

void HomeController::showDay(int weekday)
{
    if (m_day != weekday) {
        m_day = weekday;
        emit dayChanged();
    }
    const auto subjects = m_calendar.value(weekday);
    setSubjects(subjects);
    const AnimeSubject *best = nullptr;
    for (const auto &subject : subjects)
        if (!subject.coverUrl.isEmpty() && (!best || subject.rating > best->rating))
            best = &subject;
    m_featured.clear();
    if (best) {
        QStringList meta;
        if (best->rating > 0) meta << QStringLiteral("★ %1").arg(best->rating, 0, 'f', 1);
        if (best->year > 0) meta << QString::number(best->year);
        if (!best->platform.isEmpty()) meta << best->platform;
        if (best->episodeCount > 0) meta << QStringLiteral("%1 话").arg(best->episodeCount);
        m_featured = {{"subjectId", best->bangumiId}, {"title", best->displayName()}, {"meta", meta.join(QStringLiteral(" · "))},
                      {"summary", best->summary}, {"cover", BangumiClient::coverUrl(best->coverUrl, 480)}};
    }
    emit featuredChanged();
    if (subjects.isEmpty())
        setEmpty(QStringLiteral("%1暂无放送").arg(kDayNames.value(weekday - 1)), QStringLiteral("换一天看看，或点击刷新。"));
    else
        hideEmpty();
}

SearchController::SearchController(BangumiClient *bangumi, QObject *parent)
    : BrowseController(parent), m_bangumi(bangumi)
{
    reset();
}

void SearchController::setSort(int sort)
{
    if (sort == m_sort)
        return;
    m_sort = sort;
    emit sortChanged();
    if (!m_found.isEmpty())
        showSorted();
}

void SearchController::search(const QString &text)
{
    const QString query = text.trimmed();
    if (query != m_query) {
        m_query = query;
        emit queryChanged();
    }
    if (query.isEmpty()) {
        reset();
        return;
    }
    setSubjects({});
    m_found.clear();
    setStatus(QStringLiteral("搜索中…"));
    setEmpty(QStringLiteral("搜索中…"), QStringLiteral("Bangumi 查询「%1」").arg(query));

    const int generation = ++m_generation;
    QPointer<SearchController> self(this);
    m_bangumi->search(query, 48, [this, self, generation](QVector<AnimeSubject> subjects, QString error) {
        if (!self || generation != m_generation)
            return;
        if (!error.isEmpty()) {
            setStatus(QStringLiteral("搜索失败：") + error);
            setEmpty(QStringLiteral("搜索失败"), error);
            return;
        }
        if (subjects.isEmpty()) {
            setStatus(QStringLiteral("未找到匹配的番剧"));
            setEmpty(QStringLiteral("未找到匹配的番剧"), QStringLiteral("换个关键字或使用日文原名试试。"));
            return;
        }
        m_found = subjects;
        showSorted();
        setStatus(QStringLiteral("找到 %1 部番剧").arg(subjects.size()));
        hideEmpty();
    });
}

void SearchController::showSorted()
{
    auto subjects = m_found;
    if (m_sort == Newest) std::stable_sort(subjects.begin(), subjects.end(), newer);
    else if (m_sort == Rating) std::stable_sort(subjects.begin(), subjects.end(), betterRated);
    setSubjects(subjects);
}

void SearchController::reset()
{
    ++m_generation;
    if (!m_query.isEmpty()) {
        m_query.clear();
        emit queryChanged();
    }
    m_found.clear();
    setSubjects(m_suggestions);
    if (m_suggestions.isEmpty()) {
        setStatus(kIdleStatus);
        setEmpty(QStringLiteral("想看些什么？"), QStringLiteral("输入番名开始搜索，进入作品后可查看各站资源。"));
    } else {
        setStatus(QStringLiteral("本季评分最高的番剧"));
        hideEmpty();
    }
}

void SearchController::setSuggestions(const QVector<AnimeSubject> &subjects)
{
    m_suggestions = subjects;
    if (m_query.isEmpty())
        reset();
}

LibraryController::LibraryController(Database *db, BangumiClient *bangumi, LocalLibrary *library, QObject *parent)
    : BrowseController(parent), m_db(db), m_bangumi(bangumi), m_library(library)
{
    // Library changes and metadata fetches can arrive in bursts; refresh once per burst.
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(50);
    connect(m_refreshTimer, &QTimer::timeout, this, &LibraryController::refresh);
    connect(library, &LocalLibrary::changed, this, [this] { if (m_active) m_refreshTimer->start(); });
    setEmpty(QStringLiteral("你的追番，从这里开始"), QStringLiteral("在作品详情中点击「追番」，就能在这里找到它。"));
}

void LibraryController::setScope(int scope)
{
    if (scope == m_scope)
        return;
    m_scope = scope;
    emit scopeChanged();
    refresh();
}

void LibraryController::setChecking(bool checking)
{
    if (checking == m_checking)
        return;
    m_checking = checking;
    emit checkingChanged();
}

void LibraryController::setActive(bool active)
{
    m_active = active;
    if (active)
        refresh();
}

void LibraryController::refresh()
{
    const auto followed = m_db->followedIds();
    QHash<int, AnimeSubject> local;
    for (const auto &subject : m_library->subjects())
        local.insert(subject.bangumiId, subject);

    QVector<int> ids;
    if (m_scope != Local) ids = followed;
    if (m_scope != Followed)
        for (auto it = local.cbegin(); it != local.cend(); ++it)
            if (!ids.contains(it.key())) ids.append(it.key());

    setStatus(QStringLiteral("%1 部收藏 · %2 部已下载").arg(followed.size()).arg(local.size()));
    if (ids.isEmpty()) {
        setSubjects({});
        if (m_scope == Local) setEmpty(QStringLiteral("还没有下载完成的番剧"), QStringLiteral("下载完成的视频会出现在这里，可直接播放。"));
        else setEmpty(QStringLiteral("你的追番，从这里开始"), QStringLiteral("在作品详情中点击「追番」，就能在这里找到它。"));
        return;
    }
    hideEmpty();

    QVector<AnimeSubject> subjects;
    QPointer<LibraryController> self(this);
    for (const int id : ids) {
        if (auto subject = m_db->getSubject(id)) { subjects.push_back(*subject); continue; }
        if (local.contains(id)) { subjects.push_back(local.value(id)); continue; }
        if (m_fetching.contains(id)) continue;
        m_fetching.insert(id);
        m_bangumi->getSubject(id, [self, id](AnimeSubject subject, QString error) {
            if (!self) return;
            self->m_fetching.remove(id);
            if (error.isEmpty() && subject.bangumiId == id) {
                self->m_db->upsertSubject(subject);
                self->m_refreshTimer->start();
            }
        });
    }
    setSubjects(subjects);
}

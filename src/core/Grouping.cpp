// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Grouping.h"

#include "TitleParser.h"

#include <QHash>
#include <algorithm>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

const QString kUnknownGroup = QStringLiteral("未标注发布组");

namespace {

QHash<QChar, QChar> traditionalMap()
{
    static const QString traditional = QStringLiteral(
        "蓮葉戀愛們嗎說書學會體區國園無與這時開關門頭東車長見現實發話後劇場島龍鬥戰魔術師漢語雙內簡繁圖彈靈幣鐵貓獸傳紀錄");
    static const QString simplified = QStringLiteral(
        "莲叶恋爱们吗说书学会体区国园无与这时开关门头东车长见现实发话后剧场岛龙斗战魔术师汉语双内简繁图弹灵币铁猫兽传纪录");
    QHash<QChar, QChar> map;
    for (int i = 0; i < traditional.size() && i < simplified.size(); ++i)
        map.insert(traditional.at(i), simplified.at(i));
    return map;
}

QString fold(const QString &input)
{
    static const QHash<QChar, QChar> map = traditionalMap();
    static const QRegularExpression strip(
        QStringLiteral("[\\s\\-_:：꞉∶·・!！?？'\"’‘「」『』（）()\\[\\]【】/\\\\.,，。~～]+"));

    QString out = input.normalized(QString::NormalizationForm_KC).toLower();
    for (auto &ch : out) {
        const auto it = map.constFind(ch);
        if (it != map.constEnd())
            ch = *it;
    }
    out.remove(strip);
    return out;
}

QDateTime safeTime(const QDateTime &time)
{
    return time.isValid() ? time : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC);
}

bool isCjk(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 0x3040 && u <= 0x30ff) || (u >= 0x3400 && u <= 0x9fff) || (u >= 0xf900 && u <= 0xfaff);
}

bool isAsciiWord(QChar c)
{
    return c.unicode() < 0x80 && c.isLetterOrNumber();
}

bool hasCjk(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), isCjk);
}

// "剧场版 鬼灭之刃" is listed under 鬼灭之刃; the word comes back in its label.
QString filmPrefix(const QString &name)
{
    static const QRegularExpression prefix(QStringLiteral("^\\s*(?:剧场版|劇場版|映画|电影版)[\\s:：]*"));
    return prefix.match(name).captured();
}

QString mainName(const QString &full)
{
    static const QRegularExpression season(
        QStringLiteral("第[一二三四五六七八九十百\\d]+[季期部章]|最[终終]季|完[结結][篇編]|(?:The\\s+)?Final\\s+Season"
                       "|\\s(?:Season|S)\\s?\\d|\\d+(?:st|nd|rd|th)\\s+Season"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression numeral(
        QStringLiteral("(?:\\s*[ⅡⅢⅣⅤⅥⅦⅧⅨⅩ]+|(?<=[^\\x00-\\x7F])(?:II|III|IV|VI?|\\d{1,2}))$"));
    static const QString breaks = QStringLiteral(":：～〜~【「《");

    QString name = full.mid(filmPrefix(full).size());
    if (const auto match = season.match(name); match.hasMatch())
        name.truncate(match.capturedStart());
    name = name.trimmed();
    // A subtitle follows a space or colon. In Latin titles those are part of the name
    // itself (ONE PIECE, Re:ゼロ), so a break only counts after CJK text.
    for (qsizetype i = 0; i < name.size(); ++i) {
        if (!name.at(i).isSpace() && !breaks.contains(name.at(i)))
            continue;
        const QString left = name.left(i);
        if (hasCjk(left) && fold(left).size() >= 2) {
            name = left;
            break;
        }
    }
    QString bare = name;
    bare.remove(numeral);
    return fold(bare).size() >= 2 ? bare.trimmed() : name.trimmed();
}

// What follows the folded `key` at the start of the name, or nothing when the name
// merely begins with the same letters: 海贼王女 is not a season of 海贼王.
std::optional<QString> afterKey(const QString &name, const QString &key)
{
    if (key.isEmpty() || !fold(name).startsWith(key))
        return std::nullopt;
    qsizetype end = 1;
    while (end < name.size() && fold(name.left(end)).size() < key.size())
        ++end;
    const QString rest = name.mid(end);
    if (rest.isEmpty())
        return rest;
    const QChar last = name.at(end - 1), next = rest.at(0);
    if ((isCjk(last) && isCjk(next) && next.isLetterOrNumber()) || (isAsciiWord(last) && isAsciiWord(next)))
        return std::nullopt;
    return rest;
}

// Drops the punctuation that joined the rest to the title, and its closing half.
QString tidyLabel(const QString &rest)
{
    static const QString joins = QStringLiteral(":：·・/／」』》）)");
    static const QString wraps = QStringLiteral("-－—~～〜");
    QString label = rest.trimmed();
    bool wrapped = false;
    while (!label.isEmpty() && (joins.contains(label.front()) || wraps.contains(label.front()) || label.front().isSpace())) {
        wrapped = wrapped || wraps.contains(label.front());
        label.remove(0, 1);
    }
    while (wrapped && !label.isEmpty() && wraps.contains(label.back()))
        label.chop(1);
    return label.trimmed();
}

QStringList seriesKeys(const AnimeSubject &subject)
{
    QStringList keys;
    for (const auto &name : seriesNames(subject))
        keys << fold(name);
    return keys;
}

std::optional<QString> afterAnyKey(const QString &name, const QStringList &keys)
{
    const QString title = name.mid(filmPrefix(name).size());
    for (const auto &key : keys)
        if (const auto rest = afterKey(title, key))
            return rest;
    return std::nullopt;
}

} // namespace

QStringList subjectKeys(const AnimeSubject &subject)
{
    QStringList names;
    for (const auto &name : {subject.nameCn, subject.nameJp, subject.nameEn, subject.nameRomaji})
        if (!name.isEmpty())
            names << name;
    names += subject.aliases;

    static const QRegularExpression seasonSuffix(
        QStringLiteral("第[一二三四五六七八九十\\d]+[季期]|\\s(?:Season|S)\\s?\\d|\\d+(?:st|nd|rd|th) Season"),
        QRegularExpression::CaseInsensitiveOption);

    QStringList keys;
    for (const auto &name : names) {
        const auto match = seasonSuffix.match(name);
        const QString base = match.hasMatch() ? name.left(match.capturedStart()).trimmed() : name;
        const QString key = fold(base);
        if (key.size() >= 2 && !keys.contains(key))
            keys << key;
    }
    return keys;
}

QString releaseGroupKey(const QString &name)
{
    return fold(name);
}

bool isRelevant(const QString &title, const QStringList &keys)
{
    if (keys.isEmpty())
        return true;
    const QString folded = fold(title);
    for (const auto &key : keys) {
        if(key.isEmpty()||!folded.contains(key))continue;
        if(QRegularExpression("^[a-z0-9]+$").match(key).hasMatch()) {
            QStringList letters;for(auto ch:key)letters.append(QRegularExpression::escape(QString(ch)));
            const QRegularExpression boundary("(?<![a-z0-9])"+letters.join("[\\W_]*")+"(?![a-z0-9])",QRegularExpression::CaseInsensitiveOption);
            if(boundary.match(title.normalized(QString::NormalizationForm_KC)).hasMatch())return true;
        } else return true;
    }
    return false;
}

bool EpisodeRange::contains(const QString &episode) const
{
    bool ok = false;
    const double value = episode.toDouble(&ok);
    return ok && (first <= 0 || value >= first) && (last <= 0 || value <= last);
}

double Release::sortEpisode() const
{
    if (episode.isEmpty())
        return -1;
    const QString head = episode.section(QLatin1Char('-'), -1);
    bool ok = false;
    const double value = head.toDouble(&ok);
    return ok ? value : 9999;
}

QDateTime ReleaseGroup::latest() const
{
    QDateTime newest;
    for (const auto &release : releases) {
        if (!release.result.publishTime.isValid())
            continue;
        if (!newest.isValid() || release.result.publishTime > newest)
            newest = release.result.publishTime;
    }
    return newest;
}

int ReleaseGroup::totalSeeders() const
{
    int total = 0;
    for (const auto &release : releases)
        total += qMax(0, release.result.seeders);
    return total;
}

QStringList ReleaseGroup::resolutions() const
{
    static const QStringList order = {QStringLiteral("2160p"), QStringLiteral("1440p"),
                                      QStringLiteral("1080p"), QStringLiteral("720p"),
                                      QStringLiteral("480p")};
    QSet<QString> found;
    for (const auto &release : releases)
        if (!release.result.tags.resolution.isEmpty())
            found.insert(release.result.tags.resolution);
    QStringList out;
    for (const auto &resolution : order)
        if (found.contains(resolution))
            out << resolution;
    return out;
}

QSet<QString> ReleaseGroup::subtitleLangs() const
{
    QSet<QString> out;
    for (const auto &release : releases) {
        for (const auto &lang : release.result.tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts))
            out.insert(lang);
    }
    return out;
}

QVector<Release> annotate(const QVector<ResourceResult> &results)
{
    QVector<Release> out;
    out.reserve(results.size());
    for (const auto &result : results) {
        const auto episode = parseEpisode(result.title);
        Release release;
        release.result = result;
        release.episode = episode.label.isEmpty() ? result.episode : episode.label;
        release.isBatch = episode.batch;
        release.season = parseSeason(result.title);
        out.push_back(std::move(release));
    }
    return out;
}

bool passes(const Release &release, const Filters &filters)
{
    const auto &tags = release.result.tags;
    if (filters.season.has_value()) {
        if (release.season.has_value() && *release.season != *filters.season)
            return false;
        if (!release.season.has_value() && *filters.season != 1)
            return false;
    }
    if (!filters.resolution.isEmpty() && tags.resolution != filters.resolution)
        return false;
    if (!filters.subtitle.isEmpty()) {
        const auto langs = tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (filters.subtitle == QStringLiteral("zh")) {
            bool found = false;
            for (const auto &lang : langs) {
                if (lang.startsWith(QStringLiteral("zh"))) {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        } else if (!langs.contains(filters.subtitle)) {
            return false;
        }
    }
    if (filters.hdrOnly && (tags.hdrType.isEmpty() || tags.hdrType == QStringLiteral("SDR")))
        return false;
    if (filters.kind == QStringLiteral("batch") && !release.isBatch)
        return false;
    if (filters.kind == QStringLiteral("single") && release.isBatch)
        return false;
    if (filters.minSeeders > 0 && release.result.seeders < filters.minSeeders)
        return false;
    if (!filters.text.isEmpty() && !release.result.title.contains(filters.text, Qt::CaseInsensitive))
        return false;
    return true;
}

QVector<ReleaseGroup> groupReleases(const QVector<Release> &releases, const QString &sortKey, const QStringList &subjectKeys)
{
    // A leading bracket that only repeats the title ("[海贼王][1089]") is not a
    // release group; showing it as one would split unattributed releases apart.
    auto unattributed = [&subjectKeys](const QString &group) {
        if (group.isEmpty() || group == kUnknownGroup) return true;
        const QString key = releaseGroupKey(group);
        for (const auto &subject : subjectKeys)
            if (!key.isEmpty() && (key == subject || (subject.size() >= 3 && key.contains(subject))))
                return true;
        return false;
    };
    QHash<QString, ReleaseGroup> buckets;
    for (const auto &release : releases) {
        const QString name = unattributed(release.result.releaseGroup) ? kUnknownGroup : release.result.releaseGroup;
        const QString key = releaseGroupKey(name).isEmpty() ? name : releaseGroupKey(name);
        auto &group = buckets[key];
        if (group.name.isEmpty())
            group.name = name;
        group.releases.push_back(release);
    }

    for (auto &group : buckets) {
        std::sort(group.releases.begin(), group.releases.end(), [](const Release &a, const Release &b) {
            if (a.isBatch != b.isBatch)
                return a.isBatch;
            if (a.sortEpisode() != b.sortEpisode())
                return a.sortEpisode() > b.sortEpisode();
            return safeTime(a.result.publishTime) > safeTime(b.result.publishTime);
        });
    }

    QVector<ReleaseGroup> groups;
    groups.reserve(buckets.size());
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it)
        groups.push_back(it.value());

    auto latestKey = [](const ReleaseGroup &group) {
        return safeTime(group.latest()).toSecsSinceEpoch();
    };
    if (sortKey == QStringLiteral("popular")) {
        // Each group's share of the best seeder total and of the largest
        // resource count, so popular groups and complete archives both rise.
        int maxSeeders = 0;
        qsizetype maxCount = 1;
        for (const auto &group : groups) {
            if (group.name == kUnknownGroup) continue;
            maxSeeders = qMax(maxSeeders, group.totalSeeders());
            maxCount = qMax(maxCount, group.releases.size());
        }
        auto score = [&](const ReleaseGroup &group) {
            const double seeders = maxSeeders > 0 ? double(group.totalSeeders()) / maxSeeders : 0.0;
            return seeders + double(group.releases.size()) / double(maxCount);
        };
        std::sort(groups.begin(), groups.end(), [&](const ReleaseGroup &a, const ReleaseGroup &b) {
            const double sa = score(a), sb = score(b);
            if (!qFuzzyCompare(1.0 + sa, 1.0 + sb)) return sa > sb;
            return latestKey(a) > latestKey(b);
        });
    } else if (sortKey == QStringLiteral("seeders")) {
        std::sort(groups.begin(), groups.end(), [](const ReleaseGroup &a, const ReleaseGroup &b) {
            return a.totalSeeders() > b.totalSeeders();
        });
    } else if (sortKey == QStringLiteral("count")) {
        std::sort(groups.begin(), groups.end(), [](const ReleaseGroup &a, const ReleaseGroup &b) {
            return a.releases.size() > b.releases.size();
        });
    } else if (sortKey == QStringLiteral("name")) {
        std::sort(groups.begin(), groups.end(), [](const ReleaseGroup &a, const ReleaseGroup &b) {
            return a.name.toLower() < b.name.toLower();
        });
    } else {
        std::sort(groups.begin(), groups.end(), [&](const ReleaseGroup &a, const ReleaseGroup &b) {
            return latestKey(a) > latestKey(b);
        });
    }
    std::stable_sort(groups.begin(), groups.end(), [](const ReleaseGroup &a, const ReleaseGroup &b) {
        return (a.name == kUnknownGroup) < (b.name == kUnknownGroup);
    });
    return groups;
}

QVector<int> seasonsPresent(const QVector<Release> &releases)
{
    QSet<int> seasons;
    for (const auto &release : releases)
        if (release.season.has_value())
            seasons.insert(*release.season);
    QVector<int> out;
    out.reserve(seasons.size());
    for (int season : seasons)
        out.push_back(season);
    std::sort(out.begin(), out.end());
    return out;
}

QStringList seriesNames(const AnimeSubject &subject)
{
    QStringList names, keys;
    for (const auto &full : {subject.nameCn, subject.nameJp}) {
        const QString name = mainName(full);
        const QString key = fold(name);
        if (key.size() >= 2 && !keys.contains(key)) {
            keys << key;
            names << name;
        }
    }
    return names;
}

QVector<SeriesEntry> seriesEntries(const AnimeSubject &current, const QVector<AnimeSubject> &found)
{
    const QStringList keys = seriesKeys(current);
    QVector<AnimeSubject> members{current};
    QSet<int> seen{current.bangumiId};
    for (const auto &subject : found) {
        // Bangumi files promotional films and collaborations under 其他.
        if (seen.contains(subject.bangumiId) || subject.platform == QStringLiteral("其他"))
            continue;
        if (afterAnyKey(subject.nameCn, keys) || afterAnyKey(subject.nameJp, keys)) {
            seen.insert(subject.bangumiId);
            members.append(subject);
        }
    }

    // Measured against the shortest title any of them has, so that 东京喰种√A, whose
    // own title has no break in it, still reads as √A beside 东京喰种.
    QStringList stems;
    for (const auto &subject : members)
        for (const auto &key : seriesKeys(subject))
            if (!stems.contains(key))
                stems << key;
    std::sort(stems.begin(), stems.end(), [](const QString &a, const QString &b) { return a.size() < b.size(); });

    QVector<SeriesEntry> entries;
    entries.reserve(members.size());
    for (const auto &subject : members) {
        SeriesEntry entry;
        entry.subject = subject;
        const QString name = subject.displayName();
        const QString film = filmPrefix(name).trimmed();
        const auto rest = afterAnyKey(name, stems);
        const QString label = rest ? tidyLabel(*rest) : QString();
        entry.main = rest && label.isEmpty() && film.isEmpty();
        if (!rest || entry.main)
            entry.label = name;
        else
            entry.label = QStringList{film, label}.join(QLatin1Char(' ')).trimmed();
        entries.append(entry);
    }
    // Undated entries have not aired yet, so they come last.
    std::sort(entries.begin(), entries.end(), [](const SeriesEntry &a, const SeriesEntry &b) {
        const QString &x = a.subject.airDate, &y = b.subject.airDate;
        if (x.isEmpty() != y.isEmpty())
            return y.isEmpty();
        return x != y ? x < y : a.subject.bangumiId < b.subject.bangumiId;
    });
    return entries;
}

QStringList seriesMarks(const QVector<SeriesEntry> &entries)
{
    QStringList stems;
    for (const auto &entry : entries)
        if (entry.main)
            stems += seriesKeys(entry.subject);
    QStringList marks;
    for (const auto &entry : entries) {
        if (entry.main)
            continue;
        for (const auto &name : {entry.subject.nameCn, entry.subject.nameJp}) {
            const auto rest = afterAnyKey(name, stems);
            const QString mark = rest ? fold(tidyLabel(*rest)) : QString();
            // Latin leftovers such as "re" or "Z" would match inside ordinary words.
            if (mark.size() >= 2 && hasCjk(mark) && !marks.contains(mark))
                marks << mark;
        }
    }
    return marks;
}

bool mentionsAny(const QString &title, const QStringList &marks)
{
    const QString folded = fold(title);
    return std::any_of(marks.cbegin(), marks.cend(), [&folded](const QString &mark) { return folded.contains(mark); });
}

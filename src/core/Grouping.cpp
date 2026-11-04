// Anime Nexus - Copyright (C) 2026 Magician0711
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

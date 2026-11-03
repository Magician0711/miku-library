#include "BangumiClient.h"

#include "HttpClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

namespace {

QStringList infoboxValues(const QJsonArray &infobox, const QStringList &keys)
{
    QStringList out;
    for (const auto &item : infobox) {
        const auto obj = item.toObject();
        if (!keys.contains(obj.value(QStringLiteral("key")).toString()))
            continue;
        const auto value = obj.value(QStringLiteral("value"));
        if (value.isString()) {
            out << value.toString();
        } else if (value.isArray()) {
            for (const auto &entry : value.toArray()) {
                if (entry.isObject()) {
                    const auto text = entry.toObject().value(QStringLiteral("v")).toString();
                    if (!text.isEmpty())
                        out << text;
                } else if (entry.isString()) {
                    out << entry.toString();
                }
            }
        }
    }
    for (auto &text : out)
        text = text.trimmed();
    out.removeAll(QString());
    return out;
}

bool isLatin(const QString &text)
{
    if (text.isEmpty())
        return false;
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (u > 0x024f && u != 0x2013 && u != 0x2014)
            return false;
    }
    return true;
}

int yearFromDate(const QString &date)
{
    if (date.size() < 4)
        return 0;
    bool ok = false;
    const int year = date.left(4).toInt(&ok);
    return ok ? year : 0;
}

AnimeSubject parseSubjectV0(const QJsonObject &data)
{
    AnimeSubject subject;
    subject.bangumiId = data.value(QStringLiteral("id")).toInt();
    subject.nameJp = data.value(QStringLiteral("name")).toString();
    subject.nameCn = data.value(QStringLiteral("name_cn")).toString();
    subject.summary = data.value(QStringLiteral("summary")).toString();
    subject.airDate = data.value(QStringLiteral("date")).toString();
    subject.year = yearFromDate(subject.airDate);
    subject.episodeCount = data.value(QStringLiteral("total_episodes")).toInt(
        data.value(QStringLiteral("eps")).toInt());
    subject.coverUrl = data.value(QStringLiteral("images")).toObject().value(QStringLiteral("large")).toString();
    if (subject.coverUrl.isEmpty())
        subject.coverUrl = data.value(QStringLiteral("images")).toObject().value(QStringLiteral("common")).toString();
    subject.bangumiUrl = QStringLiteral("https://bgm.tv/subject/%1").arg(subject.bangumiId);
    subject.platform = data.value(QStringLiteral("platform")).toString();

    const auto rating = data.value(QStringLiteral("rating")).toObject();
    subject.rating = rating.value(QStringLiteral("score")).toDouble();
    subject.rank = rating.value(QStringLiteral("rank")).toInt();

    const auto infobox = data.value(QStringLiteral("infobox")).toArray();
    auto aliases = infoboxValues(infobox, {QStringLiteral("别名"), QStringLiteral("中文名"),
                                           QStringLiteral("英文名"), QStringLiteral("罗马音")});
    for (const auto &alias : aliases) {
        if (subject.nameEn.isEmpty() && isLatin(alias))
            subject.nameEn = alias;
    }
    QSet<QString> seen = {subject.nameJp, subject.nameCn};
    for (const auto &alias : aliases) {
        if (!alias.isEmpty() && !seen.contains(alias)) {
            seen.insert(alias);
            subject.aliases << alias;
        }
    }
    const auto studios = infoboxValues(infobox, {QStringLiteral("动画制作")});
    if (!studios.isEmpty())
        subject.studio = studios.first();

    for (const auto &tag : data.value(QStringLiteral("tags")).toArray()) {
        const auto name = tag.toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && subject.tags.size() < 12)
            subject.tags << name;
    }
    return subject;
}

AnimeSubject parseCalendarItem(const QJsonObject &item, int weekday)
{
    AnimeSubject subject;
    subject.bangumiId = item.value(QStringLiteral("id")).toInt();
    subject.nameCn = item.value(QStringLiteral("name_cn")).toString();
    subject.nameJp = item.value(QStringLiteral("name")).toString();
    subject.summary = item.value(QStringLiteral("summary")).toString();
    subject.airDate = item.value(QStringLiteral("air_date")).toString();
    subject.year = yearFromDate(subject.airDate);
    subject.episodeCount = item.value(QStringLiteral("eps_count")).toInt(
        item.value(QStringLiteral("eps")).toInt());
    subject.coverUrl = item.value(QStringLiteral("images")).toObject().value(QStringLiteral("large")).toString();
    if (subject.coverUrl.isEmpty())
        subject.coverUrl = item.value(QStringLiteral("images")).toObject().value(QStringLiteral("common")).toString();
    subject.bangumiUrl = QStringLiteral("https://bgm.tv/subject/%1").arg(subject.bangumiId);
    subject.rating = item.value(QStringLiteral("rating")).toObject().value(QStringLiteral("score")).toDouble();
    subject.rank = item.value(QStringLiteral("rank")).toInt();
    subject.airWeekday = weekday > 0 ? weekday : item.value(QStringLiteral("air_weekday")).toInt();
    return subject;
}

} // namespace

BangumiClient::BangumiClient(HttpClient *http, QObject *parent)
    : QObject(parent)
    , m_http(http)
{
}

QUrl BangumiClient::endpoint(const QString &path) const
{
    return QUrl(QStringLiteral("https://api.bgm.tv") + path);
}

void BangumiClient::search(const QString &term, int limit,
                           std::function<void(QVector<AnimeSubject>, QString)> callback)
{
    QUrl url = endpoint(QStringLiteral("/v0/search/subjects"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    url.setQuery(query);

    QJsonObject filter;
    filter.insert(QStringLiteral("type"), QJsonArray{2});
    QJsonObject body;
    body.insert(QStringLiteral("keyword"), term);
    body.insert(QStringLiteral("sort"), QStringLiteral("match"));
    body.insert(QStringLiteral("filter"), filter);

    auto *reply = m_http->postJson(url, body, 15000);
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() mutable {
        const QString error = HttpClient::replyError(reply);
        if (!error.isEmpty()) {
            reply->deleteLater();
            callback({}, error);
            return;
        }
        bool ok = false;
        const auto doc = HttpClient::json(reply, &ok);
        reply->deleteLater();
        if (!ok || !doc.isObject()) {
            callback({}, QStringLiteral("Bangumi 返回了无效 JSON"));
            return;
        }
        QVector<AnimeSubject> subjects;
        for (const auto &value : doc.object().value(QStringLiteral("data")).toArray())
            subjects.push_back(parseSubjectV0(value.toObject()));
        callback(subjects, {});
    });
}

void BangumiClient::getSubject(int id, std::function<void(AnimeSubject, QString)> callback)
{
    auto *reply = m_http->get(endpoint(QStringLiteral("/v0/subjects/%1").arg(id)), 15000);
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() mutable {
        const QString error = HttpClient::replyError(reply);
        if (!error.isEmpty()) {
            reply->deleteLater();
            callback({}, error);
            return;
        }
        bool ok = false;
        const auto doc = HttpClient::json(reply, &ok);
        reply->deleteLater();
        if (!ok || !doc.isObject()) {
            callback({}, QStringLiteral("Bangumi 返回了无效 JSON"));
            return;
        }
        callback(parseSubjectV0(doc.object()), {});
    });
}

void BangumiClient::calendar(std::function<void(QVector<QPair<int, QVector<AnimeSubject>>>, QString)> callback)
{
    auto *reply = m_http->get(endpoint(QStringLiteral("/calendar")), 15000);
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() mutable {
        const QString error = HttpClient::replyError(reply);
        if (!error.isEmpty()) {
            reply->deleteLater();
            callback({}, error);
            return;
        }
        bool ok = false;
        const auto doc = HttpClient::json(reply, &ok);
        reply->deleteLater();
        if (!ok || !doc.isArray()) {
            callback({}, QStringLiteral("Bangumi 放送表返回了无效 JSON"));
            return;
        }
        QVector<QPair<int, QVector<AnimeSubject>>> days;
        for (const auto &value : doc.array()) {
            const auto day = value.toObject();
            const int weekday = day.value(QStringLiteral("weekday")).toObject().value(QStringLiteral("id")).toInt();
            QVector<AnimeSubject> items;
            for (const auto &item : day.value(QStringLiteral("items")).toArray())
                items.push_back(parseCalendarItem(item.toObject(), weekday));
            days.push_back({weekday, items});
        }
        callback(days, {});
    });
}

QString BangumiClient::coverUrl(const QString &url, int width)
{
    Q_UNUSED(width)
    if (url.isEmpty())
        return {};
    QString normalized = url;
    if (normalized.startsWith(QStringLiteral("http://")))
        normalized.replace(0, 7, QStringLiteral("https://"));
    // lain.bgm.tv's legacy /r/<width>/ resize endpoint now answers 400 for
    // many covers. Use the original image and downscale it locally instead.
    return normalized;
}

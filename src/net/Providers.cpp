// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Providers.h"

#include "FeedParser.h"
#include "HttpClient.h"
#include "core/TitleParser.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace {

QDateTime isoDate(const QString &text)
{
    if (text.isEmpty())
        return {};
    QDateTime date = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!date.isValid())
        date = QDateTime::fromString(text, Qt::ISODate);
    if (date.isValid() && date.timeSpec() == Qt::LocalTime)
        date.setTimeSpec(Qt::UTC);
    return date;
}

QVector<ResourceResult> parseAnimeGarden(const QJsonObject &root, const SiteEntry &site)
{
    QVector<ResourceResult> results;
    for (const auto &value : root.value(QStringLiteral("resources")).toArray()) {
        const auto item = value.toObject();
        const QString title = item.value(QStringLiteral("title")).toString();
        if (title.isEmpty())
            continue;
        ResourceResult result;
        result.title = title;
        result.siteId = site.id;
        result.siteName = site.name;
        result.tags = parseTitle(title);
        result.releaseGroup = item.value(QStringLiteral("fansub")).toObject()
                                  .value(QStringLiteral("name")).toString();
        if (result.releaseGroup.isEmpty())
            result.releaseGroup = result.tags.releaseGroup;
        result.publishTime = isoDate(item.value(QStringLiteral("createdAt")).toString());
        const auto size = item.value(QStringLiteral("size"));
        if (size.isDouble())
            result.fileSize = static_cast<qint64>(size.toDouble());
        result.magnetUri = item.value(QStringLiteral("magnet")).toString();
        if (auto hash = btihFromMagnet(result.magnetUri))
            result.infoHash = *hash;
        result.sourcePage = item.value(QStringLiteral("href")).toString();
        results.push_back(std::move(result));
    }
    return results;
}

QVector<ResourceResult> parseBangumiMoe(const QJsonObject &root, const SiteEntry &site)
{
    QVector<ResourceResult> results;
    for (const auto &value : root.value(QStringLiteral("torrents")).toArray()) {
        const auto item = value.toObject();
        const QString title = item.value(QStringLiteral("title")).toString();
        if (title.isEmpty())
            continue;
        ResourceResult result;
        result.title = title;
        result.siteId = site.id;
        result.siteName = site.name;
        result.tags = parseTitle(title);
        result.releaseGroup = item.value(QStringLiteral("team")).toObject()
                                  .value(QStringLiteral("name")).toString();
        if (result.releaseGroup.isEmpty())
            result.releaseGroup = result.tags.releaseGroup;
        result.publishTime = isoDate(item.value(QStringLiteral("publish_time")).toString());
        if (item.value(QStringLiteral("size")).isString())
            result.fileSize = parseSize(item.value(QStringLiteral("size")).toString());
        else if (item.value(QStringLiteral("size")).isDouble())
            result.fileSize = static_cast<qint64>(item.value(QStringLiteral("size")).toDouble());
        result.magnetUri = item.value(QStringLiteral("magnet")).toString();
        result.infoHash = normalizeBtih(item.value(QStringLiteral("infoHash")).toString())
                              .value_or(btihFromMagnet(result.magnetUri).value_or(QString()));
        result.seeders = item.value(QStringLiteral("seeders")).toInt(-1);
        result.leechers = item.value(QStringLiteral("leechers")).toInt(-1);
        const QString id = item.value(QStringLiteral("_id")).toString();
        if (!id.isEmpty())
            result.sourcePage = QStringLiteral("https://bangumi.moe/torrent/%1").arg(id);
        results.push_back(std::move(result));
    }
    return results;
}

QVector<ResourceResult> parseSubsPlease(const QJsonDocument &doc, const SiteEntry &site)
{
    QVector<ResourceResult> results;
    if (!doc.isObject())
        return results;
    const auto root = doc.object();
    for (auto showIt = root.constBegin(); showIt != root.constEnd(); ++showIt) {
        const auto show = showIt.value().toObject();
        const QString showName = show.value(QStringLiteral("show")).toString();
        const QString episode = show.value(QStringLiteral("episode")).toString();
        QDateTime published;
        const QString releaseDate = show.value(QStringLiteral("release_date")).toString();
        if (!releaseDate.isEmpty())
            published = QDateTime::fromString(releaseDate, Qt::RFC2822Date);
        for (const auto &downloadValue : show.value(QStringLiteral("downloads")).toArray()) {
            const auto download = downloadValue.toObject();
            const QString resolution = download.value(QStringLiteral("res")).toString();
            const QString magnet = download.value(QStringLiteral("magnet")).toString();
            if (magnet.isEmpty())
                continue;
            ResourceResult result;
            result.title = QStringLiteral("[SubsPlease] %1 - %2 (%3p).mkv")
                               .arg(showName, episode, resolution);
            result.siteId = site.id;
            result.siteName = site.name;
            result.tags = parseTitle(result.title);
            result.episode = episode;
            result.publishTime = published;
            result.magnetUri = magnet;
            if (auto hash = btihFromMagnet(magnet))
                result.infoHash = *hash;
            const int xl = magnet.indexOf(QStringLiteral("xl="));
            if (xl >= 0) {
                const QString size = magnet.mid(xl + 3).section(QLatin1Char('&'), 0, 0);
                bool ok = false;
                const qint64 bytes = size.toLongLong(&ok);
                if (ok)
                    result.fileSize = bytes;
            }
            const QString page = show.value(QStringLiteral("page")).toString();
            if (!page.isEmpty())
                result.sourcePage = QStringLiteral("https://subsplease.org/shows/%1/").arg(page);
            results.push_back(std::move(result));
        }
    }
    return results;
}

class RssProvider final : public SearchProvider {
public:
    RssProvider(const SiteEntry &site, HttpClient *http)
        : SearchProvider(site, http)
    {
    }

    void search(const QString &term,
                std::function<void(QVector<ResourceResult>, QString)> callback) override
    {
        page(QUrl(rssUrlFor(m_site,term)),{}, {},std::move(callback));
    }

    void testConnection(std::function<void(bool, QString)> callback) override
    {
        auto *reply = m_http->get(QUrl(rssUrlFor(m_site, QStringLiteral("test"))), 10000);
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, site = m_site, callback = std::move(callback)]() mutable {
            const QString error = HttpClient::replyError(reply);
            const QByteArray data = reply->readAll();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            reply->deleteLater();
            if (!error.isEmpty()) {
                callback(false, error);
                return;
            }
            QString parseError;
            const auto results = parseRss(data, site, &parseError);
            callback(parseError.isEmpty(),parseError.isEmpty()?QStringLiteral("HTTP %1，%2 条").arg(status).arg(results.size()):parseError);
        });
    }
private:
    void page(QUrl url,QVector<ResourceResult> collected,QSet<QString> visited,std::function<void(QVector<ResourceResult>,QString)> callback) {
        visited.insert(url.toString());auto *reply=m_http->get(url,int(m_site.timeout*1000));
        QObject::connect(reply,&QNetworkReply::finished,reply,[this,reply,url,collected=std::move(collected),visited=std::move(visited),callback=std::move(callback)]()mutable {
            const auto error=HttpClient::replyError(reply);const auto bytes=reply->readAll();reply->deleteLater();
            if(!error.isEmpty()){callback(collected,error);return;}
            QString parseError;collected+=parseRss(bytes,m_site,&parseError);if(!parseError.isEmpty()){callback(collected,parseError);return;}
            QXmlStreamReader xml(bytes);QUrl next;while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()&&xml.name()=="link"&&xml.attributes().value("rel")=="next")next=url.resolved(QUrl(xml.attributes().value("href").toString()));}
            if(next.isEmpty()){callback(collected,{});return;}
            if(next.scheme()!=url.scheme()||next.host()!=url.host()||next.port()!=url.port()||visited.contains(next.toString())||visited.size()>=100){callback(collected,QStringLiteral("RSS 分页受限，结果可能不完整。"));return;}
            page(next,std::move(collected),std::move(visited),std::move(callback));
        });
    }
};

class JsonProvider final : public SearchProvider {
public:
    enum Kind { AnimeGarden, BangumiMoe, SubsPlease };

    JsonProvider(const SiteEntry &site, HttpClient *http, Kind kind)
        : SearchProvider(site, http)
        , m_kind(kind)
    {
    }

    void search(const QString &term,
                std::function<void(QVector<ResourceResult>, QString)> callback) override
    {
        if (m_kind == AnimeGarden) {
            gardenPage(term,1,{}, {},std::move(callback));
        } else if (m_kind == BangumiMoe) {
            QJsonObject body;
            body.insert(QStringLiteral("query"), term);
            auto *reply = m_http->postJson(QUrl(QStringLiteral("https://bangumi.moe/api/v2/torrent/search")), body, 20000);
            QObject::connect(reply, &QNetworkReply::finished, reply, [reply, site = m_site, callback = std::move(callback)]() mutable {
                const QString error = HttpClient::replyError(reply);
                bool ok = false;
                const auto doc = HttpClient::json(reply, &ok);
                reply->deleteLater();
                if (!error.isEmpty()) {
                    callback({}, error);
                    return;
                }
                if (!ok || !doc.isObject()) {
                    callback({}, QStringLiteral("Bangumi.Moe 返回了无效 JSON"));
                    return;
                }
                callback(parseBangumiMoe(doc.object(), site), {});
            });
        } else {
            QUrl url(QStringLiteral("https://subsplease.org/api/"));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("f"), QStringLiteral("search"));
            query.addQueryItem(QStringLiteral("tz"), QStringLiteral("UTC"));
            query.addQueryItem(QStringLiteral("s"), term);
            url.setQuery(query);
            auto *reply = m_http->get(url, 20000);
            QObject::connect(reply, &QNetworkReply::finished, reply, [reply, site = m_site, callback = std::move(callback)]() mutable {
                const QString error = HttpClient::replyError(reply);
                bool ok = false;
                const auto doc = HttpClient::json(reply, &ok);
                reply->deleteLater();
                if (!error.isEmpty()) {
                    callback({}, error);
                    return;
                }
                if (!ok) {
                    callback({}, QStringLiteral("SubsPlease 返回了无效 JSON"));
                    return;
                }
                callback(parseSubsPlease(doc, site), {});
            });
        }
    }

    void testConnection(std::function<void(bool, QString)> callback) override
    {
        search(QStringLiteral("test"), [callback = std::move(callback)](QVector<ResourceResult> results, QString error) mutable {
            if (!error.isEmpty())
                callback(false, error);
            else
                callback(true, QStringLiteral("API OK，%1 条").arg(results.size()));
        });
    }

private:
    void gardenPage(const QString &term,int page,QVector<ResourceResult> collected,QSet<QString> seen,
                    std::function<void(QVector<ResourceResult>,QString)> callback)
    {
        QUrl url(m_site.searchUrl.isEmpty()?QStringLiteral("https://api.animes.garden/resources"):m_site.searchUrl);
        QUrlQuery query(url);query.addQueryItem("search",term);query.addQueryItem("pageSize","100");query.addQueryItem("page",QString::number(page));url.setQuery(query);
        auto *reply=m_http->get(url,20000);
        QObject::connect(reply,&QNetworkReply::finished,reply,[this,reply,term,page,collected=std::move(collected),seen=std::move(seen),callback=std::move(callback)]()mutable {
            const auto error=HttpClient::replyError(reply);bool valid=false;const auto doc=HttpClient::json(reply,&valid);reply->deleteLater();
            if(!error.isEmpty()||!valid||!doc.isObject()||doc.object().value("status").toString()=="ERROR") {
                callback(collected,error.isEmpty()?QStringLiteral("Anime Garden 返回了无效数据，结果可能不完整。"):error);return;
            }
            const auto root=doc.object();const auto results=parseAnimeGarden(root,m_site);int added=0;
            for(const auto &r:results)if(!seen.contains(r.dedupKey())){seen.insert(r.dedupKey());collected.append(r);++added;}
            const auto pagination=root.value("pagination").toObject();
            const bool complete=pagination.contains("complete")?pagination.value("complete").toBool():results.size()<100;
            if(complete||results.isEmpty()){callback(collected,{});return;}
            if(!added||page>=qBound(1,m_site.raw.value("max_pages").toInt(100),500)) {
                callback(collected,QStringLiteral("已到分页保护上限或站点重复返回同一页，结果可能不完整。"));return;
            }
            gardenPage(term,page+1,std::move(collected),std::move(seen),std::move(callback));
        });
    }
    Kind m_kind;
};

} // namespace

void SearchProvider::testConnection(std::function<void(bool, QString)> callback)
{
    callback(true, QStringLiteral("provider ready"));
}

std::unique_ptr<SearchProvider> makeProvider(const SiteEntry &site, HttpClient *http)
{
    if (!http || site.adapter == AdapterType::None)
        return nullptr;
    if (site.adapter == AdapterType::Rss)
        return std::make_unique<RssProvider>(site, http);
    if (site.adapter == AdapterType::JsonApi) {
        if (site.id == QStringLiteral("animegarden"))
            return std::make_unique<JsonProvider>(site, http, JsonProvider::AnimeGarden);
        if (site.id == QStringLiteral("bangumimoe"))
            return std::make_unique<JsonProvider>(site, http, JsonProvider::BangumiMoe);
        if (site.id == QStringLiteral("subsplease"))
            return std::make_unique<JsonProvider>(site, http, JsonProvider::SubsPlease);
    }
    return nullptr;
}

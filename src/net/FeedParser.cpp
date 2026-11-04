// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "FeedParser.h"

#include "core/TitleParser.h"

#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>

namespace {

QString magnetFromHtml(const QString &html)
{
    static const QRegularExpression rx(QStringLiteral("href=\"(magnet:\\?[^\"]+)\""));
    const auto match = rx.match(html);
    return match.hasMatch() ? match.captured(1) : QString();
}

qint64 sizeFromHtml(const QString &html)
{
    static const QRegularExpression rx(QStringLiteral("Size:\\s*([\\d.]+\\s*[KMGT]i?B)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto match = rx.match(html);
    return match.hasMatch() ? parseSize(match.captured(1)) : 0;
}

QString hexHashFromUrl(const QString &url)
{
    static const QRegularExpression rx(QStringLiteral("\\b([0-9a-fA-F]{40})\\b"));
    const auto match = rx.match(url);
    return match.hasMatch() ? match.captured(1).toLower() : QString();
}

} // namespace

qint64 parseSize(const QString &text)
{
    const QString value = text.trimmed();
    if (value.isEmpty())
        return 0;
    bool digits = false;
    const qint64 direct = value.toLongLong(&digits);
    if (digits)
        return direct;

    static const QRegularExpression rx(QStringLiteral("^([\\d.]+)\\s*([KMGT])(i?)B$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto match = rx.match(value);
    if (!match.hasMatch())
        return 0;
    const double number = match.captured(1).toDouble();
    const QString unit = match.captured(2).toUpper();
    const int exponent = QStringLiteral("KMGT").indexOf(unit) + 1;
    const qint64 base = match.captured(3).isEmpty() ? 1000 : 1024;
    qint64 multiplier = 1;
    for (int i = 0; i < exponent; ++i)
        multiplier *= base;
    return static_cast<qint64>(number * multiplier);
}

QString rssUrlFor(const SiteEntry &site, const QString &term)
{
    if(!site.rssUrl.isEmpty()) {
        QString url=site.rssUrl;
        if(url.contains("{q}")){url.replace("{q}",QString::fromLatin1(QUrl::toPercentEncoding(term)));return url;}
        return url;
    }
    static const QHash<QString, QString> templates = {
        {QStringLiteral("nyaa"), QStringLiteral("https://nyaa.si/?page=rss&c=1_0&q={q}")},
        {QStringLiteral("dmhy"), QStringLiteral("https://share.dmhy.org/topics/rss/rss.xml?keyword={q}")},
        {QStringLiteral("mikan"), QStringLiteral("https://mikanani.me/RSS/Search?searchstr={q}")},
        {QStringLiteral("acgrip"), QStringLiteral("https://acg.rip/.xml?term={q}")},
        {QStringLiteral("tokyotosho"), QStringLiteral("https://www.tokyotosho.info/rss.php?type=1&terms={q}")},
    };
    const QByteArray encoded = QUrl::toPercentEncoding(term);
    const QString q = QString::fromLatin1(encoded);
    const auto it = templates.constFind(site.id);
    if (it != templates.constEnd()) {
        QString url = *it;
        url.replace(QStringLiteral("{q}"), q);
        return url;
    }
    QString base = site.rssUrl.isEmpty() ? site.url : site.rssUrl;
    if (base.contains(QStringLiteral("{q}"))) {
        base.replace(QStringLiteral("{q}"), q);
        return base;
    }
    return base + (base.contains(QLatin1Char('?')) ? QStringLiteral("&q=") : QStringLiteral("?q=")) + q;
}

QVector<ResourceResult> parseRss(const QByteArray &data, const SiteEntry &site, QString *error)
{
    QVector<ResourceResult> results;
    QXmlStreamReader xml(data);

    struct Fields {
        QString title;
        QString link;
        QString id;
        QString pubDate;
        QString description;
        QString infoHash;
        QString sizeText;
        QString seeders;
        QString leechers;
        QString contentLength;
        QString enclosureUrl;
    } fields;

    auto reset = [&] { fields = Fields{}; };
    auto finish = [&] {
        if (fields.title.trimmed().isEmpty())
            return;
        ResourceResult result;
        result.title = fields.title.trimmed();
        result.siteId = site.id;
        result.siteName = site.name;
        result.tags = parseTitle(result.title);
        result.releaseGroup = result.tags.releaseGroup;

        result.magnetUri = fields.enclosureUrl.startsWith(QStringLiteral("magnet:"))
            ? fields.enclosureUrl
            : magnetFromHtml(fields.description);
        if (!fields.enclosureUrl.isEmpty() && !fields.enclosureUrl.startsWith(QStringLiteral("magnet:")))
            result.torrentUrl = fields.enclosureUrl;
        if (result.torrentUrl.isEmpty() && fields.link.endsWith(QStringLiteral(".torrent")))
            result.torrentUrl = fields.link;

        if (auto hash = normalizeBtih(fields.infoHash))
            result.infoHash = *hash;
        else if (auto hash = btihFromMagnet(result.magnetUri))
            result.infoHash = *hash;
        else if (!result.torrentUrl.isEmpty())
            result.infoHash = hexHashFromUrl(result.torrentUrl);

        // Some feeds put a placeholder such as length="1" on the enclosure; no
        // video release is under 1 KiB, so treat tiny values as unknown.
        auto plausible = [](qint64 size) { return size >= 1024 ? size : 0; };
        result.fileSize = plausible(parseSize(fields.sizeText));
        if (result.fileSize <= 0)
            result.fileSize = plausible(parseSize(fields.contentLength));
        if (result.fileSize <= 0)
            result.fileSize = plausible(sizeFromHtml(fields.description));

        bool ok = false;
        const int seeders = fields.seeders.toInt(&ok);
        if (ok)
            result.seeders = seeders;
        const int leechers = fields.leechers.toInt(&ok);
        if (ok)
            result.leechers = leechers;

        if (!fields.pubDate.isEmpty()) {
            result.publishTime = QDateTime::fromString(fields.pubDate, Qt::RFC2822Date);
            if (!result.publishTime.isValid())
                result.publishTime = QDateTime::fromString(fields.pubDate, Qt::ISODate);
        }

        QString page = site.id == QStringLiteral("nyaa") ? fields.id : fields.link;
        if (page.endsWith(QStringLiteral(".torrent")) || page.startsWith(QStringLiteral("magnet:")))
            page.clear();
        result.sourcePage = page;
        results.push_back(std::move(result));
    };

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QStringLiteral("item") || name == QStringLiteral("entry")) {
                reset();
                continue;
            }
            if (name == QStringLiteral("title")) {
                fields.title = xml.readElementText();
            } else if (name == QStringLiteral("link")) {
                fields.link = xml.readElementText();
            } else if (name == QStringLiteral("id")) {
                fields.id = xml.readElementText();
            } else if (name == QStringLiteral("pubDate") || name == QStringLiteral("date")) {
                fields.pubDate = xml.readElementText();
            } else if (name == QStringLiteral("description") || name == QStringLiteral("summary")) {
                fields.description = xml.readElementText();
            } else if (name == QStringLiteral("infoHash")) {
                fields.infoHash = xml.readElementText();
            } else if (name == QStringLiteral("size")) {
                fields.sizeText = xml.readElementText();
            } else if (name == QStringLiteral("seeders")) {
                fields.seeders = xml.readElementText();
            } else if (name == QStringLiteral("leechers")) {
                fields.leechers = xml.readElementText();
            } else if (name == QStringLiteral("contentLength")) {
                fields.contentLength = xml.readElementText();
            } else if (name == QStringLiteral("enclosure")) {
                const auto attrs = xml.attributes();
                fields.enclosureUrl = attrs.value(QStringLiteral("url")).toString();
                if (fields.contentLength.isEmpty())
                    fields.contentLength = attrs.value(QStringLiteral("length")).toString();
            }
        } else if (xml.isEndElement()) {
            const QString endName = xml.name().toString();
            if (endName == QStringLiteral("item") || endName == QStringLiteral("entry"))
                finish();
        }
    }

    if (xml.hasError()) {
        if (error)
            *error = xml.errorString();
        return results;
    }
    if (error)
        error->clear();
    return results;
}

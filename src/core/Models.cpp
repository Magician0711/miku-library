// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Models.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

QString SiteEntry::adapterName() const
{
    switch (adapter) {
    case AdapterType::Rss: return QStringLiteral("rss");
    case AdapterType::JsonApi: return QStringLiteral("json_api");
    case AdapterType::None: break;
    }
    return QStringLiteral("none");
}

QVector<SiteEntry> SiteCatalog::allSites() const
{
    QVector<SiteEntry> out;
    out.reserve(torrentSites.size() + fansubPages.size() + supplementarySites.size() + historicalSites.size());
    out += torrentSites;
    out += fansubPages;
    out += supplementarySites;
    out += historicalSites;
    return out;
}

SiteEntry *SiteCatalog::byId(const QString &id)
{
    for (auto *list : {&torrentSites, &fansubPages, &supplementarySites, &historicalSites}) {
        for (auto &site : *list) {
            if (site.id == id)
                return &site;
        }
    }
    return nullptr;
}

const SiteEntry *SiteCatalog::byId(const QString &id) const
{
    return const_cast<SiteCatalog *>(this)->byId(id);
}

QString ResourceResult::dedupKey() const
{
    if (auto hash = normalizeBtih(infoHash))
        return *hash;
    if (auto hash = btihFromMagnet(magnetUri))
        return *hash;
    return title.trimmed().toLower();
}

QString ResourceResult::rowKey() const
{
    // Keep quality, language, version and group in the identity. Only normalize
    // presentation whitespace/case and an optional media filename extension.
    auto normalize=[](QString s) {
        s=s.normalized(QString::NormalizationForm_KC).simplified().toCaseFolded();
        s.remove(QRegularExpression(QStringLiteral("(?:\\.(?:torrent|mkv|mp4|m4v|webm|avi))+$")));
        return s;
    };
    if(title.trimmed().isEmpty())return animeId+"|"+normalize(releaseGroup)+"|"+dedupKey();
    return animeId+"|"+normalize(releaseGroup)+"|"+normalize(title);
}

QJsonObject ResourceSource::toJson() const
{
    return {{"site_id",siteId},{"site_name",siteName},{"torrent",torrentUrl},
            {"magnet",magnetUri},{"info_hash",infoHash},{"source_page",sourcePage},{"seeders",seeders}};
}

ResourceSource ResourceSource::fromJson(const QJsonObject &o)
{
    return {o.value("site_id").toString(),o.value("site_name").toString(),o.value("torrent").toString(),
            o.value("magnet").toString(),o.value("info_hash").toString(),o.value("source_page").toString(),o.value("seeders").toInt(-1)};
}

void mergeResource(ResourceResult &target,const ResourceResult &incoming)
{
    auto add=[&](const ResourceSource &source) {
        if(!source.siteName.isEmpty()&&!target.seenOn.contains(source.siteName))target.seenOn.append(source.siteName);
        for(auto &prior:target.sources)
            if(prior.siteId==source.siteId&&prior.torrentUrl==source.torrentUrl&&prior.magnetUri==source.magnetUri&&prior.infoHash==source.infoHash) {
                prior.seeders=qMax(prior.seeders,source.seeders);return;
            }
        target.sources.append(source);
    };
    if(target.sources.isEmpty())add({target.siteId,target.siteName,target.torrentUrl,target.magnetUri,target.infoHash,target.sourcePage,target.seeders});
    if(incoming.sources.isEmpty())add({incoming.siteId,incoming.siteName,incoming.torrentUrl,incoming.magnetUri,incoming.infoHash,incoming.sourcePage,incoming.seeders});
    else for(const auto &source:incoming.sources)add(source);
    for(const auto &name:incoming.seenOn)if(!target.seenOn.contains(name))target.seenOn.append(name);
    target.seeders=qMax(target.seeders,incoming.seeders);
    // Keep a real size when another site only reported an unknown or placeholder one.
    if(target.fileSize<1024&&incoming.fileSize>target.fileSize)target.fileSize=incoming.fileSize;
    if(target.torrentUrl.isEmpty()&&target.magnetUri.isEmpty()) {
        for(const auto &source:target.sources)if(!source.torrentUrl.isEmpty()||!source.magnetUri.isEmpty()) {
            target.siteId=source.siteId;target.siteName=source.siteName;target.torrentUrl=source.torrentUrl;
            target.magnetUri=source.magnetUri;target.infoHash=source.infoHash;target.sourcePage=source.sourcePage;break;
        }
    }
}

QVector<ResourceResult> downloadCandidates(const ResourceResult &resource)
{
    auto sources=resource.sources;
    if(sources.isEmpty())sources.append({resource.siteId,resource.siteName,resource.torrentUrl,resource.magnetUri,resource.infoHash,resource.sourcePage,resource.seeders});
    std::stable_sort(sources.begin(),sources.end(),[](const auto &a,const auto &b){
        if(a.torrentUrl.isEmpty()!=b.torrentUrl.isEmpty())return !a.torrentUrl.isEmpty();
        return a.seeders>b.seeders;
    });
    QVector<ResourceResult> out;QSet<QString> seen;
    auto add=[&](const ResourceSource &source,bool magnet) {
        const QString uri=magnet?source.magnetUri:source.torrentUrl;
        if(uri.isEmpty()||seen.contains(uri))return;
        seen.insert(uri);auto candidate=resource;candidate.sources.clear();
        candidate.siteId=source.siteId;candidate.siteName=source.siteName;candidate.sourcePage=source.sourcePage;
        candidate.infoHash=source.infoHash;candidate.seeders=source.seeders;
        candidate.torrentUrl=magnet?QString():uri;candidate.magnetUri=source.magnetUri;
        out.append(candidate);
    };
    for(const auto &source:sources)add(source,false);
    for(const auto &source:sources)add(source,true);
    return out;
}

QString AnimeSubject::displayName() const
{
    if (!nameCn.isEmpty())
        return nameCn;
    if (!nameJp.isEmpty())
        return nameJp;
    if (!nameEn.isEmpty())
        return nameEn;
    return QStringLiteral("#%1").arg(bangumiId);
}

QString confidenceName(Confidence c)
{
    switch (c) {
    case Confidence::Probed: return QStringLiteral("probed");
    case Confidence::Metadata: return QStringLiteral("metadata");
    case Confidence::Guessed: return QStringLiteral("filename");
    case Confidence::Unknown: break;
    }
    return QStringLiteral("unknown");
}

QString downloadStatusName(DownloadStatus s)
{
    switch (s) {
    case DownloadStatus::Downloading: return QStringLiteral("下载中");
    case DownloadStatus::Paused: return QStringLiteral("已暂停");
    case DownloadStatus::Seeding: return QStringLiteral("做种中");
    case DownloadStatus::Pending: return QStringLiteral("等待中");
    case DownloadStatus::Completed: return QStringLiteral("已完成");
    case DownloadStatus::Error: return QStringLiteral("错误");
    }
    return QStringLiteral("等待中");
}

std::optional<QString> normalizeBtih(const QString &value)
{
    const QString v = value.trimmed();
    if (v.size() == 40) {
        bool ok = true;
        for (const QChar c : v) {
            if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F'))) {
                ok = false;
                break;
            }
        }
        if (ok)
            return v.toLower();
    }
    if (v.size() == 32) {
        QByteArray bytes(20, '\0');
        int buffer = 0;
        int bits = 0;
        int index = 0;
        for (const QChar c : v) {
            const QChar u = c.toUpper();
            const ushort uc = u.unicode();
            int val = -1;
            if (uc >= 'A' && uc <= 'Z')
                val = uc - 'A';
            else if (uc >= '2' && uc <= '7')
                val = 26 + (uc - '2');
            if (val < 0)
                return std::nullopt;
            buffer = (buffer << 5) | val;
            bits += 5;
            if (bits >= 8) {
                bits -= 8;
                if (index >= 20)
                    return std::nullopt;
                bytes[index++] = static_cast<char>((buffer >> bits) & 0xff);
            }
        }
        if (index != 20)
            return std::nullopt;
        return QString::fromLatin1(bytes.toHex());
    }
    return std::nullopt;
}

std::optional<QString> btihFromMagnet(const QString &magnet)
{
    if (magnet.isEmpty())
        return std::nullopt;
    static const QRegularExpression rx(QStringLiteral("xt=urn:btih:([^&]+)"),
                                      QRegularExpression::CaseInsensitiveOption);
    const auto m = rx.match(magnet);
    if (!m.hasMatch())
        return std::nullopt;
    return normalizeBtih(m.captured(1));
}

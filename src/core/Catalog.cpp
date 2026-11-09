// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcCatalog, "anime.catalog")

namespace {

AdapterType defaultAdapter(const QString &id)
{
    static const QHash<QString, AdapterType> map = {
        {QStringLiteral("nyaa"), AdapterType::Rss},
        {QStringLiteral("dmhy"), AdapterType::Rss},
        {QStringLiteral("mikan"), AdapterType::Rss},
        {QStringLiteral("acgrip"), AdapterType::Rss},
        {QStringLiteral("tokyotosho"), AdapterType::Rss},
        {QStringLiteral("animegarden"), AdapterType::JsonApi},
        {QStringLiteral("bangumimoe"), AdapterType::JsonApi},
        {QStringLiteral("subsplease"), AdapterType::JsonApi},
    };
    return map.value(id, AdapterType::None);
}

QString adapterToString(AdapterType type)
{
    switch (type) {
    case AdapterType::Rss: return QStringLiteral("rss");
    case AdapterType::JsonApi: return QStringLiteral("json_api");
    case AdapterType::None: break;
    }
    return QStringLiteral("none");
}

SiteEntry parseSite(const QJsonObject &obj, bool &ok)
{
    SiteEntry site;
    site.id = obj.value(QStringLiteral("id")).toString();
    site.name = obj.value(QStringLiteral("name")).toString();
    site.url = obj.value(QStringLiteral("url")).toString();
    if (site.id.isEmpty() || site.name.isEmpty() || site.url.isEmpty()) {
        ok = false;
        return site;
    }
    site.category = obj.value(QStringLiteral("category")).toString(site.category);
    site.kind = obj.value(QStringLiteral("kind")).toString(site.kind);
    site.language = obj.value(QStringLiteral("language")).toString(site.language);
    site.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    site.priority = obj.value(QStringLiteral("priority")).toInt(50);
    site.timeout = obj.value(QStringLiteral("timeout")).toDouble(20.0);
    site.searchUrl = obj.value(QStringLiteral("search_url")).toString();
    site.rssUrl = obj.value(QStringLiteral("rss_url")).toString();
    site.rssFlag = obj.value(QStringLiteral("rss")).toBool(false);

    const auto adapter = obj.value(QStringLiteral("adapter")).toString();
    if (adapter == QStringLiteral("rss"))
        site.adapter = AdapterType::Rss;
    else if (adapter == QStringLiteral("json_api"))
        site.adapter = AdapterType::JsonApi;
    else if (adapter == QStringLiteral("none"))
        site.adapter = AdapterType::None;
    else
        site.adapter = defaultAdapter(site.id);
    if (site.adapter == AdapterType::None && defaultAdapter(site.id) != AdapterType::None
        && adapter.isEmpty())
        site.adapter = defaultAdapter(site.id);

    site.raw = obj;
    if(site.adapter==AdapterType::None&&!site.rssUrl.isEmpty())site.adapter=AdapterType::Rss;
    if(site.adapter==AdapterType::JsonApi&&site.id!="animegarden"&&site.id!="bangumimoe"&&site.id!="subsplease")site.adapter=site.rssUrl.isEmpty()?AdapterType::None:AdapterType::Rss;
    if(!obj.contains("enabled"))site.enabled=site.adapter!=AdapterType::None;
    ok = true;
    return site;
}

QJsonObject siteToJson(const SiteEntry &site)
{
    QJsonObject obj = site.raw;
    obj.insert(QStringLiteral("id"), site.id);
    obj.insert(QStringLiteral("name"), site.name);
    obj.insert(QStringLiteral("url"), site.url);
    obj.insert(QStringLiteral("category"), site.category);
    obj.insert(QStringLiteral("kind"), site.kind);
    obj.insert(QStringLiteral("language"), site.language);
    obj.insert(QStringLiteral("enabled"), site.enabled);
    obj.insert(QStringLiteral("priority"), site.priority);
    obj.insert(QStringLiteral("adapter"), adapterToString(site.adapter));
    obj.insert(QStringLiteral("timeout"), site.timeout);
    if (!site.searchUrl.isEmpty())
        obj.insert(QStringLiteral("search_url"), site.searchUrl);
    if (!site.rssUrl.isEmpty())
        obj.insert(QStringLiteral("rss_url"), site.rssUrl);
    return obj;
}

} // namespace

Catalog::Catalog(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
{
}

bool Catalog::load()
{
    QFile file(m_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCatalog) << "catalog missing:" << m_path;
        return false;
    }

    QJsonParseError error{};
    const auto bytes=file.readAll();
    const auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCatalog) << "catalog parse failed:" << error.errorString();
        return false;
    }
    const auto root = doc.object();

    auto readSection = [&](const QString &key, QVector<SiteEntry> &out) {
        out.clear();
        const auto array = root.value(key).toArray();
        out.reserve(array.size());
        for (const auto &value : array) {
            if (!value.isObject())
                continue;
            bool ok = false;
            auto site = parseSite(value.toObject(), ok);
            if (ok)
                out.push_back(std::move(site));
            else
                qCWarning(lcCatalog) << "skipping bad site row in" << key;
        }
    };

    SiteCatalog candidate;
    readSection(QStringLiteral("anime_torrent_sites"), candidate.torrentSites);
    readSection(QStringLiteral("fansub_release_pages"), candidate.fansubPages);
    readSection(QStringLiteral("supplementary_sites"), candidate.supplementarySites);
    readSection(QStringLiteral("historical_sites"), candidate.historicalSites);
    QSet<QString> ids;for(const auto &site:candidate.allSites()){if(ids.contains(site.id))return false;ids.insert(site.id);}
    m_catalog=candidate;m_lastBytes=bytes;

    qCInfo(lcCatalog) << "catalog loaded:" << m_catalog.allSites().size() << "sites";
    return true;
}

bool Catalog::save(const QString &path) const
{
    const QString target = path.isEmpty() ? m_path : path;
    QJsonObject root;
    root.insert(QStringLiteral("schema_version"), QStringLiteral("1.0.0"));
    root.insert(QStringLiteral("title"), QStringLiteral("番剧下载与字幕组资源目录"));
    root.insert(QStringLiteral("updated_on"), QDate::currentDate().toString(Qt::ISODate));

    auto writeSection = [&](const QString &key, const QVector<SiteEntry> &sites) {
        QJsonArray array;
        for (const auto &site : sites)
            array.append(siteToJson(site));
        root.insert(key, array);
    };
    writeSection(QStringLiteral("anime_torrent_sites"), m_catalog.torrentSites);
    writeSection(QStringLiteral("fansub_release_pages"), m_catalog.fansubPages);
    writeSection(QStringLiteral("supplementary_sites"), m_catalog.supplementarySites);
    writeSection(QStringLiteral("historical_sites"), m_catalog.historicalSites);

    QDir().mkpath(QFileInfo(target).absolutePath());
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const auto bytes=QJsonDocument(root).toJson(QJsonDocument::Indented);
    if(file.write(bytes)!=bytes.size()||!file.commit())return false;
    qCInfo(lcCatalog) << "catalog saved to" << target;
    return true;
}

bool Catalog::setEnabled(const QString &id,bool enabled,QString *error)
{
    auto *site=get(id);if(!site)return false;
    if(enabled&&site->adapter==AdapterType::None){if(error)*error=QStringLiteral("此条目没有搜索适配器，只能作为网页目录使用。");return false;}
    const bool before=site->enabled;site->enabled=enabled;
    if(!save()){site->enabled=before;if(error)*error=QStringLiteral("无法保存站点配置。");return false;}
    emit catalogChanged();return true;
}

bool Catalog::importFrom(const QString &path,QString *error)
{
    Catalog candidate(path);
    if(!candidate.load()||candidate.sites().isEmpty()){if(error)*error=QStringLiteral("文件没有有效站点，原配置已保留。");return false;}
    const auto previous=m_catalog;m_catalog=candidate.m_catalog;
    if(!save()){m_catalog=previous;if(error)*error=QStringLiteral("无法保存导入的站点配置。");return false;}
    emit catalogChanged();return true;
}

QVector<SiteEntry> Catalog::sites(bool searchableOnly) const
{
    auto all = m_catalog.allSites();
    if (!searchableOnly)
        return all;
    QVector<SiteEntry> out;
    for (const auto &site : all) {
        if (site.isSearchable())
            out.push_back(site);
    }
    return out;
}

SiteEntry *Catalog::get(const QString &id)
{
    return m_catalog.byId(id);
}

const SiteEntry *Catalog::get(const QString &id) const
{
    return m_catalog.byId(id);
}

void Catalog::startWatching()
{
    if (m_watcher)
        return;
    m_watcher = new QFileSystemWatcher(this);
    const QFileInfo info(m_path);
    m_watcher->addPath(info.absolutePath());
    if (info.exists())
        m_watcher->addPath(info.absoluteFilePath());
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &Catalog::reloadFromWatcher);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &Catalog::reloadFromWatcher);
}

void Catalog::reloadFromWatcher()
{
    QFile file(m_path);const bool changed=file.open(QIODevice::ReadOnly)&&file.readAll()!=m_lastBytes;file.close();
    if (changed&&load())
        emit catalogChanged();
    if (m_watcher && !m_watcher->files().contains(m_path) && QFileInfo::exists(m_path))
        m_watcher->addPath(m_path);
}

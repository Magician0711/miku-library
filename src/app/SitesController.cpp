// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "SitesController.h"

#include "DialogHub.h"
#include "core/Catalog.h"
#include "net/HttpClient.h"
#include "net/Providers.h"

#include <QDateTime>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace {
enum Scope { AllSites, Searchable, Directory };

// Catalog values are short codes; show what they mean.
QString categoryName(const QString &category)
{
    if (category == QStringLiteral("public")) return QStringLiteral("公开");
    if (category == QStringLiteral("private")) return QStringLiteral("需账号");
    if (category == QStringLiteral("community")) return QStringLiteral("社区");
    return category;
}
QString languageName(const QString &language)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("zh"), QStringLiteral("中文")}, {QStringLiteral("multi"), QStringLiteral("多语言")},
        {QStringLiteral("en"), QStringLiteral("英文")}, {QStringLiteral("ja"), QStringLiteral("日文")},
        {QStringLiteral("es"), QStringLiteral("西班牙文")}};
    return names.value(language, language);
}
QString adapterLabel(AdapterType adapter)
{
    switch (adapter) {
    case AdapterType::Rss: return QStringLiteral("RSS");
    case AdapterType::JsonApi: return QStringLiteral("JSON 接口");
    case AdapterType::None: break;
    }
    return QStringLiteral("—");
}
}

SitesController::SitesController(Catalog *catalog, HttpClient *http, DialogHub *dialogs, QObject *parent)
    : QObject(parent), m_catalog(catalog), m_http(http), m_dialogs(dialogs)
    , m_model(new MapListModel({"siteId", "name", "host", "url", "category", "language", "adapter", "searchable", "enabled",
                                "state", "tone"}, "siteId", this))
{
    connect(m_catalog, &Catalog::catalogChanged, this, &SitesController::refresh);
    refresh();
}

void SitesController::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    emit filterChanged();
    refresh();
}

void SitesController::setScope(int scope)
{
    if (scope == m_scope)
        return;
    m_scope = scope;
    emit filterChanged();
    refresh();
}

void SitesController::setSelected(const QString &id)
{
    if (id == m_selected)
        return;
    m_selected = id;
    emit selectedChanged();
}

void SitesController::refresh()
{
    // Search sources first (enabled ones on top), then the browse-only directories.
    auto sites = m_catalog->sites();
    std::stable_sort(sites.begin(), sites.end(), [](const SiteEntry &a, const SiteEntry &b) {
        const int ra = a.adapter == AdapterType::None ? 2 : a.enabled ? 0 : 1;
        const int rb = b.adapter == AdapterType::None ? 2 : b.enabled ? 0 : 1;
        return ra < rb;
    });
    const QString query = m_filter.trimmed();
    QVector<QVariantMap> rows;
    int active = 0;
    for (const auto &site : sites) {
        const bool searchable = site.adapter != AdapterType::None;
        if (site.isSearchable())
            ++active;
        if (m_scope != AllSites && (m_scope == Searchable) != searchable)
            continue;
        QString host = QUrl(site.url).host();
        if (host.startsWith(QStringLiteral("www."))) host = host.mid(4);
        const QString category = categoryName(site.category), language = languageName(site.language), adapter = adapterLabel(site.adapter);
        if (!query.isEmpty()) {
            bool match = false;
            for (const auto &text : {site.name, host, category, language, adapter})
                match = match || text.contains(query, Qt::CaseInsensitive);
            if (!match)
                continue;
        }
        QString state = QStringLiteral("未测试"), tone = QStringLiteral("muted");
        if (m_testing.contains(site.id)) { state = QStringLiteral("正在测试…"); tone = QStringLiteral("active"); }
        else if (!site.lastError.isEmpty()) { state = site.lastError; tone = QStringLiteral("error"); }
        else if (site.lastSuccess.isValid()) { state = QStringLiteral("● 连接正常"); tone = QStringLiteral("done"); }
        rows.append({{"siteId", site.id}, {"name", site.name}, {"host", host}, {"url", site.url}, {"category", category},
                     {"language", language}, {"adapter", adapter}, {"searchable", searchable}, {"enabled", site.enabled},
                     {"state", state}, {"tone", tone}});
    }
    m_model->update(rows);
    m_status = QStringLiteral("%1 个条目 · %2 个搜索源已启用").arg(sites.size()).arg(active);
    emit statusChanged();
}

void SitesController::setEnabled(const QString &id, bool enabled)
{
    QString error;
    if (!m_catalog->setEnabled(id, enabled, &error)) {
        m_dialogs->message(QStringLiteral("站点设置"), error, QStringLiteral("warning"));
        refresh();
    }
}

void SitesController::testSelected()
{
    auto *site = m_catalog->get(m_selected);
    if (!site)
        return;
    auto provider = std::shared_ptr<SearchProvider>(makeProvider(*site, m_http).release());
    if (!provider) {
        site->lastError = QStringLiteral("无搜索适配器");
        site->searchAvailable = false;
        refresh();
        return;
    }
    const QString siteId = site->id;
    m_testing.insert(siteId);
    QPointer<SitesController> self(this);
    provider->testConnection([this, self, siteId, provider](bool ok, QString message) {
        if (!self)
            return;
        m_testing.remove(siteId);
        if (auto *target = m_catalog->get(siteId)) {
            target->searchAvailable = ok;
            target->lastError = ok ? QString() : message;
            if (ok)
                target->lastSuccess = QDateTime::currentDateTimeUtc();
        }
        refresh();
    });
    refresh();
}

// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "SearchAggregator.h"

#include "Providers.h"
#include "HttpClient.h"
#include "core/Catalog.h"
#include "core/TitleParser.h"

#include <QElapsedTimer>
#include <QSet>
#include <QStringList>
#include <algorithm>

namespace {

QString normalise(const QString &text)
{
    return text.normalized(QString::NormalizationForm_KC).toLower().trimmed();
}

QStringList queryTerms(const QString &query, const AnimeSubject &subject)
{
    QStringList terms;
    QSet<QString> seen;
    auto add = [&](const QString &value) {
        if (value.trimmed().isEmpty())
            return;
        const QString key = normalise(value);
        if (seen.contains(key))
            return;
        seen.insert(key);
        terms << value.trimmed();
    };
    add(subject.nameCn);
    add(subject.nameJp);
    add(subject.nameEn);
    for (const auto &alias : subject.aliases)
        add(alias);
    add(query);
    return terms;
}

QStringList termsForSite(const SiteEntry &site, const QStringList &terms)
{
    QStringList picked;
    if (site.language == QStringLiteral("zh")) {
        for (const auto &term : terms)
            if (isCjk(term))
                picked << term;
        if (picked.isEmpty())
            picked = terms;
    } else if (site.language == QStringLiteral("en")) {
        for (const auto &term : terms)
            if (!isCjk(term))
                picked << term;
        if (picked.isEmpty())
            picked = terms;
    } else {
        QStringList latin;
        QStringList cjk;
        for (const auto &term : terms) {
            if (isCjk(term))
                cjk << term;
            else
                latin << term;
        }
        picked = latin.mid(0, 2);
        if (!cjk.isEmpty())
            picked << cjk.first();
        if (picked.isEmpty())
            picked = terms;
    }
    return picked.mid(0, 3);
}

} // namespace

class SiteJob : public std::enable_shared_from_this<SiteJob> {
public:
    using Callback = std::function<void(SiteStatus, QVector<ResourceResult>)>;

    SiteJob(SiteEntry site, HttpClient *http, QStringList terms, QString animeId, Callback callback)
        : m_site(std::move(site))
        , m_client(std::make_unique<HttpClient>())
        , m_terms(std::move(terms))
        , m_animeId(std::move(animeId))
        , m_callback(std::move(callback))
    {
        Q_UNUSED(http)
    }

    void start()
    {
        m_timer.start();
        next();
    }
    void cancel(){m_finished=true;m_callback={};m_client.reset();m_results.clear();}
    QString siteId() const {return m_site.id;}

private:
    void next()
    {
        if(m_finished)return;
        if (m_index >= m_terms.size()) {
            finish();
            return;
        }

        const QString term = m_terms.at(m_index);
        auto provider = std::shared_ptr<SearchProvider>(makeProvider(m_site, m_client.get()).release());
        if (!provider) {
            m_errors << QStringLiteral("无适配器");
            ++m_index;
            next();
            return;
        }

        provider->search(term, [weak=weak_from_this(), provider](QVector<ResourceResult> results, QString error) mutable {
            const auto self=weak.lock();if(!self||self->m_finished)return;
            if (!error.isEmpty())
                self->m_errors << error;
            for (auto &result : results) {
                result.animeId = self->m_animeId;
                self->m_results.push_back(std::move(result));
            }
            ++self->m_index;
            self->next();
        });
    }

    void finish()
    {
        if (m_finished)
            return;
        m_finished = true;
        const bool ok = m_errors.isEmpty();
        SiteStatus status;
        status.siteId = m_site.id;
        status.siteName = m_site.name;
        status.ok = ok;
        status.count = m_results.size();
        status.elapsed = m_timer.elapsed() / 1000.0;
        status.error = ok ? QString() : m_errors.value(0);
        m_callback(status, m_results);
    }

    SiteEntry m_site;
    std::unique_ptr<HttpClient> m_client;
    QStringList m_terms;
    QString m_animeId;
    Callback m_callback;
    QVector<ResourceResult> m_results;
    QStringList m_errors;
    QElapsedTimer m_timer;
    int m_index = 0;
    bool m_finished = false;
};

SearchAggregator::SearchAggregator(Catalog *catalog, HttpClient *http, BangumiClient *bangumi, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_http(http)
    , m_bangumi(bangumi)
{
}

SearchAggregator::~SearchAggregator(){cancel();}

void SearchAggregator::cancel()
{
    ++m_generation;m_pending.clear();
    for(const auto &job:m_jobs)job->cancel();
    m_jobs.clear();m_active=0;m_seen.clear();
}

void SearchAggregator::search(const QString &query, const AnimeSubject &subject, const QSet<QString> &siteIds)
{
    cancel();
    m_active = 0;
    m_done = 0;
    m_total = 0;
    m_seen.clear();

    const QStringList terms = queryTerms(query, subject);
    if (terms.isEmpty()) {
        emit finished();
        return;
    }

    auto sites = m_catalog->sites(true);
    if (!siteIds.isEmpty()) {
        QVector<SiteEntry> filtered;
        for (const auto &site : sites)
            if (siteIds.contains(site.id))
                filtered.push_back(site);
        sites = std::move(filtered);
    }
    std::sort(sites.begin(), sites.end(), [](const SiteEntry &a, const SiteEntry &b) {
        return a.priority < b.priority;
    });

    const int generation = m_generation;
    for (const auto &site : sites) {
        auto termsFor = termsForSite(site, terms);
        if (termsFor.isEmpty())
            continue;
        auto job = std::make_shared<SiteJob>(site, m_http, termsFor, QString::number(subject.bangumiId),
                                             [this, generation](SiteStatus status, QVector<ResourceResult> results) {
                                                 onJobDone(generation, status, results);
                                             });
        m_jobs.push_back(job);
        m_pending.push_back(job);
        ++m_total;
    }

    emit started(m_total);
    if (m_total == 0) {
        emit finished();
        return;
    }
    startNext();
}

void SearchAggregator::startNext()
{
    while (m_active < m_concurrency && !m_pending.isEmpty()) {
        const auto job = m_pending.takeFirst();
        ++m_active;
        job->start();
    }
}

void SearchAggregator::onJobDone(int generation, const SiteStatus &status, const QVector<ResourceResult> &results)
{
    if (generation != m_generation)
        return;
    --m_active;
    ++m_done;
    m_jobs.erase(std::remove_if(m_jobs.begin(),m_jobs.end(),[&](const auto &job){return job->siteId()==status.siteId;}),m_jobs.end());
    merge(results);
    emit siteFinished(status);
    if (m_done >= m_total)
        emit finished();
    else
        startNext();
}

void SearchAggregator::merge(const QVector<ResourceResult> &results)
{
    QVector<ResourceResult> fresh;
    for (auto result : results) {
        const QString key = result.rowKey();
        auto it = m_seen.find(key);
        if (it == m_seen.end()) {
            mergeResource(result,result);
            m_seen.insert(key, result);
            fresh.push_back(std::move(result));
            continue;
        }
        auto &prior = it.value();
        mergeResource(prior,result);
        // The UI must receive updates as slower sites add alternate sources.
        fresh.push_back(prior);
    }
    if (!fresh.isEmpty())
        emit batchReady(fresh);
}

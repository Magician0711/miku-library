// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "UpdateChecker.h"

#include "SearchAggregator.h"

#include <algorithm>

UpdateChecker::UpdateChecker(Catalog *catalog, HttpClient *http, BangumiClient *bangumi, LocalLibrary *library,
                             QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_search(new SearchAggregator(catalog, http, bangumi, this))
{
    // Slower sites send rows that are updated later with extra sources; keep the latest.
    connect(m_search, &SearchAggregator::batchReady, this, [this](const QVector<ResourceResult> &results) {
        for (const auto &result : results)
            m_results.insert(result.rowKey(), result);
    });
    connect(m_search, &SearchAggregator::finished, this, &UpdateChecker::evaluate);
}

void UpdateChecker::start()
{
    if (m_running)
        return;
    m_queue = m_library->updateTargets();
    m_checked = m_withUpdates = 0;
    m_running = true;
    next();
}

void UpdateChecker::next()
{
    if (m_queue.isEmpty()) {
        m_running = false;
        emit finished(m_checked, m_withUpdates);
        return;
    }
    m_current = m_queue.takeFirst();
    m_results.clear();
    const auto &subject = m_current.subject;
    m_search->search(subject.nameCn.isEmpty() ? subject.displayName() : subject.nameCn, subject);
}

void UpdateChecker::evaluate()
{
    ++m_checked;
    QVector<ResourceResult> candidates;
    const auto plans = m_library->newEpisodes(m_current, m_results.values().toVector(), &candidates);
    QStringList episodes;
    for (const auto &plan : plans)
        for (const auto &episode : plan.episodes.isEmpty() ? plan.onlyEpisodes : plan.episodes)
            if (!episodes.contains(episode)) episodes.append(episode);
    std::sort(episodes.begin(), episodes.end(), [](const QString &a, const QString &b) { return a.toDouble() < b.toDouble(); });
    if (!episodes.isEmpty()) {
        ++m_withUpdates;
        emit found({m_current, candidates, episodes});
    }
    // Leave the event loop a turn before the next search starts.
    QMetaObject::invokeMethod(this, &UpdateChecker::next, Qt::QueuedConnection);
}

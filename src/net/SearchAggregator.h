#pragma once

#include "Models.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#include <memory>

class Catalog;
class HttpClient;
class BangumiClient;
class SiteJob;

class SearchAggregator : public QObject {
    Q_OBJECT
public:
    SearchAggregator(Catalog *catalog, HttpClient *http, BangumiClient *bangumi, QObject *parent = nullptr);
    ~SearchAggregator() override;

    void search(const QString &query, const AnimeSubject &subject,
                const QSet<QString> &siteIds = {});
    void setConcurrency(int value) { m_concurrency = qMax(1, value); }
    void cancel();
    int retainedJobs() const {return m_jobs.size();}

signals:
    void batchReady(QVector<ResourceResult> results);
    void siteFinished(SiteStatus status);
    void finished();
    void started(int siteCount);

private:
    void startNext();
    void onJobDone(int generation, const SiteStatus &status, const QVector<ResourceResult> &results);
    void merge(const QVector<ResourceResult> &results);

    Catalog *m_catalog;
    HttpClient *m_http;
    BangumiClient *m_bangumi;
    int m_concurrency = 6;
    int m_generation = 0;
    int m_active = 0;
    int m_done = 0;
    int m_total = 0;
    QVector<std::shared_ptr<SiteJob>> m_pending;
    QVector<std::shared_ptr<SiteJob>> m_jobs;
    QHash<QString, ResourceResult> m_seen;
};

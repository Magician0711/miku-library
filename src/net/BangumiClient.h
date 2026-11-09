// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "Models.h"

#include <QHash>
#include <QDateTime>
#include <QObject>
#include <QUrl>

#include <functional>

class HttpClient;

class BangumiClient : public QObject {
    Q_OBJECT
public:
    explicit BangumiClient(HttpClient *http, QObject *parent = nullptr);

    void search(const QString &term, int limit,
                std::function<void(QVector<AnimeSubject>, QString)> callback);
    void getSubject(int id,
                    std::function<void(AnimeSubject, QString)> callback);
    using CalendarDays = QVector<QPair<int, QVector<AnimeSubject>>>;
    // cachedAt is valid when Bangumi could not be reached and the last saved
    // schedule was returned instead.
    void calendar(std::function<void(CalendarDays, QString, QDateTime)> callback);
    // Where the last successful schedule is kept for offline use; empty disables it.
    void setCacheDir(const QString &dir) { m_cacheDir = dir; }

    static QString coverUrl(const QString &url, int width = 400);

private:
    QString m_cacheDir;
    QUrl endpoint(const QString &path) const;
    HttpClient *m_http;
    // Subject metadata rarely changes within a session; reopening a show or the
    // library should not wait for the network again. Concurrent requests for the
    // same id share one reply.
    QHash<int, AnimeSubject> m_subjects;
    QHash<int, QVector<std::function<void(AnimeSubject, QString)>>> m_subjectWaiters;
};

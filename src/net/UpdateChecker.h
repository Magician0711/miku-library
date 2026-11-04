// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/LocalLibrary.h"

#include <QHash>
#include <QObject>
#include <QVector>

class Catalog;
class HttpClient;
class BangumiClient;
class SearchAggregator;

// New episodes from a followed release group, ready to hand to the downloader.
struct UpdateNotice {
    UpdateTarget target;
    QVector<ResourceResult> candidates;   // matching releases, filtered by the preference
    QStringList episodes;                 // episode numbers that would be added
};

// Startup check: for each anime the user downloads from, search once and report
// episodes newer than the latest one held from the same group and kind of release.
// Uses its own search pipeline so it never disturbs the detail page.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    UpdateChecker(Catalog *catalog, HttpClient *http, BangumiClient *bangumi, LocalLibrary *library,
                  QObject *parent = nullptr);
    void start();
    bool running() const { return m_running; }

signals:
    void found(UpdateNotice notice);
    void finished(int checked, int withUpdates);

private:
    void next();
    void evaluate();

    LocalLibrary *m_library;
    SearchAggregator *m_search;
    QVector<UpdateTarget> m_queue;
    UpdateTarget m_current;
    QHash<QString, ResourceResult> m_results;
    int m_checked = 0;
    int m_withUpdates = 0;
    bool m_running = false;
};

// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QUrl>
#include <QVersionNumber>

class HttpClient;

// Compares this build with the newest GitHub release. Any failure (offline, rate
// limit, the repository not being public) ends quietly with a status message.
class AppUpdate : public QObject {
    Q_OBJECT
public:
    static constexpr const char *kReleasesPage = "https://github.com/Magician0711/anime-nexus/releases";

    explicit AppUpdate(HttpClient *http, QObject *parent = nullptr);
    void check();
    bool running() const { return m_running; }

    // "v0.7.0-20261104" or "0.7.0" -> 0.7.0; null when the tag carries no version.
    static QVersionNumber versionFromTag(const QString &tag);

signals:
    void available(const QString &version, const QUrl &page);
    // Human-readable outcome of the last check, for the About page.
    void checked(const QString &status);

private:
    HttpClient *m_http;
    bool m_running = false;
};

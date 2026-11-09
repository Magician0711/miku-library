// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QUrl>

#include <functional>

class HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);

    QNetworkAccessManager *manager() const { return m_manager; }

    QNetworkReply *get(const QUrl &url, int timeoutMs = 20000);
    // For a read the user is waiting on: tries once more when the connection drops
    // without an answer. done runs on context and owns the final reply.
    void getWithRetry(const QUrl &url, int timeoutMs, QObject *context, std::function<void(QNetworkReply *)> done, int retries = 1);
    QNetworkReply *postJson(const QUrl &url, const QJsonObject &body, int timeoutMs = 20000);
    QNetworkReply *postForm(const QUrl &url, const QByteArray &body, int timeoutMs = 20000);

    static QString userAgent();
    // Proxy for every request the app makes (sites, Bangumi, covers, update checks).
    // mode: system | none | http | socks5. Loopback addresses are always direct, so
    // local downloader connections keep working.
    static void applyProxy(const QString &mode, const QString &host, int port);
    static QString replyError(QNetworkReply *reply);
    static QJsonDocument json(QNetworkReply *reply, bool *ok = nullptr);

private:
    QNetworkRequest request(const QUrl &url, int timeoutMs) const;

    QNetworkAccessManager *m_manager;
};

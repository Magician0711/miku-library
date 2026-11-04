// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QUrl>

class HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);

    QNetworkAccessManager *manager() const { return m_manager; }

    QNetworkReply *get(const QUrl &url, int timeoutMs = 20000);
    QNetworkReply *postJson(const QUrl &url, const QJsonObject &body, int timeoutMs = 20000);
    QNetworkReply *postForm(const QUrl &url, const QByteArray &body, int timeoutMs = 20000);

    static QString userAgent();
    static QString replyError(QNetworkReply *reply);
    static QJsonDocument json(QNetworkReply *reply, bool *ok = nullptr);

private:
    QNetworkRequest request(const QUrl &url, int timeoutMs) const;

    QNetworkAccessManager *m_manager;
};

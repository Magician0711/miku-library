// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "HttpClient.h"

#include <QHostAddress>
#include <QJsonParseError>
#include <QNetworkProxy>
#include <QTimer>

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    m_manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

QString HttpClient::userAgent()
{
    return QStringLiteral("miku-library/" MIKU_LIBRARY_VERSION " (Windows; Qt)");
}

namespace {
class AppProxyFactory final : public QNetworkProxyFactory {
public:
    AppProxyFactory(QString mode, QNetworkProxy proxy) : m_mode(std::move(mode)), m_proxy(std::move(proxy)) {}
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override
    {
        const QString host = query.peerHostName();
        if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0 || QHostAddress(host).isLoopback())
            return {QNetworkProxy(QNetworkProxy::NoProxy)};
        if (m_mode == QStringLiteral("system"))
            return systemProxyForQuery(query);
        if (m_mode == QStringLiteral("none"))
            return {QNetworkProxy(QNetworkProxy::NoProxy)};
        return {m_proxy};
    }

private:
    QString m_mode;
    QNetworkProxy m_proxy;
};
}

void HttpClient::applyProxy(const QString &mode, const QString &host, int port)
{
    const bool manual = (mode == QStringLiteral("http") || mode == QStringLiteral("socks5"))
                        && !host.trimmed().isEmpty() && port > 0 && port < 65536;
    QNetworkProxy proxy;
    if (manual)
        proxy = QNetworkProxy(mode == QStringLiteral("http") ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy,
                              host.trimmed(), quint16(port));
    // An incomplete manual entry falls back to the system settings rather than to nothing.
    const QString effective = manual || mode == QStringLiteral("none") ? mode : QStringLiteral("system");
    QNetworkProxyFactory::setApplicationProxyFactory(new AppProxyFactory(effective, proxy));
}

QNetworkRequest HttpClient::request(const QUrl &url, int timeoutMs) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    req.setRawHeader("Accept", "application/json, application/rss+xml, application/xml, text/xml, */*");
    req.setTransferTimeout(timeoutMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

QNetworkReply *HttpClient::get(const QUrl &url, int timeoutMs)
{
    return m_manager->get(request(url, timeoutMs));
}

void HttpClient::getWithRetry(const QUrl &url, int timeoutMs, QObject *context, std::function<void(QNetworkReply *)> done, int retries)
{
    constexpr int kRetryDelayMs = 800;
    auto *reply = get(url, timeoutMs);
    connect(reply, &QNetworkReply::finished, context, [=, this, done = std::move(done)]() mutable {
        const auto error = reply->error();
        // A timeout is left out: the user has already waited for the whole of it.
        const bool dropped = error == QNetworkReply::RemoteHostClosedError || error == QNetworkReply::ProxyConnectionClosedError
                             || error == QNetworkReply::TemporaryNetworkFailureError;
        if (!dropped || retries <= 0) {
            done(reply);
            return;
        }
        reply->deleteLater();
        QTimer::singleShot(kRetryDelayMs, context, [=, this, done = std::move(done)]() mutable {
            getWithRetry(url, timeoutMs, context, std::move(done), retries - 1);
        });
    });
}

QNetworkReply *HttpClient::postJson(const QUrl &url, const QJsonObject &body, int timeoutMs)
{
    auto req = request(url, timeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return m_manager->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply *HttpClient::postForm(const QUrl &url, const QByteArray &body, int timeoutMs)
{
    auto req = request(url, timeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    return m_manager->post(req, body);
}

QString HttpClient::replyError(QNetworkReply *reply)
{
    if (!reply)
        return QStringLiteral("no reply");
    if (reply->error() == QNetworkReply::NoError)
        return {};
    return QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(static_cast<int>(reply->error()));
}

QJsonDocument HttpClient::json(QNetworkReply *reply, bool *ok)
{
    if (ok)
        *ok = false;
    if (!reply)
        return {};
    const auto data = reply->readAll();
    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(data, &error);
    if (ok)
        *ok = error.error == QJsonParseError::NoError;
    return doc;
}

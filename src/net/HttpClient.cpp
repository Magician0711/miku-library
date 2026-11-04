#include "HttpClient.h"

#include <QJsonParseError>

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    m_manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

QString HttpClient::userAgent()
{
    return QStringLiteral("anime-nexus-cpp/" ANIME_NEXUS_VERSION " (Windows; Qt)");
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

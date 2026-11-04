// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "AppUpdate.h"

#include "HttpClient.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>

AppUpdate::AppUpdate(HttpClient *http, QObject *parent)
    : QObject(parent)
    , m_http(http)
{
}

QVersionNumber AppUpdate::versionFromTag(const QString &tag)
{
    static const QRegularExpression pattern(QStringLiteral("^v?(\\d+(?:\\.\\d+){1,3})(?:$|[^\\d.])"));
    const auto match = pattern.match(tag.trimmed());
    return match.hasMatch() ? QVersionNumber::fromString(match.captured(1)) : QVersionNumber();
}

void AppUpdate::check()
{
    if (m_running)
        return;
    m_running = true;
    auto *reply = m_http->get(QUrl(QStringLiteral("https://api.github.com/repos/Magician0711/anime-nexus/releases/latest")), 15000);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_running = false;
        bool ok = false;
        const auto release = HttpClient::json(reply, &ok).object();
        const auto latest = versionFromTag(release.value(QStringLiteral("tag_name")).toString());
        if (reply->error() != QNetworkReply::NoError || !ok || latest.isNull()) {
            emit checked(QStringLiteral("暂时无法检查更新，请稍后再试。"));
            return;
        }
        const auto current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
        if (QVersionNumber::compare(latest, current) <= 0) {
            emit checked(QStringLiteral("已是最新版本。"));
            return;
        }
        QUrl page(release.value(QStringLiteral("html_url")).toString());
        if (!page.isValid() || page.scheme() != QStringLiteral("https")) page = QUrl(QString::fromLatin1(kReleasesPage));
        emit checked(QStringLiteral("发现新版本 %1。").arg(latest.toString()));
        emit available(latest.toString(), page);
    });
}

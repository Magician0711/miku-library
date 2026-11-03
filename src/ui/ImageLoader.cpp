#include "ImageLoader.h"

#include "net/HttpClient.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QImage thumbnail(QImage image)
{
    image = image.scaled(200, 300, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (image.width() > 200 || image.height() > 300) {
        const int x = qMax(0, (image.width() - 200) / 2);
        const int y = qMax(0, (image.height() - 300) / 2);
        image = image.copy(x, y, 200, 300);
    }
    return image;
}

} // namespace

ImageLoader::ImageLoader(const QString &cacheDir, QObject *parent)
    : QObject(parent)
    , m_cacheDir(cacheDir)
    , m_manager(new QNetworkAccessManager(this))
{
    QDir().mkpath(m_cacheDir);
}

QString ImageLoader::cachePath(const QString &url) const
{
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(m_cacheDir).filePath(QString::fromLatin1(hash) + QStringLiteral(".img"));
}

void ImageLoader::request(const QString &url, std::function<void(QImage)> callback)
{
    if (url.isEmpty())
        return;
    if (const auto it = m_memory.constFind(url); it != m_memory.constEnd()) {
        callback(it.value());
        return;
    }

    auto &waiters = m_waiters[url];
    waiters.push_back(std::move(callback));
    if (waiters.size() > 1)
        return;

    const QString path = cachePath(url);
    if (QFile::exists(path)) {
        QImage image(path);
        if (!image.isNull()) {
            finish(url, thumbnail(image));
            return;
        }
    }
    start(url);
}

void ImageLoader::start(const QString &url)
{
    const QUrl parsed(url);
    if (parsed.scheme() != QStringLiteral("https"))
        return finish(url, {});
    const QString host = parsed.host();
    if (host != QStringLiteral("lain.bgm.tv") && host != QStringLiteral("bgm.tv")
        && host != QStringLiteral("subsplease.org"))
        return finish(url, {});

    QNetworkRequest request(parsed);
    request.setHeader(QNetworkRequest::UserAgentHeader, HttpClient::userAgent());
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, path = cachePath(url)] {
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || data.size() > 4 * 1024 * 1024)
            return finish(url, {});
        QImage image;
        if (!image.loadFromData(data))
            return finish(url, {});
        image = thumbnail(image);
        image.save(path, "PNG");
        finish(url, image);
    });
}

void ImageLoader::finish(const QString &url, const QImage &image)
{
    if (!image.isNull())
        m_memory.insert(url, image);
    const auto waiters = m_waiters.take(url);
    for (const auto &callback : waiters)
        callback(image);
}

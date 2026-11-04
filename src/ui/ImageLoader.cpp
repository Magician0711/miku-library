// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "ImageLoader.h"

#include "net/HttpClient.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

namespace {

// Large enough for a poster card on a 150 % display (216 x 305 logical).
constexpr int kWidth = 400;
constexpr int kHeight = 600;

QImage thumbnail(QImage image)
{
    image = image.scaled(kWidth, kHeight, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (image.width() > kWidth || image.height() > kHeight) {
        const int x = qMax(0, (image.width() - kWidth) / 2);
        const int y = qMax(0, (image.height() - kHeight) / 2);
        image = image.copy(x, y, kWidth, kHeight);
    }
    return image;
}

void store(const QImage &image, const QString &path)
{
    // JPEG keeps the disk cache small; fall back to PNG if the plugin is missing.
    if (!image.save(path, "JPG", 90))
        image.save(path, "PNG");
}

} // namespace

ImageLoader::ImageLoader(const QString &cacheDir, QObject *parent)
    : QObject(parent)
    // Each cover size gets its own folder, so a format change never mixes files.
    , m_cacheDir(QDir(cacheDir).filePath(QStringLiteral("%1x%2").arg(kWidth).arg(kHeight)))
    , m_manager(new QNetworkAccessManager(this))
    , m_memory(96 * 1024) // KiB of decoded images kept in memory
{
    QDir().mkpath(m_cacheDir);
    m_pool.setMaxThreadCount(2);
    // Covers from the older 200 x 300 format sit directly in the parent folder
    // and are never read again; remove them in the background.
    m_pool.start([root = cacheDir] {
        const QDir dir(root);
        for (const auto &name : dir.entryList({QStringLiteral("*.img")}, QDir::Files))
            QFile::remove(dir.filePath(name));
    });
}

ImageLoader::~ImageLoader()
{
    // Workers post results back to this object; none may outlive it.
    m_pool.clear();
    m_pool.waitForDone();
}

QString ImageLoader::cachePath(const QString &url) const
{
    // The size is part of the key so older, smaller cache files are not reused.
    const QByteArray hash = QCryptographicHash::hash((url + QStringLiteral("|%1x%2").arg(kWidth).arg(kHeight)).toUtf8(),
                                                     QCryptographicHash::Sha1).toHex();
    return QDir(m_cacheDir).filePath(QString::fromLatin1(hash) + QStringLiteral(".img"));
}

void ImageLoader::request(const QString &url, std::function<void(QImage)> callback)
{
    if (url.isEmpty())
        return;
    if (const QImage *cached = m_memory.object(url)) {
        callback(*cached);
        return;
    }

    auto &waiters = m_waiters[url];
    waiters.push_back(std::move(callback));
    if (waiters.size() > 1)
        return;

    const QString path = cachePath(url);
    m_pool.start([this, url, path] {
        QImage image;
        if (QFile::exists(path))
            image.load(path);
        QMetaObject::invokeMethod(this, [this, url, image] {
            if (image.isNull()) download(url);
            else finish(url, image);
        }, Qt::QueuedConnection);
    });
}

void ImageLoader::download(const QString &url)
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || data.isEmpty() || data.size() > 4 * 1024 * 1024)
            return finish(url, {});
        const QString path = cachePath(url);
        m_pool.start([this, url, path, data] {
            QImage image;
            if (image.loadFromData(data)) {
                image = thumbnail(image);
                store(image, path);
            }
            QMetaObject::invokeMethod(this, [this, url, image] { finish(url, image); }, Qt::QueuedConnection);
        });
    });
}

void ImageLoader::finish(const QString &url, const QImage &image)
{
    if (!image.isNull())
        m_memory.insert(url, new QImage(image), qMax<qsizetype>(1, image.sizeInBytes() / 1024));
    const auto waiters = m_waiters.take(url);
    for (const auto &callback : waiters)
        callback(image);
}

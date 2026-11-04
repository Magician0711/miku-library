#pragma once

#include <QCache>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QVector>

#include <functional>

class QNetworkAccessManager;

// Cover images: memory cache, then disk cache, then network. Decoding and scaling
// run on a small worker pool so a grid of posters never stalls the UI thread.
class ImageLoader : public QObject {
    Q_OBJECT
public:
    explicit ImageLoader(const QString &cacheDir, QObject *parent = nullptr);
    ~ImageLoader() override;

    void request(const QString &url, std::function<void(QImage)> callback);

private:
    void download(const QString &url);
    void finish(const QString &url, const QImage &image);
    QString cachePath(const QString &url) const;

    QString m_cacheDir;
    QNetworkAccessManager *m_manager;
    QThreadPool m_pool;
    QCache<QString, QImage> m_memory;
    QHash<QString, QVector<std::function<void(QImage)>>> m_waiters;
};

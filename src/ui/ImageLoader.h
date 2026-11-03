#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class QNetworkAccessManager;

class ImageLoader : public QObject {
    Q_OBJECT
public:
    explicit ImageLoader(const QString &cacheDir, QObject *parent = nullptr);

    void request(const QString &url, std::function<void(QImage)> callback);

private:
    void start(const QString &url);
    void finish(const QString &url, const QImage &image);
    QString cachePath(const QString &url) const;

    QString m_cacheDir;
    QNetworkAccessManager *m_manager;
    QHash<QString, QImage> m_memory;
    QHash<QString, QVector<std::function<void(QImage)>>> m_waiters;
};

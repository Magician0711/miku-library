#pragma once

#include "Models.h"

#include <QObject>
#include <QUrl>

#include <functional>

class HttpClient;

class BangumiClient : public QObject {
    Q_OBJECT
public:
    explicit BangumiClient(HttpClient *http, QObject *parent = nullptr);

    void search(const QString &term, int limit,
                std::function<void(QVector<AnimeSubject>, QString)> callback);
    void getSubject(int id,
                    std::function<void(AnimeSubject, QString)> callback);
    void calendar(std::function<void(QVector<QPair<int, QVector<AnimeSubject>>>, QString)> callback);

    static QString coverUrl(const QString &url, int width = 400);

private:
    QUrl endpoint(const QString &path) const;
    HttpClient *m_http;
};

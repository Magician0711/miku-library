// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "ExternalDownloader.h"
#include "core/AppConfig.h"
#include <QNetworkAccessManager>
#include <QJsonArray>

class DownloaderRpc : public QObject {
    Q_OBJECT
public:
    DownloaderRpc(LocalLibrary *library,AppConfig *config,QObject *parent=nullptr);
    bool enabled(const QString &type) const;
    bool manageable(const LibraryItem &item)const;
    void add(const LibraryItem &item,bool stopOnCompletion,ExternalDownloader::Callback callback);
    void poll();
    void control(const QString &key,const QString &action,std::function<void(bool,QString)> callback={});
    QHash<QString,DownloadTask> tasks()const{return m_tasks;}
signals:
    void changed();
    void error(QString message);
private:
    using Reply=std::function<void(bool,QJsonValue,QString)>;
    using Wire=std::function<void(int,QByteArray,QString,QByteArray)>;
    void wire(const QString &type,const QString &path,const QByteArray &body,const QByteArray &contentType,Wire callback,bool get=false);
    void qb(const QString &method,const QMap<QString,QString> &values,Reply callback,bool get=false);
    void call(const QString &type,const QString &method,QJsonValue params,Reply callback,int attempt=0);
    void synchronize(const LibraryItem &item,const QJsonObject &status);
    void files(const LibraryItem &item,bool complete);
    LocalLibrary *m_library;AppConfig *m_config;QNetworkAccessManager m_net;ExternalDownloader m_prepare;
    QHash<QString,DownloadTask> m_tasks;QSet<QString> m_busy,m_stopping;QString m_qbLogin;
    QByteArray m_transmissionSession;
};

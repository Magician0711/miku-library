// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QTimer>
#include <functional>

class InstanceGuard : public QObject {
public:
    explicit InstanceGuard(const QString &directory,QObject *parent=nullptr)
        :QObject(parent),m_lock(QDir(directory).filePath("instance.lock")) {
        QDir().mkpath(directory);
        m_name="AnimeNexus-"+QString::fromLatin1(QCryptographicHash::hash(QDir(directory).canonicalPath().toCaseFolded().toUtf8(),QCryptographicHash::Sha256).toHex().left(32));
    }
    enum Result {Primary,Forwarded,Busy};
    Result acquire(const QStringList &arguments) {
        if(!m_lock.tryLock(0)) {
            QLocalSocket socket;socket.connectToServer(m_name);
            if(!socket.waitForConnected(1500))return Busy;
            QJsonArray args;for(const auto &arg:arguments)args.append(arg);
            socket.write(QJsonDocument(args).toJson(QJsonDocument::Compact)+'\n');
            if(!socket.waitForBytesWritten(1500))return Busy;
            if(!socket.waitForReadyRead(1500)||socket.readAll()!="OK")return Busy;
            return Forwarded;
        }
        QLocalServer::removeServer(m_name);
        m_server.setSocketOptions(QLocalServer::UserAccessOption);
        if(!m_server.listen(m_name)){m_lock.unlock();return Busy;}
        connect(&m_server,&QLocalServer::newConnection,this,[this]{while(auto *socket=m_server.nextPendingConnection()) {
            connect(socket,&QLocalSocket::readyRead,socket,[this,socket]{
                auto bytes=socket->property("message").toByteArray()+socket->readAll();
                if(bytes.size()>65536){socket->abort();return;}
                socket->setProperty("message",bytes);if(!bytes.contains('\n'))return;
                const auto doc=QJsonDocument::fromJson(bytes.left(bytes.indexOf('\n')));if(!doc.isArray()){socket->abort();return;}
                QStringList args;for(const auto &arg:doc.array())if(arg.isString())args.append(arg.toString());
                socket->write("OK");socket->flush();socket->disconnectFromServer();
                if(onArguments)onArguments(args);else m_pending.append(args);
            });
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            QTimer::singleShot(5000,socket,[socket]{socket->disconnectFromServer();});
        }});
        return Primary;
    }
    void ready(){const auto pending=m_pending;m_pending.clear();for(const auto &args:pending)if(onArguments)onArguments(args);}
    std::function<void(QStringList)> onArguments;
private:
    QLockFile m_lock;QLocalServer m_server;QString m_name;QVector<QStringList> m_pending;
};

// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QCoreApplication>
#include <functional>
struct HttpFixture : QTcpServer {
    struct Response {QByteArray data,headers;int code=200,delay=0;};
    std::function<Response(QByteArray,QByteArray)> handler;
    QStringList requests;
    HttpFixture(){listen(QHostAddress::LocalHost);connect(this,&QTcpServer::newConnection,this,[this]{while(auto *socket=nextPendingConnection()){
        connect(socket,&QTcpSocket::readyRead,socket,[this,socket]{
            auto bytes=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",bytes);
            int end=bytes.indexOf("\r\n\r\n");if(end<0||socket->property("handled").toBool())return;
            const auto header=bytes.left(end);int length=0;for(auto line:header.split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
            if(bytes.size()<end+4+length)return;socket->setProperty("handled",true);requests.append(QString::fromUtf8(header.split('\n').first()));
            auto response=handler?handler(header,bytes.mid(end+4,length)):Response{};
            QTimer::singleShot(response.delay,socket,[socket,response]{socket->write("HTTP/1.1 "+QByteArray::number(response.code)+" Fixture\r\nContent-Length: "+QByteArray::number(response.data.size())+"\r\nConnection: close\r\n"+response.headers+"\r\n"+response.data);socket->disconnectFromHost();});
        });connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    }});}
    QString url(QString path={})const{return "http://127.0.0.1:"+QString::number(serverPort())+path;}
};
inline bool spinUntil(std::function<bool()> test,int timeout=5000){QElapsedTimer timer;timer.start();do{QCoreApplication::processEvents();if(test())return true;QThread::msleep(5);}while(timer.elapsed()<timeout);return false;}

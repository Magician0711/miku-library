// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include <QCoreApplication>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
int main(int argc,char **argv){QCoreApplication app(argc,argv);const auto args=app.arguments();if(args.size()<2)return 1;
    QJsonArray data;for(int i=2;i<args.size();++i)data.append(args[i]);QSaveFile out(args[1]);
    const auto bytes=QJsonDocument(data).toJson();return out.open(QIODevice::WriteOnly)&&out.write(bytes)==bytes.size()&&out.commit()?0:2;}

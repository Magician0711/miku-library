// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QJsonObject>
#include <QMap>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QStringList>

struct RpcProfile {
    bool enabled=false;QString url,username,password;
    QJsonObject toJson()const{return {{"enabled",enabled},{"url",url},{"username",username},{"password",password}};}
    static RpcProfile fromJson(const QJsonObject &o){return {o.value("enabled").toBool(),o.value("url").toString(),o.value("username").toString(),o.value("password").toString()};}
};

struct DownloaderSettings {
    QString selected="builtin";
    QMap<QString,QString> programs;
    QMap<QString,RpcProfile> rpc;
    RpcProfile rpcProfile(const QString &type) const {
        auto profile=rpc.value(type);if(profile.url.isEmpty())profile.url=type=="qbittorrent"?"http://127.0.0.1:8080":type=="transmission"?"http://127.0.0.1:9091/transmission/rpc":"http://127.0.0.1:6800/jsonrpc";return profile;
    }
    static bool supportsRpc(const QString &type){return type=="qbittorrent"||type=="transmission"||type=="aria2";}
    QString customArguments=QStringLiteral("\"{input}\"");
    static QStringList types(){return {"builtin","qbittorrent","transmission","aria2","bitcomet","custom"};}
    static QString name(const QString &type) {
        if(type=="builtin")return QStringLiteral("内置下载器");
        if(type=="qbittorrent")return "qBittorrent";
        if(type=="transmission")return "Transmission";
        if(type=="aria2")return "aria2";
        if(type=="bitcomet")return "BitComet";
        return QStringLiteral("自定义下载器");
    }
    QJsonObject toJson() const {
        QJsonObject paths;for(auto it=programs.begin();it!=programs.end();++it)paths.insert(it.key(),it.value());
        QJsonObject profiles;for(auto it=rpc.begin();it!=rpc.end();++it)profiles.insert(it.key(),it.value().toJson());
        return {{"selected",selected},{"programs",paths},{"custom_arguments",customArguments},{"rpc",profiles}};
    }
    static DownloaderSettings fromJson(const QJsonObject &o) {
        DownloaderSettings s;s.selected=o.value("selected").toString("builtin");
        if(!types().contains(s.selected))s.selected="builtin";
        const auto paths=o.value("programs").toObject();
        const auto profiles=o.value("rpc").toObject();for(auto it=profiles.begin();it!=profiles.end();++it)if(supportsRpc(it.key()))s.rpc.insert(it.key(),RpcProfile::fromJson(it.value().toObject()));
        for(auto it=paths.begin();it!=paths.end();++it)if(types().contains(it.key()))s.programs.insert(it.key(),it.value().toString());
        s.customArguments=o.value("custom_arguments").toString(s.customArguments);return s;
    }
    static QString detect(const QString &type) {
        const QMap<QString,QStringList> names={{"qbittorrent",{"qbittorrent.exe"}},
            {"transmission",{"transmission-qt.exe"}},{"aria2",{"aria2c.exe"}},{"bitcomet",{"BitComet.exe","BitComet_x64.exe"}}};
        for(const auto &exe:names.value(type)) {
            const QString found=QStandardPaths::findExecutable(exe);if(!found.isEmpty())return found;
            for(const auto &registry:{QStringLiteral("HKEY_CURRENT_USER"),QStringLiteral("HKEY_LOCAL_MACHINE")}) {
                QSettings app(registry+"\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"+exe,QSettings::NativeFormat);
                const QString path=app.value(".").toString().remove('"');if(QFileInfo(path).isFile())return path;
            }
            for(const auto &root:{qEnvironmentVariable("ProgramFiles"),qEnvironmentVariable("ProgramFiles(x86)"),qEnvironmentVariable("LOCALAPPDATA")+"/Programs"})
                for(const auto &folder:{name(type),type,QStringLiteral("Transmission"),QStringLiteral("BitComet")}) {
                    const QString path=QDir(root).filePath(folder+"/"+exe);if(QFileInfo(path).isFile())return path;
                }
        }
        return {};
    }
};

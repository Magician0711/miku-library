// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Backup.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
// Backup key -> file in the data folder.
const QList<QPair<QString, QString>> kFiles = {
    {QStringLiteral("config"), QStringLiteral("config.json")},
    {QStringLiteral("sites"), QStringLiteral("anime_sites.json")},
    {QStringLiteral("library"), QStringLiteral("local-library.json")},
};

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeFile(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) return true;
    if (error) *error = QStringLiteral("无法写入 %1：%2").arg(QDir::toNativeSeparators(path), file.errorString());
    return false;
}
}

namespace Backup {

QJsonObject create(const QString &dataDir, const QVector<int> &followed)
{
    QJsonObject backup{
        {QStringLiteral("format"), QStringLiteral("miku-library-backup")},
        {QStringLiteral("version"), kVersion},
        {QStringLiteral("app_version"), QCoreApplication::applicationVersion()},
        {QStringLiteral("created"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
    for (const auto &[key, file] : kFiles) {
        const auto object = readObject(QDir(dataDir).filePath(file));
        if (!object.isEmpty()) backup.insert(key, object);
    }
    QJsonArray ids;
    for (const int id : followed) ids.append(id);
    backup.insert(QStringLiteral("followed"), ids);
    return backup;
}

bool save(const QJsonObject &backup, const QString &path, QString *error)
{
    return writeFile(path, QJsonDocument(backup).toJson(QJsonDocument::Indented), error);
}

bool load(const QString &path, QJsonObject *backup, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法打开文件：%1").arg(file.errorString());
        return false;
    }
    QJsonParseError parse{};
    const auto object = QJsonDocument::fromJson(file.readAll(), &parse).object();
    if (parse.error != QJsonParseError::NoError || object.value(QStringLiteral("format")).toString() != QStringLiteral("miku-library-backup")) {
        if (error) *error = QStringLiteral("这不是 Miku Library 的备份文件。");
        return false;
    }
    if (object.value(QStringLiteral("version")).toInt() > kVersion) {
        if (error) *error = QStringLiteral("这份备份来自更新版本的 Miku Library，请先升级软件。");
        return false;
    }
    const auto library = object.value(QStringLiteral("library")).toObject();
    if (!library.isEmpty() && library.value(QStringLiteral("schema_version")).toInt() != 1) {
        if (error) *error = QStringLiteral("备份中的下载记录格式无法识别。");
        return false;
    }
    if (backup) *backup = object;
    return true;
}

QString summary(const QJsonObject &backup)
{
    const auto created = QDateTime::fromString(backup.value(QStringLiteral("created")).toString(), Qt::ISODate).toLocalTime();
    return QStringLiteral("%1 部追番 · %2 条下载记录 · 备份于 %3")
        .arg(backup.value(QStringLiteral("followed")).toArray().size())
        .arg(backup.value(QStringLiteral("library")).toObject().value(QStringLiteral("downloads")).toArray().size())
        .arg(created.isValid() ? created.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("未知时间"));
}

bool restore(const QJsonObject &backup, const QString &dataDir, QVector<int> *followed, QString *error)
{
    QDir().mkpath(dataDir);
    for (const auto &[key, file] : kFiles) {
        if (!backup.contains(key)) continue;   // older or partial backups keep what is there
        const auto data = QJsonDocument(backup.value(key).toObject()).toJson(QJsonDocument::Indented);
        if (!writeFile(QDir(dataDir).filePath(file), data, error)) return false;
    }
    if (followed) {
        followed->clear();
        for (const auto &id : backup.value(QStringLiteral("followed")).toArray())
            if (id.toInt() > 0) followed->append(id.toInt());
    }
    return true;
}

}

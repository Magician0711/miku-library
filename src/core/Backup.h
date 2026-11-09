// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

// One-file backup of what a user would miss after reinstalling: settings, the site
// list, download records with per-anime preferences, and the followed anime.
// Caches (covers, Bangumi metadata, engine state) are rebuilt and left out.
namespace Backup {

constexpr int kVersion = 1;

QJsonObject create(const QString &dataDir, const QVector<int> &followed);
bool save(const QJsonObject &backup, const QString &path, QString *error = nullptr);
// Reads and checks a backup file; nothing is changed.
bool load(const QString &path, QJsonObject *backup, QString *error = nullptr);
// Short description for the confirmation prompt, e.g. "3 部追番 · 12 条下载记录".
QString summary(const QJsonObject &backup);
// Writes the files back into dataDir. Only call while nothing else is using them.
bool restore(const QJsonObject &backup, const QString &dataDir, QVector<int> *followed, QString *error = nullptr);

}

// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "Models.h"

#include <QByteArray>
#include <QString>

qint64 parseSize(const QString &text);
QString rssUrlFor(const SiteEntry &site, const QString &term);
QVector<ResourceResult> parseRss(const QByteArray &data, const SiteEntry &site, QString *error = nullptr);

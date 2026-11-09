// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "Models.h"

#include <QString>

#include <optional>

struct EpisodeInfo {
    QString label;
    bool batch = false;
};

ResourceTags parseTitle(const QString &title);
EpisodeInfo parseEpisode(const QString &title);
std::optional<int> parseSeason(const QString &title);
bool isCjk(const QString &text);
QStringList episodeNumbers(const QString &label);
QStringList fileEpisodeNumbers(const QString &path);
bool isVideoFile(const QString &path);

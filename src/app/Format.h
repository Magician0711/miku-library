// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/Models.h"

#include <QDateTime>
#include <QStringList>
#include <QVariantList>

// Text shown to the user, shared by the controllers.
namespace Format {
QString size(qint64 bytes);
QString speed(qint64 bytesPerSecond);
QString age(const QDateTime &time);
QString eta(qint64 seconds);
// "第 13 话", "第 13–14 话" or "第 13、15、16 话".
QString episodeSummary(const QStringList &episodes);
// Tags of a release as {text, bg, fg, tip}; bg and fg name colours in T.qml.
QVariantList qualityChips(const ResourceTags &tags);
}

namespace Notes {
// Version of the usage declaration; bump it when the wording changes materially
// so every user is asked to accept the new text once.
constexpr int kDisclaimerVersion = 1;
QStringList disclaimerItems();
QString playerNote();
QString freeSoftwareNote();
}

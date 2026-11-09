// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>

class QWindow;

// The system file pickers. Each returns an empty string when dismissed.
namespace NativeDialogs {
QString chooseDirectory(QWindow *parent, const QString &title, const QString &start);
QString openExecutable(QWindow *parent, const QString &start);
QString openJson(QWindow *parent, const QString &title);
QString saveJson(QWindow *parent, const QString &title, const QString &fileName);
// A message for the moments before the app's window exists.
void alert(const QString &title, const QString &text, bool error = true);
}

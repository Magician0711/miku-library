// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

// macOS and Linux: the system pickers through Qt, which uses the native panel on macOS
// and the desktop's portal or GTK dialog on Linux. The Windows version is NativeDialogs.cpp.

#include "NativeDialogs.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

namespace {
// The nearest folder that exists, so a stale setting still opens somewhere useful.
QString existingFolder(const QString &start)
{
    if (start.isEmpty())
        return QDir::homePath();
    QFileInfo info(start);
    QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    while (!QFileInfo(folder).isDir() && QFileInfo(folder).absolutePath() != folder)
        folder = QFileInfo(folder).absolutePath();
    return folder;
}

QString jsonFilter()
{
    return QCoreApplication::translate("Miku", "JSON 配置文件 (*.json)");
}
}

namespace NativeDialogs {
QString chooseDirectory(QWindow *, const QString &title, const QString &start)
{
    return QFileDialog::getExistingDirectory(nullptr, title, existingFolder(start));
}

QString openExecutable(QWindow *, const QString &start)
{
    QString path = QFileDialog::getOpenFileName(nullptr, QCoreApplication::translate("Miku", "选择下载器程序"),
                                                existingFolder(start));
#ifdef Q_OS_MACOS
    // An application bundle stands for the program inside it.
    if (path.endsWith(QStringLiteral(".app")) && QFileInfo(path).isDir()) {
        const QSettings info(path + QStringLiteral("/Contents/Info.plist"), QSettings::NativeFormat);
        const QString name = info.value(QStringLiteral("CFBundleExecutable"), QFileInfo(path).completeBaseName()).toString();
        path += QStringLiteral("/Contents/MacOS/") + name;
    }
#endif
    return path;
}

QString openJson(QWindow *, const QString &title)
{
    return QFileDialog::getOpenFileName(nullptr, title, QDir::homePath(), jsonFilter());
}

QString saveJson(QWindow *, const QString &title, const QString &fileName)
{
    const QFileInfo info(fileName);
    const QString start = info.isAbsolute() ? fileName : QDir::home().filePath(info.fileName());
    QString path = QFileDialog::getSaveFileName(nullptr, title, start, jsonFilter());
    if (!path.isEmpty() && QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".json");
    return path;
}

void alert(const QString &title, const QString &text, bool error)
{
    if (error)
        QMessageBox::critical(nullptr, title, text);
    else
        QMessageBox::warning(nullptr, title, text);
}
}

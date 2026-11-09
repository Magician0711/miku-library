// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "StartupFlow.h"

#include "Format.h"
#include "NativeDialogs.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>

#include <memory>

StartupFlow::StartupFlow(const QString &mode, const QString &downloadDir, const QString &reason, QObject *parent)
    : QObject(parent), m_mode(mode), m_downloadDir(downloadDir), m_reason(reason)
{
}

QStringList StartupFlow::disclaimer() const { return Notes::disclaimerItems(); }
QString StartupFlow::playerNote() const { return Notes::playerNote(); }
QString StartupFlow::freeNote() const { return Notes::freeSoftwareNote(); }

bool StartupFlow::run(QQmlApplicationEngine *engine)
{
    // The window lives exactly as long as this call. Closing it instead would count as
    // declining (the close button does), and overwrite the answer just given.
    QQmlContext context(engine->rootContext());
    context.setContextProperty(QStringLiteral("Startup"), this);
    QQmlComponent component(engine, QStringLiteral("MikuLibrary"), QStringLiteral("StartupWindow"));
    const std::unique_ptr<QObject> window(component.create(&context));
    if (!window) {
        qWarning() << component.errors();
        return false;
    }
    m_window = qobject_cast<QWindow *>(window.get());
    QEventLoop loop;
    connect(this, &StartupFlow::finished, &loop, &QEventLoop::quit);
    loop.exec();
    m_window = nullptr;
    return m_accepted;
}

void StartupFlow::chooseDirectory()
{
    // The default folder may not exist yet; the picker starts from its nearest parent.
    QString from = m_downloadDir;
    if (m_mode == QLatin1String("directory")) {
        from = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (!QFileInfo(from).isDir())
            from = QDir::homePath();
    }
    const QString chosen = NativeDialogs::chooseDirectory(m_window, QStringLiteral("选择下载目录"), from);
    if (chosen.isEmpty())
        return;
    m_downloadDir = chosen;
    emit downloadDirChanged();
    if (m_mode == QLatin1String("directory"))
        finish(true);
}

void StartupFlow::finish(bool accepted)
{
    m_accepted = accepted;
    emit finished();
}

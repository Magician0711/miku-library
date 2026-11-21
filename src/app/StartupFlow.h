// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QStringList>

class QQmlApplicationEngine;
class QWindow;
class ThemeController;

// What must be answered before the main window can open: the welcome guide on first
// launch, and a replacement when the saved download folder is gone.
class StartupFlow final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode CONSTANT)   // "guide" or "directory"
    Q_PROPERTY(QString downloadDir READ downloadDir NOTIFY downloadDirChanged)
    Q_PROPERTY(QString reason READ reason NOTIFY textsChanged)
    Q_PROPERTY(QStringList disclaimer READ disclaimer NOTIFY textsChanged)
    Q_PROPERTY(QString playerNote READ playerNote NOTIFY textsChanged)
    Q_PROPERTY(QString freeNote READ freeNote NOTIFY textsChanged)
public:
    StartupFlow(const QString &mode, const QString &downloadDir, const QString &reason = {}, QObject *parent = nullptr);

    QString mode() const { return m_mode; }
    QString downloadDir() const { return m_downloadDir; }
    QString reason() const { return m_reason; }
    QStringList disclaimer() const;
    QString playerNote() const;
    QString freeNote() const;

    // Shows the window and returns once it is answered; true when accepted.
    bool run(QQmlApplicationEngine *engine);

    Q_INVOKABLE void chooseDirectory();
    Q_INVOKABLE void finish(bool accepted);

signals:
    // The guide's language picker changed the language.
    void textsChanged();
    void downloadDirChanged();
    void finished();
    void directoryRejected();

private:
    QString m_mode, m_downloadDir, m_reason;
    QWindow *m_window = nullptr;
    bool m_accepted = false;
};

// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/Models.h"
#include "MapListModel.h"

#include <QObject>
#include <QSet>

class AnimeDownloads;
class DialogHub;
class DownloaderRpc;
class LocalLibrary;
class QTimer;
class TorrentEngine;
struct AppConfig;

// The task list. Rows are keyed by task, so a progress tick updates cells in place.
class DownloadsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(MapListModel *model READ model CONSTANT)
    Q_PROPERTY(QString summary READ summary NOTIFY statsChanged)
    Q_PROPERTY(QString engineVersion READ engineVersion CONSTANT)
    Q_PROPERTY(QString activeCount READ activeCount NOTIFY statsChanged)
    Q_PROPERTY(QString downSpeed READ downSpeed NOTIFY statsChanged)
    Q_PROPERTY(QString upSpeed READ upSpeed NOTIFY statsChanged)
    Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool allSelected READ allSelected NOTIFY selectionChanged)
    Q_PROPERTY(bool canControl READ canControl NOTIFY selectionChanged)
    Q_PROPERTY(bool canOpenClient READ canOpenClient NOTIFY selectionChanged)
    Q_PROPERTY(QString removeText READ removeText NOTIFY selectionChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY selectionChanged)
    // Set while the page is on screen; the list only refreshes then.
    Q_PROPERTY(bool active READ active WRITE setActive)
public:
    DownloadsController(TorrentEngine *engine, LocalLibrary *library, AppConfig *config, DownloaderRpc *rpc, DialogHub *dialogs,
                        QObject *parent = nullptr);

    MapListModel *model() const { return m_model; }
    QString summary() const { return m_summary; }
    QString engineVersion() const;
    QString activeCount() const { return m_activeCount; }
    QString downSpeed() const { return m_downSpeed; }
    QString upSpeed() const { return m_upSpeed; }
    QString backend() const;
    int selectedCount() const { return m_selected.size(); }
    bool allSelected() const;
    bool canControl() const;
    bool canOpenClient() const;
    QString removeText() const;
    QString hint() const;
    bool active() const { return m_active; }
    void setActive(bool active);

    Q_INVOKABLE void refresh();
    // Plain click, Ctrl-click, Shift-click, and the checkbox column.
    Q_INVOKABLE void selectOnly(const QString &key);
    Q_INVOKABLE void toggle(const QString &key);
    Q_INVOKABLE void extendTo(const QString &key);
    Q_INVOKABLE void toggleAll();
    // A right click outside the selection acts on that row alone.
    Q_INVOKABLE void prepareMenu(const QString &key);

    Q_INVOKABLE void pauseSelected() { control(QStringLiteral("pause")); }
    Q_INVOKABLE void resumeSelected() { control(QStringLiteral("resume")); }
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void openClient();
    Q_INVOKABLE void openFolder(const QString &path);
    Q_INVOKABLE bool folderExists(const QString &path) const;
    Q_INVOKABLE void copyTitle(const QString &title);

    void updateBackend() { emit backendChanged(); }

signals:
    void statsChanged();
    void backendChanged();
    void selectionChanged();

private:
    void control(const QString &action);
    void markSelection();
    void removeRecords(const QVector<struct LibraryItem> &records);

    TorrentEngine *m_engine;
    LocalLibrary *m_library;
    AppConfig *m_config;
    DownloaderRpc *m_rpc;
    DialogHub *m_dialogs;
    MapListModel *m_model;
    QTimer *m_timer;
    QSet<QString> m_selected;
    QString m_anchor;
    QString m_summary, m_activeCount, m_downSpeed, m_upSpeed;
    bool m_active = false;
};

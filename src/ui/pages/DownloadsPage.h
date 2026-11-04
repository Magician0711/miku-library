// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/Models.h"

#include <QHideEvent>
#include <QShowEvent>
#include <QWidget>

class TorrentEngine;
class LocalLibrary;
struct AppConfig;
class QTableWidget;
class QLabel;
class QTimer;
class EmptyState;
class QPushButton;
class DownloaderRpc;

class DownloadsPage : public QWidget {
    Q_OBJECT
public:
    explicit DownloadsPage(TorrentEngine *engine,LocalLibrary *library,AppConfig *config,QWidget *parent = nullptr);
    void updateDownloader();
    void setRpc(DownloaderRpc *rpc);

signals:
    void openDirectoryRequested();
    void switchDownloaderRequested(QWidget *anchor);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refresh();
    void pauseSelected();
    void resumeSelected();
    void deleteSelected();
    void updateActions();
    void showContextMenu(const QPoint &position);
    void openExternalClient();
    QString selectedHash() const;
    QStringList selectedHashes() const;
    void control(const QString &action);

    LocalLibrary *m_library;
    AppConfig *m_config;
    QPushButton *m_backendButton;
    QPushButton *m_openClient;
    TorrentEngine *m_engine;
    QTableWidget *m_table;
    QLabel *m_status;
    QTimer *m_timer;
    QLabel *m_summary;
    QLabel *m_taskCount;
    QLabel *m_downSpeed;
    QLabel *m_upSpeed;
    EmptyState *m_empty;
    QVector<QPushButton *> m_actions;
    DownloaderRpc *m_rpc=nullptr;
};

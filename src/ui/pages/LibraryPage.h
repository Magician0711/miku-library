// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/Models.h"

#include <QSet>
#include <QShowEvent>
#include <QWidget>

class LocalLibrary;
class Database;
class BangumiClient;
class ImageLoader;
class CardGrid;
class EmptyState;
class QButtonGroup;
class QLabel;
class QPushButton;
class QTimer;

class LibraryPage : public QWidget {
    Q_OBJECT
public:
    LibraryPage(Database *db, BangumiClient *bangumi, ImageLoader *images, LocalLibrary *library, QWidget *parent = nullptr);

    // Reflects a running update check on the 检查追更 button; result is shown once done.
    void setUpdateCheck(bool running, const QString &result = {});

signals:
    void animeSelected(AnimeSubject subject);
    void checkUpdatesRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void refresh();

    LocalLibrary *m_library;
    Database *m_db;
    BangumiClient *m_bangumi;
    ImageLoader *m_images;
    CardGrid *m_grid;
    QWidget *m_results;
    EmptyState *m_empty;
    QLabel *m_count;
    QButtonGroup *m_scope;
    QPushButton *m_checkUpdates;
    QTimer *m_refreshTimer;
    QSet<int> m_fetching;
};

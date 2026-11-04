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
class QTimer;

class LibraryPage : public QWidget {
    Q_OBJECT
public:
    LibraryPage(Database *db, BangumiClient *bangumi, ImageLoader *images, LocalLibrary *library, QWidget *parent = nullptr);

signals:
    void animeSelected(AnimeSubject subject);

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
    QTimer *m_refreshTimer;
    QSet<int> m_fetching;
};

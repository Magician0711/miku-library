#pragma once

#include "core/Models.h"

#include <QHash>
#include <QShowEvent>
#include <QWidget>

class BangumiClient;
class ImageLoader;
class CardGrid;
class EmptyState;
class QButtonGroup;
class QLabel;

class HomePage : public QWidget {
    Q_OBJECT
public:
    HomePage(BangumiClient *bangumi, ImageLoader *images, QWidget *parent = nullptr);

signals:
    void animeSelected(AnimeSubject subject);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void load();
    void showDay(int weekday);
    void selectToday();

    BangumiClient *m_bangumi;
    ImageLoader *m_images;
    CardGrid *m_grid;
    EmptyState *m_empty;
    QLabel *m_status;
    QButtonGroup *m_tabs;
    QHash<int, QVector<AnimeSubject>> m_calendar;
    bool m_loaded = false;
    int m_generation = 0;
};

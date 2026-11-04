#pragma once

#include "core/Models.h"

#include <QWidget>

class BangumiClient;
class ImageLoader;
class CardGrid;
class EmptyState;
class QLabel;
class SearchInput;
class QFrame;

class SearchPage : public QWidget {
    Q_OBJECT
public:
    SearchPage(BangumiClient *bangumi, ImageLoader *images, QWidget *parent = nullptr);

    void setQuery(const QString &query);
    void focusInput();

signals:
    void animeSelected(AnimeSubject subject);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void search();
    void resetSearch();

    BangumiClient *m_bangumi;
    ImageLoader *m_images;
    SearchInput *m_edit;
    QFrame *m_panel;
    QLabel *m_status;
    CardGrid *m_grid;
    QWidget *m_results;
    EmptyState *m_empty;
    int m_generation = 0;
};

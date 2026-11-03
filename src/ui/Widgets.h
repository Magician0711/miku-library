#pragma once

#include "core/Models.h"

#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QWidget>

#include <functional>

class ImageLoader;
class FlowLayout;
class QEnterEvent;
class QResizeEvent;
class QKeyEvent;
class QTableWidget;
class QHBoxLayout;

class Chip : public QLabel {
public:
    Chip(const QString &text, const QString &background, const QString &foreground,
         const QString &tooltip = QString(), QWidget *parent = nullptr);
};

QVector<Chip *> qualityChips(const ResourceTags &tags);

class StatusDot : public QWidget {
public:
    explicit StatusDot(const QString &state = QStringLiteral("busy"), QWidget *parent = nullptr);
    void setState(const QString &state);
    QString state() const { return m_state; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_state;
};

QPixmap coverPixmap(const QImage &image, int width, int height, int radius = 10);

class AnimeCard : public QWidget {
    Q_OBJECT
public:
    AnimeCard(ImageLoader *images, QWidget *parent = nullptr);
    void setSubject(const AnimeSubject &subject);
    const AnimeSubject &subject() const { return m_subject; }
    void setCardWidth(int width);

signals:
    void clicked(AnimeSubject subject);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateCardLayout();
    ImageLoader *m_images;
    AnimeSubject m_subject;
    QLabel *m_poster;
    QLabel *m_title;
    QLabel *m_subtitle;
    QImage m_cover;
    bool m_keyboardFocus = false;
};

class CardGrid : public QWidget {
    Q_OBJECT
public:
    explicit CardGrid(ImageLoader *images, QWidget *parent = nullptr);
    void setSubjects(const QVector<AnimeSubject> &subjects);
    void clear();

signals:
    void cardClicked(AnimeSubject subject);

private:
    void resizeEvent(QResizeEvent *event) override;
    void updateCardWidths();
    ImageLoader *m_images;
    FlowLayout *m_layout;
};

class EmptyState : public QWidget {
    Q_OBJECT
public:
    explicit EmptyState(const QString &icon, const QString &title = QString(),
                        const QString &description = QString(), QWidget *parent = nullptr);
    void setText(const QString &title, const QString &description = QString());

private:
    QLabel *m_icon;
    QLabel *m_title;
    QLabel *m_description;
};

QFrame *divider(QWidget *parent = nullptr);
QLabel *label(const QString &text, const QString &objectName = QString(),
              bool wrap = false, QWidget *parent = nullptr);
QString fmtSize(qint64 bytes);
QString fmtAge(const QDateTime &time);
QWidget *pageHeading(const QString &title, const QString &description);
QLabel *metricCard(QHBoxLayout *row, const QString &caption);
void prepareTable(QTableWidget *table);

#pragma once

#include "core/Models.h"
#include "Theme.h"

#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QWidget>

class ImageLoader;
class FlowLayout;
class QEnterEvent;
class QResizeEvent;
class QKeyEvent;
class QTableWidget;
class QHBoxLayout;
class QVBoxLayout;
class QScrollArea;
class QPushButton;
class QVariantAnimation;

// Small rounded tag. Colours are palette roles, resolved while painting.
class Chip : public QLabel {
public:
    Chip(const QString &text, Theme::Role background, Theme::Role foreground,
         const QString &tooltip = QString(), QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Theme::Role m_background;
    Theme::Role m_foreground;
};

// A label that paints a theme icon centred over its stylesheet background.
class IconLabel : public QLabel {
public:
    IconLabel(const QString &icon, Theme::Role role, int iconSize, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QIcon m_icon;
    int m_iconSize;
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
    void updateCover();
    ImageLoader *m_images;
    AnimeSubject m_subject;
    QLabel *m_poster;
    QLabel *m_rating;
    QLabel *m_title;
    QLabel *m_subtitle;
    QImage m_cover;
    bool m_keyboardFocus = false;
    void animateHover(bool hover);
    QVariantAnimation *m_hoverAnimation = nullptr;
    qreal m_hoverLevel = 0;   // 0..1, eased in and out
};

class CardGrid : public QWidget {
    Q_OBJECT
public:
    explicit CardGrid(ImageLoader *images, QWidget *parent = nullptr);
    // Reuses existing cards, so switching between lists does not rebuild every widget.
    void setSubjects(const QVector<AnimeSubject> &subjects);
    void clear();

signals:
    void cardClicked(AnimeSubject subject);

private:
    void resizeEvent(QResizeEvent *event) override;
    void updateCardWidths();
    ImageLoader *m_images;
    FlowLayout *m_layout;
    int m_cardWidth = 0;
};

class EmptyState : public QWidget {
    Q_OBJECT
public:
    // icon is a Theme::icon name such as "search" or "download".
    explicit EmptyState(const QString &icon, const QString &title = QString(),
                        const QString &description = QString(), QWidget *parent = nullptr);
    void setText(const QString &title, const QString &description = QString());

private:
    QLabel *m_icon;
    QLabel *m_title;
    QLabel *m_description;
};

// Shared page skeleton: title with inline meta text on the left, actions on the right.
struct PageHeader {
    QWidget *widget = nullptr;
    QLabel *meta = nullptr;
    QHBoxLayout *actions = nullptr;
};
PageHeader pageHeader(const QString &title, QWidget *parent = nullptr);
QVBoxLayout *pageLayout(QWidget *page);
QScrollArea *scrollArea(QWidget *content, QWidget *parent = nullptr);
// Places an empty state over the given content; only one is visible at a time.
QWidget *overlayStack(QWidget *content, EmptyState *empty, QWidget *parent = nullptr);
QPushButton *iconButton(const QString &icon, const QString &tip, QWidget *parent = nullptr, int size = 32);

QFrame *divider(QWidget *parent = nullptr);
QLabel *label(const QString &text, const QString &objectName = QString(),
              bool wrap = false, QWidget *parent = nullptr);
QString fmtSize(qint64 bytes);
QString fmtSpeed(qint64 bytesPerSecond);
QString fmtAge(const QDateTime &time);
// Compact statistic for summary strips: caption above a value.
QLabel *statTile(QHBoxLayout *row, const QString &icon, const QString &caption);
void prepareTable(QTableWidget *table);

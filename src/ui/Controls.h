#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include "core/DownloaderSettings.h"

class QTimer;
class QVariantAnimation;

// One popup implementation for resource filters and engine preferences.
class SelectBox : public QComboBox {
public:
    explicit SelectBox(QWidget *parent = nullptr);
    void showPopup() override;
};

class SoftFocusDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

class SearchInput : public QLineEdit {
    Q_OBJECT
public:
    explicit SearchInput(QWidget *parent = nullptr);
    bool composing() const { return m_composing; }

signals:
    void compositionChanged(bool composing);

protected:
    bool event(QEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void guardCandidateConfirmation();
    bool m_composing = false;
    bool m_confirmingCandidate = false;
    QTimer *m_candidateRelease = nullptr;
};

// A QCheckBox drawn as an on/off switch; it keeps QCheckBox semantics and signals.
class ToggleSwitch : public QCheckBox {
public:
    explicit ToggleSwitch(const QString &text = QString(), QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    bool hitButton(const QPoint &position) const override;
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int kTrackWidth = 38;
    static constexpr int kTrackHeight = 22;
    QVariantAnimation *m_animation;
    qreal m_position = 0;
};

namespace Ui {
// Returns a downloader type, "configure", or an empty string on dismissal.
QString chooseDownloader(QWidget *anchor,const DownloaderSettings &settings);
}

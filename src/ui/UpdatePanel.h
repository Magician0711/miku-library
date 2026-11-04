#pragma once

#include "net/UpdateChecker.h"

#include <QFrame>

class QLabel;
class QPushButton;
class QVBoxLayout;

// Persistent notice in the bottom-right of the content area listing anime whose
// followed release group has new episodes. Rows stay until handled or dismissed.
class UpdatePanel : public QFrame {
    Q_OBJECT
public:
    explicit UpdatePanel(QWidget *canvas);
    void addNotice(const UpdateNotice &notice);

signals:
    void downloadRequested(UpdateNotice notice);
    void ignoreRequested(UpdateNotice notice);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void removeRow(QWidget *row);
    void refresh();
    void place();

    QLabel *m_count;
    QVBoxLayout *m_rows;
    QPushButton *m_all;
    QVector<QPair<QWidget *, UpdateNotice>> m_notices;
};

// "第 13 话", "第 13–14 话" or "第 13、15、16 话".
QString episodeSummary(const QStringList &episodes);

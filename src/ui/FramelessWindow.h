#pragma once

#include <QMainWindow>

class QLabel;
class QToolButton;

// Owns the application chrome; pages and the download engine remain independent.
class FramelessWindow : public QMainWindow {
public:
    explicit FramelessWindow(QWidget *parent = nullptr);
    QWidget *titleBar() const { return m_titleBar; }
    void setContextTitle(const QString &title);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;

private:
    void toggleMaximized();
    void updateControls();
    bool isDragArea(const QPoint &position) const;
    QWidget *m_titleBar;
    QLabel *m_context;
    QToolButton *m_maximize;
};

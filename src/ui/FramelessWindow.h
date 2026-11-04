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
    // Places a control in the title bar, just left of the window buttons.
    void addTitleWidget(QWidget *widget);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;

private:
    void toggleMaximized();
    void applyThemeToChrome();
    void updateControls();
    void reveal();
    bool isDragArea(const QPoint &position) const;
    QWidget *m_titleBar;
    QLabel *m_context;
    QToolButton *m_minimize;
    QToolButton *m_maximize;
    QToolButton *m_close;
    bool m_cloaked = false;
    bool m_revealed = false;
};

#pragma once

#include <QWidget>

class Catalog;
class HttpClient;
class QTableWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class SelectBox;

class SitesPage : public QWidget {
    Q_OBJECT
public:
    SitesPage(Catalog *catalog, HttpClient *http, QWidget *parent = nullptr);

    void refresh();

signals:
    void importRequested();
    void exportRequested();

private:
    void filterRows();
    void testSelected();
    int selectedRow() const;

    Catalog *m_catalog;
    HttpClient *m_http;
    QLineEdit *m_filter;
    SelectBox *m_scope;
    QPushButton *m_test;
    QTableWidget *m_table;
    QLabel *m_status;
};

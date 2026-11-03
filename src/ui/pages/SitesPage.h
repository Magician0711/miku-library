#pragma once

#include <QWidget>

class Catalog;
class HttpClient;
class QTableWidget;
class QLabel;
class QLineEdit;

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
    QLineEdit *m_filter;
    void toggleSelected();
    void testSelected();
    int selectedRow() const;

    Catalog *m_catalog;
    HttpClient *m_http;
    QTableWidget *m_table;
    QLabel *m_status;
};

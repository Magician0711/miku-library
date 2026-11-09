// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "MapListModel.h"

#include <QObject>
#include <QSet>

class Catalog;
class DialogHub;
class HttpClient;

class SitesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(MapListModel *model READ model CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int scope READ scope WRITE setScope NOTIFY filterChanged)
    Q_PROPERTY(QString selected READ selected WRITE setSelected NOTIFY selectedChanged)
public:
    SitesController(Catalog *catalog, HttpClient *http, DialogHub *dialogs, QObject *parent = nullptr);

    MapListModel *model() const { return m_model; }
    QString status() const { return m_status; }
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    int scope() const { return m_scope; }
    void setScope(int scope);
    QString selected() const { return m_selected; }
    void setSelected(const QString &id);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void testSelected();

signals:
    void statusChanged();
    void filterChanged();
    void selectedChanged();
    void importRequested();
    void exportRequested();

private:
    Catalog *m_catalog;
    HttpClient *m_http;
    DialogHub *m_dialogs;
    MapListModel *m_model;
    QString m_status, m_filter, m_selected;
    QSet<QString> m_testing;
    int m_scope = 0;
};

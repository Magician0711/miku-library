// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

// A list of named values for QML views. update() applies a new list as the smallest
// change it can find, so rows that stay keep their delegates: a new batch of results
// or a progress tick never rebuilds what is on screen.
class MapListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    // Every row must carry keyRole, and each row's key must be unique.
    MapListModel(const QStringList &roles, const QString &keyRole, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_rows.size(); }
    const QVector<QVariantMap> &rows() const { return m_rows; }
    void update(const QVector<QVariantMap> &rows);
    Q_INVOKABLE QVariantMap get(int row) const { return m_rows.value(row); }

signals:
    void countChanged();

private:
    QStringList m_roles;
    QString m_key;
    QVector<QVariantMap> m_rows;
};

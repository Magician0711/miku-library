// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "MapListModel.h"

MapListModel::MapListModel(const QStringList &roles, const QString &keyRole, QObject *parent)
    : QAbstractListModel(parent), m_roles(roles), m_key(keyRole)
{
}

int MapListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant MapListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    return m_rows.at(index.row()).value(m_roles.value(role - Qt::UserRole));
}

QHash<int, QByteArray> MapListModel::roleNames() const
{
    QHash<int, QByteArray> names;
    for (int i = 0; i < m_roles.size(); ++i)
        names.insert(Qt::UserRole + i, m_roles.at(i).toUtf8());
    return names;
}

void MapListModel::update(const QVector<QVariantMap> &rows)
{
    const int before = m_rows.size();
    auto sameKey = [this](const QVariantMap &a, const QVariantMap &b) { return a.value(m_key) == b.value(m_key); };

    // The rows that differ sit between an unchanged start and an unchanged end.
    int head = 0;
    while (head < before && head < rows.size() && sameKey(m_rows.at(head), rows.at(head)))
        ++head;
    int tail = 0;
    while (tail < before - head && tail < rows.size() - head
           && sameKey(m_rows.at(before - 1 - tail), rows.at(rows.size() - 1 - tail)))
        ++tail;

    auto refresh = [&](int from, int to, int offset) {
        for (int row = from; row < to; ++row) {
            const auto &fresh = rows.at(row + offset);
            if (m_rows.at(row) == fresh)
                continue;
            m_rows[row] = fresh;
            const auto where = index(row);
            emit dataChanged(where, where);
        }
    };
    refresh(0, head, 0);
    refresh(before - tail, before, int(rows.size()) - before);

    const int removed = before - head - tail;
    if (removed > 0) {
        beginRemoveRows({}, head, head + removed - 1);
        m_rows.remove(head, removed);
        endRemoveRows();
    }
    const int added = int(rows.size()) - head - tail;
    if (added > 0) {
        beginInsertRows({}, head, head + added - 1);
        m_rows.insert(head, added, {});
        for (int i = 0; i < added; ++i)
            m_rows[head + i] = rows.at(head + i);
        endInsertRows();
    }
    if (before != rows.size())
        emit countChanged();
}

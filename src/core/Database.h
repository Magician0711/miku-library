// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "Models.h"

#include <QSqlDatabase>
#include <QString>

class Database {
public:
    explicit Database(const QString &path);
    ~Database();

    bool init(QString *error = nullptr);
    bool isOpen() const;

    bool upsertSubject(const AnimeSubject &subject);
    std::optional<AnimeSubject> getSubject(int bangumiId) const;

    bool follow(int bangumiId);
    bool unfollow(int bangumiId);
    QVector<int> followedIds() const;
    QVector<AnimeSubject> cachedSubjects() const;
    bool isFollowing(int bangumiId) const;

private:
    QString m_path;
    QSqlDatabase m_db;
    QString m_connectionName;
};

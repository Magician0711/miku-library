// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Database.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace {

QString jsonArray(const QStringList &values)
{
    QJsonArray array;
    for (const auto &value : values)
        array.append(value);
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QStringList stringList(const QVariant &value)
{
    const auto doc = QJsonDocument::fromJson(value.toString().toUtf8());
    QStringList out;
    if (!doc.isArray())
        return out;
    for (const auto &item : doc.array())
        out << item.toString();
    return out;
}

} // namespace

Database::Database(const QString &path)
    : m_path(path)
    , m_connectionName(QStringLiteral("miku_library_%1").arg(reinterpret_cast<quintptr>(this), 0, 16))
{
}

Database::~Database()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::init(QString *error)
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_path);
    if (!m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    const QStringList ddl = {
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS anime_subjects ("
                       "bangumi_id INTEGER PRIMARY KEY, name_cn TEXT, name_jp TEXT, name_en TEXT,"
                       "name_romaji TEXT, aliases_json TEXT NOT NULL DEFAULT '[]', summary TEXT,"
                       "year INTEGER, air_date TEXT, episode_count INTEGER,"
                       "tags_json TEXT NOT NULL DEFAULT '[]', cover_url TEXT, bangumi_url TEXT,"
                       "cached_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS subscriptions ("
                       "bangumi_id INTEGER PRIMARY KEY, added_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS search_results ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, query_key TEXT NOT NULL, title TEXT NOT NULL,"
                       "anime_id TEXT, site_id TEXT NOT NULL, site_name TEXT NOT NULL, release_group TEXT,"
                       "episode TEXT, publish_time TEXT, file_size INTEGER, magnet_uri TEXT, torrent_url TEXT,"
                       "info_hash TEXT, seeders INTEGER, leechers INTEGER, tags_json TEXT NOT NULL DEFAULT '{}',"
                       "source_page TEXT, raw_json TEXT NOT NULL DEFAULT '{}', created_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS download_tasks ("
                       "task_id INTEGER PRIMARY KEY AUTOINCREMENT, info_hash TEXT, magnet_uri TEXT,"
                       "torrent_url TEXT, title TEXT NOT NULL, save_path TEXT NOT NULL,"
                       "status TEXT NOT NULL DEFAULT 'pending', progress REAL NOT NULL DEFAULT 0,"
                       "dl_speed INTEGER NOT NULL DEFAULT 0, up_speed INTEGER NOT NULL DEFAULT 0,"
                       "downloaded INTEGER NOT NULL DEFAULT 0, size INTEGER, seeders INTEGER NOT NULL DEFAULT 0,"
                       "leechers INTEGER NOT NULL DEFAULT 0, eta INTEGER, qbt_hash TEXT, anime_id TEXT,"
                       "site_id TEXT, created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS site_overrides ("
                       "site_id TEXT PRIMARY KEY, enabled INTEGER NOT NULL DEFAULT 1,"
                       "priority INTEGER NOT NULL DEFAULT 50, adapter TEXT NOT NULL DEFAULT 'none',"
                       "indexer_id TEXT, search_url TEXT, rss_url TEXT, timeout REAL NOT NULL DEFAULT 20.0,"
                       "rate_limit REAL NOT NULL DEFAULT 0.0, proxy_profile TEXT, updated_at TEXT NOT NULL)"),
    };
    for (const auto &statement : ddl) {
        if (!query.exec(statement)) {
            if (error)
                *error = query.lastError().text();
            return false;
        }
    }

    // The Python version did not persist these fields. Add them when opening
    // an existing database so C++ and Python can share the same file.
    const QStringList extraColumns = {
        QStringLiteral("rating REAL"),
        QStringLiteral("rank INTEGER"),
        QStringLiteral("platform TEXT"),
        QStringLiteral("air_weekday INTEGER"),
        QStringLiteral("studio TEXT"),
    };
    QSet<QString> existing;
    if (query.exec(QStringLiteral("PRAGMA table_info(anime_subjects)"))) {
        while (query.next())
            existing.insert(query.value(1).toString());
    }
    for (const auto &column : extraColumns) {
        const QString name = column.section(QLatin1Char(' '), 0, 0);
        if (!existing.contains(name))
            query.exec(QStringLiteral("ALTER TABLE anime_subjects ADD COLUMN ") + column);
    }
    return true;
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

bool Database::upsertSubject(const AnimeSubject &subject)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO anime_subjects "
        "(bangumi_id, name_cn, name_jp, name_en, name_romaji, aliases_json, summary, year, air_date,"
        " episode_count, tags_json, cover_url, bangumi_url, cached_at, rating, rank, platform, air_weekday, studio) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(bangumi_id) DO UPDATE SET "
        "name_cn=excluded.name_cn, name_jp=excluded.name_jp, name_en=excluded.name_en,"
        "name_romaji=excluded.name_romaji, aliases_json=excluded.aliases_json, summary=excluded.summary,"
        "year=excluded.year, air_date=excluded.air_date, episode_count=excluded.episode_count,"
        "tags_json=excluded.tags_json, cover_url=excluded.cover_url, bangumi_url=excluded.bangumi_url,"
        "cached_at=excluded.cached_at, rating=excluded.rating, rank=excluded.rank,"
        "platform=excluded.platform, air_weekday=excluded.air_weekday, studio=excluded.studio"));
    query.addBindValue(subject.bangumiId);
    query.addBindValue(subject.nameCn);
    query.addBindValue(subject.nameJp);
    query.addBindValue(subject.nameEn);
    query.addBindValue(subject.nameRomaji);
    query.addBindValue(jsonArray(subject.aliases));
    query.addBindValue(subject.summary);
    query.addBindValue(subject.year > 0 ? QVariant(subject.year) : QVariant());
    query.addBindValue(subject.airDate);
    query.addBindValue(subject.episodeCount > 0 ? QVariant(subject.episodeCount) : QVariant());
    query.addBindValue(jsonArray(subject.tags));
    query.addBindValue(subject.coverUrl);
    query.addBindValue(subject.bangumiUrl);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    query.addBindValue(subject.rating > 0 ? QVariant(subject.rating) : QVariant());
    query.addBindValue(subject.rank > 0 ? QVariant(subject.rank) : QVariant());
    query.addBindValue(subject.platform);
    query.addBindValue(subject.airWeekday > 0 ? QVariant(subject.airWeekday) : QVariant());
    query.addBindValue(subject.studio);
    return query.exec();
}

namespace {
// Reads the current row of a "SELECT * FROM anime_subjects" query.
AnimeSubject subjectFromRow(const QSqlQuery &query)
{
    const auto record = query.record();
    auto text = [&](const QString &name) {
        const int idx = record.indexOf(name);
        return idx >= 0 ? query.value(idx).toString() : QString();
    };
    auto number = [&](const QString &name) {
        const int idx = record.indexOf(name);
        return idx >= 0 ? query.value(idx).toInt() : 0;
    };

    AnimeSubject subject;
    subject.bangumiId = query.value(record.indexOf(QStringLiteral("bangumi_id"))).toInt();
    subject.nameCn = text(QStringLiteral("name_cn"));
    subject.nameJp = text(QStringLiteral("name_jp"));
    subject.nameEn = text(QStringLiteral("name_en"));
    subject.nameRomaji = text(QStringLiteral("name_romaji"));
    subject.aliases = stringList(query.value(record.indexOf(QStringLiteral("aliases_json"))));
    subject.summary = text(QStringLiteral("summary"));
    subject.year = number(QStringLiteral("year"));
    subject.airDate = text(QStringLiteral("air_date"));
    subject.episodeCount = number(QStringLiteral("episode_count"));
    subject.tags = stringList(query.value(record.indexOf(QStringLiteral("tags_json"))));
    subject.coverUrl = text(QStringLiteral("cover_url"));
    subject.bangumiUrl = text(QStringLiteral("bangumi_url"));
    subject.rating = query.value(record.indexOf(QStringLiteral("rating"))).toDouble();
    subject.rank = number(QStringLiteral("rank"));
    subject.platform = text(QStringLiteral("platform"));
    subject.airWeekday = number(QStringLiteral("air_weekday"));
    subject.studio = text(QStringLiteral("studio"));
    return subject;
}
} // namespace

std::optional<AnimeSubject> Database::getSubject(int bangumiId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM anime_subjects WHERE bangumi_id=?"));
    query.addBindValue(bangumiId);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return subjectFromRow(query);
}

bool Database::follow(int bangumiId)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO subscriptions (bangumi_id, added_at) VALUES (?,?)"));
    query.addBindValue(bangumiId);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return query.exec();
}

bool Database::unfollow(int bangumiId)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM subscriptions WHERE bangumi_id=?"));
    query.addBindValue(bangumiId);
    return query.exec();
}

QVector<int> Database::followedIds() const
{
    QSqlQuery query(m_db);
    QVector<int> ids;
    if (!query.exec(QStringLiteral("SELECT bangumi_id FROM subscriptions ORDER BY added_at DESC")))
        return ids;
    while (query.next())
        ids.push_back(query.value(0).toInt());
    return ids;
}

bool Database::isFollowing(int bangumiId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT 1 FROM subscriptions WHERE bangumi_id=?"));
    query.addBindValue(bangumiId);
    return query.exec() && query.next();
}

QVector<AnimeSubject> Database::cachedSubjects() const
{
    QVector<AnimeSubject> out;QSqlQuery query(m_db);
    if(query.exec(QStringLiteral("SELECT * FROM anime_subjects")))
        while(query.next())out.append(subjectFromRow(query));
    return out;
}

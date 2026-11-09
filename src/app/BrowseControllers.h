// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/Models.h"
#include "MapListModel.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVariantList>

class BangumiClient;
class Database;
class LocalLibrary;
class QTimer;

// A page of posters: the list, a status line, and an empty state over it.
class BrowseController : public QObject {
    Q_OBJECT
    Q_PROPERTY(MapListModel *model READ model CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusTip READ statusTip NOTIFY statusChanged)
    Q_PROPERTY(QString emptyTitle READ emptyTitle NOTIFY emptyChanged)
    Q_PROPERTY(QString emptyText READ emptyText NOTIFY emptyChanged)
    Q_PROPERTY(bool emptyVisible READ emptyVisible NOTIFY emptyChanged)
public:
    explicit BrowseController(QObject *parent = nullptr);

    MapListModel *model() const { return m_model; }
    QString status() const { return m_status; }
    QString statusTip() const { return m_statusTip; }
    QString emptyTitle() const { return m_emptyTitle; }
    QString emptyText() const { return m_emptyText; }
    bool emptyVisible() const { return m_emptyVisible; }

    Q_INVOKABLE void open(int subjectId);

signals:
    void statusChanged();
    void emptyChanged();
    void animeSelected(const AnimeSubject &subject);

protected:
    void setStatus(const QString &text, const QString &tip = {});
    void setEmpty(const QString &title, const QString &text);
    void hideEmpty();
    void setSubjects(const QVector<AnimeSubject> &subjects);

private:
    MapListModel *m_model;
    QHash<int, AnimeSubject> m_subjects;
    QString m_status, m_statusTip, m_emptyTitle, m_emptyText;
    bool m_emptyVisible = true;
};

class HomeController final : public BrowseController {
    Q_OBJECT
    // Monday is 1. Each entry is {day, name, count}.
    Q_PROPERTY(QVariantList days READ days NOTIFY daysChanged)
    Q_PROPERTY(int day READ day NOTIFY dayChanged)
    // The best rated show of the chosen day: {subjectId, title, meta, summary, rating, cover}.
    Q_PROPERTY(QVariantMap featured READ featured NOTIFY featuredChanged)
public:
    HomeController(BangumiClient *bangumi, QObject *parent = nullptr);

    QVariantList days() const;
    int day() const { return m_day; }
    QVariantMap featured() const { return m_featured; }

    // quiet keeps what is on screen until the new schedule arrives, and on failure.
    Q_INVOKABLE void load(bool quiet = false);
    // Loads the calendar the first time the page is shown.
    Q_INVOKABLE void ensureLoaded() { if (!m_loaded) load(); }
    Q_INVOKABLE void showDay(int weekday);

signals:
    void daysChanged();
    void dayChanged();
    void featuredChanged();
    // The season's best rated shows, once the calendar has loaded.
    void topRatedChanged(const QVector<AnimeSubject> &subjects);

private:
    BangumiClient *m_bangumi;
    QHash<int, QVector<AnimeSubject>> m_calendar;
    QVariantMap m_featured;
    bool m_loaded = false;
    int m_generation = 0;
    int m_day = 0;
};

class SearchController final : public BrowseController {
    Q_OBJECT
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(int sort READ sort WRITE setSort NOTIFY sortChanged)
public:
    enum Sort { Newest, Rating, Relevance };
    SearchController(BangumiClient *bangumi, QObject *parent = nullptr);

    QString query() const { return m_query; }
    int sort() const { return m_sort; }
    void setSort(int sort);

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void reset();
    // Shown while nothing has been searched, so the page opens with something to pick.
    void setSuggestions(const QVector<AnimeSubject> &subjects);

signals:
    void queryChanged();
    void sortChanged();

private:
    void showSorted();

    BangumiClient *m_bangumi;
    QString m_query;
    int m_sort = Newest;
    int m_generation = 0;
    QVector<AnimeSubject> m_found;   // Bangumi's order, i.e. relevance
    QVector<AnimeSubject> m_suggestions;
};

class LibraryController final : public BrowseController {
    Q_OBJECT
    Q_PROPERTY(int scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    // Set while the page is on screen; it only refreshes then.
    Q_PROPERTY(bool active READ active WRITE setActive)
public:
    enum Scope { All, Followed, Local };
    LibraryController(Database *db, BangumiClient *bangumi, LocalLibrary *library, QObject *parent = nullptr);

    int scope() const { return m_scope; }
    void setScope(int scope);
    bool checking() const { return m_checking; }
    void setChecking(bool checking);
    bool active() const { return m_active; }
    void setActive(bool active);
    Q_INVOKABLE void refresh();

signals:
    void scopeChanged();
    void checkingChanged();
    void checkUpdatesRequested();

private:
    Database *m_db;
    BangumiClient *m_bangumi;
    LocalLibrary *m_library;
    QTimer *m_refreshTimer;
    QSet<int> m_fetching;
    int m_scope = All;
    bool m_checking = false;
    bool m_active = false;
};

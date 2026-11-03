#pragma once

#include "Models.h"

#include <QObject>
#include <QString>

class QFileSystemWatcher;

class Catalog : public QObject {
    Q_OBJECT
public:
    explicit Catalog(const QString &path, QObject *parent = nullptr);

    bool load();
    bool save(const QString &path = QString()) const;
    bool setEnabled(const QString &id,bool enabled,QString *error=nullptr);
    bool importFrom(const QString &path,QString *error=nullptr);

    QString path() const { return m_path; }
    void setPath(const QString &path) { m_path = path; }

    const SiteCatalog &catalog() const { return m_catalog; }
    QVector<SiteEntry> sites(bool searchableOnly = false) const;
    SiteEntry *get(const QString &id);
    const SiteEntry *get(const QString &id) const;

    void startWatching();

signals:
    void catalogChanged();

private slots:
    void reloadFromWatcher();

private:
    QString m_path;
    QByteArray m_lastBytes;
    SiteCatalog m_catalog;
    QFileSystemWatcher *m_watcher = nullptr;
};

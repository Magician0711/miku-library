// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/DownloaderSettings.h"
#include "core/EngineSettings.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class DialogHub;
class QWindow;
class TorrentEngine;
class ThemeController;
struct AppConfig;

// The 偏好设置 form. form holds what is on screen; general settings save as they
// change, the rest wait for 应用设置. dirty says whether those differ from what is saved.
class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap form READ form NOTIFY formChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int section READ section WRITE setSection NOTIFY sectionChanged)
    Q_PROPERTY(QVariantList interfaces READ interfaces NOTIFY interfacesChanged)
    Q_PROPERTY(QVariantList downloaderTypes READ downloaderTypes CONSTANT)
    Q_PROPERTY(QString cacheSize READ cacheSize NOTIFY cacheChanged)
    Q_PROPERTY(QString updateStatus READ updateStatus NOTIFY updateStatusChanged)
    Q_PROPERTY(bool checkingUpdate READ checkingUpdate NOTIFY updateStatusChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QStringList disclaimer READ disclaimer CONSTANT)
    Q_PROPERTY(QString playerNote READ playerNote CONSTANT)
    Q_PROPERTY(QString freeNote READ freeNote CONSTANT)
public:
    SettingsController(TorrentEngine *engine, AppConfig *config, ThemeController *theme, DialogHub *dialogs, QObject *parent = nullptr);

    QVariantMap form() const { return m_form; }
    bool dirty() const { return m_dirty; }
    QString status() const { return m_status; }
    int section() const { return m_section; }
    void setSection(int section);
    QVariantList interfaces() const { return m_interfaces; }
    QVariantList downloaderTypes() const;
    QString cacheSize() const { return m_cacheSize; }
    QString updateStatus() const { return m_updateStatus; }
    bool checkingUpdate() const { return m_checkingUpdate; }
    QString version() const;
    QStringList disclaimer() const;
    QString playerNote() const;
    QString freeNote() const;
    void setWindow(QWindow *window) { m_window = window; }

    Q_INVOKABLE void set(const QString &name, const QVariant &value);
    Q_INVOKABLE void load();
    Q_INVOKABLE void apply();
    Q_INVOKABLE void browseSavePath();
    Q_INVOKABLE void browseDownloader();
    Q_INVOKABLE void detectDownloader();
    Q_INVOKABLE void clearCovers();
    Q_INVOKABLE void updateCacheSize();
    // Stopping at completion needs a second confirmation; the switch asks here.
    Q_INVOKABLE void requestStopOnCompletion(bool on);
    Q_INVOKABLE void setAppearance(const QString &mode);
    Q_INVOKABLE void checkAppUpdate();
    Q_INVOKABLE void exportBackup() { emit exportBackupRequested(); }
    Q_INVOKABLE void importBackup() { emit importBackupRequested(); }

    void editDownloader(const QString &type = {});
    void setUpdateResult(const QString &status);
    // Re-reads the 常规 values after the config changed elsewhere (e.g. the close prompt).
    void reloadGeneral();
    // Builds what the form would save; public for tests.
    EngineSettings engineFromForm() const;

signals:
    void formChanged();
    void dirtyChanged();
    void statusChanged();
    void sectionChanged();
    void interfacesChanged();
    void cacheChanged();
    void updateStatusChanged();
    void downloaderChanged();
    // The download folder was changed and saved; existing downloads are still in from.
    void downloadFolderChanged(const QString &from, const QString &to);
    void checkAppUpdateRequested();
    void generalSettingsChanged();
    void proxyChanged();
    void exportBackupRequested();
    void importBackupRequested();

private:
    DownloaderSettings downloaderFromForm() const;
    bool sameProxy() const;
    void updateDirty();
    void saveGeneral();
    void loadDownloaderFields();
    void saveDownloaderDraft();

    TorrentEngine *m_engine;
    AppConfig *m_config;
    ThemeController *m_theme;
    DialogHub *m_dialogs;
    QWindow *m_window = nullptr;
    QVariantMap m_form;
    QVariantList m_interfaces;
    DownloaderSettings m_draft;
    QString m_editType;
    QString m_status, m_cacheSize, m_updateStatus;
    int m_section = 0;
    bool m_dirty = false;
    bool m_checkingUpdate = false;
};

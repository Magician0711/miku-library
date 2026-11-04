#pragma once

#include <QWidget>
#include "core/DownloaderSettings.h"
#include "core/EngineSettings.h"
#include "ui/Theme.h"

class TorrentEngine;
struct AppConfig;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QTabWidget;
class QButtonGroup;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    SettingsPage(TorrentEngine *engine, AppConfig *config, QWidget *parent = nullptr);
    void editDownloader(const QString &type={});
signals:
    void downloaderChanged();
    // Appearance applies immediately; the main window saves it.
    void appearanceRequested(Theme::Mode mode);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void load();
    void apply();
    // What the form would save; shared by apply() and change detection.
    EngineSettings engineFromForm() const;
    DownloaderSettings downloaderFromForm() const;
    // Shows the apply bar only while the form differs from the saved settings.
    void updateDirty();
    void updateCacheSize();
    void updateDownloaderFields();
    void saveDownloaderDraft();
    QTabWidget *m_tabs;
    QWidget *m_saveBar = nullptr;
    QLabel *m_cacheSize = nullptr;
    bool m_dirty = false;
    bool m_loading = false;   // form is being refilled from the saved settings
    QButtonGroup *m_appearance;
    QWidget *m_downloaderTab;
    QComboBox *m_downloaderType;
    QLineEdit *m_downloaderPath;
    QLineEdit *m_customArguments;
    QLabel *m_downloaderHelp;
    QWidget *m_programCard;
    QWidget *m_argumentsCard;
    QLabel *m_externalNote;
    DownloaderSettings m_downloaderDraft;
    QString m_editType;
    QWidget *m_rpcCard;QCheckBox *m_rpcEnabled;QLineEdit *m_rpcUrl,*m_rpcUser,*m_rpcPassword;

    TorrentEngine *m_engine;
    AppConfig *m_config;
    QLineEdit *m_savePath;
    QComboBox *m_interface;
    QSpinBox *m_port;
    QCheckBox *m_dht;
    QCheckBox *m_lsd;
    QCheckBox *m_upnp;
    QCheckBox *m_publicTrackerFallback;
    QComboBox *m_encryption;
    QSpinBox *m_downloadLimit;
    QSpinBox *m_uploadLimit;
    QSpinBox *m_maxConnections;
    QSpinBox *m_maxUploads;
    QCheckBox *m_queueing;
    QCheckBox *m_stopOnCompletion;
    QSpinBox *m_activeDownloads;
    QSpinBox *m_activeSeeds;
    QSpinBox *m_activeLimit;
    QSpinBox *m_sourceStall;
    QLabel *m_status;
};

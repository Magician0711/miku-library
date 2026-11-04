// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "core/LocalLibrary.h"
#include "core/DownloaderSettings.h"
#include <QObject>
#include <functional>

class QNetworkAccessManager;
struct ExternalCommand { QString program;QStringList arguments;QString directory;bool console=false; };
struct ExternalResult { QString hash;QString torrentFile;QVector<LibraryFile> files;QStringList episodes;bool retryable=false;QString remoteId;QStringList selectedIndexes;int fileCount=0; };
class ExternalDownloader : public QObject {
    Q_OBJECT
public:
    using Launcher=std::function<bool(const ExternalCommand &,QString *)>;
    using Callback=std::function<void(bool,QString,ExternalResult)>;
    ExternalDownloader(LocalLibrary *library,QObject *parent=nullptr,Launcher launcher={},bool prepareOnly=false);
    void send(const LibraryItem &item,bool stopOnCompletion,Callback callback);
    static bool validate(const DownloaderSettings &settings,QString *error=nullptr);
    static ExternalCommand command(const LibraryItem &item,const QString &input,const QStringList &selectedIndexes,
                                   bool stopOnCompletion,QString *error=nullptr);
    static QVector<LibraryFile> verifyFiles(const LibraryItem &item);
    void checkLocalFiles();
    static bool start(const ExternalCommand &command,QString *error=nullptr);
private:
    void launch(const LibraryItem &item,const QString &input,bool stopOnCompletion,Callback callback);
    LocalLibrary *m_library;
    QNetworkAccessManager *m_net;
    Launcher m_launcher;
    QSet<QString> m_sentHashes;
    QHash<QString,QString> m_checkedFiles;
    bool m_verifying=false;
    bool m_prepareOnly=false;
};

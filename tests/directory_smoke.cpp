// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "core/AppConfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdlib>

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir sandbox;
    QFile report(QCoreApplication::applicationDirPath()+"/directory-smoke.log");
    if(!sandbox.isValid()||!report.open(QIODevice::WriteOnly|QIODevice::Text))return 2;
    int failures=0;
    auto check=[&](bool ok,const char *name){report.write(QByteArray(name)+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failures;};
    const QString previousDataPath=qEnvironmentVariable("MIKU_LIBRARY_DATA_DIR");
    const QString isolated=sandbox.path()+QStringLiteral("/独立 数据目录");
#ifdef Q_OS_WIN
    _wputenv_s(L"MIKU_LIBRARY_DATA_DIR",isolated.toStdWString().c_str());
#else
    qputenv("MIKU_LIBRARY_DATA_DIR",isolated.toUtf8());
#endif
    check(AppConfig::dataDir()==QDir(isolated).absolutePath(),"explicit data directory isolates package startup checks");
#ifdef Q_OS_WIN
    _wputenv_s(L"MIKU_LIBRARY_DATA_DIR",previousDataPath.toStdWString().c_str());
#else
    if(previousDataPath.isEmpty())qunsetenv("MIKU_LIBRARY_DATA_DIR");else qputenv("MIKU_LIBRARY_DATA_DIR",previousDataPath.toUtf8());
#endif
    using State=AppConfig::DownloadDirectoryState;
    AppConfig first;QString error;
    const QString root=sandbox.path()+QStringLiteral("/系统下载");
    check(first.prepareDownloadDirectory(&error,root)==State::CreatedDefault,"first use creates default directory");
    check(QFileInfo(first.engine.savePath).isDir()&&first.engine.savePath==QDir(root).filePath("library"),"default folder is inside system Downloads root");
    check(first.paths.downloadDir==first.engine.savePath&&first.hasSavedDownloadDirectory,"new default is selected consistently");
    check(first.prepareDownloadDirectory(&error,root)==State::Ready,"existing saved folder is preserved");
    const QString marker=first.engine.savePath+"/keep.txt";
    QFile keep(marker);keep.open(QIODevice::WriteOnly);keep.write("keep");keep.close();
    AppConfig again;
    check(again.prepareDownloadDirectory(&error,root)==State::CreatedDefault&&QFileInfo::exists(marker),"pre-existing default folder and contents are preserved");
    AppConfig missing;missing.hasSavedDownloadDirectory=true;missing.engine.savePath=sandbox.path()+"/removed";
    check(missing.prepareDownloadDirectory(&error,root)==State::MissingSaved&&!QFileInfo::exists(missing.engine.savePath)&&!error.isEmpty(),"missing saved folder requires replacement without recreation");
    missing.engine.savePath=marker;
    check(missing.prepareDownloadDirectory(&error,root)==State::MissingSaved,"file replacing saved directory requires replacement");
    AppConfig blocked;
    check(blocked.prepareDownloadDirectory(&error,marker)==State::Error&&!error.isEmpty(),"failed default creation reports error");
    const QString downloads=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    check(downloads.isEmpty()||PathConfig().downloadDir==QDir(downloads).filePath("library"),"default path follows operating-system Downloads location");
    return failures?1:0;
}

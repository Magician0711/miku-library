// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "ui/pages/SettingsPage.h"
#include "core/AppConfig.h"
#include "net/TorrentEngine.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSpinBox>
#include <QFile>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QEventLoop>
#include <QTimer>
#include <QCheckBox>

int main(int argc,char **argv) {
    QApplication app(argc,argv);Theme::applyTheme(&app);QTemporaryDir dir;
    QFile report(QCoreApplication::applicationDirPath()+"/settings-limits-smoke.log");report.open(QIODevice::WriteOnly);
    int failures=0;auto check=[&](bool ok,const char *name){report.write(QByteArray(name)+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failures;};
    AppConfig config;TorrentEngine engine(config.engine,nullptr,dir.path());SettingsPage page(&engine,&config);
    for(const auto *name:{"DownloadLimit","ConnectionLimit","UploadSlots","ActiveDownloads","ActiveSeeds","ActiveLimit"}) {
        const auto *field=page.findChild<QSpinBox*>(name);
        check(field&&field->minimum()==0&&field->value()==0&&field->text()==QStringLiteral("不限"),name);
    }
    auto *upload=page.findChild<QSpinBox*>("UploadLimit");
    check(upload&&upload->value()==1024&&upload->text().contains("1024"),"global upload shows 1024 KiB per second");
    upload->setValue(0);check(upload->text()==QStringLiteral("不限"),"upload unlimited remains optional");
    auto *connections=page.findChild<QSpinBox*>("ConnectionLimit");connections->setValue(500);
    check(connections->value()==500&&connections->text()=="500","custom connection limit remains editable");
    auto *type=page.findChild<QComboBox*>("DownloaderType");auto *program=page.findChild<QLineEdit*>("DownloaderProgram");auto *arguments=page.findChild<QLineEdit*>("DownloaderArguments");
    check(type&&type->count()==6&&type->currentData().toString()=="builtin","built-in and four presets plus custom are offered");
    check(!program->isEnabled(),"builtin keeps executable picker inactive");
    check(page.findChild<QWidget*>("DownloaderProgramSection")->isHidden()&&page.findChild<QWidget*>("DownloaderArgumentSection")->isHidden(),"builtin hides irrelevant external configuration");
    page.editDownloader("qbittorrent");program->setText(QStringLiteral("C:/程序 文件/qbittorrent.exe"));
    check(!page.findChild<QWidget*>("DownloaderProgramSection")->isHidden()&&page.findChild<QWidget*>("DownloaderArgumentSection")->isHidden(),"preset only shows required program configuration");
    check(program->isEnabled()&&!arguments->isEnabled(),"preset enables executable selection with fixed compatibility arguments");
    type->setCurrentIndex(type->findData("aria2"));program->setText("C:/tools/aria2c.exe");
    type->setCurrentIndex(type->findData("qbittorrent"));
    check(program->text()==QStringLiteral("C:/程序 文件/qbittorrent.exe"),"switching types preserves each draft executable path");
    type->setCurrentIndex(type->findData("custom"));
    check(!page.findChild<QWidget*>("DownloaderArgumentSection")->isHidden(),"custom mode reveals its argument section");
    check(arguments->isEnabled()&&page.findChild<QPushButton*>("BrowseDownloader"),"custom type exposes template and native executable picker");
    check(config.downloader.selected=="builtin"&&config.downloader.programs.isEmpty(),"draft changes do not affect active downloader until applied");
    page.resize(1120,850);page.setAttribute(Qt::WA_DontShowOnScreen);page.show();page.editDownloader("custom");
    QCoreApplication::processEvents();{QEventLoop loop;QTimer::singleShot(80,&loop,&QEventLoop::quit);loop.exec();}page.grab().save(QCoreApplication::applicationDirPath()+"/downloader-settings-preview.png");
    page.editDownloader("builtin");QCoreApplication::processEvents();page.grab().save(QCoreApplication::applicationDirPath()+"/downloader-builtin-preview.png");
    page.editDownloader("qbittorrent");auto *rpc=page.findChild<QCheckBox*>("DownloaderRpcEnabled");auto *url=page.findChild<QLineEdit*>("DownloaderRpcUrl");
    rpc->setChecked(true);url->setText("http://127.0.0.1:8099");
    check(page.findChild<QWidget*>("DownloaderProgramSection")->isHidden()&&url->isEnabled(),"RPC mode only shows relevant connection controls");
    type->setCurrentIndex(type->findData("transmission"));check(!rpc->isChecked()&&url->text().endsWith("9091/transmission/rpc"),"RPC defaults are isolated by client type");
    type->setCurrentIndex(type->findData("qbittorrent"));check(rpc->isChecked()&&url->text().endsWith(":8099"),"RPC draft survives changing client type");
    QCoreApplication::processEvents();page.grab().save(QCoreApplication::applicationDirPath()+"/downloader-rpc-preview.png");
    check(page.findChild<QSpinBox*>("SourceStallSeconds")->value()==90,"stall recovery defaults to ninety seconds");
    check(page.findChild<QCheckBox*>("PublicTrackerFallback")->isChecked(),"public tracker fallback is visible and enabled by default");
    auto limited=config.engine;limited.publicTrackerFallback=false;limited.fallbackTrackers={"http://127.0.0.1:1234/announce"};
    const auto reload=EngineSettings::fromJson(limited.toJson());check(!reload.publicTrackerFallback&&reload.fallbackTrackers==limited.fallbackTrackers,"public discovery preference and tracker list survive serialization");
    {
        // The apply bar exists only while something would actually change.
        auto *bar=page.findChild<QWidget*>("SaveBar");
        auto wait=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
        page.editDownloader("builtin");wait(350);
        check(bar&&!bar->isVisible(),"no apply bar without changes");
        type->setCurrentIndex(type->findData("aria2"));type->setCurrentIndex(type->findData("builtin"));wait(350);
        check(!bar->isVisible(),"browsing downloader types alone is not a change");
        auto *limit=page.findChild<QSpinBox*>("ConnectionLimit");const int saved=limit->value();
        limit->setValue(saved+321);wait(350);
        check(bar->isVisible(),"apply bar appears after a real change");
        limit->setValue(saved);wait(350);
        check(!bar->isVisible(),"changing a value back hides the apply bar");
        limit->setValue(saved+77);wait(350);
        for(auto *b:bar->findChildren<QPushButton*>())if(b->text()==QStringLiteral("放弃更改"))b->click();
        wait(350);
        check(!bar->isVisible()&&limit->value()==saved,"discarding restores the value and hides the apply bar");
    }
    check(!engine.running(),"settings checks do not start network or touch real tasks");
    return failures?1:0;
}

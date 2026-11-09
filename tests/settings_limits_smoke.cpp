// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "app/DialogHub.h"
#include "app/SettingsController.h"
#include "app/ThemeController.h"
#include "core/AppConfig.h"
#include "net/TorrentEngine.h"

#include <QFile>
#include <QGuiApplication>
#include <QJsonObject>
#include <QTemporaryDir>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    // Some settings save immediately; never let that reach the real profile.
    qputenv("MIKU_LIBRARY_DATA_DIR", dir.path().toUtf8());
    QFile report(QCoreApplication::applicationDirPath() + "/settings-limits-smoke.log");
    report.open(QIODevice::WriteOnly);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { report.write(QByteArray(name) + (ok ? " PASS\n" : " FAIL\n")); report.flush(); if (!ok) ++failures; };

    AppConfig config;
    TorrentEngine engine(config.engine, nullptr, dir.path());
    ThemeController theme(QStringLiteral("light"));
    DialogHub dialogs;
    SettingsController page(&engine, &config, &theme, &dialogs);
    auto form = [&](const char *name) { return page.form().value(QString::fromLatin1(name)); };

    for (const auto *name : {"downloadLimit", "maxConnections", "maxUploads", "activeDownloads", "activeSeeds", "activeLimit"})
        check(form(name).toInt() == 0, name);
    check(form("uploadLimit").toInt() == 1024, "global upload shows 1024 KiB per second");
    page.set("maxConnections", 500);
    check(page.engineFromForm().maxConnections == 500, "custom connection limit reaches the engine settings");
    page.set("maxConnections", 0);

    check(page.downloaderTypes().size() == 6 && form("downloaderType") == "builtin", "built-in and four presets plus custom are offered");
    check(!form("externalVisible").toBool() && !form("customVisible").toBool(), "builtin hides irrelevant external configuration");
    page.editDownloader("qbittorrent");
    page.set("downloaderPath", QStringLiteral("C:/程序 文件/qbittorrent.exe"));
    check(form("externalVisible").toBool() && !form("customVisible").toBool(), "preset only shows required program configuration");
    page.set("downloaderType", "aria2");
    page.set("downloaderPath", "C:/tools/aria2c.exe");
    page.set("downloaderType", "qbittorrent");
    check(form("downloaderPath") == QStringLiteral("C:/程序 文件/qbittorrent.exe"), "switching types preserves each draft executable path");
    page.set("downloaderType", "custom");
    check(form("customVisible").toBool(), "custom mode reveals its argument section");
    check(config.downloader.selected == "builtin" && config.downloader.programs.isEmpty(), "draft changes do not affect active downloader until applied");
    page.editDownloader("qbittorrent");
    page.set("rpcEnabled", true);
    page.set("rpcUrl", "http://127.0.0.1:8099");
    page.set("downloaderType", "transmission");
    check(!form("rpcEnabled").toBool() && form("rpcUrl").toString().endsWith("9091/transmission/rpc"), "RPC defaults are isolated by client type");
    page.set("downloaderType", "qbittorrent");
    check(form("rpcEnabled").toBool() && form("rpcUrl").toString().endsWith(":8099"), "RPC draft survives changing client type");
    page.load();

    check(form("sourceStall").toInt() == 90, "stall recovery defaults to ninety seconds");
    check(form("publicTrackerFallback").toBool(), "public tracker fallback is enabled by default");
    auto limited = config.engine;
    limited.publicTrackerFallback = false;
    limited.fallbackTrackers = {"http://127.0.0.1:1234/announce"};
    const auto reload = EngineSettings::fromJson(limited.toJson());
    check(!reload.publicTrackerFallback && reload.fallbackTrackers == limited.fallbackTrackers, "public discovery preference and tracker list survive serialization");

    {
        // Seeding is the default; stopping it takes a deliberate second click.
        check(!EngineSettings{}.stopOnCompletion && !EngineSettings::fromJson(QJsonObject{}).stopOnCompletion, "seeding after completion is the default");
        check(!form("stopOnCompletion").toBool(), "the stop switch starts off");
        page.requestStopOnCompletion(true);
        const auto spec = dialogs.current();
        check(spec.value("text").toString().contains(QStringLiteral("死种")), "turning the stop on explains the harm to others");
        check(spec.value("rejectIsDefault").toBool() && spec.value("rejectText") == QStringLiteral("继续做种"), "keeping seeding is the default answer");
        check(!form("stopOnCompletion").toBool(), "nothing changes until the question is answered");
        dialogs.answer("reject");
        check(!form("stopOnCompletion").toBool(), "declining leaves seeding on");
        page.requestStopOnCompletion(true);
        dialogs.answer("accept");
        check(form("stopOnCompletion").toBool(), "a second confirmation turns the stop on");
        page.requestStopOnCompletion(false);
        check(!form("stopOnCompletion").toBool() && dialogs.current().isEmpty(), "going back to seeding needs no confirmation");
    }
    {
        // The apply bar exists only while something would actually change.
        page.load();
        check(!page.dirty(), "no apply bar without changes");
        page.set("downloaderType", "aria2");
        page.set("downloaderType", "builtin");
        check(!page.dirty(), "browsing downloader types alone is not a change");
        page.set("maxConnections", 321);
        check(page.dirty(), "apply bar appears after a real change");
        page.set("maxConnections", 0);
        check(!page.dirty(), "changing a value back hides the apply bar");
        page.set("maxConnections", 77);
        page.load();
        check(!page.dirty() && form("maxConnections").toInt() == 0, "discarding restores the value and hides the apply bar");
    }
    {
        page.set("closeAction", "tray");
        page.set("updateInterval", 6);
        const auto saved = AppConfig::load();
        check(config.closeAction == "tray" && saved.closeAction == "tray" && saved.updateIntervalHours == 6, "window and update-check choices save at once");
        check(!page.dirty(), "immediate settings do not raise the apply bar");
        config.closeAction = "quit";
        page.reloadGeneral();
        check(form("closeAction") == "quit", "general controls reload from the saved config");
        page.set("proxyHost", "10.0.0.1");
        check(!page.dirty(), "proxy address is ignored while the system proxy is used");
        page.set("proxyMode", "socks5");
        check(page.dirty(), "switching to a manual proxy needs applying");
        page.load();
    }
    {
        page.setAppearance("dark");
        check(theme.mode() == "dark" && config.appearance == "dark" && AppConfig::load().appearance == "dark", "appearance applies and saves at once");
    }
    check(!engine.running(), "settings checks do not start network or touch real tasks");
    return failures ? 1 : 0;
}

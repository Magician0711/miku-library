// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "SettingsController.h"

#include "DialogHub.h"
#include "Format.h"
#include "NativeDialogs.h"
#include "ThemeController.h"
#include "core/AppConfig.h"
#include "net/ExternalDownloader.h"
#include "net/TorrentEngine.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QNetworkInterface>
#include <QSet>

namespace {
constexpr int kDownloaderSection = 1;

const QSet<QString> kGeneralKeys = {"closeAction", "removeAction", "notifyDownloads", "updateInterval"};

// Compare what each setting resolves to, so merely looking at another
// downloader type (which leaves empty drafts and default URLs) is no change.
bool sameDownloader(const DownloaderSettings &a, const DownloaderSettings &b)
{
    if (a.selected != b.selected || a.customArguments != b.customArguments)
        return false;
    for (const auto &type : DownloaderSettings::types()) {
        if (a.programs.value(type) != b.programs.value(type))
            return false;
        if (!DownloaderSettings::supportsRpc(type))
            continue;
        const auto x = a.rpcProfile(type), y = b.rpcProfile(type);
        if (x.enabled != y.enabled || x.url != y.url || x.username != y.username || x.password != y.password)
            return false;
    }
    return true;
}
}

SettingsController::SettingsController(TorrentEngine *engine, AppConfig *config, ThemeController *theme, DialogHub *dialogs, QObject *parent)
    : QObject(parent), m_engine(engine), m_config(config), m_theme(theme), m_dialogs(dialogs)
{
    m_interfaces << QVariantMap{{"text", QStringLiteral("自动选择（默认路由）")}, {"value", QString()}};
    QSet<QString> seen;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const auto &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.ip().isLoopback())
                continue;
            const QString address = entry.ip().toString();
            if (seen.contains(address))
                continue;
            seen.insert(address);
            m_interfaces << QVariantMap{{"text", QStringLiteral("%1 · %2").arg(address, iface.humanReadableName())}, {"value", address}};
        }
    }
    m_updateStatus = QStringLiteral("当前版本 %1").arg(version());
    load();
}

void SettingsController::setSection(int section)
{
    if (section == m_section)
        return;
    m_section = section;
    emit sectionChanged();
}

QVariantList SettingsController::downloaderTypes() const
{
    QVariantList list;
    for (const auto &type : DownloaderSettings::types())
        list << QVariantMap{{"text", DownloaderSettings::name(type)}, {"value", type}};
    return list;
}

QString SettingsController::version() const { return QCoreApplication::applicationVersion(); }
QStringList SettingsController::disclaimer() const { return Notes::disclaimerItems(); }
QString SettingsController::playerNote() const { return Notes::playerNote(); }
QString SettingsController::freeNote() const { return Notes::freeSoftwareNote(); }

void SettingsController::setAppearance(const QString &mode)
{
    if (mode == m_theme->mode())
        return;
    m_theme->setMode(mode);
    m_config->appearance = mode;
    if (!m_config->save())
        m_dialogs->message(QStringLiteral("外观未保存"), QStringLiteral("已切换外观，但无法写入配置文件，下次启动将恢复原设置。"), QStringLiteral("warning"));
}

void SettingsController::reloadGeneral()
{
    m_form.insert("closeAction", m_config->closeAction);
    m_form.insert("removeAction", m_config->removeAction);
    m_form.insert("notifyDownloads", m_config->notifyDownloads);
    m_form.insert("updateInterval", m_config->updateIntervalHours);
    m_form.insert("checkAppUpdates", m_config->checkAppUpdates);
    emit formChanged();
}

void SettingsController::load()
{
    const auto &s = m_config->engine;
    m_form.insert("proxyMode", m_config->proxyMode);
    m_form.insert("proxyHost", m_config->proxyHost);
    m_form.insert("proxyPort", m_config->proxyPort > 0 ? m_config->proxyPort : 7890);
    m_form.insert("savePath", s.savePath);
    m_form.insert("sourceStall", s.sourceStallSeconds);
    bool knownInterface = false;
    for (const auto &entry : std::as_const(m_interfaces))
        knownInterface = knownInterface || entry.toMap().value("value").toString() == s.bindAddress;
    if (!knownInterface && !s.bindAddress.isEmpty()) {
        m_interfaces << QVariantMap{{"text", QStringLiteral("当前：%1").arg(s.bindAddress)}, {"value", s.bindAddress}};
        emit interfacesChanged();
    }
    m_form.insert("bindAddress", s.bindAddress);
    m_form.insert("listenPort", s.listenPort);
    m_form.insert("dht", s.dht);
    m_form.insert("lsd", s.lsd);
    m_form.insert("upnp", s.upnp);
    m_form.insert("publicTrackerFallback", s.publicTrackerFallback);
    m_form.insert("encryption", qBound(0, s.encryption, 2));
    m_form.insert("downloadLimit", s.downloadLimit);
    m_form.insert("uploadLimit", s.uploadLimit);
    m_form.insert("maxConnections", s.maxConnections);
    m_form.insert("maxUploads", s.maxUploads);
    m_form.insert("queueing", s.queueing);
    m_form.insert("stopOnCompletion", s.stopOnCompletion);
    m_form.insert("activeDownloads", s.activeDownloads);
    m_form.insert("activeSeeds", s.activeSeeds);
    m_form.insert("activeLimit", s.activeLimit);
    m_draft = m_config->downloader;
    m_editType.clear();
    m_form.insert("downloaderType", m_draft.selected);
    loadDownloaderFields();
    reloadGeneral();
    m_status = m_engine->running() ? QStringLiteral("内置引擎运行中 · %1").arg(m_engine->version()) : QStringLiteral("内置引擎未启动");
    emit statusChanged();
    updateDirty();
}

void SettingsController::loadDownloaderFields()
{
    m_editType = m_form.value("downloaderType").toString();
    const bool external = m_editType != "builtin";
    const auto rpc = m_draft.rpcProfile(m_editType);
    m_form.insert("rpcEnabled", rpc.enabled);
    m_form.insert("rpcUrl", rpc.url);
    m_form.insert("rpcUser", rpc.username);
    m_form.insert("rpcPassword", rpc.password);
    m_form.insert("rpcVisible", DownloaderSettings::supportsRpc(m_editType));
    m_form.insert("downloaderPath", m_draft.programs.value(m_editType));
    m_form.insert("customArguments", m_draft.customArguments);
    m_form.insert("customVisible", m_editType == "custom");
    m_form.insert("externalVisible", external);
    QString help = QStringLiteral("直接管理下载进度、补缺集与本地视频，无需额外配置。");
    if (m_editType == "qbittorrent" || m_editType == "bitcomet") help = QStringLiteral("自动传入番剧保存目录和种子，已有任务继续由原下载器处理。");
    else if (m_editType == "transmission") help = QStringLiteral("请在 Transmission 添加任务窗口中核对保存目录。");
    else if (m_editType == "aria2") help = QStringLiteral("选择 aria2c.exe，支持后台运行与本地合集缺集选择。");
    else if (m_editType == "custom") help = QStringLiteral("选择其他下载器程序，并按该软件的说明填写启动参数。");
    m_form.insert("downloaderHelp", rpc.enabled ? QStringLiteral("通过本机接口传入番剧目录，支持暂停、恢复、移除与完成后停止。") : help);
    emit formChanged();
}

void SettingsController::saveDownloaderDraft()
{
    const QString selected = m_draft.selected;
    m_draft = downloaderFromForm();
    m_draft.selected = selected;
}

void SettingsController::set(const QString &name, const QVariant &value)
{
    if (m_form.value(name) == value)
        return;
    if (name == "downloaderType") {
        saveDownloaderDraft();
        m_form.insert(name, value);
        loadDownloaderFields();
        updateDirty();
        return;
    }
    m_form.insert(name, value);
    emit formChanged();
    if (name == "checkAppUpdates") {
        m_config->checkAppUpdates = value.toBool();
        if (!m_config->save())
            m_dialogs->message(QStringLiteral("保存失败"), QStringLiteral("无法保存设置，请检查配置文件写入权限。"), QStringLiteral("error"));
        return;
    }
    if (kGeneralKeys.contains(name)) {
        saveGeneral();
        return;
    }
    updateDirty();
}

void SettingsController::saveGeneral()
{
    const AppConfig before = *m_config;
    m_config->closeAction = m_form.value("closeAction").toString();
    m_config->removeAction = m_form.value("removeAction").toString();
    m_config->notifyDownloads = m_form.value("notifyDownloads").toBool();
    m_config->updateIntervalHours = m_form.value("updateInterval").toInt();
    if (!m_config->save()) {
        *m_config = before;
        reloadGeneral();
        m_dialogs->message(QStringLiteral("保存失败"), QStringLiteral("无法保存设置，请检查配置文件写入权限。"), QStringLiteral("error"));
        return;
    }
    emit generalSettingsChanged();
}

EngineSettings SettingsController::engineFromForm() const
{
    EngineSettings s = m_config->engine;
    s.savePath = m_form.value("savePath").toString().trimmed();
    s.bindAddress = m_form.value("bindAddress").toString();
    s.listenPort = m_form.value("listenPort").toInt();
    s.dht = m_form.value("dht").toBool();
    s.lsd = m_form.value("lsd").toBool();
    s.upnp = m_form.value("upnp").toBool();
    s.publicTrackerFallback = m_form.value("publicTrackerFallback").toBool();
    s.encryption = m_form.value("encryption").toInt();
    s.downloadLimit = m_form.value("downloadLimit").toInt();
    s.uploadLimit = m_form.value("uploadLimit").toInt();
    s.maxConnections = m_form.value("maxConnections").toInt();
    s.maxUploads = m_form.value("maxUploads").toInt();
    s.queueing = m_form.value("queueing").toBool();
    s.stopOnCompletion = m_form.value("stopOnCompletion").toBool();
    s.sourceStallSeconds = m_form.value("sourceStall").toInt();
    s.activeDownloads = m_form.value("activeDownloads").toInt();
    s.activeSeeds = m_form.value("activeSeeds").toInt();
    s.activeLimit = m_form.value("activeLimit").toInt();
    return s;
}

DownloaderSettings SettingsController::downloaderFromForm() const
{
    // The draft holds other types' edits; overlay the type currently on screen.
    DownloaderSettings s = m_draft;
    if (DownloaderSettings::supportsRpc(m_editType))
        s.rpc[m_editType] = {m_form.value("rpcEnabled").toBool(), m_form.value("rpcUrl").toString().trimmed(),
                             m_form.value("rpcUser").toString(), m_form.value("rpcPassword").toString()};
    if (!m_editType.isEmpty() && m_editType != "builtin")
        s.programs[m_editType] = m_form.value("downloaderPath").toString().trimmed();
    if (m_editType == "custom")
        s.customArguments = m_form.value("customArguments").toString();
    s.selected = m_form.value("downloaderType").toString();
    return s;
}

bool SettingsController::sameProxy() const
{
    const QString mode = m_form.value("proxyMode").toString();
    if (mode != m_config->proxyMode)
        return false;
    // Host and port only matter for a manual proxy.
    if (mode != QStringLiteral("http") && mode != QStringLiteral("socks5"))
        return true;
    return m_form.value("proxyHost").toString().trimmed() == m_config->proxyHost && m_form.value("proxyPort").toInt() == m_config->proxyPort;
}

void SettingsController::updateDirty()
{
    const bool dirty = engineFromForm().toJson() != m_config->engine.toJson() || !sameDownloader(downloaderFromForm(), m_config->downloader) || !sameProxy();
    if (dirty == m_dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged();
}

void SettingsController::apply()
{
    m_draft = downloaderFromForm();
    QString error;
    if (!ExternalDownloader::validate(m_draft, &error)) {
        m_dialogs->message(QStringLiteral("下载器配置未完成"), error, QStringLiteral("warning"));
        return;
    }
    const EngineSettings settings = engineFromForm();
    const AppConfig before = *m_config;
    m_config->engine = settings;
    m_config->paths.downloadDir = settings.savePath;
    m_config->downloader = m_draft;
    m_config->proxyMode = m_form.value("proxyMode").toString();
    m_config->proxyHost = m_form.value("proxyHost").toString().trimmed();
    m_config->proxyPort = m_form.value("proxyPort").toInt();
    if (!m_config->save()) {
        *m_config = before;
        m_dialogs->message(QStringLiteral("保存失败"), QStringLiteral("无法保存设置，请检查配置文件写入权限。"), QStringLiteral("error"));
        return;
    }
    emit downloaderChanged();
    if (before.proxyMode != m_config->proxyMode || before.proxyHost != m_config->proxyHost || before.proxyPort != m_config->proxyPort)
        emit proxyChanged();
    m_engine->applySettings(settings);
    if (QDir(before.engine.savePath) != QDir(settings.savePath))
        emit downloadFolderChanged(before.engine.savePath, settings.savePath);
    m_status = QStringLiteral("已应用 · %1").arg(m_engine->version());
    emit statusChanged();
    updateDirty();
}

void SettingsController::browseSavePath()
{
    const QString path = NativeDialogs::chooseDirectory(m_window, QStringLiteral("选择下载目录"), m_form.value("savePath").toString());
    if (!path.isEmpty())
        set("savePath", path);
}

void SettingsController::browseDownloader()
{
    const QString path = NativeDialogs::openExecutable(m_window, m_form.value("downloaderPath").toString());
    if (!path.isEmpty())
        set("downloaderPath", path);
}

void SettingsController::detectDownloader()
{
    const QString path = DownloaderSettings::detect(m_editType);
    if (!path.isEmpty())
        set("downloaderPath", path);
    else
        m_dialogs->message(QStringLiteral("未找到程序"), QStringLiteral("未在常见安装位置找到此下载器，请点击浏览手动选择。"));
}

void SettingsController::updateCacheSize()
{
    qint64 bytes = 0;
    int count = 0;
    QDirIterator it(QDir(AppConfig::dataDir()).filePath(QStringLiteral("covers")), {QStringLiteral("*.img")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        bytes += it.fileInfo().size();
        ++count;
    }
    m_cacheSize = count ? QStringLiteral("%1 张 · %2").arg(count).arg(Format::size(bytes)) : QStringLiteral("空");
    emit cacheChanged();
}

void SettingsController::clearCovers()
{
    QDirIterator it(QDir(AppConfig::dataDir()).filePath(QStringLiteral("covers")), {QStringLiteral("*.img")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        QFile::remove(it.next());
    updateCacheSize();
}

void SettingsController::requestStopOnCompletion(bool on)
{
    if (!on) {
        set("stopOnCompletion", false);
        return;
    }
    m_dialogs->ask(QStringLiteral("确定要在下载完成后停止上传吗？"),
        QStringLiteral("下载完成后继续上传（做种）是 BitTorrent 运转的基础：你能下载到的每个资源，都来自其他用户的上传。\n\n"
                       "如果大家下载完就停止，资源会很快失去来源，冷门番剧和旧番尤其容易变成再也下不动的死种，后来的人只能一直等待。\n\n"
                       "如果只是担心占用网络，可以在本页降低上传上限，不必完全停止。如果所在地对 P2P 上传有限制，请按当地规定选择。"),
        QStringLiteral("仍要停止上传"), QStringLiteral("继续做种"),
        [this](bool confirmed) {
            // The switch shows the form's value, so a refusal puts it back.
            set("stopOnCompletion", confirmed);
            emit formChanged();
        }, true);
}

void SettingsController::checkAppUpdate()
{
    m_checkingUpdate = true;
    m_updateStatus = QStringLiteral("正在检查…");
    emit updateStatusChanged();
    emit checkAppUpdateRequested();
}

void SettingsController::setUpdateResult(const QString &status)
{
    m_checkingUpdate = false;
    m_updateStatus = QStringLiteral("当前版本 %1 · %2").arg(version(), status);
    emit updateStatusChanged();
}

void SettingsController::editDownloader(const QString &type)
{
    load();
    setSection(kDownloaderSection);
    if (!type.isEmpty())
        set("downloaderType", type);
}

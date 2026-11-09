// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "DownloadsController.h"

#include "DialogHub.h"
#include "Format.h"
#include "core/AppConfig.h"
#include "core/LocalLibrary.h"
#include "net/DownloaderRpc.h"
#include "net/ExternalDownloader.h"
#include "net/TorrentEngine.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QTimer>
#include <QUrl>

namespace {
QString statusTone(const DownloadTask &task)
{
    switch (task.status) {
    case DownloadStatus::Downloading: return QStringLiteral("active");
    case DownloadStatus::Seeding:
    case DownloadStatus::Completed: return QStringLiteral("done");
    case DownloadStatus::Error: return QStringLiteral("error");
    case DownloadStatus::Pending: return QStringLiteral("wait");
    case DownloadStatus::Paused: break;
    }
    return QStringLiteral("muted");
}
}

DownloadsController::DownloadsController(TorrentEngine *engine, LocalLibrary *library, AppConfig *config, DownloaderRpc *rpc,
                                         DialogHub *dialogs, QObject *parent)
    : QObject(parent), m_engine(engine), m_library(library), m_config(config), m_rpc(rpc), m_dialogs(dialogs)
    , m_model(new MapListModel({"key", "title", "status", "tone", "progress", "progressText", "down", "up", "size", "peers",
                                "eta", "path", "selected"}, "key", this))
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, &DownloadsController::refresh);
    if (m_rpc)
        connect(m_rpc, &DownloaderRpc::changed, this, [this] { if (m_active) refresh(); });
    m_summary = QStringLiteral("暂无任务");
    m_activeCount = QStringLiteral("0");
    m_downSpeed = m_upSpeed = Format::speed(0);
}

QString DownloadsController::engineVersion() const { return m_engine->version(); }

QString DownloadsController::backend() const
{
    return QStringLiteral("下载器：") + DownloaderSettings::name(m_config->downloader.selected);
}

void DownloadsController::setActive(bool active)
{
    m_active = active;
    if (active) {
        m_timer->start();
        emit backendChanged();
        refresh();
    } else {
        m_timer->stop();
    }
}

void DownloadsController::refresh()
{
    auto tasks = m_engine->listTorrents();
    const auto remote = m_rpc ? m_rpc->tasks() : QHash<QString, DownloadTask>{};
    static const QSet<QString> externalPhases = {"external", "external_paused", "external_pending", "error", "complete"};
    for (const auto &i : m_library->items()) {
        if (i.downloader == "builtin" || !externalPhases.contains(i.phase))
            continue;
        if (remote.contains(i.key)) {
            auto t = remote.value(i.key);
            t.title = i.resource.title;
            t.qbtHash = "external:" + i.key;
            t.savePath = i.savePath;
            tasks.append(t);
            continue;
        }
        DownloadTask t;
        t.title = i.resource.title;
        t.qbtHash = "external:" + i.key;
        t.progress = i.complete ? 100 : -1;
        t.savePath = i.savePath;
        t.statusMessage = (i.phase == "complete" ? QStringLiteral("已校验完成 · ") : i.phase == "error" ? QStringLiteral("发送失败 · ")
                           : i.phase == "external_pending" ? QStringLiteral("正在发送 · ") : QStringLiteral("已发送 · "))
                          + DownloaderSettings::name(i.downloader);
        t.status = i.phase == "complete" ? DownloadStatus::Completed : i.phase == "error" ? DownloadStatus::Error : DownloadStatus::Pending;
        t.size = i.resource.fileSize;
        tasks.append(t);
    }

    qint64 down = 0, up = 0;
    int activeTasks = 0;
    QVector<QVariantMap> rows;
    rows.reserve(tasks.size());
    QSet<QString> present;
    for (const auto &task : tasks) {
        down += task.dlSpeed;
        up += task.upSpeed;
        if (task.status == DownloadStatus::Downloading || task.status == DownloadStatus::Pending)
            ++activeTasks;
        const bool external = task.qbtHash.startsWith("external:") && !remote.contains(task.qbtHash.mid(9));
        present.insert(task.qbtHash);
        rows.append({{"key", task.qbtHash}, {"title", task.title},
                     {"status", task.statusMessage.isEmpty() ? downloadStatusName(task.status) : task.statusMessage},
                     {"tone", statusTone(task)}, {"progress", task.progress},
                     {"progressText", task.progress < 0 ? QString() : QString::number(task.progress, 'f', task.progress >= 100 ? 0 : 1) + "%"},
                     {"down", external ? QStringLiteral("—") : QStringLiteral("↓ ") + Format::speed(task.dlSpeed)},
                     {"up", external ? QString() : QStringLiteral("↑ ") + Format::speed(task.upSpeed)},
                     {"size", Format::size(task.size)},
                     {"peers", external ? QStringLiteral("—") : QStringLiteral("%1 / %2").arg(task.seeders).arg(task.leechers)},
                     {"eta", task.status == DownloadStatus::Downloading ? Format::eta(task.eta) : QStringLiteral("—")},
                     {"path", task.savePath}, {"selected", m_selected.contains(task.qbtHash)}});
    }
    m_model->update(rows);
    // Tasks that left the list leave the selection too.
    const auto kept = m_selected & present;
    const bool selectionChangedNow = kept.size() != m_selected.size();
    m_selected = kept;
    if (selectionChangedNow)
        emit selectionChanged();

    m_activeCount = QString::number(activeTasks);
    m_downSpeed = Format::speed(down);
    m_upSpeed = Format::speed(up);
    m_summary = tasks.isEmpty() ? QStringLiteral("暂无任务") : QStringLiteral("共 %1 个任务").arg(tasks.size());
    emit statsChanged();
}

void DownloadsController::markSelection()
{
    auto rows = m_model->rows();
    for (auto &row : rows)
        row.insert("selected", m_selected.contains(row.value("key").toString()));
    m_model->update(rows);
    emit selectionChanged();
}

void DownloadsController::selectOnly(const QString &key)
{
    m_selected = {key};
    m_anchor = key;
    markSelection();
}

void DownloadsController::toggle(const QString &key)
{
    if (!m_selected.remove(key))
        m_selected.insert(key);
    m_anchor = key;
    markSelection();
}

void DownloadsController::extendTo(const QString &key)
{
    int from = -1, to = -1;
    const auto &rows = m_model->rows();
    for (int i = 0; i < rows.size(); ++i) {
        const QString rowKey = rows.at(i).value("key").toString();
        if (rowKey == m_anchor) from = i;
        if (rowKey == key) to = i;
    }
    if (from < 0 || to < 0) {
        selectOnly(key);
        return;
    }
    m_selected.clear();
    for (int i = qMin(from, to); i <= qMax(from, to); ++i)
        m_selected.insert(rows.at(i).value("key").toString());
    markSelection();
}

void DownloadsController::toggleAll()
{
    if (allSelected()) {
        m_selected.clear();
    } else {
        for (const auto &row : m_model->rows())
            m_selected.insert(row.value("key").toString());
    }
    markSelection();
}

void DownloadsController::prepareMenu(const QString &key)
{
    if (!m_selected.contains(key))
        selectOnly(key);
}

bool DownloadsController::allSelected() const
{
    return !m_selected.isEmpty() && m_selected.size() == m_model->count();
}

bool DownloadsController::canControl() const
{
    for (const auto &key : m_selected) {
        if (!key.startsWith("external:"))
            return true;
        const auto item = m_library->item(key.mid(9));
        if (item && m_rpc && m_rpc->manageable(*item))
            return true;
    }
    return false;
}

bool DownloadsController::canOpenClient() const
{
    return m_selected.size() == 1 && m_selected.cbegin()->startsWith("external:");
}

QString DownloadsController::removeText() const
{
    if (m_selected.size() > 1)
        return QStringLiteral("移除 %1 项").arg(m_selected.size());
    for (const auto &key : m_selected) {
        if (!key.startsWith("external:"))
            continue;
        const auto item = m_library->item(key.mid(9));
        if (!(item && m_rpc && m_rpc->manageable(*item)))
            return QStringLiteral("移除记录");
    }
    return QStringLiteral("移除任务");
}

QString DownloadsController::hint() const
{
    return m_selected.size() > 1 ? QStringLiteral("已选择 %1 个任务").arg(m_selected.size())
                                 : QStringLiteral("勾选或按住 Ctrl / Shift 可多选，右键查看更多选项");
}

void DownloadsController::control(const QString &action)
{
    for (const auto &key : m_selected) {
        if (key.startsWith("external:")) {
            if (m_rpc) m_rpc->control(key.mid(9), action);
            continue;
        }
        if (action == "pause") m_engine->pause(key);
        else m_engine->resume(key);
    }
}

void DownloadsController::removeSelected()
{
    if (m_selected.isEmpty())
        return;
    QStringList builtin;
    QVector<LibraryItem> records;
    for (const auto &key : m_selected) {
        if (!key.startsWith("external:")) {
            builtin.append(key);
            continue;
        }
        auto item = m_library->item(key.mid(9));
        if (!item)
            continue;
        if (m_rpc && m_rpc->manageable(*item)) m_rpc->control(item->key, "remove");
        else records.append(*item);
    }
    auto finish = [this, records] {
        removeRecords(records);
        refresh();
    };
    if (builtin.isEmpty()) {
        finish();
        return;
    }
    // A remembered choice skips the question; it can be changed in 偏好设置 → 下载.
    auto removeTasks = [this, builtin](bool deleteFiles) {
        for (const auto &hash : builtin)
            m_engine->remove(hash, deleteFiles);
    };
    if (m_config->removeAction != QStringLiteral("ask")) {
        removeTasks(m_config->removeAction == QStringLiteral("delete"));
        finish();
        return;
    }
    // One confirmation covers the whole selection.
    m_dialogs->confirmRemoval([this, removeTasks, finish](const QString &choice, bool remember) {
        if (choice != QLatin1String("cancel")) {
            if (remember) {
                m_config->removeAction = choice;
                if (!m_config->save())
                    m_dialogs->message(QStringLiteral("设置未保存"), QStringLiteral("无法写入配置文件，下次仍会询问。"), QStringLiteral("warning"));
            }
            removeTasks(choice == QLatin1String("delete"));
        }
        finish();
    });
}

void DownloadsController::removeRecords(const QVector<LibraryItem> &records)
{
    if (records.isEmpty())
        return;
    QStringList errors;
    for (auto item : records) {
        item.phase = "cancelled";
        QString error;
        if (!m_library->put(item, &error))
            errors.append(error);
    }
    if (!errors.isEmpty())
        m_dialogs->message(QStringLiteral("无法移除记录"), errors.join("\n"), QStringLiteral("warning"));
    else
        m_dialogs->message(QStringLiteral("记录已移除"), QStringLiteral("对应下载器中的任务仍会继续；需要取消时请在该软件中操作。"));
}

void DownloadsController::openClient()
{
    if (m_selected.size() != 1)
        return;
    const auto item = m_library->item(m_selected.cbegin()->mid(9));
    if (!item)
        return;
    if (item->downloader == "aria2") {
        m_dialogs->message(QStringLiteral("aria2 后台任务"), QStringLiteral("aria2 作为后台进程运行。可在任务保存目录查看文件及 .aria2 进度文件。"));
        return;
    }
    QString error;
    ExternalCommand command;
    command.program = item->externalProgram;
    command.directory = item->savePath;
    command.console = false;
    if (!ExternalDownloader::start(command, &error))
        m_dialogs->message(QStringLiteral("无法打开下载器"), error, QStringLiteral("warning"));
}

void DownloadsController::openFolder(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool DownloadsController::folderExists(const QString &path) const
{
    return !path.isEmpty() && QDir(path).exists();
}

void DownloadsController::copyTitle(const QString &title)
{
    QGuiApplication::clipboard()->setText(title);
}

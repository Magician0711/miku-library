// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "DialogHub.h"

#include <QCollator>
#include <QFileInfo>

#include <algorithm>

void DialogHub::show(const QVariantMap &spec, Answer done)
{
    m_queue.enqueue({spec, std::move(done)});
    if (m_current.isEmpty())
        next();
}

void DialogHub::next()
{
    if (m_queue.isEmpty()) {
        m_current.clear();
        m_done = {};
    } else {
        auto pending = m_queue.dequeue();
        m_current = pending.spec;
        m_done = std::move(pending.done);
    }
    emit currentChanged();
}

void DialogHub::answer(const QString &choice, bool remember)
{
    if (m_current.isEmpty())
        return;
    const auto done = std::exchange(m_done, {});
    // The answer may open the next dialog, which must queue behind nothing.
    next();
    if (done)
        done(choice, remember);
}

void DialogHub::message(const QString &title, const QString &text, const QString &notice)
{
    show({{"kind", "message"}, {"title", title}, {"text", text}, {"notice", notice}}, {});
}

void DialogHub::ask(const QString &title, const QString &text, const QString &acceptText, const QString &rejectText,
                    std::function<void(bool)> done, bool rejectIsDefault)
{
    show({{"kind", "ask"}, {"title", title}, {"text", text}, {"acceptText", acceptText},
          {"rejectText", rejectText}, {"rejectIsDefault", rejectIsDefault}},
         [done = std::move(done)](const QString &choice, bool) { if (done) done(choice == QLatin1String("accept")); });
}

void DialogHub::confirmRemoval(Answer done)
{
    show({{"kind", "removal"}, {"title", QStringLiteral("移除下载任务")},
          {"text", QStringLiteral("请选择是否保留已下载的文件。同时删除时，视频、种子文件和留空的文件夹都会被删除，无法撤销。")}},
         std::move(done));
}

void DialogHub::askClose(int activeDownloads, Answer done)
{
    QString text = QStringLiteral("最小化到托盘后，下载和追更检查会在后台继续，点托盘图标即可回来。");
    if (activeDownloads > 0)
        text = QStringLiteral("还有 %1 个任务正在下载。").arg(activeDownloads) + text
               + QStringLiteral("退出软件会暂停这些下载，下次启动时继续。");
    show({{"kind", "close"}, {"title", QStringLiteral("关闭 Miku Library")}, {"text", text}}, std::move(done));
}

void DialogHub::chooseVideo(const QStringList &paths, std::function<void(const QString &)> done)
{
    if (paths.isEmpty())
        return;
    if (paths.size() == 1) {
        done(paths.first());
        return;
    }
    QStringList sorted = paths;
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(sorted.begin(), sorted.end(), [&](const QString &a, const QString &b) {
        return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
    });
    QVariantList videos;
    for (const auto &path : sorted)
        videos << QVariantMap{{"path", path}, {"name", QFileInfo(path).fileName()}};
    show({{"kind", "video"}, {"title", QStringLiteral("选择要播放的视频")}, {"videos", videos}},
         [done = std::move(done)](const QString &choice, bool) { if (!choice.isEmpty()) done(choice); });
}

void DialogHub::toast(const QString &text, const QString &icon)
{
    emit toastRequested(text, icon);
}

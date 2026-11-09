// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "Format.h"

#include <QHash>
#include <QVariantMap>

namespace {
QPair<QString, QString> confidenceColors(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Probed: return {QStringLiteral("probedBg"), QStringLiteral("probedFg")};
    case Confidence::Metadata: return {QStringLiteral("subBg"), QStringLiteral("subFg")};
    case Confidence::Guessed: return {QStringLiteral("guessedBg"), QStringLiteral("guessedFg")};
    case Confidence::Unknown: break;
    }
    return {QStringLiteral("unknownBg"), QStringLiteral("unknownFg")};
}

QString confidenceTip(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Probed: return QStringLiteral("已实测（MediaInfo/FFprobe）");
    case Confidence::Metadata: return QStringLiteral("种子元数据确认");
    case Confidence::Guessed: return QStringLiteral("文件名推测，未经实测");
    case Confidence::Unknown: break;
    }
    return QStringLiteral("未知");
}

QString subtitleText(const ResourceTags &tags)
{
    if (tags.subtitleLang.isEmpty() && tags.subtitleType.isEmpty())
        return {};
    static const QHash<QString, QString> labels = {
        {QStringLiteral("zh-Hans"), QStringLiteral("简")}, {QStringLiteral("zh-Hant"), QStringLiteral("繁")},
        {QStringLiteral("zh"), QStringLiteral("中")}, {QStringLiteral("ja"), QStringLiteral("日")},
        {QStringLiteral("en"), QStringLiteral("英")}, {QStringLiteral("multi"), QStringLiteral("多语")},
    };
    QStringList langs;
    for (const auto &lang : tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts))
        langs << labels.value(lang, lang);
    QString text = langs.join(QStringLiteral("/"));
    if (!tags.subtitleType.isEmpty()) {
        static const QHash<QString, QString> types = {
            {QStringLiteral("internal"), QStringLiteral("内封")}, {QStringLiteral("hardcoded"), QStringLiteral("内嵌")},
            {QStringLiteral("external"), QStringLiteral("外挂")}, {QStringLiteral("ass"), QStringLiteral("ASS")},
            {QStringLiteral("srt"), QStringLiteral("SRT")}, {QStringLiteral("pgs"), QStringLiteral("PGS")},
        };
        text += types.value(tags.subtitleType, tags.subtitleType);
    }
    return text;
}

QVariantMap chip(const QString &text, const QString &bg, const QString &fg, const QString &tip)
{
    return {{QStringLiteral("text"), text}, {QStringLiteral("bg"), bg}, {QStringLiteral("fg"), fg}, {QStringLiteral("tip"), tip}};
}
}

namespace Format {
QString size(qint64 bytes)
{
    if (bytes <= 0)
        return QStringLiteral("—");
    static const QVector<QPair<QString, qint64>> units = {
        {QStringLiteral("TiB"), 1LL << 40}, {QStringLiteral("GiB"), 1LL << 30},
        {QStringLiteral("MiB"), 1LL << 20}, {QStringLiteral("KiB"), 1LL << 10},
    };
    for (const auto &[unit, factor] : units)
        if (bytes >= factor)
            return QStringLiteral("%1 %2").arg(QString::number(static_cast<double>(bytes) / factor, 'f', 1), unit);
    return QStringLiteral("%1 B").arg(bytes);
}

QString speed(qint64 bytes)
{
    if (bytes >= (1LL << 20))
        return QStringLiteral("%1 MB/s").arg(bytes / 1048576.0, 0, 'f', 1);
    if (bytes >= (1LL << 10))
        return QStringLiteral("%1 KB/s").arg(bytes / 1024.0, 0, 'f', 0);
    return QStringLiteral("%1 B/s").arg(bytes);
}

QString age(const QDateTime &time)
{
    if (!time.isValid())
        return {};
    const qint64 days = time.date().daysTo(QDate::currentDate());
    if (days <= 0)
        return QStringLiteral("今天");
    if (days < 31)
        return QStringLiteral("%1 天前").arg(days);
    return time.date().toString(QStringLiteral("yyyy-MM-dd"));
}

QString eta(qint64 seconds)
{
    if (seconds < 0)
        return QStringLiteral("—");
    if (seconds >= 86400)
        return QStringLiteral("%1 天").arg(seconds / 86400);
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (hours > 0)
        return QStringLiteral("%1 时 %2 分").arg(hours).arg(minutes);
    if (minutes > 0)
        return QStringLiteral("%1 分 %2 秒").arg(minutes).arg(seconds % 60);
    return QStringLiteral("%1 秒").arg(seconds);
}

QString episodeSummary(const QStringList &episodes)
{
    if (episodes.isEmpty())
        return {};
    bool consecutive = true;
    for (int i = 1; i < episodes.size() && consecutive; ++i)
        consecutive = episodes.at(i).toDouble() == episodes.at(i - 1).toDouble() + 1;
    if (episodes.size() == 1)
        return QStringLiteral("第 %1 话").arg(episodes.first());
    if (consecutive)
        return QStringLiteral("第 %1–%2 话").arg(episodes.first(), episodes.last());
    if (episodes.size() <= 4)
        return QStringLiteral("第 %1 话").arg(episodes.join(QStringLiteral("、")));
    return QStringLiteral("第 %1 等 %2 话").arg(episodes.mid(0, 3).join(QStringLiteral("、"))).arg(episodes.size());
}

QVariantList qualityChips(const ResourceTags &tags)
{
    QVariantList chips;
    auto add = [&](const QString &text, Confidence confidence, const QString &kind) {
        if (text.isEmpty())
            return;
        const auto colors = confidenceColors(confidence);
        chips << chip(text, colors.first, colors.second, QStringLiteral("%1：%2").arg(kind, confidenceTip(confidence)));
    };
    add(tags.resolution, tags.resolutionConf, QStringLiteral("分辨率"));
    QString video = tags.videoCodec;
    if (tags.bitDepth > 0)
        video += QStringLiteral(" %1bit").arg(tags.bitDepth);
    add(video, tags.videoConf, QStringLiteral("视频"));
    if (!tags.hdrType.isEmpty())
        chips << chip(tags.hdrType, QStringLiteral("hdrBg"), QStringLiteral("hdrFg"), QStringLiteral("HDR：%1").arg(confidenceTip(tags.hdrConf)));
    QString audio = tags.audioCodec;
    if (!tags.audioChannels.isEmpty())
        audio += QStringLiteral(" %1").arg(tags.audioChannels);
    add(audio, tags.audioConf, QStringLiteral("音频"));
    add(subtitleText(tags), tags.subtitleConf, QStringLiteral("字幕"));
    add(tags.sourceType, Confidence::Guessed, QStringLiteral("来源"));
    return chips;
}
}

namespace Notes {
QString playerNote()
{
    return QStringLiteral("下载完成的视频在内置播放器中打开，能完整显示 ASS 特效字幕，并可开启 Anime4K 实时超分。"
                          "离开播放页面也会继续播放，从侧栏的「正在播放」随时回来。");
}

QString freeSoftwareNote()
{
    return QStringLiteral("Miku Library 是免费的开源项目，从不以任何形式收费。如果你是付费购买、"
                          "或通过付费下载渠道获得本软件的，请立即申请退款并向平台举报售卖者。");
}

QStringList disclaimerItems()
{
    return {
        QStringLiteral("本软件仅是番剧资源的检索与下载管理工具，不提供、不存储、不上传任何影视内容。检索结果全部来自第三方公开站点，其内容与本软件无关。"),
        QStringLiteral("通过本软件下载的番剧仅供个人学习与交流使用，请在下载后 24 小时内删除。"),
        QStringLiteral("请支持正版。喜欢的作品请通过正版平台观看或购买，以支持创作者。"),
        QStringLiteral("请遵守所在地法律法规。不得将下载内容用于商业用途、公开放映、上传分享、二次分发或其他侵犯著作权的行为；由此产生的一切法律责任由使用者自行承担。"),
        QStringLiteral("BitTorrent 下载时会同时向其他用户上传已下载的数据（做种），下载完成后默认继续上传。请了解所在地关于 P2P 传输的规定；可在「偏好设置 → 下载」限制上传，或开启下载完成后自动停止。"),
        QStringLiteral("如权利人认为某些资源侵犯其合法权益，请联系提供该资源的站点处理。本软件不对第三方站点的内容、可用性及合法性负责。"),
        QStringLiteral("本软件按「现状」提供，不对下载内容的完整性、安全性及因使用本软件造成的任何损失承担责任。"),
        QStringLiteral("继续使用即表示您已阅读、理解并同意以上全部声明。"),
    };
}
}

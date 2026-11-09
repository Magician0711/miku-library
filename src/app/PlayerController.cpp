// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "PlayerController.h"

#include "core/AppConfig.h"

#include <mpv/render_d3d11.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>

#include <clocale>

namespace {

// Resolved once: the DLL ships beside the program and may be absent in a source build.
struct Mpv {
#define NX_MPV(name) decltype(&::mpv_##name) name = nullptr;
    NX_MPV(create) NX_MPV(initialize) NX_MPV(terminate_destroy) NX_MPV(set_option_string) NX_MPV(set_property_string)
    NX_MPV(get_property_string) NX_MPV(observe_property) NX_MPV(wait_event) NX_MPV(set_wakeup_callback) NX_MPV(command)
    NX_MPV(command_string) NX_MPV(free) NX_MPV(error_string) NX_MPV(render_context_create) NX_MPV(render_context_free)
    NX_MPV(render_context_render) NX_MPV(render_context_update) NX_MPV(render_context_set_update_callback)
#undef NX_MPV
} api;

bool resolve(QLibrary &library)
{
#define NX_LOAD(name) api.name = reinterpret_cast<decltype(api.name)>(library.resolve("mpv_" #name)); if (!api.name) return false;
    NX_LOAD(create) NX_LOAD(initialize) NX_LOAD(terminate_destroy) NX_LOAD(set_option_string) NX_LOAD(set_property_string)
    NX_LOAD(get_property_string) NX_LOAD(observe_property) NX_LOAD(wait_event) NX_LOAD(set_wakeup_callback) NX_LOAD(command)
    NX_LOAD(command_string) NX_LOAD(free) NX_LOAD(error_string) NX_LOAD(render_context_create) NX_LOAD(render_context_free)
    NX_LOAD(render_context_render) NX_LOAD(render_context_update) NX_LOAD(render_context_set_update_callback)
#undef NX_LOAD
    return true;
}

enum Observed : uint64_t { TimePos = 1, Duration, Pause, Volume, Mute, TrackList };

QString statePath() { return AppConfig::dataDir() + QStringLiteral("/playback.json"); }

// A position this close to either end is not worth coming back to.
constexpr qreal kResumeMargin = 15;

QStringList anime4k(int mode)
{
    static const QStringList fast = {"Clamp_Highlights", "Restore_CNN_M", "Upscale_CNN_x2_M", "AutoDownscalePre_x2", "AutoDownscalePre_x4", "Upscale_CNN_x2_S"};
    static const QStringList high = {"Clamp_Highlights", "Restore_CNN_VL", "Upscale_CNN_x2_VL", "AutoDownscalePre_x2", "AutoDownscalePre_x4", "Upscale_CNN_x2_M"};
    if (mode != 1 && mode != 2)
        return {};
    const QDir folder(QCoreApplication::applicationDirPath() + QStringLiteral("/shaders/anime4k"));
    QStringList files;
    for (const auto &name : mode == 1 ? fast : high)
        files << folder.filePath(QStringLiteral("Anime4K_%1.glsl").arg(name));
    return files;
}

QString trackLabel(const QJsonObject &track, const QString &fallback)
{
    QStringList parts;
    const QString title = track.value("title").toString(), language = track.value("lang").toString();
    if (!title.isEmpty()) parts << title;
    if (!language.isEmpty()) parts << language;
    return parts.isEmpty() ? fallback : parts.join(QStringLiteral(" · "));
}

} // namespace

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
    m_library.setFileName(QCoreApplication::applicationDirPath() + QStringLiteral("/libmpv-2.dll"));
    if (!m_library.load() || !resolve(m_library)) {
        qWarning().noquote() << "libmpv unavailable:" << m_library.errorString();
        m_library.unload();
    }
    loadState();
    m_saveTimer = new QTimer(this);
    m_saveTimer->setInterval(10000);
    connect(m_saveTimer, &QTimer::timeout, this, &PlayerController::rememberPosition);
}

PlayerController::~PlayerController()
{
    shutdown(nullptr);
}

bool PlayerController::initialize()
{
    if (m_mpv)
        return true;
    if (!available())
        return false;
    // mpv parses numbers itself and refuses to start under a locale with decimal commas.
    std::setlocale(LC_NUMERIC, "C");
    m_mpv = api.create();
    if (!m_mpv)
        return false;
    for (const auto &[name, value] : std::initializer_list<std::pair<const char *, const char *>>{
             {"vo", "libmpv"}, {"hwdec", "auto-safe"}, {"idle", "yes"}, {"keep-open", "yes"}, {"config", "no"},
             {"terminal", "no"}, {"input-default-bindings", "no"}, {"input-vo-keyboard", "no"}, {"osc", "no"},
             {"load-scripts", "no"}, {"ytdl", "no"}, {"sub-auto", "fuzzy"}, {"audio-file-auto", "fuzzy"},
             {"slang", "chs,sc,zh-Hans,zho,chi,zh,cht,tc,zh-Hant"}, {"alang", "ja,jpn,jp"}, {"volume-max", "100"}})
        api.set_option_string(m_mpv, name, value);
    setOption("volume", QString::number(m_volume));
    // Developers only: mpv's own log, for playback that misbehaves.
    if (qEnvironmentVariableIsSet("MIKU_LIBRARY_MPV_LOG"))
        setOption("log-file", qEnvironmentVariable("MIKU_LIBRARY_MPV_LOG"));
    if (api.initialize(m_mpv) < 0) {
        api.terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }
    api.observe_property(m_mpv, TimePos, "time-pos", MPV_FORMAT_DOUBLE);
    api.observe_property(m_mpv, Duration, "duration", MPV_FORMAT_DOUBLE);
    api.observe_property(m_mpv, Pause, "pause", MPV_FORMAT_FLAG);
    api.observe_property(m_mpv, Volume, "volume", MPV_FORMAT_DOUBLE);
    api.observe_property(m_mpv, Mute, "mute", MPV_FORMAT_FLAG);
    api.observe_property(m_mpv, TrackList, "track-list", MPV_FORMAT_STRING);
    api.set_wakeup_callback(m_mpv, [](void *self) {
        auto *player = static_cast<PlayerController *>(self);
        // One queued drain is enough however many events arrive before it runs.
        if (!player->m_wakeQueued.exchange(true))
            QMetaObject::invokeMethod(player, "drainEvents", Qt::QueuedConnection);
    }, this);
    applyUpscale();
    return true;
}

void PlayerController::setOption(const char *name, const QString &value)
{
    if (m_mpv)
        api.set_option_string(m_mpv, name, value.toUtf8().constData());
}

void PlayerController::command(const QStringList &arguments)
{
    if (!m_mpv)
        return;
    QList<QByteArray> utf8;
    std::vector<const char *> pointers;
    for (const auto &argument : arguments)
        utf8 << argument.toUtf8();
    for (const auto &argument : utf8)
        pointers.push_back(argument.constData());
    pointers.push_back(nullptr);
    api.command(m_mpv, pointers.data());
}

bool PlayerController::open(const QString &path, const QString &title, const QString &subtitle)
{
    if (!initialize())
        return false;
    rememberPosition();
    m_title = title;
    m_subtitle = subtitle;
    m_path = path;
    m_resumeAt = m_positions.value(path, 0);
    m_position = 0;
    m_duration = 0;
    m_active = true;
    emit activeChanged();
    emit positionChanged();
    emit durationChanged();
    m_pendingPath = path;
    loadPending();
    m_saveTimer->start();
    return true;
}

void PlayerController::loadPending()
{
    // Video needs somewhere to go: wait for the page's picture to exist.
    if (m_pendingPath.isEmpty() || !m_renderReady.load())
        return;
    const QString path = std::exchange(m_pendingPath, QString());
    command({"loadfile", path, "replace"});
    api.set_property_string(m_mpv, "pause", "no");
}

void PlayerController::togglePause()
{
    if (m_mpv)
        api.set_property_string(m_mpv, "pause", m_paused ? "no" : "yes");
}

void PlayerController::seek(qreal seconds)
{
    command({"seek", QString::number(qMax<qreal>(0, seconds), 'f', 2), "absolute+exact"});
}

void PlayerController::seekBy(qreal seconds)
{
    command({"seek", QString::number(seconds, 'f', 2), "relative+exact"});
}

void PlayerController::setVolume(int volume)
{
    m_volume = qBound(0, volume, 100);
    if (m_mpv)
        api.set_property_string(m_mpv, "volume", QByteArray::number(m_volume).constData());
    emit volumeChanged();
    saveState();
}

void PlayerController::toggleMute()
{
    if (m_mpv)
        api.set_property_string(m_mpv, "mute", m_muted ? "no" : "yes");
}

void PlayerController::setAudioTrack(int id)
{
    if (m_mpv)
        api.set_property_string(m_mpv, "aid", QByteArray::number(id).constData());
}

void PlayerController::setSubtitleTrack(int id)
{
    if (m_mpv)
        api.set_property_string(m_mpv, "sid", id > 0 ? QByteArray::number(id).constData() : "no");
}

void PlayerController::setUpscale(int mode)
{
    if (mode == m_upscale)
        return;
    m_upscale = mode;
    applyUpscale();
    // A paused picture has to show the change too.
    m_redrawWanted.store(true);
    emit frameReady();
    emit upscaleChanged();
    saveState();
}

void PlayerController::applyUpscale()
{
    if (m_mpv)
        api.set_property_string(m_mpv, "glsl-shaders", anime4k(m_upscale).join(QLatin1Char(';')).toUtf8().constData());
}

QString PlayerController::shaderList() const
{
    if (!m_mpv)
        return {};
    char *value = api.get_property_string(m_mpv, "glsl-shaders");
    const QString result = QString::fromUtf8(value);
    if (value)
        api.free(value);
    return result;
}

void PlayerController::setVideoVisible(bool visible)
{
    if (visible == m_videoVisible)
        return;
    m_videoVisible = visible;
    if (m_mpv && m_active)
        api.set_property_string(m_mpv, "vid", visible ? "auto" : "no");
    emit videoVisibleChanged();
}

void PlayerController::stop()
{
    rememberPosition();
    m_pendingPath.clear();
    command({"stop"});
    m_saveTimer->stop();
    m_active = false;
    m_title.clear();
    m_subtitle.clear();
    m_path.clear();
    emit activeChanged();
}

void PlayerController::drainEvents()
{
    m_wakeQueued.store(false);
    while (m_mpv) {
        const mpv_event *event = api.wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            if (m_resumeAt > kResumeMargin) {
                seek(m_resumeAt);
                m_resumeAt = 0;
            }
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<const mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR) {
                emit failed(QStringLiteral("无法播放这个视频：%1").arg(QString::fromUtf8(api.error_string(end->error))));
                m_active = false;
                emit activeChanged();
            }
        } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *change = static_cast<const mpv_event_property *>(event->data);
            const bool isDouble = change->format == MPV_FORMAT_DOUBLE, isFlag = change->format == MPV_FORMAT_FLAG;
            const double number = isDouble ? *static_cast<double *>(change->data) : 0;
            const bool flag = isFlag && *static_cast<int *>(change->data) != 0;
            switch (event->reply_userdata) {
            case TimePos:
                if (isDouble && qAbs(number - m_position) >= 0.05) { m_position = number; emit positionChanged(); }
                break;
            case Duration:
                if (isDouble) { m_duration = number; emit durationChanged(); }
                break;
            case Pause:
                if (isFlag) { m_paused = flag; emit pausedChanged(); }
                break;
            case Volume:
                if (isDouble) { m_volume = qRound(number); emit volumeChanged(); }
                break;
            case Mute:
                if (isFlag) { m_muted = flag; emit volumeChanged(); }
                break;
            case TrackList:
                if (change->format == MPV_FORMAT_STRING)
                    parseTracks(QString::fromUtf8(*static_cast<char **>(change->data)));
                break;
            default:
                break;
            }
        }
    }
}

void PlayerController::parseTracks(const QString &json)
{
    QVariantList audio, subtitles{QVariantMap{{"text", QStringLiteral("关闭字幕")}, {"value", 0}}};
    int chosenAudio = 0, chosenSubtitle = 0, audioNumber = 0, subtitleNumber = 0;
    for (const auto &value : QJsonDocument::fromJson(json.toUtf8()).array()) {
        const auto track = value.toObject();
        const int id = track.value("id").toInt();
        const bool selected = track.value("selected").toBool();
        if (track.value("type").toString() == QLatin1String("audio")) {
            audio << QVariantMap{{"text", trackLabel(track, QStringLiteral("音轨 %1").arg(++audioNumber))}, {"value", id}};
            if (selected) chosenAudio = id;
        } else if (track.value("type").toString() == QLatin1String("sub")) {
            subtitles << QVariantMap{{"text", trackLabel(track, QStringLiteral("字幕 %1").arg(++subtitleNumber))}, {"value", id}};
            if (selected) chosenSubtitle = id;
        }
    }
    m_audioTracks = audio;
    m_subtitleTracks = subtitles;
    m_audioTrack = chosenAudio;
    m_subtitleTrack = chosenSubtitle;
    emit tracksChanged();
}

bool PlayerController::createRenderContext(void *device)
{
    QMutexLocker lock(&m_renderLock);
    if (m_render)
        return true;
    if (!m_mpv)
        return false;
    mpv_d3d11_init_params init{device};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_D3D11)},
        {MPV_RENDER_PARAM_D3D11_INIT_PARAMS, &init},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    const int result = api.render_context_create(&m_render, m_mpv, params);
    if (result < 0) {
        qWarning().noquote() << "mpv render context:" << api.error_string(result);
        m_render = nullptr;
        return false;
    }
    api.render_context_set_update_callback(m_render, [](void *self) {
        emit static_cast<PlayerController *>(self)->frameReady();
    }, this);
    m_renderReady.store(true);
    QMetaObject::invokeMethod(this, "loadPending", Qt::QueuedConnection);
    return true;
}

bool PlayerController::renderFrame(void *texture, bool force)
{
    QMutexLocker lock(&m_renderLock);
    if (!m_render)
        return false;
    const uint64_t flags = api.render_context_update(m_render);
    if (!force && !m_redrawWanted.exchange(false) && !(flags & MPV_RENDER_UPDATE_FRAME))
        return false;
    mpv_d3d11_fbo target{texture};
    mpv_render_param params[] = {{MPV_RENDER_PARAM_D3D11_FBO, &target}, {MPV_RENDER_PARAM_INVALID, nullptr}};
    return api.render_context_render(m_render, params) >= 0;
}

void PlayerController::shutdown(QQuickWindow *window)
{
    if (!m_mpv)
        return;
    rememberPosition();
    if (window)
        window->hide();
    {
        QMutexLocker lock(&m_renderLock);
        m_renderReady.store(false);
        if (m_render) {
            api.render_context_free(m_render);
            m_render = nullptr;
        }
    }
    api.set_wakeup_callback(m_mpv, nullptr, nullptr);
    api.terminate_destroy(m_mpv);
    m_mpv = nullptr;
}

void PlayerController::rememberPosition()
{
    if (m_path.isEmpty() || m_path.startsWith(QStringLiteral("av://")) || m_duration <= 0)
        return;
    if (m_position > kResumeMargin && m_position < m_duration - kResumeMargin)
        m_positions.insert(m_path, m_position);
    else
        m_positions.remove(m_path);
    saveState();
}

void PlayerController::loadState()
{
    QFile file(statePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    m_volume = qBound(0, root.value("volume").toInt(100), 100);
    m_upscale = qBound(0, root.value("upscale").toInt(0), 2);
    const auto positions = root.value("positions").toObject();
    for (auto it = positions.begin(); it != positions.end(); ++it)
        m_positions.insert(it.key(), it.value().toDouble());
}

void PlayerController::saveState()
{
    QJsonObject positions;
    for (auto it = m_positions.cbegin(); it != m_positions.cend(); ++it)
        positions.insert(it.key(), it.value());
    QSaveFile file(statePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{"volume", m_volume}, {"upscale", m_upscale}, {"positions", positions}}).toJson(QJsonDocument::Compact));
    file.commit();
}

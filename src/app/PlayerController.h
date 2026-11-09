// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <mpv/client.h>
#include <mpv/render.h>

#include <QHash>
#include <QLibrary>
#include <QMutex>
#include <QObject>
#include <QVariant>
#include <QVariantList>

#include <atomic>

class QQuickWindow;
class QTimer;

// The built-in player: one libmpv instance for the whole app, so that playback goes on
// while the viewer looks at other pages. libmpv is loaded at run time; without it the app
// falls back to the system's default player.
class PlayerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString title READ title NOTIFY activeChanged)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY activeChanged)
    Q_PROPERTY(qreal position READ position NOTIFY positionChanged)
    Q_PROPERTY(qreal duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(int audioTrack READ audioTrack NOTIFY tracksChanged)
    Q_PROPERTY(int subtitleTrack READ subtitleTrack NOTIFY tracksChanged)
    // 0 off, 1 fast, 2 high quality: the Anime4K presets.
    Q_PROPERTY(int upscale READ upscale NOTIFY upscaleChanged)
    // Decoding pictures nobody can see would be wasted; audio goes on without them.
    Q_PROPERTY(bool videoVisible READ videoVisible WRITE setVideoVisible NOTIFY videoVisibleChanged)
public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    bool available() const { return m_library.isLoaded(); }
    bool active() const { return m_active; }
    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }
    qreal position() const { return m_position; }
    qreal duration() const { return m_duration; }
    bool paused() const { return m_paused; }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QVariantList audioTracks() const { return m_audioTracks; }
    QVariantList subtitleTracks() const { return m_subtitleTracks; }
    int audioTrack() const { return m_audioTrack; }
    int subtitleTrack() const { return m_subtitleTrack; }
    int upscale() const { return m_upscale; }
    bool videoVisible() const { return m_videoVisible; }
    void setVideoVisible(bool visible);

    // False when libmpv is missing.
    bool open(const QString &path, const QString &title, const QString &subtitle);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seek(qreal seconds);
    Q_INVOKABLE void seekBy(qreal seconds);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void setAudioTrack(int id);
    Q_INVOKABLE void setSubtitleTrack(int id);
    Q_INVOKABLE void setUpscale(int mode);
    Q_INVOKABLE void stop();
    // What mpv currently applies to the picture; tests read it.
    QString shaderList() const;

    // Render thread, while the scene graph is between frames.
    bool createRenderContext(void *device);
    bool renderContextReady() const { return m_renderReady.load(); }
    // True when a frame was drawn into the texture.
    bool renderFrame(void *texture, bool force);

    // Hides the window first, so that nothing else uses the graphics device meanwhile.
    void shutdown(QQuickWindow *window);

signals:
    void activeChanged();
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void volumeChanged();
    void tracksChanged();
    void upscaleChanged();
    void videoVisibleChanged();
    void frameReady();
    void failed(const QString &message);

private:
    Q_INVOKABLE void drainEvents();
    Q_INVOKABLE void loadPending();
    bool initialize();
    void command(const QStringList &arguments);
    void setOption(const char *name, const QString &value);
    void applyUpscale();
    void parseTracks(const QString &json);
    void loadState();
    void saveState();
    void rememberPosition();

    QLibrary m_library;
    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_render = nullptr;
    QMutex m_renderLock;
    std::atomic_bool m_renderReady{false};
    std::atomic_bool m_wakeQueued{false};
    std::atomic_bool m_redrawWanted{false};
    QTimer *m_saveTimer = nullptr;

    bool m_active = false, m_paused = true, m_muted = false, m_videoVisible = true;
    bool m_loading = false;
    QString m_path, m_pendingPath, m_title, m_subtitle;
    qreal m_position = 0, m_duration = 0, m_resumeAt = 0;
    int m_volume = 100, m_upscale = 0, m_audioTrack = 0, m_subtitleTrack = 0;
    QVariantList m_audioTracks, m_subtitleTracks;
    QHash<QString, qreal> m_positions;
};

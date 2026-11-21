// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QQuickItem>

#include "PlayerController.h"
class QSGTexture;
struct ID3D11DeviceContext;

// Shows the player's picture as part of the scene: mpv draws into a texture on the
// scene graph's own graphics device, and the item displays that texture. That device is
// Direct3D 11 on Windows and OpenGL on macOS and Linux (see Platform.h).
class MpvVideoItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(PlayerController *player READ player WRITE setPlayer NOTIFY playerChanged)
public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    PlayerController *player() const { return m_player; }
    void setPlayer(PlayerController *player);

signals:
    void playerChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void releaseResources() override;

private:
    // The native objects behind one texture: an ID3D11Texture2D, or an OpenGL texture
    // and the framebuffer object that draws into it.
    struct Target {
        quintptr texture = 0;
        quintptr framebuffer = 0;
    };
    static void releaseTarget(const Target &target);
    bool createTarget(const QSize &size, Target *target, QSGTexture **scene);
    void renderToTexture();
    void attach(QQuickWindow *window);
    void dropTexture();
    void followScreen();

    PlayerController *m_player = nullptr;
    QMetaObject::Connection m_renderConnection, m_swapConnection, m_screenConnection, m_rateConnection;
    Target m_target;
    ID3D11DeviceContext *m_context = nullptr;
    QSGTexture *m_sceneTexture = nullptr;
    QSize m_size;
    bool m_redraw = false;
};

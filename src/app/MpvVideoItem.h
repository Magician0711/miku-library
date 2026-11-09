// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QQuickItem>

#include "PlayerController.h"
class QSGTexture;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

// Shows the player's picture as part of the scene: mpv draws into a texture on the
// scene graph's own Direct3D device, and the item displays that texture.
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
    void renderToTexture();
    void attach(QQuickWindow *window);
    void dropTexture();

    PlayerController *m_player = nullptr;
    QMetaObject::Connection m_renderConnection;
    ID3D11Texture2D *m_texture = nullptr;
    ID3D11DeviceContext *m_context = nullptr;
    QSGTexture *m_sceneTexture = nullptr;
    QSize m_size;
    bool m_redraw = false;
};

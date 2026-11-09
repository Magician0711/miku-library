// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "MpvVideoItem.h"

#include "PlayerController.h"

#include <QQuickWindow>
#include <QRunnable>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QtQuick/qsgtexture_platform.h>

#include <d3d11.h>

#include <cmath>

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

MpvVideoItem::~MpvVideoItem()
{
    disconnect(m_renderConnection);
}

void MpvVideoItem::setPlayer(PlayerController *player)
{
    if (player == m_player)
        return;
    if (m_player)
        disconnect(m_player, nullptr, this, nullptr);
    m_player = player;
    if (m_player)
        connect(m_player, &PlayerController::frameReady, this, &QQuickItem::update, Qt::QueuedConnection);
    emit playerChanged();
    update();
}

void MpvVideoItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemSceneChange)
        attach(value.window);
    QQuickItem::itemChange(change, value);
}

void MpvVideoItem::attach(QQuickWindow *window)
{
    disconnect(m_renderConnection);
    if (window)
        m_renderConnection = connect(window, &QQuickWindow::beforeRendering, this, &MpvVideoItem::renderToTexture, Qt::DirectConnection);
}

void MpvVideoItem::dropTexture()
{
    delete m_sceneTexture;
    m_sceneTexture = nullptr;
    if (m_texture) {
        m_texture->Release();
        m_texture = nullptr;
    }
    m_size = {};
}

void MpvVideoItem::releaseResources()
{
    // Scene graph resources belong to the render thread, which is about to go idle.
    class Release final : public QRunnable {
    public:
        Release(QSGTexture *texture, ID3D11Texture2D *native) : m_texture(texture), m_native(native) {}
        void run() override { delete m_texture; if (m_native) m_native->Release(); }
    private:
        QSGTexture *m_texture;
        ID3D11Texture2D *m_native;
    };
    if (window())
        window()->scheduleRenderJob(new Release(m_sceneTexture, m_texture), QQuickWindow::BeforeSynchronizingStage);
    m_sceneTexture = nullptr;
    m_texture = nullptr;
    m_size = {};
}

QSGNode *MpvVideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    auto *renderer = window()->rendererInterface();
    if (!m_player || renderer->graphicsApi() != QSGRendererInterface::Direct3D11) {
        delete node;
        return nullptr;
    }
    auto *device = static_cast<ID3D11Device *>(renderer->getResource(window(), QSGRendererInterface::DeviceResource));
    m_context = static_cast<ID3D11DeviceContext *>(renderer->getResource(window(), QSGRendererInterface::DeviceContextResource));
    if (!m_player->renderContextReady() && !m_player->createRenderContext(device)) {
        delete node;
        return nullptr;
    }

    const qreal ratio = window()->effectiveDevicePixelRatio();
    const QSize wanted(qMax(1, int(std::ceil(width() * ratio))), qMax(1, int(std::ceil(height() * ratio))));
    if (wanted != m_size) {
        D3D11_TEXTURE2D_DESC description{};
        description.Width = UINT(wanted.width());
        description.Height = UINT(wanted.height());
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D *texture = nullptr;
        if (FAILED(device->CreateTexture2D(&description, nullptr, &texture))) {
            delete node;
            return nullptr;
        }
        QSGTexture *scene = QNativeInterface::QSGD3D11Texture::fromNative(texture, window(), wanted);
        if (!node) {
            node = new QSGSimpleTextureNode;
            node->setFiltering(QSGTexture::Linear);
        }
        node->setTexture(scene);
        dropTexture();
        m_texture = texture;
        m_sceneTexture = scene;
        m_size = wanted;
        m_redraw = true;
    }
    if (node) {
        node->setRect(boundingRect());
        node->markDirty(QSGNode::DirtyMaterial);
    }
    return node;
}

void MpvVideoItem::renderToTexture()
{
    if (!m_player || !m_texture || !m_context || !m_player->renderContextReady())
        return;
    QQuickWindow *scene = window();
    if (!scene)
        return;
    // Qt keeps its own record of the device's state; these two calls tell it to start over.
    scene->beginExternalCommands();
    m_context->ClearState();
    m_player->renderFrame(m_texture, m_redraw);
    m_redraw = false;
    scene->endExternalCommands();
}

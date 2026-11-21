// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "MpvVideoItem.h"

#include "PlayerController.h"

#include <QQuickWindow>
#include <QRunnable>
#include <QScreen>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QtQuick/qsgtexture_platform.h>

#ifdef Q_OS_WIN
#include <d3d11.h>
#else
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#endif

#include <cmath>

namespace {
#ifdef Q_OS_WIN
constexpr auto kGraphicsApi = QSGRendererInterface::Direct3D11;
#else
constexpr auto kGraphicsApi = QSGRendererInterface::OpenGL;
#endif
}

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

MpvVideoItem::~MpvVideoItem()
{
    disconnect(m_renderConnection);
    disconnect(m_swapConnection);
    disconnect(m_screenConnection);
    disconnect(m_rateConnection);
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
    followScreen();
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
    disconnect(m_swapConnection);
    disconnect(m_screenConnection);
    if (window) {
        m_renderConnection = connect(window, &QQuickWindow::beforeRendering, this, &MpvVideoItem::renderToTexture, Qt::DirectConnection);
        // Display-synced playback and interpolation time frames by when they reach the screen.
        m_swapConnection = connect(window, &QQuickWindow::frameSwapped, this, [this] {
            if (m_player && m_player->renderContextReady())
                m_player->reportSwap();
        }, Qt::DirectConnection);
        m_screenConnection = connect(window, &QWindow::screenChanged, this, &MpvVideoItem::followScreen);
    }
    QMetaObject::invokeMethod(this, &MpvVideoItem::followScreen, Qt::QueuedConnection);
}

void MpvVideoItem::followScreen()
{
    disconnect(m_rateConnection);
    QScreen *screen = window() ? window()->screen() : nullptr;
    if (!screen || !m_player)
        return;
    m_player->setDisplayFps(screen->refreshRate());
    m_rateConnection = connect(screen, &QScreen::refreshRateChanged, this, [this](qreal rate) {
        if (m_player) m_player->setDisplayFps(rate);
    });
}

// Render thread; with OpenGL the scene graph's context is current there.
void MpvVideoItem::releaseTarget(const Target &target)
{
#ifdef Q_OS_WIN
    if (target.texture)
        reinterpret_cast<ID3D11Texture2D *>(target.texture)->Release();
#else
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context || (!target.texture && !target.framebuffer))
        return;
    QOpenGLFunctions *gl = context->functions();
    const GLuint framebuffer = GLuint(target.framebuffer), texture = GLuint(target.texture);
    if (framebuffer)
        gl->glDeleteFramebuffers(1, &framebuffer);
    if (texture)
        gl->glDeleteTextures(1, &texture);
#endif
}

void MpvVideoItem::dropTexture()
{
    delete m_sceneTexture;
    m_sceneTexture = nullptr;
    releaseTarget(m_target);
    m_target = {};
    m_size = {};
}

void MpvVideoItem::releaseResources()
{
    // Scene graph resources belong to the render thread, which is about to go idle.
    class Release final : public QRunnable {
    public:
        Release(QSGTexture *texture, const Target &native) : m_texture(texture), m_native(native) {}
        void run() override { delete m_texture; MpvVideoItem::releaseTarget(m_native); }
    private:
        QSGTexture *m_texture;
        Target m_native;
    };
    if (window())
        window()->scheduleRenderJob(new Release(m_sceneTexture, m_target), QQuickWindow::BeforeSynchronizingStage);
    m_sceneTexture = nullptr;
    m_target = {};
    m_size = {};
}

bool MpvVideoItem::createTarget(const QSize &size, Target *target, QSGTexture **scene)
{
#ifdef Q_OS_WIN
    auto *renderer = window()->rendererInterface();
    auto *device = static_cast<ID3D11Device *>(renderer->getResource(window(), QSGRendererInterface::DeviceResource));
    D3D11_TEXTURE2D_DESC description{};
    description.Width = UINT(size.width());
    description.Height = UINT(size.height());
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *texture = nullptr;
    if (!device || FAILED(device->CreateTexture2D(&description, nullptr, &texture)))
        return false;
    target->texture = quintptr(texture);
    *scene = QNativeInterface::QSGD3D11Texture::fromNative(texture, window(), size);
#else
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context)
        return false;
    QOpenGLFunctions *gl = context->functions();
    GLuint texture = 0, framebuffer = 0;
    gl->glGenTextures(1, &texture);
    gl->glBindTexture(GL_TEXTURE_2D, texture);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, context->isOpenGLES() ? GL_RGBA : 0x8058 /* GL_RGBA8 */,
                     size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    gl->glGenFramebuffers(1, &framebuffer);
    gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    const bool complete = gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    gl->glBindFramebuffer(GL_FRAMEBUFFER, context->defaultFramebufferObject());
    target->texture = texture;
    target->framebuffer = framebuffer;
    if (!complete) {
        releaseTarget(*target);
        *target = {};
        return false;
    }
    *scene = QNativeInterface::QSGOpenGLTexture::fromNative(texture, window(), size);
#endif
    return *scene != nullptr;
}

QSGNode *MpvVideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    auto *renderer = window()->rendererInterface();
    if (!m_player || renderer->graphicsApi() != kGraphicsApi) {
        delete node;
        return nullptr;
    }
#ifdef Q_OS_WIN
    void *device = renderer->getResource(window(), QSGRendererInterface::DeviceResource);
    m_context = static_cast<ID3D11DeviceContext *>(renderer->getResource(window(), QSGRendererInterface::DeviceContextResource));
#else
    void *device = nullptr;
#endif
    if (!m_player->renderContextReady() && !m_player->createRenderContext(device)) {
        delete node;
        return nullptr;
    }

    const qreal ratio = window()->effectiveDevicePixelRatio();
    const QSize wanted(qMax(1, int(std::ceil(width() * ratio))), qMax(1, int(std::ceil(height() * ratio))));
    if (wanted != m_size) {
        Target target;
        QSGTexture *scene = nullptr;
        if (!createTarget(wanted, &target, &scene)) {
            releaseTarget(target);
            delete node;
            return nullptr;
        }
        if (!node) {
            node = new QSGSimpleTextureNode;
            node->setFiltering(QSGTexture::Linear);
        }
        node->setTexture(scene);
        dropTexture();
        m_target = target;
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
    if (!m_player || !m_target.texture || !m_player->renderContextReady())
        return;
    QQuickWindow *scene = window();
    if (!scene)
        return;
    // Qt keeps its own record of the device's state; these calls tell it to start over.
    scene->beginExternalCommands();
#ifdef Q_OS_WIN
    if (!m_context) {
        scene->endExternalCommands();
        return;
    }
    m_context->ClearState();
    m_player->renderFrame(m_target.texture, m_size, m_redraw);
#else
    m_player->renderFrame(m_target.framebuffer, m_size, m_redraw);
    if (QOpenGLContext *context = QOpenGLContext::currentContext())
        context->functions()->glBindFramebuffer(GL_FRAMEBUFFER, context->defaultFramebufferObject());
#endif
    m_redraw = false;
    scene->endExternalCommands();
}

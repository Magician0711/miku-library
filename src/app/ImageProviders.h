// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QIcon>
#include <QPointer>
#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>

class ImageLoader;

// image://icon/<name>/<rrggbb>: line icons authored on a 24 x 24 grid.
class IconProvider final : public QQuickImageProvider {
public:
    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

// image://cover/<radius in device pixels>/<percent-encoded url>: posters through
// ImageLoader, cut to the requested size with rounded corners off the UI thread.
class CoverProvider final : public QQuickAsyncImageProvider {
public:
    explicit CoverProvider(ImageLoader *images);
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    QPointer<ImageLoader> m_images;
};

// The teal "N" mark at several sizes, for the window, taskbar and tray.
QIcon appIcon();

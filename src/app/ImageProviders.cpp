// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "ImageProviders.h"

#include "net/ImageLoader.h"

#include <QCoreApplication>
#include <QFont>
#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRunnable>
#include <QThreadPool>
#include <QUrl>

namespace {
// Glyphs are authored on a 24 x 24 grid.
void drawGlyph(QPainter &p, const QString &name, const QColor &color)
{
 p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(color,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p.setBrush(Qt::NoBrush); QPainterPath path;
 if(name=="home"){ path.moveTo(3,11); path.lineTo(12,4); path.lineTo(21,11); path.moveTo(6,10); path.lineTo(6,20); path.lineTo(18,20); path.lineTo(18,10); path.moveTo(10,20); path.lineTo(10,14); path.lineTo(14,14); path.lineTo(14,20); }
 else if(name=="calendar"){ p.drawRoundedRect(QRectF(3.5,5,17,15.5),3,3); p.drawLine(QPointF(3.5,10),QPointF(20.5,10)); p.drawLine(8,3,8,7); p.drawLine(16,3,16,7); p.drawPoint(QPointF(8,14.5)); p.drawPoint(QPointF(12,14.5)); p.drawPoint(QPointF(16,14.5)); }
 else if(name=="search"){ p.drawEllipse(QRectF(4,4,12.5,12.5)); p.drawLine(QPointF(14.5,14.5),QPointF(20,20)); }
 else if(name=="library" || name=="heart"){ path.moveTo(12,20); path.cubicTo(5,15,3,12,3,8.5); path.cubicTo(3,5.5,5.3,3.8,7.6,3.8); path.cubicTo(9.6,3.8,11,5,12,6.6); path.cubicTo(13,5,14.4,3.8,16.4,3.8); path.cubicTo(18.7,3.8,21,5.5,21,8.5); path.cubicTo(21,12,19,15,12,20); }
 else if(name=="download"){ path.moveTo(12,3.5); path.lineTo(12,15); path.moveTo(7,10); path.lineTo(12,15); path.lineTo(17,10); path.moveTo(4,16); path.lineTo(4,20); path.lineTo(20,20); path.lineTo(20,16); }
 else if(name=="sites"){ p.drawEllipse(QRectF(3,3,18,18)); p.drawEllipse(QRectF(8,3,8,18)); p.drawLine(3,12,21,12); }
 else if(name=="settings"){ for(int y:{6,12,18}) p.drawLine(4,y,20,y); const QPointF knobs[]={{9,6},{15,12},{10,18}}; p.setCompositionMode(QPainter::CompositionMode_Clear); p.setPen(Qt::NoPen); p.setBrush(Qt::black); for(const auto &k:knobs) p.drawEllipse(k,2.6,2.6); p.setCompositionMode(QPainter::CompositionMode_SourceOver); p.setPen(QPen(color,1.7)); p.setBrush(Qt::NoBrush); for(const auto &k:knobs) p.drawEllipse(k,2.2,2.2); }
 else if(name=="volume" || name=="mute"){ path.moveTo(4,10);path.lineTo(4,14);path.lineTo(8,14);path.lineTo(12,18);path.lineTo(12,6);path.lineTo(8,10);path.closeSubpath(); if(name=="volume"){ path.moveTo(15.5,9);path.quadTo(17.5,12,15.5,15);path.moveTo(18,6.5);path.quadTo(22,12,18,17.5); } else { path.moveTo(16,10);path.lineTo(21,15);path.moveTo(21,10);path.lineTo(16,15); } }
 else if(name=="fullscreen"){ path.moveTo(4,9);path.lineTo(4,4);path.lineTo(9,4);path.moveTo(15,4);path.lineTo(20,4);path.lineTo(20,9);path.moveTo(20,15);path.lineTo(20,20);path.lineTo(15,20);path.moveTo(9,20);path.lineTo(4,20);path.lineTo(4,15); }
 else if(name=="exit-fullscreen"){ path.moveTo(9,4);path.lineTo(9,9);path.lineTo(4,9);path.moveTo(15,4);path.lineTo(15,9);path.lineTo(20,9);path.moveTo(20,15);path.lineTo(15,15);path.lineTo(15,20);path.moveTo(9,20);path.lineTo(9,15);path.lineTo(4,15); }
 else if(name=="sidebar"){ p.drawRoundedRect(QRectF(3.5,5,17,14),3,3); p.drawLine(QPointF(9.5,5),QPointF(9.5,19)); }
 else if(name=="spinner"){ p.drawArc(QRectF(4.5,4.5,15,15),0,270*16); }
 else if(name=="refresh"){ path.moveTo(19,8); path.cubicTo(16,1,5,3,5,11); path.moveTo(5,16); path.cubicTo(9,23,20,21,20,13); path.moveTo(19,3); path.lineTo(19,8); path.lineTo(14,8); path.moveTo(5,21); path.lineTo(5,16); path.lineTo(10,16); }
 else if(name=="chevron-down"){ path.moveTo(7,10);path.lineTo(12,15);path.lineTo(17,10); }
 else if(name=="chevron-up"){ path.moveTo(7,14);path.lineTo(12,9);path.lineTo(17,14); }
 else if(name=="chevron-right"){ path.moveTo(10,7);path.lineTo(15,12);path.lineTo(10,17); }
 else if(name=="chevron-left"){ path.moveTo(14,7);path.lineTo(9,12);path.lineTo(14,17); }
 else if(name=="selector"){ path.moveTo(8,9);path.lineTo(12,5);path.lineTo(16,9);path.moveTo(8,15);path.lineTo(12,19);path.lineTo(16,15); }
 else if(name=="back"){ path.moveTo(14,5); path.lineTo(7,12); path.lineTo(14,19); path.moveTo(7,12); path.lineTo(21,12); }
 else if(name=="forward"){ path.moveTo(10,5);path.lineTo(17,12);path.lineTo(10,19);path.moveTo(3,12);path.lineTo(17,12); }
 else if(name=="up"){ path.moveTo(5,11);path.lineTo(12,4);path.lineTo(19,11);path.moveTo(12,4);path.lineTo(12,21); }
 else if(name=="folder" || name=="folder-plus"){ path.moveTo(3,8);path.lineTo(3,5);path.lineTo(10,5);path.lineTo(12,8);path.lineTo(21,8);path.lineTo(21,20);path.lineTo(3,20);path.closeSubpath();if(name=="folder-plus"){path.moveTo(9,14);path.lineTo(15,14);path.moveTo(12,11);path.lineTo(12,17);} }
 else if(name=="grid"){ for(int x:{4,14})for(int y:{4,14})p.drawRoundedRect(QRectF(x,y,6,6),1.2,1.2); }
 else if(name=="list"){ for(int y:{6,12,18}){p.drawPoint(4,y);p.drawLine(9,y,20,y);} }
 else if(name=="external"){ path.moveTo(14,4); path.lineTo(20,4); path.lineTo(20,10); path.moveTo(20,4); path.lineTo(11,13); path.moveTo(10,5); path.lineTo(5,5); path.lineTo(5,19); path.lineTo(19,19); path.lineTo(19,14); }
 else if(name=="copy"){ p.drawRoundedRect(QRectF(8,7.5,12,13),2,2); path.moveTo(15,4); path.lineTo(6,4); path.cubicTo(4.9,4,4,4.9,4,6); path.lineTo(4,16.5); }
 else if(name=="close" || name=="error"){ if(name=="error")p.drawEllipse(QRectF(3,3,18,18));path.moveTo(8,8);path.lineTo(16,16);path.moveTo(16,8);path.lineTo(8,16); }
 else if(name=="warning"){ path.moveTo(12,3.5);path.lineTo(21.5,20);path.lineTo(2.5,20);path.closeSubpath();p.drawLine(12,9.5,12,13.5);p.drawPoint(12,17); }
 else if(name=="info"){ p.drawEllipse(QRectF(3,3,18,18));p.drawLine(12,11,12,17);p.drawPoint(12,7.5); }
 else if(name=="play"){ path.moveTo(8,5);path.lineTo(19,12);path.lineTo(8,19);path.closeSubpath(); }
 else if(name=="pause"){ p.drawLine(9,5.5,9,18.5); p.drawLine(15,5.5,15,18.5); }
 else if(name=="trash"){ p.drawLine(4,7,20,7); path.moveTo(9,7);path.lineTo(9,4.5);path.lineTo(15,4.5);path.lineTo(15,7); path.moveTo(6,7);path.lineTo(7,20);path.lineTo(17,20);path.lineTo(18,7); p.drawLine(10,11,10,16); p.drawLine(14,11,14,16); }
 else if(name=="plus"){ p.drawLine(12,5,12,19); p.drawLine(5,12,19,12); }
 else if(name=="minus"){ p.drawLine(5,12,19,12); }
 else if(name=="check"){ path.moveTo(5,12.5);path.lineTo(10,17);path.lineTo(19,7.5); }
 else if(name=="star"){ path.moveTo(12,3.5);path.lineTo(14.6,9);path.lineTo(20.5,9.6);path.lineTo(16,13.6);path.lineTo(17.3,19.5);path.lineTo(12,16.5);path.lineTo(6.7,19.5);path.lineTo(8,13.6);path.lineTo(3.5,9.6);path.lineTo(9.4,9);path.closeSubpath(); }
 else if(name=="filter"){ path.moveTo(4,5);path.lineTo(20,5);path.lineTo(14,12.5);path.lineTo(14,19);path.lineTo(10,17);path.lineTo(10,12.5);path.closeSubpath(); }
 else if(name=="import"){ path.moveTo(12,3.5);path.lineTo(12,14);path.moveTo(8,10);path.lineTo(12,14);path.lineTo(16,10);path.moveTo(5,13);path.lineTo(5,20);path.lineTo(19,20);path.lineTo(19,13); }
 else if(name=="export"){ path.moveTo(12,14);path.lineTo(12,3.5);path.moveTo(8,7.5);path.lineTo(12,3.5);path.lineTo(16,7.5);path.moveTo(5,13);path.lineTo(5,20);path.lineTo(19,20);path.lineTo(19,13); }
 else if(name=="pulse"){ path.moveTo(3,12);path.lineTo(7,12);path.lineTo(10,5);path.lineTo(14,19);path.lineTo(17,12);path.lineTo(21,12); }
 else if(name=="sun"){ p.drawEllipse(QPointF(12,12),4,4); for(int i=0;i<8;++i){ const QLineF ray=QLineF::fromPolar(7,i*45.0).translated(12,12); p.drawLine(ray.p2(),QLineF::fromPolar(9.5,i*45.0).translated(12,12).p2()); } }
 else if(name=="moon"){ path.moveTo(19.5,14.5); path.cubicTo(18,18.5,14.5,20.5,11,20); path.cubicTo(6.5,19.5,3.5,15.5,4,11); path.cubicTo(4.5,7,7.5,4.2,11,3.8); path.cubicTo(8.8,6.5,8.8,11,11.3,13.6); path.cubicTo(13.6,16,17,16.4,19.5,14.5); }
 else if(name=="monitor"){ p.drawRoundedRect(QRectF(3,4,18,12.5),2,2); p.drawLine(QPointF(12,16.5),QPointF(12,20)); p.drawLine(QPointF(8,20),QPointF(16,20)); }
 p.drawPath(path);
}

QImage roundedCover(const QImage &source, const QSize &size, int radius)
{
    if (source.isNull() || size.isEmpty())
        return source;
    const QImage scaled = source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QPoint offset((scaled.width() - size.width()) / 2, (scaled.height() - size.height()) / 2);
    QImage result(size, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(size)), radius, radius);
    painter.setClipPath(path);
    painter.drawImage(QPoint(0, 0), scaled, QRect(offset, size));
    return result;
}

class CoverResponse final : public QQuickImageResponse {
public:
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(m_image); }
    // A missing cover is normal, not an error to log: it arrives as a single clear pixel.
    void finish(const QImage &image)
    {
        m_image = image;
        if (m_image.isNull()) {
            m_image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            m_image.fill(Qt::transparent);
        }
        emit finished();
    }

private:
    QImage m_image;
};
}

IconProvider::IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString name = id.section(QLatin1Char('/'), 0, 0);
    const QColor color(QLatin1Char('#') + id.section(QLatin1Char('/'), 1, 1));
    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(24, 24);
    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.scale(target.width() / 24.0, target.height() / 24.0);
    drawGlyph(painter, name, color);
    if (size) *size = target;
    return image;
}

CoverProvider::CoverProvider(ImageLoader *images) : m_images(images) {}

QQuickImageResponse *CoverProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // The reader keeps a response alive until it reports finished, even if cancelled.
    auto *response = new CoverResponse;
    const int radius = id.section(QLatin1Char('/'), 0, 0).toInt();
    const QString url = QUrl::fromPercentEncoding(id.section(QLatin1Char('/'), 1).toUtf8());
    // ImageLoader lives on the UI thread; this call does not.
    QMetaObject::invokeMethod(qApp, [images = m_images, url, response, requestedSize, radius] {
        if (!images || url.isEmpty()) {
            response->finish({});
            return;
        }
        images->request(url, [response, requestedSize, radius](QImage image) {
            QThreadPool::globalInstance()->start([response, image, requestedSize, radius] {
                response->finish(roundedCover(image, requestedSize, radius));
            });
        });
    });
    return response;
}

QIcon appIcon()
{
    const QImage logo(QStringLiteral(":/qt/qml/MikuLibrary/assets/logo.png"));
    QIcon result;
    for (const int size : {16, 20, 24, 32, 40, 48, 64, 128, 256})
        result.addPixmap(QPixmap::fromImage(logo.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    return result;
}

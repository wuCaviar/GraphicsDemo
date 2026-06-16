#ifndef VIEWCONVERTER_H
#define VIEWCONVERTER_H

#include "qruler_export.h"

#include <QtGlobal>

class QPointF;
class QRectF;
class QSizeF;
class QTransform;

/**
 * Simple view converter that maps between document coordinates (pt) and view
 * coordinates (pixels) using a uniform zoom level.
 *
 * This is a simplified standalone replacement for KoViewConverter.
 */
class QRULER_EXPORT ViewConverter
{
public:
    ViewConverter();
    virtual ~ViewConverter() {}

    virtual QPointF documentToView(const QPointF &documentPoint) const;
    virtual QPointF viewToDocument(const QPointF &viewPoint) const;
    virtual QRectF documentToView(const QRectF &documentRect) const;
    virtual QRectF viewToDocument(const QRectF &viewRect) const;
    virtual QSizeF documentToView(const QSizeF &documentSize) const;
    virtual QSizeF viewToDocument(const QSizeF &viewSize) const;

    virtual qreal documentToViewX(qreal documentX) const;
    virtual qreal documentToViewY(qreal documentY) const;
    virtual qreal viewToDocumentX(qreal viewX) const;
    virtual qreal viewToDocumentY(qreal viewY) const;

    virtual void zoom(qreal *zoomX, qreal *zoomY) const;
    virtual void setZoom(qreal zoom);
    qreal zoom() const;

    QTransform documentToView() const;
    QTransform viewToDocument() const;

private:
    qreal m_zoomLevel; // 1.0 is 100%
};

#endif // VIEWCONVERTER_H

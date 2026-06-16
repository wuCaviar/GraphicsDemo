#include "ViewConverter.h"

#include <QPointF>
#include <QRectF>
#include <QTransform>

ViewConverter::ViewConverter()
    : m_zoomLevel(1.0)
{
}

QPointF ViewConverter::documentToView(const QPointF &documentPoint) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return documentPoint;
    return QPointF(documentToViewX(documentPoint.x()), documentToViewY(documentPoint.y()));
}

QPointF ViewConverter::viewToDocument(const QPointF &viewPoint) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return viewPoint;
    return QPointF(viewToDocumentX(viewPoint.x()), viewToDocumentY(viewPoint.y()));
}

QRectF ViewConverter::documentToView(const QRectF &documentRect) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return documentRect;
    return QRectF(documentToView(documentRect.topLeft()), documentToView(documentRect.size()));
}

QRectF ViewConverter::viewToDocument(const QRectF &viewRect) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return viewRect;
    return QRectF(viewToDocument(viewRect.topLeft()), viewToDocument(viewRect.size()));
}

QSizeF ViewConverter::documentToView(const QSizeF &documentSize) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return documentSize;
    return QSizeF(documentToViewX(documentSize.width()), documentToViewY(documentSize.height()));
}

QSizeF ViewConverter::viewToDocument(const QSizeF &viewSize) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return viewSize;
    return QSizeF(viewToDocumentX(viewSize.width()), viewToDocumentY(viewSize.height()));
}

void ViewConverter::zoom(qreal *zoomX, qreal *zoomY) const
{
    *zoomX = m_zoomLevel;
    *zoomY = m_zoomLevel;
}

qreal ViewConverter::documentToViewX(qreal documentX) const
{
    return documentX * m_zoomLevel;
}

qreal ViewConverter::documentToViewY(qreal documentY) const
{
    return documentY * m_zoomLevel;
}

qreal ViewConverter::viewToDocumentX(qreal viewX) const
{
    return viewX / m_zoomLevel;
}

qreal ViewConverter::viewToDocumentY(qreal viewY) const
{
    return viewY / m_zoomLevel;
}

void ViewConverter::setZoom(qreal zoom)
{
    if (qFuzzyCompare(zoom, qreal(0.0)) || qFuzzyCompare(zoom, qreal(1.0))) {
        zoom = 1;
    }
    m_zoomLevel = zoom;
}

qreal ViewConverter::zoom() const
{
    return m_zoomLevel;
}

QTransform ViewConverter::documentToView() const
{
    qreal zoomX, zoomY;
    zoom(&zoomX, &zoomY);
    return QTransform::fromScale(zoomX, zoomY);
}

QTransform ViewConverter::viewToDocument() const
{
    qreal zoomX, zoomY;
    zoom(&zoomX, &zoomY);
    return QTransform::fromScale(1.0 / zoomX, 1.0 / zoomY);
}

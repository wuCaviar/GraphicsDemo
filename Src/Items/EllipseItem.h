#ifndef ELLIPSEITEM_H
#define ELLIPSEITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsEllipseItem>

class EllipseItem : public QGraphicsEllipseItem, public IGraphicsItem
{
public:
    enum { Type = UserType + EllipseItemType };

    explicit EllipseItem(QGraphicsItem *parent = nullptr);
    EllipseItem(const QRectF &rect, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return EllipseItemType; }
    PropertyFlags propertyFlags() const override { return HasPen | HasBrush | HasRotation; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return pen(); }
    void setItemPen(const QPen &p) override { setPen(p); }
    QBrush itemBrush() const override { return brush(); }
    void setItemBrush(const QBrush &b) override { setBrush(b); }

    // CMYK 颜色存储
    void setItemPenCmyk(double c, double m, double y, double k) override { m_penCmyk = {c, m, y, k, true}; }
    bool hasPenCmyk() const override { return m_penCmyk.valid; }
    void penCmyk(double &c, double &m, double &y, double &k) const override { c = m_penCmyk.c; m = m_penCmyk.m; y = m_penCmyk.y; k = m_penCmyk.k; }
    void clearPenCmyk() override { m_penCmyk.valid = false; }
    void setItemBrushCmyk(double c, double m, double y, double k) override { m_brushCmyk = {c, m, y, k, true}; }
    bool hasBrushCmyk() const override { return m_brushCmyk.valid; }
    void brushCmyk(double &c, double &m, double &y, double &k) const override { c = m_brushCmyk.c; m = m_brushCmyk.m; y = m_brushCmyk.y; k = m_brushCmyk.k; }
    void clearBrushCmyk() override { m_brushCmyk.valid = false; }

    // 精确几何矩形 — 返回不含画笔边距的 rect()
    QRectF geometryRect() const override { return rect(); }
    bool supportsGeometryRect() const override { return true; }
    void setGeometryRect(const QRectF &r) override { setRect(r); }
    bool supportsSetGeometryRect() const override { return true; }

    void serialize(QDataStream &out) const override;
    bool deserialize(QDataStream &in) override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

private:
    CmykColor m_penCmyk;
    CmykColor m_brushCmyk;
};

#endif // ELLIPSEITEM_H

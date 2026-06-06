#ifndef TEXTITEM_H
#define TEXTITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsTextItem>

class TextItem
    : public QGraphicsTextItem
    , public IGraphicsItem
{
    Q_OBJECT

public:
    enum
    {
        Type = UserType + TextItemType
    };

    explicit TextItem(QGraphicsItem *parent = nullptr);
    TextItem(const QString &text, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return TextItemType; }
    PropertyFlags propertyFlags() const override
    {
        return HasFont | HasText | HasRotation;
    }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override;
    void setItemPen(const QPen &pen) override;
    QBrush itemBrush() const override { return Qt::NoBrush; }
    void setItemBrush(const QBrush &) override { }

    // CMYK 颜色存储（仅笔/文字颜色）
    void setItemPenCmyk(double c, double m, double y, double k) override
    {
        m_penCmyk = { c, m, y, k, true };
    }
    bool hasPenCmyk() const override { return m_penCmyk.valid; }
    void penCmyk(double &c, double &m, double &y, double &k) const override
    {
        c = m_penCmyk.c;
        m = m_penCmyk.m;
        y = m_penCmyk.y;
        k = m_penCmyk.k;
    }
    void clearPenCmyk() override { m_penCmyk.valid = false; }

    QString text() const override;
    void setText(const QString &text) override;
    QFont itemFont() const override;
    void setItemFont(const QFont &font) override;

    // 精确几何矩形 — 始终返回文字自然包围盒
    QRectF geometryRect() const override;
    bool supportsGeometryRect() const override { return true; }
    // 拖拽句柄缩放：字号等比缩放 + 设置文本宽度启用自动换行
    void setGeometryRect(const QRectF &rect) override;
    bool supportsSetGeometryRect() const override { return true; }

    // 文本宽度：<=0 为点文本（无换行），>0 为区域文本（自动换行）
    qreal textWidth() const { return m_textWidth; }
    void setTextWidth(qreal w);

    void serialize(QDataStream &out) const override;
    bool deserialize(QDataStream &in) override;

    // 禁用默认文本交互，改由属性面板编辑
    void enableEditing(bool on);

signals:
    void editingFinished();

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    bool m_editing = false;
    CmykColor m_penCmyk;
    qreal m_textWidth = -1; // <=0 = 点文本（无换行），>0 = 区域文本宽度

    // 获取当前字号的点数值
    qreal currentFontSize() const;
    void applyTextWidth();
};

#endif // TEXTITEM_H

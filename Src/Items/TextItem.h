#ifndef TEXTITEM_H
#define TEXTITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsTextItem>

class TextItem : public QGraphicsTextItem, public IGraphicsItem
{
    Q_OBJECT

public:
    enum { Type = UserType + TextItemType };

    explicit TextItem(QGraphicsItem *parent = nullptr);
    TextItem(const QString &text, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return TextItemType; }
    PropertyFlags propertyFlags() const override { return HasFont | HasText | HasRotation; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override;
    void setItemPen(const QPen &pen) override;
    QBrush itemBrush() const override;
    void setItemBrush(const QBrush &brush) override;

    QString text() const override;
    void setText(const QString &text) override;
    QFont itemFont() const override;
    void setItemFont(const QFont &font) override;

    // 矩形尺寸（支持拖拽缩放）
    QRectF rect() const;
    void setRect(const QRectF &rect);

    // 重写 boundingRect 以返回 m_rect（若有）或自然文本尺寸
    QRectF boundingRect() const override;

    // 精确几何矩形 — 返回 m_rect 或自然文本包围
    QRectF geometryRect() const override;
    bool supportsGeometryRect() const override { return true; }

    void serialize(QDataStream &out) const override;
    void deserialize(QDataStream &in) override;

    // 禁用默认文本交互，改由属性面板编辑
    void enableEditing(bool on);

signals:
    void editingFinished();

protected:
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QBrush m_bgBrush;   // 背景画刷
    QRectF m_rect;       // 自定义包围矩形（由缩放手柄设置）
    bool m_editing = false;
};

#endif // TEXTITEM_H

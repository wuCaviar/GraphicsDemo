#include "TextItem.h"
#include <QPainter>
#include <QTextDocument>

TextItem::TextItem(QGraphicsItem *parent) : QGraphicsTextItem(parent)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(ItemIsFocusable, false);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

TextItem::TextItem(const QString &text, QGraphicsItem *parent)
    : QGraphicsTextItem(text, parent)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(ItemIsFocusable, false);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

QGraphicsItem *TextItem::cloneItem() const
{
    auto *item = new TextItem(toPlainText());
    item->setFont(font());
    item->setDefaultTextColor(defaultTextColor());
    item->setItemBrush(m_bgBrush);
    item->setPos(pos());
    item->setRotation(rotation());
    if (m_rect.isValid())
        item->setRect(m_rect);
    return item;
}

QPen TextItem::itemPen() const
{
    return QPen(defaultTextColor());
}

void TextItem::setItemPen(const QPen &pen)
{
    setDefaultTextColor(pen.color());
}

QBrush TextItem::itemBrush() const { return m_bgBrush; }

void TextItem::setItemBrush(const QBrush &brush)
{
    m_bgBrush = brush;
    update();
}

QString TextItem::text() const { return toPlainText(); }

void TextItem::setText(const QString &t)
{
    setPlainText(t);
    // 文本变更时，若 m_rect 已设置，保持宽度不变、更新高度
    if (m_rect.isValid()) {
        qreal w = m_rect.width();
        document()->setTextWidth(w);
        qreal h = document()->size().height();
        prepareGeometryChange();
        m_rect.setHeight(h);
        update();
    }
}

QFont TextItem::itemFont() const { return font(); }

void TextItem::setItemFont(const QFont &f)
{
    setFont(f);
    // 字体变更时，若 m_rect 已设置，保持宽度不变、更新高度
    if (m_rect.isValid()) {
        qreal w = m_rect.width();
        document()->setTextWidth(w);
        qreal h = document()->size().height();
        prepareGeometryChange();
        m_rect.setHeight(h);
        update();
    }
}

QRectF TextItem::rect() const
{
    return m_rect.isValid() ? m_rect : QGraphicsTextItem::boundingRect();
}

void TextItem::setRect(const QRectF &rect)
{
    prepareGeometryChange();
    m_rect = rect;
    // 设置文本宽度以触发重排
    if (rect.width() > 0)
        document()->setTextWidth(rect.width());
    update();
}

QRectF TextItem::boundingRect() const
{
    return m_rect.isValid() ? m_rect : QGraphicsTextItem::boundingRect();
}

QRectF TextItem::geometryRect() const
{
    // TextItem 没有画笔边距问题，geometryRect 与 boundingRect 一致
    return boundingRect();
}

QPainterPath TextItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

void TextItem::enableEditing(bool on)
{
    m_editing = on;
    if (on) {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setFlag(ItemIsFocusable, true);
        setFocus();
    } else {
        setTextInteractionFlags(Qt::NoTextInteraction);
        setFlag(ItemIsFocusable, false);
    }
}

void TextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // 先绘制背景
    if (m_bgBrush != Qt::NoBrush) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_bgBrush);
        painter->drawRect(boundingRect());
    }

    // 绘制文本
    QGraphicsTextItem::paint(painter, option, widget);
}

void TextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    enableEditing(true);
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void TextItem::focusOutEvent(QFocusEvent *event)
{
    if (m_editing) {
        enableEditing(false);
        emit editingFinished();
    }
    QGraphicsTextItem::focusOutEvent(event);
}

void TextItem::serialize(QDataStream &out) const
{
    out << toPlainText() << defaultTextColor() << font() << m_bgBrush << pos() << rotation()
        << m_rect;
}

void TextItem::deserialize(QDataStream &in)
{
    QString txt;
    QColor color;
    QFont f;
    QBrush bg;
    qreal rot;
    QPointF pos_;
    QRectF r;
    in >> txt >> color >> f >> bg >> pos_ >> rot >> r;
    setPlainText(txt);
    setDefaultTextColor(color);
    setFont(f);
    m_bgBrush = bg;
    setPos(pos_);
    setRotation(rot);
    if (r.isValid())
        setRect(r);
}

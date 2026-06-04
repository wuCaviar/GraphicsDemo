#include "TextItem.h"
#include "ColorUtils.h"

#include <QPainter>
#include <QTextDocument>

TextItem::TextItem(QGraphicsItem *parent) : QGraphicsTextItem(parent)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(ItemIsFocusable, false);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);

    m_bgBrush = QBrush(Qt::white);
}

TextItem::TextItem(const QString &text, QGraphicsItem *parent)
    : QGraphicsTextItem(text, parent)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(ItemIsFocusable, false);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);

    m_bgBrush = QBrush(Qt::white);
}

QGraphicsItem *TextItem::cloneItem() const
{
    auto *item = new TextItem(toPlainText());
    item->setFont(font());
    item->setDefaultTextColor(defaultTextColor());
    item->setItemBrush(m_bgBrush);
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    item->setTransformOriginPoint(transformOriginPoint());
    if (m_rect.isValid())
        item->setRect(m_rect);
    // 复制缩放状态
    item->m_originalRect = m_originalRect;
    item->m_originalFontSize = m_originalFontSize;
    // 复制 CMYK 颜色存储
    if (m_penCmyk.valid)
        item->setItemPenCmyk(m_penCmyk.c, m_penCmyk.m, m_penCmyk.y,
                             m_penCmyk.k);
    if (m_brushCmyk.valid)
        item->setItemBrushCmyk(m_brushCmyk.c, m_brushCmyk.m, m_brushCmyk.y,
                               m_brushCmyk.k);
    if (!m_gradientCmyk.isEmpty())
        item->setGradientStopCmykMap(m_gradientCmyk);
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

QBrush TextItem::itemBrush() const
{
    return m_bgBrush;
}

void TextItem::setItemBrush(const QBrush &brush)
{
    m_bgBrush = brush;
    update();
}

QString TextItem::text() const
{
    return toPlainText();
}

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

QFont TextItem::itemFont() const
{
    return font();
}

void TextItem::setItemFont(const QFont &f)
{
    setFont(f);

    // 更新原始字体大小和矩形，用于后续缩放计算
    m_originalFontSize = f.pointSizeF() > 0 ? f.pointSizeF() : f.pixelSize();
    if (m_originalFontSize <= 0)
        m_originalFontSize = 12.0;
    if (m_rect.isValid())
        m_originalRect = m_rect;

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

void TextItem::updateFontScale()
{
    if (!m_originalRect.isValid() || m_originalRect.width() <= 0
        || !m_rect.isValid() || m_rect.width() <= 0)
        return;
    if (m_originalFontSize <= 0)
        return;

    // 基于宽度比例缩放字体
    qreal widthRatio = m_rect.width() / m_originalRect.width();
    qreal newFontSize = m_originalFontSize * widthRatio;

    // 限制最小字体大小
    if (newFontSize < 1.0)
        newFontSize = 1.0;

    // 应用新的字体大小
    QFont f = font();
    if (f.pointSizeF() > 0)
        f.setPointSizeF(newFontSize);
    else
        f.setPixelSize(static_cast<int>(newFontSize));

    // 临时阻塞信号，避免setFont触发不必要的更新
    blockSignals(true);
    setFont(f);
    blockSignals(false);
}

void TextItem::setRect(const QRectF &rect)
{
    prepareGeometryChange();

    // 首次设置矩形时，记录初始状态
    if (!m_rect.isValid() || m_originalFontSize <= 0) {
        m_originalRect = rect;
        QFont f = font();
        m_originalFontSize =
            f.pointSizeF() > 0 ? f.pointSizeF() : f.pixelSize();
        if (m_originalFontSize <= 0)
            m_originalFontSize = 12.0; // 默认字体大小
    }

    m_rect = rect;

    // 更新字体缩放
    updateFontScale();

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

void TextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                     QWidget *widget)
{
    // 先绘制背景
    if (m_bgBrush != Qt::NoBrush) {
        painter->setPen(Qt::NoPen);
        QRectF bgRect = boundingRect();
        painter->setBrush(
            ColorUtils::mapGradientBrushToRect(m_bgBrush, bgRect));
        painter->drawRect(bgRect);
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
    out << toPlainText() << defaultTextColor() << font() << m_bgBrush << pos()
        << rotation() << m_rect;
    writeCmykIfValid(out, m_penCmyk);
    writeCmykIfValid(out, m_brushCmyk);
    writeGradientCmykIfValid(out, m_gradientCmyk);

    // 保存缩放状态
    out << m_originalRect << m_originalFontSize;
}

bool TextItem::deserialize(QDataStream &in)
{
    QString txt;
    QColor color;
    QFont f;
    QBrush bg;
    qreal rot;
    QPointF pos_;
    QRectF r;
    in >> txt >> color >> f >> bg >> pos_ >> rot >> r;
    if (in.status() != QDataStream::Ok)
        return false;
    setPlainText(txt);
    setDefaultTextColor(color);
    setFont(f);
    m_bgBrush = bg;
    setPos(pos_);
    setRotation(rot);
    if (r.isValid())
        setRect(r);
    if (!readCmykIfAvailable(in, in.device(), m_penCmyk))
        return false;
    if (!readCmykIfAvailable(in, in.device(), m_brushCmyk))
        return false;
    if (!readGradientCmykIfAvailable(in, in.device(), m_gradientCmyk))
        return false;

    // 读取缩放状态（向后兼容：如果流未结束则读取）
    if (!in.device()->atEnd()) {
        in >> m_originalRect >> m_originalFontSize;
    } else {
        // 旧版本文件，使用默认值
        m_originalRect = r;
        m_originalFontSize =
            f.pointSizeF() > 0 ? f.pointSizeF() : f.pixelSize();
        if (m_originalFontSize <= 0)
            m_originalFontSize = 12.0;
    }

    return true;
}

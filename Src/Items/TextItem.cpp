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

TextItem::TextItem(const QString &text, QGraphicsItem *parent) : QGraphicsTextItem(text, parent)
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
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    item->setTransformOriginPoint(transformOriginPoint());
    item->m_textWidth = m_textWidth;
    item->applyTextWidth();
    // 复制 CMYK 颜色存储
    if (m_penCmyk.valid)
        item->setItemPenCmyk(m_penCmyk.c, m_penCmyk.m, m_penCmyk.y, m_penCmyk.k);
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

QString TextItem::text() const
{
    return toPlainText();
}

void TextItem::setText(const QString &t)
{
    setPlainText(t);
    applyTextWidth();
}

QFont TextItem::itemFont() const
{
    return font();
}

void TextItem::setItemFont(const QFont &f)
{
    setFont(f);
    applyTextWidth();
}

void TextItem::setTextWidth(qreal w)
{
    if (qAbs(w - m_textWidth) < 0.5)
        return;
    prepareGeometryChange();
    m_textWidth = (w > 0) ? w : -1;
    applyTextWidth();
}

void TextItem::applyTextWidth()
{
    document()->setTextWidth(m_textWidth > 0 ? m_textWidth : -1);
}

QRectF TextItem::geometryRect() const
{
    return QGraphicsTextItem::boundingRect();
}

qreal TextItem::currentFontSize() const
{
    QFont f = font();
    qreal sz = f.pointSizeF();
    if (sz <= 0)
        sz = f.pixelSize();
    if (sz <= 0)
        sz = 12.0;
    return sz;
}

void TextItem::setGeometryRect(const QRectF &newRect)
{
    QRectF current = geometryRect();
    if (current.width() <= 0 || current.height() <= 0)
        return;

    qreal curFs = currentFontSize();
    if (curFs <= 0)
        return;

    // 使用几何平均（对角比例）— 适配顶点句柄的斜向拖拽语义
    qreal scaleW = newRect.width() / current.width();
    qreal scaleH = newRect.height() / current.height();
    qreal scale = qSqrt(scaleW * scaleH);

    qreal newFs = curFs * scale;
    if (newFs < 1.0)
        newFs = 1.0;
    if (qAbs(newFs - curFs) < 0.05 && qAbs(newRect.width() - m_textWidth) < 0.5)
        return; // 无显著变化，跳过以避免更新抖动

    prepareGeometryChange();

    QFont f = font();
    if (f.pointSizeF() > 0)
        f.setPointSizeF(newFs);
    else
        f.setPixelSize(static_cast<int>(newFs));
    setFont(f);

    // 设置文本宽度以启用自动换行
    m_textWidth = newRect.width();
    applyTextWidth();
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
    // v4 格式：无 m_rect，无背景画刷，无 brush/gradient CMYK，无缩放状态
    //       boundingRect 始终 = 文字自然尺寸，字号决定一切
    out << static_cast<quint8>(4);
    out << toPlainText() << defaultTextColor() << font() << pos() << rotation();
    writeCmykIfValid(out, m_penCmyk);
    out << m_textWidth;
}

bool TextItem::deserialize(QDataStream &in)
{
    qint64 startPos = in.device()->pos();
    quint8 version;
    in >> version;
    if (in.status() != QDataStream::Ok)
        return false;

    QString txt;
    QColor color;
    QFont f;
    QBrush bg; // 兼容旧格式，读取后丢弃
    qreal rot;
    QPointF pos_;
    QRectF r; // 兼容旧格式 m_rect，读取后丢弃

    if (version == 4) {
        // v4：纯文字数据，无 rect / brush / gradient / scale
        in >> txt >> color >> f >> pos_ >> rot;
        if (in.status() != QDataStream::Ok)
            return false;
        setPlainText(txt);
        setDefaultTextColor(color);
        setFont(f);
        setPos(pos_);
        setRotation(rot);
        if (!readCmykIfAvailable(in, in.device(), m_penCmyk))
            return false;
        // 读取 m_textWidth（v4 新增字段）
        if (!in.device()->atEnd()) {
            in >> m_textWidth;
        }
        applyTextWidth();
    } else if (version == 3) {
        // v3：有 m_rect + scale state，无 brush/gradient。
        //     从 m_rect.width 恢复文本宽度
        in >> txt >> color >> f >> bg >> pos_ >> rot >> r;
        if (in.status() != QDataStream::Ok)
            return false;
        Q_UNUSED(bg);
        setPlainText(txt);
        setDefaultTextColor(color);
        setFont(f);
        setPos(pos_);
        setRotation(rot);
        m_textWidth = r.isValid() && r.width() > 0 ? r.width() : -1;
        applyTextWidth();
        if (!readCmykIfAvailable(in, in.device(), m_penCmyk))
            return false;
        // 跳过 scale state（m_originalRect + m_originalFontSize）
        if (!in.device()->atEnd()) {
            QRectF dummyRect;
            qreal dummyFs;
            in >> dummyRect >> dummyFs;
        }
    } else {
        // v2 及更早：有背景画刷 + brushCMYK + gradientCMYK + rect + scale
        in.device()->seek(startPos);
        in.setStatus(QDataStream::Ok);
        in >> txt >> color >> f >> bg >> pos_ >> rot >> r;
        if (in.status() != QDataStream::Ok)
            return false;
        Q_UNUSED(bg);
        setPlainText(txt);
        setDefaultTextColor(color);
        setFont(f);
        setPos(pos_);
        setRotation(rot);
        m_textWidth = r.isValid() && r.width() > 0 ? r.width() : -1;
        applyTextWidth();
        if (!readCmykIfAvailable(in, in.device(), m_penCmyk))
            return false;
        // 跳过 brushCMYK
        {
            CmykColor dummy;
            if (!readCmykIfAvailable(in, in.device(), dummy))
                return false;
        }
        // 跳过 gradientCMYK
        {
            QMap<double, CmykColor> dummy;
            if (!readGradientCmykIfAvailable(in, in.device(), dummy))
                return false;
        }
        // 跳过 scale state
        if (!in.device()->atEnd()) {
            QRectF dummyRect;
            qreal dummyFs;
            in >> dummyRect >> dummyFs;
        }
    }

    return true;
}

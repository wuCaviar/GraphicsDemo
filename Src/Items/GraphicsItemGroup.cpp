#include "GraphicsItemGroup.h"

#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>

GraphicsItemGroup::GraphicsItemGroup(QGraphicsItem *parent) : QGraphicsItemGroup(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

void GraphicsItemGroup::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                              QWidget *widget)
{
    // 去掉 State_Selected 以禁用 Qt 内置的选中虚线框
    // 选中框由 ResizeHandleItem 统一绘制，避免两层叠加及残影
    QStyleOptionGraphicsItem opt = *option;
    opt.state &= ~QStyle::State_Selected;
    QGraphicsItemGroup::paint(painter, &opt, widget);
}

QGraphicsItem *GraphicsItemGroup::cloneItem() const
{
    auto *group = new GraphicsItemGroup;
    group->setPos(pos());
    group->setRotation(rotation());
    group->setTransform(transform());
    group->setTransformOriginPoint(transformOriginPoint());

    // 深拷贝子图元
    for (auto *child : childItems()) {
        auto *gi = dynamic_cast<IGraphicsItem *>(child);
        if (!gi)
            continue;
        QGraphicsItem *clone = gi->cloneItem();
        if (clone) {
            // cloneItem 拷贝了子图元的 pos()（组本地坐标）
            // 先转为场景坐标，addToGroup 会自动转换回新组的本地坐标
            clone->setPos(mapToScene(child->pos()));
            group->addToGroup(clone);
        }
    }
    return group;
}

bool GraphicsItemGroup::containsImageItem() const
{
    for (auto *child : childItems()) {
        auto *igi = dynamic_cast<IGraphicsItem *>(child);
        if (igi && !igi->isResizable())
            return true;
        // 递归检查嵌套组
        auto *nestedGroup = dynamic_cast<GraphicsItemGroup *>(child);
        if (nestedGroup && nestedGroup->containsImageItem())
            return true;
    }
    return false;
}

void GraphicsItemGroup::setGeometryRect(const QRectF &newRect)
{
    QRectF oldRect = geometryRect();
    if (oldRect.isEmpty() || newRect.isEmpty())
        return;

    prepareGeometryChange();

    qreal sx = newRect.width() / oldRect.width();
    qreal sy = newRect.height() / oldRect.height();

    for (auto *child : childItems()) {
        auto *igi = dynamic_cast<IGraphicsItem *>(child);
        QPointF childPos = child->pos();
        QRectF childRect =
            (igi && igi->supportsGeometryRect()) ? igi->geometryRect() : child->boundingRect();

        // 相对于 oldRect 左上角的位置，按比例缩放
        qreal relX = childPos.x() + childRect.left() - oldRect.left();
        qreal relY = childPos.y() + childRect.top() - oldRect.top();
        qreal newLeft = newRect.left() + relX * sx;
        qreal newTop = newRect.top() + relY * sy;

        if (igi && igi->supportsSetGeometryRect()) {
            QSizeF newSize(childRect.width() * sx, childRect.height() * sy);
            QRectF scaledRect(childRect.topLeft(), newSize);
            igi->setGeometryRect(scaledRect);
            child->setPos(QPointF(newLeft - childRect.left(), newTop - childRect.top()));
        } else {
            QSizeF newSize(childRect.width() * sx, childRect.height() * sy);
            QSizeF origSize = childRect.size();
            if (origSize.width() > 0 && origSize.height() > 0) {
                qreal itemSx = newSize.width() / origSize.width();
                qreal itemSy = newSize.height() / origSize.height();
                child->setTransform(QTransform().scale(itemSx, itemSy));
            }
            child->setPos(QPointF(newLeft, newTop));
        }
    }
}

QRectF GraphicsItemGroup::geometryRect() const
{
    // 返回子图元在 group 本地坐标下的联合包围矩形
    QRectF united;
    for (auto *child : childItems()) {
        QRectF childLocal = child->mapRectToParent(child->boundingRect());
        if (united.isEmpty())
            united = childLocal;
        else
            united = united.united(childLocal);
    }
    return united;
}

void GraphicsItemGroup::serialize(QDataStream &out) const
{
    // 写入子图元数量
    QList<QGraphicsItem *> children = childItems();
    out << (int)children.size();

    for (auto *child : children) {
        auto *gi = dynamic_cast<IGraphicsItem *>(child);
        if (!gi) {
            // 写入无效标记
            out << static_cast<int>(-1);
            continue;
        }
        out << static_cast<int>(gi->itemType());
        gi->serialize(out);
    }

    out << pos() << rotation();
}

bool GraphicsItemGroup::deserialize(QDataStream &in)
{
    int childCount = 0;
    in >> childCount;
    if (in.status() != QDataStream::Ok)
        return false;

    // ---- 第一阶段：反序列化所有子图元，暂存到临时列表 ----
    // 不能立即 addToGroup，因为组的 pos/rotation 尚未读取，
    // addToGroup 内部的 mapFromScene 依赖组的 transform，会算错坐标。
    QList<QGraphicsItem *> pendingChildren;
    for (int i = 0; i < childCount; ++i) {
        int typeInt = 0;
        in >> typeInt;
        if (typeInt < 0) {
            // 无效标记，跳过
            continue;
        }

        auto type = static_cast<IGraphicsItem::ItemType>(typeInt);
        auto *gi = createItemByType(type);
        if (!gi)
            return false;

        if (!gi->deserialize(in)) {
            delete gi;
            return false;
        }

        auto *item = dynamic_cast<QGraphicsItem *>(gi);
        if (item)
            pendingChildren.append(item);
        else
            delete gi;
    }

    // ---- 读取组的位置和旋转 ----
    QPointF p;
    qreal rot = 0;
    in >> p >> rot;
    if (in.status() != QDataStream::Ok)
        return false;

    // ---- 第二阶段：先设置组的 transform，再将子图元加入组 ----
    // addToGroup 假设子图元的 pos 是场景坐标，执行 mapFromScene 转换为组本地坐标。
    // 子图元 deserialize 设置的 pos 是组本地坐标，需先用 mapToScene 转为场景坐标。
    setPos(p);
    setRotation(rot);

    for (auto *item : pendingChildren) {
        QPointF localPos = item->pos();
        QPointF scenePos = mapToScene(localPos);
        item->setPos(scenePos);
        addToGroup(item);
    }

    return true;
}

void GraphicsItemGroup::addChildFromScene(QGraphicsItem *child)
{
    if (!child)
        return;

    // 从父级/场景中移除
    if (child->scene())
        child->scene()->removeItem(child);

    // addToGroup 自动处理坐标变换：将场景坐标转为组本地坐标
    addToGroup(child);
}

QList<QGraphicsItem *> GraphicsItemGroup::childGraphicsItems() const
{
    return childItems();
}

void GraphicsItemGroup::extractChildren()
{
    // removeFromGroup 自动将子图元坐标从组本地转为场景坐标（保持视觉位置不变）
    // 无需手动 setPos/mapToScene
    auto children = childItems();
    for (auto *child : children)
        removeFromGroup(child);
}

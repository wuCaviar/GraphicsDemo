#include "GraphicsItemGroup.h"

#include <QGraphicsScene>
#include <QPainter>

GraphicsItemGroup::GraphicsItemGroup(QGraphicsItem *parent)
    : QGraphicsItemGroup(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
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
            // cloneItem 返回的是场景坐标下的独立图元
            // 需要转为 group 的本地坐标
            clone->setPos(mapFromScene(clone->pos()));
            group->addToGroup(clone);
        }
    }
    return group;
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
    out << children.size();

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

    for (int i = 0; i < childCount; ++i) {
        int typeInt = 0;
        in >> typeInt;
        if (typeInt < 0) {
            // 无效标记，跳过
            continue;
        }

        // 对于嵌套组，需要递归创建
        auto type = static_cast<IGraphicsItem::ItemType>(typeInt);
        auto *gi = createItemByType(type);
        if (!gi) {
            return false;
        }

        if (!gi->deserialize(in)) {
            delete gi;
            return false;
        }

        auto *item = dynamic_cast<QGraphicsItem *>(gi);
        if (item)
            addToGroup(item);
        else
            delete gi;
    }

    QPointF p;
    qreal rot = 0;
    in >> p >> rot;
    if (in.status() != QDataStream::Ok)
        return false;

    setPos(p);
    setRotation(rot);
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
    // 提取所有子图元到场景坐标，并从组中移除
    auto children = childItems();
    for (auto *child : children) {
        // mapToScene 将组本地坐标转为场景坐标
        QPointF scenePos = child->mapToScene(child->pos());
        child->setPos(scenePos);
        removeFromGroup(child);
    }
}

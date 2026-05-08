#include "AlignmentUtils.h"
#include "IGraphicsItem.h"

#include <algorithm>

namespace AlignmentUtils {

// ============================================================
// 精确几何矩形 — 优先使用 IGraphicsItem::geometryRect()
// ============================================================
QRectF itemGeometryRect(const QGraphicsItem *item)
{
    if (!item) return {};
    auto *gi = dynamic_cast<const IGraphicsItem *>(item);
    if (gi && gi->supportsGeometryRect())
        return gi->geometryRect();
    return item->boundingRect();
}

qreal itemGeomWidth(const QGraphicsItem *item)
{
    return itemGeometryRect(item).width();
}

qreal itemGeomHeight(const QGraphicsItem *item)
{
    return itemGeometryRect(item).height();
}

// ============================================================
// 对齐方向名称
// ============================================================
QString alignDirectionName(AlignDirection dir)
{
    switch (dir) {
    case AlignLeft:    return QStringLiteral("Left");
    case AlignHCenter: return QStringLiteral("HCenter");
    case AlignRight:   return QStringLiteral("Right");
    case AlignTop:     return QStringLiteral("Top");
    case AlignVCenter: return QStringLiteral("VCenter");
    case AlignBottom:  return QStringLiteral("Bottom");
    }
    return QStringLiteral("Unknown");
}

QString distributeDirectionName(DistributeDirection dir)
{
    switch (dir) {
    case DistributeH: return QStringLiteral("Horizontally");
    case DistributeV: return QStringLiteral("Vertically");
    }
    return QStringLiteral("Unknown");
}

// ============================================================
// 对齐算法
// ============================================================
AlignResult computeAlign(const QList<QGraphicsItem *> &items, AlignDirection direction)
{
    AlignResult result;
    if (items.size() < 2) return result;

    // 保存旧位置
    for (auto *item : items)
        result.oldPositions << item->pos();

    switch (direction) {
    case AlignLeft: {
        qreal minLeft = 1e9;
        for (auto *item : items)
            minLeft = qMin(minLeft, item->pos().x() + itemGeometryRect(item).left());
        for (auto *item : items) {
            qreal itemLeft = item->pos().x() + itemGeometryRect(item).left();
            qreal offsetX = minLeft - itemLeft;
            result.newPositions << QPointF(item->pos().x() + offsetX, item->pos().y());
        }
        break;
    }
    case AlignRight: {
        qreal maxRight = -1e9;
        for (auto *item : items)
            maxRight = qMax(maxRight, item->pos().x() + itemGeometryRect(item).right());
        for (auto *item : items) {
            qreal itemRight = item->pos().x() + itemGeometryRect(item).right();
            qreal offsetX = maxRight - itemRight;
            result.newPositions << QPointF(item->pos().x() + offsetX, item->pos().y());
        }
        break;
    }
    case AlignTop: {
        qreal minTop = 1e9;
        for (auto *item : items)
            minTop = qMin(minTop, item->pos().y() + itemGeometryRect(item).top());
        for (auto *item : items) {
            qreal itemTop = item->pos().y() + itemGeometryRect(item).top();
            qreal offsetY = minTop - itemTop;
            result.newPositions << QPointF(item->pos().x(), item->pos().y() + offsetY);
        }
        break;
    }
    case AlignBottom: {
        qreal maxBottom = -1e9;
        for (auto *item : items)
            maxBottom = qMax(maxBottom, item->pos().y() + itemGeometryRect(item).bottom());
        for (auto *item : items) {
            qreal itemBottom = item->pos().y() + itemGeometryRect(item).bottom();
            qreal offsetY = maxBottom - itemBottom;
            result.newPositions << QPointF(item->pos().x(), item->pos().y() + offsetY);
        }
        break;
    }
    case AlignHCenter: {
        qreal minLeft = 1e9, maxRight = -1e9;
        for (auto *item : items) {
            minLeft = qMin(minLeft, item->pos().x() + itemGeometryRect(item).left());
            maxRight = qMax(maxRight, item->pos().x() + itemGeometryRect(item).right());
        }
        qreal centerX = (minLeft + maxRight) / 2.0;
        for (auto *item : items) {
            qreal itemCenterX = item->pos().x() + itemGeometryRect(item).center().x();
            result.newPositions << QPointF(item->pos().x() + (centerX - itemCenterX), item->pos().y());
        }
        break;
    }
    case AlignVCenter: {
        qreal minTop = 1e9, maxBottom = -1e9;
        for (auto *item : items) {
            minTop = qMin(minTop, item->pos().y() + itemGeometryRect(item).top());
            maxBottom = qMax(maxBottom, item->pos().y() + itemGeometryRect(item).bottom());
        }
        qreal centerY = (minTop + maxBottom) / 2.0;
        for (auto *item : items) {
            qreal itemCenterY = item->pos().y() + itemGeometryRect(item).center().y();
            result.newPositions << QPointF(item->pos().x(), item->pos().y() + (centerY - itemCenterY));
        }
        break;
    }
    }

    result.valid = !result.newPositions.isEmpty();
    return result;
}

// ============================================================
// 分布算法
// ============================================================
DistributeResult computeDistribute(const QList<QGraphicsItem *> &items,
                                  DistributeDirection direction,
                                  const DistributeParams &params)
{
    DistributeResult result;
    if (items.size() < 2) return result;

    // 保存旧位置
    for (auto *item : items)
        result.oldPositions << item->pos();

    // 判断是否使用自定义间距（只有当非自动且间距>0时才走自定义路径）
    // 注意：autoSpacing=false + customSpacing=0 意味着间距为0（紧密排列），
    //       也走自定义路径，间距值为0
    bool useCustomSpacing = !params.autoSpacing;

    if (direction == DistributeH) {
        // 按左边缘排序
        QList<QGraphicsItem *> sortedItems = items;
        std::sort(sortedItems.begin(), sortedItems.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().x() + itemGeometryRect(a).left())
                   < (b->pos().x() + itemGeometryRect(b).left());
        });

        QMap<QGraphicsItem *, QPointF> newPosMap;

        if (useCustomSpacing) {
            // 自定义间距：首图元保持原位，后续图元按指定间距依次排列
            qreal currentLeft = sortedItems.first()->pos().x() + itemGeometryRect(sortedItems.first()).left();
            for (int i = 0; i < sortedItems.size(); ++i) {
                qreal itemLeft = sortedItems[i]->pos().x() + itemGeometryRect(sortedItems[i]).left();
                qreal offsetX = currentLeft - itemLeft;
                newPosMap[sortedItems[i]] = QPointF(sortedItems[i]->pos().x() + offsetX,
                                                     sortedItems[i]->pos().y());
                qreal itemWidth = itemGeomWidth(sortedItems[i]);
                currentLeft += itemWidth + params.customSpacing;
            }
        } else {
            // 自动间距：等间隙分布（首末图元保持原位，中间图元按等间隙排列）
            qreal minLeft = sortedItems.first()->pos().x() + itemGeometryRect(sortedItems.first()).left();
            qreal maxRight = sortedItems.last()->pos().x() + itemGeometryRect(sortedItems.last()).right();
            qreal totalWidth = 0;
            for (auto *item : sortedItems)
                totalWidth += itemGeomWidth(item);
            qreal gap = (maxRight - minLeft - totalWidth) / (sortedItems.size() - 1);

            qreal currentLeft = minLeft;
            for (int i = 0; i < sortedItems.size(); ++i) {
                qreal itemLeft = sortedItems[i]->pos().x() + itemGeometryRect(sortedItems[i]).left();
                qreal offsetX = currentLeft - itemLeft;
                newPosMap[sortedItems[i]] = QPointF(sortedItems[i]->pos().x() + offsetX,
                                                     sortedItems[i]->pos().y());
                currentLeft += itemGeomWidth(sortedItems[i]) + gap;
            }
        }

        // 按原始 items 顺序输出结果
        for (auto *item : items)
            result.newPositions << newPosMap.value(item, item->pos());

    } else { // DistributeV
        // 按顶边缘排序
        QList<QGraphicsItem *> sortedItems = items;
        std::sort(sortedItems.begin(), sortedItems.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().y() + itemGeometryRect(a).top())
                   < (b->pos().y() + itemGeometryRect(b).top());
        });

        QMap<QGraphicsItem *, QPointF> newPosMap;

        if (useCustomSpacing) {
            // 自定义间距：首图元保持原位，后续图元按指定间距依次排列
            qreal currentTop = sortedItems.first()->pos().y() + itemGeometryRect(sortedItems.first()).top();
            for (int i = 0; i < sortedItems.size(); ++i) {
                qreal itemTop = sortedItems[i]->pos().y() + itemGeometryRect(sortedItems[i]).top();
                qreal offsetY = currentTop - itemTop;
                newPosMap[sortedItems[i]] = QPointF(sortedItems[i]->pos().x(),
                                                     sortedItems[i]->pos().y() + offsetY);
                qreal itemHeight = itemGeomHeight(sortedItems[i]);
                currentTop += itemHeight + params.customSpacing;
            }
        } else {
            // 自动间距：等间隙分布（首末图元保持原位，中间图元按等间隙排列）
            qreal minTop = sortedItems.first()->pos().y() + itemGeometryRect(sortedItems.first()).top();
            qreal maxBottom = sortedItems.last()->pos().y() + itemGeometryRect(sortedItems.last()).bottom();
            qreal totalHeight = 0;
            for (auto *item : sortedItems)
                totalHeight += itemGeomHeight(item);
            qreal gap = (maxBottom - minTop - totalHeight) / (sortedItems.size() - 1);

            qreal currentTop = minTop;
            for (int i = 0; i < sortedItems.size(); ++i) {
                qreal itemTop = sortedItems[i]->pos().y() + itemGeometryRect(sortedItems[i]).top();
                qreal offsetY = currentTop - itemTop;
                newPosMap[sortedItems[i]] = QPointF(sortedItems[i]->pos().x(),
                                                     sortedItems[i]->pos().y() + offsetY);
                currentTop += itemGeomHeight(sortedItems[i]) + gap;
            }
        }

        // 按原始 items 顺序输出结果
        for (auto *item : items)
            result.newPositions << newPosMap.value(item, item->pos());
    }

    result.valid = !result.newPositions.isEmpty();
    return result;
}

} // namespace AlignmentUtils

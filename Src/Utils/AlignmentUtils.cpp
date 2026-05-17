#include "AlignmentUtils.h"
#include "IGraphicsItem.h"

#include <algorithm>
#include <cmath>

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

QRectF overallGeometryRect(const QList<QGraphicsItem *> &items)
{
    if (items.isEmpty()) return {};
    QRectF r;
    for (auto *item : items) {
        QRectF ir = itemGeometryRect(item);
        ir.translate(item->pos());
        r = r.isNull() ? ir : r.united(ir);
    }
    return r;
}

// ============================================================
// 对齐方向名称
// ============================================================
QString alignDirectionName(AlignDirection dir)
{
    switch (dir) {
    case AlignLeft:         return QStringLiteral("Align Left");
    case AlignHCenter:      return QStringLiteral("Align H-Center");
    case AlignRight:        return QStringLiteral("Align Right");
    case AlignTop:          return QStringLiteral("Align Top");
    case AlignVCenter:      return QStringLiteral("Align V-Center");
    case AlignBottom:       return QStringLiteral("Align Bottom");
    case AlignLeftRight:    return QStringLiteral("Align Left-Right");
    case AlignTopBottom:    return QStringLiteral("Align Top-Bottom");
    case AlignHProportional:return QStringLiteral("Align H-Proportional");
    case AlignVProportional:return QStringLiteral("Align V-Proportional");
    case AlignHCenterOnPage:return QStringLiteral("Center H on Page");
    case AlignVCenterOnPage:return QStringLiteral("Center V on Page");
    }
    return QStringLiteral("Unknown");
}

QString distributeDirectionName(DistributeDirection dir)
{
    switch (dir) {
    case DistributeH:       return QStringLiteral("Distribute H");
    case DistributeV:       return QStringLiteral("Distribute V");
    case DistributeHLeft:   return QStringLiteral("Distribute H-Left");
    case DistributeHCenter: return QStringLiteral("Distribute H-Center");
    case DistributeHRight:  return QStringLiteral("Distribute H-Right");
    case DistributeVTop:    return QStringLiteral("Distribute V-Top");
    case DistributeVCenter: return QStringLiteral("Distribute V-Center");
    case DistributeVBottom: return QStringLiteral("Distribute V-Bottom");
    }
    return QStringLiteral("Unknown");
}

bool isStretchAlign(AlignDirection dir)
{
    return dir == AlignLeftRight || dir == AlignTopBottom
           || dir == AlignHProportional || dir == AlignVProportional;
}

bool isPageCenter(AlignDirection dir)
{
    return dir == AlignHCenterOnPage || dir == AlignVCenterOnPage;
}

bool isHorizontal(AlignDirection dir)
{
    switch (dir) {
    case AlignLeft:
    case AlignHCenter:
    case AlignRight:
    case AlignLeftRight:
    case AlignHProportional:
    case AlignHCenterOnPage:
        return true;
    default:
        return false;
    }
}

bool isHorizontal(DistributeDirection dir)
{
    switch (dir) {
    case DistributeH:
    case DistributeHLeft:
    case DistributeHCenter:
    case DistributeHRight:
        return true;
    default:
        return false;
    }
}

// ============================================================
// 对齐算法
// ============================================================
AlignResult computeAlign(const QList<QGraphicsItem *> &items, AlignDirection direction)
{
    AlignResult result;
    if (items.size() < 2) return result;

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
    default:
        break; // 其他类型由 computeStretchAlign / computePageCenter 处理
    }

    result.valid = !result.newPositions.isEmpty();
    return result;
}

// ============================================================
// 拉伸对齐算法 — 同时改变位置和大小
// ============================================================
StretchAlignResult computeStretchAlign(const QList<QGraphicsItem *> &items,
                                       AlignDirection direction)
{
    StretchAlignResult result;
    if (items.size() < 2) return result;

    for (auto *item : items) {
        result.oldPositions << item->pos();
        result.oldGeometries << itemGeometryRect(item);
    }

    QRectF overall = overallGeometryRect(items);

    switch (direction) {
    case AlignLeftRight: {
        // 所有图元拉伸至与整体包围盒同宽，左、右边缘分别对齐
        for (auto *item : items) {
            QRectF geom = itemGeometryRect(item);
            QRectF newGeom = geom;
            newGeom.setLeft(0);
            newGeom.setWidth(overall.width());
            qreal newX = overall.left();
            result.newPositions << QPointF(newX, item->pos().y());
            result.newGeometries << newGeom;
        }
        break;
    }
    case AlignTopBottom: {
        // 所有图元拉伸至与整体包围盒同高，上、下边缘分别对齐
        for (auto *item : items) {
            QRectF geom = itemGeometryRect(item);
            QRectF newGeom = geom;
            newGeom.setTop(0);
            newGeom.setHeight(overall.height());
            qreal newY = overall.top();
            result.newPositions << QPointF(item->pos().x(), newY);
            result.newGeometries << newGeom;
        }
        break;
    }
    case AlignHProportional: {
        // 横向撑满并等比缩放高度
        for (auto *item : items) {
            QRectF geom = itemGeometryRect(item);
            if (geom.width() <= 0) {
                result.newPositions << item->pos();
                result.newGeometries << geom;
                continue;
            }
            qreal scale = overall.width() / geom.width();
            QRectF newGeom(0, 0, overall.width(), geom.height() * scale);
            result.newPositions << QPointF(overall.left(),
                                           item->pos().y() + geom.top() - newGeom.top());
            result.newGeometries << newGeom;
        }
        break;
    }
    case AlignVProportional: {
        // 纵向撑齐并等比缩放宽度
        for (auto *item : items) {
            QRectF geom = itemGeometryRect(item);
            if (geom.height() <= 0) {
                result.newPositions << item->pos();
                result.newGeometries << geom;
                continue;
            }
            qreal scale = overall.height() / geom.height();
            QRectF newGeom(0, 0, geom.width() * scale, overall.height());
            result.newPositions << QPointF(item->pos().x() + geom.left() - newGeom.left(),
                                           overall.top());
            result.newGeometries << newGeom;
        }
        break;
    }
    default:
        break;
    }

    result.valid = !result.newPositions.isEmpty();
    return result;
}

// ============================================================
// 页内居中算法
// ============================================================
AlignResult computePageCenter(const QList<QGraphicsItem *> &items,
                              AlignDirection direction, const QRectF &pageRect)
{
    AlignResult result;
    if (items.isEmpty() || pageRect.isNull()) return result;

    for (auto *item : items)
        result.oldPositions << item->pos();

    if (direction == AlignHCenterOnPage) {
        qreal pageCenterX = pageRect.center().x();
        for (auto *item : items) {
            qreal itemCenterX = item->pos().x() + itemGeometryRect(item).center().x();
            result.newPositions << QPointF(item->pos().x() + (pageCenterX - itemCenterX),
                                           item->pos().y());
        }
    } else if (direction == AlignVCenterOnPage) {
        qreal pageCenterY = pageRect.center().y();
        for (auto *item : items) {
            qreal itemCenterY = item->pos().y() + itemGeometryRect(item).center().y();
            result.newPositions << QPointF(item->pos().x(),
                                           item->pos().y() + (pageCenterY - itemCenterY));
        }
    }

    result.valid = !result.newPositions.isEmpty();
    return result;
}

// ============================================================
// 按边缘排序辅助
// ============================================================
static QList<QGraphicsItem *> sortByEdge(const QList<QGraphicsItem *> &items, DistributeDirection dir)
{
    QList<QGraphicsItem *> sorted = items;
    switch (dir) {
    case DistributeH:
    case DistributeHLeft:
        std::sort(sorted.begin(), sorted.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().x() + itemGeometryRect(a).left())
                   < (b->pos().x() + itemGeometryRect(b).left());
        });
        break;
    case DistributeHCenter:
        std::sort(sorted.begin(), sorted.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().x() + itemGeometryRect(a).center().x())
                   < (b->pos().x() + itemGeometryRect(b).center().x());
        });
        break;
    case DistributeHRight:
        std::sort(sorted.begin(), sorted.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().x() + itemGeometryRect(a).right())
                   < (b->pos().x() + itemGeometryRect(b).right());
        });
        break;
    case DistributeV:
    case DistributeVTop:
        std::sort(sorted.begin(), sorted.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().y() + itemGeometryRect(a).top())
                   < (b->pos().y() + itemGeometryRect(b).top());
        });
        break;
    case DistributeVCenter:
        std::sort(sorted.begin(), sorted.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().y() + itemGeometryRect(a).center().y())
                   < (b->pos().y() + itemGeometryRect(b).center().y());
        });
        break;
    case DistributeVBottom:
        std::sort(sorted.begin(), sorted.end(), [](QGraphicsItem *a, QGraphicsItem *b) {
            return (a->pos().y() + itemGeometryRect(a).bottom())
                   < (b->pos().y() + itemGeometryRect(b).bottom());
        });
        break;
    }
    return sorted;
}

// 获取图元在分布方向上的参考坐标（排序用）
static qreal refCoord(QGraphicsItem *item, DistributeDirection dir)
{
    QRectF g = itemGeometryRect(item);
    switch (dir) {
    case DistributeH:
    case DistributeHLeft:   return item->pos().x() + g.left();
    case DistributeHCenter: return item->pos().x() + g.center().x();
    case DistributeHRight:  return item->pos().x() + g.right();
    case DistributeV:
    case DistributeVTop:    return item->pos().y() + g.top();
    case DistributeVCenter: return item->pos().y() + g.center().y();
    case DistributeVBottom: return item->pos().y() + g.bottom();
    }
    return 0;
}

// 获取图元的几何尺寸（分布方向上的跨度）
static qreal geomSpan(QGraphicsItem *item, DistributeDirection dir)
{
    return isHorizontal(dir) ? itemGeomWidth(item) : itemGeomHeight(item);
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

    for (auto *item : items)
        result.oldPositions << item->pos();

    QList<QGraphicsItem *> sortedItems = sortByEdge(items, direction);
    bool useCustomSpacing = !params.autoSpacing;
    bool isH = isHorizontal(direction);

    QMap<QGraphicsItem *, QPointF> newPosMap;

    if (useCustomSpacing) {
        // 自定义间距：首图元保持原位，后续图元按指定间距依次排列
        qreal current = refCoord(sortedItems.first(), direction);
        for (int i = 0; i < sortedItems.size(); ++i) {
            auto *item = sortedItems[i];
            qreal itemRef = refCoord(item, direction);
            qreal offset = current - itemRef;
            if (isH)
                newPosMap[item] = QPointF(item->pos().x() + offset, item->pos().y());
            else
                newPosMap[item] = QPointF(item->pos().x(), item->pos().y() + offset);
            current += geomSpan(item, direction) + params.customSpacing;
        }
    } else {
        // 自动间距：等间距分布（首末图元保持原位）
        qreal firstRef = refCoord(sortedItems.first(), direction);
        qreal lastRef = refCoord(sortedItems.last(), direction);
        int n = sortedItems.size();

        if (direction == DistributeH || direction == DistributeV) {
            // 等间隙分布：间隙 = (末边缘 - 首起始 - 总宽度) / (n-1)
            qreal totalSpan = 0;
            for (auto *item : sortedItems)
                totalSpan += geomSpan(item, direction);
            qreal lastEnd = lastRef + geomSpan(sortedItems.last(), direction);
            qreal gap0 = (lastEnd - firstRef - totalSpan) / (n - 1);

            qreal current = firstRef;
            for (int i = 0; i < n; ++i) {
                auto *item = sortedItems[i];
                qreal itemRef = refCoord(item, direction);
                qreal offset = current - itemRef;
                if (isH)
                    newPosMap[item] = QPointF(item->pos().x() + offset, item->pos().y());
                else
                    newPosMap[item] = QPointF(item->pos().x(), item->pos().y() + offset);
                current += geomSpan(item, direction) + gap0;
            }
        } else {
            // 边缘/中线等距分布：参考点间距相等
            qreal totalRefSpan = lastRef - firstRef;
            qreal step = totalRefSpan / (n - 1);

            qreal current = firstRef;
            for (int i = 0; i < n; ++i) {
                auto *item = sortedItems[i];
                qreal itemRef = refCoord(item, direction);
                qreal offset = current - itemRef;
                if (isH)
                    newPosMap[item] = QPointF(item->pos().x() + offset, item->pos().y());
                else
                    newPosMap[item] = QPointF(item->pos().x(), item->pos().y() + offset);
                current += step;
            }
        }
    }

    // 按原始 items 顺序输出结果
    for (auto *item : items)
        result.newPositions << newPosMap.value(item, item->pos());

    result.valid = !result.newPositions.isEmpty();
    return result;
}

} // namespace AlignmentUtils

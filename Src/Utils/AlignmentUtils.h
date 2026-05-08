#ifndef ALIGNMENTUTILS_H
#define ALIGNMENTUTILS_H

#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QRectF>

// 对齐与分布算法工具集
// 所有算法使用 itemGeometryRect() 获取精确几何矩形（不含画笔边距），
// 避免 boundingRect() 因画笔半像素边距导致的累积误差。
namespace AlignmentUtils {

// ---- 对齐方向 ----
enum AlignDirection {
    AlignLeft,
    AlignHCenter,
    AlignRight,
    AlignTop,
    AlignVCenter,
    AlignBottom
};

// ---- 分布方向 ----
enum DistributeDirection {
    DistributeH,
    DistributeV
};

// ---- 分布参数 ----
struct DistributeParams {
    bool autoSpacing = true;   // true = 等间隙（首末不动），false = 指定间距
    qreal customSpacing = 0.0; // autoSpacing=false 时的间距值（≥0）
};

// ---- 算法结果 ----
struct AlignResult {
    bool valid = false;
    QList<QPointF> oldPositions;
    QList<QPointF> newPositions;
};

struct DistributeResult {
    bool valid = false;
    QList<QPointF> oldPositions;
    QList<QPointF> newPositions;
};

// ---- 核心算法 ----

// 计算对齐后的新位置（不修改图元）
AlignResult computeAlign(const QList<QGraphicsItem *> &items, AlignDirection direction);

// 计算分布后的新位置（不修改图元）
DistributeResult computeDistribute(const QList<QGraphicsItem *> &items,
                                   DistributeDirection direction,
                                   const DistributeParams &params = DistributeParams());

// ---- 辅助函数 ----

// 获取图元的精确几何矩形（优先使用 IGraphicsItem::geometryRect()）
QRectF itemGeometryRect(const QGraphicsItem *item);

// 获取图元几何宽度/高度
qreal itemGeomWidth(const QGraphicsItem *item);
qreal itemGeomHeight(const QGraphicsItem *item);

// 对齐方向名称（用于 undo 描述）
QString alignDirectionName(AlignDirection dir);
QString distributeDirectionName(DistributeDirection dir);

} // namespace AlignmentUtils

#endif // ALIGNMENTUTILS_H

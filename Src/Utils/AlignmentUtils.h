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
    AlignBottom,
    AlignLeftRight,     // 横向左右靠齐 — 拉伸至同宽，左右对齐
    AlignTopBottom,     // 纵向向撑齐   — 拉伸至同高，上下对齐
    AlignHProportional, // 横向等比撑满 — 横向撑满并等比缩放高度
    AlignVProportional, // 纵向等比撑齐 — 纵向撑齐并等比缩放宽度
    AlignHCenterOnPage, // 横向页内居中
    AlignVCenterOnPage  // 纵向页内居中
};

// ---- 分布方向 ----
enum DistributeDirection {
    DistributeH,        // 横向等间距分布（间隙相等）
    DistributeV,        // 纵向等间距分布（间隙相等）
    DistributeHLeft,    // 横向左边等距分布
    DistributeHCenter,  // 横向中线等距分布
    DistributeHRight,   // 横向右边等距分布
    DistributeVTop,     // 纵向上边等距分布
    DistributeVCenter,  // 纵向中线等距分布
    DistributeVBottom   // 纵向下边等距分布
};

// ---- 分布参数 ----
struct DistributeParams {
    bool autoSpacing = true;   // true = 等间隙（首末不动），false = 指定间距
    qreal customSpacing = 0.0; // autoSpacing=false 时的间距值（>=0）
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

// 拉伸对齐结果 — 同时改变位置和大小
struct StretchAlignResult {
    bool valid = false;
    QList<QPointF> oldPositions;
    QList<QPointF> newPositions;
    QList<QRectF> oldGeometries;   // 使用 geometryRect()
    QList<QRectF> newGeometries;
};

// ---- 核心算法 ----

// 计算对齐后的新位置（不修改图元）
AlignResult computeAlign(const QList<QGraphicsItem *> &items, AlignDirection direction);

// 计算拉伸对齐后的新位置和大小
StretchAlignResult computeStretchAlign(const QList<QGraphicsItem *> &items,
                                       AlignDirection direction);

// 计算页内居中后的新位置
AlignResult computePageCenter(const QList<QGraphicsItem *> &items,
                              AlignDirection direction, const QRectF &pageRect);

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

// 获取所有图元的整体包围矩形（使用 geometryRect，场景坐标）
QRectF overallGeometryRect(const QList<QGraphicsItem *> &items);

// 对齐方向名称（用于 undo 描述）
QString alignDirectionName(AlignDirection dir);
QString distributeDirectionName(DistributeDirection dir);

// 判断方向是否为拉伸对齐
bool isStretchAlign(AlignDirection dir);

// 判断方向是否为页内居中
bool isPageCenter(AlignDirection dir);

// 判断方向是否为水平操作
bool isHorizontal(AlignDirection dir);
bool isHorizontal(DistributeDirection dir);

} // namespace AlignmentUtils

#endif // ALIGNMENTUTILS_H

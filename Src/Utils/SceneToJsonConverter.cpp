#include "SceneToJsonConverter.h"
#include "atMath.h"

#include "CanvasItem.h"
#include "GraphicsItemGroup.h"
#include "IGraphicsItem.h"
#include "RectItem.h"

#include <QBrush>
#include <QConicalGradient>
#include <QFont>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================================
//  内部辅助
// ============================================================================

namespace {

struct CollectedItem
{
    QGraphicsItem *item = nullptr;
    IGraphicsItem *gi = nullptr;
    int z = 0;
};

// 递归收集所有 IGraphicsItem（展开分组）
void collectItems(QGraphicsItem *parent, std::vector<CollectedItem> &out, int &zCounter)
{
    auto *group = dynamic_cast<GraphicsItemGroup *>(parent);
    if (group) {
        for (auto *child : group->childItems())
            collectItems(child, out, zCounter);
        return;
    }

    auto *gi = dynamic_cast<IGraphicsItem *>(parent);
    if (gi && gi->itemType() != IGraphicsItem::GroupItemType) {
        out.push_back({ parent, gi, zCounter++ });
    }
}

// ============================================================================
//  LineStyle 映射
// ============================================================================

// QPen style → EE lineStyle 字符串（与 JsonSceneParser::parseLineStyle 一致）
// 参考: ExportEngine/Core/SceneData.h enum class LineStyle
const char *lineStyleString(Qt::PenStyle style)
{
    switch (style) {
    case Qt::DashLine:
        return "dashed";
    case Qt::DotLine:
        return "dotted";
    case Qt::DashDotLine:
        return "dashDot";
    case Qt::DashDotDotLine:
        return "doubleDotDash";
    case Qt::NoPen:
        return "none";
    default:
        return "solid";
    }
}

// ============================================================================
//  Color JSON 构建（纯色，与 ExportEngine Color 结构对齐）
//  参考: ExportEngine/Core/SceneData.h struct Color
//        ExportEngine/Core/JsonSceneParser.cpp parseColor()
// ============================================================================

// 构建颜色 JSON 对象（纯色，无渐变）
// 优先使用存储的 CMYK 值（0-100%），否则从 QColor 转 RGBA（0-255）
QJsonObject makeColorJson(IGraphicsItem *gi, bool isPen, const QColor &fallback)
{
    QJsonObject obj;

    if (isPen && gi->hasPenCmyk()) {
        double c = 0, m = 0, y = 0, k = 0;
        gi->penCmyk(c, m, y, k);
        obj["space"] = QStringLiteral("cmyk");
        obj["c"] = c;
        obj["m"] = m;
        obj["y"] = y;
        obj["k"] = k;
        obj["a"] = 1.0;
    } else if (!isPen && gi->hasBrushCmyk()) {
        double c = 0, m = 0, y = 0, k = 0;
        gi->brushCmyk(c, m, y, k);
        obj["space"] = QStringLiteral("cmyk");
        obj["c"] = c;
        obj["m"] = m;
        obj["y"] = y;
        obj["k"] = k;
        obj["a"] = 1.0;
    } else {
        QColor col = fallback.isValid() ? fallback : QColor(0, 0, 0);
        obj["space"] = QStringLiteral("rgba");
        obj["r"] = static_cast<double>(col.red());
        obj["g"] = static_cast<double>(col.green());
        obj["b"] = static_cast<double>(col.blue());
        obj["a"] = static_cast<double>(col.alphaF());
    }
    return obj;
}

// build fillColor JSON (no gradient) — for fill-only primitives
QJsonObject makeFillColorJson(IGraphicsItem *gi, const QBrush &brush)
{
    QJsonObject obj;
    if (gi->hasBrushCmyk()) {
        double c = 0, m = 0, y = 0, k = 0;
        gi->brushCmyk(c, m, y, k);
        obj["space"] = QStringLiteral("cmyk");
        obj["c"] = c;
        obj["m"] = m;
        obj["y"] = y;
        obj["k"] = k;
        obj["a"] = 1.0;
    } else {
        QColor col = brush.color();
        obj["space"] = QStringLiteral("rgba");
        obj["r"] = static_cast<double>(col.red());
        obj["g"] = static_cast<double>(col.green());
        obj["b"] = static_cast<double>(col.blue());
        obj["a"] = static_cast<double>(col.alphaF());
    }
    return obj;
}

// 渐变停止点颜色 JSON
// 以容差 1e-9 匹配 stop position，防止 QGradient::stops() 与
// gradientStopCmykMap 的 key 因浮点序列化/反序列化产生微小差异
QJsonObject makeGradientStopColorJson(IGraphicsItem *gi, double pos, const QColor &qtColor)
{
    QJsonObject obj;

    QMap<double, CmykColor> cmykMap = gi->gradientStopCmykMap();
    const CmykColor *cmyk = nullptr;
    for (auto it = cmykMap.constBegin(); it != cmykMap.constEnd(); ++it) {
        if (AtMath::isEqual(it.key(), pos)) {
            cmyk = &it.value();
            break;
        }
    }

    if (cmyk && cmyk->valid) {
        obj["space"] = QStringLiteral("cmyk");
        obj["c"] = cmyk->c;
        obj["m"] = cmyk->m;
        obj["y"] = cmyk->y;
        obj["k"] = cmyk->k;
        obj["a"] = 1.0;
    } else {
        QColor col = qtColor.isValid() ? qtColor : QColor(0, 0, 0);
        obj["space"] = QStringLiteral("rgba");
        obj["r"] = static_cast<double>(col.red());
        obj["g"] = static_cast<double>(col.green());
        obj["b"] = static_cast<double>(col.blue());
        obj["a"] = static_cast<double>(col.alphaF());
    }
    return obj;
}

// ============================================================================
//  Gradient JSON 构建
//  参考: ExportEngine/Core/SceneData.h struct Gradient
//        ExportEngine/Core/JsonSceneParser.cpp parseGradient()
//
//  渐变坐标在 QBrush 中始终以 [0, 1] 归一化存储（QtGradientEditor 默认值
//  即为 (0,0)→(1,1) 等），ColorUtils::mapGradientBrushToRect 仅在绘制时展
//  开为像素坐标。此处直接输出原始归一化坐标，与 ExportEngine 的期望一致。
// ============================================================================

QJsonObject makeGradientJson(IGraphicsItem *gi, const QBrush &brush)
{
    const QGradient *gradient = brush.gradient();
    if (!gradient)
        return { };

    const auto stops = gradient->stops();
    if (stops.size() < 2)
        return { };

    QJsonObject obj;

    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        auto *linear = static_cast<const QLinearGradient *>(gradient);
        obj["type"] = QStringLiteral("linear");
        obj["x1"] = linear->start().x();
        obj["y1"] = linear->start().y();
        obj["x2"] = linear->finalStop().x();
        obj["y2"] = linear->finalStop().y();
        break;
    }
    case QGradient::RadialGradient: {
        auto *radial = static_cast<const QRadialGradient *>(gradient);
        obj["type"] = QStringLiteral("radial");
        obj["cx"] = radial->center().x();
        obj["cy"] = radial->center().y();
        obj["r"] = radial->radius();
        break;
    }
    case QGradient::ConicalGradient: {
        auto *conical = static_cast<const QConicalGradient *>(gradient);
        obj["type"] = QStringLiteral("conic");
        obj["cx"] = conical->center().x();
        obj["cy"] = conical->center().y();
        obj["startAngle"] = conical->angle();
        break;
    }
    default:
        // 未知渐变类型，退化为默认水平线性渐变
        obj["type"] = QStringLiteral("linear");
        obj["x1"] = 0.0;
        obj["y1"] = 0.0;
        obj["x2"] = 1.0;
        obj["y2"] = 0.0;
        break;
    }

    QJsonArray stopsArr;
    for (const auto &stop : stops) {
        QJsonObject stopObj;
        stopObj["offset"] = stop.first;
        stopObj["color"] = makeGradientStopColorJson(gi, stop.first, stop.second);
        stopsArr.append(stopObj);
    }
    obj["stops"] = stopsArr;

    return obj;
}

// ============================================================================
//  通用填充/描边辅助函数（减少 rect/circle 代码重复）
//  ExportEngine 中 fillColor 与 gradient 互斥（JsonSceneParser 校验）
// ============================================================================

// 添加 fillColor 或 gradient 到 JSON 对象（二者互斥）
void addFillToObject(QJsonObject &obj, IGraphicsItem *gi, const QBrush &brush)
{
    QJsonObject gradient = makeGradientJson(gi, brush);
    if (!gradient.isEmpty()) {
        obj["gradient"] = gradient;
    } else if (brush.style() != Qt::NoBrush) {
        obj["fillColor"] = makeFillColorJson(gi, brush);
    }
}

// 添加描边属性（strokeColor, strokeWidth, lineStyle）到 JSON 对象
// 仅在有描边时输出，与原有行为一致
void addStrokeToObject(QJsonObject &obj, IGraphicsItem *gi, const QPen &pen, qreal scale)
{
    if (pen.style() == Qt::NoPen)
        return;
    qreal pw = pen.widthF();
    if (pw <= 0.0)
        return;
    obj["strokeColor"] = makeColorJson(gi, true, pen.color());
    obj["strokeWidth"] = pw * scale;
    obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));
}

// 计算去除画笔边距后的精确几何矩形
// ExportEngine 的 x/y/width/height 描述图形核心区域，不含描边
QRectF insetGeometryRect(IGraphicsItem *gi, QGraphicsItem *item, const QPen &pen)
{
    QRectF geo = gi->supportsGeometryRect() ? gi->geometryRect() : item->boundingRect();
    qreal pw = (pen.style() != Qt::NoPen) ? pen.widthF() : 0.0;
    qreal halfPw = pw / 2.0;
    return QRectF(geo.x() + halfPw, geo.y() + halfPw, std::max(0.0, geo.width() - pw),
                  std::max(0.0, geo.height() - pw));
}

} // namespace

// ============================================================================
//  SceneToJsonConverter — 主转换逻辑
// ============================================================================

SceneToJsonConverter::ConvertResult
SceneToJsonConverter::convert(QGraphicsScene *scene, const QString &rgbProfile,
                              const QString &cmykProfile, const QString &grayProfile, int exportDpi)
{
    ConvertResult result;

    if (!scene) {
        result.errorMessage = QStringLiteral("Scene is null");
        return result;
    }

    // 1. 查找 CanvasItem 获取画布尺寸和 DPI
    CanvasItem *canvasItem = nullptr;
    for (auto *item : scene->items()) {
        canvasItem = dynamic_cast<CanvasItem *>(item);
        if (canvasItem)
            break;
    }

    if (!canvasItem) {
        result.errorMessage = QStringLiteral("No CanvasItem found in scene");
        return result;
    }

    QRectF canvasRect = canvasItem->rect();
    int sourceDpi = canvasItem->canvasDpiX();
    if (sourceDpi <= 0)
        sourceDpi = canvasItem->ppi() > 0 ? static_cast<int>(canvasItem->ppi()) : 150;

    // 导出 DPI 缩放因子
    // 不修改画布，仅在 JSON 输出时按比例缩放所有像素坐标
    int dpi = sourceDpi;
    qreal scale = 1.0;
    if (exportDpi > 0 && sourceDpi > 0 && exportDpi != sourceDpi) {
        scale = static_cast<qreal>(exportDpi) / sourceDpi;
        dpi = exportDpi;
    }

    int width = static_cast<int>(std::round(canvasRect.width() * scale));
    int height = static_cast<int>(std::round(canvasRect.height() * scale));

    // 2. 收集所有图元（展开分组），按 z-order 排列
    std::vector<CollectedItem> items;
    int zCounter = 0;
    for (auto *item : scene->items()) {
        if (dynamic_cast<CanvasItem *>(item))
            continue;
        // ResizeHandleItem — 不参与导出
        if (item->type() == QGraphicsItem::UserType + 200)
            continue;
        if (item->parentItem())
            continue; // 子图元由分组递归处理

        collectItems(item, items, zCounter);
    }

    // scene->items() 按 z 从高到低排序，反转得到从低到高（ExportEngine 约定）
    std::reverse(items.begin(), items.end());

    // 3. 按类型分桶
    std::vector<const CollectedItem *> rects, circles, lines, beziers, freelines, texts, images;
    for (const auto &ci : items) {
        switch (ci.gi->itemType()) {
        case IGraphicsItem::RectItemType:
            rects.push_back(&ci);
            break;
        case IGraphicsItem::EllipseItemType:
            circles.push_back(&ci);
            break;
        case IGraphicsItem::LineItemType:
            lines.push_back(&ci);
            break;
        case IGraphicsItem::BezierCurveItemType:
            beziers.push_back(&ci);
            break;
        case IGraphicsItem::FreehandItemType:
            freelines.push_back(&ci);
            break;
        case IGraphicsItem::TextItemType:
            texts.push_back(&ci);
            break;
        case IGraphicsItem::ImageItemType:
            images.push_back(&ci);
            break;
        default:
            break;
        }
    }

    // =====================================================================
    //  构建各类型 JSON 数组
    //  字段名与 ExportEngine SceneData.h / JsonSceneParser.cpp 严格对齐
    // =====================================================================

    // --- rects ---
    // 参考: ExportEngine/Core/SceneData.h struct Rect
    QJsonArray rectsArr;
    if (!rects.empty()) {
        for (const auto &ci : rects) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPen pen = gi->itemPen();
            QBrush brush = gi->itemBrush();
            QPointF scenePos = item->scenePos();

            QRectF geomRect = insetGeometryRect(gi, item, pen);
            QRectF sceneRect(scenePos + geomRect.topLeft(), geomRect.size());

            QJsonObject obj;
            obj["x"] = sceneRect.x() * scale;
            obj["y"] = sceneRect.y() * scale;
            obj["width"] = sceneRect.width() * scale;
            obj["height"] = sceneRect.height() * scale;
            obj["z"] = ci->z;
            obj["rotation"] = item->rotation();

            addFillToObject(obj, gi, brush);
            addStrokeToObject(obj, gi, pen, scale);

            // 圆角（仅 RectItem 子类且有正值时输出，与原有行为一致）
            auto *rectItem = dynamic_cast<RectItem *>(item);
            if (rectItem) {
                qreal cr = rectItem->cornerRadius();
                if (cr > 0.0)
                    obj["cornerRadius"] = cr * scale;
            }

            rectsArr.append(obj);
        }
    }

    // --- circles ---
    // 参考: ExportEngine/Core/SceneData.h struct Circle
    QJsonArray circlesArr;
    if (!circles.empty()) {
        for (const auto &ci : circles) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPen pen = gi->itemPen();
            QBrush brush = gi->itemBrush();
            QPointF scenePos = item->scenePos();

            QRectF geomRect = insetGeometryRect(gi, item, pen);

            qreal cx = scenePos.x() + geomRect.center().x();
            qreal cy = scenePos.y() + geomRect.center().y();
            qreal rx = geomRect.width() / 2.0;
            qreal ry = geomRect.height() / 2.0;

            QJsonObject obj;
            obj["cx"] = cx * scale;
            obj["cy"] = cy * scale;
            obj["radiusX"] = rx * scale;
            obj["radiusY"] = ry * scale;
            obj["z"] = ci->z;
            obj["rotation"] = item->rotation();

            addFillToObject(obj, gi, brush);
            addStrokeToObject(obj, gi, pen, scale);

            circlesArr.append(obj);
        }
    }

    // --- freeLines ---
    // 参考: ExportEngine/Core/SceneData.h struct FreeLine
    // FreeLine 仅支持描边，不支持填充
    QJsonArray freeLinesArr;
    if (!freelines.empty()) {
        for (const auto &ci : freelines) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPen pen = gi->itemPen();
            QPointF scenePos = item->scenePos();

            auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
            QPainterPath path = pathItem ? pathItem->path() : item->shape();

            QJsonArray pointsArr;
            for (int j = 0; j < path.elementCount(); ++j) {
                auto el = path.elementAt(j);
                // MoveTo + LineTo 构成自由线段的顶点序列
                if (el.isMoveTo() || el.isLineTo()) {
                    QJsonObject pt;
                    pt["x"] = (scenePos.x() + el.x) * scale;
                    pt["y"] = (scenePos.y() + el.y) * scale;
                    pointsArr.append(pt);
                }
            }

            QJsonObject obj;
            obj["points"] = pointsArr;
            obj["z"] = ci->z;
            obj["strokeWidth"] = pen.widthF() * scale;
            obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));
            obj["strokeColor"] = makeColorJson(gi, true, pen.color());

            freeLinesArr.append(obj);
        }
    }

    // --- bezierCurves ---
    // 参考: ExportEngine/Core/SceneData.h struct BezierCurve
    // controlPoints 序列：MoveTo → CurveTo → 2×CurveToDataElement → …（每段 3 个增量元素）
    // ExportEngine 渲染时按 4 点一组（含共享端点）解析为三次贝塞尔曲线段
    QJsonArray bezierArr;
    if (!beziers.empty()) {
        for (const auto &ci : beziers) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPen pen = gi->itemPen();
            QPointF scenePos = item->scenePos();

            auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
            QPainterPath path = pathItem ? pathItem->path() : QPainterPath();

            QJsonArray cpArr;
            for (int j = 0; j < path.elementCount(); ++j) {
                auto el = path.elementAt(j);
                // MoveTo（起始点）+ CurveTo + CurveToDataElement 构成控制点序列
                if (el.isMoveTo() || el.isLineTo() || el.isCurveTo()
                    || el.type == QPainterPath::CurveToDataElement) {
                    QJsonObject pt;
                    pt["x"] = (scenePos.x() + el.x) * scale;
                    pt["y"] = (scenePos.y() + el.y) * scale;
                    cpArr.append(pt);
                }
            }

            QJsonObject obj;
            obj["controlPoints"] = cpArr;
            obj["z"] = ci->z;
            obj["strokeWidth"] = pen.widthF() * scale;
            obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));
            obj["strokeColor"] = makeColorJson(gi, true, pen.color());

            bezierArr.append(obj);
        }
    }

    // --- lines ---
    // 参考: ExportEngine/Core/SceneData.h struct Line
    QJsonArray linesArr;
    if (!lines.empty()) {
        for (const auto &ci : lines) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPen pen = gi->itemPen();
            QPointF scenePos = item->scenePos();

            auto *lineItem = dynamic_cast<QGraphicsLineItem *>(item);
            QLineF line = lineItem ? lineItem->line() : QLineF();

            QJsonObject obj;
            obj["x1"] = (scenePos.x() + line.x1()) * scale;
            obj["y1"] = (scenePos.y() + line.y1()) * scale;
            obj["x2"] = (scenePos.x() + line.x2()) * scale;
            obj["y2"] = (scenePos.y() + line.y2()) * scale;
            obj["z"] = ci->z;
            obj["strokeWidth"] = pen.widthF() * scale;
            obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));
            obj["strokeColor"] = makeColorJson(gi, true, pen.color());

            linesArr.append(obj);
        }
    }

    // --- texts ---
    // 参考: ExportEngine/Core/SceneData.h struct Text
    // fontSize 为 pt（物理单位），不受 unit 转换影响。
    // 注意：EE FontEngine 不使用 canvas.dpi 渲染文字，因此当导出 DPI 与画布
    // 原生 DPI 不同时，按比例缩放 fontSize 以保持文字在画布中的视觉占比。
    QJsonArray textsArr;
    if (!texts.empty()) {
        for (const auto &ci : texts) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPointF scenePos = item->scenePos();
            QFont font = gi->itemFont();
            QString content = gi->text();

            // 文字颜色（ExportEngine 使用 textColor，非 fillColor）
            QJsonObject textCol;
            if (gi->hasPenCmyk()) {
                double c = 0, m = 0, y = 0, k = 0;
                gi->penCmyk(c, m, y, k);
                textCol["space"] = QStringLiteral("cmyk");
                textCol["c"] = c;
                textCol["m"] = m;
                textCol["y"] = y;
                textCol["k"] = k;
                textCol["a"] = 1.0;
            } else {
                QColor col = gi->itemPen().color();
                textCol["space"] = QStringLiteral("rgba");
                textCol["r"] = static_cast<double>(col.red());
                textCol["g"] = static_cast<double>(col.green());
                textCol["b"] = static_cast<double>(col.blue());
                textCol["a"] = static_cast<double>(col.alphaF());
            }

            // 字号：优先点值（qreal），其次像素值
            qreal fontSize = font.pointSizeF();
            if (fontSize <= 0.0)
                fontSize = static_cast<qreal>(font.pixelSize());
            fontSize *= scale;

            QJsonObject obj;
            obj["x"] = scenePos.x() * scale;
            obj["y"] = scenePos.y() * scale;
            obj["z"] = ci->z;
            obj["content"] = content;
            obj["fontFamily"] = font.family();
            obj["fontSize"] = fontSize;
            obj["bold"] = font.bold();
            obj["italic"] = font.italic();
            obj["textColor"] = textCol;
            obj["rotation"] = item->rotation();

            textsArr.append(obj);
        }
    }

    // --- images ---
    // 参考: ExportEngine/Core/SceneData.h struct ImageItem
    QJsonArray imagesArr;
    if (!images.empty()) {
        for (const auto &ci : images) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPointF scenePos = item->scenePos();
            QRectF r = item->boundingRect();
            QString filePath = gi->filePath();

            QJsonObject obj;
            obj["filePath"] = filePath;
            obj["x"] = (scenePos.x() + r.x()) * scale;
            obj["y"] = (scenePos.y() + r.y()) * scale;
            obj["width"] = r.width() * scale;
            obj["height"] = r.height() * scale;
            obj["z"] = ci->z;

            imagesArr.append(obj);
        }
    }

    // =====================================================================
    //  4. 组装顶层 canvas JSON
    //  与 ExportEngine Canvas struct 对齐
    // =====================================================================
    QJsonObject canvasObj;
    canvasObj["width"] = width;
    canvasObj["height"] = height;
    canvasObj["dpi"] = dpi;
    canvasObj["unit"] = QStringLiteral("px");

    // 背景色（默认白色 RGBA）
    QJsonObject bg;
    bg["space"] = QStringLiteral("rgba");
    bg["r"] = 255.0;
    bg["g"] = 255.0;
    bg["b"] = 255.0;
    bg["a"] = 1.0;
    canvasObj["background"] = bg;

    if (!rgbProfile.isEmpty())
        canvasObj["rgbProfile"] = rgbProfile;
    if (!cmykProfile.isEmpty())
        canvasObj["cmykProfile"] = cmykProfile;
    if (!grayProfile.isEmpty())
        canvasObj["grayProfile"] = grayProfile;

    if (!rectsArr.isEmpty())
        canvasObj["rects"] = rectsArr;
    if (!circlesArr.isEmpty())
        canvasObj["circles"] = circlesArr;
    if (!freeLinesArr.isEmpty())
        canvasObj["freeLines"] = freeLinesArr;
    if (!bezierArr.isEmpty())
        canvasObj["bezierCurves"] = bezierArr;
    if (!linesArr.isEmpty())
        canvasObj["lines"] = linesArr;
    if (!textsArr.isEmpty())
        canvasObj["texts"] = textsArr;
    if (!imagesArr.isEmpty())
        canvasObj["images"] = imagesArr;

    QJsonObject root;
    root["canvas"] = canvasObj;

    QJsonDocument doc(root);
    result.json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    result.success = true;

    return result;
}

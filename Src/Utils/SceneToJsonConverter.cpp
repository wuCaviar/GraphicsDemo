#include "SceneToJsonConverter.h"

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

// QPen style → EE lineStyle 字符串
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

// 构建颜色 JSON 对象（纯色，无渐变）
// 优先使用存储的 CMYK 值，否则从 QColor 转 RGBA
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

// 渐变停止点颜色 JSON
QJsonObject makeGradientStopColorJson(IGraphicsItem *gi, double pos, const QColor &qtColor)
{
    QJsonObject obj;

    if (gi->hasGradientStopCmyk(pos)) {
        double c = 0, m = 0, y = 0, k = 0;
        gi->gradientStopCmyk(pos, c, m, y, k);
        obj["space"] = QStringLiteral("cmyk");
        obj["c"] = c;
        obj["m"] = m;
        obj["y"] = y;
        obj["k"] = k;
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

// 从 QBrush 提取渐变信息，返回 QJsonObject
// 返回空对象表示无渐变
QJsonObject makeGradientJson(IGraphicsItem *gi, const QBrush &brush, const QRectF &itemRect)
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
        QPointF start = linear->start();
        QPointF end = linear->finalStop();

        // 归一化坐标（qreal 精度，避免整数除法）
        qreal nx1 = (start.x() - itemRect.left()) / itemRect.width();
        qreal ny1 = (start.y() - itemRect.top()) / itemRect.height();
        qreal nx2 = (end.x() - itemRect.left()) / itemRect.width();
        qreal ny2 = (end.y() - itemRect.top()) / itemRect.height();

        obj["type"] = QStringLiteral("linear");
        obj["x1"] = nx1;
        obj["y1"] = ny1;
        obj["x2"] = nx2;
        obj["y2"] = ny2;
        break;
    }
    case QGradient::RadialGradient: {
        auto *radial = static_cast<const QRadialGradient *>(gradient);
        QPointF center = radial->center();
        qreal radius = radial->radius();

        qreal cx = (center.x() - itemRect.left()) / itemRect.width();
        qreal cy = (center.y() - itemRect.top()) / itemRect.height();
        qreal r = radius / std::max(itemRect.width(), itemRect.height());

        obj["type"] = QStringLiteral("radial");
        obj["cx"] = cx;
        obj["cy"] = cy;
        obj["r"] = r;
        break;
    }
    case QGradient::ConicalGradient: {
        auto *conical = static_cast<const QConicalGradient *>(gradient);
        QPointF center = conical->center();

        qreal cx = (center.x() - itemRect.left()) / itemRect.width();
        qreal cy = (center.y() - itemRect.top()) / itemRect.height();

        obj["type"] = QStringLiteral("conic");
        obj["cx"] = cx;
        obj["cy"] = cy;
        obj["startAngle"] = conical->angle();
        break;
    }
    default:
        obj["type"] = QStringLiteral("linear");
        obj["x1"] = 0.0;
        obj["y1"] = 0.0;
        obj["x2"] = 1.0;
        obj["y2"] = 0.0;
        break;
    }

    // 色标数组
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

// 构建 fillColor JSON 对象（纯色，不含渐变）
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

} // namespace

// ============================================================================
//  SceneToJsonConverter
// ============================================================================

SceneToJsonConverter::ConvertResult SceneToJsonConverter::convert(QGraphicsScene *scene,
                                                                  const QString &rgbProfile,
                                                                  const QString &cmykProfile,
                                                                  const QString &grayProfile)
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
    int width = static_cast<int>(canvasRect.width());
    int height = static_cast<int>(canvasRect.height());
    int dpi = canvasItem->canvasDpiX();
    if (dpi <= 0)
        dpi = canvasItem->ppi() > 0 ? static_cast<int>(canvasItem->ppi()) : 150;

    // 2. 收集所有图元（展开分组），按 z-order 排列
    std::vector<CollectedItem> items;
    int zCounter = 0;
    for (auto *item : scene->items()) {
        if (dynamic_cast<CanvasItem *>(item))
            continue;
        if (item->type() == QGraphicsItem::UserType + 200) // ResizeHandleItem
            continue;
        if (item->parentItem())
            continue; // 子图元由分组递归处理

        collectItems(item, items, zCounter);
    }

    // items 按 scene->items() 顺序（z 从高到低），反转得到从低到高
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

    // 画笔颜色辅助 lambda
    auto penColor = [](IGraphicsItem *gi) -> QJsonObject {
        return makeColorJson(gi, true, gi->itemPen().color());
    };

    // === 构建各类型 JSON 数组 ===

    // --- rects ---
    QJsonArray rectsArr;
    if (!rects.empty()) {
        for (const auto &ci : rects) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPen pen = gi->itemPen();
            QBrush brush = gi->itemBrush();
            QPointF scenePos = item->scenePos();

            // 精确几何矩形（不含画笔边距）
            QRectF geo = gi->supportsGeometryRect() ? gi->geometryRect() : item->boundingRect();
            qreal pw = (pen.style() != Qt::NoPen) ? pen.widthF() : 0.0;
            QRectF geomRect(geo.x() + pw / 2.0, geo.y() + pw / 2.0, geo.width() - pw,
                            geo.height() - pw);
            QRectF sceneRect(scenePos + geomRect.topLeft(), geomRect.size());

            QJsonObject obj;
            obj["x"] = sceneRect.x();
            obj["y"] = sceneRect.y();
            obj["width"] = sceneRect.width();
            obj["height"] = sceneRect.height();
            obj["z"] = ci->z;

            // 渐变（顶层，不与 fillColor 共存）
            QJsonObject gradient = makeGradientJson(gi, brush, sceneRect);
            if (!gradient.isEmpty()) {
                obj["gradient"] = gradient;
            } else if (brush.style() != Qt::NoBrush) {
                obj["fillColor"] = makeFillColorJson(gi, brush);
            }

            // 描边
            if (pen.style() != Qt::NoPen && pw > 0.0) {
                obj["strokeColor"] = penColor(gi);
                obj["strokeWidth"] = pw;
            }

            obj["rotation"] = item->rotation();

            // 圆角
            auto *rectItem = dynamic_cast<RectItem *>(item);
            if (rectItem) {
                qreal cr = rectItem->cornerRadius();
                if (cr > 0.0)
                    obj["cornerRadius"] = cr;
            }

            if (pen.style() != Qt::NoPen)
                obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));

            rectsArr.append(obj);
        }
    }

    // --- circles ---
    QJsonArray circlesArr;
    if (!circles.empty()) {
        for (const auto &ci : circles) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPen pen = gi->itemPen();
            QBrush brush = gi->itemBrush();
            QPointF scenePos = item->scenePos();

            // 精确几何矩形（不含画笔边距）
            QRectF geo = gi->supportsGeometryRect() ? gi->geometryRect() : item->boundingRect();
            qreal pw = (pen.style() != Qt::NoPen) ? pen.widthF() : 0.0;
            QRectF geomRect(geo.x() + pw / 2.0, geo.y() + pw / 2.0, geo.width() - pw,
                            geo.height() - pw);

            qreal cx = scenePos.x() + geomRect.center().x();
            qreal cy = scenePos.y() + geomRect.center().y();
            qreal rx = geomRect.width() / 2.0;
            qreal ry = geomRect.height() / 2.0;
            QRectF sceneRect(scenePos + geomRect.topLeft(), geomRect.size());

            QJsonObject obj;
            obj["cx"] = cx;
            obj["cy"] = cy;
            obj["radiusX"] = rx;
            obj["radiusY"] = ry;
            obj["z"] = ci->z;

            // 渐变（顶层）
            QJsonObject gradient = makeGradientJson(gi, brush, sceneRect);
            if (!gradient.isEmpty()) {
                obj["gradient"] = gradient;
            } else if (brush.style() != Qt::NoBrush) {
                obj["fillColor"] = makeFillColorJson(gi, brush);
            }

            if (pen.style() != Qt::NoPen && pw > 0.0) {
                obj["strokeColor"] = penColor(gi);
                obj["strokeWidth"] = pw;
            }

            obj["rotation"] = item->rotation();

            if (pen.style() != Qt::NoPen)
                obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));

            circlesArr.append(obj);
        }
    }

    // --- freeLines ---
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
                if (el.isMoveTo() || el.isLineTo()) {
                    QJsonObject pt;
                    pt["x"] = scenePos.x() + el.x;
                    pt["y"] = scenePos.y() + el.y;
                    pointsArr.append(pt);
                }
            }

            QJsonObject obj;
            obj["points"] = pointsArr;
            obj["z"] = ci->z;
            obj["strokeColor"] = penColor(gi);
            obj["strokeWidth"] = pen.widthF();
            obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));
            freeLinesArr.append(obj);
        }
    }

    // --- bezierCurves ---
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
                if (el.isMoveTo() || el.isLineTo() || el.isCurveTo()) {
                    QJsonObject pt;
                    pt["x"] = scenePos.x() + el.x;
                    pt["y"] = scenePos.y() + el.y;
                    cpArr.append(pt);
                }
            }

            QJsonObject obj;
            obj["controlPoints"] = cpArr;
            obj["z"] = ci->z;
            obj["strokeColor"] = penColor(gi);
            obj["strokeWidth"] = pen.widthF();
            obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));
            bezierArr.append(obj);
        }
    }

    // --- lines ---
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
            obj["x1"] = scenePos.x() + line.x1();
            obj["y1"] = scenePos.y() + line.y1();
            obj["x2"] = scenePos.x() + line.x2();
            obj["y2"] = scenePos.y() + line.y2();
            obj["z"] = ci->z;
            obj["strokeColor"] = penColor(gi);
            obj["strokeWidth"] = pen.widthF();
            obj["lineStyle"] = QString::fromLatin1(lineStyleString(pen.style()));
            linesArr.append(obj);
        }
    }

    // --- texts ---
    QJsonArray textsArr;
    if (!texts.empty()) {
        for (const auto &ci : texts) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPointF scenePos = item->scenePos();
            QFont font = gi->itemFont();
            QString content = gi->text();

            // 文字颜色
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

            QJsonObject obj;
            obj["x"] = scenePos.x();
            obj["y"] = scenePos.y();
            obj["z"] = ci->z;
            obj["content"] = content; // QJsonObject 自动处理 Unicode 和转义
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
    QJsonArray imagesArr;
    if (!images.empty()) {
        for (const auto &ci : images) {
            auto *item = ci->item;
            auto *gi = ci->gi;
            QPointF scenePos = item->scenePos();
            QRectF r = item->boundingRect();
            QString filePath = gi->filePath();

            QJsonObject obj;
            obj["filePath"] = filePath; // QJsonObject 自动转义反斜杠
            obj["x"] = scenePos.x() + r.x();
            obj["y"] = scenePos.y() + r.y();
            obj["width"] = r.width();
            obj["height"] = r.height();
            obj["z"] = ci->z;
            imagesArr.append(obj);
        }
    }

    // 4. 组装顶层 canvas JSON
    QJsonObject canvasObj;
    canvasObj["width"] = width;
    canvasObj["height"] = height;
    canvasObj["dpi"] = dpi;
    canvasObj["unit"] = QStringLiteral("px");

    // 背景色（白色）
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

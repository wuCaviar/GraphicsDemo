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
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <sstream>
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

// 颜色 → EE JSON 片段（纯色，无渐变）
// 优先使用存储的 CMYK 值，否则从 QColor 转 RGBA
std::string colorToJson(IGraphicsItem *gi, bool isPen, const QColor &fallback)
{
    std::ostringstream os;
    double c = 0, m = 0, y = 0, k = 0;

    if (isPen && gi->hasPenCmyk()) {
        gi->penCmyk(c, m, y, k);
        os << "{\"space\":\"cmyk\",\"c\":" << c << ",\"m\":" << m << ",\"y\":" << y
           << ",\"k\":" << k << ",\"a\":1.0}";
    } else if (!isPen && gi->hasBrushCmyk()) {
        gi->brushCmyk(c, m, y, k);
        os << "{\"space\":\"cmyk\",\"c\":" << c << ",\"m\":" << m << ",\"y\":" << y
           << ",\"k\":" << k << ",\"a\":1.0}";
    } else {
        QColor col = fallback.isValid() ? fallback : QColor(0, 0, 0);
        os << "{\"space\":\"rgba\",\"r\":" << col.red() << ",\"g\":" << col.green()
           << ",\"b\":" << col.blue() << ",\"a\":" << (col.alphaF() >= 0 ? col.alphaF() : 1.0)
           << "}";
    }
    return os.str();
}

// 渐变停止点颜色 → JSON（优先 CMYK，否则 RGBA）
std::string gradientStopColorToJson(IGraphicsItem *gi, double pos, const QColor &qtColor)
{
    std::ostringstream os;
    double c = 0, m = 0, y = 0, k = 0;

    if (gi->hasGradientStopCmyk(pos)) {
        gi->gradientStopCmyk(pos, c, m, y, k);
        os << "{\"space\":\"cmyk\",\"c\":" << c << ",\"m\":" << m << ",\"y\":" << y
           << ",\"k\":" << k << ",\"a\":1.0}";
    } else {
        QColor col = qtColor.isValid() ? qtColor : QColor(0, 0, 0);
        os << "{\"space\":\"rgba\",\"r\":" << col.red() << ",\"g\":" << col.green()
           << ",\"b\":" << col.blue() << ",\"a\":" << (col.alphaF() >= 0 ? col.alphaF() : 1.0)
           << "}";
    }
    return os.str();
}

// 从 QBrush 提取渐变信息并写入 JSON
// 返回 true 表示写入了渐变数据
bool writeGradientJson(std::ostringstream &os, IGraphicsItem *gi, const QBrush &brush,
                       const QRectF &itemRect)
{
    const QGradient *gradient = brush.gradient();
    if (!gradient)
        return false;

    const auto stops = gradient->stops();
    if (stops.size() < 2)
        return false;

    os << ",\"gradient\":{";

    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        auto *linear = static_cast<const QLinearGradient *>(gradient);
        QPointF start = linear->start();
        QPointF end = linear->finalStop();

        // 转换为归一化坐标（减去图元位置，除以尺寸）
        double nx1 = (start.x() - itemRect.left()) / itemRect.width();
        double ny1 = (start.y() - itemRect.top()) / itemRect.height();
        double nx2 = (end.x() - itemRect.left()) / itemRect.width();
        double ny2 = (end.y() - itemRect.top()) / itemRect.height();

        os << "\"type\":\"linear\""
           << ",\"x1\":" << nx1 << ",\"y1\":" << ny1 << ",\"x2\":" << nx2 << ",\"y2\":" << ny2;
        break;
    }
    case QGradient::RadialGradient: {
        auto *radial = static_cast<const QRadialGradient *>(gradient);
        QPointF center = radial->center();
        double radius = radial->radius();

        double cx = (center.x() - itemRect.left()) / itemRect.width();
        double cy = (center.y() - itemRect.top()) / itemRect.height();
        double r = radius / std::max(itemRect.width(), itemRect.height());

        os << "\"type\":\"radial\""
           << ",\"cx\":" << cx << ",\"cy\":" << cy << ",\"r\":" << r;
        break;
    }
    case QGradient::ConicalGradient: {
        auto *conical = static_cast<const QConicalGradient *>(gradient);
        QPointF center = conical->center();

        double cx = (center.x() - itemRect.left()) / itemRect.width();
        double cy = (center.y() - itemRect.top()) / itemRect.height();

        os << "\"type\":\"conic\""
           << ",\"cx\":" << cx << ",\"cy\":" << cy << ",\"startAngle\":" << conical->angle();
        break;
    }
    default:
        os << "\"type\":\"linear\",\"x1\":0,\"y1\":0,\"x2\":1,\"y2\":0";
        break;
    }

    // 停止点
    os << ",\"stops\":[";
    for (int i = 0; i < stops.size(); ++i) {
        if (i > 0)
            os << ",";
        os << "{\"offset\":" << stops[i].first
           << ",\"color\":" << gradientStopColorToJson(gi, stops[i].first, stops[i].second) << "}";
    }
    os << "]";

    os << "}";
    return true;
}

// fillColor JSON：纯色 或 纯色+渐变
// itemRect: 图元在场景坐标中的几何矩形（用于渐变归一化坐标计算）
std::string fillColorToJson(IGraphicsItem *gi, const QBrush &brush, const QRectF &itemRect)
{
    std::ostringstream os;
    os << "{";

    // 基础色（渐变时用作 fallback，实际渲染以 stops 为准）
    if (gi->hasBrushCmyk()) {
        double c, m, y, k;
        gi->brushCmyk(c, m, y, k);
        os << "\"space\":\"cmyk\",\"c\":" << c << ",\"m\":" << m << ",\"y\":" << y << ",\"k\":" << k
           << ",\"a\":1.0";
    } else {
        QColor col = brush.color();
        os << "\"space\":\"rgba\",\"r\":" << col.red() << ",\"g\":" << col.green()
           << ",\"b\":" << col.blue() << ",\"a\":" << (col.alphaF() >= 0 ? col.alphaF() : 1.0);
    }

    // 渐变（嵌入同一 JSON 对象）
    writeGradientJson(os, gi, brush, itemRect);

    os << "}";
    return os.str();
}

} // namespace

// ============================================================================
//  SceneToJsonConverter
// ============================================================================

QString SceneToJsonConverter::convert(QGraphicsScene *scene, const QString &rgbProfile,
                                      const QString &cmykProfile, QString *errorOut)
{
    auto fail = [&](const QString &msg) -> QString {
        if (errorOut)
            *errorOut = msg;
        return {};
    };

    if (!scene)
        return fail(QStringLiteral("Scene is null"));

    // 1. 查找 CanvasItem 获取画布尺寸和 DPI
    CanvasItem *canvasItem = nullptr;
    for (auto *item : scene->items()) {
        canvasItem = dynamic_cast<CanvasItem *>(item);
        if (canvasItem)
            break;
    }

    if (!canvasItem)
        return fail(QStringLiteral("No CanvasItem found in scene"));

    QRectF canvasRect = canvasItem->rect();
    int width = static_cast<int>(canvasRect.width());
    int height = static_cast<int>(canvasRect.height());
    int dpi = canvasItem->isDpiLocked() ? canvasItem->canvasDpiX() : 300;
    if (dpi <= 0)
        dpi = 300;

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

    // 3. 按类型分桶，构建 JSON
    std::ostringstream os;
    os << "{\"canvas\":{";
    os << "\"width\":" << width << ",\"height\":" << height << ",\"dpi\":" << dpi;

    if (!rgbProfile.isEmpty())
        os << ",\"rgbProfile\":\"" << rgbProfile.toStdString() << "\"";
    if (!cmykProfile.isEmpty())
        os << ",\"cmykProfile\":\"" << cmykProfile.toStdString() << "\"";

    // 分桶
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

    auto penColor = [](IGraphicsItem *gi, QGraphicsItem * /*item*/) -> std::string {
        return colorToJson(gi, true, gi->itemPen().color());
    };

    // --- rects ---
    if (!rects.empty()) {
        os << ",\"rects\":[";
        for (size_t i = 0; i < rects.size(); ++i) {
            const auto &ci = *rects[i];
            auto *item = ci.item;
            auto *gi = ci.gi;
            QPen pen = gi->itemPen();
            QBrush brush = gi->itemBrush();
            QPointF scenePos = item->scenePos();

            // 使用精确几何矩形（不含画笔边距），手动计算以避免 boundingRect 的额外边距
            QRectF geo = gi->supportsGeometryRect() ? gi->geometryRect() : item->boundingRect();
            double pw = (pen.style() != Qt::NoPen) ? pen.widthF() : 0.0;
            QRectF geomRect(geo.x() + pw / 2.0, geo.y() + pw / 2.0, geo.width() - pw,
                            geo.height() - pw);
            QRectF sceneRect(scenePos + geomRect.topLeft(), geomRect.size());

            if (i > 0)
                os << ",";
            os << "{\"x\":" << sceneRect.x() << ",\"y\":" << sceneRect.y()
               << ",\"width\":" << sceneRect.width() << ",\"height\":" << sceneRect.height()
               << ",\"z\":" << ci.z;

            // 填充
            if (brush.style() != Qt::NoBrush) {
                os << ",\"fillColor\":" << fillColorToJson(gi, brush, sceneRect);
            }

            // 描边
            if (pen.style() != Qt::NoPen && pw > 0) {
                os << ",\"strokeColor\":" << penColor(gi, item) << ",\"strokeWidth\":" << pw;
            }

            os << ",\"rotation\":" << item->rotation();

            // 圆角
            auto *rectItem = dynamic_cast<RectItem *>(item);
            if (rectItem) {
                qreal cr = rectItem->cornerRadius();
                if (cr > 0)
                    os << ",\"cornerRadius\":" << cr;
            }

            if (pen.style() != Qt::NoPen)
                os << ",\"lineStyle\":\"" << lineStyleString(pen.style()) << "\"";

            os << "}";
        }
        os << "]";
    }

    // --- circles ---
    if (!circles.empty()) {
        os << ",\"circles\":[";
        for (size_t i = 0; i < circles.size(); ++i) {
            const auto &ci = *circles[i];
            auto *item = ci.item;
            auto *gi = ci.gi;
            QPen pen = gi->itemPen();
            QBrush brush = gi->itemBrush();
            QPointF scenePos = item->scenePos();

            // 精确几何矩形（不含画笔边距）
            QRectF geo = gi->supportsGeometryRect() ? gi->geometryRect() : item->boundingRect();
            double pw = (pen.style() != Qt::NoPen) ? pen.widthF() : 0.0;
            QRectF geomRect(geo.x() + pw / 2.0, geo.y() + pw / 2.0, geo.width() - pw,
                            geo.height() - pw);

            double cx = scenePos.x() + geomRect.center().x();
            double cy = scenePos.y() + geomRect.center().y();
            double rx = geomRect.width() / 2.0;
            double ry = geomRect.height() / 2.0;
            QRectF sceneRect(scenePos + geomRect.topLeft(), geomRect.size());

            if (i > 0)
                os << ",";
            os << "{\"cx\":" << cx << ",\"cy\":" << cy << ",\"radiusX\":" << rx
               << ",\"radiusY\":" << ry << ",\"z\":" << ci.z;

            if (brush.style() != Qt::NoBrush) {
                os << ",\"fillColor\":" << fillColorToJson(gi, brush, sceneRect);
            }

            if (pen.style() != Qt::NoPen && pw > 0) {
                os << ",\"strokeColor\":" << penColor(gi, item) << ",\"strokeWidth\":" << pw;
            }

            os << ",\"rotation\":" << item->rotation();

            if (pen.style() != Qt::NoPen)
                os << ",\"lineStyle\":\"" << lineStyleString(pen.style()) << "\"";

            os << "}";
        }
        os << "]";
    }

    // --- freeLines ---
    if (!freelines.empty()) {
        os << ",\"freeLines\":[";
        for (size_t i = 0; i < freelines.size(); ++i) {
            const auto &ci = *freelines[i];
            auto *item = ci.item;
            auto *gi = ci.gi;
            QPen pen = gi->itemPen();
            QPointF scenePos = item->scenePos();

            // 使用 path() 而非 shape()：path() 返回原始控制点（item-local 坐标），
            // shape() 返回描边后的轮廓，不适合提取折线顶点
            auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
            QPainterPath path = pathItem ? pathItem->path() : item->shape();

            if (i > 0)
                os << ",";
            os << "{\"points\":[";

            bool first = true;
            for (int j = 0; j < path.elementCount(); ++j) {
                auto el = path.elementAt(j);
                if (el.isMoveTo() || el.isLineTo()) {
                    if (!first)
                        os << ",";
                    os << "{\"x\":" << scenePos.x() + el.x << ",\"y\":" << scenePos.y() + el.y
                       << "}";
                    first = false;
                }
            }

            os << "],\"z\":" << ci.z << ",\"strokeColor\":" << penColor(gi, item)
               << ",\"strokeWidth\":" << pen.widthF() << ",\"lineStyle\":\""
               << lineStyleString(pen.style()) << "\"}";
        }
        os << "]";
    }

    // --- bezierCurves ---
    if (!beziers.empty()) {
        os << ",\"bezierCurves\":[";
        for (size_t i = 0; i < beziers.size(); ++i) {
            const auto &ci = *beziers[i];
            auto *item = ci.item;
            auto *gi = ci.gi;
            QPen pen = gi->itemPen();
            QPointF scenePos = item->scenePos();

            auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
            QPainterPath path = pathItem ? pathItem->path() : QPainterPath();

            if (i > 0)
                os << ",";
            os << "{\"controlPoints\":[";

            bool first = true;
            for (int j = 0; j < path.elementCount(); ++j) {
                auto el = path.elementAt(j);
                if (el.isMoveTo() || el.isLineTo() || el.isCurveTo()) {
                    if (!first)
                        os << ",";
                    os << "{\"x\":" << scenePos.x() + el.x << ",\"y\":" << scenePos.y() + el.y
                       << "}";
                    first = false;
                }
            }

            os << "],\"z\":" << ci.z << ",\"strokeColor\":" << penColor(gi, item)
               << ",\"strokeWidth\":" << pen.widthF() << ",\"lineStyle\":\""
               << lineStyleString(pen.style()) << "\"}";
        }
        os << "]";
    }

    // --- lines ---
    if (!lines.empty()) {
        os << ",\"lines\":[";
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto &ci = *lines[i];
            auto *item = ci.item;
            auto *gi = ci.gi;
            QPen pen = gi->itemPen();
            QPointF scenePos = item->scenePos();

            // LineItem 的 line() 是本地坐标
            auto *lineItem = dynamic_cast<QGraphicsLineItem *>(item);
            QLineF line = lineItem ? lineItem->line() : QLineF();

            if (i > 0)
                os << ",";
            os << "{\"x1\":" << scenePos.x() + line.x1() << ",\"y1\":" << scenePos.y() + line.y1()
               << ",\"x2\":" << scenePos.x() + line.x2() << ",\"y2\":" << scenePos.y() + line.y2()
               << ",\"z\":" << ci.z << ",\"strokeColor\":" << penColor(gi, item)
               << ",\"strokeWidth\":" << pen.widthF() << ",\"lineStyle\":\""
               << lineStyleString(pen.style()) << "\"}";
        }
        os << "]";
    }

    // --- texts ---
    if (!texts.empty()) {
        os << ",\"texts\":[";
        for (size_t i = 0; i < texts.size(); ++i) {
            const auto &ci = *texts[i];
            auto *item = ci.item;
            auto *gi = ci.gi;
            QPointF scenePos = item->scenePos();
            QFont font = gi->itemFont();
            QString content = gi->text();

            // 文字颜色
            std::string textCol;
            if (gi->hasPenCmyk()) {
                double c, m, y, k;
                gi->penCmyk(c, m, y, k);
                std::ostringstream t;
                t << "{\"space\":\"cmyk\",\"c\":" << c << ",\"m\":" << m << ",\"y\":" << y
                  << ",\"k\":" << k << ",\"a\":1.0}";
                textCol = t.str();
            } else {
                QColor col = gi->itemPen().color();
                std::ostringstream t;
                t << "{\"space\":\"rgba\",\"r\":" << col.red() << ",\"g\":" << col.green()
                  << ",\"b\":" << col.blue()
                  << ",\"a\":" << (col.alphaF() >= 0 ? col.alphaF() : 1.0) << "}";
                textCol = t.str();
            }

            // 转义 JSON 字符串中的特殊字符
            std::string escaped;
            for (int j = 0; j < content.size(); ++j) {
                QChar ch = content.at(j);
                if (ch == '\"')
                    escaped += "\\\"";
                else if (ch == '\\')
                    escaped += "\\\\";
                else if (ch == '\n')
                    escaped += "\\n";
                else if (ch == '\r')
                    escaped += "\\r";
                else if (ch == '\t')
                    escaped += "\\t";
                else
                    escaped += ch.toLatin1();
            }

            // 字号：优先点值，若为像素字体则用 pixelSize
            double fontSize = font.pointSizeF();
            if (fontSize <= 0)
                fontSize = font.pixelSize();

            if (i > 0)
                os << ",";
            os << "{\"x\":" << scenePos.x() << ",\"y\":" << scenePos.y() << ",\"z\":" << ci.z
               << ",\"content\":\"" << escaped << "\""
               << ",\"fontFamily\":\"" << font.family().toStdString() << "\""
               << ",\"fontSize\":" << fontSize << ",\"bold\":" << (font.bold() ? "true" : "false")
               << ",\"italic\":" << (font.italic() ? "true" : "false")
               << ",\"textColor\":" << textCol << ",\"rotation\":" << item->rotation() << "}";
        }
        os << "]";
    }

    // --- images ---
    if (!images.empty()) {
        os << ",\"images\":[";
        for (size_t i = 0; i < images.size(); ++i) {
            const auto &ci = *images[i];
            auto *item = ci.item;
            auto *gi = ci.gi;
            QPointF scenePos = item->scenePos();
            QRectF r = item->boundingRect();
            QString filePath = gi->filePath();

            // 转义文件路径中的反斜杠
            std::string pathStr = filePath.toStdString();
            std::string escapedPath;
            for (char ch : pathStr) {
                if (ch == '\\' || ch == '\"')
                    escapedPath += "/";
                else
                    escapedPath += ch;
            }

            if (i > 0)
                os << ",";
            os << "{\"filePath\":\"" << escapedPath << "\""
               << ",\"x\":" << scenePos.x() + r.x() << ",\"y\":" << scenePos.y() + r.y()
               << ",\"width\":" << r.width() << ",\"height\":" << r.height() << ",\"z\":" << ci.z
               << "}";
        }
        os << "]";
    }

    os << "}}";

    return QString::fromStdString(os.str());
}

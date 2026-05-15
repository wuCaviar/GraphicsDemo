#include "qatgraphicsview.h"

#include "BezierCurveItem.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "EllipseItem.h"
#include "FreehandItem.h"
#include "GraphicsItemGroup.h"
#include "GraphicsScene.h"
#include "IGraphicsItem.h"
#include "ImageItem.h"
#include "ImageUtils.h"
#include "LineItem.h"
#include "RectItem.h"
#include "ResizeHandleItem.h"
#include "TextItem.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QUndoStack>
#include <QWheelEvent>
#include <QtMath>

#include <tiff.h>
#include <tiffio.h>

QAtGraphicsView::QAtGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    m_scene = new GraphicsScene(this);
    m_scene->setBackgroundBrush(QColor(255, 255, 255));
    setScene(m_scene);
    setDragMode(RubberBandDrag);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(FullViewportUpdate);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);

    m_defaultPen = QPen(Qt::black, 1.0);
    m_defaultBrush = QBrush(Qt::NoBrush);
    m_defaultFont = QFont("Arial", 14);

    // 将 scene 的 selectionChanged 转发为 view 的 selectionChanged，
    // 供 MainWindow → PropertyPanel 联动
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &QAtGraphicsView::selectionChanged);

    // 不创建默认画布 — 用户需要通过 New File 操作显式创建
    m_scene->setSceneRect(-500, -500, 1000, 1000);
}

QAtGraphicsView::~QAtGraphicsView()
{
    // m_scene is a child QObject, cleaned up by Qt object tree
    // GraphicsScene destructor handles ResizeHandle cleanup and signal disconnection
}

void QAtGraphicsView::initCanvas(const QSizeF &size)
{
    if (m_pCanvas) {
        m_scene->removeItem(m_pCanvas);
        delete m_pCanvas;
        m_pCanvas = nullptr;
    }

    m_pCanvas = new CanvasItem(size);
    m_scene->addItem(m_pCanvas);

    m_scene->setSceneRect(-500, -500, size.width() + 1000, size.height() + 1000);

    scrollToCanvasOrigin();
}

void QAtGraphicsView::setCanvasSize(const QSizeF &size)
{
    if (m_pCanvas) {
        m_pCanvas->setCanvasSize(size);
        m_scene->setSceneRect(-500, -500, size.width() + 1000, size.height() + 1000);
    } else {
        initCanvas(size);
    }
}

void QAtGraphicsView::clearScene()
{
    m_scene->clearScene();
    m_pCanvas = nullptr;
}

void QAtGraphicsView::resetCanvas(const QSizeF &size)
{
    clearScene();
    initCanvas(size);
}

void QAtGraphicsView::setZoomLevel(qreal level)
{
    level = qBound(0.1, level, 5.0);
    if (qFuzzyCompare(m_zoomLevel, level))
        return;

    qreal factor = level / m_zoomLevel;
    scale(factor, factor);
    m_zoomLevel = level;

    emit zoomChanged(m_zoomLevel);
}

void QAtGraphicsView::fitToCanvas()
{
    if (!m_pCanvas)
        return;

    resetTransform();
    m_zoomLevel = 1.0;

    fitInView(m_pCanvas->rect(), Qt::KeepAspectRatio);

    scale(0.9, 0.9);

    qreal actualScale = transform().m11();
    m_zoomLevel = qBound(0.1, actualScale, 5.0);

    emit zoomChanged(m_zoomLevel);
}

void QAtGraphicsView::scrollToCanvasOrigin()
{
    QMetaObject::invokeMethod(
        this, [this]() { centerOn(m_pCanvas ? m_pCanvas->rect().center() : QPointF(400, 560)); },
        Qt::QueuedConnection);
}

void QAtGraphicsView::setTool(Tool tool)
{
    if (m_drawing)
        finishDrawing();

    m_tool = tool;

    switch (tool) {
    case Tool::Select:
        setDragMode(RubberBandDrag);
        setCursor(Qt::ArrowCursor);
        break;
    case Tool::Hand:
        setDragMode(NoDrag);
        setCursor(Qt::OpenHandCursor);
        break;
    case Tool::Text:
    case Tool::Image:
        setDragMode(NoDrag);
        setCursor(Qt::ArrowCursor);
        break;
    default:
        setDragMode(NoDrag);
        setCursor(Qt::CrossCursor);
        break;
    }

    emit toolChanged(tool);
}

// ============================================================
// ResizeHandle / pasted-items — thin wrappers that delegate to GraphicsScene
// ============================================================

void QAtGraphicsView::setUndoStack(QUndoStack *stack)
{
    m_undoStack = stack;
    m_scene->setUndoStack(stack);
}

void QAtGraphicsView::scheduleResizeHandleUpdate()
{
    m_scene->scheduleResizeHandleUpdate();
}

void QAtGraphicsView::refreshResizeHandle()
{
    m_scene->refreshResizeHandle();
}

void QAtGraphicsView::setPastedItems(const QList<QGraphicsItem *> &items)
{
    m_scene->setPastedItems(items);
}

void QAtGraphicsView::clearPastedItems()
{
    m_scene->clearPastedItems();
}

// ============================================================
// 鼠标事件
// ============================================================
void QAtGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QPointF scenePos = mapToScene(event->pos());
    emit mousePositionChanged(scenePos);

    if (m_tool == Tool::Hand) {
        m_handPanning = true;
        m_handLastPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_tool == Tool::Select) {
        QGraphicsItem *hit = m_scene->itemAt(scenePos, transform());
        bool isCanvasOrHandle = hit && (hit->type() == CanvasItem::Type
                                        || hit->type() == ResizeHandleItem::Type);
        m_rubberBanding = (!hit || isCanvasOrHandle);
        if (m_rubberBanding)
            m_scene->scheduleResizeHandleUpdate();

        m_moving = true;
        m_moveStartPositions.clear();
        const auto selected = m_scene->selectedItems();
        for (auto *item : selected) {
            if (item->type() != CanvasItem::Type && item->type() != ResizeHandleItem::Type)
                m_moveStartPositions[item] = item->pos();
        }
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (m_tool == Tool::Image) {
        auto result = ImageUtils::importImageWithDialog(this);
        if (result.isValid()) {
            auto *item = new ImageItem(QPixmap::fromImage(result.image));
            item->setItemPen(QPen(Qt::NoPen));
            item->setFilePath(result.filePath);
            item->setRawTiffData(result.rawTiffData);
            item->setPos(scenePos);

            if (m_undoStack)
                m_undoStack->push(new AddItemCommand(m_scene, item));
            emit itemAdded(item);
        }
        return;
    }

    if (m_tool == Tool::Text) {
        auto *item = new TextItem(tr("Text"));
        item->setFont(m_defaultFont);
        item->setDefaultTextColor(m_defaultPen.color());
        item->setPos(scenePos);
        if (m_undoStack)
            m_undoStack->push(new AddItemCommand(m_scene, item));
        emit itemAdded(item);
        return;
    }

    m_drawing = true;
    m_startPos = scenePos;
    m_lastPos = scenePos;

    switch (m_tool) {
    case Tool::Rect: {
        auto *item = new RectItem(QRectF(scenePos, scenePos));
        item->setPen(m_defaultPen);
        item->setBrush(m_defaultBrush);
        m_tempItem = item;
        break;
    }
    case Tool::Ellipse: {
        auto *item = new EllipseItem(QRectF(scenePos, scenePos));
        item->setPen(m_defaultPen);
        item->setBrush(m_defaultBrush);
        m_tempItem = item;
        break;
    }
    case Tool::Line: {
        auto *item = new LineItem(QLineF(scenePos, scenePos));
        item->setPen(m_defaultPen);
        m_tempItem = item;
        break;
    }
    case Tool::BezierCurve: {
        auto *item = new BezierCurveItem();
        item->setPen(m_defaultPen);
        m_tempItem = item;
        break;
    }
    case Tool::Freehand: {
        auto *item = new FreehandItem();
        item->setPen(m_defaultPen);
        item->appendPoint(scenePos);
        m_tempItem = item;
        break;
    }
    default:
        break;
    }

    if (m_tempItem)
        m_scene->addItem(m_tempItem);
}

void QAtGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    emit mousePositionChanged(scenePos);

    if (m_handPanning) {
        QPoint delta = event->pos() - m_handLastPos;
        m_handLastPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        return;
    }

    if (m_tool != Tool::Select && !m_drawing) {
        return;
    }

    if (!m_drawing || !m_tempItem) {
        QGraphicsView::mouseMoveEvent(event);

        if (m_tool == Tool::Select && m_moving) {
            m_scene->syncResizeHandleDuringMove();
            emit selectionChanged();
        }
        return;
    }

    m_lastPos = scenePos;

    switch (m_tool) {
    case Tool::Rect: {
        auto *ri = qgraphicsitem_cast<RectItem *>(m_tempItem);
        if (ri)
            ri->setRect(QRectF(m_startPos, scenePos).normalized());
        break;
    }
    case Tool::Ellipse: {
        auto *ei = qgraphicsitem_cast<EllipseItem *>(m_tempItem);
        if (ei)
            ei->setRect(QRectF(m_startPos, scenePos).normalized());
        break;
    }
    case Tool::Line: {
        auto *li = qgraphicsitem_cast<LineItem *>(m_tempItem);
        if (li)
            li->setLine(QLineF(m_startPos, scenePos));
        break;
    }
    case Tool::BezierCurve: {
        auto *bi = qgraphicsitem_cast<BezierCurveItem *>(m_tempItem);
        if (bi) {
            QPointF mid = (m_startPos + scenePos) / 2;
            QPointF cp1(mid.x(), qMin(m_startPos.y(), scenePos.y()) - 50);
            QPointF cp2(mid.x(), qMax(m_startPos.y(), scenePos.y()) + 50);
            bi->setBezierCurve(m_startPos, cp1, cp2, scenePos);
        }
        break;
    }
    case Tool::Freehand: {
        auto *fi = qgraphicsitem_cast<FreehandItem *>(m_tempItem);
        if (fi)
            fi->appendPoint(scenePos);
        break;
    }
    default:
        break;
    }
}

void QAtGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    if (m_handPanning) {
        m_handPanning = false;
        setCursor(m_tool == Tool::Hand ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    m_rubberBanding = false;

    if (m_moving && m_tool == Tool::Select) {
        m_moving = false;
        QList<QGraphicsItem *> movedItems;
        QList<QPointF> oldPositions;
        QList<QPointF> newPositions;

        for (auto it = m_moveStartPositions.begin(); it != m_moveStartPositions.end(); ++it) {
            QGraphicsItem *item = it.key();
            QPointF oldPos = it.value();
            QPointF newPos = item->pos();
            if (oldPos != newPos) {
                movedItems << item;
                oldPositions << oldPos;
                newPositions << newPos;
            }
        }

        if (!movedItems.isEmpty() && m_undoStack) {
            m_undoStack->push(
                new MoveItemsCommand(movedItems, oldPositions, newPositions, m_scene));
        }
        m_moveStartPositions.clear();

        m_scene->syncResizeHandleDuringMove();
    }

    if (!m_drawing) {
        QGraphicsView::mouseReleaseEvent(event);
        m_scene->cleanupInvalidHandle();
        return;
    }

    finishDrawing();
}

void QAtGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QGraphicsView::mouseDoubleClickEvent(event);
}

// ============================================================
// 右键菜单
// ============================================================
void QAtGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    QGraphicsItem *hitItem = nullptr;
    const auto allItems = items(event->pos());
    for (auto *item : allItems) {
        if (item->type() != CanvasItem::Type && item->type() != ResizeHandleItem::Type) {
            hitItem = item;
            break;
        }
    }
    if (!hitItem)
        return;

    if (!hitItem->isSelected()) {
        m_scene->clearSelection();
        hitItem->setSelected(true);
    }

    auto selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty())
        return;

    QMenu menu;
    menu.addAction(QIcon(":/icons/icons/bring-front.svg"), tr("Bring to Front"),
                   this, &QAtGraphicsView::bringToFrontRequested);
    menu.addAction(QIcon(":/icons/icons/send-back.svg"), tr("Send to Back"),
                   this, &QAtGraphicsView::sendToBackRequested);

    menu.addSeparator();

    auto filtered = ::filterSelectableItems(selectedItems);
    int topLevelCount = 0;
    bool hasGroup = false;
    for (auto *item : filtered) {
        if (qgraphicsitem_cast<GraphicsItemGroup *>(item))
            hasGroup = true;
        if (!item->parentItem())
            topLevelCount++;
    }

    if (topLevelCount >= 2)
        menu.addAction(QIcon(":/icons/icons/group.svg"), tr("Group"), this, &QAtGraphicsView::groupRequested);
    if (hasGroup)
        menu.addAction(QIcon(":/icons/icons/ungroup.svg"), tr("Ungroup"), this, &QAtGraphicsView::ungroupRequested);

    menu.exec(event->globalPos());
}

void QAtGraphicsView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !m_drawing && !m_spaceHandMode) {
        m_previousTool = m_tool;
        m_spaceHandMode = true;
        setTool(Tool::Hand);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && m_drawing) {
        cancelDrawing();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void QAtGraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && m_spaceHandMode) {
        m_spaceHandMode = false;
        if (!m_handPanning) {
            setTool(m_previousTool);
        } else {
            m_tool = m_previousTool;
        }
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void QAtGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        qreal factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
        setZoomLevel(m_zoomLevel * factor);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void QAtGraphicsView::setGridVisible(bool visible)
{
    if (m_gridVisible == visible)
        return;
    m_gridVisible = visible;
    viewport()->update();
}

void QAtGraphicsView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, m_scene->backgroundBrush().color());

    if (!m_pCanvas)
        return;

    QRectF canvasRect = m_pCanvas->rect();

    painter->fillRect(canvasRect, Qt::white);

    QRectF rightShadow(canvasRect.right(), canvasRect.top() + 3,
                       6, canvasRect.height() - 3);
    QRectF bottomShadow(canvasRect.left() + 3, canvasRect.bottom(),
                        canvasRect.width() - 3, 6);
    QRectF cornerShadow(canvasRect.right(), canvasRect.bottom(), 6, 6);
    QColor shadowColor(60, 60, 60, 80);
    painter->fillRect(rightShadow, shadowColor);
    painter->fillRect(bottomShadow, shadowColor);
    painter->fillRect(cornerShadow, shadowColor);

    if (m_gridVisible) {
        QRectF gridRect = rect.intersected(canvasRect);
        if (!gridRect.isValid())
            return;

        qreal baseInterval = 10.0;
        if (m_zoomLevel < 0.2)
            baseInterval = 100;
        else if (m_zoomLevel < 0.5)
            baseInterval = 50;
        else if (m_zoomLevel < 1.0)
            baseInterval = 20;
        else if (m_zoomLevel < 2.0)
            baseInterval = 10;
        else if (m_zoomLevel < 4.0)
            baseInterval = 10;
        else
            baseInterval = 5;

        qreal majorInterval = baseInterval * 5;

        painter->setClipRect(gridRect);

        QPen minorPen(QColor(0, 0, 0, 20));
        minorPen.setWidthF(0.5);
        painter->setPen(minorPen);

        qreal startX = qFloor(gridRect.left() / baseInterval) * baseInterval;
        qreal startY = qFloor(gridRect.top() / baseInterval) * baseInterval;

        for (qreal x = startX; x <= gridRect.right(); x += baseInterval) {
            if (qFuzzyCompare(qRound(x / majorInterval) * majorInterval, x))
                continue;
            painter->drawLine(QPointF(x, gridRect.top()), QPointF(x, gridRect.bottom()));
        }
        for (qreal y = startY; y <= gridRect.bottom(); y += baseInterval) {
            if (qFuzzyCompare(qRound(y / majorInterval) * majorInterval, y))
                continue;
            painter->drawLine(QPointF(gridRect.left(), y), QPointF(gridRect.right(), y));
        }

        QPen majorPen(QColor(0, 0, 0, 40));
        majorPen.setWidthF(0.8);
        painter->setPen(majorPen);

        qreal majorStartX = qFloor(gridRect.left() / majorInterval) * majorInterval;
        qreal majorStartY = qFloor(gridRect.top() / majorInterval) * majorInterval;

        for (qreal x = majorStartX; x <= gridRect.right(); x += majorInterval) {
            painter->drawLine(QPointF(x, gridRect.top()), QPointF(x, gridRect.bottom()));
        }
        for (qreal y = majorStartY; y <= gridRect.bottom(); y += majorInterval) {
            painter->drawLine(QPointF(gridRect.left(), y), QPointF(gridRect.right(), y));
        }

        painter->setClipping(false);
    }
}

void QAtGraphicsView::finishDrawing()
{
    if (!m_tempItem)
        return;

    m_drawing = false;

    QRectF br = m_tempItem->boundingRect();
    if (br.width() < 3 && br.height() < 3) {
        m_scene->removeItem(m_tempItem);
        delete m_tempItem;
        m_tempItem = nullptr;
        return;
    }

    m_scene->removeItem(m_tempItem);

    if (m_undoStack)
        m_undoStack->push(new AddItemCommand(m_scene, m_tempItem));

    emit itemAdded(m_tempItem);
    m_tempItem = nullptr;
}

void QAtGraphicsView::cancelDrawing()
{
    if (m_tempItem) {
        m_scene->removeItem(m_tempItem);
        delete m_tempItem;
        m_tempItem = nullptr;
    }
    m_drawing = false;
}

#include "qatgraphicsview.h"
#include "atMath.h"

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
#include <QEvent>
#include <cmath>

#include <tiff.h>
#include <tiffio.h>

QAtGraphicsView::QAtGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    m_scene = new GraphicsScene(this);
    m_scene->setBackgroundBrush(QColor(171, 171, 171));
    setScene(m_scene);
    setDragMode(RubberBandDrag);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(MinimalViewportUpdate);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);
    setOptimizationFlags(DontAdjustForAntialiasing | DontSavePainterState);
    setCacheMode(CacheBackground); // cache grid background as pixmap — invalidated on zoom/resize

    m_defaultPen = QPen(Qt::black, 1.0);
    m_defaultBrush = QBrush(Qt::black);
    m_defaultFont = QFont("Arial", 14);

    // 将 scene 的 selectionChanged 转发为 view 的 selectionChanged，
    // 供 MainWindow → PropertyPanel 联动
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &QAtGraphicsView::selectionChanged);

    // 不创建默认画布 — 用户需要通过 New File 操作显式创建
    m_scene->setSceneRect(-500, -500, 1000, 1000);
}

QAtGraphicsView::~QAtGraphicsView()
{
    if (m_scene)
        m_scene->disconnect();
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
    resetCachedContent(); // canvas rect changed — grid cache stale

    scrollToCanvasOrigin();
}

void QAtGraphicsView::setCanvasSize(const QSizeF &size)
{
    if (m_pCanvas) {
        m_pCanvas->setCanvasSize(size);
        m_scene->setSceneRect(-500, -500, size.width() + 1000, size.height() + 1000);
        resetCachedContent(); // canvas rect changed — grid cache stale
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
    level = AtMath::clamp(level, 0.01, 32.0);
    if (AtMath::isEqual(m_zoomLevel, level))
        return;

    qreal factor = level / m_zoomLevel;
    scale(factor, factor);
    m_zoomLevel = level;
    resetCachedContent(); // grid cache invalid since zoom changed

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
    m_zoomLevel = AtMath::clamp(actualScale, 0.01, 32.0);
    resetCachedContent(); // grid cache invalid since zoom changed

    emit zoomChanged(m_zoomLevel);
}

void QAtGraphicsView::fitToSelection()
{
    if (!scene())
        return;

    auto items = scene()->selectedItems();
    if (items.isEmpty()) {
        fitToCanvas();
        return;
    }

    QRectF rect;
    for (auto *item : items)
        rect = rect.united(item->sceneBoundingRect());

    if (rect.isEmpty())
        return;

    resetTransform();
    m_zoomLevel = 1.0;

    fitInView(rect, Qt::KeepAspectRatio);

    scale(0.9, 0.9);

    qreal actualScale = transform().m11();
    m_zoomLevel = AtMath::clamp(actualScale, 0.01, 32.0);
    resetCachedContent();

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
        viewport()->setCursor(Qt::ArrowCursor);
        break;
    case Tool::Hand:
        setDragMode(NoDrag);
        viewport()->setCursor(Qt::OpenHandCursor);
        break;
    case Tool::Text:
        setDragMode(NoDrag);
        viewport()->setCursor(Qt::ArrowCursor);
        break;
    default:
        setDragMode(NoDrag);
        viewport()->setCursor(Qt::CrossCursor);
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
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_tool == Tool::Select) {
        QGraphicsItem *hit = m_scene->itemAt(scenePos, transform());
        bool isCanvasOrHandle =
            hit && (hit->type() == CanvasItem::Type || hit->type() == ResizeHandleItem::Type);
        m_rubberBanding = (!hit || isCanvasOrHandle);
        if (m_rubberBanding)
            m_scene->scheduleResizeHandleUpdate();

        // Disable ItemIsMovable on selected items so Qt won't start
        // per-item drag — we handle drag ourselves via ghost outline.
        const auto selectedBefore = m_scene->selectedItems();
        for (auto *item : selectedBefore) {
            if (item->flags() & QGraphicsItem::ItemIsMovable) {
                m_ghostMovableStash.insert(item);
                item->setFlag(QGraphicsItem::ItemIsMovable, false);
            }
        }

        m_moving = true;
        QGraphicsView::mousePressEvent(event); // 先让 Qt 处理选中变更，再捕获位置

        m_moveStartPositions.clear();
        const auto selected = m_scene->selectedItems();
        for (auto *item : selected) {
            if (item->type() != CanvasItem::Type && item->type() != ResizeHandleItem::Type)
                m_moveStartPositions[item] = item->pos();
        }

        if (!m_rubberBanding && !selected.isEmpty()) {
            // Ghost-Drag: 隐藏原图元，创建虚线轮廓跟随鼠标
            QPainterPath path = buildGhostPath();
            if (!path.isEmpty()) {
                m_ghostPressScenePos = scenePos;

                QPen ghostPen(QColor(0, 120, 212)); // 蓝色，区分于选中框黑色虚线
                ghostPen.setStyle(Qt::DashLine);
                ghostPen.setCosmetic(true);
                ghostPen.setWidthF(1.5);

                m_ghostItem = new QGraphicsPathItem();
                m_ghostItem->setPath(path);
                m_ghostItem->setPen(ghostPen);
                m_ghostItem->setBrush(Qt::NoBrush);
                m_ghostItem->setZValue(9999);
                m_ghostItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
                m_ghostItem->setFlag(QGraphicsItem::ItemIsMovable, false);
                m_ghostItem->setAcceptHoverEvents(false);
                m_scene->addItem(m_ghostItem);

                // Keep ItemIsMovable off during ghost drag
                // (restored in endGhostDrag / cancelGhostDrag)
                m_ghostDragging = true;
            } else {
                // No valid path — restore ItemIsMovable, fall through to normal drag
                for (auto *item : m_ghostMovableStash)
                    item->setFlag(QGraphicsItem::ItemIsMovable, true);
                m_ghostMovableStash.clear();
            }
        } else {
            // Rubber-banding or no selection — restore ItemIsMovable immediately
            for (auto *item : m_ghostMovableStash)
                item->setFlag(QGraphicsItem::ItemIsMovable, true);
            m_ghostMovableStash.clear();
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
        item->setItemPenCmyk(0, 0, 0, 100);

        item->setBrush(m_defaultBrush);
        item->setItemBrushCmyk(0, 0, 0, 100);
        m_tempItem = item;
        break;
    }
    case Tool::Ellipse: {
        auto *item = new EllipseItem(QRectF(scenePos, scenePos));
        item->setPen(m_defaultPen);
        item->setItemPenCmyk(0, 0, 0, 100);

        item->setBrush(m_defaultBrush);
        item->setItemBrushCmyk(0, 0, 0, 100);
        m_tempItem = item;
        break;
    }
    case Tool::Line: {
        auto *item = new LineItem(QLineF(scenePos, scenePos));
        item->setPen(m_defaultPen);
        item->setItemPenCmyk(0, 0, 0, 100);
        m_tempItem = item;
        break;
    }
    case Tool::BezierCurve: {
        auto *item = new BezierCurveItem();
        item->setPen(m_defaultPen);
        item->setItemPenCmyk(0, 0, 0, 100);
        m_tempItem = item;
        break;
    }
    case Tool::Freehand: {
        auto *item = new FreehandItem();
        item->setPen(m_defaultPen);
        item->setItemPenCmyk(0, 0, 0, 100);
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
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    // Ghost-Drag: manually move the lightweight outline, skip all
    // expensive per-frame operations (handle sync, selection emit, item paint).
    if (m_ghostDragging && m_ghostItem) {
        m_ghostItem->setPos(scenePos - m_ghostPressScenePos);
        return;
    }

    if (m_tool != Tool::Select && !m_drawing) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (!m_drawing || !m_tempItem) {
        QGraphicsView::mouseMoveEvent(event);

        if (m_tool == Tool::Select && m_moving) {
            // Throttle: sync handle only every 3rd frame to reduce per-frame cost.
            // At 60 fps this still updates handles ~20 fps — imperceptibly smooth.
            m_dragThrottle = (m_dragThrottle + 1) % 3;
            if (m_dragThrottle == 0) {
                m_scene->syncResizeHandleDuringMove();
                emit selectionChanged();
            }
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
        viewport()->setCursor(m_tool == Tool::Hand ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    m_rubberBanding = false;

    // Ghost-Drag completion: teleport real items to ghost position
    if (m_ghostDragging) {
        endGhostDrag();
        m_moving = false;
        QGraphicsView::mouseReleaseEvent(event);
        m_scene->cleanupInvalidHandle();
        return;
    }

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
    menu.addAction(QIcon(":/icons/icons/bring-front.svg"), tr("Bring Forward"), this,
                   &QAtGraphicsView::bringToFrontRequested)
        ->setToolTip(tr("Bring selected items forward one step"));
    menu.addAction(QIcon(":/icons/icons/send-back.svg"), tr("Send Backward"), this,
                   &QAtGraphicsView::sendToBackRequested)
        ->setToolTip(tr("Send selected items backward one step"));

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
        menu.addAction(QIcon(":/icons/icons/group.svg"), tr("Group"), this,
                       &QAtGraphicsView::groupRequested)
            ->setToolTip(tr("Group selected items together"));
    if (hasGroup)
        menu.addAction(QIcon(":/icons/icons/ungroup.svg"), tr("Ungroup"), this,
                       &QAtGraphicsView::ungroupRequested)
            ->setToolTip(tr("Ungroup selected items"));

    if (!filtered.isEmpty() && m_pCanvas) {
        menu.addSeparator();
        menu.addAction(tr("Fit Canvas to Selection"), this,
                       &QAtGraphicsView::fitCanvasToItemsRequested)
            ->setToolTip(tr("Resize the canvas to fit the selected items"));
    }

    menu.exec(event->globalPos());
}

void QAtGraphicsView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !m_drawing && !m_ghostDragging && !m_spaceHandMode) {
        m_previousTool = m_tool;
        m_spaceHandMode = true;
        setTool(Tool::Hand);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        if (m_ghostDragging) {
            cancelGhostDrag();
            event->accept();
            return;
        }
        if (m_drawing) {
            cancelDrawing();
            event->accept();
            return;
        }
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
    resetCachedContent(); // grid is part of cached background
    viewport()->update();
}

bool QAtGraphicsView::isDarkTheme() const
{
    const QPalette &pal = palette();
    return pal.color(QPalette::Window).value() < pal.color(QPalette::WindowText).value();
}

void QAtGraphicsView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange) {
        viewport()->update();
    }
    QGraphicsView::changeEvent(event);
}

void QAtGraphicsView::drawBackground(QPainter *painter, const QRectF &rect)
{
    // Derive colors from palette so grid adapts to QSS theme changes
    const QPalette &pal = palette();
    QColor sceneBg = pal.color(QPalette::Window);
    // Slightly tinted grid lines relative to background
    QColor canvasBg = pal.color(QPalette::Base);
    if (canvasBg == sceneBg || canvasBg == QPalette().color(QPalette::Base))
        canvasBg = Qt::white;

    painter->fillRect(rect, sceneBg);

    if (!m_pCanvas)
        return;

    QRectF canvasRect = m_pCanvas->rect();
    painter->fillRect(canvasRect, canvasBg);

    // Shadow (subtle, alpha-blended with bg)
    QColor shadowColor = sceneBg.darker(130);
    shadowColor.setAlpha(60);
    QRectF rightShadow(canvasRect.right(), canvasRect.top() + 3, 6, canvasRect.height() - 3);
    QRectF bottomShadow(canvasRect.left() + 3, canvasRect.bottom(), canvasRect.width() - 3, 6);
    QRectF cornerShadow(canvasRect.right(), canvasRect.bottom(), 6, 6);
    painter->fillRect(rightShadow, shadowColor);
    painter->fillRect(bottomShadow, shadowColor);
    painter->fillRect(cornerShadow, shadowColor);

    if (m_gridVisible) {
        QRectF gridRect = rect.intersected(canvasRect);
        if (!gridRect.isValid())
            return;

        // Grid aligned with ruler: mm-based spacing derived from canvas PPI.
        // Grid lines land on integer-mm positions matching ruler tick marks.
        // Spacing auto-adapts to zoom so cells stay ~50 screen pixels wide.
        const qreal ppi = m_pCanvas->ppi();
        const qreal pxPerMm = AtMath::Units::DPIContext(ppi).mmToPx(1.0); // scene pixels per mm

        static constexpr qreal kTargetScreenPx = 50.0;
        qreal idealMm = kTargetScreenPx / (pxPerMm * m_zoomLevel);

        // Snap to 1-2-5 nice mm values
        qreal mag = std::pow(10.0, std::floor(std::log10(idealMm)));
        qreal norm = idealMm / mag;
        if (norm < 1.5)
            idealMm = 1.0 * mag;
        else if (norm < 3.5)
            idealMm = 2.0 * mag;
        else if (norm < 7.5)
            idealMm = 5.0 * mag;
        else
            idealMm = 10.0 * mag;
        qreal mmInterval = qMax(1.0, idealMm);

        qreal baseInterval = mmInterval * pxPerMm; // scene-pixel interval

        // Cap max interval so grid doesn't vanish at very low zoom
        static constexpr int kMinGridCells = 5;
        qreal maxInterval = qMin(canvasRect.width(), canvasRect.height()) / kMinGridCells;
        if (baseInterval > maxInterval)
            baseInterval = maxInterval;

        qreal majorInterval = baseInterval * 5;

        painter->setClipRect(gridRect);

        // Minor grid — derived from WindowText at low alpha
        QColor minorColor = pal.color(QPalette::WindowText);
        minorColor.setAlpha(isDarkTheme() ? 15 : 20);
        QPen minorPen(minorColor);
        minorPen.setCosmetic(true); // fixed 1px width, doesn't scale with zoom
        painter->setPen(minorPen);

        // Start from 0 so grid lines land exactly on ruler mm marks
        qreal startX = qFloor(gridRect.left() / baseInterval) * baseInterval;
        qreal startY = qFloor(gridRect.top() / baseInterval) * baseInterval;

        for (qreal x = startX; x <= gridRect.right(); x += baseInterval) {
            if (std::fmod(std::abs(x), majorInterval) < baseInterval * 0.01)
                continue;
            painter->drawLine(QPointF(x, gridRect.top()), QPointF(x, gridRect.bottom()));
        }
        for (qreal y = startY; y <= gridRect.bottom(); y += baseInterval) {
            if (std::fmod(std::abs(y), majorInterval) < baseInterval * 0.01)
                continue;
            painter->drawLine(QPointF(gridRect.left(), y), QPointF(gridRect.right(), y));
        }

        // Major grid — slightly more opaque, also cosmetic (fixed width)
        QColor majorColor = pal.color(QPalette::WindowText);
        majorColor.setAlpha(isDarkTheme() ? 30 : 40);
        QPen majorPen(majorColor);
        majorPen.setCosmetic(true); // fixed width, doesn't scale with zoom
        painter->setPen(majorPen);

        qreal majorStartX = qFloor(gridRect.left() / majorInterval) * majorInterval;
        qreal majorStartY = qFloor(gridRect.top() / majorInterval) * majorInterval;

        for (qreal x = majorStartX; x <= gridRect.right(); x += majorInterval)
            painter->drawLine(QPointF(x, gridRect.top()), QPointF(x, gridRect.bottom()));
        for (qreal y = majorStartY; y <= gridRect.bottom(); y += majorInterval)
            painter->drawLine(QPointF(gridRect.left(), y), QPointF(gridRect.right(), y));

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
    // Enable device-coordinate cache for drag performance.
    // Items drawn frame-by-frame (Rect, Ellipse, Line, Bezier, Freehand) would
    // thrash the cache during creation, so we enable it only once stable.
    m_tempItem->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    if (m_undoStack)
        m_undoStack->push(new AddItemCommand(m_scene, m_tempItem));

    emit itemAdded(m_tempItem);
    m_tempItem = nullptr;
}

// ============================================================
// Ghost-Drag: 拖拽时仅绘制虚线轮廓，松手后瞬移原图元
// ============================================================
QPainterPath QAtGraphicsView::buildGhostPath() const
{
    QPainterPath combined;
    const auto selected = m_scene->selectedItems();
    for (auto *item : selected) {
        if (item->type() == CanvasItem::Type || item->type() == ResizeHandleItem::Type)
            continue;
        auto *igi = dynamic_cast<IGraphicsItem *>(item);
        QPainterPath itemPath;
        if (igi && igi->supportsGeometryRect()) {
            itemPath.addRect(igi->geometryRect());
        } else {
            itemPath = item->shape();
        }
        itemPath.translate(item->pos());
        combined.addPath(itemPath);
    }
    return combined;
}

void QAtGraphicsView::endGhostDrag()
{
    if (!m_ghostItem)
        return;

    const QPointF delta = m_ghostItem->pos();

    // Remove ghost from scene
    m_scene->removeItem(m_ghostItem);
    delete m_ghostItem;
    m_ghostItem = nullptr;
    m_ghostDragging = false;

    // Restore ItemIsMovable on stashed items
    for (auto *item : m_ghostMovableStash)
        item->setFlag(QGraphicsItem::ItemIsMovable, true);
    m_ghostMovableStash.clear();

    // Teleport real items to new positions
    QList<QGraphicsItem *> movedItems;
    QList<QPointF> oldPositions;
    QList<QPointF> newPositions;
    for (auto it = m_moveStartPositions.begin(); it != m_moveStartPositions.end(); ++it) {
        QGraphicsItem *item = it.key();
        QPointF oldPos = it.value();
        QPointF newPos = oldPos + delta;
        if (oldPos != newPos) {
            item->setPos(newPos);
            movedItems << item;
            oldPositions << oldPos;
            newPositions << newPos;
        }
    }

    if (!movedItems.isEmpty() && m_undoStack)
        m_undoStack->push(new MoveItemsCommand(movedItems, oldPositions, newPositions, m_scene));

    m_moveStartPositions.clear();
    m_scene->syncResizeHandleDuringMove();
}

void QAtGraphicsView::cancelGhostDrag()
{
    if (!m_ghostItem)
        return;

    m_scene->removeItem(m_ghostItem);
    delete m_ghostItem;
    m_ghostItem = nullptr;
    m_ghostDragging = false;

    // Restore ItemIsMovable
    for (auto *item : m_ghostMovableStash)
        item->setFlag(QGraphicsItem::ItemIsMovable, true);
    m_ghostMovableStash.clear();

    m_moveStartPositions.clear();
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

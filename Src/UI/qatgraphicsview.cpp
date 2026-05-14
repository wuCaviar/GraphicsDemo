#include "qatgraphicsview.h"

#include "BezierCurveItem.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "EllipseItem.h"
#include "FreehandItem.h"
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
    m_pMainScene = new QGraphicsScene(this);
    m_pMainScene->setBackgroundBrush(QColor(255, 255, 255));
    setScene(m_pMainScene);
    setDragMode(RubberBandDrag);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(FullViewportUpdate);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);

    // 默认样式
    m_defaultPen = QPen(Qt::black, 1.0);
    m_defaultBrush = QBrush(Qt::NoBrush);
    m_defaultFont = QFont("Arial", 14);

    // 初始化默认画布 (A4)
    initCanvas(QSizeF(793.7, 1122.5));

    connect(m_pMainScene, &QGraphicsScene::selectionChanged, this,
            &QAtGraphicsView::onSceneSelectionChanged);
}

QAtGraphicsView::~QAtGraphicsView()
{
    // m_pMainScene 是 this 的子对象，Qt 对象树会自动删除，无需手动 deleteLater()
    // 先断开信号，防止析构期间 selectionChanged 触发 updateResizeHandle 访问半销毁状态
    disconnect(m_pMainScene, &QGraphicsScene::selectionChanged, this,
               &QAtGraphicsView::onSceneSelectionChanged);
    removeResizeHandle();
}

void QAtGraphicsView::initCanvas(const QSizeF &size)
{
    // 移除旧画布
    if (m_pCanvas) {
        m_pMainScene->removeItem(m_pCanvas);
        delete m_pCanvas;
        m_pCanvas = nullptr;
    }

    // 创建新画布，左上角在 (0,0)
    m_pCanvas = new CanvasItem(size);
    m_pMainScene->addItem(m_pCanvas);

    // 设置场景范围（比画布大，留出灰色工作区）
    m_pMainScene->setSceneRect(-500, -500, size.width() + 1000, size.height() + 1000);

    // 滚动到画布左上角
    scrollToCanvasOrigin();
}

void QAtGraphicsView::setCanvasSize(const QSizeF &size)
{
    if (m_pCanvas) {
        m_pCanvas->setCanvasSize(size);
        m_pMainScene->setSceneRect(-500, -500, size.width() + 1000, size.height() + 1000);
    } else {
        initCanvas(size);
    }
}

void QAtGraphicsView::clearScene()
{
    // 断开信号，避免 clear() 过程中 selectionChanged 触发 updateResizeHandle 访问悬空指针
    disconnect(m_pMainScene, &QGraphicsScene::selectionChanged, this,
               &QAtGraphicsView::onSceneSelectionChanged);

    // 清空 ResizeHandle（必须在 clear 之前，否则 m_target 悬空）
    removeResizeHandle();

    // 清空粘贴图元追踪
    m_pastedItems.clear();

    // 清空场景（删除所有 item，包括 CanvasItem）
    m_pMainScene->clear();

    // 指针已被 clear() 释放，必须置空
    m_pCanvas = nullptr;

    // 重新连接信号
    connect(m_pMainScene, &QGraphicsScene::selectionChanged, this,
            &QAtGraphicsView::onSceneSelectionChanged);
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

    // 计算当前视口中心对应的场景坐标，缩放后仍保持居中
    QPointF center = mapToScene(viewport()->rect().center());

    qreal factor = level / m_zoomLevel;
    scale(factor, factor);
    m_zoomLevel = level;

    // 恢复视口中心
    centerOn(center);

    emit zoomChanged(m_zoomLevel);
}

void QAtGraphicsView::fitToCanvas()
{
    if (!m_pCanvas)
        return;

    // 先重置变换矩阵，避免 fitInView 在已有缩放基础上叠加
    resetTransform();
    m_zoomLevel = 1.0;

    // fitInView 会自动计算并设置缩放+平移，使画布完整显示
    fitInView(m_pCanvas->rect(), Qt::KeepAspectRatio);

    // 稍微缩小一点留边
    scale(0.9, 0.9);

    // 从变换矩阵反算实际缩放比
    qreal actualScale = transform().m11();
    m_zoomLevel = qBound(0.1, actualScale, 5.0);

    emit zoomChanged(m_zoomLevel);
}

void QAtGraphicsView::scrollToCanvasOrigin()
{
    // 确保画布左上角 (0,0) 在视图的合适位置
    QMetaObject::invokeMethod(
        this, [this]() { centerOn(m_pCanvas ? m_pCanvas->rect().center() : QPointF(400, 560)); },
        Qt::QueuedConnection);
}

void QAtGraphicsView::setTool(Tool tool)
{
    // 如果正在绘制，先完成
    if (m_drawing)
        finishDrawing();

    m_tool = tool;

    // 根据工具设置拖拽模式和光标
    switch (tool) {
    case Tool::Select:
        setDragMode(RubberBandDrag);
        setCursor(Qt::ArrowCursor);
        break;
    case Tool::Hand:
        setDragMode(NoDrag);  // 不用 ScrollHandDrag，手动实现平移
        setCursor(Qt::OpenHandCursor);
        break;
    case Tool::Text:        // 点击即插入文字
    case Tool::Image:       // 点击弹出文件对话框
        setDragMode(NoDrag);
        setCursor(Qt::ArrowCursor);
        break;
    default:                // Rect / Ellipse / Line / BezierCurve / Freehand
        setDragMode(NoDrag);
        setCursor(Qt::CrossCursor);
        break;
    }

    emit toolChanged(tool);  // 通知工具变更
}

// ============================================================
// ResizeHandle 管理
// ============================================================
void QAtGraphicsView::onSceneSelectionChanged()
{
    // 如果当前选择不包含任何粘贴图元，清除粘贴状态
    auto currentSelection = m_pMainScene->selectedItems();
    bool anyPastedSelected = false;
    for (auto *item : currentSelection) {
        if (m_pastedItems.contains(item)) {
            anyPastedSelected = true;
            break;
        }
    }
    if (!anyPastedSelected)
        m_pastedItems.clear();

    // 框选进行中：延迟更新避免闪烁
    if (m_rubberBanding) {
        scheduleResizeHandleUpdate();
        emit selectionChanged();
        return;
    }

    // 选择为空：立即移除 ResizeHandle（防止残留影响后续鼠标事件）
    // 选择非空：同步更新（updateResizeHandle 支持就地复用已有 handle）
    auto selectableItems = ::filterSelectableItems(currentSelection);
    if (selectableItems.isEmpty())
        removeResizeHandle();
    else
        updateResizeHandle();

    emit selectionChanged();
}

void QAtGraphicsView::updateResizeHandle()
{
    auto selectableItems = ::filterSelectableItems(m_pMainScene->selectedItems());

    if (selectableItems.isEmpty()) {
        removeResizeHandle();
        return;
    }

    // 复用已有 handle 或创建新的
    if (!m_resizeHandle) {
        m_resizeHandle = new ResizeHandleItem(nullptr);
        m_pMainScene->addItem(m_resizeHandle);
        m_resizeHandle->setUndoStack(m_undoStack);
    }

    if (selectableItems.size() == 1)
        m_resizeHandle->setTargetItem(selectableItems.first());
    else
        m_resizeHandle->setTargetItems(selectableItems);

    // 检查选中项是否包含粘贴图元，设置样式
    bool hasPasted = false;
    for (auto *item : selectableItems) {
        if (m_pastedItems.contains(item)) {
            hasPasted = true;
            break;
        }
    }
    m_resizeHandle->setPastedStyle(hasPasted);
}

void QAtGraphicsView::scheduleResizeHandleUpdate()
{
    // 通过 QMetaObject::invokeMethod 延迟到下一个事件循环再更新
    // 使用标志位去重，框选过程中的多次 selectionChanged 只触发一次实际更新
    if (m_resizeHandleUpdatePending)
        return;
    m_resizeHandleUpdatePending = true;
    QMetaObject::invokeMethod(
        this,
        [this]() {
            m_resizeHandleUpdatePending = false;
            updateResizeHandle();
        },
        Qt::QueuedConnection);
}

void QAtGraphicsView::removeResizeHandle()
{
    if (m_resizeHandle) {
        m_pMainScene->removeItem(m_resizeHandle);
        delete m_resizeHandle;
        m_resizeHandle = nullptr;
    }
}

// 原地刷新选中框位置（不删除重建），适用于 Z 序/旋转等几何不变的操作
void QAtGraphicsView::refreshResizeHandle()
{
    if (m_resizeHandle) {
        m_resizeHandle->updateHandlePositions();
        m_resizeHandle->update();
    }
}

// ============================================================
// 粘贴图元追踪
// ============================================================
void QAtGraphicsView::setPastedItems(const QList<QGraphicsItem *> &items)
{
    m_pastedItems = QSet<QGraphicsItem *>(items.begin(), items.end());
}

void QAtGraphicsView::clearPastedItems()
{
    m_pastedItems.clear();
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

    // 手型工具：开始平移（只移动视图，不碰元素）
    if (m_tool == Tool::Hand) {
        m_handPanning = true;
        m_handLastPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_tool == Tool::Select) {
        // 检测是否将启动框选（点击位置没有可选图元）
        QGraphicsItem *hit = m_pMainScene->itemAt(scenePos, transform());
        bool isCanvasOrHandle = hit && (hit->type() == CanvasItem::Type
                                        || hit->type() == ResizeHandleItem::Type);
        m_rubberBanding = (!hit || isCanvasOrHandle);

        // 记录移动起始位置（用于 MoveItemsCommand）
        m_moving = true;
        m_moveStartPositions.clear();
        for (auto *item : m_pMainScene->selectedItems()) {
            if (item->type() != CanvasItem::Type && item->type() != ResizeHandleItem::Type)
                m_moveStartPositions[item] = item->pos();
        }
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (m_tool == Tool::Image) {
        // 图片工具：弹出文件对话框
        auto result = ImageUtils::importImageWithDialog(this);
        if (result.isValid()) {
            auto *item = new ImageItem(QPixmap::fromImage(result.image));
            item->setItemPen(QPen(Qt::NoPen));
            item->setFilePath(result.filePath);
            item->setRawTiffData(result.rawTiffData);
            item->setPos(scenePos);

            if (m_undoStack)
                m_undoStack->push(new AddItemCommand(m_pMainScene, item));
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
            m_undoStack->push(new AddItemCommand(m_pMainScene, item));
        emit itemAdded(item);
        return;
    }

    // 需要拖拽的绘图工具
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
        m_pMainScene->addItem(m_tempItem);
}

void QAtGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    emit mousePositionChanged(scenePos);

    // 手型工具：只平移视图，不碰任何图元
    if (m_handPanning) {
        QPoint delta = event->pos() - m_handLastPos;
        m_handLastPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        return;
    }

    // ★ 非 Select 工具在未活跃绘制时，跳过 QGraphicsView::mouseMoveEvent
    //   防止父类根据图元 hover 状态覆写光标（Hand=OpenHand, Text=Arrow, 绘图=Cross等）
    if (m_tool != Tool::Select && !m_drawing) {
        return;
    }

    if (!m_drawing || !m_tempItem) {
        QGraphicsView::mouseMoveEvent(event);

        // Select 模式下拖动图元时，实时同步 ResizeHandle 位置
        if (m_tool == Tool::Select && m_resizeHandle && m_moving) {
            if (m_resizeHandle->isGroupMode()) {
                // 组模式：重算包围框
                m_resizeHandle->updateHandlePositions();
            } else {
                // 单目标模式：同步位置和旋转
                m_resizeHandle->setPos(m_resizeHandle->targetItem()->pos());
                m_resizeHandle->setRotation(m_resizeHandle->targetItem()->rotation());
                m_resizeHandle->updateHandlePositions();
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
            // cp1: 起点方向偏移; cp2: 终点方向偏移（镜像对称）
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

    // 手型工具：结束平移（独立于 m_tool，因为空格松手后 m_tool 可能已恢复）
    if (m_handPanning) {
        m_handPanning = false;
        setCursor(m_tool == Tool::Hand ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    // 框选结束
    m_rubberBanding = false;

    // 处理移动操作的 Undo
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
                new MoveItemsCommand(movedItems, oldPositions, newPositions, m_pMainScene));
        }
        m_moveStartPositions.clear();

        // 更新 ResizeHandle 位置
        if (m_resizeHandle)
            m_resizeHandle->updateHandlePositions();
        else
            updateResizeHandle();
    }

    if (!m_drawing) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    finishDrawing();
}

void QAtGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QGraphicsView::mouseDoubleClickEvent(event);
}

// ============================================================
// 右键菜单 — 通过信号复用 MainWindow 的 onBringToFront / onSendToBack
// ============================================================
void QAtGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    // 遍历点击位置的所有图元（按 z 序从高到低），
    // 找到第一个可交互的图元（跳过 CanvasItem 和 ResizeHandleItem）
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

    // 若该图元未被选中，先清除选择再选中它（右键点击即选中）
    if (!hitItem->isSelected()) {
        m_pMainScene->clearSelection();
        hitItem->setSelected(true);
    }

    auto selectedItems = m_pMainScene->selectedItems();
    if (selectedItems.isEmpty())
        return;

    QMenu menu;
    menu.addAction(QIcon(":/icons/icons/bring-front.svg"), tr("Bring to Front"),
                   this, &QAtGraphicsView::bringToFrontRequested);
    menu.addAction(QIcon(":/icons/icons/send-back.svg"), tr("Send to Back"),
                   this, &QAtGraphicsView::sendToBackRequested);

    menu.exec(event->globalPos());
}

void QAtGraphicsView::keyPressEvent(QKeyEvent *event)
{
    // 空格键：临时切换到手型工具
    if (event->key() == Qt::Key_Space && !m_drawing && !m_spaceHandMode) {
        m_previousTool = m_tool;
        m_spaceHandMode = true;
        setTool(Tool::Hand);
        event->accept();
        return;
    }

    // Escape 键取消当前绘制
    if (event->key() == Qt::Key_Escape && m_drawing) {
        cancelDrawing();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void QAtGraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    // 空格键释放：恢复之前的工具
    if (event->key() == Qt::Key_Space && m_spaceHandMode) {
        m_spaceHandMode = false;
        if (!m_handPanning) {
            // 未拖拽中：正常恢复工具光标
            setTool(m_previousTool);
        } else {
            // 正在拖拽中：仅更新工具类型，保持 ClosedHandCursor 直至松手
            m_tool = m_previousTool;
        }
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void QAtGraphicsView::wheelEvent(QWheelEvent *event)
{
    // Ctrl+滚轮缩放
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
    // 1. 绘制灰色工作区背景
    painter->fillRect(rect, m_pMainScene->backgroundBrush().color());

    if (!m_pCanvas)
        return;

    QRectF canvasRect = m_pCanvas->rect();

    // 2. 绘制画布白色填充（从 CanvasItem 移到这里，确保网格不被遮挡）
    painter->fillRect(canvasRect, Qt::white);

    // 3. 绘制画布阴影效果（光源来自左上角，右侧/底部/角落无空隙/重叠）
    QRectF rightShadow(canvasRect.right(), canvasRect.top() + 3,
                       6, canvasRect.height() - 3);
    QRectF bottomShadow(canvasRect.left() + 3, canvasRect.bottom(),
                        canvasRect.width() - 3, 6);
    QRectF cornerShadow(canvasRect.right(), canvasRect.bottom(), 6, 6);
    QColor shadowColor(60, 60, 60, 80);
    painter->fillRect(rightShadow, shadowColor);
    painter->fillRect(bottomShadow, shadowColor);
    painter->fillRect(cornerShadow, shadowColor);

    // 4. 绘制网格（仅在画布区域内，位于白色填充之上、图元之下）
    if (m_gridVisible) {
        QRectF gridRect = rect.intersected(canvasRect);
        if (!gridRect.isValid())
            return;

        // 根据缩放级别选择网格间距
        qreal baseInterval = 10.0; // 基础网格间距（像素）
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

        qreal majorInterval = baseInterval * 5; // 主网格线间隔

        painter->setClipRect(gridRect);

        // 次网格线（细浅色）
        QPen minorPen(QColor(0, 0, 0, 20));
        minorPen.setWidthF(0.5);
        painter->setPen(minorPen);

        qreal startX = qFloor(gridRect.left() / baseInterval) * baseInterval;
        qreal startY = qFloor(gridRect.top() / baseInterval) * baseInterval;

        // 绘制竖线
        for (qreal x = startX; x <= gridRect.right(); x += baseInterval) {
            if (qFuzzyCompare(qRound(x / majorInterval) * majorInterval, x))
                continue; // 跳过主网格线位置
            painter->drawLine(QPointF(x, gridRect.top()), QPointF(x, gridRect.bottom()));
        }
        // 绘制横线
        for (qreal y = startY; y <= gridRect.bottom(); y += baseInterval) {
            if (qFuzzyCompare(qRound(y / majorInterval) * majorInterval, y))
                continue;
            painter->drawLine(QPointF(gridRect.left(), y), QPointF(gridRect.right(), y));
        }

        // 主网格线（稍深色）
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

    // 检查是否太小（误点击）
    QRectF br = m_tempItem->boundingRect();
    if (br.width() < 3 && br.height() < 3) {
        m_pMainScene->removeItem(m_tempItem);
        delete m_tempItem;
        m_tempItem = nullptr;
        return;
    }

    // 将临时图元转为正式（通过 undo 系统）
    m_pMainScene->removeItem(m_tempItem);

    if (m_undoStack)
        m_undoStack->push(new AddItemCommand(m_pMainScene, m_tempItem));

    emit itemAdded(m_tempItem);
    m_tempItem = nullptr;
}

void QAtGraphicsView::cancelDrawing()
{
    if (m_tempItem) {
        m_pMainScene->removeItem(m_tempItem);
        delete m_tempItem;
        m_tempItem = nullptr;
    }
    m_drawing = false;
}

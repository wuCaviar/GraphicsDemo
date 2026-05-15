#ifndef QATGRAPHICSVIEW_H
#define QATGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMap>

class QGraphicsItem;
class QUndoStack;
class CanvasItem;
class GraphicsScene;

// 绘图工具类型
enum class Tool {
    Select,         // 选择/移动
    Hand,           // 抓手/平移
    Rect,           // 矩形
    Ellipse,        // 椭圆
    Line,           // 直线
    BezierCurve,    // 贝塞尔曲线
    Freehand,       // 自由线条
    Text,           // 文字
    Image,          // 图片（打开文件对话框）
};

// 自定义 GraphicsView，支持多种绘图工具
class QAtGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit QAtGraphicsView(QWidget *parent = nullptr);
    ~QAtGraphicsView();

    void setTool(Tool tool);
    Tool currentTool() const { return m_tool; }

    void setUndoStack(QUndoStack *stack);

    CanvasItem *canvasItem() const { return m_pCanvas; }
    void setCanvasSize(const QSizeF &size);
    void clearScene();  // 安全清空场景，重建画布
    void resetCanvas(const QSizeF &size); // 清空场景并重建画布

    qreal zoomLevel() const { return m_zoomLevel; }
    void setZoomLevel(qreal level);
    void fitToCanvas();

    // 网格显示
    bool isGridVisible() const { return m_gridVisible; }
    void setGridVisible(bool visible);

    // 粘贴图元追踪（委托给 GraphicsScene）
    void setPastedItems(const QList<QGraphicsItem *> &items);
    void clearPastedItems();

    // ResizeHandle 更新（委托给 GraphicsScene）
    void scheduleResizeHandleUpdate();
    void refreshResizeHandle();

signals:
    void itemAdded(QGraphicsItem *item);
    void selectionChanged();
    void mousePositionChanged(const QPointF &scenePos);
    void zoomChanged(qreal level);
    void toolChanged(Tool tool);  // 通知工具变更（含空格临时切换）
    // 右键菜单请求（复用 MainWindow 已有功能）
    void bringToFrontRequested();
    void sendToBackRequested();
    void groupRequested();
    void ungroupRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void finishDrawing();
    void cancelDrawing();
    void initCanvas(const QSizeF &size);
    void scrollToCanvasOrigin();

    GraphicsScene *m_scene = nullptr;
    QUndoStack *m_undoStack = nullptr;
    CanvasItem *m_pCanvas = nullptr;

    Tool m_tool = Tool::Select;

    // 绘制状态
    bool m_drawing = false;
    QGraphicsItem *m_tempItem = nullptr;
    QPointF m_startPos;    // 鼠标按下位置（场景坐标）
    QPointF m_lastPos;

    // 移动跟踪（用于 MoveItemsCommand）
    bool m_moving = false;
    QMap<QGraphicsItem *, QPointF> m_moveStartPositions;

    // 缩放
    qreal m_zoomLevel = 1.0;

    // 空格键临时手型工具
    Tool m_previousTool = Tool::Select;
    bool m_spaceHandMode = false;

    // 手型工具平移状态
    bool m_handPanning = false;
    QPoint m_handLastPos;

    // 框选状态（用于延迟 ResizeHandle 更新）
    bool m_rubberBanding = false;

    // 网格
    bool m_gridVisible = true;

    // 默认画笔/画刷（用于新建图元）
    QPen m_defaultPen;
    QBrush m_defaultBrush;
    QFont m_defaultFont;
};

#endif // QATGRAPHICSVIEW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "PropertyPanel.h"
#include "qatgraphicsview.h"
#include "AlignmentUtils.h"
#include <QMainWindow>
#include <QMap>
#include <QUndoStack>


class QLabel;
class QSlider;
class QToolBar;
class AlignLayoutDialog;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onNew();
    void onImportImage();
    void onExportImage();
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onDelete();
    void onSelectAll();
    void onBringToFront();
    void onSendToBack();
    void onAlignLayoutDialog();
    void onAlignLeft();
    void onAlignRight();
    void onAlignTop();
    void onAlignBottom();
    void onAlignHCenter();
    void onAlignVCenter();
    void onDistributeH();
    void onDistributeV();
    void onToolTriggered(Tool tool);
    void onSelectionChanged();
    void onItemAdded(QGraphicsItem *item);

    // 属性变更槽
    void onPenChanged(QGraphicsItem *item, const QPen &oldPen, const QPen &newPen);
    void onBrushChanged(QGraphicsItem *item, const QBrush &oldBrush, const QBrush &newBrush);
    void onFontChanged(QGraphicsItem *item, const QFont &oldFont, const QFont &newFont);
    void onTextChanged(QGraphicsItem *item, const QString &oldText, const QString &newText);
    void onGeometryChanged(QGraphicsItem *item, const QRectF &oldRect, const QRectF &newRect);
    void onCornerRadiusChanged(QGraphicsItem *item, qreal oldR, qreal newR);
    void onPositionChanged(QGraphicsItem *item, const QPointF &oldPos, const QPointF &newPos);
    void onRotationChanged(QGraphicsItem *item, qreal oldRotation, qreal newRotation);

private:
    void _initWidget();
    void _initMenuBar();
    void _initToolBar();
    void _initPropertyPanel();
    void _initRulers();
    void _initConnections();
    void _initStatusBar();
    void _updateUndoRedoActions();
    void loadStyleSheet();

    void copyItemsToClipboard(const QList<QGraphicsItem *> &items);
    QList<QGraphicsItem *> pasteItemsFromClipboard();
    QList<QGraphicsItem *> filterSelectableItems() const;
    void rotateSelectedItems(qreal angleDelta);

    // 对齐/分布辅助方法
    void applyAlign(AlignmentUtils::AlignDirection direction);
    void applyDistribute(AlignmentUtils::DistributeDirection direction,
                         const AlignmentUtils::DistributeParams &params = AlignmentUtils::DistributeParams());

    // 无损 TIFF 导出
    bool exportTiffLossless(const QString &path, const QImage &image);

    Ui::MainWindow *ui;

    QAtGraphicsView *m_pView = nullptr;
    PropertyPanel *m_pPropertyPanel = nullptr;
    QUndoStack *m_undoStack = nullptr;

    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;

    // 当前工具 Action 组
    QMap<Tool, QAction *> m_toolActions;
    Tool m_currentTool = Tool::Select;

    // 对齐工具栏
    QToolBar *m_alignToolBar = nullptr;

    // 对齐与布局对话框（非模态单例）
    AlignLayoutDialog *m_alignLayoutDlg = nullptr;

    // 刻度尺
    class RulerBar *m_hRuler = nullptr;
    class RulerBar *m_vRuler = nullptr;

    // 网格显示切换
    QAction *m_gridAction = nullptr;

    // 状态栏控件
    QLabel *m_posLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_canvasLabel = nullptr;
    QLabel *m_toolLabel = nullptr;
    QSlider *m_zoomSlider = nullptr;
};

#endif // MAINWINDOW_H

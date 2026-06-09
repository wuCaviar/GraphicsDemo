#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "PropertyPanel.h"
#include "ImageUtils.h"
#include "ImageWorker.h"
#include "ProgressManager.h"
#include "qatgraphicsview.h"
#include "AlignmentUtils.h"
#include "NetWorkUtils.h"
#include "version.h"
#include "ProcessGuard.h"

#include <QMainWindow>
#include <QMap>
#include <QUndoStack>
#include <QProgressBar>
#include <QTimer>

class QLabel;
class QPushButton;
class QSlider;
class QToolBar;
class QToolButton;
class AlignLayoutDialog;
class TaskHistoryPopup;

class TiffExportEngine;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setMainWindowVisibility(bool state);

    void getToolInfo();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNew();
    void onOpenProject();
    void onSaveProject();
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
    void onGroup();
    void onUngroup();
    void onAlignLayoutDialog();
    void onFitCanvasToItems();
    void onResizeCanvas();
    void onSettings();
    void onPreferences();
    void onAbout();
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
    void onRequestFinished(const QJsonDocument &json, NetworkRequestType type);
    void onUpdateInfo();

private:
    void _initWidget();
    void _initMenuBar();
    void _initToolBar();
    void _initPropertyPanel();
    void _initRulers();
    void _initConnections();
    void _initStatusBar();
    void _initNetWork();
    void _initProcess();
    void _updateUndoRedoActions();

    // 窗口状态持久化
    void loadWindowState();
    void saveWindowState();

    void importMultipleImages(const QStringList &paths);

    // DPI 已移除 — 画布以物理 mm 为单位，无 DPI 锁定概念
    // 导出时由用户指定 DPI

    void copyItemsToClipboard(const QList<QGraphicsItem *> &items);
    QList<QGraphicsItem *> pasteItemsFromClipboard();
    QList<QGraphicsItem *> filterSelectableItems() const;
    void rotateSelectedItems(qreal angleDelta);

    bool _maybeSaveProject(); // 提示保存，返回 false 表示用户取消操作

    // 对齐/分布辅助方法
    void applyAlign(AlignmentUtils::AlignDirection direction);
    void applyDistribute(
        AlignmentUtils::DistributeDirection direction,
        const AlignmentUtils::DistributeParams &params = AlignmentUtils::DistributeParams());

    Ui::MainWindow *ui;

    QAtGraphicsView *m_pView = nullptr;
    PropertyPanel *m_pPropertyPanel = nullptr;
    QUndoStack *m_undoStack = nullptr;

    NetWorkUtils *m_pNetWorkUtils = nullptr;

    TiffExportEngine *m_tiffEngine = nullptr; // TIFF 导出模块
    QString m_exportTaskId; // 当前导出进度任务 ID

    // 当前导出的 RIP 配置（由 onExportImage 设置，exportFinished 回调中使用）
    bool m_ripEnabled = false;
    int m_ripXRes = 0;
    int m_ripYRes = 0;

    QString m_currentProjectPath; // 当前工程文件路径，空表示未保存
    bool m_projectModified = false; // 工程文件是否已修改（未保存）

    ImageUtils::ImageImportPipeline m_importSinglePipeline; // 单图导入处理管线
    ImageUtils::ImageImportPipeline m_importMultiPipeline; // 批量导入处理管线

    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;

    // 当前工具 Action 组
    QMap<Tool, QAction *> m_toolActions;
    Tool m_currentTool = Tool::Select;

    // 对齐与布局对话框（非模态单例）
    AlignLayoutDialog *m_alignLayoutDlg = nullptr;

    // 刻度尺
    class RulerBar *m_hRuler = nullptr;
    class RulerBar *m_vRuler = nullptr;

    // 网格显示切换
    QAction *m_gridAction = nullptr;

    // 主题切换
    QAction *m_lightThemeAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QString m_currentTheme;
    void _initThemeMenu(QMenu *viewMenu);
    void switchTheme(const QString &theme);

    // 通用进度管理器
    ProgressManager *m_pProgressMgr = nullptr;

    // 任务历史按钮与弹窗
    QToolButton *m_taskHistoryBtn = nullptr;
    TaskHistoryPopup *m_taskHistoryPopup = nullptr;
    void _toggleHistoryPopup();

    // 进程管理器
    ProcessGuard *m_pProcessGuard = nullptr;

    // 当前 RIP 进度任务 ID
    QString m_ripTaskId;

    // 状态栏控件
    QLabel *m_posLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_canvasLabel = nullptr;
    QLabel *m_toolLabel = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QToolButton *m_resizeCanvasBtn = nullptr;

    // 状态栏辅助
    QPointF m_lastScenePos; // 最近一次鼠标场景坐标
    void _updatePosLabel(const QPointF &scenePos); // 根据单位模式更新坐标标签
    void _updateCanvasLabel(); // 根据单位模式更新画布尺寸标签
    void _updateToolLabel(); // 根据当前工具更新工具标签
};

#endif // MAINWINDOW_H

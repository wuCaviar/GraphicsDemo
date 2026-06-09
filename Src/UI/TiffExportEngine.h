#ifndef TIFFEXPORTENGINE_H
#define TIFFEXPORTENGINE_H

#include "ImageWorker.h"

#include <QObject>
#include <QList>
#include <QSet>
#include <QRectF>
#include <QBrush>
#include <functional>
#include <memory>
#include <atomic>
#include <lcms2.h>

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class ImageItem;

// ============================================================================
// TiffExportEngine — 将 TIFF 导出流程从 MainWindow 中解耦为独立模块
//
// 职责：场景检查 → 图元分离 → 源图收集 → Overlay 渲染 → 后台线程导出
// MainWindow 仅负责：文件对话框、RIP 对话框、进度管理对接
// ============================================================================

class TiffExportEngine : public QObject
{
    Q_OBJECT

public:
    explicit TiffExportEngine(QObject *parent = nullptr);
    ~TiffExportEngine() override;

    // ---- 状态查询 ----

    bool isRunning() const { return m_running; }
    QString lastError() const { return m_lastError; }

    // 取消正在进行的导出（线程安全，可从任意线程调用）
    void cancelExport();

    // ---- 主入口 ----

    // 从 scene 中自动发现图元、分离 ImageItem/非ImageItem、
    // 渲染非 ImageItem 为 CMYK overlay、在后台线程合成并写入输出 TIFF。
    //
    // scene:       画布场景
    // view:        视图（用于 setUpdatesEnabled 优化，可为 nullptr）
    // outputPath:  输出 .tif 文件的完整路径
    // exportDpi:   用户指定的导出 DPI（必填）
    //
    // 返回 false 表示同步校验失败（调用方应读取 lastError() 展示错误对话框）。
    // 返回 true  表示后台导出已启动，结果通过 exportFinished 信号异步通知。
    bool startExport(QGraphicsScene *scene, QGraphicsView *view,
                     const QString &outputPath, int exportDpi);

signals:
    // 后台线程中的进度回调，0–100，通过 QueuedConnection 跨线程投递
    void progressChanged(int percent);

    // 导出完成（成功或失败），通过 QueuedConnection 跨线程投递
    void exportFinished(bool success, const QString &filePath, const QString &errorMessage);

private:
    // ================================================================
    //  场景检查
    // ================================================================

    // 将 scene items 分离为 imageItems（TIFF 源图）和 nonImageItems（矢量等）
    static void separateItems(const QList<QGraphicsItem *> &allItems,
                              QList<ImageItem *> &imageItems,
                              QList<QGraphicsItem *> &nonImageItems);

    // 确定导出矩形（优先 CanvasItem，否则 itemsBoundingRect + margin）
    static QRectF determineExportRect(QGraphicsScene *scene);

    // 确定目标 DPI — 直接返回用户指定值
    static int determineTargetDpi(int exportDpi) { return exportDpi; }

    // ================================================================
    //  源 TIFF 输入构建
    // ================================================================

    QList<ImageUtils::SourceTiffInput>
    buildSources(const QList<ImageItem *> &imageItems,
                 const QRectF &exportRect, int exportDpi, qreal displayPpi) const;

    // ================================================================
    //  Overlay 渲染
    // ================================================================

    QList<ImageUtils::CmykOverlay>
    renderOverlays(QGraphicsScene *scene, const QList<QGraphicsItem *> &nonImageItems,
                   const QRectF &exportRect, const QList<QGraphicsItem *> &allSceneItems,
                   const QBrush &oldSceneBg, cmsHTRANSFORM sharedXform, bool hasSharedXform);

    // 使用存储的精确 CMYK 值渲染（跳过 QPainter + LCMS2）
    ImageUtils::CmykOverlay
    renderCmykOverlay(QGraphicsScene *scene, class IGraphicsItem *gi, const QRectF &sceneRect,
                      const QRectF &exportRect, const QRectF &outRect, int w, int h,
                      const QList<QGraphicsItem *> &allSceneItems, const QBrush &oldSceneBg,
                      bool hasBrush, bool hasPen, double brushC, double brushM, double brushY,
                      double brushK, double penC, double penM, double penY, double penK);

    // 通过 QPainter 渲染 → BGRA → CMYK
    ImageUtils::CmykOverlay renderBgraOverlay(QGraphicsScene *scene, QGraphicsItem *target,
                                              const QRectF &sceneRect, const QRectF &exportRect,
                                              const QRectF &outRect, int w, int h,
                                              const QList<QGraphicsItem *> &allSceneItems,
                                              const QBrush &oldSceneBg, cmsHTRANSFORM sharedXform,
                                              bool hasSharedXform);

    // ================================================================
    //  后台线程
    // ================================================================

    void launchExport(const QString &outputPath, QList<ImageUtils::SourceTiffInput> &&sources,
                      QList<ImageUtils::CmykOverlay> &&overlays, const QSize &outputSize,
                      const ImageUtils::TiffExportSettings &settings);

    // ================================================================
    //  辅助
    // ================================================================

    // 收集 item 及其所有后代（处理分组）
    static void collectDescendants(QGraphicsItem *root, QSet<QGraphicsItem *> &keepVisible);

    // 在渲染单个图元时保存/恢复其他图元的可见性
    struct VisibilityScope
    {
        QGraphicsScene *scene;
        QList<QGraphicsItem *> allItems;
        QHash<QGraphicsItem *, bool> saved;
        QSet<QGraphicsItem *> keepVisible;
        QBrush oldBackground;

        // 进入：隐藏非目标图元、清除背景
        void enter(QGraphicsScene *s, QGraphicsItem *target, const QList<QGraphicsItem *> &all,
                   const QBrush &oldBg);
        // 退出：恢复所有可见性和背景
        void exit();
    };

    // ================================================================
    //  状态
    // ================================================================

    bool m_running = false;
    QString m_lastError;

    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
};

#endif // TIFFEXPORTENGINE_H

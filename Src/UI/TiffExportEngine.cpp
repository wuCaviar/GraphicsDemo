#include "TiffExportEngine.h"
#include "TiffExportPipeline.h"

#include "CanvasItem.h"
#include "GraphicsItemGroup.h"
#include "IGraphicsItem.h"
#include "ImageItem.h"
#include "colortransform.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QPointer>
#include <QtConcurrent>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstring>

// ============================================================================
//  VisibilityScope — RAII 管理单图元渲染时的场景可见性状态
// ============================================================================

void TiffExportEngine::VisibilityScope::enter(QGraphicsScene *s, QGraphicsItem *target,
                                              const QList<QGraphicsItem *> &all,
                                              const QBrush &oldBg)
{
    scene = s;
    allItems = all;
    oldBackground = oldBg;

    collectDescendants(target, keepVisible);

    // Qt 渲染规则：父图元不可见时，子图元即使 setVisible(true) 也不会被绘制。
    // 因此需要将目标图元的所有祖先也加入 keepVisible，确保渲染链完整。
    for (QGraphicsItem *p = target->parentItem(); p; p = p->parentItem())
        keepVisible.insert(p);

    scene->setBackgroundBrush(Qt::NoBrush);
    for (auto *other : allItems) {
        saved[other] = other->isVisible();
        if (!keepVisible.contains(other))
            other->setVisible(false);
    }
}

void TiffExportEngine::VisibilityScope::exit()
{
    if (!scene)
        return;
    for (auto *other : allItems)
        other->setVisible(saved.value(other, true));
    scene->setBackgroundBrush(oldBackground);
    scene = nullptr;
}

// ============================================================================
//  静态辅助
// ============================================================================

void TiffExportEngine::collectDescendants(QGraphicsItem *root, QSet<QGraphicsItem *> &keepVisible)
{
    keepVisible.insert(root);
    const auto &children = root->childItems();
    for (auto *child : children)
        collectDescendants(child, keepVisible);
}

void TiffExportEngine::separateItems(const QList<QGraphicsItem *> &allItems,
                                     QList<ImageItem *> &imageItems,
                                     QList<QGraphicsItem *> &nonImageItems)
{
    for (auto *gi : allItems) {
        auto *group = dynamic_cast<GraphicsItemGroup *>(gi);
        if (group) {
            // 展开组，遍历组内包含的图元
            separateItems(group->childItems(), imageItems, nonImageItems);
            continue;
        }
        auto *imgItem = dynamic_cast<ImageItem *>(gi);
        if (imgItem)
            imageItems.append(imgItem);
        else
            nonImageItems.append(gi);
    }
}

QRectF TiffExportEngine::determineExportRect(QGraphicsScene *scene)
{
    // 尝试从 CanvasItem 获取
    const auto &items = scene->items();
    for (auto *item : items) {
        auto *canvas = dynamic_cast<CanvasItem *>(item);
        if (canvas)
            return canvas->rect();
    }
    // 回退：场景包围盒 + 边距
    return scene->itemsBoundingRect().adjusted(-10, -10, 10, 10);
}

// DPI determination now trivial — always user-specified.
// The static inline method in the header handles this directly.

// ============================================================================
//  构造 / 析构 / 配置
// ============================================================================

TiffExportEngine::TiffExportEngine(QObject *parent) : QObject(parent) { }

TiffExportEngine::~TiffExportEngine() = default;

void TiffExportEngine::cancelExport()
{
    if (m_cancelFlag)
        m_cancelFlag->store(true, std::memory_order_relaxed);
}

// ============================================================================
//  源 TIFF 输入构建
// ============================================================================

QList<ImageUtils::SourceTiffInput>
TiffExportEngine::buildSources(const QList<ImageItem *> &imageItems,
                               const QRectF &exportRect, int exportDpi,
                               qreal displayPpi) const
{
    QList<ImageUtils::SourceTiffInput> sources;
    sources.reserve(imageItems.size());

    // 场景像素 → 导出像素换算系数
    qreal pxToExportPx = exportDpi / displayPpi;

    for (auto *imgItem : imageItems) {
        ImageUtils::SourceTiffInput src;
        src.filePath = imgItem->filePath();

        // 计算目标输出像素尺寸
        QSize targetPx = imgItem->targetOutputPixels(exportDpi);
        src.targetPixelSize = targetPx;

        // 判断是否需要重采样
        QSize origSize = imgItem->originalSize();
        if (origSize.isValid() && targetPx.isValid()
            && (targetPx.width() != origSize.width()
                || targetPx.height() != origSize.height())) {
            src.needsResample = true;
        }

        // 导出像素坐标: 场景像素 × exportDpi / displayPpi
        QRectF sceneRect = imgItem->sceneBoundingRect();
        qreal outX = (sceneRect.left() - exportRect.left()) * pxToExportPx;
        qreal outY = (sceneRect.top() - exportRect.top()) * pxToExportPx;
        qreal outW = sceneRect.width() * pxToExportPx;
        qreal outH = sceneRect.height() * pxToExportPx;
        src.outputRect = QRectF(outX, outY, outW, outH);
        src.zOrder = static_cast<int>(imgItem->zValue());
        sources.append(src);
    }

    return sources;
}

// ============================================================================
//  Overlay 渲染 — 精确 CMYK 路径
// ============================================================================

ImageUtils::CmykOverlay TiffExportEngine::renderCmykOverlay(
    QGraphicsScene *scene, IGraphicsItem *gi, const QRectF &sceneRect, const QRectF &exportRect,
    const QRectF &outRect, int w, int h, const QList<QGraphicsItem *> &allSceneItems,
    const QBrush &oldSceneBg, bool hasBrush, bool hasPen, double brushC, double brushM,
    double brushY, double brushK, double penC, double penM, double penY, double penK)
{
    ImageUtils::CmykOverlay overlay;
    overlay.width = static_cast<uint32_t>(w);
    overlay.height = static_cast<uint32_t>(h);
    overlay.outputRect = outRect;
    overlay.data.resize(static_cast<size_t>(w) * h * 4);

    // 保存/恢复可见性
    VisibilityScope scope;
    scope.enter(scene, dynamic_cast<QGraphicsItem *>(gi), allSceneItems, oldSceneBg);

    // 用 sceneRect 计算渲染区域，保留亚像素偏移使内容在 overlay 中精确定位
    QRectF renderSrcRect = sceneRect.intersected(exportRect);
    double offX = renderSrcRect.left() - (outRect.left() + exportRect.left());
    double offY = renderSrcRect.top() - (outRect.top() + exportRect.top());
    QRectF renderDstRect(offX, offY, renderSrcRect.width(), renderSrcRect.height());

    if (hasBrush && hasPen) {
        // 两者都有 CMYK：分别渲染填充蒙版和边框蒙版，各自使用精确 CMYK 值

        // 填充蒙版（临时去掉 pen）
        QImage fillMask(w, h, QImage::Format_ARGB32);
        fillMask.fill(Qt::transparent);
        {
            QPen savedPen = gi->itemPen();
            gi->setItemPen(Qt::NoPen);
            QPainter painter(&fillMask);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            scene->render(&painter, renderDstRect, renderSrcRect);
            gi->setItemPen(savedPen);
        }

        // 边框蒙版（临时去掉 brush）
        QImage borderMask(w, h, QImage::Format_ARGB32);
        borderMask.fill(Qt::transparent);
        {
            QBrush savedBrush = gi->itemBrush();
            gi->setItemBrush(Qt::NoBrush);
            QPainter painter(&borderMask);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            scene->render(&painter, renderDstRect, renderSrcRect);
            gi->setItemBrush(savedBrush);
        }

        auto toCmyk = [](double c, double m, double y, double k) {
            return std::array<uint8_t, 4>{
                { static_cast<uint8_t>(qBound(0.0, std::round(c * 2.55), 255.0)),
                  static_cast<uint8_t>(qBound(0.0, std::round(m * 2.55), 255.0)),
                  static_cast<uint8_t>(qBound(0.0, std::round(y * 2.55), 255.0)),
                  static_cast<uint8_t>(qBound(0.0, std::round(k * 2.55), 255.0)) }
            };
        };
        auto brushCmyk = toCmyk(brushC, brushM, brushY, brushK);
        auto penCmyk = toCmyk(penC, penM, penY, penK);

        // 边框优先于填充
        for (int y = 0; y < h; ++y) {
            const uchar *fillSrc = fillMask.constScanLine(y);
            const uchar *borderSrc = borderMask.constScanLine(y);
            uint8_t *row = overlay.data.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                bool inBorder = borderSrc[x * 4 + 3] > 0;
                bool inFill = fillSrc[x * 4 + 3] > 0;
                if (inBorder)
                    std::memcpy(row + x * 4, penCmyk.data(), 4);
                else if (inFill)
                    std::memcpy(row + x * 4, brushCmyk.data(), 4);
                else
                    std::memset(row + x * 4, 0, 4);
            }
        }
    } else {
        // 只有一个属性有 CMYK：全量填充，再通过蒙版清零透明区域
        double dC = hasBrush ? brushC : penC;
        double dM = hasBrush ? brushM : penM;
        double dY = hasBrush ? brushY : penY;
        double dK = hasBrush ? brushK : penK;

        uint8_t cmyk[4] = { static_cast<uint8_t>(qBound(0.0, std::round(dC * 2.55), 255.0)),
                            static_cast<uint8_t>(qBound(0.0, std::round(dM * 2.55), 255.0)),
                            static_cast<uint8_t>(qBound(0.0, std::round(dY * 2.55), 255.0)),
                            static_cast<uint8_t>(qBound(0.0, std::round(dK * 2.55), 255.0)) };

        size_t totalPixels = static_cast<size_t>(w) * h;
        uint8_t *dst = overlay.data.data();
        for (size_t i = 0; i < totalPixels; ++i) {
            dst[i * 4 + 0] = cmyk[0];
            dst[i * 4 + 1] = cmyk[1];
            dst[i * 4 + 2] = cmyk[2];
            dst[i * 4 + 3] = cmyk[3];
        }

        QImage maskImg(w, h, QImage::Format_ARGB32);
        maskImg.fill(Qt::transparent);
        {
            QPainter painter(&maskImg);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            scene->render(&painter, renderDstRect, renderSrcRect);
        }

        for (int y = 0; y < h; ++y) {
            const uchar *src = maskImg.constScanLine(y);
            uint8_t *row = overlay.data.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                if (src[x * 4 + 3] == 0)
                    std::memset(row + x * 4, 0, 4);
            }
        }
    }

    scope.exit();
    return overlay;
}

// ============================================================================
//  Overlay 渲染 — BGRA → CMYK 路径
// ============================================================================

ImageUtils::CmykOverlay TiffExportEngine::renderBgraOverlay(
    QGraphicsScene *scene, QGraphicsItem *target, const QRectF &sceneRect, const QRectF &exportRect,
    const QRectF &outRect, int w, int h, const QList<QGraphicsItem *> &allSceneItems,
    const QBrush &oldSceneBg, cmsHTRANSFORM sharedXform, bool hasSharedXform)
{
    ImageUtils::CmykOverlay overlay;
    overlay.width = static_cast<uint32_t>(w);
    overlay.height = static_cast<uint32_t>(h);
    overlay.outputRect = outRect;

    // 隔离目标图元，避免场景中其他可见图元渗透到当前 overlay
    VisibilityScope scope;
    scope.enter(scene, target, allSceneItems, oldSceneBg);

    // 用 sceneRect 计算渲染区域，保留亚像素偏移使内容在 overlay 中精确定位
    QRectF renderSrcRect = sceneRect.intersected(exportRect);
    double offX = renderSrcRect.left() - (outRect.left() + exportRect.left());
    double offY = renderSrcRect.top() - (outRect.top() + exportRect.top());
    QRectF renderDstRect(offX, offY, renderSrcRect.width(), renderSrcRect.height());
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    {
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        scene->render(&painter, renderDstRect, renderSrcRect);
    }

    scope.exit();

    // BGRA → CMYK
    overlay.data.resize(static_cast<size_t>(w) * h * 4);
    if (hasSharedXform) {
        QATColorManager::convertBgra8ToCmyk8(sharedXform, img.constBits(), overlay.data.data(), w,
                                             h);
    } else {
        ImageUtils::bgraToCmykFallback(img.constBits(), overlay.data.data(), w * h);
    }

    // 清零透明像素
    for (int y = 0; y < h; ++y) {
        const uchar *src = img.constScanLine(y);
        uint8_t *dst = overlay.data.data() + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; ++x) {
            if (src[x * 4 + 3] == 0)
                std::memset(dst + x * 4, 0, 4);
        }
    }

    return overlay;
}

// ============================================================================
//  Overlay 渲染 — 调度循环
// ============================================================================

QList<ImageUtils::CmykOverlay> TiffExportEngine::renderOverlays(
    QGraphicsScene *scene, const QList<QGraphicsItem *> &nonImageItems, const QRectF &exportRect,
    const QList<QGraphicsItem *> &allSceneItems, const QBrush &oldSceneBg,
    cmsHTRANSFORM sharedXform, bool hasSharedXform)
{
    QList<ImageUtils::CmykOverlay> overlays;
    if (nonImageItems.isEmpty())
        return overlays;

    for (auto *item : nonImageItems) {
        // 使用不含画笔的几何矩形确定 overlay 位置和尺寸，
        // 再按画笔宽度扩展渲染源，确保 1:1 映射且画笔完整包含
        auto *gi = dynamic_cast<IGraphicsItem *>(item);
        QRectF localRect =
            (gi && gi->supportsGeometryRect()) ? gi->geometryRect() : item->boundingRect();
        QRectF sceneRect = item->mapToScene(localRect).boundingRect();

        // 按画笔宽度扩展渲染区域，包含完整的画笔像素
        qreal penExpand = 0.0;
        if (gi && gi->itemPen().style() != Qt::NoPen)
            penExpand = gi->itemPen().widthF() / 2.0;
        QRectF expandedRect = sceneRect.adjusted(-penExpand, -penExpand, penExpand, penExpand);

        // overlay 位置和尺寸基于扩展后的矩形（含画笔）
        QRectF clipped = expandedRect.intersected(exportRect);
        if (clipped.isEmpty())
            continue;

        int olLeft = static_cast<int>(std::floor(clipped.left()));
        int olTop = static_cast<int>(std::floor(clipped.top()));
        int olRight = static_cast<int>(std::ceil(clipped.right()));
        int olBottom = static_cast<int>(std::ceil(clipped.bottom()));
        int w = std::max(1, olRight - olLeft);
        int h = std::max(1, olBottom - olTop);
        QRectF outRect(olLeft - exportRect.left(), olTop - exportRect.top(), w, h);

        ImageUtils::CmykOverlay overlay;
        overlay.zOrder = static_cast<int>(item->zValue());

        // 检查精确 CMYK 值
        bool hasBrushCmyk = false, hasPenCmyk = false;
        double brushC = 0, brushM = 0, brushY = 0, brushK = 0;
        double penC = 0, penM = 0, penY = 0, penK = 0;

        if (gi) {
            if (gi->hasBrushCmyk()) {
                QBrush b = gi->itemBrush();
                if (b.style() == Qt::SolidPattern) {
                    gi->brushCmyk(brushC, brushM, brushY, brushK);
                    hasBrushCmyk = true;
                }
            }
            if (gi->hasPenCmyk()) {
                QPen p = gi->itemPen();
                if (p.style() != Qt::NoPen) {
                    gi->penCmyk(penC, penM, penY, penK);
                    hasPenCmyk = true;
                }
            }
        }

        if (hasBrushCmyk || hasPenCmyk) {
            overlay = renderCmykOverlay(scene, gi, expandedRect, exportRect, outRect, w, h,
                                        allSceneItems, oldSceneBg, hasBrushCmyk, hasPenCmyk, brushC,
                                        brushM, brushY, brushK, penC, penM, penY, penK);
        } else {
            overlay = renderBgraOverlay(scene, item, expandedRect, exportRect, outRect, w, h,
                                        allSceneItems, oldSceneBg, sharedXform, hasSharedXform);
        }

        overlays.append(std::move(overlay));
    }

    return overlays;
}

// ============================================================================
//  后台线程导出
// ============================================================================

void TiffExportEngine::launchExport(const QString &outputPath,
                                    QList<ImageUtils::SourceTiffInput> &&sources,
                                    QList<ImageUtils::CmykOverlay> &&overlays,
                                    const QSize &outputSize,
                                    const ImageUtils::TiffExportSettings &settings)
{
    QPointer<TiffExportEngine> guard(this);

    // 创建取消标志
    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);

    auto progress = [guard](int pct) {
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, pct]() {
                if (guard)
                    emit guard->progressChanged(pct);
            },
            Qt::QueuedConnection);
    };

    auto *thread = QThread::create([guard, outputPath, sources = std::move(sources),
                                    overlays = std::move(overlays), outputSize, settings, progress,
                                    cancelFlag = m_cancelFlag]() mutable {
        try {
            // ---- 使用 StripPipeline 执行流水线导出 ----
            ImageUtils::StripPipeline::Config pipelineCfg;
            pipelineCfg.stripHeight = 256;
            pipelineCfg.pipelineDepth = 3;
            pipelineCfg.maxReaderThreads = 4;

            ImageUtils::StripPipeline pipeline(pipelineCfg);
            auto result = pipeline.execute(outputPath, sources, std::move(overlays), outputSize,
                                           settings, progress, cancelFlag.get());

            QMetaObject::invokeMethod(
                guard.data(),
                [guard, result]() {
                    if (!guard)
                        return;
                    guard->m_running = false;

                    if (result.success) {
                        emit guard->exportFinished(true, result.filePath, QString());
                    } else {
                        emit guard->exportFinished(false, result.filePath, result.errorMessage);
                    }
                },
                Qt::QueuedConnection);
        } catch (const std::exception &ex) {
            QMetaObject::invokeMethod(
                guard.data(),
                [guard, msg = QString::fromUtf8(ex.what())]() {
                    if (!guard)
                        return;
                    guard->m_running = false;
                    emit guard->exportFinished(false, QString(), msg);
                },
                Qt::QueuedConnection);
        } catch (...) {
            QMetaObject::invokeMethod(
                guard.data(),
                [guard]() {
                    if (!guard)
                        return;
                    guard->m_running = false;
                    emit guard->exportFinished(false, QString(),
                                               tr("Export failed: unknown error"));
                },
                Qt::QueuedConnection);
        }
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

// ============================================================================
//  主入口
// ============================================================================

bool TiffExportEngine::startExport(QGraphicsScene *scene, QGraphicsView *view,
                                   const QString &outputPath, int exportDpi)
{
    m_lastError.clear();

    if (m_running) {
        m_lastError = tr("An export is already in progress.");
        return false;
    }

    if (!scene) {
        m_lastError = tr("No scene to export.");
        return false;
    }

    // ---- 1. 分离图元 ----
    const auto allItems = ::filterSelectableItems(scene->items());
    QList<ImageItem *> imageItems;
    QList<QGraphicsItem *> nonImageItems;
    separateItems(allItems, imageItems, nonImageItems);

    if (imageItems.isEmpty() && nonImageItems.isEmpty()) {
        m_lastError = QStringLiteral("No items to export.");
        return false;
    }

    // ---- 2. 导出区域与 DPI ----
    QRectF exportRect = determineExportRect(scene);
    int targetDpi = determineTargetDpi(exportDpi);

    // 获取画布显示 PPI（用于场景像素 → 导出像素换算）
    qreal displayPpi = 300.0;
    {
        const auto &sceneItems = scene->items();
        for (auto *item : sceneItems) {
            auto *canvas = dynamic_cast<CanvasItem *>(item);
            if (canvas) {
                displayPpi = canvas->displayPpi();
                break;
            }
        }
    }

    // 注意：画布矩形在"适配图元"时已包含留白，此处不再重复扩展

    // 将 exportRect 对齐到整数像素边界，消除 mm→px 转换中的浮点精度误差。
    // 例如：留白 6.35mm @300dpi = 75px（精确），但 6.35mm @150dpi = 37.5px（半像素），
    // 或 6.35 在 float64 中不可精确表示导致 ceil(right) 比预期大 1。
    // 使用 floor(left/top) + ceil(right/bottom) 保证所有内容像素被完整覆盖。
    {
        int exLeft = static_cast<int>(std::floor(exportRect.left()));
        int exTop = static_cast<int>(std::floor(exportRect.top()));
        int exRight = static_cast<int>(std::ceil(exportRect.right()));
        int exBottom = static_cast<int>(std::ceil(exportRect.bottom()));
        exportRect = QRectF(exLeft, exTop, exRight - exLeft, exBottom - exTop);
    }

    // ---- 3. 构建源 TIFF 输入 ----
    QList<ImageUtils::SourceTiffInput> sources =
        buildSources(imageItems, exportRect, targetDpi, displayPpi);

    // 校验源文件
    for (const auto &src : sources) {
        if (src.filePath.isEmpty()) {
            m_lastError = QStringLiteral("An image on the canvas has no source file "
                                         "and cannot be exported.");
            return false;
        }
        if (!QFile::exists(src.filePath)) {
            m_lastError = QStringLiteral("Source file not found:\n%1").arg(src.filePath);
            return false;
        }
    }

    // ---- 4. 渲染 Overlay（主线程） ----
    QList<QGraphicsItem *> allSceneItems = scene->items();
    QBrush oldSceneBg = scene->backgroundBrush();

    // 禁用视图更新以减少闪烁
    if (view)
        view->setUpdatesEnabled(false);

    // 创建共享 LCMS2 句柄（主线程顺序循环，线程安全）
    QATColorManager &cm = QATColorManager::instance();
    cmsHTRANSFORM sharedXform = nullptr;
    if (cm.isValid()) {
        sharedXform = cm.createBgraToCmyk8(INTENT_PERCEPTUAL, cmsFLAGS_BLACKPOINTCOMPENSATION
                                                                  | cmsFLAGS_HIGHRESPRECALC);
    }

    QList<ImageUtils::CmykOverlay> overlays =
        renderOverlays(scene, nonImageItems, exportRect, allSceneItems, oldSceneBg, sharedXform,
                       sharedXform != nullptr);

    if (sharedXform)
        cmsDeleteTransform(sharedXform);

    if (view)
        view->setUpdatesEnabled(true);

    // ---- 5. 导出设置 ----
    ImageUtils::TiffExportSettings settings;
    settings.dpi = targetDpi;

    // ---- 6. 计算导出像素尺寸（场景像素 × exportDpi / displayPpi） ----
    qreal pxToExportPx = exportDpi / displayPpi;
    QSize outSize(qCeil(exportRect.width() * pxToExportPx),
                  qCeil(exportRect.height() * pxToExportPx));
    m_running = true;

    launchExport(outputPath, std::move(sources), std::move(overlays), outSize, settings);

    return true;
}

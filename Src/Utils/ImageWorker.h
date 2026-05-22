#ifndef IMAGEWORKER_H
#define IMAGEWORKER_H

#include "ImageUtils.h"
#include <QList>
#include <QStringList>
#include <memory>
#include <vector>

namespace ImageUtils {

// ========== 扩展接口 ==========

// 导入后处理器接口 — 实现此接口并注册到 ImageImportPipeline 即可扩展导入流程
class IImportPostProcessor
{
public:
    virtual ~IImportPostProcessor() = default;
    virtual QString name() const = 0;
    // 工作线程中调用，对加载完成的 ImportResult 做后处理（色彩校正、元数据提取等）
    virtual void process(ImportResult &result) = 0;
};

// 导出预处理器接口 — 实现此接口并注册到 ImageExportPipeline 即可扩展导出流程
class IExportPreProcessor
{
public:
    virtual ~IExportPreProcessor() = default;
    virtual QString name() const = 0;
    // 主线程中调用，在场景渲染之后、CMYK 转换之前修改 QImage
    virtual void process(QImage &image, const QRectF &exportRect) = 0;
};

// ========== 处理管线 ==========

class ImageImportPipeline
{
public:
    void addProcessor(std::unique_ptr<IImportPostProcessor> processor);
    void removeProcessor(const QString &name);
    void run(ImportResult &result);

private:
    std::vector<std::unique_ptr<IImportPostProcessor>> m_processors;
};

class ImageExportPipeline
{
public:
    void addProcessor(std::unique_ptr<IExportPreProcessor> processor);
    void removeProcessor(const QString &name);
    void run(QImage &image, const QRectF &exportRect);

private:
    std::vector<std::unique_ptr<IExportPreProcessor>> m_processors;
};

// ========== CMYK 覆写数据快照（线程安全，在主线程收集） ==========

struct CmykItemSnapshot
{
    struct BrushCmyk
    {
        QRectF sceneRect;
        uint8_t c, m, y, k;
    };
    struct PenCmyk
    {
        QRectF sceneRect;
        qreal penWidth;
        bool hasBrush; // true 时仅覆写边缘（stroke），false 时覆写整个区域
        uint8_t c, m, y, k;
    };
    struct ImageCmykSource
    {
        QRectF sceneRect;
        RawPixelBuffer cmykMat;
    };

    QList<BrushCmyk> brushItems;
    QList<PenCmyk> penItems;
    QList<ImageCmykSource> imageSources;
};

// 从场景图元收集 CMYK 覆写数据（必须在主线程调用）
CmykItemSnapshot collectCmykItemSnapshot(const QList<QGraphicsItem *> &items,
                                         const QRectF &exportRect);

// ========== 工作线程结果 ==========

struct ImportWorkerResult
{
    ImportResult importResult;
    QString filePath;
    QString errorMessage;
    bool success = false;
};

struct ExportWorkerResult
{
    QString filePath;
    QString errorMessage;
    bool success = false;
};

// ========== 线程池入口函数（线程安全） ==========

ImportWorkerResult runImportWorker(const QString &filePath,
                                   const QSizeF &canvasSize, bool scaleToFit);

ExportWorkerResult runExportWorker(const QString &path, const QImage &image,
                                   const CmykItemSnapshot &snapshot,
                                   const QRectF &exportRect);

// 线程安全的 CMYK TIFF 导出（使用预收集的快照，不访问 QGraphicsItem）
bool exportTiffCmykFromSnapshot(const QString &path, const QImage &image,
                                const CmykItemSnapshot &snapshot,
                                const QRectF &exportRect);

} // namespace ImageUtils

#endif // IMAGEWORKER_H

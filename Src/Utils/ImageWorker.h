#ifndef IMAGEWORKER_H
#define IMAGEWORKER_H

#include "ImageUtils.h"
#include <QList>
#include <QPixmap>
#include <QStringList>
#include <memory>
#include <vector>

namespace ImageUtils {

// ========== 扩展接口 ==========

class IImportPostProcessor
{
public:
    virtual ~IImportPostProcessor() = default;
    virtual QString name() const = 0;
};

class IExportPreProcessor
{
public:
    virtual ~IExportPreProcessor() = default;
    virtual QString name() const = 0;
    virtual void process(QImage &image, const QRectF &exportRect) = 0;
};

// ========== 处理管线 ==========

class ImageImportPipeline
{
public:
    void addProcessor(std::unique_ptr<IImportPostProcessor> processor);
    void removeProcessor(const QString &name);
    void run(const QStringList &filepaths);

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

// ========== 工作线程结果 ==========

struct ImportWorkerResult
{
    QPixmap pixmap;
    QString path;

    bool isValid() const { return !pixmap.isNull(); }

    QString errorMessage;
};

struct ExportWorkerResult
{
    QString filePath;
    QString errorMessage;
    bool success = false;
};

// ========== 线程池入口函数（线程安全） ==========

ImportWorkerResult runImportWorker(const QString &filePath);

// 从源 TIFF 直接复制数据到输出 TIFF，保留原始像素和所有 tag
ExportWorkerResult exportFromSourceTiff(const QString &sourcePath,
                                        const QString &outputPath);

// 场景渲染导出（非 TIFF 源或混合场景）
ExportWorkerResult exportFromScene(const QString &outputPath,
                                   QImage image,
                                   const QRectF &exportRect,
                                   int dpi);

// ========== 多源 TIFF 并行导出 ==========

struct SourceTiffInput
{
    QString filePath;
    QRectF outputRect; // 在输出图像中的像素坐标
    int zOrder = 0;
};

// 并行读取多个源 TIFF 的原始 CMYK 数据，按 z-order 合成为一张输出 TIFF
ExportWorkerResult exportFromMultipleSourceTiffs(
    const QString &outputPath,
    const QList<SourceTiffInput> &sources,
    const QSize &outputSize,
    int dpi);

} // namespace ImageUtils

#endif // IMAGEWORKER_H

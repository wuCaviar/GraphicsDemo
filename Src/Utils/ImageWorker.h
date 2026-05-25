#ifndef IMAGEWORKER_H
#define IMAGEWORKER_H

#include "ImageUtils.h"
#include <QList>
#include <QPixmap>
#include <QStringList>
#include <functional>
#include <memory>
#include <vector>

#include <tiff.h>

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

// ========== 进度回调（线程安全） ==========

using ProgressCallback = std::function<void(int)>;

// ========== 线程池入口函数（线程安全） ==========

ImportWorkerResult runImportWorker(const QString &filePath);

// ========== TIFF 导出设置 ==========

struct TiffExportSettings
{
    int dpi = 300;
    uint16_t compression = COMPRESSION_NONE; // libtiff compression constant
    QString iccProfilePath;                   // empty = use built-in default
};

// ========== 源 TIFF 输入描述 ==========

struct SourceTiffInput
{
    QString filePath;
    QRectF outputRect; // 在输出图像中的像素坐标
    int zOrder = 0;
};

// ========== 预渲染 CMYK 图层 ==========

struct CmykOverlay
{
    std::vector<uint8_t> data; // CMYK 像素 (width * height * 4)
    uint32_t width = 0;
    uint32_t height = 0;
    QRectF outputRect; // 在输出图像中的像素坐标
    int zOrder = 0;
};

// ========== 统一 TIFF 导出 ==========

// 并行读取所有源 TIFF 的像素数据（自动处理不同色域和压缩方式，统一转换为 CMYK），
// 按 z-order 合成为一张输出 TIFF。所有 tag 值来自 TiffExportSettings，不从源 TIFF 复制。
// overlays: 预先渲染好的 CMYK 图层（如矢量图元），与 TIFF 源一起按 z-order 合成。
ExportWorkerResult exportTiff(const QString &outputPath,
                              const QList<SourceTiffInput> &sources,
                              const QList<CmykOverlay> &overlays,
                              const QSize &outputSize,
                              const TiffExportSettings &settings,
                              ProgressCallback progress = nullptr);

} // namespace ImageUtils

#endif // IMAGEWORKER_H

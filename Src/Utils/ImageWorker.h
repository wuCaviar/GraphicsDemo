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
    QSize size;
    QString path;
    int dpiX = 0; // 图片 X 方向 DPI（仅 TIFF 有效，非 TIFF 为 0）
    int dpiY = 0; // 图片 Y 方向 DPI

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
    uint16_t compression = COMPRESSION_LZW; // libtiff compression constant
    QString iccProfilePath; // empty = use built-in default
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

// ========== RGB → CMYK fallback 转换 ==========

// 不使用 LCMS2 时的简单数学转换，用于 fallback 路径。
// 输入: bgra[4*pixelCount] (B,G,R,A)，输出: cmyk[4*pixelCount] (C,M,Y,K)
// 透明像素应先由调用方处理（A=0 的像素应跳过或清零输出）。
// 注意：此函数不涉及 LCMS2，可在任意线程安全调用。
void bgraToCmykFallback(const uint8_t *bgra, uint8_t *cmyk, int pixelCount);

// ========== 统一 TIFF 导出 (Legacy — 全缓冲模式) ==========

// [DEPRECATED] 全缓冲实现，保留作为 StripPipeline 的验证参考路径。
// 新代码应使用 StripPipeline::execute()。
ExportWorkerResult exportTiff(const QString &outputPath,
                              const QList<SourceTiffInput> &sources,
                              QList<CmykOverlay> &&overlays,
                              const QSize &outputSize,
                              const TiffExportSettings &settings,
                              ProgressCallback progress = nullptr);

} // namespace ImageUtils

#endif // IMAGEWORKER_H

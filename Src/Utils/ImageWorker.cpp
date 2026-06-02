#include "ImageWorker.h"
#include "AppConfig.h"
#include "colortransform.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QtConcurrent>
#include <QFuture>
#include <QThread>

#include <tiff.h>
#include <tiffio.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ImageUtils {

// ========== Pipeline 实现 ==========

void ImageImportPipeline::addProcessor(
    std::unique_ptr<IImportPostProcessor> processor)
{
    m_processors.push_back(std::move(processor));
}

void ImageImportPipeline::removeProcessor(const QString &name)
{
    m_processors.erase(
        std::remove_if(m_processors.begin(), m_processors.end(),
                       [&](const std::unique_ptr<IImportPostProcessor> &p) {
                           return p->name() == name;
                       }),
        m_processors.end());
}

void ImageImportPipeline::run(const QStringList &filepaths)
{
    Q_UNUSED(filepaths);
}

void ImageExportPipeline::addProcessor(
    std::unique_ptr<IExportPreProcessor> processor)
{
    m_processors.push_back(std::move(processor));
}

void ImageExportPipeline::removeProcessor(const QString &name)
{
    m_processors.erase(
        std::remove_if(m_processors.begin(), m_processors.end(),
                       [&](const std::unique_ptr<IExportPreProcessor> &p) {
                           return p->name() == name;
                       }),
        m_processors.end());
}

void ImageExportPipeline::run(QImage &image, const QRectF &exportRect)
{
    for (auto &p : m_processors)
        p->process(image, exportRect);
}

// ========== 线程池入口函数 ==========

ImportWorkerResult runImportWorker(const QString &filePath)
{
    ImportWorkerResult result;
    result.path = filePath;

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    reader.setAllocationLimit(0);
    QImage image = reader.read();

    if (image.isNull()) {
        result.errorMessage = QString("Failed to load image: %1").arg(filePath);
        return result;
    }

    // 提取 TIFF DPI（仅 TIFF 格式有效，轻量级读取标签不解码图像）
    if (ImageUtils::isTiffFile(filePath)) {
        QPair<int, int> dpi = ImageUtils::readTiffDpi(filePath);
        result.dpiX = dpi.first;
        result.dpiY = dpi.second;

        QSize size = ImageUtils::readTiffSize(filePath);
        result.size = size;
    }

    // 生成缩略图：固定 1/4 缩放，减少内存占用
    // 缩略图仅用于屏幕显示，物理尺寸通过 m_rect 保持与原图一致
    // 导出时从磁盘重新读取原图，不受缩略图影响
    constexpr int kThumbScaleDiv = 4;
    if (image.width() > kThumbScaleDiv || image.height() > kThumbScaleDiv) {
        QSize thumbSize(qMax(1, image.width() / kThumbScaleDiv),
                        qMax(1, image.height() / kThumbScaleDiv));
        image = image.scaled(thumbSize, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    }

    result.pixmap = QPixmap::fromImage(image);
    return result;
}

// ========== 源 TIFF 读取 → CMYK 缓冲区 ==========

struct CmykBuffer
{
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    QRectF outputRect;
    int zOrder = 0;
    bool valid = false;
    QString errorMessage;
};

// ========== RGB → CMYK fallback（不使用 LCMS2 时的简单数学转换） ==========

void bgraToCmykFallback(const uint8_t *bgra, uint8_t *cmyk, int pixelCount)
{
    for (int i = 0; i < pixelCount; ++i) {
        int b = bgra[i * 4 + 0];
        int g = bgra[i * 4 + 1];
        int r = bgra[i * 4 + 2];
        // R,G,B → C,M,Y: 直接取反
        int c = 255 - r;
        int m = 255 - g;
        int y = 255 - b;
        int k = std::min({ c, m, y });
        if (k < 255) {
            int denom = 255 - k;
            cmyk[i * 4 + 0] = static_cast<uint8_t>(((c - k) * 255) / denom);
            cmyk[i * 4 + 1] = static_cast<uint8_t>(((m - k) * 255) / denom);
            cmyk[i * 4 + 2] = static_cast<uint8_t>(((y - k) * 255) / denom);
        } else {
            cmyk[i * 4 + 0] = 0;
            cmyk[i * 4 + 1] = 0;
            cmyk[i * 4 + 2] = 0;
        }
        cmyk[i * 4 + 3] = static_cast<uint8_t>(k);
    }
}

// 读取单个源 TIFF，自动处理色域转换（CMYK/RGB/Grayscale）和压缩方式（libtiff 自动解压）
// 注意：此函数通过 QtConcurrent::run 在多线程中并行调用。
// LCMS2 cmsDoTransform 文档声明线程安全，但本项目 colortransform 设计约定
// "需传入线程独立的变换句柄"（参见 colortransform.cpp:172）。
// 且 LCMS2 的 threaded 插件会内部并行化单次 cmsDoTransform 调用，
// 共享句柄 + 外部多线程 + 内部并行三重叠加存在不确定风险。
// 因此每个并行调用独立创建/销毁自己的 cmsHTRANSFORM，确保线程安全。
static CmykBuffer readSourceToCmyk(const SourceTiffInput &input)
{
    CmykBuffer buf;
    buf.outputRect = input.outputRect;
    buf.zOrder = input.zOrder;

    QByteArray pathBytes = input.filePath.toLocal8Bit();
    TIFF *tif = TIFFOpen(pathBytes.constData(), "r");
    if (!tif) {
        buf.errorMessage = QString("Cannot open: %1").arg(input.filePath);
        return buf;
    }

    uint32_t w = 0, h = 0;
    uint16_t bitsPerSample = 8;
    uint16_t samplesPerPixel = 1;
    uint16_t photometric = PHOTOMETRIC_MINISWHITE;
    uint16_t planarConfig = PLANARCONFIG_CONTIG;
    uint16_t sampleFormat = SAMPLEFORMAT_UINT;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

    if (w == 0 || h == 0 || w > 30000 || h > 300000) {
        TIFFClose(tif);
        buf.errorMessage = QString("Invalid dimensions: %1x%2").arg(w).arg(h);
        return buf;
    }

    if (bitsPerSample != 8 || sampleFormat != SAMPLEFORMAT_UINT) {
        TIFFClose(tif);
        buf.errorMessage =
            QString("Only 8-bit unsigned integer TIFFs are supported: %1")
                .arg(input.filePath);
        return buf;
    }

    buf.width = w;
    buf.height = h;
    try {
        buf.data.resize(static_cast<size_t>(w) * h * 4); // CMYK output
    } catch (const std::bad_alloc &) {
        buf.errorMessage =
            QString("Not enough memory for %1x%2 CMYK buffer").arg(w).arg(h);
        return buf;
    }

    // Determine source color type and setup conversion
    enum SrcColorType
    {
        CMYK,
        RGB,
        GRAY
    };
    SrcColorType srcType;
    int srcBpp; // source bytes per pixel in CONTIG mode

    if (photometric == PHOTOMETRIC_SEPARATED && samplesPerPixel >= 4) {
        srcType = CMYK;
        srcBpp = 4;
    } else if (photometric == PHOTOMETRIC_RGB && samplesPerPixel >= 3) {
        srcType = RGB;
        srcBpp = samplesPerPixel; // 3 or 4
    } else if ((photometric == PHOTOMETRIC_MINISBLACK
                || photometric == PHOTOMETRIC_MINISWHITE)
               && samplesPerPixel == 1) {
        srcType = GRAY;
        srcBpp = 1;
    } else {
        TIFFClose(tif);
        buf.errorMessage =
            QString("Unsupported TIFF format (photometric=%1, samples=%2): %3")
                .arg(photometric)
                .arg(samplesPerPixel)
                .arg(input.filePath);
        return buf;
    }

    tsize_t scanlineSize = TIFFScanlineSize(tif);
    std::vector<uint8_t> scanBuf(scanlineSize);

    // For RGB→CMYK, create LCMS2 transform (each parallel call owns its own)
    QATColorManager &cm = QATColorManager::instance();
    cmsHTRANSFORM rgbToCmyk = nullptr;
    if (srcType == RGB && cm.isValid()) {
        rgbToCmyk = cm.createBgraToCmyk8(INTENT_PERCEPTUAL,
                                         cmsFLAGS_BLACKPOINTCOMPENSATION
                                             | cmsFLAGS_HIGHRESPRECALC);
    }

    bool isMiniswhite = (photometric == PHOTOMETRIC_MINISWHITE);

    auto processContigRow = [&](uint32_t row, const uint8_t *rowData) {
        uint8_t *dst = buf.data.data() + static_cast<size_t>(row) * w * 4;

        switch (srcType) {
        case CMYK:
            std::memcpy(dst, rowData, static_cast<size_t>(w) * 4);
            break;

        case RGB: {
            // Expand RGB/RGBA to BGRA, then convert to CMYK
            std::vector<uint8_t> bgraLine(w * 4);
            if (samplesPerPixel == 3) {
                for (uint32_t x = 0; x < w; ++x) {
                    bgraLine[x * 4 + 0] = rowData[x * 3 + 2]; // B
                    bgraLine[x * 4 + 1] = rowData[x * 3 + 1]; // G
                    bgraLine[x * 4 + 2] = rowData[x * 3 + 0]; // R
                    bgraLine[x * 4 + 3] = 255; // A
                }
            } else { // samplesPerPixel == 4
                for (uint32_t x = 0; x < w; ++x) {
                    bgraLine[x * 4 + 0] = rowData[x * 4 + 2]; // B
                    bgraLine[x * 4 + 1] = rowData[x * 4 + 1]; // G
                    bgraLine[x * 4 + 2] = rowData[x * 4 + 0]; // R
                    bgraLine[x * 4 + 3] = 255; // A (ignore original)
                }
            }
            if (rgbToCmyk) {
                QATColorManager::convertBgra8ToCmyk8(rgbToCmyk, bgraLine.data(),
                                                     dst, static_cast<int>(w));
            } else {
                bgraToCmykFallback(bgraLine.data(), dst, static_cast<int>(w));
            }
            break;
        }

        case GRAY:
            for (uint32_t x = 0; x < w; ++x) {
                uint8_t gray = isMiniswhite
                                   ? static_cast<uint8_t>(255 - rowData[x])
                                   : rowData[x];
                int off = static_cast<int>(x) * 4;
                dst[off + 0] = 0; // C
                dst[off + 1] = 0; // M
                dst[off + 2] = 0; // Y
                dst[off + 3] = gray; // K
            }
            break;
        }
    };

    if (planarConfig == PLANARCONFIG_CONTIG) {
        for (uint32_t row = 0; row < h; ++row) {
            if (TIFFReadScanline(tif, scanBuf.data(), row, 0) < 0) {
                if (rgbToCmyk)
                    cmsDeleteTransform(rgbToCmyk);
                TIFFClose(tif);
                buf.errorMessage = QString("Read error at row %1: %2")
                                       .arg(row)
                                       .arg(input.filePath);
                buf.valid = false;
                return buf;
            }
            processContigRow(row, scanBuf.data());
        }
    } else {
        // Separate planes: read each plane into temp buffers, then interleave
        std::vector<std::vector<uint8_t>> planes(
            samplesPerPixel > 4 ? 4 : samplesPerPixel);
        int effectivePlanes = (srcType == CMYK) ? 4 : samplesPerPixel;

        for (int s = 0; s < effectivePlanes; ++s) {
            planes[s].resize(static_cast<size_t>(w) * h);
            for (uint32_t row = 0; row < h; ++row) {
                if (TIFFReadScanline(tif, scanBuf.data(), row,
                                     static_cast<uint16_t>(s))
                    < 0) {
                    if (rgbToCmyk)
                        cmsDeleteTransform(rgbToCmyk);
                    TIFFClose(tif);
                    buf.errorMessage =
                        QString("Read error at row %1 plane %2: %3")
                            .arg(row)
                            .arg(s)
                            .arg(input.filePath);
                    buf.valid = false;
                    return buf;
                }
                std::memcpy(planes[s].data() + row * w, scanBuf.data(), w);
            }
        }

        // Interleave and convert
        for (uint32_t row = 0; row < h; ++row) {
            uint8_t *dst = buf.data.data() + static_cast<size_t>(row) * w * 4;
            switch (srcType) {
            case CMYK:
                for (uint32_t x = 0; x < w; ++x) {
                    int off = static_cast<int>(x) * 4;
                    dst[off + 0] = planes[0][row * w + x];
                    dst[off + 1] = planes[1][row * w + x];
                    dst[off + 2] = planes[2][row * w + x];
                    dst[off + 3] = planes[3][row * w + x];
                }
                break;
            case RGB: {
                std::vector<uint8_t> bgraLine(w * 4);
                for (uint32_t x = 0; x < w; ++x) {
                    bgraLine[x * 4 + 0] = planes[2][row * w + x]; // B
                    bgraLine[x * 4 + 1] = planes[1][row * w + x]; // G
                    bgraLine[x * 4 + 2] = planes[0][row * w + x]; // R
                    bgraLine[x * 4 + 3] = 255;
                }
                if (rgbToCmyk) {
                    QATColorManager::convertBgra8ToCmyk8(
                        rgbToCmyk, bgraLine.data(), dst, static_cast<int>(w));
                } else {
                    bgraToCmykFallback(bgraLine.data(), dst,
                                       static_cast<int>(w));
                }
                break;
            }
            case GRAY:
                for (uint32_t x = 0; x < w; ++x) {
                    uint8_t gray =
                        isMiniswhite
                            ? static_cast<uint8_t>(255 - planes[0][row * w + x])
                            : planes[0][row * w + x];
                    int off = static_cast<int>(x) * 4;
                    dst[off + 0] = 0;
                    dst[off + 1] = 0;
                    dst[off + 2] = 0;
                    dst[off + 3] = gray;
                }
                break;
            }
        }
    }

    if (rgbToCmyk)
        cmsDeleteTransform(rgbToCmyk);
    TIFFClose(tif);

    buf.valid = true;
    return buf;
}

// ========== 统一 TIFF 导出 (Legacy — 全缓冲模式) ==========

ExportWorkerResult
exportTiff(const QString &outputPath, const QList<SourceTiffInput> &sources,
           QList<CmykOverlay> &&overlays, const QSize &outputSize,
           const TiffExportSettings &settings, ProgressCallback progress)
{
    ExportWorkerResult result;
    result.filePath = outputPath;

    if (sources.isEmpty() && overlays.isEmpty()) {
        result.errorMessage = "No content to export";
        return result;
    }

    auto report = [&progress](int pct) {
        if (progress)
            progress(pct);
    };

    report(0);

    // ===== Phase 1: 并行读取所有源 TIFF → CMYK 缓冲区 (0→40%) =====
    // 注意：不共享 LCMS2 transform 句柄。每个并行调用独立创建/销毁自己的句柄，
    // 遵循 colortransform.h 的"需传入线程独立的变换句柄"设计约定。
    QList<QFuture<CmykBuffer>> readFutures;
    readFutures.reserve(sources.size());
    for (const auto &src : sources)
        readFutures.append(QtConcurrent::run(readSourceToCmyk, src));

    QList<CmykBuffer> buffers;
    buffers.reserve(readFutures.size() + overlays.size());
    for (int i = 0; i < readFutures.size(); ++i) {
        // takeResult() moves the buffer out, avoiding a deep copy of pixel data.
        // Blocks if this future is not yet done (same as result()).
        CmykBuffer buf = readFutures[i].takeResult();
        if (!buf.valid) {
            result.errorMessage = buf.errorMessage;
            // Wait for remaining futures to finish so we don't leak orphaned
            // tasks into the thread pool — they'd keep consuming memory and I/O.
            for (int j = i + 1; j < readFutures.size(); ++j)
                readFutures[j].waitForFinished();
            return result;
        }
        buffers.append(std::move(buf));
        report(40 * (i + 1) / readFutures.size());
    }

    // ===== Phase 1b: 将预渲染的 CMYK 图层包装为 CmykBuffer（移动语义，零拷贝） =====
    for (auto &overlay : overlays) {
        CmykBuffer buf;
        buf.width = overlay.width;
        buf.height = overlay.height;
        buf.outputRect = overlay.outputRect;
        buf.zOrder = overlay.zOrder;
        buf.data = std::move(overlay.data); // 移动像素数据，避免深拷贝
        buf.valid = true;
        buffers.append(std::move(buf));
    }

    // ===== Phase 2: 按 z-order 排序 =====
    std::sort(buffers.begin(), buffers.end(),
              [](const CmykBuffer &a, const CmykBuffer &b) {
                  return a.zOrder < b.zOrder;
              });

    const int outW = outputSize.width();
    const int outH = outputSize.height();

    if (outW <= 0 || outH <= 0) {
        result.errorMessage = "Invalid output size";
        return result;
    }

    // ===== Phase 3: 并行合成 (40→90%) =====
    constexpr size_t kMaxOutputPixels = static_cast<size_t>(30000) * 300000;
    size_t outputPixels = static_cast<size_t>(outW) * static_cast<size_t>(outH);
    if (outputPixels > kMaxOutputPixels) {
        result.errorMessage =
            QString("Output too large: %1x%2").arg(outW).arg(outH);
        return result;
    }

    std::vector<uint8_t> outBuf;
    try {
        outBuf.resize(outputPixels * 4);
    } catch (const std::bad_alloc &) {
        result.errorMessage =
            QString("Not enough memory for %1x%2 output").arg(outW).arg(outH);
        return result;
    }

    int numThreads = std::max(1, QThread::idealThreadCount());
    int rowsPerChunk = (outH + numThreads - 1) / numThreads;

    QList<QFuture<void>> chunkFutures;
    int totalChunks = 0;

    for (int t = 0; t < numThreads; ++t) {
        int startRow = t * rowsPerChunk;
        int endRow = std::min(startRow + rowsPerChunk, outH);
        if (startRow >= outH)
            break;
        ++totalChunks;

        chunkFutures.append(QtConcurrent::run([&, startRow, endRow]() {
            for (int y = startRow; y < endRow; ++y) {
                uint8_t *outRow =
                    outBuf.data() + static_cast<size_t>(y) * outW * 4;
                // Initialize to white (CMYK 0,0,0,0)
                std::memset(outRow, 0, static_cast<size_t>(outW) * 4);

                for (int bi = 0; bi < buffers.size(); ++bi) {
                    const CmykBuffer &buf = buffers[bi];
                    int bx0 =
                        std::max(0, static_cast<int>(buf.outputRect.left()));
                    int by0 =
                        std::max(0, static_cast<int>(buf.outputRect.top()));
                    int bx1 = std::min(
                        outW, static_cast<int>(buf.outputRect.right()));
                    int by1 = std::min(
                        outH, static_cast<int>(buf.outputRect.bottom()));

                    if (y < by0 || y >= by1)
                        continue;

                    double outWf = buf.outputRect.width();
                    double outHf = buf.outputRect.height();
                    if (outWf <= 0.0 || outHf <= 0.0)
                        continue;

                    double scaleX = static_cast<double>(buf.width) / outWf;
                    double scaleY = static_cast<double>(buf.height) / outHf;
                    double srcY = (y - buf.outputRect.top()) * scaleY;

                    int iy0 = static_cast<int>(std::floor(srcY));
                    int iy1 = iy0 + 1;
                    iy0 = std::clamp(iy0, 0, static_cast<int>(buf.height) - 1);
                    iy1 = std::clamp(iy1, 0, static_cast<int>(buf.height) - 1);
                    double fy = srcY - std::floor(srcY);

                    const uint8_t *row0 =
                        buf.data.data()
                        + static_cast<size_t>(iy0) * buf.width * 4;
                    const uint8_t *row1 =
                        buf.data.data()
                        + static_cast<size_t>(iy1) * buf.width * 4;

                    for (int x = bx0; x < bx1; ++x) {
                        double srcX = (x - buf.outputRect.left()) * scaleX;
                        int ix0 = static_cast<int>(std::floor(srcX));
                        int ix1 = ix0 + 1;
                        ix0 =
                            std::clamp(ix0, 0, static_cast<int>(buf.width) - 1);
                        ix1 =
                            std::clamp(ix1, 0, static_cast<int>(buf.width) - 1);
                        // 8.8 定点数插值：fx/fy ∈ [0, 1) 映射到 [0, 255]
                        constexpr int FP_SHIFT = 8;
                        int fxI = static_cast<int>((srcX - std::floor(srcX))
                                                   * (1 << FP_SHIFT));
                        int fyI = static_cast<int>(fy * (1 << FP_SHIFT));

                        int off = x * 4;
                        for (int c = 0; c < 4; ++c) {
                            int p00 = row0[ix0 * 4 + c];
                            int p10 = row0[ix1 * 4 + c];
                            int p01 = row1[ix0 * 4 + c];
                            int p11 = row1[ix1 * 4 + c];
                            int top = (p00 << FP_SHIFT) + (p10 - p00) * fxI;
                            int bot = (p01 << FP_SHIFT) + (p11 - p01) * fxI;
                            int val = top + ((bot - top) * fyI >> FP_SHIFT);
                            outRow[off + c] = static_cast<uint8_t>(
                                std::clamp(val >> FP_SHIFT, 0, 255));
                        }
                    }
                }
            }
        }));
    }

    // Wait for all chunks; report progress from the calling thread only.
    // progress is a std::function and may not be thread-safe.
    for (int i = 0; i < chunkFutures.size(); ++i) {
        chunkFutures[i].waitForFinished();
        // Each chunk that finishes represents 1/totalChunks progress from 40→90
        report(40 + (i + 1) * 50 / totalChunks);
    }

    // ===== Phase 4: 写入输出 TIFF (90→100%) =====
    TIFF *tif = TIFFOpen(outputPath.toLocal8Bit().constData(), "w");
    if (!tif) {
        result.errorMessage =
            QString("Cannot create output TIFF: %1").arg(outputPath);
        return result;
    }

    // Basic image dimensions
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(outW));
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(outH));

    // Sample configuration: 4-channel CMYK, 8 bits each, unsigned integer
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
    // libtiff's _TIFFVSetField only accepts a single uint16_t for BITSPERSAMPLE,
    // not a count+array. The value is auto-replicated to all samples on write.
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);

    // CMYK color separation
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(tif, TIFFTAG_INKSET, INKSET_CMYK);
    TIFFSetField(tif, TIFFTAG_NUMBEROFINKS, 4);

    // Data layout
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, settings.compression);
    // Horizontal differencing predictor: for CMYK images, adjacent pixels
    // are highly correlated, improving LZW/ZIP compression by 10-30%.
    TIFFSetField(tif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1));

    // Metadata
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, static_cast<float>(settings.dpi));
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, static_cast<float>(settings.dpi));
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "GraphicsDemo");
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, static_cast<uint32_t>(0));

    QString dateTime =
        QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    TIFFSetField(tif, TIFFTAG_DATETIME, dateTime.toUtf8().constData());

    // ICC Profile
    QString iccPath = settings.iccProfilePath;
    if (iccPath.isEmpty())
        iccPath = AppConfig::instance().cmykIccPath();
    if (!iccPath.isEmpty()) {
        QFile iccFile(iccPath);
        if (iccFile.open(QIODevice::ReadOnly)) {
            QByteArray iccData = iccFile.readAll();
            TIFFSetField(tif, TIFFTAG_ICCPROFILE,
                         static_cast<uint32_t>(iccData.size()),
                         iccData.constData());
            iccFile.close();
        }
    }

    // Write pixel data row by row.
    // outBuf is interleaved CMYK (C,M,Y,K,C,M,Y,K,...), 4 bytes per pixel.
    const size_t rowStride = static_cast<size_t>(outW) * 4;
    for (int y = 0; y < outH; ++y) {
        if (TIFFWriteScanline(tif, outBuf.data() + y * rowStride, y, 0) != 1) {
            TIFFClose(tif);
            result.errorMessage = QString("Write error at row %1").arg(y);
            return result;
        }
        if (y % 128 == 0)
            report(90 + y * 10 / outH);
    }

    TIFFClose(tif);
    report(100);

    result.success = true;
    return result;
}

} // namespace ImageUtils

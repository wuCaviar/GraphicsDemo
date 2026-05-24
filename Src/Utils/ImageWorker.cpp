#include "ImageWorker.h"
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

    result.pixmap = QPixmap::fromImage(image);
    return result;
}

// ========== 从源 TIFF 直接复制数据到输出（保留原始像素和 tag） ==========

ExportWorkerResult exportFromSourceTiff(const QString &sourcePath,
                                        const QString &outputPath)
{
    ExportWorkerResult result;
    result.filePath = outputPath;

    QByteArray srcBytes = sourcePath.toLocal8Bit();
    TIFF *in = TIFFOpen(srcBytes.constData(), "r");
    if (!in) {
        result.errorMessage =
            QString("Cannot open source TIFF: %1").arg(sourcePath);
        return result;
    }

    // 创建输出 TIFF
    TIFF *out = TIFFOpen(outputPath.toUtf8().constData(), "w8");
    if (!out) {
        TIFFClose(in);
        result.errorMessage =
            QString("Cannot create output TIFF: %1").arg(outputPath);
        return result;
    }

    int pageNum = 0;
    tdata_t buf = nullptr;

    do {
        // 读取源文件当前页的关键 tag
        uint32_t width = 0, height = 0;
        uint16_t bitsPerSample = 8, samplesPerPixel = 4;
        uint16_t photometric = PHOTOMETRIC_SEPARATED;
        uint16_t planarConfig = PLANARCONFIG_CONTIG;
        uint16_t compression = COMPRESSION_NONE;
        uint16_t orientation = ORIENTATION_TOPLEFT;
        uint16_t resUnit = RESUNIT_INCH;
        float xRes = 72.0f, yRes = 72.0f;

        TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height);
        TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
        TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
        TIFFGetFieldDefaulted(in, TIFFTAG_PHOTOMETRIC, &photometric);
        TIFFGetFieldDefaulted(in, TIFFTAG_PLANARCONFIG, &planarConfig);
        TIFFGetFieldDefaulted(in, TIFFTAG_COMPRESSION, &compression);
        TIFFGetFieldDefaulted(in, TIFFTAG_ORIENTATION, &orientation);
        TIFFGetFieldDefaulted(in, TIFFTAG_XRESOLUTION, &xRes);
        TIFFGetFieldDefaulted(in, TIFFTAG_YRESOLUTION, &yRes);
        TIFFGetFieldDefaulted(in, TIFFTAG_RESOLUTIONUNIT, &resUnit);

        if (width == 0 || height == 0 || width > 50000 || height > 50000) {
            _TIFFfree(buf);
            TIFFClose(in);
            TIFFClose(out);
            result.errorMessage = QString("Invalid TIFF dimensions on page %1: %2x%3")
                                      .arg(pageNum).arg(width).arg(height);
            return result;
        }

        // 写入当前页 tag
        TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
        if (pageNum == 0) {
            // 首页：标准 tag
            TIFFSetField(out, TIFFTAG_SUBFILETYPE, static_cast<uint32_t>(0));
            TIFFSetField(out, TIFFTAG_PAGENUMBER, 0,
                         static_cast<uint16_t>(0));
        } else {
            // 后续页：子文件类型
            TIFFSetField(out, TIFFTAG_SUBFILETYPE,
                         static_cast<uint32_t>(FILETYPE_PAGE));
            TIFFSetField(out, TIFFTAG_PAGENUMBER,
                         static_cast<uint16_t>(pageNum),
                         static_cast<uint16_t>(0));
        }
        {
            std::vector<uint16_t> bpsArray(samplesPerPixel,
                                           static_cast<uint16_t>(bitsPerSample));
            TIFFSetField(out, TIFFTAG_BITSPERSAMPLE,
                         static_cast<uint16_t>(samplesPerPixel), bpsArray.data());
        }
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, samplesPerPixel);
        TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, photometric);
        if (photometric == PHOTOMETRIC_SEPARATED) {
            TIFFSetField(out, TIFFTAG_INKSET, INKSET_CMYK);
            TIFFSetField(out, TIFFTAG_NUMBEROFINKS, 4);
        }
        TIFFSetField(out, TIFFTAG_PLANARCONFIG, planarConfig);
        TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
        TIFFSetField(out, TIFFTAG_ORIENTATION, orientation);
        TIFFSetField(out, TIFFTAG_XRESOLUTION, xRes);
        TIFFSetField(out, TIFFTAG_YRESOLUTION, yRes);
        TIFFSetField(out, TIFFTAG_RESOLUTIONUNIT, resUnit);
        TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, -1));

        // DateTime
        QString dateTime =
            QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
        TIFFSetField(out, TIFFTAG_DATETIME, dateTime.toUtf8().constData());

        // 标准 TIFF Tag
        TIFFSetField(out, TIFFTAG_SOFTWARE, "GraphicsDemo");

        // ICC profile
        uint32_t iccSize = 0;
        void *iccData = nullptr;
        if (TIFFGetField(in, TIFFTAG_ICCPROFILE, &iccSize, &iccData) && iccData) {
            TIFFSetField(out, TIFFTAG_ICCPROFILE, iccSize, iccData);
        }

        // 复制当前页像素数据
        tsize_t scanlineSize = TIFFScanlineSize(in);
        buf = _TIFFmalloc(scanlineSize);
        if (!buf) {
            TIFFClose(in);
            TIFFClose(out);
            result.errorMessage = "Memory allocation failed";
            return result;
        }

        if (planarConfig == PLANARCONFIG_CONTIG) {
            for (uint32_t row = 0; row < height; ++row) {
                if (TIFFReadScanline(in, buf, row, 0) < 0) {
                    _TIFFfree(buf);
                    TIFFClose(in);
                    TIFFClose(out);
                    result.errorMessage =
                        QString("Failed to read source row %1 page %2")
                            .arg(row).arg(pageNum);
                    return result;
                }
                if (TIFFWriteScanline(out, buf, row, 0) < 0) {
                    _TIFFfree(buf);
                    TIFFClose(in);
                    TIFFClose(out);
                    result.errorMessage =
                        QString("Failed to write output row %1 page %2")
                            .arg(row).arg(pageNum);
                    return result;
                }
            }
        } else {
            for (uint16_t s = 0; s < samplesPerPixel; ++s) {
                for (uint32_t row = 0; row < height; ++row) {
                    if (TIFFReadScanline(in, buf, row, s) < 0) {
                        _TIFFfree(buf);
                        TIFFClose(in);
                        TIFFClose(out);
                        result.errorMessage =
                            QString("Failed to read source row %1 sample %2 page %3")
                                .arg(row).arg(s).arg(pageNum);
                        return result;
                    }
                    if (TIFFWriteScanline(out, buf, row, s) < 0) {
                        _TIFFfree(buf);
                        TIFFClose(in);
                        TIFFClose(out);
                        result.errorMessage =
                            QString("Failed to write output row %1 sample %2 page %3")
                                .arg(row).arg(s).arg(pageNum);
                        return result;
                    }
                }
            }
        }

        _TIFFfree(buf);
        buf = nullptr;

        // 如果不是最后一页，写入当前 IFD 并开始新 IFD
        if (!TIFFLastDirectory(in)) {
            TIFFWriteDirectory(out);
        }
        pageNum++;
    } while (TIFFReadDirectory(in));

    TIFFClose(in);
    TIFFClose(out);

    result.success = true;
    return result;
}

// ========== 场景渲染导出 ==========

ExportWorkerResult exportFromScene(const QString &outputPath,
                                   QImage image,
                                   const QRectF &exportRect,
                                   int dpi)
{
    ExportWorkerResult result;
    result.filePath = outputPath;

    Q_UNUSED(exportRect);

    int width = image.width();
    int height = image.height();

    TIFF *tif = TIFFOpen(outputPath.toUtf8().constData(), "w8");
    if (!tif) {
        result.errorMessage =
            QString("Cannot create output TIFF: %1").arg(outputPath);
        return result;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    {
        uint16_t bps[] = {8, 8, 8, 8};
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 4, bps);
    }
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(tif, TIFFTAG_INKSET, INKSET_CMYK);
    TIFFSetField(tif, TIFFTAG_NUMBEROFINKS, 4);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1));
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, static_cast<float>(dpi));
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, static_cast<float>(dpi));
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);

    QString dateTime =
        QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    TIFFSetField(tif, TIFFTAG_DATETIME, dateTime.toUtf8().constData());

    // 标准 TIFF Tag
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "GraphicsDemo");
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, static_cast<uint32_t>(0));

    // Embed ICC profile
    QATColorManager &cm = QATColorManager::instance();
    cmsHTRANSFORM xform = nullptr;
    if (cm.isValid()) {
        xform = cm.createBgraToCmyk8(INTENT_PERCEPTUAL,
                                     cmsFLAGS_BLACKPOINTCOMPENSATION
                                         | cmsFLAGS_HIGHRESPRECALC);
    }
#if defined(Q_OS_WIN)
    QString iccPath = QCoreApplication::applicationDirPath()
                      + "/../ICC Profile/CMYK/JapanColor2001Coated.icc";
#elif defined(Q_OS_MACOS)
    QString iccPath = "/Volumes/Caviar/Test/GraphicsDemo/Bin/../ICC "
                      "Profile/CMYK/JapanColor2001Coated.icc";
#endif
    {
        QFile iccFile(iccPath);
        if (iccFile.open(QIODevice::ReadOnly)) {
            QByteArray iccData = iccFile.readAll();
            TIFFSetField(tif, TIFFTAG_ICCPROFILE,
                         static_cast<uint32_t>(iccData.size()),
                         iccData.constData());
            iccFile.close();
        }
    }

    // 逐行 RGB→CMYK 转换并写入
    QVector<uint8_t> rowBuf(width * 4);

    for (int y = 0; y < height; ++y) {
        if (xform) {
            QATColorManager::convertBgra8ToCmyk8(xform, image.constScanLine(y),
                                                 rowBuf.data(), width);
        } else {
            const QRgb *scanLine =
                reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = 0; x < width; ++x) {
                QRgb c = scanLine[x];
                double r = qRed(c) / 255.0, g = qGreen(c) / 255.0,
                       b = qBlue(c) / 255.0;
                double cd = 1.0 - r, md = 1.0 - g, yd = 1.0 - b;
                double kd = qMin(cd, qMin(md, yd));
                if (kd < 1.0) {
                    cd = (cd - kd) / (1.0 - kd) * 100.0;
                    md = (md - kd) / (1.0 - kd) * 100.0;
                    yd = (yd - kd) / (1.0 - kd) * 100.0;
                } else {
                    cd = md = yd = 0.0;
                }
                kd *= 100.0;
                int off = x * 4;
                rowBuf[off + 0] =
                    static_cast<uint8_t>(qBound(0.0, cd * 2.55, 255.0));
                rowBuf[off + 1] =
                    static_cast<uint8_t>(qBound(0.0, md * 2.55, 255.0));
                rowBuf[off + 2] =
                    static_cast<uint8_t>(qBound(0.0, yd * 2.55, 255.0));
                rowBuf[off + 3] =
                    static_cast<uint8_t>(qBound(0.0, kd * 2.55, 255.0));
            }
        }

        if (TIFFWriteScanline(tif, rowBuf.data(), y) < 0) {
            TIFFClose(tif);
            if (xform)
                cmsDeleteTransform(xform);
            result.errorMessage = QString("Write error at row %1").arg(y);
            return result;
        }
    }

    TIFFClose(tif);
    if (xform)
        cmsDeleteTransform(xform);

    result.success = true;
    return result;
}

// ========== 多源 TIFF 并行导出 ==========

struct SourceTiffBuffer
{
    QByteArray cmykData;
    uint32_t srcWidth = 0;
    uint32_t srcHeight = 0;
    QRectF outputRect;
    int zOrder = 0;
    bool valid = false;
    QString errorMessage;
};

// 从单个源 TIFF 读取原始 CMYK 数据（线程安全，每个调用打开独立的 TIFF 句柄）
static SourceTiffBuffer readSourceTiffBuffer(const SourceTiffInput &input)
{
    SourceTiffBuffer buf;
    buf.outputRect = input.outputRect;
    buf.zOrder = input.zOrder;

    QByteArray pathBytes = input.filePath.toLocal8Bit();
    TIFF *tif = TIFFOpen(pathBytes.constData(), "r");
    if (!tif) {
        buf.errorMessage = QString("Cannot open: %1").arg(input.filePath);
        return buf;
    }

    uint32_t w = 0, h = 0;
    uint16_t bitsPerSample = 8, samplesPerPixel = 4;
    uint16_t photometric = PHOTOMETRIC_SEPARATED;
    uint16_t planarConfig = PLANARCONFIG_CONTIG;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarConfig);

    if (w == 0 || h == 0 || w > 50000 || h > 50000) {
        TIFFClose(tif);
        buf.errorMessage = QString("Invalid dimensions: %1x%2").arg(w).arg(h);
        return buf;
    }

    bool isCmyk = (photometric == PHOTOMETRIC_SEPARATED && samplesPerPixel >= 4);
    if (!isCmyk || bitsPerSample != 8) {
        TIFFClose(tif);
        buf.errorMessage = QString("Not 8-bit CMYK: %1").arg(input.filePath);
        return buf;
    }

    buf.srcWidth = w;
    buf.srcHeight = h;
    buf.cmykData.resize(static_cast<int>(w) * h * 4);

    tsize_t scanlineSize = TIFFScanlineSize(tif);
    tdata_t scanBuf = _TIFFmalloc(scanlineSize);
    if (!scanBuf) {
        TIFFClose(tif);
        buf.errorMessage = "Memory allocation failed";
        return buf;
    }

    if (planarConfig == PLANARCONFIG_CONTIG) {
        for (uint32_t row = 0; row < h; ++row) {
            if (TIFFReadScanline(tif, scanBuf, row, 0) < 0)
                break;
            memcpy(buf.cmykData.data() + row * w * 4, scanBuf,
                   static_cast<size_t>(w) * 4);
        }
    } else {
        tdata_t channelBuf = _TIFFmalloc(scanlineSize);
        if (channelBuf) {
            std::vector<uint8_t> rowPixels(w * 4);
            for (uint32_t row = 0; row < h; ++row) {
                for (uint16_t sample = 0; sample < 4; ++sample) {
                    if (TIFFReadScanline(tif, channelBuf, row, sample) < 0)
                        break;
                    auto *src = static_cast<uint8_t *>(channelBuf);
                    for (uint32_t col = 0; col < w; ++col)
                        rowPixels[col * 4 + sample] = src[col];
                }
                memcpy(buf.cmykData.data() + row * w * 4, rowPixels.data(),
                       static_cast<size_t>(w) * 4);
            }
            _TIFFfree(channelBuf);
        }
    }

    _TIFFfree(scanBuf);
    TIFFClose(tif);

    buf.valid = true;
    return buf;
}

// 双线性插值：从源 CMYK 缓冲区读取一个像素（处理越界）
static inline void sampleCmyk(const SourceTiffBuffer &buf, double srcX, double srcY,
                              uint8_t *out)
{
    // 边界钳位
    int ix0 = static_cast<int>(srcX);
    int iy0 = static_cast<int>(srcY);
    double fx = srcX - ix0;
    double fy = srcY - iy0;

    auto clamp = [&](int x, int y) {
        x = qBound(0, x, static_cast<int>(buf.srcWidth) - 1);
        y = qBound(0, y, static_cast<int>(buf.srcHeight) - 1);
        return reinterpret_cast<const uint8_t *>(buf.cmykData.constData())
               + (y * buf.srcWidth + x) * 4;
    };

    const uint8_t *p00 = clamp(ix0,     iy0);
    const uint8_t *p10 = clamp(ix0 + 1, iy0);
    const uint8_t *p01 = clamp(ix0,     iy0 + 1);
    const uint8_t *p11 = clamp(ix0 + 1, iy0 + 1);

    for (int c = 0; c < 4; ++c) {
        double top = p00[c] * (1.0 - fx) + p10[c] * fx;
        double bot = p01[c] * (1.0 - fx) + p11[c] * fx;
        double val = top * (1.0 - fy) + bot * fy;
        out[c] = static_cast<uint8_t>(qBound(0.0, val, 255.0));
    }
}

ExportWorkerResult exportFromMultipleSourceTiffs(
    const QString &outputPath,
    const QList<SourceTiffInput> &sources,
    const QSize &outputSize,
    int dpi)
{
    ExportWorkerResult result;
    result.filePath = outputPath;

    if (sources.isEmpty()) {
        result.errorMessage = "No source TIFFs";
        return result;
    }

    // 1. 并行读取所有源 TIFF 的原始 CMYK 数据
    QList<QFuture<SourceTiffBuffer>> futures;
    futures.reserve(sources.size());
    for (const auto &src : sources)
        futures.append(QtConcurrent::run(readSourceTiffBuffer, src));

    QList<SourceTiffBuffer> buffers;
    buffers.reserve(futures.size());
    for (auto &f : futures) {
        SourceTiffBuffer buf = f.result();
        if (!buf.valid) {
            result.errorMessage = buf.errorMessage;
            return result;
        }
        buffers.append(std::move(buf));
    }

    // 2. 按 z-order 排序（低→高，高值覆盖低值）
    std::sort(buffers.begin(), buffers.end(),
              [](const SourceTiffBuffer &a, const SourceTiffBuffer &b) {
                  return a.zOrder < b.zOrder;
              });

    const int outW = outputSize.width();
    const int outH = outputSize.height();

    // 3. 创建输出 TIFF
    TIFF *tif = TIFFOpen(outputPath.toUtf8().constData(), "w8");
    if (!tif) {
        result.errorMessage = "Cannot create output TIFF";
        return result;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(outW));
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(outH));
    {
        uint16_t bps[] = {8, 8, 8, 8};
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 4, bps);
    }
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(tif, TIFFTAG_INKSET, INKSET_CMYK);
    TIFFSetField(tif, TIFFTAG_NUMBEROFINKS, 4);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1));
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, static_cast<float>(dpi));
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, static_cast<float>(dpi));
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);

    QString dateTime =
        QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    TIFFSetField(tif, TIFFTAG_DATETIME, dateTime.toUtf8().constData());

    // 标准 TIFF Tag
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "GraphicsDemo");
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, static_cast<uint32_t>(0));

    // Embed ICC profile (from first source or default)
    {
        QByteArray pathBytes = sources.first().filePath.toLocal8Bit();
        TIFF *firstIn = TIFFOpen(pathBytes.constData(), "r");
        if (firstIn) {
            uint32_t iccSize = 0;
            void *iccData = nullptr;
            if (TIFFGetField(firstIn, TIFFTAG_ICCPROFILE, &iccSize, &iccData)
                && iccData) {
                TIFFSetField(tif, TIFFTAG_ICCPROFILE, iccSize, iccData);
            }
            TIFFClose(firstIn);
        }
    }

    // 4. 逐行合成并写入
    QVector<uint8_t> outRow(outW * 4);
    QVector<uint8_t> srcRowBuf; // 按需分配
    double srcToOutScaleX = 1.0;
    double srcToOutScaleY = 1.0;

    for (int y = 0; y < outH; ++y) {
        // 白色背景 (CMYK: 0,0,0,0)
        memset(outRow.data(), 0, static_cast<size_t>(outW) * 4);

        for (int bi = 0; bi < buffers.size(); ++bi) {
            const SourceTiffBuffer &buf = buffers[bi];
            int bx0 = qMax(0, static_cast<int>(buf.outputRect.left()));
            int by0 = qMax(0, static_cast<int>(buf.outputRect.top()));
            int bx1 = qMin(outW, static_cast<int>(buf.outputRect.right()));
            int by1 = qMin(outH, static_cast<int>(buf.outputRect.bottom()));

            if (y < by0 || y >= by1)
                continue;

            double outWf = buf.outputRect.width();
            double outHf = buf.outputRect.height();
            if (outWf <= 0.0 || outHf <= 0.0)
                continue;

            srcToOutScaleX = static_cast<double>(buf.srcWidth) / outWf;
            srcToOutScaleY = static_cast<double>(buf.srcHeight) / outHf;
            double srcY = (y - buf.outputRect.top()) * srcToOutScaleY;

            for (int x = bx0; x < bx1; ++x) {
                double srcX = (x - buf.outputRect.left()) * srcToOutScaleX;
                int off = x * 4;
                sampleCmyk(buf, srcX, srcY, outRow.data() + off);
            }
        }

        if (TIFFWriteScanline(tif, outRow.data(), y) < 0) {
            TIFFClose(tif);
            result.errorMessage = QString("Write error at row %1").arg(y);
            return result;
        }
    }

    TIFFClose(tif);
    result.success = true;
    return result;
}

} // namespace ImageUtils

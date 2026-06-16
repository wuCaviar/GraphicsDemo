#include "ImageUtils.h"
#include "atMath.h"
#include "ImageCacheManager.h"
#include "ImageWorker.h"
#include "IGraphicsItem.h"
#include "ImageItem.h"
#include "colortransform.h"

#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QDebug>
#include <QColorSpace>
#include <QVariant>
#include <QDateTime>
#include <QMessageBox>
#include <QPainter>

#include <tiff.h>
#include <tiffio.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

namespace ImageUtils {

// ========== 辅助函数声明 ==========
bool isTiffFile(const QString &path)
{
    return path.endsWith(".tif", Qt::CaseInsensitive)
           || path.endsWith(".tiff", Qt::CaseInsensitive);
}

// 读取 TIFF 的 DPI 信息
static QPair<int, int> getTiffDpi(TIFF *tif)
{
    float xres = 72.0, yres = 72.0;
    uint16_t resUnit = RESUNIT_NONE;

    TIFFGetFieldDefaulted(tif, TIFFTAG_XRESOLUTION, &xres);
    TIFFGetFieldDefaulted(tif, TIFFTAG_YRESOLUTION, &yres);
    TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resUnit);

    if (resUnit == RESUNIT_INCH) {
        return qMakePair(qRound(xres), qRound(yres));
    } else if (resUnit == RESUNIT_CENTIMETER) {
        return qMakePair(qRound(xres * 2.54), qRound(yres * 2.54));
    }
    // RESUNIT_NONE 或 RESUNIT_OTHER：返回默认值
    return qMakePair(72, 72);
}

// 轻量级读取 TIFF DPI 标签（不解码图像数据）
QPair<int, int> readTiffDpi(const QString &path)
{
    QByteArray array = path.toLocal8Bit();
    TIFF *tif = TIFFOpen(array.data(), "r");
    if (!tif)
        return { 0, 0 };

    QPair<int, int> dpi = getTiffDpi(tif);
    TIFFClose(tif);
    return dpi;
}

QSize readTiffSize(const QString &path)
{
    QByteArray array = path.toLocal8Bit();
    TIFF *tif = TIFFOpen(array.data(), "r");
    if (!tif)
        return { 0, 0 };
    uint32_t width = 0, height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFClose(tif);
    return QSize(width, height);
}

// 使用 libtiff 解码 TIFF 为显示用 QImage（线程安全）
// 支持 CMYK（LCMS2 批量转换 → RGB）、RGB/RGBA/灰度/16-bit/压缩/tiled 等
QImage loadTiffImage(const QString &path, QPair<int, int> *dpi, bool *isCmyk)
{
    QByteArray array = path.toLocal8Bit();
    TIFF *tif = TIFFOpen(array.data(), "r");
    if (!tif)
        return { };

    uint32_t width = 0, height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

    if (width == 0 || height == 0 || width > 50000 || height > 50000) {
        qWarning("TIFF invalid/large dimensions: %u x %u", width, height);
        TIFFClose(tif);
        return { };
    }

    if (dpi)
        *dpi = getTiffDpi(tif);

    uint16_t photometric = PHOTOMETRIC_MINISWHITE;
    uint16_t samplesPerPixel = 1;
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);

    bool cmyk = (photometric == PHOTOMETRIC_SEPARATED && samplesPerPixel >= 4);
    if (isCmyk)
        *isCmyk = cmyk;

    QImage result;

    if (cmyk) {
        // CMYK TIFF：读取原始 CMYK 扫描线 → 批量 LCMS2 CMYK→RGB 生成显示 QImage
        uint16_t bitsPerSample = 8, planarConfig = PLANARCONFIG_CONTIG;
        TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
        TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);

        if (bitsPerSample != 8) {
            qWarning("Unsupported CMYK TIFF bit depth: %u", bitsPerSample);
            TIFFClose(tif);
            return { };
        }

        // 先读取全部 CMYK 数据到内存
        tsize_t scanlineSize = TIFFScanlineSize(tif);
        QByteArray cmykData;
        cmykData.resize(width * height * 4);

        tdata_t scanBuf = _TIFFmalloc(scanlineSize);
        if (!scanBuf) {
            TIFFClose(tif);
            return { };
        }

        if (planarConfig == PLANARCONFIG_CONTIG) {
            for (uint32_t row = 0; row < height; ++row) {
                if (TIFFReadScanline(tif, scanBuf, row, 0) < 0)
                    break;
                memcpy(cmykData.data() + row * width * 4, scanBuf, width * 4);
            }
        } else {
            // 分离平面：合并四个通道
            tdata_t channelBuf = _TIFFmalloc(scanlineSize);
            if (channelBuf) {
                std::vector<uint8_t> rowPixels(width * 4);
                for (uint32_t row = 0; row < height; ++row) {
                    for (uint16_t sample = 0; sample < 4; ++sample) {
                        if (TIFFReadScanline(tif, channelBuf, row, sample) < 0)
                            break;
                        auto *src = static_cast<uint8_t *>(channelBuf);
                        for (uint32_t col = 0; col < width; ++col)
                            rowPixels[col * 4 + sample] = src[col];
                    }
                    memcpy(cmykData.data() + row * width * 4, rowPixels.data(), width * 4);
                }
                _TIFFfree(channelBuf);
            }
        }
        _TIFFfree(scanBuf);

        // 批量 LCMS2 CMYK→RGB 转换
        QATColorManager &cm = QATColorManager::instance();
        result = QImage(width, height, QImage::Format_ARGB32);
        if (cm.isValid()) {
            cmsHTRANSFORM xform = cm.createCmyk8ToBgra(
                INTENT_PERCEPTUAL, cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);
            QATColorManager::convertCmyk8ToBgra8(
                xform, reinterpret_cast<const uint8_t *>(cmykData.constData()), result.bits(),
                width, height);
            cmsDeleteTransform(xform);
        } else {
            // 无 LCMS2：简单数学逆转换 CMYK→RGB
            for (uint32_t y = 0; y < height; ++y) {
                QRgb *scanLine = reinterpret_cast<QRgb *>(result.scanLine(y));
                const uint8_t *src =
                    reinterpret_cast<const uint8_t *>(cmykData.constData()) + y * width * 4;
                for (uint32_t x = 0; x < width; ++x) {
                    int off = x * 4;
                    double c = src[off + 0] / 2.55, m = src[off + 1] / 2.55,
                           y2 = src[off + 2] / 2.55, k = src[off + 3] / 2.55;
                    double f = 1.0 - k / 100.0;
                    int r = AtMath::clamp(qRound(255 * (1 - c / 100) * f), 0, 255);
                    int g = AtMath::clamp(qRound(255 * (1 - m / 100) * f), 0, 255);
                    int b = AtMath::clamp(qRound(255 * (1 - y2 / 100) * f), 0, 255);
                    scanLine[x] = qRgb(r, g, b);
                }
            }
        }
    } else {
        // 非 CMYK TIFF：TIFFReadRGBAImage 统一处理
        uint32_t *raster = static_cast<uint32_t *>(_TIFFmalloc(width * height * sizeof(uint32_t)));
        if (!raster) {
            TIFFClose(tif);
            return { };
        }

        if (!TIFFReadRGBAImage(tif, width, height, raster, 0)) {
            _TIFFfree(raster);
            TIFFClose(tif);
            return { };
        }

        result = QImage(width, height, QImage::Format_ARGB32);
        for (uint32_t y = 0; y < height; ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(result.scanLine(y));
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t argb = raster[y * width + x];
                int a = TIFFGetA(argb);
                int r = TIFFGetR(argb);
                int g = TIFFGetG(argb);
                int b = TIFFGetB(argb);
                scanLine[x] = qRgba(r, g, b, a);
            }
        }
        _TIFFfree(raster);
    }

    TIFFClose(tif);
    return result;
}

// ========== 统一缩略图生成（缓存感知） ==========

QImage generateDisplayThumbnail(const QString &filePath, int targetLongEdge)
{
    // 1. 优先从缓存加载（原子 check+load，避免 TOCTOU 竞争）
    ImageCacheManager &cache = ImageCacheManager::instance();
    QImage cached = cache.loadIfCached(filePath);
    if (!cached.isNull())
        return cached;

    // 2. 缓存未命中：从原图解码
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    reader.setAllocationLimit(0);

    // JPEG 等格式支持解码时缩放，优先使用
    QSize origSize = reader.size();
    if (origSize.isValid()) {
        int longEdge = qMax(origSize.width(), origSize.height());
        if (longEdge > targetLongEdge) {
            qreal scale = static_cast<qreal>(targetLongEdge) / longEdge;
            QSize decodeSize(qMax(1, qRound(origSize.width() * scale)),
                             qMax(1, qRound(origSize.height() * scale)));
            reader.setScaledSize(decodeSize);
        }
    }

    QImage image = reader.read();
    if (image.isNull())
        return { };

    // 3. 若解码时未缩放（格式不支持 setScaledSize 或尺寸已 ≤ 目标），再执行缩放
    int longEdge = qMax(image.width(), image.height());
    if (longEdge > targetLongEdge) {
        qreal scale = static_cast<qreal>(targetLongEdge) / longEdge;
        int thumbW = qMax(1, qRound(image.width() * scale));
        int thumbH = qMax(1, qRound(image.height() * scale));
        image = image.scaled(thumbW, thumbH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // 4. 写入缓存（不影响返回值）
    cache.saveThumbnail(filePath, image);

    return image;
}

} // namespace ImageUtils

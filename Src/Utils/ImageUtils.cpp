#include "ImageUtils.h"
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
#include <stdexcept>

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

// 将 QImage 的 RGB 像素转换为 CMYK RawPixelBuffer
// LCMS2 可用时使用批量扫描线转换（TYPE_BGRA_8 → TYPE_CMYK_8），
// 否则回退到逐像素数学转换
static RawPixelBuffer imageToCmykBuffer(const QImage &image)
{
    QATColorManager &cm = QATColorManager::instance();
    bool useLcms2 = cm.isValid();

    int w = image.width();
    int h = image.height();
    RawPixelBuffer buf;
    buf.width = w;
    buf.height = h;
    buf.data.resize(w * h * 4);

    // Format_ARGB32 小端序扫描线 = BGRA 字节序，匹配 TYPE_BGRA_8
    QImage img32 = image.convertToFormat(QImage::Format_ARGB32);

    if (useLcms2) {
        // 创建线程独立的批量变换句柄，使用后立即销毁
        cmsHTRANSFORM xform = cm.createBgraToCmyk8(
            INTENT_PERCEPTUAL,
            cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);
        QATColorManager::convertBgra8ToCmyk8(xform, img32.constBits(), buf.ptr(0), w, h);
        cmsDeleteTransform(xform);
    } else {
        // 回退路径：逐像素数学转换（无 LCMS2 时）
        for (int y = 0; y < h; ++y) {
            const QRgb *scanLine =
                reinterpret_cast<const QRgb *>(img32.constScanLine(y));
            uint8_t *dst = buf.ptr(y);
            for (int x = 0; x < w; ++x) {
                QRgb c = scanLine[x];
                double r = qRed(c) / 255.0, g = qGreen(c) / 255.0,
                       b = qBlue(c) / 255.0;
                double kd = qMin(1.0 - r, qMin(1.0 - g, 1.0 - b));
                double cd = 0.0, md = 0.0, yd = 0.0;
                if (kd < 1.0) {
                    cd = (1.0 - r - kd) / (1.0 - kd) * 100.0;
                    md = (1.0 - g - kd) / (1.0 - kd) * 100.0;
                    yd = (1.0 - b - kd) / (1.0 - kd) * 100.0;
                }
                kd *= 100.0;
                int off = x * 4;
                dst[off + 0] = static_cast<uint8_t>(qRound(cd * 2.55));
                dst[off + 1] = static_cast<uint8_t>(qRound(md * 2.55));
                dst[off + 2] = static_cast<uint8_t>(qRound(yd * 2.55));
                dst[off + 3] = static_cast<uint8_t>(qRound(kd * 2.55));
            }
        }
    }
    return buf;
}

void importTiffWithLibtiff(const QString &path, ImportResult *result)
{
    QByteArray array = path.toLocal8Bit();
    TIFF *tif = TIFFOpen(array.data(), "r");
    if (!tif)
        return;

    uint32_t width = 0, height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

    // 安全检查：宽高必须大于 0
    if (width == 0 || height == 0) {
        qWarning("TIFF has invalid dimensions: %u x %u", width, height);
        TIFFClose(tif);
        return;
    }

    // 防止超大图像导致内存耗尽（限制 50000x50000）
    if (width > 50000 || height > 50000) {
        qWarning("TIFF dimensions too large: %u x %u", width, height);
        TIFFClose(tif);
        return;
    }

    // 检测色彩空间
    uint16_t photometric = PHOTOMETRIC_MINISWHITE;
    uint16_t samplesPerPixel = 1;
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);

    uint16_t bitsPerSample, planarConfig, inkSet;
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
    TIFFGetField(tif, TIFFTAG_INKSET, &inkSet);

    bool isCmyk =
        (photometric == PHOTOMETRIC_SEPARATED && samplesPerPixel >= 4);

    // 读取 DPI 信息
    if (result) {
        QPair<int, int> dpi = getTiffDpi(tif);
        result->dpiX = dpi.first;
        result->dpiY = dpi.second;
    }

    bool readError = false;

    if (isCmyk) {
        // CMYK TIFF：读取原始 CMYK 扫描线，

        // 保留原始 CMYK 像素数据
        RawPixelBuffer cmykMat;
        if (result) {
            cmykMat.width = width;
            cmykMat.height = height;
            cmykMat.data.resize(width * height * 4);
        }

        QByteArray cmykData;
        uint32_t *stripByteCounts = nullptr;
        if (TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripByteCounts)
            && stripByteCounts[0] > 0) {
            // ----- Scanline 读取（libtiff 内部处理条带） -----
            uint16_t samplesPerPixel = 0, bitsPerSample = 0;
            uint16_t photometric = 0, planarConfig = 0;
            TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
            TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
            TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
            TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);

            if (photometric != PHOTOMETRIC_SEPARATED || samplesPerPixel != 4) {
                qDebug() << "Not a standard CMYK TIFF.";
                TIFFClose(tif);
                return;
            }

            tsize_t scanlineSize = TIFFScanlineSize(tif);
            tdata_t buf = _TIFFmalloc(scanlineSize);
            if (!buf) {
                qDebug() << "Memory allocation failed.";
                TIFFClose(tif);
                return;
            }

            if (planarConfig == PLANARCONFIG_CONTIG) { // 交错存储
                for (uint32_t row = 0; row < height; ++row) {
                    if (TIFFReadScanline(tif, buf, row, 0) < 0) {
                        qDebug() << "Read error at row" << row;
                        break;
                    }
                    // 整行 CMYK 数据直接追加
                    cmykData.append(static_cast<const char *>(buf),
                                    scanlineSize);
                }
            } else { // PLANARCONFIG_SEPARATE：独立平面，需合并四个通道
                tdata_t channelBuf = _TIFFmalloc(scanlineSize);
                if (!channelBuf) {
                    _TIFFfree(buf);
                    TIFFClose(tif);
                    return;
                }

                std::vector<unsigned char> rowPixels(width
                                                     * 4); // 一行合并后的像素
                for (uint32_t row = 0; row < height; ++row) {
                    // 依次读取四个通道到 channelBuf，并填入 rowPixels 对应位置
                    for (uint16_t sample = 0; sample < samplesPerPixel;
                         ++sample) {
                        if (TIFFReadScanline(tif, channelBuf, row, sample)
                            < 0) {
                            qDebug() << "Read error at row" << row << "channel"
                                     << sample;
                            break;
                        }
                        unsigned char *chanData =
                            static_cast<unsigned char *>(channelBuf);
                        for (uint32_t col = 0; col < width; ++col) {
                            rowPixels[col * 4 + sample] = chanData[col];
                        }
                    }
                    // 将该行完整的 CMYK 数据追加到 cmykData
                    cmykData.append(
                        reinterpret_cast<const char *>(rowPixels.data()),
                        static_cast<int>(rowPixels.size()));
                }
                _TIFFfree(channelBuf);
            }
            _TIFFfree(buf);
        } else {
            // ----- 无条带信息，按二进制文件设置偏移量读取原始数据 -----
            const std::string filename = path.toStdString();
            const std::streampos startOffset = 194;
            const std::size_t byteCount =
                static_cast<std::size_t>(width) * height * 4; // CMYK

            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                qDebug() << "Cannot open file for binary read:" << path;
                TIFFClose(tif);
                return;
            }

            file.seekg(startOffset);
            if (!file) {
                qDebug() << "Failed to seek to offset" << startOffset;
                TIFFClose(tif);
                return;
            }

            std::vector<uint8_t> buffer(byteCount);
            file.read(reinterpret_cast<char *>(buffer.data()), byteCount);
            if (file.gcount() != static_cast<std::streamsize>(byteCount)) {
                qDebug() << "Read failed: expected" << byteCount << "bytes, got"
                         << file.gcount();
                TIFFClose(tif);
                return;
            }

            // 原始像素数据直接存入 cmykData
            cmykData.append(reinterpret_cast<const char *>(buffer.data()),
                            static_cast<int>(buffer.size()));
        }

        cmykMat.data = cmykData;
        if (result && !readError) {
            result->rawCmykMat = cmykMat;
        }

    } else {
        // 非 CMYK TIFF：使用 TIFFReadRGBAImage 自动处理各种格式
        // （RGB/RGBA/灰度/调色板/16-bit/压缩/tiled 等）
        uint32_t *raster = static_cast<uint32_t *>(
            _TIFFmalloc(width * height * sizeof(uint32_t)));
        if (!raster) {
            qWarning("Failed to allocate raster buffer for TIFF import");
            TIFFClose(tif);
            return;
        }

        if (!TIFFReadRGBAImage(tif, width, height, raster, 0)) {
            qWarning("TIFFReadRGBAImage failed");
            _TIFFfree(raster);
            TIFFClose(tif);
            return;
        }

        QImage img(width, height, QImage::Format_RGBA8888_Premultiplied);
        for (uint32_t y = 0; y < height; ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (uint32_t x = 0; x < width; ++x) {

                uint32_t argb = raster[y * width + x];
                int a = (argb >> 24) & 0xFF;
                int r = (argb >> 16) & 0xFF;
                int g = (argb >> 8) & 0xFF;
                int b = argb & 0xFF;

                scanLine[x] = qRgba(r, g, b, a);
            }
        }

        if (!img.isNull() && result) {
            result->rawCmykMat = imageToCmykBuffer(img);
        }

        _TIFFfree(raster);
    }

    TIFFClose(tif);
}

ImportResult loadImageFromFile(const QString &path)
{
    ImportResult result;
    result.filePath = path;

    QImageReader reader(path);
    reader.setAllocationLimit(0);
    result.image = reader.read();

    result.originalSize = result.image.size();

    if (!result.image.isNull()) {
        int dpmX = result.image.dotsPerMeterX();
        int dpmY = result.image.dotsPerMeterY();
        result.dpiX = qRound(dpmX / 39.3701); // DPM to DPI
        result.dpiY = qRound(dpmY / 39.3701);
    }

    if (isTiffFile(path)) {
        importTiffWithLibtiff(path, &result);
    } else {
        // 对于非 CMYK 源的图像（PNG、JPG、RGB TIFF 等），将每个像素的 RGB 转为 CMYK 存储
        if (!result.image.isNull()) {
            result.rawCmykMat = imageToCmykBuffer(result.image);
        }
    }

    return result;
}

ImportResult importImageWithDialog(QWidget *parent, const QSizeF &canvasSize)
{
    // 第一步：选择文件
    QString path = QFileDialog::getOpenFileName(
        parent, QObject::tr("Import Image"), QString(),
        QObject::tr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp);;"
                    "TIFF (*.tif *.tiff);;"
                    "PNG (*.png);;"
                    "JPEG (*.jpg *.jpeg);;"
                    "BMP (*.bmp);;"
                    "All Files (*)"));
    if (path.isEmpty())
        return ImportResult();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    // 读取图片
    ImportResult result = loadImageFromFile(path);
    QApplication::restoreOverrideCursor();
    if (!result.isValid())
        return result;

    // 第二步：对比图像尺寸与画布尺寸，询问是否缩放以适配
    if (!canvasSize.isEmpty()) {
        int iw = result.image.width();
        int ih = result.image.height();
        int cw = static_cast<int>(canvasSize.width());
        int ch = static_cast<int>(canvasSize.height());

        if (iw > cw || ih > ch) {
            auto answer = QMessageBox::question(
                parent, QObject::tr("Import Image"),
                QObject::tr("Image size (%1 x %2) exceeds canvas size (%3 x "
                            "%4).\nScale to fit canvas?")
                    .arg(iw)
                    .arg(ih)
                    .arg(cw)
                    .arg(ch),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            if (answer == QMessageBox::Yes) {
                double scale = qMin(static_cast<double>(cw) / iw,
                                    static_cast<double>(ch) / ih);
                int newW = qRound(iw * scale);
                int newH = qRound(ih * scale);
                result.image =
                    result.image.scaled(newW, newH, Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
            }
        }
    }

    return result;
}

bool exportTiffCmyk(const QString &path, const QImage &image,
                    const QList<QGraphicsItem *> &items,
                    const QRectF &exportRect)
{
    CmykItemSnapshot snapshot = collectCmykItemSnapshot(items, exportRect);
    return exportTiffCmykFromSnapshot(path, image, snapshot, exportRect);
}

} // namespace ImageUtils

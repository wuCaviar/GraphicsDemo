#include "ImageUtils.h"
#include "ImportImageDialog.h"
#include "IGraphicsItem.h"
#include "ImageItem.h"
#include "colortransform.h"

#include <QCoreApplication>
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
#include <QPainter>

#include <tiff.h>
#include <tiffio.h>

namespace ImageUtils {

// ========== 辅助函数声明 ==========
static void readTiffMetadata(TIFF *tif, QMap<QString, QVariant> &metadata);
static void applyDpiPolicy(ImportResult &result, const ImportParameters &params);
static void applyColorSpaceConversion(QImage &image, ImportParameters::ColorSpace colorSpace);
static void applyAlphaHandling(QImage &image, ImportParameters::AlphaHandling alphaHandling);

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

QImage importTiffWithLibtiff(const QString &path, const ImportParameters &params, ImportResult *result)
{
    TIFF *tif = TIFFOpen(path.toUtf8().constData(), "r");
    if (!tif)
        return QImage();

    uint32_t width = 0, height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

    // 安全检查：宽高必须大于 0
    if (width == 0 || height == 0) {
        qWarning("TIFF has invalid dimensions: %u x %u", width, height);
        TIFFClose(tif);
        return QImage();
    }

    // 防止超大图像导致内存耗尽（限制 50000x50000）
    if (width > 50000 || height > 50000) {
        qWarning("TIFF dimensions too large: %u x %u", width, height);
        TIFFClose(tif);
        return QImage();
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

    bool isCmyk = (photometric == PHOTOMETRIC_SEPARATED && samplesPerPixel >= 4);

    // 读取 DPI 信息
    if (result) {
        QPair<int, int> dpi = getTiffDpi(tif);
        result->dpiX = dpi.first;
        result->dpiY = dpi.second;
    }

    // 读取 TIFF 元数据（如果启用）
    if (result && params.preserveTiffMetadata) {
        readTiffMetadata(tif, result->metadata);
    }

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    bool readError = false;

    if (isCmyk) {
        // CMYK TIFF：读取原始 CMYK 扫描线，同时转换为 RGB 用于显示
        QATColorManager &cm = QATColorManager::instance();
        bool useLcms2 = cm.isValid();
        if (useLcms2) {
            cm.buildCMYK2RGBTransforms(INTENT_PERCEPTUAL,
                                       cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);
        }

        // 保留原始 CMYK 像素数据
        RawPixelBuffer cmykMat;
        if (result) {
            cmykMat.width = width;
            cmykMat.height = height;
            cmykMat.data.resize(width * height * 4);
        }

        tsize_t scanlineSize = TIFFScanlineSize(tif); // 一行数据的字节数

        QVector<uint8_t> cmykLine(scanlineSize);
        for (uint32_t y = 0; y < height; ++y) {
            if (TIFFReadScanline(tif, cmykLine.data(), y) < 0) {
                qWarning("TIFFReadScanline failed at row %u", y);
                readError = true;
                break;
            }

            // 保存原始 CMYK 数据
            if (result)
                memcpy(cmykMat.ptr(y), cmykLine.data(), width * 4);

            // 转换为 RGB 用于显示
            QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (uint32_t x = 0; x < width; ++x) {
                int off = x * 4;
                // libtiff CMYK: 0 = max ink, 255 = no ink
                // QATColorManager::toRgb 期望 0-100 范围
                double c = cmykLine[off + 0] / 255.0 * 100.0;
                double m = cmykLine[off + 1] / 255.0 * 100.0;
                double yv = cmykLine[off + 2] / 255.0 * 100.0;
                double k = cmykLine[off + 3] / 255.0 * 100.0;

                QColor rgb = cm.toRgb(QATColorManager::Cmyk{c, m, yv, k});
                scanLine[x] = rgb.rgba();
            }
        }

        if (result && !readError) {
            result->rawCmykMat = cmykMat;
            result->isCmykSource = true;
        }
    } else {
        // 非 CMYK TIFF：使用 TIFFReadRGBAImage 自动处理各种格式
        // （RGB/RGBA/灰度/调色板/16-bit/压缩/tiled 等）
        uint32_t *raster = static_cast<uint32_t *>(_TIFFmalloc(width * height * sizeof(uint32_t)));
        if (!raster) {
            qWarning("Failed to allocate raster buffer for TIFF import");
            TIFFClose(tif);
            return QImage();
        }

        if (!TIFFReadRGBAImage(tif, width, height, raster, 0)) {
            qWarning("TIFFReadRGBAImage failed");
            _TIFFfree(raster);
            TIFFClose(tif);
            return QImage();
        }

        // TIFFReadRGBAImage 输出 ABGR（libtiff 约定）：需要转换为 ARGB
        QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(0));
        for (uint32_t i = 0; i < width * height; ++i) {
            uint32_t abgr = raster[i];
            int a = (abgr >> 24) & 0xFF;
            int b = (abgr >> 16) & 0xFF;
            int g = (abgr >> 8) & 0xFF;
            int r = abgr & 0xFF;
            scanLine[i] = qRgba(r, g, b, a);
        }

        // 保留原始 TIFF 像素数据
        if (result) {
            result->rawTiffMat.width = width;
            result->rawTiffMat.height = height;
            result->rawTiffMat.data = QByteArray(reinterpret_cast<const char *>(raster), width * height * 4);
        }

        _TIFFfree(raster);
    }

    TIFFClose(tif);

    if (readError)
        return QImage();

    return image;
}

// 读取 TIFF 元数据
static void readTiffMetadata(TIFF *tif, QMap<QString, QVariant> &metadata)
{
    // 读取常见的 TIFF 标签
    char *tagValue = nullptr;

    // 软件信息
    if (TIFFGetField(tif, TIFFTAG_SOFTWARE, &tagValue) && tagValue) {
        metadata["Software"] = QString::fromLatin1(tagValue);
    }

    // 文档名称
    if (TIFFGetField(tif, TIFFTAG_DOCUMENTNAME, &tagValue) && tagValue) {
        metadata["DocumentName"] = QString::fromLatin1(tagValue);
    }

    // 图像描述
    if (TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &tagValue) && tagValue) {
        metadata["ImageDescription"] = QString::fromLatin1(tagValue);
    }

    // 作者
    if (TIFFGetField(tif, TIFFTAG_ARTIST, &tagValue) && tagValue) {
        metadata["Artist"] = QString::fromLatin1(tagValue);
    }

    // 版权
    if (TIFFGetField(tif, TIFFTAG_COPYRIGHT, &tagValue) && tagValue) {
        metadata["Copyright"] = QString::fromLatin1(tagValue);
    }

    // 日期时间
    if (TIFFGetField(tif, TIFFTAG_DATETIME, &tagValue) && tagValue) {
        metadata["DateTime"] = QString::fromLatin1(tagValue);
    }

    // 色度信息（白点、原色）
    float *colorant = nullptr;
    if (TIFFGetField(tif, TIFFTAG_WHITEPOINT, &colorant) && colorant) {
        metadata["WhitePointX"] = colorant[0];
        metadata["WhitePointY"] = colorant[1];
    }
}

ImportResult loadImageFromFile(const QString &path, const ImportParameters &params)
{
    ImportResult result;
    result.filePath = path;

    if (isTiffFile(path)) {
        result.image = importTiffWithLibtiff(path, params, &result);
    } else {
        // 使用 QImageReader 读取非 TIFF 图像
        QImageReader reader(path);
        result.image = reader.read();

        // 从已读取的图像获取 DPI（dots per meter 转为 DPI）
        if (!result.image.isNull()) {
            int dpmX = result.image.dotsPerMeterX();
            int dpmY = result.image.dotsPerMeterY();
            result.dpiX = qRound(dpmX / 39.3701);  // DPM to DPI
            result.dpiY = qRound(dpmY / 39.3701);
        }

        // 读取元数据（使用 QImage 的 text() 方法）
        if (params.preserveTiffMetadata) {
            QImage img = result.image;
            QStringList keys = img.textKeys();
            for (const QString &key : keys) {
                result.metadata[key] = img.text(key);
            }
        }
    }

    if (result.image.isNull()) {
        qWarning("Failed to load image: %s", qPrintable(path));
        return result;
    }

    // 应用 DPI 策略
    applyDpiPolicy(result, params);

    // 应用颜色空间转换
    applyColorSpaceConversion(result.image, params.colorSpace);

    // 应用 Alpha 通道处理
    applyAlphaHandling(result.image, params.alphaHandling);

    // 应用缩放
    if (params.enableScaling && params.scaleFactor != 1.0) {
        int newWidth = qRound(result.image.width() * params.scaleFactor);
        int newHeight = qRound(result.image.height() * params.scaleFactor);
        result.image = result.image.scaled(QSize(newWidth, newHeight), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    return result;
}

// 应用 DPI 策略
static void applyDpiPolicy(ImportResult &result, const ImportParameters &params)
{
    switch (params.dpiPolicy) {
    case ImportParameters::DpiPolicy::UseImageDpi:
        // 使用图像自带的 DPI，无需修改
        break;
    case ImportParameters::DpiPolicy::ForceDpi:
        result.dpiX = params.forcedDpi;
        result.dpiY = params.forcedDpi;
        break;
    case ImportParameters::DpiPolicy::IgnoreDpi:
        result.dpiX = 72;
        result.dpiY = 72;
        break;
    }
}

// 应用颜色空间转换
static void applyColorSpaceConversion(QImage &image, ImportParameters::ColorSpace colorSpace)
{
    if (colorSpace == ImportParameters::ColorSpace::KeepOriginal)
        return;

    // 使用 Qt 的 QColorSpace 进行颜色空间转换
    QColorSpace targetSpace;
    switch (colorSpace) {
    case ImportParameters::ColorSpace::ConvertToSRGB:
        targetSpace = QColorSpace::SRgb;
        break;
    case ImportParameters::ColorSpace::ConvertToAdobeRGB:
        // Adobe RGB 需要定义或使用预定义值
        targetSpace = QColorSpace(QColorSpace::Primaries::AdobeRgb, QColorSpace::TransferFunction::Gamma, 2.2);
        break;
    default:
        return;
    }

    if (image.colorSpace() != targetSpace) {
        image.convertToColorSpace(targetSpace);
    }
}

// 应用 Alpha 通道处理
static void applyAlphaHandling(QImage &image, ImportParameters::AlphaHandling alphaHandling)
{
    switch (alphaHandling) {
    case ImportParameters::AlphaHandling::Keep:
        // 保持 Alpha 通道，无需修改
        break;
    case ImportParameters::AlphaHandling::Discard:
        // 丢弃 Alpha 通道，替换为白色背景
        if (image.hasAlphaChannel()) {
            QImage newImage(image.size(), QImage::Format_RGB32);
            newImage.fill(Qt::white);
            QPainter painter(&newImage);
            painter.drawImage(0, 0, image);
            image = newImage;
        }
        break;
    case ImportParameters::AlphaHandling::Premultiply:
        // 预乘 Alpha
        if (image.hasAlphaChannel()) {
            image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
        break;
    }
}

// ========== 导出功能辅助函数 ==========

ImportResult importImageWithDialog(QWidget *parent, ImportParameters *params)
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

    // 第二步：显示参数对话框（如果 params 为 nullptr，使用默认参数）
    ImportParameters importParams;
    if (params) {
        importParams = *params;
    }

    ImportImageDialog dialog(parent, importParams);
    if (dialog.exec() == QDialog::Accepted) {
        importParams = dialog.getParameters();
    }

    // 第三步：加载图像并应用参数
    return loadImageFromFile(path, importParams);
}

// ========== TIFF 导出 ==========

// 写入 TIFF 元数据
static void writeTiffMetadata(TIFF *tif, const ExportParameters &params)
{
    // 始终写入软件和时间戳
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "GraphicsDemo");
    QString dateTime = QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    TIFFSetField(tif, TIFFTAG_DATETIME, dateTime.toUtf8().constData());

    if (!params.tiff.preserveMetadata)
        return;

    // 预留：从导入元数据写回 DocumentName, ImageDescription, Artist, Copyright
    // 需要调用方传入 ImportResult::metadata，当前未连接
}

// 应用颜色空间转换（导出时）
static void applyColorSpaceForExport(QImage &image, ExportParameters::ColorSpace colorSpace)
{
    if (colorSpace == ExportParameters::ColorSpace::KeepOriginal)
        return;

    QColorSpace targetSpace;
    switch (colorSpace) {
    case ExportParameters::ColorSpace::ConvertToSRGB:
        targetSpace = QColorSpace::SRgb;
        break;
    case ExportParameters::ColorSpace::ConvertToAdobeRGB:
        targetSpace = QColorSpace(QColorSpace::Primaries::AdobeRgb,
                                  QColorSpace::TransferFunction::Gamma, 2.2);
        break;
    default:
        return;
    }

    if (image.colorSpace() != targetSpace) {
        image.convertToColorSpace(targetSpace);
    }
}

bool exportTiffLossless(const QString &path, const QImage &image, const ExportParameters &params)
{
    // 1. 颜色空间转换
    QImage img = image;
    applyColorSpaceForExport(img, params.colorSpace);

    // 2. 透明度处理
    bool hasAlpha = true;
    if (params.transparency == ExportParameters::TransparencyHandling::FlattenOnWhite) {
        QImage flattened(img.size(), QImage::Format_ARGB32);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, img);
        painter.end();
        img = flattened;
        hasAlpha = false;
    }

    // 3. 位深度与格式转换
    int bitsPerSample = (params.tiff.bitDepth == ExportParameters::BitDepth::Bits16) ? 16 : 8;
    int samplesPerPixel = hasAlpha ? 4 : 3;

    // 4. 字节序控制
    const char *mode = (params.tiff.byteOrder == ExportParameters::ByteOrder::LittleEndian)
                           ? "wl" : "wb";
    TIFF *tif = TIFFOpen(path.toUtf8().constData(), mode);
    if (!tif)
        return false;

    int width = img.width();
    int height = img.height();

    // 5. 基本标签
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bitsPerSample);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samplesPerPixel);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG,
                 params.tiff.planarConfig == ExportParameters::PlanarConfig::Separate
                     ? PLANARCONFIG_SEPARATE : PLANARCONFIG_CONTIG);

    // 6. 压缩
    uint16_t compression = COMPRESSION_NONE;
    switch (params.tiff.compression) {
    case ExportParameters::CompressionType::None:
        compression = COMPRESSION_NONE;
        break;
    case ExportParameters::CompressionType::LZW:
        compression = COMPRESSION_LZW;
        break;
    case ExportParameters::CompressionType::ZIP:
        compression = COMPRESSION_ADOBE_DEFLATE;
        break;
    case ExportParameters::CompressionType::JPEG:
        compression = COMPRESSION_JPEG;
        TIFFSetField(tif, TIFFTAG_JPEGQUALITY, params.tiff.jpegQuality);
        break;
    case ExportParameters::CompressionType::PackBits:
        compression = COMPRESSION_PACKBITS;
        break;
    }
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    // 7. 预测器（LZW 和 ZIP 均支持水平差分）
    if (params.tiff.predictor == ExportParameters::Predictor::Horizontal &&
        (params.tiff.compression == ExportParameters::CompressionType::LZW ||
         params.tiff.compression == ExportParameters::CompressionType::ZIP)) {
        TIFFSetField(tif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    }

    // 8. EXTRASAMPLES（保留透明度时必须声明 Alpha 通道）
    if (hasAlpha) {
        uint16_t extra = EXTRASAMPLE_UNASSALPHA;
        TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, &extra);
    }

    // 9. ROWSPERSTRIP — 使用 libtiff 默认计算（约 8KB 一 strip）
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));

    // 10. 分辨率
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, static_cast<float>(params.dpi));
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, static_cast<float>(params.dpi));
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);

    // 11. 元数据
    writeTiffMetadata(tif, params);

    // 12. ICC 配置文件
    if (params.tiff.embedICCProfile && img.colorSpace().isValid()) {
        QByteArray iccProfile = img.colorSpace().iccProfile();
        if (!iccProfile.isEmpty()) {
            TIFFSetField(tif, TIFFTAG_ICCPROFILE,
                         static_cast<uint32_t>(iccProfile.size()),
                         iccProfile.constData());
        }
    }

    // 13. 写入像素数据
    bool writeError = false;
    bool isContig = (params.tiff.planarConfig == ExportParameters::PlanarConfig::Contig);

    if (bitsPerSample == 8) {
        // ---- 8-bit ----
        QImage img32 = img.convertToFormat(hasAlpha ? QImage::Format_ARGB32
                                                     : QImage::Format_RGB32);
        QVector<uint8_t> rowBuf(width * samplesPerPixel);

        for (int y = 0; y < height; ++y) {
            const QRgb *scanLine = reinterpret_cast<const QRgb *>(img32.constScanLine(y));

            if (isContig) {
                // 交错模式：RGBA 或 RGB
                for (int x = 0; x < width; ++x) {
                    QRgb c = scanLine[x];
                    int off = x * samplesPerPixel;
                    rowBuf[off + 0] = qRed(c);
                    rowBuf[off + 1] = qGreen(c);
                    rowBuf[off + 2] = qBlue(c);
                    if (hasAlpha)
                        rowBuf[off + 3] = qAlpha(c);
                }
                if (TIFFWriteScanline(tif, rowBuf.data(), y) < 0) {
                    writeError = true;
                    break;
                }
            } else {
                // 分离模式：逐通道写入
                for (int plane = 0; plane < samplesPerPixel; ++plane) {
                    for (int x = 0; x < width; ++x) {
                        QRgb c = scanLine[x];
                        switch (plane) {
                        case 0: rowBuf[x] = qRed(c); break;
                        case 1: rowBuf[x] = qGreen(c); break;
                        case 2: rowBuf[x] = qBlue(c); break;
                        case 3: rowBuf[x] = qAlpha(c); break;
                        }
                    }
                    if (TIFFWriteScanline(tif, rowBuf.data(), y, plane) < 0) {
                        writeError = true;
                        break;
                    }
                }
                if (writeError) break;
            }
        }
    } else {
        // ---- 16-bit ----
        QImage img64 = img.convertToFormat(hasAlpha ? QImage::Format_RGBA64
                                                     : QImage::Format_RGBX64);
        QVector<uint16_t> rowBuf(width * samplesPerPixel);

        for (int y = 0; y < height; ++y) {
            const QRgba64 *scanLine = reinterpret_cast<const QRgba64 *>(img64.constScanLine(y));

            if (isContig) {
                for (int x = 0; x < width; ++x) {
                    QRgba64 c = scanLine[x];
                    int off = x * samplesPerPixel;
                    rowBuf[off + 0] = c.red();
                    rowBuf[off + 1] = c.green();
                    rowBuf[off + 2] = c.blue();
                    if (hasAlpha)
                        rowBuf[off + 3] = c.alpha();
                }
                if (TIFFWriteScanline(tif, rowBuf.data(), y) < 0) {
                    writeError = true;
                    break;
                }
            } else {
                for (int plane = 0; plane < samplesPerPixel; ++plane) {
                    for (int x = 0; x < width; ++x) {
                        QRgba64 c = scanLine[x];
                        switch (plane) {
                        case 0: rowBuf[x] = c.red(); break;
                        case 1: rowBuf[x] = c.green(); break;
                        case 2: rowBuf[x] = c.blue(); break;
                        case 3: rowBuf[x] = c.alpha(); break;
                        }
                    }
                    if (TIFFWriteScanline(tif, rowBuf.data(), y, plane) < 0) {
                        writeError = true;
                        break;
                    }
                }
                if (writeError) break;
            }
        }
    }

    TIFFClose(tif);
    return !writeError;
}

bool exportTiffCmyk(const QString &path, const QImage &image,
                    const QList<QGraphicsItem *> &items, const QRectF &exportRect,
                    const ExportParameters &params)
{
    // 1. Flatten transparency on white
    QImage img(image.size(), QImage::Format_ARGB32);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        p.drawImage(0, 0, image);
        p.end();
    }

    int width = img.width();
    int height = img.height();

    // 2. Open TIFF
    const char *mode = (params.tiff.byteOrder == ExportParameters::ByteOrder::LittleEndian)
                           ? "wl" : "wb";
    TIFF *tif = TIFFOpen(path.toUtf8().constData(), mode);
    if (!tif)
        return false;

    // 3. TIFF tags — CMYK
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    uint16_t compression = COMPRESSION_NONE;
    switch (params.tiff.compression) {
    case ExportParameters::CompressionType::None:    compression = COMPRESSION_NONE; break;
    case ExportParameters::CompressionType::LZW:     compression = COMPRESSION_LZW; break;
    case ExportParameters::CompressionType::ZIP:     compression = COMPRESSION_ADOBE_DEFLATE; break;
    case ExportParameters::CompressionType::JPEG:    compression = COMPRESSION_JPEG;
        TIFFSetField(tif, TIFFTAG_JPEGQUALITY, params.tiff.jpegQuality); break;
    case ExportParameters::CompressionType::PackBits: compression = COMPRESSION_PACKBITS; break;
    }
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    if (params.tiff.predictor == ExportParameters::Predictor::Horizontal &&
        (params.tiff.compression == ExportParameters::CompressionType::LZW ||
         params.tiff.compression == ExportParameters::CompressionType::ZIP))
        TIFFSetField(tif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);

    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, static_cast<float>(params.dpi));
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, static_cast<float>(params.dpi));
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    writeTiffMetadata(tif, params);

    // 4. Embed CMYK ICC profile
    QATColorManager &cm = QATColorManager::instance();
    bool useLcms2 = cm.isValid();
    if (useLcms2) {
        cm.buildRGB2CMYKTransforms(INTENT_PERCEPTUAL,
                                   cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);
    }
    // Always embed the CMYK ICC profile (same path as QATColorManager)
#if defined(Q_OS_WIN)
    QString iccPath = QCoreApplication::applicationDirPath() + "/../ICC Profile/CMYK/JapanColor2001Coated.icc";
#elif defined(Q_OS_MACOS)
    QString iccPath = "/Volumes/Caviar/Test/GraphicsDemo/Bin/../ICC Profile/CMYK/JapanColor2001Coated.icc";
#endif
    {
        QFile iccFile(iccPath);
        if (iccFile.open(QIODevice::ReadOnly)) {
            QByteArray iccData = iccFile.readAll();
            TIFFSetField(tif, TIFFTAG_ICCPROFILE,
                         static_cast<uint32_t>(iccData.size()), iccData.constData());
            iccFile.close();
        }
    }

    // 5. Convert and write pixel data
    QVector<uint8_t> rowBuf(width * 4);
    QImage img32 = img.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < height; ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(img32.constScanLine(y));

        for (int x = 0; x < width; ++x) {
            QRgb c = scanLine[x];
            double cd, md, yd, kd;
            if (useLcms2) {
                QATColorManager::Cmyk cmyk = cm.toCmyk(QColor(c));
                cd = cmyk.c; md = cmyk.m; yd = cmyk.y; kd = cmyk.k;
            } else {
                // Fallback: naive conversion
                double r = qRed(c) / 255.0, g = qGreen(c) / 255.0, b = qBlue(c) / 255.0;
                cd = 1.0 - r; md = 1.0 - g; yd = 1.0 - b;
                kd = qMin(cd, qMin(md, yd));
                cd = (cd - kd) / (1.0 - kd) * 100.0;
                md = (md - kd) / (1.0 - kd) * 100.0;
                yd = (yd - kd) / (1.0 - kd) * 100.0;
                kd *= 100.0;
            }
            int off = x * 4;
            rowBuf[off + 0] = static_cast<uint8_t>(qBound(0.0, cd * 2.55, 255.0));
            rowBuf[off + 1] = static_cast<uint8_t>(qBound(0.0, md * 2.55, 255.0));
            rowBuf[off + 2] = static_cast<uint8_t>(qBound(0.0, yd * 2.55, 255.0));
            rowBuf[off + 3] = static_cast<uint8_t>(qBound(0.0, kd * 2.55, 255.0));
        }

        // Overwrite with exact CMYK for items that have stored values
        for (QGraphicsItem *gi : items) {
            auto *ii = dynamic_cast<IGraphicsItem *>(gi);
            if (!ii) continue;

            // Brush (solid fill)
            if (ii->hasBrushCmyk() && ii->itemBrush().style() != Qt::NoBrush) {
                double bc, bm, by, bk;
                ii->brushCmyk(bc, bm, by, bk);
                QRectF sceneRect = gi->sceneBoundingRect();
                QRectF pixelRect((sceneRect.x() - exportRect.x()),
                                 (sceneRect.y() - exportRect.y()),
                                 sceneRect.width(), sceneRect.height());
                int x0 = qBound(0, static_cast<int>(pixelRect.left()), width - 1);
                int x1 = qBound(0, static_cast<int>(pixelRect.right()), width - 1);
                if (y >= pixelRect.top() && y <= pixelRect.bottom()) {
                    uint8_t c8 = static_cast<uint8_t>(qBound(0.0, bc * 2.55, 255.0));
                    uint8_t m8 = static_cast<uint8_t>(qBound(0.0, bm * 2.55, 255.0));
                    uint8_t y8 = static_cast<uint8_t>(qBound(0.0, by * 2.55, 255.0));
                    uint8_t k8 = static_cast<uint8_t>(qBound(0.0, bk * 2.55, 255.0));
                    for (int x = x0; x <= x1; ++x) {
                        int off = x * 4;
                        rowBuf[off + 0] = c8;
                        rowBuf[off + 1] = m8;
                        rowBuf[off + 2] = y8;
                        rowBuf[off + 3] = k8;
                    }
                }
            }

            // Pen (stroke)
            if (ii->hasPenCmyk() && ii->itemPen().style() != Qt::NoPen) {
                double pc, pm, py, pk;
                ii->penCmyk(pc, pm, py, pk);
                qreal penWidth = ii->itemPen().widthF();
                QRectF sceneRect = gi->sceneBoundingRect();
                // Expand rect by half pen width to cover stroke area
                QRectF strokeRect = sceneRect.adjusted(-penWidth/2, -penWidth/2,
                                                       penWidth/2, penWidth/2);
                QRectF pixelRect((strokeRect.x() - exportRect.x()),
                                 (strokeRect.y() - exportRect.y()),
                                 strokeRect.width(), strokeRect.height());
                // Only overwrite border pixels (not interior, which is handled by brush)
                if (ii->itemBrush().style() == Qt::NoBrush && ii->hasBrushCmyk()) {
                    // skip — brush already handled
                } else if (ii->itemBrush().style() == Qt::NoBrush) {
                    // No brush — overwrite entire stroke area
                    int x0 = qBound(0, static_cast<int>(pixelRect.left()), width - 1);
                    int x1 = qBound(0, static_cast<int>(pixelRect.right()), width - 1);
                    if (y >= pixelRect.top() && y <= pixelRect.bottom()) {
                        uint8_t c8 = static_cast<uint8_t>(qBound(0.0, pc * 2.55, 255.0));
                        uint8_t m8 = static_cast<uint8_t>(qBound(0.0, pm * 2.55, 255.0));
                        uint8_t y8 = static_cast<uint8_t>(qBound(0.0, py * 2.55, 255.0));
                        uint8_t k8 = static_cast<uint8_t>(qBound(0.0, pk * 2.55, 255.0));
                        for (int x = x0; x <= x1; ++x) {
                            int off = x * 4;
                            rowBuf[off + 0] = c8;
                            rowBuf[off + 1] = m8;
                            rowBuf[off + 2] = y8;
                            rowBuf[off + 3] = k8;
                        }
                    }
                }
            }

            // ImageItem with raw CMYK source: composite raw CMYK pixels directly
            auto *imgItem = dynamic_cast<ImageItem *>(gi);
            if (imgItem && imgItem->isCmykSource()) {
                const RawPixelBuffer &cmykMat = imgItem->rawCmykPixels();
                int srcW = cmykMat.width;
                int srcH = cmykMat.height;
                QRectF sceneRect = gi->sceneBoundingRect();
                QRectF pixelRect((sceneRect.x() - exportRect.x()),
                                 (sceneRect.y() - exportRect.y()),
                                 sceneRect.width(), sceneRect.height());
                if (srcW > 0 && srcH > 0 && y >= pixelRect.top() && y <= pixelRect.bottom()) {
                    int x0 = qBound(0, static_cast<int>(pixelRect.left()), width - 1);
                    int x1 = qBound(0, static_cast<int>(pixelRect.right()), width - 1);
                    double scaleX = static_cast<double>(srcW) / pixelRect.width();
                    double scaleY = static_cast<double>(srcH) / pixelRect.height();
                    int srcY = qBound(0, static_cast<int>((y - pixelRect.top()) * scaleY), srcH - 1);
                    const uint8_t *srcRow = cmykMat.ptr(srcY);
                    for (int x = x0; x <= x1; ++x) {
                        int srcX = qBound(0, static_cast<int>((x - pixelRect.left()) * scaleX), srcW - 1);
                        int srcOff = srcX * 4;
                        int dstOff = x * 4;
                        // libtiff: 0 = max ink, 255 = no ink
                        // export:  0 = no ink,  255 = max ink → invert
                        rowBuf[dstOff + 0] = srcRow[srcOff + 0];
                        rowBuf[dstOff + 1] = srcRow[srcOff + 1];
                        rowBuf[dstOff + 2] = srcRow[srcOff + 2];
                        rowBuf[dstOff + 3] = srcRow[srcOff + 3];
                    }
                }
            }
        }

        if (TIFFWriteScanline(tif, rowBuf.data(), y) < 0) {
            TIFFClose(tif);
            return false;
        }
    }

    TIFFClose(tif);
    return true;
}

bool exportImageWithParams(const QString &path, const QImage &image, const ExportParameters &params)
{
    QImage img = image;

    // 处理透明度
    if (params.transparency == ExportParameters::TransparencyHandling::FlattenOnWhite) {
        QImage flattened(img.size(), QImage::Format_ARGB32_Premultiplied);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, img);
        painter.end();
        img = flattened;
    }

    // 处理颜色空间转换
    applyColorSpaceForExport(img, params.colorSpace);

    // 使用 QImageWriter 设置参数
    QImageWriter writer(path);
    if (!writer.canWrite()) {
        qWarning("Cannot write image format: %s", qPrintable(path));
        return false;
    }

    // 根据格式设置参数
    QString fmt = QFileInfo(path).suffix().toLower();
    if (fmt == "png") {
        writer.setCompression(params.png.compressionLevel);
    } else if (fmt == "jpg" || fmt == "jpeg") {
        writer.setQuality(params.jpeg.quality);
    }

    return writer.write(img);
}

} // namespace ImageUtils

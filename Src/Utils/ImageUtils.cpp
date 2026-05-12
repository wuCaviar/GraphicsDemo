#include "ImageUtils.h"
#include "ImportImageDialog.h"

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

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    QVector<uint32_t> buf(width);

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

    bool readError = false;
    for (uint32_t y = 0; y < height; ++y) {
        if (TIFFReadScanline(tif, buf.data(), y) < 0) {
            qWarning("TIFFReadScanline failed at row %u", y);
            readError = true;
            break;
        }
        QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t rgba = buf[x];
            int a = (rgba >> 24) & 0xFF;
            int r = (rgba >> 16) & 0xFF;
            int g = (rgba >> 8) & 0xFF;
            int b = rgba & 0xFF;
            scanLine[x] = qRgba(r, g, b, a);
        }
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
        // 导入 TIFF 并读取元数据
        result.image = importTiffWithLibtiff(path, params, &result);

        // 保留原始 TIFF 数据用于无损导出
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            result.rawTiffData = f.readAll();
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

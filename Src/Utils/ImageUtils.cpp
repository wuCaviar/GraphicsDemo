#include "ImageUtils.h"
#include "ImportImageDialog.h"

#include <QFileDialog>
#include <QFile>
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

        // 读取 DPI 信息（通过 QImage 的 dotsPerMeterX/Y 转换）
        QImage tempImage;
        if (reader.read(&tempImage)) {
            result.image = tempImage;
            // 从 QImage 获取 DPI（dots per meter 转为 DPI）
            int dpmX = tempImage.dotsPerMeterX();
            int dpmY = tempImage.dotsPerMeterY();
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

bool applyExportParameters(QImageWriter &writer, const ExportParameters &params)
{
    // 设置压缩类型
    switch (params.compression) {
    case ExportParameters::CompressionType::None:
        writer.setCompression(0);
        break;
    case ExportParameters::CompressionType::LZW:
        writer.setCompression(1);  // LZW compression
        break;
    case ExportParameters::CompressionType::ZIP:
        writer.setCompression(2);  // ZIP/DEFLATE compression
        break;
    case ExportParameters::CompressionType::JPEG:
        writer.setCompression(params.jpegQuality);
        break;
    }

    // 设置 PNG 压缩级别（仅对 PNG 有效）
    if (writer.format() == "png") {
        writer.setCompression(params.pngCompression);
    }

    // 设置 JPEG 质量（仅对 JPEG 有效）
    if (writer.format() == "jpeg" || writer.format() == "jpg") {
        writer.setQuality(params.jpegQuality);
    }

    // 设置元数据保留
    if (params.preserveMetadata) {
        // QImageWriter 会自动保留一些元数据
        // 这里可以添加更多元数据设置
    }

    return writer.canWrite();
}

void applyDpiToImage(QImage &image, const ImportResult &result, const ExportParameters &params)
{
    Q_UNUSED(image);
    Q_UNUSED(result);
    Q_UNUSED(params);
    // QImage 本身不存储 DPI，DPI 信息在导出时通过 QImageWriter::setResolution() 设置
    // 这个函数在导出时调用，在实际导出函数中会用到
}

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

} // namespace ImageUtils

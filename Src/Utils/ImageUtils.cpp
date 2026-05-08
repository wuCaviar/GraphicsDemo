#include "ImageUtils.h"

#include <QFileDialog>
#include <QFile>

#include <tiff.h>
#include <tiffio.h>

namespace ImageUtils {

bool isTiffFile(const QString &path)
{
    return path.endsWith(".tif", Qt::CaseInsensitive)
           || path.endsWith(".tiff", Qt::CaseInsensitive);
}

QImage importTiffWithLibtiff(const QString &path)
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

ImportResult loadImageFromFile(const QString &path)
{
    ImportResult result;
    result.filePath = path;

    if (isTiffFile(path)) {
        result.image = importTiffWithLibtiff(path);
        // 保留原始 TIFF 数据用于无损导出
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            result.rawTiffData = f.readAll();
    } else {
        result.image.load(path);
    }

    if (result.image.isNull())
        qWarning("Failed to load image: %s", qPrintable(path));

    return result;
}

ImportResult importImageWithDialog(QWidget *parent)
{
    QString path = QFileDialog::getOpenFileName(
            parent, QObject::tr("Import Image"), QString(),
            QObject::tr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp);;TIFF (*.tif "
                        "*.tiff);;All Files (*)"));
    if (path.isEmpty())
        return ImportResult();

    return loadImageFromFile(path);
}

} // namespace ImageUtils

#include "ImageWorker.h"
#include "IGraphicsItem.h"
#include "ImageItem.h"
#include "colortransform.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QPaintEngine>
#include <QPainter>

#include <tiff.h>
#include <tiffio.h>
#include <cmath>

namespace ImageUtils {

// ========== Pipeline 实现 ==========

void ImageImportPipeline::addProcessor(std::unique_ptr<IImportPostProcessor> processor)
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

void ImageImportPipeline::run(ImportResult &result)
{
    for (auto &p : m_processors)
        p->process(result);
}

void ImageExportPipeline::addProcessor(std::unique_ptr<IExportPreProcessor> processor)
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

// ========== CMYK 快照收集（主线程） ==========

CmykItemSnapshot collectCmykItemSnapshot(const QList<QGraphicsItem *> &items,
                                          const QRectF &exportRect)
{
    Q_UNUSED(exportRect);
    CmykItemSnapshot snapshot;

    for (QGraphicsItem *gi : items) {
        auto *ii = dynamic_cast<IGraphicsItem *>(gi);
        if (!ii)
            continue;

        // Brush CMYK override
        if (ii->hasBrushCmyk() && ii->itemBrush().style() != Qt::NoBrush) {
            double bc, bm, by, bk;
            ii->brushCmyk(bc, bm, by, bk);
            CmykItemSnapshot::BrushCmyk brush;
            brush.sceneRect = gi->sceneBoundingRect();
            brush.c = static_cast<uint8_t>(qBound(0.0, bc * 2.55, 255.0));
            brush.m = static_cast<uint8_t>(qBound(0.0, bm * 2.55, 255.0));
            brush.y = static_cast<uint8_t>(qBound(0.0, by * 2.55, 255.0));
            brush.k = static_cast<uint8_t>(qBound(0.0, bk * 2.55, 255.0));
            snapshot.brushItems.append(brush);
        }

        // Pen CMYK override
        if (ii->hasPenCmyk() && ii->itemPen().style() != Qt::NoPen) {
            double pc, pm, py, pk;
            ii->penCmyk(pc, pm, py, pk);
            CmykItemSnapshot::PenCmyk pen;
            pen.sceneRect = gi->sceneBoundingRect();
            pen.penWidth = ii->itemPen().widthF();
            pen.hasBrush = (ii->itemBrush().style() != Qt::NoBrush);
            pen.c = static_cast<uint8_t>(qBound(0.0, pc * 2.55, 255.0));
            pen.m = static_cast<uint8_t>(qBound(0.0, pm * 2.55, 255.0));
            pen.y = static_cast<uint8_t>(qBound(0.0, py * 2.55, 255.0));
            pen.k = static_cast<uint8_t>(qBound(0.0, pk * 2.55, 255.0));
            snapshot.penItems.append(pen);
        }

        // ImageItem with raw CMYK source
        auto *imgItem = dynamic_cast<ImageItem *>(gi);
        if (imgItem && imgItem->isCmykSource()) {
            CmykItemSnapshot::ImageCmykSource src;
            src.sceneRect = gi->sceneBoundingRect();
            src.cmykMat = imgItem->rawCmykPixels();
            snapshot.imageSources.append(src);
        }
    }

    return snapshot;
}

// ========== 线程池入口函数 ==========

ImportWorkerResult runImportWorker(const QString &filePath,
                                    const QSizeF &canvasSize,
                                    bool scaleToFit)
{
    ImportWorkerResult result;
    result.filePath = filePath;

    ImportResult importResult = loadImageFromFile(filePath);
    if (!importResult.isValid()) {
        result.errorMessage = QString("Failed to load image: %1").arg(filePath);
        return result;
    }

    if (scaleToFit && !canvasSize.isEmpty()) {
        int iw = importResult.image.width();
        int ih = importResult.image.height();
        int cw = static_cast<int>(canvasSize.width());
        int ch = static_cast<int>(canvasSize.height());
        if (iw > cw || ih > ch) {
            double scale = qMin(static_cast<double>(cw) / iw,
                                static_cast<double>(ch) / ih);
            importResult.image =
                importResult.image.scaled(qRound(iw * scale), qRound(ih * scale),
                                          Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation);
        }
    }

    result.importResult = std::move(importResult);
    result.success = true;
    return result;
}

ExportWorkerResult runExportWorker(const QString &path,
                                    const QImage &image,
                                    const CmykItemSnapshot &snapshot,
                                    const QRectF &exportRect)
{
    ExportWorkerResult result;
    result.filePath = path;

    bool ok = exportTiffCmykFromSnapshot(path, image, snapshot, exportRect);
    result.success = ok;
    if (!ok)
        result.errorMessage = QString("Failed to export TIFF: %1").arg(path);

    return result;
}

// ========== 线程安全的 CMYK TIFF 导出 ==========

bool exportTiffCmykFromSnapshot(const QString &path, const QImage &image,
                                 const CmykItemSnapshot &snapshot,
                                 const QRectF &exportRect)
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
    TIFF *tif = TIFFOpen(path.toUtf8().constData(), "wl");
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
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, x_dpi);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, y_dpi);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);

    // DateTime metadata
    QString dateTime =
        QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    TIFFSetField(tif, TIFFTAG_DATETIME, dateTime.toUtf8().constData());

    // 4. Embed CMYK ICC profile
    QATColorManager &cm = QATColorManager::instance();
    bool useLcms2 = cm.isValid();
    if (useLcms2) {
        cm.buildRGB2CMYKTransforms(INTENT_PERCEPTUAL,
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

    // 5. Convert and write pixel data row by row
    QVector<uint8_t> rowBuf(width * 4);
    QImage img32 = img.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < height; ++y) {
        const QRgb *scanLine =
            reinterpret_cast<const QRgb *>(img32.constScanLine(y));

        // RGB → CMYK per pixel
        for (int x = 0; x < width; ++x) {
            QRgb c = scanLine[x];
            double cd, md, yd, kd;
            if (useLcms2) {
                QATColorManager::Cmyk cmyk = cm.toCmyk(QColor(c));
                cd = cmyk.c;
                md = cmyk.m;
                yd = cmyk.y;
                kd = cmyk.k;
            } else {
                double r = qRed(c) / 255.0, g = qGreen(c) / 255.0,
                       b = qBlue(c) / 255.0;
                cd = 1.0 - r;
                md = 1.0 - g;
                yd = 1.0 - b;
                kd = qMin(cd, qMin(md, yd));
                cd = (cd - kd) / (1.0 - kd) * 100.0;
                md = (md - kd) / (1.0 - kd) * 100.0;
                yd = (yd - kd) / (1.0 - kd) * 100.0;
                kd *= 100.0;
            }
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

        // Overwrite brush CMYK areas from snapshot
        for (const auto &brush : snapshot.brushItems) {
            QRectF pixelRect((brush.sceneRect.x() - exportRect.x()),
                             (brush.sceneRect.y() - exportRect.y()),
                             brush.sceneRect.width(), brush.sceneRect.height());
            int x0 = qBound(0, static_cast<int>(pixelRect.left()), width - 1);
            int x1 = qBound(0, static_cast<int>(pixelRect.right()), width - 1);
            if (y >= pixelRect.top() && y <= pixelRect.bottom()) {
                for (int x = x0; x <= x1; ++x) {
                    int off = x * 4;
                    rowBuf[off + 0] = brush.c;
                    rowBuf[off + 1] = brush.m;
                    rowBuf[off + 2] = brush.y;
                    rowBuf[off + 3] = brush.k;
                }
            }
        }

        // Overwrite pen CMYK areas from snapshot
        for (const auto &pen : snapshot.penItems) {
            QRectF strokeRect = pen.sceneRect.adjusted(
                -pen.penWidth / 2, -pen.penWidth / 2,
                pen.penWidth / 2, pen.penWidth / 2);
            QRectF pixelRect((strokeRect.x() - exportRect.x()),
                             (strokeRect.y() - exportRect.y()),
                             strokeRect.width(), strokeRect.height());
            if (pen.hasBrush) {
                // Has brush — skip interior, pen handles only stroke edges
                // For simplicity, skip when brush already covers interior
                continue;
            }
            int x0 = qBound(0, static_cast<int>(pixelRect.left()), width - 1);
            int x1 = qBound(0, static_cast<int>(pixelRect.right()), width - 1);
            if (y >= pixelRect.top() && y <= pixelRect.bottom()) {
                for (int x = x0; x <= x1; ++x) {
                    int off = x * 4;
                    rowBuf[off + 0] = pen.c;
                    rowBuf[off + 1] = pen.m;
                    rowBuf[off + 2] = pen.y;
                    rowBuf[off + 3] = pen.k;
                }
            }
        }

        // Overwrite ImageItem CMYK source areas from snapshot
        for (const auto &src : snapshot.imageSources) {
            int srcW = src.cmykMat.width;
            int srcH = src.cmykMat.height;
            QRectF pixelRect((src.sceneRect.x() - exportRect.x()),
                             (src.sceneRect.y() - exportRect.y()),
                             src.sceneRect.width(), src.sceneRect.height());

            if (pixelRect.width() <= 0.0 || pixelRect.height() <= 0.0)
                continue;
            if (y < pixelRect.top() || y > pixelRect.bottom())
                continue;

            int x0 = static_cast<int>(std::ceil(pixelRect.left()));
            int x1 = static_cast<int>(std::ceil(pixelRect.right())) - 1;
            x0 = qBound(0, x0, width - 1);
            x1 = qBound(0, x1, width - 1);
            if (x0 > x1)
                continue;

            double scaleX = static_cast<double>(srcW) / pixelRect.width();
            double scaleY = static_cast<double>(srcH) / pixelRect.height();
            double srcYFloat = (y - pixelRect.top()) * scaleY + 0.5;
            int srcY = qBound(0, static_cast<int>(srcYFloat), srcH - 1);
            const uint8_t *srcRow = src.cmykMat.ptr(srcY);

            for (int x = x0; x <= x1; ++x) {
                double srcXFloat = (x - pixelRect.left()) * scaleX + 0.5;
                int srcX = qBound(0, static_cast<int>(srcXFloat), srcW - 1);
                int srcOff = srcX * 4;
                int dstOff = x * 4;
                rowBuf[dstOff + 0] = srcRow[srcOff + 0];
                rowBuf[dstOff + 1] = srcRow[srcOff + 1];
                rowBuf[dstOff + 2] = srcRow[srcOff + 2];
                rowBuf[dstOff + 3] = srcRow[srcOff + 3];
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

} // namespace ImageUtils

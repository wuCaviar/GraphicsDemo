// #include "mainwindow.h"

// #include <QApplication>

// int main(int argc, char *argv[])
// {
//     QApplication a(argc, argv);
//     MainWindow w;
//     w.show();
//     return QCoreApplication::exec();
// }

// #include <QCoreApplication>
// #include <QImage>
// #include <QPainter>
// #include <QDebug>
// #include <cstdint>

// #include <tiffio.h>

// /**
//  * @brief 生成纯 CMYK 色块并保存为 CMYK TIFF
//  */
// bool saveCmykTiff(const QString &filePath, int width, int height, uint8_t c, uint8_t m, uint8_t y,
//                   uint8_t k)
// {
//     TIFF *tif = TIFFOpen(filePath.toUtf8().constData(), "w");
//     if (!tif) {
//         qWarning() << "Cannot open TIFF file for writing:" << filePath;
//         return false;
//     }

//     // ====== 设置 TIFF 标签 ======
//     TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32_t)width);
//     TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32_t)height);
//     TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4); // CMYK = 4 通道
//     TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); // 每通道 8 位
//     TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED); // CMYK
//     TIFFSetField(tif, TIFFTAG_INKSET, INKSET_CMYK);
//     TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG); // 像素交错
//     TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
//     TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, (uint32_t)height);

//     // 可选：嵌入颜色配置信息
//     TIFFSetField(
//         tif, TIFFTAG_IMAGEDESCRIPTION,
//         qPrintable(QString("CMYK Swatch C=%1 M=%2 Y=%3 K=%4").arg(c).arg(m).arg(y).arg(k)));

//     // ====== 分配行缓冲区 ======
//     tsize_t rowBytes = width * 4; // 每像素 4 字节 (C, M, Y, K)
//     uint8_t *rowBuf = new uint8_t[rowBytes];

//     // ====== 逐行写入像素数据 ======
//     for (int row = 0; row < height; ++row) {
//         for (int col = 0; col < width; ++col) {
//             int idx = col * 4;
//             rowBuf[idx + 0] = c; // Cyan
//             rowBuf[idx + 1] = m; // Magenta
//             rowBuf[idx + 2] = y; // Yellow
//             rowBuf[idx + 3] = k; // Black (Key)
//         }
//         if (TIFFWriteScanline(tif, rowBuf, (uint32_t)row, 0) < 0) {
//             qWarning() << "Error writing scanline" << row;
//             delete[] rowBuf;
//             TIFFClose(tif);
//             return false;
//         }
//     }

//     delete[] rowBuf;
//     TIFFClose(tif);
//     return true;
// }

// /**
//  * @brief 带颜色标注的版本：
//  *        先用 QPainter 在 RGB QImage 上绘制标注文字，
//  *        再将其转换为 CMYK 写入 TIFF
//  */
// bool saveCmykTiffWithAnnotation(const QString &filePath, int width, int height, uint8_t c,
//                                 uint8_t m, uint8_t y, uint8_t k)
// {
//     // --- Step 1: 用 QPainter 绘制带标注的 RGB 图像 ---
//     QImage rgbImage(width, height, QImage::Format_ARGB32);
//     rgbImage.fill(Qt::white);

//     // CMYK → RGB（用于绘制）
//     double C = c / 255.0;
//     double M = m / 255.0;
//     double Y = y / 255.0;
//     double K = k / 255.0;
//     QColor rgbColor(qRound(255.0 * (1.0 - C) * (1.0 - K)), qRound(255.0 * (1.0 - M) * (1.0 - K)),
//                     qRound(255.0 * (1.0 - Y) * (1.0 - K)));

//     QPainter painter(&rgbImage);
//     painter.setRenderHint(QPainter::Antialiasing);

//     // 色块
//     QRect colorRect(30, 30, width - 60, height - 120);
//     painter.fillRect(colorRect, rgbColor);

//     // 边框
//     painter.setPen(QPen(QColor(80, 80, 80), 1));
//     painter.drawRect(colorRect);

//     // 文字
//     painter.setPen(Qt::black);
//     QFont font("Monospace", 16);
//     font.setStyleHint(QFont::Monospace);
//     painter.setFont(font);
//     QString label = QString("C:%1  M:%2  Y:%3  K:%4").arg(c).arg(m).arg(y).arg(k);
//     painter.drawText(QRect(30, height - 80, width - 60, 60), Qt::AlignCenter | Qt::AlignVCenter,
//                      label);
//     painter.end();

//     // --- Step 2: RGB → CMYK 转换并写入 TIFF ---
//     TIFF *tif = TIFFOpen(filePath.toUtf8().constData(), "w");
//     if (!tif)
//         return false;

//     TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32_t)width);
//     TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32_t)height);
//     TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
//     TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
//     TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
//     TIFFSetField(tif, TIFFTAG_INKSET, INKSET_CMYK);
//     TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
//     TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW); // LZW 压缩

//     tsize_t rowBytes = width * 4;
//     uint8_t *rowBuf = new uint8_t[rowBytes];

//     for (int row = 0; row < height; ++row) {
//         const QRgb *scanLine = reinterpret_cast<const QRgb *>(rgbImage.constScanLine(row));
//         for (int col = 0; col < width; ++col) {
//             QRgb px = scanLine[col];
//             int R = qRed(px), G = qGreen(px), B = qBlue(px);

//             // 简单 RGB → CMYK 转换
//             double r = R / 255.0, g = G / 255.0, b = B / 255.0;
//             double kk = 1.0 - qMax(r, qMax(g, b));
//             if (kk >= 1.0) {
//                 rowBuf[col * 4 + 0] = 0;
//                 rowBuf[col * 4 + 1] = 0;
//                 rowBuf[col * 4 + 2] = 0;
//                 rowBuf[col * 4 + 3] = 255;
//             } else {
//                 rowBuf[col * 4 + 0] = qRound(255.0 * (1.0 - r - kk) / (1.0 - kk));
//                 rowBuf[col * 4 + 1] = qRound(255.0 * (1.0 - g - kk) / (1.0 - kk));
//                 rowBuf[col * 4 + 2] = qRound(255.0 * (1.0 - b - kk) / (1.0 - kk));
//                 rowBuf[col * 4 + 3] = qRound(255.0 * kk);
//             }
//         }
//         TIFFWriteScanline(tif, rowBuf, (uint32_t)row, 0);
//     }

//     delete[] rowBuf;
//     TIFFClose(tif);
//     return true;
// }

// // ====== main ======
// int main(int argc, char *argv[])
// {
//     QCoreApplication app(argc, argv);

//     uint8_t c = 173; // 75%  (0~255 对应 0%~100%)
//     uint8_t m = 94; // 10%
//     uint8_t y = 168; // 0%
//     uint8_t k = 61; // 0%

//     // 纯色块（无标注）
//     bool ok1 = saveCmykTiff("pure_swatch.tiff", 400, 400, c, m, y, k);

//     // 带文字标注的色块
//     // bool ok2 = saveCmykTiffWithAnnotation("annotated_swatch.tiff",
//     //                                       400, 400, c, m, y, k);

//     qDebug() << "Pure swatch saved:" << ok1;
//     // qDebug() << "Annotated swatch saved:" << ok2;

//     return 0;
// }


#include <stdio.h>
#include <stdlib.h>
#include <tiffio.h>

int main(int argc, char *argv[]) {

    TIFF *tif = TIFFOpen("C:/Users/12257/Desktop/1357.tif", "r");
    if (!tif) {
        fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 1;
    }

    // 读取必要标签
    uint32_t width, height;
    uint16_t samplesPerPixel, bitsPerSample, photometric, planarConfig, inkSet;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
    TIFFGetField(tif, TIFFTAG_INKSET, &inkSet);

    // 验证是否为 CMYK
    if (photometric != PHOTOMETRIC_SEPARATED || samplesPerPixel != 4 || inkSet != INKSET_CMYK) {
        fprintf(stderr, "Not a standard CMYK TIFF.\n");
        TIFFClose(tif);
        return 1;
    }

    printf("Image: %ux%u, %u-bit CMYK, PlanarConfig=%u\n",
           width, height, bitsPerSample, planarConfig);

    tsize_t scanlineSize = TIFFScanlineSize(tif); // 一行数据的字节数
    tdata_t buf = _TIFFmalloc(scanlineSize);
    if (!buf) {
        fprintf(stderr, "Memory allocation failed.\n");
        TIFFClose(tif);
        return 1;
    }

    if (planarConfig == PLANARCONFIG_CONTIG) { // 1 - 交错存储
        for (uint32_t row = 0; row < height; row++) {
            if (TIFFReadScanline(tif, buf, row, 0) < 0) {
                fprintf(stderr, "Error reading row %u\n", row);
                break;
            }
            // 处理该行: buf 中按 C,M,Y,K 顺序存放每个像素的4个分量
            unsigned char *pixel = (unsigned char *)buf;
            for (uint32_t col = 0; col < width; col++) {
                unsigned char C = pixel[col * 4 + 0];
                unsigned char M = pixel[col * 4 + 1];
                unsigned char Y = pixel[col * 4 + 2];
                unsigned char K = pixel[col * 4 + 3];
                // 在此处使用 CMYK 值
            }
        }
    } else { // planarConfig == PLANARCONFIG_SEPARATE (2) - 独立平面
        // 为每个通道分配一个扫描线缓冲区（尺寸可能相同）
        tdata_t channelBuf = _TIFFmalloc(scanlineSize); // 每个通道一行的大小
        for (uint32_t row = 0; row < height; row++) {
            for (uint16_t sample = 0; sample < 4; sample++) {
                if (TIFFReadScanline(tif, channelBuf, row, sample) < 0) {
                    fprintf(stderr, "Error reading row %u, sample %u\n", row, sample);
                    break;
                }
                // channelBuf 现在是该行该通道的原始数据
                unsigned char *values = (unsigned char *)channelBuf;
                // 可根据需要收集或合并到完整像素中
            }
            // 此处 row 行所有通道数据已就绪，可合并为一个 CMYK 像素数组
        }
        _TIFFfree(channelBuf);
    }

    _TIFFfree(buf);
    TIFFClose(tif);
    return 0;
}



// #include "mainwindow.h"

// #include <QApplication>

// int main(int argc, char *argv[])
// {
//     QApplication a(argc, argv);
//     MainWindow w;
//     w.show();
//     return QCoreApplication::exec();
// }


#include <stdio.h>
#include <stdlib.h>
#include <tiffio.h>
#include <QDebug>

int main(int argc, char *argv[]) {

    TIFF *tif = TIFFOpen("/Volumes/Caviar/Test/1357.tif", "r");
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

    // 验证是否为 CMYK（InkSet 标签可选，缺失时默认为 CMYK）
    int hasInkSet = TIFFGetField(tif, TIFFTAG_INKSET, &inkSet);
    if (photometric != PHOTOMETRIC_SEPARATED || samplesPerPixel != 4
        || (hasInkSet && inkSet != INKSET_CMYK)) {
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



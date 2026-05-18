#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGraphicsScene>
#include <QFileDialog>
#include <QImageReader>
#include <tiffio.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QGraphicsScene *scene = new QGraphicsScene(ui->graphicsView);
    ui->graphicsView->setScene(scene);
    connect(ui->btnOpen, &QPushButton::clicked, this,
            &MainWindow::slotOpenImage);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slotOpenImage()
{
    // 第一步：选择文件
    QString path = QFileDialog::getOpenFileName(
        this, QObject::tr("Import Image"), QString(),
        QObject::tr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp);;"
                    "TIFF (*.tif *.tiff);;"
                    "PNG (*.png);;"
                    "JPEG (*.jpg *.jpeg);;"
                    "BMP (*.bmp);;"
                    "All Files (*)"));
    if (path.isEmpty())
        return;

    ui->lineEdit->setText(path);

    readTiffByQt(path);
    readTiffByLibTiff(path);
}

void MainWindow::readTiffByQt(const QString &path)
{
    QGraphicsScene *scene = ui->graphicsView->scene();
    scene->clear();

    QImageReader reader(path);
    QImage image = reader.read();
    scene->addPixmap(QPixmap::fromImageReader(&reader));
}

#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>

void MainWindow::readTiffByLibTiff(const QString &path)
{
    TIFF *tif = TIFFOpen(path.toStdString().c_str(), "r");
    if (!tif)
        qDebug() << "Cannot open " << path;

    uint32_t width, height;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

    const std::string &filename = path.toStdString();
    std::streampos startOffset = 194;
    std::size_t byteCount = width * height * 4; // CMYK 每像素4字节
    // 以二进制模式打开文件，并设置区域设置避免空格转换
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open file: " + filename);

    // 定位到起始偏移
    file.seekg(startOffset);
    if (!file)
        throw std::runtime_error("Failed to seek to offset "
                                 + std::to_string(startOffset));

    // 读取指定字节数
    std::vector<uint8_t> buffer(byteCount);
    file.read(reinterpret_cast<char *>(buffer.data()), byteCount);

    // 检查实际读取量
    if (file.gcount() != static_cast<std::streamsize>(byteCount))
        throw std::runtime_error("Read failed: expected "
                                 + std::to_string(byteCount) + " bytes, got "
                                 + std::to_string(file.gcount()));

    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            unsigned char C = buffer[(row * width + col) * 4 + 0];
            unsigned char M = buffer[(row * width + col) * 4 + 1];
            unsigned char Y = buffer[(row * width + col) * 4 + 2];
            unsigned char K = buffer[(row * width + col) * 4 + 3];

            int c = static_cast<int>(C);
            int m = static_cast<int>(M);
            int y = static_cast<int>(Y);
            int k = static_cast<int>(K);

            if (c == 0 && m == 0 && y == 0 && k == 0) {
                // 纯白像素
            } else if (c == 255 && m == 255 && y == 255 && k == 255) {
                // 纯黑像素
            } else {
                // 其他颜色
                qDebug() << "Pixel at (" << col << "," << row << "): C=" << c
                         << " M=" << m << " Y=" << y << " K=" << k;
            }
        }
    }

#if 0
    TIFF *tif = TIFFOpen(path.toStdString().c_str(), "r");
    if (!tif)
        qDebug() << "Cannot open " << path;

    uint32_t width, height, rowsPerStrip;
    uint16_t samplesPerPixel, bitsPerSample, photometric, planarConfig,
        compression;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);

    if (compression != COMPRESSION_NONE) {
        fprintf(stderr, "Only uncompressed files can be recovered this way.\n");
    }
    if (!TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip)) {
        // 如果 RowsPerStrip 也缺失，则假设整个图像是一个条带
        rowsPerStrip = height;
    }

    tsize_t scanlineSize = TIFFScanlineSize(tif);
    uint32_t nStrips = TIFFNumberOfStrips(
        tif); // 这个函数不依赖 StripByteCounts，它用 RowsPerStrip 和 height 计算

    // 获取 strip 偏移
    uint64_t *stripOffsets;
    if (!TIFFGetField(tif, TIFFTAG_STRIPOFFSETS, &stripOffsets)) {
        fprintf(stderr, "StripOffsets missing, impossible to recover.\n");
    }

    // 分配一个完整图像的缓冲区
    size_t totalBytes = width * height * 4; // CMYK 4字节
    unsigned char *image = (unsigned char *)malloc(totalBytes);

    for (uint32_t strip = 0; strip < nStrips; strip++) {
        uint32_t startRow = strip * rowsPerStrip;
        uint32_t rowsInStrip = (startRow + rowsPerStrip > height)
                                   ? height - startRow
                                   : rowsPerStrip;
        tsize_t rawCount = rowsInStrip * scanlineSize;

        tdata_t stripBuf = _TIFFmalloc(rawCount);
        TIFFReadRawStrip(tif, strip, stripBuf, rawCount);

        // 复制到图像缓冲区（CONTIG 格式）
        memcpy(image + startRow * width * 4, stripBuf, rawCount);
        _TIFFfree(stripBuf);
    }

    // 此时 image 中已经包含了整个图像的 CMYK 数据，可以根据需要进行处理
    // 读取每个像素的 CMYK 值并进行处理
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            unsigned char C = image[(row * width + col) * 4 + 0];
            unsigned char M = image[(row * width + col) * 4 + 1];
            unsigned char Y = image[(row * width + col) * 4 + 2];
            unsigned char K = image[(row * width + col) * 4 + 3];

            int c = static_cast<int>(C);
            int m = static_cast<int>(M);
            int y = static_cast<int>(Y);
            int k = static_cast<int>(K);

            if (c == 0 && m == 0 && y == 0 && k == 0) {
                // 纯白像素
            } else if (c == 255 && m == 255 && y == 255 && k == 255) {
                // 纯黑像素
            } else {
                // 其他颜色
                qDebug() << "Pixel at (" << col << "," << row << "): C=" << c
                         << " M=" << m << " Y=" << y << " K=" << k;
            }
        }
    }

    free(image);
    TIFFClose(tif);

#endif

#if 0

    // 读取必要标签
    uint32_t width, height;
    uint16_t samplesPerPixel, bitsPerSample, photometric, planarConfig;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);

    // 验证是否为 CMYK
    if (photometric != PHOTOMETRIC_SEPARATED || samplesPerPixel != 4) {
        fprintf(stderr, "Not a standard CMYK TIFF.\n");
        TIFFClose(tif);
    }

    printf("Image: %ux%u, %u-bit CMYK, PlanarConfig=%u\n", width, height,
           bitsPerSample, planarConfig);

    tsize_t scanlineSize = TIFFScanlineSize(tif); // 一行数据的字节数
    tdata_t buf = _TIFFmalloc(scanlineSize);
    if (!buf) {
        fprintf(stderr, "Memory allocation failed.\n");
        TIFFClose(tif);
    }

    if (planarConfig == PLANARCONFIG_CONTIG) { // 1 - 交错存储
        for (uint32_t row = 0; row < height; row++) {
            int size = TIFFReadScanline(tif, buf, row, 0);
            // 处理该行: buf 中按 C,M,Y,K 顺序存放每个像素的4个分量
            unsigned char *pixel = (unsigned char *)buf;
            for (uint32_t col = 0; col < width; col++) {
                unsigned char C = pixel[col * 4 + 0];
                unsigned char M = pixel[col * 4 + 1];
                unsigned char Y = pixel[col * 4 + 2];
                unsigned char K = pixel[col * 4 + 3];
                // 在此处使用 CMYK 值

                // 转换成Int
                int c = static_cast<int>(C);
                int m = static_cast<int>(M);    
                int y = static_cast<int>(Y);
                int k = static_cast<int>(K);

                if (c == 0 && m == 0 && y == 0 && k == 0) {
                    // 纯白像素
                } else if (c == 255 && m == 255 && y == 255 && k == 255) {
                    // 纯黑像素
                } else {
                    // 其他颜色
                    qDebug() << "Pixel at (" << col << "," << row << "): C=" << c
                             << " M=" << m << " Y=" << y << " K=" << k;
                }
            }
        }
    } else { // planarConfig == PLANARCONFIG_SEPARATE (2) - 独立平面
        // 为每个通道分配一个扫描线缓冲区（尺寸可能相同）
        tdata_t channelBuf = _TIFFmalloc(scanlineSize); // 每个通道一行的大小
        for (uint32_t row = 0; row < height; row++) {
            for (uint16_t sample = 0; sample < 4; sample++) {
                TIFFReadScanline(tif, channelBuf, row, sample);
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

#endif
}

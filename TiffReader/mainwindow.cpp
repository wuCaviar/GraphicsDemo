#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGraphicsScene>
#include <QFileDialog>
#include <QImageReader>
#include <tiffio.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>

#include "TiffRawReader.h"

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

    // readTiffByQt(path);
    // readTiffByLibTiff(path);
    readTiffRawData(path);
}

void MainWindow::readTiffByQt(const QString &path)
{
    QGraphicsScene *scene = ui->graphicsView->scene();
    scene->clear();

    QImageReader reader(path);
    QImage image = reader.read();
    scene->addPixmap(QPixmap::fromImageReader(&reader));

    qDebug() << image.format();
}
void MainWindow::readTiffByLibTiff(const QString &path)
{
    QByteArray temp = path.toLocal8Bit();
    TIFF *tif = TIFFOpen(temp.data(), "r");
    if (!tif) {
        qDebug() << "Cannot open " << path;
        return;
    }

    uint32_t width = 0, height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

    QByteArray
        cmykData; // 存放所有像素的 CMYK 原始字节，顺序: C0,M0,Y0,K0, C1,M1,Y1,K1, ...

    // 检查是否有条带字节计数标签（表示图像是按条带组织的）
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
                cmykData.append(static_cast<const char *>(buf), scanlineSize);
            }
        } else { // PLANARCONFIG_SEPARATE：独立平面，需合并四个通道
            tdata_t channelBuf = _TIFFmalloc(scanlineSize);
            if (!channelBuf) {
                _TIFFfree(buf);
                TIFFClose(tif);
                return;
            }

            std::vector<unsigned char> rowPixels(width * 4); // 一行合并后的像素
            for (uint32_t row = 0; row < height; ++row) {
                // 依次读取四个通道到 channelBuf，并填入 rowPixels 对应位置
                for (uint16_t sample = 0; sample < samplesPerPixel; ++sample) {
                    if (TIFFReadScanline(tif, channelBuf, row, sample) < 0) {
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

    TIFFClose(tif);

    // ===== 统一输出：遍历 cmykData，保持原有的输出条件（非纯白、非纯黑） =====
    const int totalPixels = cmykData.size() / 4; // 每个像素 4 字节
    for (int i = 0; i < totalPixels; ++i) {
        int offset = i * 4;
        unsigned char C = cmykData[offset];
        unsigned char M = cmykData[offset + 1];
        unsigned char Y = cmykData[offset + 2];
        unsigned char K = cmykData[offset + 3];

        int c = C, m = M, y = Y, k = K;
        if (c == 0 && m == 0 && y == 0 && k == 0) {
            // 纯白，不输出
        } else if (c == 255 && m == 255 && y == 255 && k == 255) {
            // 纯黑，不输出
        } else {
            uint32_t row = i / width;
            uint32_t col = i % width;
            qDebug() << "Pixel at (" << col << "," << row << "): C=" << c
                     << " M=" << m << " Y=" << y << " K=" << k;
        }
    }
}

void MainWindow::readTiffRawData(const QString &path)
{
    TiffRawReader reader;
    if (!reader.open(path)) {
        qDebug() << "Open failed:" << reader.lastError();
    }

    qDebug() << "Image:" << reader.width() << "x" << reader.height()
             << "samples:" << reader.samplesPerPixel()
             << "bits:" << reader.bitsPerSample();

    QByteArray raw = reader.readRawData();
    if (raw.isEmpty()) {
        qDebug() << "Read failed:" << reader.lastError();
    }

    // raw 现在包含完整的像素数据，可进一步处理
    qDebug() << "Raw data size:" << raw.size() << "bytes";
}

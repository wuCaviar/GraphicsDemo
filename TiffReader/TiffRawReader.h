#ifndef TIFFRAWREADER_H
#define TIFFRAWREADER_H

#include <QByteArray>
#include <QString>
#include <tiffio.h>
#include <memory>

/// cmyk、rgb色域的tiff文件
/// png、jpg、bmp、jpeg等非cmyk文件
/// 要求获取到原始的cmyk色域数据，输出每个像素的cmyk值（0-255）
/// 存储到QByteArray中，格式为C0,M0,Y0,K0, C1,M1,Y1,K1, ...（每个像素4字节，依次是CMYK通道值）

class TiffRawReader
{
public:
    TiffRawReader();
    ~TiffRawReader();

    bool open(const QString &filePath);
    QByteArray readRawData();

    uint32_t width() const;
    uint32_t height() const;
    uint16_t samplesPerPixel() const;
    uint16_t bitsPerSample() const;
    uint16_t photometric() const;
    QString lastError() const;

private:
    bool readTags();
    bool processStrips(QByteArray &data);
    bool processTiles(QByteArray &data);

    void byteswap16IfNeeded(QByteArray &data);
    static void byteswap16Buffer(void *buf, size_t count);

    TIFF *m_tif = nullptr;
    QString m_lastError;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint16_t m_samplesPerPixel = 0;
    uint16_t m_bitsPerSample = 0;
    uint16_t m_photometric = 0;
    uint16_t m_planarConfig = PLANARCONFIG_CONTIG;
    uint16_t m_compression = COMPRESSION_NONE;
    bool m_isTiled = false;
    bool m_swapBytes = false; // 是否需要将16位数据转为本机序
};

#endif // TIFFRAWREADER_H
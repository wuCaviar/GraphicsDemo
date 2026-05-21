#include "tiffrawreader.h"
#include <cstring>
#include <algorithm> // for std::min (optional, we use qMin but ensure it's included)

// 辅助：判断本机字节序 — 现在不再需要，但保留也无害，为求干净此处移除
// 已移除 isLittleEndian()

TiffRawReader::TiffRawReader() = default;

TiffRawReader::~TiffRawReader()
{
    if (m_tif) {
        TIFFClose(m_tif);
        m_tif = nullptr;
    }
}

bool TiffRawReader::open(const QString &filePath)
{
    if (m_tif) {
        TIFFClose(m_tif);
        m_tif = nullptr;
    }

    const QByteArray pathBytes = filePath.toLocal8Bit();
    m_tif = TIFFOpen(pathBytes.constData(), "r");
    if (!m_tif) {
        m_lastError = "Cannot open file: " + filePath;
        return false;
    }

    return readTags();
}

bool TiffRawReader::readTags()
{
    if (!TIFFGetField(m_tif, TIFFTAG_IMAGEWIDTH, &m_width)
        || !TIFFGetField(m_tif, TIFFTAG_IMAGELENGTH, &m_height)
        || !TIFFGetField(m_tif, TIFFTAG_SAMPLESPERPIXEL, &m_samplesPerPixel)
        || !TIFFGetField(m_tif, TIFFTAG_BITSPERSAMPLE, &m_bitsPerSample)
        || !TIFFGetField(m_tif, TIFFTAG_PHOTOMETRIC, &m_photometric)) {
        m_lastError = "Missing required TIFF tags";
        return false;
    }

    // 可选标签
    if (!TIFFGetField(m_tif, TIFFTAG_PLANARCONFIG, &m_planarConfig))
        m_planarConfig = PLANARCONFIG_CONTIG;
    if (!TIFFGetField(m_tif, TIFFTAG_COMPRESSION, &m_compression))
        m_compression = COMPRESSION_NONE;

    // 检查色域（只支持 RGB 和 CMYK）
    if (m_photometric != PHOTOMETRIC_RGB
        && m_photometric != PHOTOMETRIC_SEPARATED) {
        m_lastError =
            QString("Unsupported photometric: %1 (only RGB/Separated)")
                .arg(m_photometric);
        return false;
    }

    // 检查位深（只处理 8/16 位整数）
    if (m_bitsPerSample != 8 && m_bitsPerSample != 16) {
        m_lastError =
            QString("Unsupported bits per sample: %1").arg(m_bitsPerSample);
        return false;
    }

    // 判断是否为瓦片结构
    m_isTiled = TIFFIsTiled(m_tif);

    // ✅ 修正：使用 TIFFIsByteSwapped 判断是否需要交换16位数据
    m_swapBytes = false;
    if (m_bitsPerSample == 16) {
        // TIFFIsByteSwapped 返回非零表示文件字节序与主机不同，需要交换
        if (TIFFIsByteSwapped(m_tif)) {
            m_swapBytes = true;
        }
    }

    return true;
}

QByteArray TiffRawReader::readRawData()
{
    QByteArray data;
    if (!m_tif) {
        m_lastError = "File not opened";
        return data;
    }

    const size_t bytesPerSample = m_bitsPerSample / 8;
    const size_t totalSize = static_cast<size_t>(m_width) * m_height
                             * m_samplesPerPixel * bytesPerSample;
    data.resize(static_cast<int>(totalSize));
    if (data.size() != static_cast<int>(totalSize)) {
        m_lastError = "Memory allocation failed";
        return QByteArray();
    }

    bool ok = false;
    if (m_isTiled)
        ok = processTiles(data);
    else
        ok = processStrips(data);

    if (!ok) {
        data.clear();
        return data;
    }

    // 16位数据的字节序转换
    if (m_swapBytes)
        byteswap16IfNeeded(data);

    return data;
}

// ---------- 条带图像读取 ----------
bool TiffRawReader::processStrips(QByteArray &data)
{
    const size_t bytesPerSample = m_bitsPerSample / 8;
    const size_t rowBytes = m_width * m_samplesPerPixel * bytesPerSample;
    uint8_t *dst = reinterpret_cast<uint8_t *>(data.data());

    if (m_planarConfig == PLANARCONFIG_CONTIG) {
        for (uint32_t y = 0; y < m_height; ++y) {
            if (TIFFReadScanline(m_tif, dst + y * rowBytes, y, 0) < 0) {
                m_lastError = QString("Error reading scanline %1").arg(y);
                return false;
            }
        }
    } else {
        const size_t rowBytesPerSample = m_width * bytesPerSample;
        QByteArray sampleBuf(static_cast<int>(rowBytesPerSample), 0);
        for (uint16_t s = 0; s < m_samplesPerPixel; ++s) {
            for (uint32_t y = 0; y < m_height; ++y) {
                if (TIFFReadScanline(m_tif, sampleBuf.data(), y, s) < 0) {
                    m_lastError =
                        QString("Error reading scanline %1 (sample %2)")
                            .arg(y)
                            .arg(s);
                    return false;
                }
                uint8_t *rowStart = dst + y * rowBytes;
                for (uint32_t x = 0; x < m_width; ++x) {
                    size_t srcOffset = x * bytesPerSample;
                    size_t dstOffset =
                        (x * m_samplesPerPixel + s) * bytesPerSample;
                    memcpy(rowStart + dstOffset,
                           sampleBuf.constData() + srcOffset, bytesPerSample);
                }
            }
        }
    }
    return true;
}

// ---------- 瓦片图像读取 ----------
bool TiffRawReader::processTiles(QByteArray &data)
{
    const size_t bytesPerSample = m_bitsPerSample / 8;
    uint32_t tileWidth = 0, tileHeight = 0;
    TIFFGetField(m_tif, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(m_tif, TIFFTAG_TILELENGTH, &tileHeight);
    if (tileWidth == 0 || tileHeight == 0) {
        m_lastError = "Invalid tile dimensions";
        return false;
    }

    tsize_t tileSize = TIFFTileSize(m_tif);
    if (tileSize <= 0) {
        m_lastError = "Cannot compute tile size";
        return false;
    }
    QByteArray tileBuf(static_cast<int>(tileSize), 0);

    uint8_t *imgPtr = reinterpret_cast<uint8_t *>(data.data());
    const size_t rowBytes = m_width * m_samplesPerPixel * bytesPerSample;

    const uint32_t tilesAcross = (m_width + tileWidth - 1) / tileWidth;
    const uint32_t tilesDown = (m_height + tileHeight - 1) / tileHeight;

    if (m_planarConfig == PLANARCONFIG_CONTIG) {
        for (uint32_t ty = 0; ty < tilesDown; ++ty) {
            for (uint32_t tx = 0; tx < tilesAcross; ++tx) {
                ttile_t tile = TIFFComputeTile(m_tif, tx * tileWidth,
                                               ty * tileHeight, 0, 0);
                tsize_t bytesRead =
                    TIFFReadEncodedTile(m_tif, tile, tileBuf.data(), tileSize);
                if (bytesRead < 0) {
                    m_lastError =
                        QString("Error reading tile (%1,%2)").arg(tx).arg(ty);
                    return false;
                }

                uint32_t copyWidth = qMin(tileWidth, m_width - tx * tileWidth);
                uint32_t copyHeight =
                    qMin(tileHeight, m_height - ty * tileHeight);
                const size_t tileRowBytes =
                    tileWidth * m_samplesPerPixel * bytesPerSample;
                for (uint32_t j = 0; j < copyHeight; ++j) {
                    size_t srcOffset = j * tileRowBytes;
                    size_t dstOffset =
                        ((ty * tileHeight + j) * m_width + tx * tileWidth)
                        * m_samplesPerPixel * bytesPerSample;
                    memcpy(imgPtr + dstOffset, tileBuf.constData() + srcOffset,
                           copyWidth * m_samplesPerPixel * bytesPerSample);
                }
            }
        }
    } else {
        const size_t planeSize =
            static_cast<size_t>(m_width) * m_height * bytesPerSample;
        QByteArray planeData(static_cast<int>(planeSize * m_samplesPerPixel),
                             0);
        uint8_t *planePtr = reinterpret_cast<uint8_t *>(planeData.data());

        for (uint16_t s = 0; s < m_samplesPerPixel; ++s) {
            const size_t planeOffset = s * planeSize;
            for (uint32_t ty = 0; ty < tilesDown; ++ty) {
                for (uint32_t tx = 0; tx < tilesAcross; ++tx) {
                    ttile_t tile = TIFFComputeTile(m_tif, tx * tileWidth,
                                                   ty * tileHeight, 0, s);
                    tsize_t bytesRead = TIFFReadEncodedTile(
                        m_tif, tile, tileBuf.data(), tileSize);
                    if (bytesRead < 0) {
                        m_lastError =
                            QString("Error reading tile (%1,%2) sample %3")
                                .arg(tx)
                                .arg(ty)
                                .arg(s);
                        return false;
                    }

                    uint32_t copyWidth =
                        qMin(tileWidth, m_width - tx * tileWidth);
                    uint32_t copyHeight =
                        qMin(tileHeight, m_height - ty * tileHeight);
                    const size_t tileRowBytes = tileWidth * bytesPerSample;
                    for (uint32_t j = 0; j < copyHeight; ++j) {
                        size_t srcOffset = j * tileRowBytes;
                        uint32_t globalY = ty * tileHeight + j;
                        uint32_t globalX = tx * tileWidth;
                        size_t dstOffset =
                            planeOffset
                            + (globalY * m_width + globalX) * bytesPerSample;
                        memcpy(planePtr + dstOffset,
                               tileBuf.constData() + srcOffset,
                               copyWidth * bytesPerSample);
                    }
                }
            }
        }

        const size_t pixelSize = m_samplesPerPixel * bytesPerSample;
        for (uint32_t y = 0; y < m_height; ++y) {
            uint8_t *rowDst = imgPtr + y * rowBytes;
            for (uint32_t x = 0; x < m_width; ++x) {
                for (uint16_t s = 0; s < m_samplesPerPixel; ++s) {
                    size_t srcIdx =
                        s * planeSize + (y * m_width + x) * bytesPerSample;
                    size_t dstIdx = x * pixelSize + s * bytesPerSample;
                    memcpy(rowDst + dstIdx, planePtr + srcIdx, bytesPerSample);
                }
            }
        }
    }
    return true;
}

void TiffRawReader::byteswap16IfNeeded(QByteArray &data)
{
    if (m_bitsPerSample != 16 || !m_swapBytes)
        return;
    size_t count = static_cast<size_t>(data.size()) / 2;
    byteswap16Buffer(data.data(), count);
}

void TiffRawReader::byteswap16Buffer(void *buf, size_t count)
{
    uint16_t *p = static_cast<uint16_t *>(buf);
    for (size_t i = 0; i < count; ++i) {
        p[i] = (p[i] << 8) | (p[i] >> 8);
    }
}

// ---------- Getter ----------
uint32_t TiffRawReader::width() const
{
    return m_width;
}
uint32_t TiffRawReader::height() const
{
    return m_height;
}
uint16_t TiffRawReader::samplesPerPixel() const
{
    return m_samplesPerPixel;
}
uint16_t TiffRawReader::bitsPerSample() const
{
    return m_bitsPerSample;
}
uint16_t TiffRawReader::photometric() const
{
    return m_photometric;
}
QString TiffRawReader::lastError() const
{
    return m_lastError;
}
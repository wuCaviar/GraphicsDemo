#include "SourceReader.h"
#include "colortransform.h"

#include <QByteArray>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ImageUtils {

bool SourceReader::open(const QString &filePath)
{
    close(); // 确保干净起始

    QByteArray pathBytes = filePath.toLocal8Bit();
    m_tif = ScopedTiffHandle(pathBytes.constData(), "r");
    if (!m_tif) {
        m_error = QString("Cannot open: %1").arg(filePath);
        return false;
    }

    // 读取尺寸
    uint32_t w = 0, h = 0;
    TIFFGetField(m_tif.get(), TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(m_tif.get(), TIFFTAG_IMAGELENGTH, &h);
    if (w == 0 || h == 0 || w > 30000 || h > 300000) {
        m_error = QString("Invalid dimensions: %1x%2").arg(w).arg(h);
        return false;
    }

    // 读取格式属性
    uint16_t bitsPerSample = 8;
    uint16_t sampleFormat = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted(m_tif.get(), TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(m_tif.get(), TIFFTAG_SAMPLESPERPIXEL, &m_samplesPerPixel);
    TIFFGetFieldDefaulted(m_tif.get(), TIFFTAG_PHOTOMETRIC, &m_photometric);
    TIFFGetFieldDefaulted(m_tif.get(), TIFFTAG_PLANARCONFIG, &m_planarConfig);
    TIFFGetFieldDefaulted(m_tif.get(), TIFFTAG_SAMPLEFORMAT, &sampleFormat);

    if (bitsPerSample != 8 || sampleFormat != SAMPLEFORMAT_UINT) {
        m_error = QString("Only 8-bit unsigned integer TIFFs are supported: %1")
                      .arg(filePath);
        return false;
    }

    // 确定颜色类型
    if (m_photometric == PHOTOMETRIC_SEPARATED && m_samplesPerPixel >= 4) {
        m_colorType = ColorType::CMYK;
    } else if (m_photometric == PHOTOMETRIC_RGB && m_samplesPerPixel >= 3) {
        m_colorType = ColorType::RGB;
    } else if ((m_photometric == PHOTOMETRIC_MINISBLACK
                || m_photometric == PHOTOMETRIC_MINISWHITE)
               && m_samplesPerPixel == 1) {
        m_colorType = ColorType::GRAY;
    } else {
        m_error = QString("Unsupported TIFF format (photometric=%1, samples=%2): %3")
                      .arg(m_photometric)
                      .arg(m_samplesPerPixel)
                      .arg(filePath);
        return false;
    }

    m_width = w;
    m_height = h;
    m_isMiniswhite = (m_photometric == PHOTOMETRIC_MINISWHITE);
    m_scanlineSize = TIFFScanlineSize(m_tif.get());

    // 预分配缓存
    try {
        m_rowCache0.resize(static_cast<size_t>(w) * 4);
        m_rowCache1.resize(static_cast<size_t>(w) * 4);
        m_scanBuf.resize(static_cast<size_t>(m_scanlineSize));
    } catch (const std::bad_alloc &) {
        m_error = QString("Not enough memory for %1x%2 source buffer").arg(w).arg(h);
        return false;
    }

    // 对于 RGB 源，创建 LCMS2 变换句柄（线程独立）
    if (m_colorType == ColorType::RGB) {
        QATColorManager &cm = QATColorManager::instance();
        if (cm.isValid()) {
            m_rgbToCmyk = cm.createBgraToCmyk8(
                INTENT_PERCEPTUAL,
                cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);
        }
    }

    // SEPARATE 模式：预分配平面缓冲区
    if (m_planarConfig == PLANARCONFIG_SEPARATE) {
        int effectivePlanes = (m_colorType == ColorType::CMYK) ? 4 : m_samplesPerPixel;
        m_planeBufs.resize(effectivePlanes);
        try {
            for (auto &plane : m_planeBufs)
                plane.resize(static_cast<size_t>(w));  // 每行一个临时平面缓冲区
        } catch (const std::bad_alloc &) {
            m_error = QString("Not enough memory for plane buffers: %1").arg(filePath);
            return false;
        }
    }

    m_valid = true;
    return true;
}

void SourceReader::close()
{
    if (m_rgbToCmyk) {
        cmsDeleteTransform(m_rgbToCmyk);
        m_rgbToCmyk = nullptr;
    }
    m_tif.close();
    m_rowCache0.clear();
    m_rowCache1.clear();
    m_scanBuf.clear();
    m_planeBufs.clear();
    m_valid = false;
    m_cachedRow0 = -1;
    m_cachedRow1 = -1;
}

bool SourceReader::readAndConvertRow(int sourceRow, std::vector<uint8_t> &outCmyk)
{
    if (!m_valid || sourceRow < 0 || sourceRow >= static_cast<int>(m_height)) {
        m_error = QString("Invalid row access: row=%1, height=%2").arg(sourceRow).arg(m_height);
        return false;
    }

    const uint32_t w = m_width;

    if (m_planarConfig == PLANARCONFIG_CONTIG) {
        if (TIFFReadScanline(m_tif.get(), m_scanBuf.data(), static_cast<uint32_t>(sourceRow), 0) < 0) {
            m_error = QString("Read error at row %1").arg(sourceRow);
            return false;
        }

        uint8_t *dst = outCmyk.data();

        switch (m_colorType) {
        case ColorType::CMYK:
            std::memcpy(dst, m_scanBuf.data(), static_cast<size_t>(w) * 4);
            break;

        case ColorType::RGB: {
            // 展开 RGB/RGBA → BGRA
            std::vector<uint8_t> bgraLine(static_cast<size_t>(w) * 4);
            if (m_samplesPerPixel == 3) {
                for (uint32_t x = 0; x < w; ++x) {
                    bgraLine[x * 4 + 0] = m_scanBuf[x * 3 + 2]; // B
                    bgraLine[x * 4 + 1] = m_scanBuf[x * 3 + 1]; // G
                    bgraLine[x * 4 + 2] = m_scanBuf[x * 3 + 0]; // R
                    bgraLine[x * 4 + 3] = 255;
                }
            } else { // m_samplesPerPixel == 4
                for (uint32_t x = 0; x < w; ++x) {
                    bgraLine[x * 4 + 0] = m_scanBuf[x * 4 + 2]; // B
                    bgraLine[x * 4 + 1] = m_scanBuf[x * 4 + 1]; // G
                    bgraLine[x * 4 + 2] = m_scanBuf[x * 4 + 0]; // R
                    bgraLine[x * 4 + 3] = 255;
                }
            }
            if (m_rgbToCmyk) {
                QATColorManager::convertBgra8ToCmyk8(m_rgbToCmyk, bgraLine.data(),
                                                      dst, static_cast<int>(w));
            } else {
                bgraToCmykFallback(bgraLine.data(), dst, static_cast<int>(w));
            }
            break;
        }

        case ColorType::GRAY:
            for (uint32_t x = 0; x < w; ++x) {
                uint8_t gray = m_isMiniswhite
                                   ? static_cast<uint8_t>(255 - m_scanBuf[x])
                                   : m_scanBuf[x];
                int off = static_cast<int>(x) * 4;
                dst[off + 0] = 0;
                dst[off + 1] = 0;
                dst[off + 2] = 0;
                dst[off + 3] = gray;
            }
            break;
        }
    } else {
        // SEPARATE 模式：按平面读取 → interleave
        int effectivePlanes = (m_colorType == ColorType::CMYK) ? 4
                             : static_cast<int>(m_samplesPerPixel);

        for (int s = 0; s < effectivePlanes; ++s) {
            if (TIFFReadScanline(m_tif.get(), m_scanBuf.data(),
                                 static_cast<uint32_t>(sourceRow),
                                 static_cast<uint16_t>(s)) < 0) {
                m_error = QString("Read error at row %1 plane %2")
                              .arg(sourceRow).arg(s);
                return false;
            }
            uint8_t *plane = m_planeBufs[s].data();
            std::memcpy(plane, m_scanBuf.data(), static_cast<size_t>(w));
        }

        uint8_t *dst = outCmyk.data();
        switch (m_colorType) {
        case ColorType::CMYK:
            for (uint32_t x = 0; x < w; ++x) {
                int off = static_cast<int>(x) * 4;
                dst[off + 0] = m_planeBufs[0][x];
                dst[off + 1] = m_planeBufs[1][x];
                dst[off + 2] = m_planeBufs[2][x];
                dst[off + 3] = m_planeBufs[3][x];
            }
            break;

        case ColorType::RGB: {
            std::vector<uint8_t> bgraLine(static_cast<size_t>(w) * 4);
            for (uint32_t x = 0; x < w; ++x) {
                bgraLine[x * 4 + 0] = m_planeBufs[2][x];
                bgraLine[x * 4 + 1] = m_planeBufs[1][x];
                bgraLine[x * 4 + 2] = m_planeBufs[0][x];
                bgraLine[x * 4 + 3] = 255;
            }
            if (m_rgbToCmyk) {
                QATColorManager::convertBgra8ToCmyk8(m_rgbToCmyk, bgraLine.data(),
                                                      dst, static_cast<int>(w));
            } else {
                bgraToCmykFallback(bgraLine.data(), dst, static_cast<int>(w));
            }
            break;
        }

        case ColorType::GRAY:
            for (uint32_t x = 0; x < w; ++x) {
                uint8_t gray = m_isMiniswhite
                                   ? static_cast<uint8_t>(255 - m_planeBufs[0][x])
                                   : m_planeBufs[0][x];
                int off = static_cast<int>(x) * 4;
                dst[off + 0] = 0;
                dst[off + 1] = 0;
                dst[off + 2] = 0;
                dst[off + 3] = gray;
            }
            break;
        }
    }

    return true;
}

SourceReader::RowWindow SourceReader::getRowWindow(double srcY)
{
    RowWindow window;

    if (!m_valid)
        return window;

    int iy0 = static_cast<int>(std::floor(srcY));
    int iy1 = iy0 + 1;
    iy0 = std::clamp(iy0, 0, static_cast<int>(m_height) - 1);
    iy1 = std::clamp(iy1, 0, static_cast<int>(m_height) - 1);

    if (iy0 == iy1) {
        // 边界情况：不需要 Y 方向插值，两行相同
        iy1 = iy0;
    }

    // 滑动窗口优化：利用输出行号单调递增的性质
    if (m_cachedRow0 == iy0) {
        // row0 命中缓存，只需加载 row1（如果与 row0 不同）
        if (iy1 != iy0 && m_cachedRow1 != iy1) {
            if (!readAndConvertRow(iy1, m_rowCache1)) {
                m_error.clear(); // 行级错误不阻止继续
            }
            m_cachedRow1 = iy1;
        }
    } else if (m_cachedRow1 == iy0) {
        // 旧 row1 变成新 row0：向前滑动一行
        std::swap(m_rowCache0, m_rowCache1);
        m_cachedRow0 = m_cachedRow1;
        if (iy1 != iy0) {
            if (!readAndConvertRow(iy1, m_rowCache1)) {
                m_error.clear();
            }
            m_cachedRow1 = iy1;
        } else {
            m_cachedRow1 = m_cachedRow0;
        }
    } else {
        // 跳变（strip 边界或第一个 strip）：重新加载两行
        if (!readAndConvertRow(iy0, m_rowCache0)) {
            m_error.clear();
        }
        m_cachedRow0 = iy0;
        if (iy1 != iy0) {
            if (!readAndConvertRow(iy1, m_rowCache1)) {
                m_error.clear();
            }
            m_cachedRow1 = iy1;
        } else {
            m_cachedRow1 = iy0;
            m_rowCache1 = m_rowCache0;
        }
    }

    window.row0 = m_rowCache0.data();
    window.row1 = (iy0 == iy1) ? m_rowCache0.data() : m_rowCache1.data();

    return window;
}

} // namespace ImageUtils

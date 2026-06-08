#ifndef SOURCEREADER_H
#define SOURCEREADER_H

#include "ScopedTiffHandle.h"
#include "ImageWorker.h"

#include <QString>
#include <cstdint>
#include <vector>
#include <lcms2.h>

namespace ImageUtils {

// ============================================================================
// SourceReader — 源 TIFF 文件的滑动窗口读取器
//
// 设计要点：
// - 每个实例独立拥有 TIFF 句柄和 LCMS2 cmsHTRANSFORM，可在任意线程使用
// - 维护 2 行 CMYK 缓存（滑动窗口），支持双线性插值的行访问
// - 利用输出行号单调递增的性质，最大化滑动窗口缓存命中
// - 支持 CONTIG 和 SEPARATE 两种 TIFF planar 配置
// - 内存占用: 2 × sourceWidth × 4 bytes + scanlineSize (通常 < 1 MB)
// ============================================================================

class SourceReader
{
public:
    // 源图像的颜色类型
    enum class ColorType
    {
        CMYK,
        RGB,
        GRAY
    };

    // 双线性插值所需的源行窗口
    struct RowWindow
    {
        const uint8_t *row0 = nullptr; // 指向内部缓存的 CMYK 行 (width × 4 bytes)
        const uint8_t *row1 = nullptr; // 同上。row0 == row1 表示无需 Y 方向插值
    };

    SourceReader() = default;

    // 移动语义（内部缓存使用 std::vector，自动支持）
    SourceReader(SourceReader &&) noexcept = default;
    SourceReader &operator=(SourceReader &&) noexcept = default;

    ~SourceReader() { close(); }

    // ---- 生命周期 ----

    // 打开源 TIFF 文件，读取尺寸和颜色类型信息
    // 成功返回 true，失败返回 false 且可通过 errorString() 获取错误信息
    bool open(const QString &filePath);

    // 关闭文件，释放所有资源
    void close();

    // ---- 查询 ----

    bool isValid() const { return m_valid; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    ColorType colorType() const { return m_colorType; }
    QString errorString() const { return m_error; }

    // ---- 核心接口 ----

    // 获取源图像中 srcY 行对应的双线性插值窗口
    // srcY 为浮点源行坐标（对应输出坐标通过 scaleY 映射后的值）
    // 返回两行 CMYK 数据指针，row0 = floor(srcY), row1 = ceil(srcY)
    // 调用方无需管理返回指针的生命周期（指向内部缓存）
    // 前提：调用方按单调递增顺序调用（输出行从上到下推进）
    RowWindow getRowWindow(double srcY);

    // 读取源 TIFF 的一行原始数据，转换为 CMYK 存储到 outCmyk
    // 返回值: true 成功, false 失败。
    // 用于需要逐行控制读取的场景（如 StripPipeline producer）
    bool readAndConvertRow(int sourceRow, std::vector<uint8_t> &outCmyk);

private:
    // ---- 数据成员 ----

    ScopedTiffHandle m_tif;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    ColorType m_colorType = ColorType::GRAY;

    // 源 TIFF 格式属性
    uint16_t m_samplesPerPixel = 1;
    uint16_t m_photometric = PHOTOMETRIC_MINISWHITE;
    uint16_t m_planarConfig = PLANARCONFIG_CONTIG;
    tsize_t m_scanlineSize = 0;
    bool m_isMiniswhite = false;

    // 滑动窗口：两行 CMYK 缓存 (每行 m_width × 4 bytes)
    std::vector<uint8_t> m_rowCache0;
    std::vector<uint8_t> m_rowCache1;
    int m_cachedRow0 = -1; // 缓存在 rowCache0 中的源行号 (-1 表示无效)
    int m_cachedRow1 = -1; // 缓存在 rowCache1 中的源行号

    // 原始 TIFF 扫描线读取缓冲区 (m_scanlineSize bytes)
    std::vector<uint8_t> m_scanBuf;

    // SEPARATE 模式下读取各平面时用到的临时缓冲区
    std::vector<std::vector<uint8_t>> m_planeBufs;

    // LCMS2 RGB→CMYK 变换句柄（仅 RGB 源时有效，线程独立）
    cmsHTRANSFORM m_rgbToCmyk = nullptr;

    bool m_valid = false;
    QString m_error;
};

} // namespace ImageUtils

#endif // SOURCEREADER_H

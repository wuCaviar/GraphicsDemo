#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QByteArray>
#include <QGraphicsItem>
#include <QImage>
#include <QMap>
#include <QVariant>
#include <QString>

#include <opencv2/opencv.hpp>

namespace ImageUtils {

// 导入参数
struct ImportParameters {
    // DPI 设置
    enum class DpiPolicy {
        UseImageDpi,    // 使用图像自带的 DPI
        ForceDpi,       // 强制使用指定的 DPI
        IgnoreDpi       // 忽略 DPI（作为 72 DPI 处理）
    };
    DpiPolicy dpiPolicy = DpiPolicy::UseImageDpi;
    int forcedDpi = 300;  // 当 dpiPolicy = ForceDpi 时有效

    // 颜色空间
    enum class ColorSpace {
        KeepOriginal,    // 保持原始颜色空间
        ConvertToSRGB,   // 转换为 sRGB
        ConvertToAdobeRGB // 转换为 Adobe RGB
    };
    ColorSpace colorSpace = ColorSpace::KeepOriginal;

    // Alpha 通道处理
    enum class AlphaHandling {
        Keep,            // 保持 Alpha 通道
        Discard,         // 丢弃 Alpha 通道（替换为白色）
        Premultiply      // 预乘 Alpha
    };
    AlphaHandling alphaHandling = AlphaHandling::Keep;

    // 图像缩放
    bool enableScaling = false;
    qreal scaleFactor = 1.0;  // 缩放因子（1.0 = 原始大小）

    // TIFF 特定选项
    bool preserveTiffMetadata = true;  // 保留 TIFF 元数据
};

// 导出参数
struct ExportParameters {
    // ===== 通用参数 =====
    int dpi = 300;

    enum class ColorSpace {
        KeepOriginal,
        ConvertToSRGB,
        ConvertToAdobeRGB,
        ConvertToCMYK
    };
    ColorSpace colorSpace = ColorSpace::KeepOriginal;

    enum class TransparencyHandling {
        Keep,
        FlattenOnWhite
    };
    TransparencyHandling transparency = TransparencyHandling::Keep;

    // ===== 格式通用枚举 =====
    enum class CompressionType {
        None,
        LZW,
        ZIP,
        JPEG,
        PackBits
    };

    enum class ByteOrder { LittleEndian, BigEndian };
    enum class BitDepth { Bits8, Bits16 };
    enum class PlanarConfig { Contig, Separate };
    enum class Predictor { None, Horizontal };

    // ===== TIFF 专业参数 =====
    struct TiffOptions {
        CompressionType compression = CompressionType::None;
        ByteOrder byteOrder = ByteOrder::LittleEndian;
        BitDepth bitDepth = BitDepth::Bits8;
        PlanarConfig planarConfig = PlanarConfig::Contig;
        Predictor predictor = Predictor::None;
        bool embedICCProfile = true;
        bool preserveMetadata = true;
        int jpegQuality = 95;  // 1-100, 仅 JPEG 压缩时有效
    };
    TiffOptions tiff;

    // ===== PNG 参数（预留扩展） =====
    struct PngOptions {
        int compressionLevel = 6;  // 0-9
    };
    PngOptions png;

    // ===== JPEG 参数（预留扩展） =====
    struct JpegOptions {
        int quality = 95;  // 1-100
    };
    JpegOptions jpeg;
};

// 判断文件路径是否为 TIFF 格式
bool isTiffFile(const QString &path);

// 图像导入结果
struct ImportResult {
    QImage image;              // 解码后的图像
    cv::Mat rawTiffMat;    // 原始 TIFF 数据（仅 TIFF 文件有值）
    QString filePath;          // 文件路径
    QMap<QString, QVariant> metadata; // 元数据（EXIF、XMP等）
    int dpiX = 72;            // 水平 DPI
    int dpiY = 72;            // 垂直 DPI
    bool isValid() const { return !image.isNull(); }

    // CMYK 源数据（仅 CMYK TIFF 导入时有值）
    cv::Mat rawCmykMat;  // 原始 CMYK 像素（4 bytes/pixel, 0-255/通道, libtiff 顺序）
    bool isCmykSource = false;
};

// 使用 libtiff 导入 TIFF 图像（支持 RGBA、CMYK、安全检查）
QImage importTiffWithLibtiff(const QString &path, const ImportParameters &params = ImportParameters(),
                             ImportResult *result = nullptr);

// 从文件加载图像（自动识别 TIFF 与普通格式，保留 TIFF 原始数据）
ImportResult loadImageFromFile(const QString &path, const ImportParameters &params = ImportParameters());

// 弹出文件对话框并加载所选图像
ImportResult importImageWithDialog(QWidget *parent, ImportParameters *params = nullptr);

// 使用 libtiff 导出 TIFF（支持压缩参数、DPI、元数据）
bool exportTiffLossless(const QString &path, const QImage &image,
                        const ExportParameters &params = ExportParameters());

// 导出 CMYK TIFF：RGB 图像逐像素转换 CMYK，并用图元存储的精确 CMYK 覆写纯色区域
bool exportTiffCmyk(const QString &path, const QImage &image,
                    const QList<QGraphicsItem *> &items, const QRectF &exportRect,
                    const ExportParameters &params = ExportParameters());

// 使用 QImageWriter 导出 PNG/JPEG 等格式（支持压缩参数）
bool exportImageWithParams(const QString &path, const QImage &image,
                           const ExportParameters &params);

} // namespace ImageUtils

#endif // IMAGEUTILS_H

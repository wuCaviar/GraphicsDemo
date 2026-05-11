#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QByteArray>
#include <QImage>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QImageWriter>

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
    // 分辨率/DPI 设置
    int dpi = 300;  // 导出图像的分辨率

    // 压缩设置
    enum class CompressionType {
        None,    // 无压缩
        LZW,      // LZW 压缩（TIFF/PNG）
        ZIP,      // ZIP/DEFLATE 压缩（TIFF/PNG）
        JPEG     // JPEG 压缩（TIFF）
    };
    CompressionType compression = CompressionType::LZW;

    // JPEG 质量（仅 JPEG 压缩）
    int jpegQuality = 95;  // 1-100

    // PNG 压缩级别（仅 PNG）
    int pngCompression = 6;  // 0-9

    // 颜色空间
    enum class ColorSpace {
        KeepOriginal,    // 保持原始颜色空间
        ConvertToSRGB,   // 转换为 sRGB
        ConvertToAdobeRGB // 转换为 Adobe RGB
    };
    ColorSpace colorSpace = ColorSpace::KeepOriginal;

    // 元数据处理
    bool preserveMetadata = true;   // 保留元数据（EXIF、ICC 等）
    bool preserveICCProfile = true;  // 保留 ICC 配置文件

    // 透明度处理
    enum class TransparencyHandling {
        Keep,            // 保持透明度
        FlattenOnWhite  // 扁平化为白色背景
    };
    TransparencyHandling transparency = TransparencyHandling::Keep;

    // TIFF 特定选项
    bool tiffZipHorizontalDifferencing = false;  // ZIP 压缩时启用水平差分（减小文件大小）
};

// 判断文件路径是否为 TIFF 格式
bool isTiffFile(const QString &path);

// 使用 libtiff 导入 TIFF 图像（支持 RGBA、安全检查）
QImage importTiffWithLibtiff(const QString &path, const ImportParameters &params = ImportParameters());

// 图像导入结果
struct ImportResult {
    QImage image;              // 解码后的图像
    QByteArray rawTiffData;    // 原始 TIFF 数据（仅 TIFF 文件有值）
    QString filePath;          // 文件路径
    QMap<QString, QVariant> metadata; // 元数据（EXIF、XMP等）
    int dpiX = 72;            // 水平 DPI
    int dpiY = 72;            // 垂直 DPI
    bool isValid() const { return !image.isNull(); }
};

// 从文件加载图像（自动识别 TIFF 与普通格式，保留 TIFF 原始数据）
ImportResult loadImageFromFile(const QString &path, const ImportParameters &params = ImportParameters());

// 弹出文件对话框并加载所选图像
ImportResult importImageWithDialog(QWidget *parent, ImportParameters *params = nullptr);

// 应用导出参数到 QImageWriter
bool applyExportParameters(QImageWriter &writer, const ExportParameters &params);

// 将 ImportResult 的 DPI 信息应用到图像（用于导出）
void applyDpiToImage(QImage &image, const ImportResult &result, const ExportParameters &params);

// 使用 libtiff 导出 TIFF（支持压缩参数、DPI、元数据）
bool exportTiffLossless(const QString &path, const QImage &image,
                        const ExportParameters &params = ExportParameters());

// 使用 QImageWriter 导出 PNG/JPEG 等格式（支持压缩参数）
bool exportImageWithParams(const QString &path, const QImage &image,
                           const ExportParameters &params);

} // namespace ImageUtils

#endif // IMAGEUTILS_H

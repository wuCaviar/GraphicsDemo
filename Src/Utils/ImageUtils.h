#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QByteArray>
#include <QGraphicsItem>
#include <cstdint>
#include <QImage>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QByteArray>

static int x_dpi = 72;
static int y_dpi = 72;

namespace ImageUtils {

// NOTE:导入导出不在需要设置参数，按照默认要求参数即可。

// 4-channel 8-bit raw pixel buffer, replaces cv::Mat (CV_8UC4)
struct RawPixelBuffer
{
    int width = 0;
    int height = 0;
    QByteArray data;

    bool isEmpty() const { return data.isEmpty(); }
    uint8_t *ptr(int y)
    {
        return reinterpret_cast<uint8_t *>(data.data()) + y * width * 4;
    }
    const uint8_t *ptr(int y) const
    {
        return reinterpret_cast<const uint8_t *>(data.data()) + y * width * 4;
    }
};

// 图像导入结果
struct ImportResult
{
    QImage image; // 通过QImageReader读取的用于显示的图片
    RawPixelBuffer rawCmykMat; // 读取/转换到的cmyk原始数据值(0-100.0)
    QString filePath; // 文件路径
    QMap<QString, QVariant> metadata; // 元数据（EXIF、XMP等）
    int dpiX = 72; // 水平 DPI
    int dpiY = 72; // 垂直 DPI
    bool isValid() const { return !image.isNull(); }
};

// 判断文件路径是否为 TIFF 格式
bool isTiffFile(const QString &path);

// NOTE:导入流程
/*
    选择文件路径，判断文件名后缀，libTiff\QImageReader读取
    保存导入数据
*/

// 使用 libtiff 、QImageReader导入图像（支持 RGBA、CMYK、安全检查）
void importTiffWithLibtiff(const QString &path, ImportResult *result = nullptr);

// 从文件加载图像（自动识别 TIFF 与普通格式，保留 TIFF 原始数据）
ImportResult loadImageFromFile(const QString &path);

// 弹出文件对话框并加载所选图像，对比画布尺寸询问是否缩放适配
ImportResult importImageWithDialog(QWidget *parent,
                                   const QSizeF &canvasSize = QSizeF());

// 使用 libtiff 导出 TIFF（默认 LZW 压缩、300 DPI）
bool exportTiffLossless(const QString &path, const QImage &image);

// 导出 CMYK TIFF：RGB 图像逐像素转换 CMYK，并用图元存储的精确 CMYK 覆写纯色区域
bool exportTiffCmyk(const QString &path, const QImage &image,
                    const QList<QGraphicsItem *> &items,
                    const QRectF &exportRect);

} // namespace ImageUtils

#endif // IMAGEUTILS_H

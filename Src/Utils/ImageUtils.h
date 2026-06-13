#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QImage>
#include <QString>

namespace ImageUtils {

// 判断文件路径是否为 TIFF 格式
bool isTiffFile(const QString &path);

// 使用 libtiff 解码 TIFF 为显示用 QImage，可选返回 DPI 和 CMYK 标志
[[deprecated("使用QImageReader替换.")]]
QImage loadTiffImage(const QString &path, QPair<int, int> *dpi = nullptr, bool *isCmyk = nullptr);

// 轻量级读取 TIFF DPI 标签（不解码图像数据）
QPair<int, int> readTiffDpi(const QString &path);

QSize readTiffSize(const QString &path);

// 生成屏幕显示用缩略图（缓存感知，自适应目标长边）
// targetLongEdge 默认 800px：长边 ≤ 800 → 不缩放；> 800 → 按比例缩放
// 首次调用从原图解码并写入磁盘缓存；后续调用直接从缓存加载
QImage generateDisplayThumbnail(const QString &filePath, int targetLongEdge = 800);

} // namespace ImageUtils

#endif // IMAGEUTILS_H

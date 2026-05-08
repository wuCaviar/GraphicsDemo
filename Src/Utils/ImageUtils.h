#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QByteArray>
#include <QImage>
#include <QString>

namespace ImageUtils {

// 判断文件路径是否为 TIFF 格式
bool isTiffFile(const QString &path);

// 使用 libtiff 导入 TIFF 图像（支持 RGBA、安全检查）
QImage importTiffWithLibtiff(const QString &path);

// 图像导入结果
struct ImportResult {
    QImage image;              // 解码后的图像
    QByteArray rawTiffData;    // 原始 TIFF 数据（仅 TIFF 文件有值）
    QString filePath;          // 文件路径
    bool isValid() const { return !image.isNull(); }
};

// 从文件加载图像（自动识别 TIFF 与普通格式，保留 TIFF 原始数据）
ImportResult loadImageFromFile(const QString &path);

// 弹出文件对话框并加载所选图像
ImportResult importImageWithDialog(QWidget *parent);

} // namespace ImageUtils

#endif // IMAGEUTILS_H

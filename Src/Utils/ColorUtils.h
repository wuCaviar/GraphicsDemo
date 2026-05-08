#ifndef COLORUTILS_H
#define COLORUTILS_H

#include <QColor>
#include <QString>

class QPushButton;
class QWidget;

namespace ColorUtils {

// 打开颜色选择对话框，返回用户选择的颜色（取消返回 invalid）
QColor pickColor(QWidget *parent, const QColor &initial, const QString &title);

// 更新按钮背景色样式（显示颜色预览）
// transparent 标记是否在没有颜色时显示透明边框样式
void updateColorButton(QPushButton *btn, const QColor &color, bool transparent = false);

} // namespace ColorUtils

#endif // COLORUTILS_H

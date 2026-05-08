#include "ColorUtils.h"

#include <QPushButton>
#include <QColorDialog>

namespace ColorUtils {

QColor pickColor(QWidget *parent, const QColor &initial, const QString &title)
{
    QColorDialog dialog(initial, parent);
    dialog.setWindowTitle(title);
    dialog.setOption(QColorDialog::ShowAlphaChannel);
    if (dialog.exec() == QDialog::Accepted)
        return dialog.currentColor();
    return initial;
}

void updateColorButton(QPushButton *btn, const QColor &color, bool transparent)
{
    if (!btn)
        return;

    if (transparent && !color.isValid()) {
        btn->setStyleSheet(QStringLiteral("background-color: transparent; border: 1px solid gray;"));
    } else {
        btn->setStyleSheet(
                QStringLiteral("background-color: %1").arg(color.isValid() ? color.name() : "transparent"));
    }
}

} // namespace ColorUtils

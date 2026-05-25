#include "ColorUtils.h"

#include <QConicalGradient>
#include <QPushButton>
#include <QColorDialog>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QtGlobal>

namespace ColorUtils {

namespace {
QPointF mapPointToRect(const QPointF &point, const QRectF &rect)
{
    return QPointF(rect.left() + point.x() * rect.width(),
                   rect.top() + point.y() * rect.height());
}

void copyGradientProperties(QGradient *target, const QGradient *source)
{
    target->setStops(source->stops());
    target->setSpread(source->spread());
    target->setCoordinateMode(QGradient::LogicalMode);
}
} // namespace

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

QBrush mapGradientBrushToRect(const QBrush &brush, const QRectF &rect)
{
    const QGradient *gradient = brush.gradient();
    if (!gradient || !rect.isValid() || qFuzzyIsNull(rect.width()) || qFuzzyIsNull(rect.height()))
        return brush;

    QBrush mappedBrush;
    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        const auto *linear = static_cast<const QLinearGradient *>(gradient);
        QLinearGradient mapped(mapPointToRect(linear->start(), rect),
                               mapPointToRect(linear->finalStop(), rect));
        copyGradientProperties(&mapped, gradient);
        mappedBrush = QBrush(mapped);
        break;
    }
    case QGradient::RadialGradient: {
        const auto *radial = static_cast<const QRadialGradient *>(gradient);
        const qreal radius = qMax(radial->radius() * qMax(qAbs(rect.width()), qAbs(rect.height())), 0.001);
        QRadialGradient mapped(mapPointToRect(radial->center(), rect),
                               radius,
                               mapPointToRect(radial->focalPoint(), rect));
        copyGradientProperties(&mapped, gradient);
        mappedBrush = QBrush(mapped);
        break;
    }
    case QGradient::ConicalGradient: {
        const auto *conical = static_cast<const QConicalGradient *>(gradient);
        QConicalGradient mapped(mapPointToRect(conical->center(), rect), conical->angle());
        copyGradientProperties(&mapped, gradient);
        mappedBrush = QBrush(mapped);
        break;
    }
    default:
        return brush;
    }

    mappedBrush.setTransform(brush.transform());
    return mappedBrush;
}

} // namespace ColorUtils

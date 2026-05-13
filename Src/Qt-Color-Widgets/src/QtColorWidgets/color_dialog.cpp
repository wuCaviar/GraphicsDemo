/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 * SPDX-FileCopyrightText: 2014 Calle Laakkonen
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "QtColorWidgets/color_dialog.hpp"
#include "ui_color_dialog.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPushButton>
#include <QScreen>

#include "QtColorWidgets/color_utils.hpp"

#include "../Src/ColorTrans/colortransform.h"

namespace color_widgets {

/// @brief Convert CMYK values to QColor (RGB)
/// @param c Cyan (0-100)
/// @param m Magenta (0-100)
/// @param y Yellow (0-100)
/// @param k Black (0-100)
/// @return QColor in RGB space
QColor ColorDialog::cmyk_to_rgb(double c, double m, double y, double k)
{
    QATColorManager &cm = QATColorManager::instance();
    if (cm.isValid()) {
        cm.buildCMYK2RGBTransforms(INTENT_PERCEPTUAL,
                                   cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_LOWRESPRECALC);

        QColor cmyk = cm.toRgb(QATColorManager::Cmyk{c, m, y, k});
        return cmyk;
    }

    return QColor();
}

/// @brief Convert QColor (RGB) to CMYK values
/// @param color QColor in RGB space
/// @param c Output Cyan (0-100)
/// @param m Output Magenta (0-100)
/// @param y Output Yellow (0-100)
/// @param k Output Black (0-100)
void ColorDialog::rgb_to_cmyk(const QColor &color, double &c, double &m, double &y, double &k)
{
    QATColorManager &cm = QATColorManager::instance();
    if (cm.isValid()) {
        cm.buildRGB2CMYKTransforms(INTENT_PERCEPTUAL,
                                   cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);

        QATColorManager::Cmyk cmyk = cm.toCmyk(color);

        c = cmyk.c;
        m = cmyk.m;
        y = cmyk.y;
        k = cmyk.k;
    }
}

class ColorDialog::Private
{
public:
    Ui_ColorDialog ui;
    ButtonMode button_mode;
    bool pick_from_screen;
    bool alpha_enabled;
    QColor color;
    CmykColor pendingCmyk;  // CMYK 来源值，用于导出精确 CMYK

    Private() : pick_from_screen(false), alpha_enabled(true)
    {}

#ifdef Q_OS_ANDROID
    void screen_rotation()
    {
        auto scr = QApplication::primaryScreen();
        if (scr->size().height() > scr->size().width())
            ui.layout_main->setDirection(QBoxLayout::TopToBottom);
        else
            ui.layout_main->setDirection(QBoxLayout::LeftToRight);
    }
#endif
};

ColorDialog::ColorDialog(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), p(new Private)
{
    p->ui.setupUi(this);

    setAcceptDrops(true);

#ifdef Q_OS_ANDROID
    connect(QGuiApplication::primaryScreen(), &QScreen::primaryOrientationChanged, this,
            [this] { p->screen_rotation(); });
    p->ui.wheel->setWheelWidth(50);
    p->screen_rotation();
#else
    // Add "pick color" button
    QPushButton *pickButton = p->ui.buttonBox->addButton(tr("Pick"), QDialogButtonBox::ActionRole);
    pickButton->setIcon(QIcon::fromTheme(QStringLiteral("color-picker")));
#endif

    setButtonMode(OkApplyCancel);

    connect(p->ui.wheel, &ColorWheel::colorSpaceChanged, this, &ColorDialog::colorSpaceChanged);
    connect(p->ui.wheel, &ColorWheel::selectorShapeChanged, this, &ColorDialog::wheelShapeChanged);
    connect(p->ui.wheel, &ColorWheel::rotatingSelectorChanged, this,
            &ColorDialog::wheelRotatingChanged);
    connect(p->ui.wheel, &ColorWheel::mirroredSelectorChanged, this,
            &ColorDialog::wheelMirroredChanged);

    connect(p->ui.spin_cyan, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorDialog::set_cmyk);
    connect(p->ui.spin_magenta, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorDialog::set_cmyk);
    connect(p->ui.spin_yellow, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorDialog::set_cmyk);
    connect(p->ui.spin_black, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorDialog::set_cmyk);

    QATColorManager &cm = QATColorManager::instance();
    cm.initialize();
}

ColorDialog::~ColorDialog()
{
    delete p;
}

QSize ColorDialog::sizeHint() const
{
    return QSize(400, 0);
}

QColor ColorDialog::color() const
{
    QColor col = p->color;
    if (!p->alpha_enabled)
        col.setAlpha(255);
    return col;
}

void ColorDialog::setColor(const QColor &c)
{
    p->ui.preview->setComparisonColor(c);
    p->ui.edit_hex->setModified(false);
    setColorInternal(c);
}

void ColorDialog::showColor(const QColor &c)
{
    setColor(c);
    show();
}

void ColorDialog::setPreviewDisplayMode(ColorPreview::DisplayMode mode)
{
    p->ui.preview->setDisplayMode(mode);
}

ColorPreview::DisplayMode ColorDialog::previewDisplayMode() const
{
    return p->ui.preview->displayMode();
}

void ColorDialog::setAlphaEnabled(bool a)
{
    if (a != p->alpha_enabled) {
        p->alpha_enabled = a;

        p->ui.edit_hex->setShowAlpha(a);
        p->ui.line_alpha->setVisible(a);
        p->ui.label_alpha->setVisible(a);
        p->ui.slide_alpha->setVisible(a);
        p->ui.spin_alpha->setVisible(a);

        Q_EMIT alphaEnabledChanged(a);
    }
}

bool ColorDialog::alphaEnabled() const
{
    return p->alpha_enabled;
}

void ColorDialog::setButtonMode(ButtonMode mode)
{
    p->button_mode = mode;
    QDialogButtonBox::StandardButtons btns;
    switch (mode) {
    case OkCancel:
        btns = QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
        break;
    case OkApplyCancel:
        btns = QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply |
               QDialogButtonBox::Reset;
        break;
    case Close:
        btns = QDialogButtonBox::Close;
    }
    p->ui.buttonBox->setStandardButtons(btns);
}

ColorDialog::ButtonMode ColorDialog::buttonMode() const
{
    return p->button_mode;
}

void ColorDialog::setColorInternal(const QColor &col)
{
    /**
     * \note Unlike setColor, this is used to update the current color which
     * migth differ from the final selected color
     */
    p->ui.wheel->setColor(col);

    p->color = col;

    bool blocked = signalsBlocked();
    blockSignals(true);
    for (QWidget *w : findChildren<QWidget *>())
        w->blockSignals(true);

    p->ui.slide_red->setValue(col.red());
    p->ui.spin_red->setValue(p->ui.slide_red->value());
    p->ui.slide_red->setFirstColor(QColor(0, col.green(), col.blue()));
    p->ui.slide_red->setLastColor(QColor(255, col.green(), col.blue()));

    p->ui.slide_green->setValue(col.green());
    p->ui.spin_green->setValue(p->ui.slide_green->value());
    p->ui.slide_green->setFirstColor(QColor(col.red(), 0, col.blue()));
    p->ui.slide_green->setLastColor(QColor(col.red(), 255, col.blue()));

    p->ui.slide_blue->setValue(col.blue());
    p->ui.spin_blue->setValue(p->ui.slide_blue->value());
    p->ui.slide_blue->setFirstColor(QColor(col.red(), col.green(), 0));
    p->ui.slide_blue->setLastColor(QColor(col.red(), col.green(), 255));

    p->ui.slide_hue->setValue(qRound(p->ui.wheel->hue() * 360.0));
    p->ui.slide_hue->setColorSaturation(p->ui.wheel->saturation());
    p->ui.slide_hue->setColorValue(p->ui.wheel->value());
    p->ui.spin_hue->setValue(p->ui.slide_hue->value());

    p->ui.slide_saturation->setValue(qRound(p->ui.wheel->saturation() * 100.0));
    p->ui.spin_saturation->setValue(p->ui.slide_saturation->value());
    p->ui.slide_saturation->setFirstColor(
        QColor::fromHsvF(p->ui.wheel->hue(), 0, p->ui.wheel->value()));
    p->ui.slide_saturation->setLastColor(
        QColor::fromHsvF(p->ui.wheel->hue(), 1, p->ui.wheel->value()));

    p->ui.slide_value->setValue(qRound(p->ui.wheel->value() * 100.0));
    p->ui.spin_value->setValue(p->ui.slide_value->value());
    p->ui.slide_value->setFirstColor(
        QColor::fromHsvF(p->ui.wheel->hue(), p->ui.wheel->saturation(), 0));
    p->ui.slide_value->setLastColor(
        QColor::fromHsvF(p->ui.wheel->hue(), p->ui.wheel->saturation(), 1));

    // RGB → CMYK
    double c, m, y, k;
    rgb_to_cmyk(col, c, m, y, k);
    p->ui.spin_cyan->setValue(c);
    p->ui.spin_magenta->setValue(m);
    p->ui.spin_yellow->setValue(y);
    p->ui.spin_black->setValue(k);

    QColor apha_color = col;
    apha_color.setAlpha(0);
    p->ui.slide_alpha->setFirstColor(apha_color);
    apha_color.setAlpha(255);
    p->ui.slide_alpha->setLastColor(apha_color);
    p->ui.spin_alpha->setValue(col.alpha());
    p->ui.slide_alpha->setValue(col.alpha());

    if (!p->ui.edit_hex->isModified())
        p->ui.edit_hex->setColor(col);

    p->ui.preview->setColor(col);

    blockSignals(blocked);
    for (QWidget *w : findChildren<QWidget *>())
        w->blockSignals(false);

    Q_EMIT colorSelectedCmyk(col, -1, -1, -1, -1); // 非 CMYK 来源
    Q_EMIT colorChanged(col);
}

void ColorDialog::set_hsv()
{
    if (!signalsBlocked()) {
        QColor col = QColor::fromHsv(p->ui.slide_hue->value(), p->ui.slide_saturation->value(),
                                     p->ui.slide_value->value(), p->ui.slide_alpha->value());
        p->ui.wheel->setColor(col);
        setColorInternal(col);
    }
}

void ColorDialog::set_alpha()
{
    if (!signalsBlocked()) {
        QColor col = p->color;
        col.setAlpha(p->ui.slide_alpha->value());
        setColorInternal(col);
    }
}

void ColorDialog::set_cmyk()
{
    if (!signalsBlocked()) {
        double c = p->ui.spin_cyan->value();
        double m = p->ui.spin_magenta->value();
        double y = p->ui.spin_yellow->value();
        double k = p->ui.spin_black->value();

        QColor newColor = cmyk_to_rgb(c, m, y, k);
        newColor.setAlpha(p->ui.slide_alpha->value());
        p->ui.wheel->setColor(newColor);

        p->color = newColor;

        bool blocked = signalsBlocked();
        blockSignals(true);
        for (QWidget *w : findChildren<QWidget *>())
            w->blockSignals(true);

        p->ui.slide_red->setValue(newColor.red());
        p->ui.spin_red->setValue(p->ui.slide_red->value());
        p->ui.slide_red->setFirstColor(QColor(0, newColor.green(), newColor.blue()));
        p->ui.slide_red->setLastColor(QColor(255, newColor.green(), newColor.blue()));

        p->ui.slide_green->setValue(newColor.green());
        p->ui.spin_green->setValue(p->ui.slide_green->value());
        p->ui.slide_green->setFirstColor(QColor(newColor.red(), 0, newColor.blue()));
        p->ui.slide_green->setLastColor(QColor(newColor.red(), 255, newColor.blue()));

        p->ui.slide_blue->setValue(newColor.blue());
        p->ui.spin_blue->setValue(p->ui.slide_blue->value());
        p->ui.slide_blue->setFirstColor(QColor(newColor.red(), newColor.green(), 0));
        p->ui.slide_blue->setLastColor(QColor(newColor.red(), newColor.green(), 255));

        p->ui.slide_hue->setValue(qRound(p->ui.wheel->hue() * 360.0));
        p->ui.slide_hue->setColorSaturation(p->ui.wheel->saturation());
        p->ui.slide_hue->setColorValue(p->ui.wheel->value());
        p->ui.spin_hue->setValue(p->ui.slide_hue->value());

        p->ui.slide_saturation->setValue(qRound(p->ui.wheel->saturation() * 100.0));
        p->ui.spin_saturation->setValue(p->ui.slide_saturation->value());
        p->ui.slide_saturation->setFirstColor(
            QColor::fromHsvF(p->ui.wheel->hue(), 0, p->ui.wheel->value()));
        p->ui.slide_saturation->setLastColor(
            QColor::fromHsvF(p->ui.wheel->hue(), 1, p->ui.wheel->value()));

        p->ui.slide_value->setValue(qRound(p->ui.wheel->value() * 100.0));
        p->ui.spin_value->setValue(p->ui.slide_value->value());
        p->ui.slide_value->setFirstColor(
            QColor::fromHsvF(p->ui.wheel->hue(), p->ui.wheel->saturation(), 0));
        p->ui.slide_value->setLastColor(
            QColor::fromHsvF(p->ui.wheel->hue(), p->ui.wheel->saturation(), 1));

        p->ui.slide_alpha->setFirstColor(QColor(newColor.red(), newColor.green(), newColor.blue(), 0));
        p->ui.slide_alpha->setLastColor(QColor(newColor.red(), newColor.green(), newColor.blue(), 255));
        p->ui.spin_alpha->setValue(newColor.alpha());
        p->ui.slide_alpha->setValue(newColor.alpha());

        if (!p->ui.edit_hex->isModified())
            p->ui.edit_hex->setColor(newColor);

        p->ui.preview->setColor(newColor);

        blockSignals(blocked);
        for (QWidget *w : findChildren<QWidget *>())
            w->blockSignals(false);

        p->pendingCmyk = {c, m, y, k, true};
        Q_EMIT colorSelectedCmyk(newColor, c, m, y, k);
        Q_EMIT colorChanged(newColor);
    }
}

void ColorDialog::set_rgb()
{
    if (!signalsBlocked()) {
        QColor col(p->ui.slide_red->value(), p->ui.slide_green->value(), p->ui.slide_blue->value(),
                   p->ui.slide_alpha->value());
        if (col.saturation() == 0)
            col = QColor::fromHsv(p->ui.slide_hue->value(), 0, col.value());
        p->ui.wheel->setColor(col);

        setColorInternal(col);
    }
}

void ColorDialog::on_edit_hex_colorChanged(const QColor &color)
{
    setColorInternal(color);
}

void ColorDialog::on_edit_hex_colorEditingFinished(const QColor &color)
{
    p->ui.edit_hex->setModified(false);
    setColorInternal(color);
}

void ColorDialog::on_buttonBox_clicked(QAbstractButton *btn)
{
    QDialogButtonBox::ButtonRole role = p->ui.buttonBox->buttonRole(btn);

    switch (role) {
    case QDialogButtonBox::AcceptRole:
    case QDialogButtonBox::ApplyRole:
        // Explicitly select the color
        p->ui.preview->setComparisonColor(color());
        if (p->pendingCmyk.valid)
            Q_EMIT colorSelectedCmyk(color(), p->pendingCmyk.c, p->pendingCmyk.m,
                                      p->pendingCmyk.y, p->pendingCmyk.k);
        Q_EMIT colorSelected(color());
        break;

    case QDialogButtonBox::ActionRole:
        // Currently, the only action button is the "pick color" button
        grabMouse(Qt::CrossCursor);
        p->pick_from_screen = true;
        break;

    case QDialogButtonBox::ResetRole:
        // Restore old color
        setColorInternal(p->ui.preview->comparisonColor());
        break;

    default:
        break;
    }
}

void ColorDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasColor() ||
        (event->mimeData()->hasText() && QColor(event->mimeData()->text()).isValid()))
        event->acceptProposedAction();
}

void ColorDialog::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasColor()) {
        setColorInternal(event->mimeData()->colorData().value<QColor>());
        event->accept();
    } else if (event->mimeData()->hasText()) {
        QColor col(event->mimeData()->text());
        if (col.isValid()) {
            setColorInternal(col);
            event->accept();
        }
    }
}

void ColorDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (p->pick_from_screen) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        setColorInternal(utils::get_screen_color(event->globalPosition().toPoint()));
#else
        setColorInternal(utils::get_screen_color(event->globalPos()));
#endif
        p->pick_from_screen = false;
        releaseMouse();
    }
}

void ColorDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (p->pick_from_screen) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        setColorInternal(utils::get_screen_color(event->globalPosition().toPoint()));
#else
        setColorInternal(utils::get_screen_color(event->globalPos()));
#endif
    }
}

void ColorDialog::keyReleaseEvent(QKeyEvent *ev)
{
    QDialog::keyReleaseEvent(ev);

#ifdef Q_OS_ANDROID
    if (ev->key() == Qt::Key_Back) {
        reject();
        ev->accept();
    }
#endif
}

void ColorDialog::setWheelShape(ColorWheel::ShapeEnum shape)
{
    p->ui.wheel->setSelectorShape(shape);
}

ColorWheel::ShapeEnum ColorDialog::wheelShape() const
{
    return p->ui.wheel->selectorShape();
}

void ColorDialog::setColorSpace(ColorWheel::ColorSpaceEnum space)
{
    p->ui.wheel->setColorSpace(space);
}

ColorWheel::ColorSpaceEnum ColorDialog::colorSpace() const
{
    return p->ui.wheel->colorSpace();
}

void ColorDialog::setWheelRotating(bool rotating)
{
    p->ui.wheel->setRotatingSelector(rotating);
}

bool ColorDialog::wheelRotating() const
{
    return p->ui.wheel->rotatingSelector();
}

void ColorDialog::setWheelMirrored(bool mirrored)
{
    p->ui.wheel->setMirroredSelector(mirrored);
}

bool ColorDialog::wheelMirrored() const
{
    return p->ui.wheel->mirroredSelector();
}

int ColorDialog::exec()
{
#if defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_FAKE)
    showMaximized();
    setFocus();
#endif
    return QDialog::exec();
}

} // namespace color_widgets

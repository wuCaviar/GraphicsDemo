#include "PropertyPanel.h"

#include "RectItem.h"
#include "EllipseItem.h"
#include "LineItem.h"
#include "BezierCurveItem.h"
#include "TextItem.h"
#include "ImageItem.h"
#include "GradientDialog.h"
#include "ColorUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

PropertyPanel::PropertyPanel(QWidget *parent) : QDockWidget(tr("Properties"), parent)
{
    setupUI();
}

void PropertyPanel::setupUI()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto *mainWidget = new QWidget;
    auto *mainLayout = new QVBoxLayout(mainWidget);

    // ---- 无选中项时的占位标签 ----
    m_noSelectionLabel = new QLabel(tr("No item selected"));
    m_noSelectionLabel->setAlignment(Qt::AlignCenter);
    m_noSelectionLabel->setStyleSheet(
            QStringLiteral("color: gray; font-style: italic; padding: 40px 0;"));
    mainLayout->addWidget(m_noSelectionLabel);

    // ---- 位置/尺寸 ----
    m_geomGroup = new QGroupBox(tr("Geometry"));
    auto *geomLayout = new QFormLayout(m_geomGroup);
    m_xSpin = new QDoubleSpinBox;
    m_xSpin->setRange(-99999, 99999);
    m_ySpin = new QDoubleSpinBox;
    m_ySpin->setRange(-99999, 99999);
    m_wSpin = new QDoubleSpinBox;
    m_wSpin->setRange(0, 99999);
    m_hSpin = new QDoubleSpinBox;
    m_hSpin->setRange(0, 99999);
    geomLayout->addRow(tr("X:"), m_xSpin);
    geomLayout->addRow(tr("Y:"), m_ySpin);
    geomLayout->addRow(tr("Width:"), m_wSpin);
    geomLayout->addRow(tr("Height:"), m_hSpin);
    mainLayout->addWidget(m_geomGroup);

    // ---- 边框 ----
    m_penGroup = new QGroupBox(tr("Border"));
    auto *penLayout = new QFormLayout(m_penGroup);
    m_penColorSelector = new ColorSelector(this);
    m_penColorSelector->setFixedSize(60, 24);
    m_penColorSelector->setUpdateMode(ColorSelector::Continuous);
    m_penColorSelector->setDialogModality(Qt::ApplicationModal);
    m_penWidthSpin = new QSpinBox;
    m_penWidthSpin->setRange(0, 100);
    m_penStyleCombo = new QComboBox;
    m_penStyleCombo->addItems({tr("Solid"), tr("Dash"), tr("Dot"), tr("DashDot"), tr("DashDotDot"), tr("None")});
    penLayout->addRow(tr("Color:"), m_penColorSelector);
    penLayout->addRow(tr("Width:"), m_penWidthSpin);
    penLayout->addRow(tr("Style:"), m_penStyleCombo);
    mainLayout->addWidget(m_penGroup);

    // ---- 填充 ----
    m_brushGroup = new QGroupBox(tr("Fill"));

    auto *brushLayout = new QFormLayout(m_brushGroup);

    m_fillModeCombo = new QComboBox;
    m_fillModeCombo->addItems({tr("No Fill"), tr("Solid Color"), tr("Gradient")});

    m_brushSolid = new ColorSelector(this);
    m_brushSolid->setFixedSize(60, 24);
    m_brushSolid->setUpdateMode(ColorSelector::Continuous);
    m_brushSolid->setDialogModality(Qt::ApplicationModal);

    m_brushGradient = new GradientPreview(this);
    m_brushGradient->setFixedSize(60, 24);

    auto *brushBtnLayout = new QHBoxLayout;
    brushBtnLayout->addWidget(m_brushSolid);
    brushBtnLayout->addWidget(m_brushGradient);

    brushLayout->addRow(tr("Mode:"), m_fillModeCombo);
    brushLayout->addRow(tr("Color:"), brushBtnLayout);
    mainLayout->addWidget(m_brushGroup);

    // ---- 文字 ----
    m_textGroup = new QGroupBox(tr("Text"));
    auto *textLayout = new QFormLayout(m_textGroup);
    m_fontCombo = new QFontComboBox;
    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(1, 200);
    m_boldBtn = new QPushButton(tr("B"));
    m_boldBtn->setCheckable(true);
    m_boldBtn->setFixedSize(30, 24);
    m_italicBtn = new QPushButton(tr("I"));
    m_italicBtn->setCheckable(true);
    m_italicBtn->setFixedSize(30, 24);
    auto *styleLayout = new QHBoxLayout;
    styleLayout->addWidget(m_boldBtn);
    styleLayout->addWidget(m_italicBtn);
    m_textColorSelector = new ColorSelector(this);
    m_textColorSelector->setFixedSize(60, 24);
    m_textColorSelector->setUpdateMode(ColorSelector::Continuous);
    m_textColorSelector->setDialogModality(Qt::ApplicationModal);
    m_textBgColorSelector = new ColorSelector(this);
    m_textBgColorSelector->setFixedSize(60, 24);
    m_textBgColorSelector->setUpdateMode(ColorSelector::Continuous);
    m_textBgColorSelector->setDialogModality(Qt::ApplicationModal);
    m_textEdit = new QLineEdit;
    textLayout->addRow(tr("Font:"), m_fontCombo);
    textLayout->addRow(tr("Size:"), m_fontSizeSpin);
    textLayout->addRow(tr("Style:"), styleLayout);
    textLayout->addRow(tr("Color:"), m_textColorSelector);
    textLayout->addRow(tr("Background:"), m_textBgColorSelector);
    textLayout->addRow(tr("Text:"), m_textEdit);
    mainLayout->addWidget(m_textGroup);

    // ---- 圆角 ----
    m_cornerGroup = new QGroupBox(tr("Corner Radius"));
    auto *cornerLayout = new QFormLayout(m_cornerGroup);
    m_cornerSpin = new QDoubleSpinBox;
    m_cornerSpin->setRange(0, 9999);
    cornerLayout->addRow(tr("Radius:"), m_cornerSpin);
    mainLayout->addWidget(m_cornerGroup);

    // ---- 旋转 ----
    m_rotationGroup = new QGroupBox(tr("Rotation"));
    auto *rotationLayout = new QFormLayout(m_rotationGroup);
    m_rotationSpin = new QDoubleSpinBox;
    m_rotationSpin->setRange(-360, 360);
    m_rotationSpin->setWrapping(true);
    m_rotationSpin->setSingleStep(1.0);
    m_rotationSpin->setDecimals(1);
    m_rotationSpin->setSuffix(QString::fromUtf8("°"));
    rotationLayout->addRow(tr("Angle:"), m_rotationSpin);
    mainLayout->addWidget(m_rotationGroup);

    mainLayout->addStretch();
    scrollArea->setWidget(mainWidget);
    setWidget(scrollArea);

    // ---- 初始状态：隐藏所有分组，显示占位标签 ----
    m_geomGroup->setVisible(false);
    m_penGroup->setVisible(false);
    m_brushGroup->setVisible(false);
    m_textGroup->setVisible(false);
    m_cornerGroup->setVisible(false);
    m_rotationGroup->setVisible(false);
    m_noSelectionLabel->setVisible(true);

    // ---- 信号连接 ----
    connect(m_penColorSelector, &ColorSelector::colorEditingStarted, this,
            [this](const QColor &) { beginColorPreview(ColorPreviewTarget::Border); });
    connect(m_penColorSelector, &ColorSelector::colorChanged, this,
            [this](const QColor &color) { previewColorChange(ColorPreviewTarget::Border, color); });
    connect(m_penColorSelector, &ColorSelector::colorSelected, this, &PropertyPanel::onPenColorSelected);
    connect(m_penColorSelector, &ColorSelector::colorSelectionCanceled, this,
            [this](const QColor &) { cancelColorPreview(ColorPreviewTarget::Border); });
    connect(m_penColorSelector, &ColorSelector::colorSelectedCmyk, this,
            [this](const QColor &, double c, double m, double y, double k) {
                if (auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem)) {
                    if (c >= 0) gi->setItemPenCmyk(c, m, y, k);
                    else gi->clearPenCmyk();
                }
            });

    connect(m_penWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &PropertyPanel::onPenWidthChanged);
    connect(m_penStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PropertyPanel::onPenStyleChanged);
    connect(m_brushSolid, &ColorSelector::colorEditingStarted, this,
            [this](const QColor &) { beginColorPreview(ColorPreviewTarget::Fill); });
    connect(m_brushSolid, &ColorSelector::colorChanged, this,
            [this](const QColor &color) { previewColorChange(ColorPreviewTarget::Fill, color); });
    connect(m_brushSolid, &ColorSelector::colorSelected, this, &PropertyPanel::onBrushColorClicked);
    connect(m_brushSolid, &ColorSelector::colorSelectionCanceled, this,
            [this](const QColor &) { cancelColorPreview(ColorPreviewTarget::Fill); });
    connect(m_brushSolid, &ColorSelector::colorSelectedCmyk, this,
            [this](const QColor &, double c, double m, double y, double k) {
                if (auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem)) {
                    if (c >= 0) gi->setItemBrushCmyk(c, m, y, k);
                    else gi->clearBrushCmyk();
                }
            });

    connect(m_fillModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PropertyPanel::onFillModeChanged);
    connect(m_brushGradient, &GradientPreview::brushEditingStarted, this,
            [this](const QBrush &) { beginBrushPreview(ColorPreviewTarget::FillGradient); });
    connect(m_brushGradient, &GradientPreview::brushPreviewed, this,
            &PropertyPanel::onBrushGradientPreviewed);
    connect(m_brushGradient, &GradientPreview::brushSelected, this,
            &PropertyPanel::onBrushGradientSelected);
    connect(m_brushGradient, &GradientPreview::brushSelectionCanceled, this,
            &PropertyPanel::onBrushGradientCanceled);

    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this,
            &PropertyPanel::onFontFamilyChanged);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &PropertyPanel::onFontSizeChanged);
    connect(m_boldBtn, &QPushButton::toggled, this, &PropertyPanel::onBoldToggled);
    connect(m_italicBtn, &QPushButton::toggled, this, &PropertyPanel::onItalicToggled);
    connect(m_textColorSelector, &ColorSelector::colorEditingStarted, this,
            [this](const QColor &) { beginColorPreview(ColorPreviewTarget::Text); });
    connect(m_textColorSelector, &ColorSelector::colorChanged, this,
            [this](const QColor &color) { previewColorChange(ColorPreviewTarget::Text, color); });
    connect(m_textColorSelector, &ColorSelector::colorSelected, this, &PropertyPanel::onTextColorClicked);
    connect(m_textColorSelector, &ColorSelector::colorSelectionCanceled, this,
            [this](const QColor &) { cancelColorPreview(ColorPreviewTarget::Text); });
    connect(m_textColorSelector, &ColorSelector::colorSelectedCmyk, this,
            [this](const QColor &, double c, double m, double y, double k) {
                if (auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem)) {
                    if (c >= 0) gi->setItemPenCmyk(c, m, y, k);
                    else gi->clearPenCmyk();
                }
            });

    connect(m_textBgColorSelector, &ColorSelector::colorEditingStarted, this,
            [this](const QColor &) { beginColorPreview(ColorPreviewTarget::TextBackground); });
    connect(m_textBgColorSelector, &ColorSelector::colorChanged, this,
            [this](const QColor &color) { previewColorChange(ColorPreviewTarget::TextBackground, color); });
    connect(m_textBgColorSelector, &ColorSelector::colorSelected, this, &PropertyPanel::onTextBgColorClicked);
    connect(m_textBgColorSelector, &ColorSelector::colorSelectionCanceled, this,
            [this](const QColor &) { cancelColorPreview(ColorPreviewTarget::TextBackground); });
    connect(m_textBgColorSelector, &ColorSelector::colorSelectedCmyk, this,
            [this](const QColor &, double c, double m, double y, double k) {
                if (auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem)) {
                    if (c >= 0) gi->setItemBrushCmyk(c, m, y, k);
                    else gi->clearBrushCmyk();
                }
            });

    connect(m_textEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onTextChanged);
    connect(m_xSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &PropertyPanel::onGeometryChanged);
    connect(m_ySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &PropertyPanel::onGeometryChanged);
    connect(m_wSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &PropertyPanel::onGeometryChanged);
    connect(m_hSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &PropertyPanel::onGeometryChanged);
    connect(m_cornerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &PropertyPanel::onCornerRadiusChanged);
    connect(m_rotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &PropertyPanel::onRotationChanged);
}

void PropertyPanel::setItem(QGraphicsItem *item)
{
    m_currentItem = item;
    m_updating = true;
    updatePanel();
    m_updating = false;
}

void PropertyPanel::beginColorPreview(ColorPreviewTarget target)
{
    if (m_updating || !m_currentItem)
        return;

    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi)
        return;
    if (target == ColorPreviewTarget::Text && !qgraphicsitem_cast<TextItem *>(m_currentItem))
        return;

    m_previewTarget = target;
    m_previewItem = m_currentItem;

    if (target == ColorPreviewTarget::Border || target == ColorPreviewTarget::Text)
        m_previewOldPen = gi->itemPen();
    else
        m_previewOldBrush = gi->itemBrush();
}

void PropertyPanel::previewColorChange(ColorPreviewTarget target, const QColor &color)
{
    if (m_updating || !color.isValid() || m_previewTarget != target || !m_previewItem)
        return;

    auto *gi = dynamic_cast<IGraphicsItem *>(m_previewItem);
    if (!gi)
        return;

    switch (target) {
    case ColorPreviewTarget::Border:
    case ColorPreviewTarget::Text: {
        QPen pen = gi->itemPen();
        pen.setColor(color);
        gi->setItemPen(pen);
        break;
    }
    case ColorPreviewTarget::Fill:
    case ColorPreviewTarget::TextBackground: {
        QBrush brush(color);
        brush.setStyle(Qt::SolidPattern);
        gi->setItemBrush(brush);
        break;
    }
    default:
        break;
    }
}

void PropertyPanel::commitColorChange(ColorPreviewTarget target, const QColor &color)
{
    if (m_updating || !color.isValid())
        return;

    if (m_previewTarget != target || !m_previewItem)
        beginColorPreview(target);
    if (m_previewTarget != target || !m_previewItem)
        return;

    previewColorChange(target, color);

    auto *item = m_previewItem;
    auto *gi = dynamic_cast<IGraphicsItem *>(item);
    if (!gi) {
        clearColorPreview();
        return;
    }

    if (target == ColorPreviewTarget::Border || target == ColorPreviewTarget::Text) {
        QPen newPen = gi->itemPen();
        QPen oldPen = m_previewOldPen;
        clearColorPreview();
        if (newPen != oldPen)
            Q_EMIT penChanged(item, oldPen, newPen);
    } else {
        QBrush newBrush = gi->itemBrush();
        QBrush oldBrush = m_previewOldBrush;
        clearColorPreview();
        if (newBrush != oldBrush)
            Q_EMIT brushChanged(item, oldBrush, newBrush);
    }
}

void PropertyPanel::cancelColorPreview(ColorPreviewTarget target)
{
    if (m_previewTarget != target || !m_previewItem) {
        clearColorPreview();
        return;
    }

    auto *gi = dynamic_cast<IGraphicsItem *>(m_previewItem);
    if (gi) {
        if (target == ColorPreviewTarget::Border || target == ColorPreviewTarget::Text)
            gi->setItemPen(m_previewOldPen);
        else
            gi->setItemBrush(m_previewOldBrush);
    }

    clearColorPreview();
}

void PropertyPanel::beginBrushPreview(ColorPreviewTarget target)
{
    if (m_updating || !m_currentItem)
        return;

    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi)
        return;

    m_previewTarget = target;
    m_previewItem = m_currentItem;
    m_previewOldBrush = gi->itemBrush();
}

void PropertyPanel::previewBrushChange(ColorPreviewTarget target, const QBrush &brush)
{
    if (m_updating || m_previewTarget != target || !m_previewItem)
        return;

    auto *gi = dynamic_cast<IGraphicsItem *>(m_previewItem);
    if (gi)
        gi->setItemBrush(brush);
}

void PropertyPanel::commitBrushChange(ColorPreviewTarget target, const QBrush &brush)
{
    if (m_updating)
        return;

    if (m_previewTarget != target || !m_previewItem)
        beginBrushPreview(target);
    if (m_previewTarget != target || !m_previewItem)
        return;

    previewBrushChange(target, brush);

    auto *item = m_previewItem;
    auto *gi = dynamic_cast<IGraphicsItem *>(item);
    if (!gi) {
        clearColorPreview();
        return;
    }

    QBrush newBrush = gi->itemBrush();
    QBrush oldBrush = m_previewOldBrush;
    clearColorPreview();
    if (newBrush != oldBrush)
        Q_EMIT brushChanged(item, oldBrush, newBrush);
}

void PropertyPanel::clearColorPreview()
{
    m_previewTarget = ColorPreviewTarget::None;
    m_previewItem = nullptr;
    m_previewOldPen = QPen();
    m_previewOldBrush = QBrush();
}

void PropertyPanel::updatePanel()
{
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) {
        // 无选中项：显示占位标签，隐藏所有分组
        m_noSelectionLabel->setVisible(true);
        m_geomGroup->setVisible(false);
        m_penGroup->setVisible(false);
        m_brushGroup->setVisible(false);
        m_textGroup->setVisible(false);
        m_cornerGroup->setVisible(false);
        m_rotationGroup->setVisible(false);
        return;
    }

    // 有选中项：隐藏占位标签
    m_noSelectionLabel->setVisible(false);

    auto flags = gi->propertyFlags();

    // ---- 位置/尺寸 ----
    m_geomGroup->setVisible(true);
    m_xSpin->setValue(m_currentItem->pos().x());
    m_ySpin->setValue(m_currentItem->pos().y());

    // 判断 W/H 是否可编辑：支持 setRect() 的图元
    bool canResizeRect = (qgraphicsitem_cast<RectItem *>(m_currentItem)
                          || qgraphicsitem_cast<EllipseItem *>(m_currentItem)
                          || qgraphicsitem_cast<TextItem *>(m_currentItem)
                          || qgraphicsitem_cast<ImageItem *>(m_currentItem));
    m_wSpin->setReadOnly(!canResizeRect);
    m_hSpin->setReadOnly(!canResizeRect);

    // 尺寸（根据图元类型）
    QRectF rect;
    if (auto *ri = qgraphicsitem_cast<RectItem *>(m_currentItem))
        rect = ri->rect();
    else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(m_currentItem))
        rect = ei->rect();
    else if (auto *li = qgraphicsitem_cast<LineItem *>(m_currentItem)) {
        rect = li->boundingRect();
    } else if (auto *bi = qgraphicsitem_cast<BezierCurveItem *>(m_currentItem)) {
        rect = bi->boundingRect();
    } else if (auto *ti = qgraphicsitem_cast<TextItem *>(m_currentItem)) {
        rect = ti->boundingRect();
    } else if (auto *ii = qgraphicsitem_cast<ImageItem *>(m_currentItem)) {
        rect = ii->boundingRect();
    }
    m_wSpin->setValue(rect.width());
    m_hSpin->setValue(rect.height());

    // ---- 边框 ----
    m_penGroup->setVisible(flags & IGraphicsItem::HasPen);
    if (flags & IGraphicsItem::HasPen) {
        QPen p = gi->itemPen();
        m_oldPen = p;
        m_penColorSelector->setColor(p.color());
        m_penWidthSpin->setValue(p.width());
        int styleIdx = 0;
        switch (p.style()) {
        case Qt::SolidLine:      styleIdx = 0; break;
        case Qt::DashLine:       styleIdx = 1; break;
        case Qt::DotLine:        styleIdx = 2; break;
        case Qt::DashDotLine:    styleIdx = 3; break;
        case Qt::DashDotDotLine: styleIdx = 4; break;
        case Qt::NoPen:          styleIdx = 5; break;
        default: break;
        }
        m_penStyleCombo->setCurrentIndex(styleIdx);
    }

    // ---- 填充 ----
    m_brushGroup->setVisible(flags & IGraphicsItem::HasBrush);
    if (flags & IGraphicsItem::HasBrush) {
        QBrush b = gi->itemBrush();
        m_oldBrush = b;

        Qt::BrushStyle style = b.style();
        int nIndex = static_cast<int>(FillMode::NoFill);
        if (style == Qt::LinearGradientPattern ||
            style == Qt::RadialGradientPattern ||
            style == Qt::ConicalGradientPattern) {
            // 渐变
            nIndex = static_cast<int>(FillMode::Gradient);
            m_brushGradient->setBrush(b);

        } else if (style == Qt::SolidPattern) {
            // 纯色
            nIndex = static_cast<int>(FillMode::Solid);
            m_brushSolid->setColor(b.color());
        }
        else
        {
            // 隐藏控件
            m_brushGradient->setVisible(false);
            m_brushSolid->setVisible(false);
        }

        m_fillModeCombo->blockSignals(true);
        m_fillModeCombo->setCurrentIndex(nIndex);
        m_fillModeCombo->blockSignals(false);
    }

    // ---- 文字 ----
    m_textGroup->setVisible(flags & IGraphicsItem::HasText);
    if (flags & IGraphicsItem::HasText) {
        m_oldFont = gi->itemFont();
        m_oldText = gi->text();
        m_fontCombo->setCurrentFont(m_oldFont);
        m_fontSizeSpin->setValue(m_oldFont.pointSize());
        m_boldBtn->setChecked(m_oldFont.bold());
        m_italicBtn->setChecked(m_oldFont.italic());
        m_textEdit->setText(m_oldText);

        // 文字颜色（仅 TextItem 有此概念）
        if (qgraphicsitem_cast<TextItem *>(m_currentItem))
        {
            m_textColorSelector->setVisible(true);
            m_textBgColorSelector->setVisible(true);

            m_textColorSelector->setColor(gi->itemPen().color());
            m_textBgColorSelector->setColor(gi->itemBrush().color());
        }
        else
        {
            m_textColorSelector->setVisible(false);
            m_textBgColorSelector->setVisible(false);
        }
    }

    // ---- 圆角（仅 RectItem） ----
    if (auto *ri = qgraphicsitem_cast<RectItem *>(m_currentItem)) {
        m_oldCornerRadius = ri->cornerRadius();
        m_cornerSpin->setValue(m_oldCornerRadius);
        m_cornerGroup->setVisible(true);
    } else {
        m_cornerGroup->setVisible(false);
    }

    // ---- 旋转 ----
    if (flags & IGraphicsItem::HasRotation) {
        m_oldRotation = m_currentItem->rotation();
        m_rotationSpin->setValue(m_oldRotation);
        m_rotationGroup->setVisible(true);
    } else {
        m_rotationGroup->setVisible(false);
    }
}

// ---- 边框颜色 ----
void PropertyPanel::onPenColorSelected(const QColor& color)
{
    if (!color.isValid()) return;
    commitColorChange(ColorPreviewTarget::Border, color);
}

void PropertyPanel::onPenWidthChanged(int w)
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QPen oldPen = gi->itemPen();
    QPen newPen = oldPen;
    newPen.setWidth(w);
    emit penChanged(m_currentItem, oldPen, newPen);
}

void PropertyPanel::onPenStyleChanged(int idx)
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    static const Qt::PenStyle styles[] = {
        Qt::SolidLine, Qt::DashLine, Qt::DotLine,
        Qt::DashDotLine, Qt::DashDotDotLine, Qt::NoPen
    };
    QPen oldPen = gi->itemPen();
    QPen newPen = oldPen;
    newPen.setStyle(styles[idx]);
    emit penChanged(m_currentItem, oldPen, newPen);
}

// ---- 填充颜色 ----
void PropertyPanel::onBrushColorClicked(const QColor& color)
{
    if (!color.isValid()) return;
    commitColorChange(ColorPreviewTarget::Fill, color);
}

void PropertyPanel::onFillModeChanged(int idx)
{
    if (!m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    FillMode mode = static_cast<FillMode>(idx);
    QBrush newBrush;
    switch (mode) {
    case FillMode::NoFill:
        newBrush = QBrush(Qt::NoBrush);
        break;
    case FillMode::Solid:
        newBrush = QBrush(m_brushSolid->color());
        break;
    case FillMode::Gradient:
        newBrush = m_brushGradient->brush();
        break;
    default:
        break;
    }

    // 根据模式切换按钮可见性或可用性
    m_brushSolid->setVisible(mode == FillMode::Solid);
    m_brushGradient->setVisible(mode == FillMode::Gradient);

    if (m_updating)
        return;

    QBrush oldBrush = gi->itemBrush();
    if(newBrush != oldBrush)
        Q_EMIT brushChanged(m_currentItem, oldBrush, newBrush);
}

void PropertyPanel::onBrushGradientPreviewed(const QBrush& brush)
{
    previewBrushChange(ColorPreviewTarget::FillGradient, brush);
}

void PropertyPanel::onBrushGradientSelected(const QBrush& brush)
{
    commitBrushChange(ColorPreviewTarget::FillGradient, brush);
}

void PropertyPanel::onBrushGradientCanceled(const QBrush&)
{
    cancelColorPreview(ColorPreviewTarget::FillGradient);
}

// ---- 文字属性 ----
void PropertyPanel::onFontFamilyChanged(const QFont &f)
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QFont oldFont = gi->itemFont();
    QFont newFont = f;
    newFont.setPointSize(oldFont.pointSize());
    newFont.setBold(oldFont.bold());
    newFont.setItalic(oldFont.italic());
    emit fontChanged(m_currentItem, oldFont, newFont);
}

void PropertyPanel::onFontSizeChanged(int s)
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QFont oldFont = gi->itemFont();
    QFont newFont = oldFont;
    newFont.setPointSize(s);
    emit fontChanged(m_currentItem, oldFont, newFont);
}

void PropertyPanel::onBoldToggled(bool b)
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QFont oldFont = gi->itemFont();
    QFont newFont = oldFont;
    newFont.setBold(b);
    emit fontChanged(m_currentItem, oldFont, newFont);
}

void PropertyPanel::onItalicToggled(bool b)
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QFont oldFont = gi->itemFont();
    QFont newFont = oldFont;
    newFont.setItalic(b);
    emit fontChanged(m_currentItem, oldFont, newFont);
}

void PropertyPanel::onTextColorClicked(const QColor& color)
{
    if (!color.isValid()) return;
    commitColorChange(ColorPreviewTarget::Text, color);
}

void PropertyPanel::onTextBgColorClicked(const QColor& color)
{
    if (!color.isValid()) return;
    commitColorChange(ColorPreviewTarget::TextBackground, color);
}

void PropertyPanel::onTextChanged()
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QString oldText = gi->text();
    QString newText = m_textEdit->text();
    if (oldText != newText)
        emit textChanged(m_currentItem, oldText, newText);
}

// ---- 几何属性 ----
void PropertyPanel::onGeometryChanged()
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    qreal x = m_xSpin->value();
    qreal y = m_ySpin->value();
    qreal w = m_wSpin->value();
    qreal h = m_hSpin->value();

    // 位置变更：通过 positionChanged 信号走 undo 系统
    QPointF oldPos = m_currentItem->pos();
    QPointF newPos(x, y);
    if (oldPos != newPos)
        emit positionChanged(m_currentItem, oldPos, newPos);

    // 尺寸变更：通过 geometryChanged 信号走 undo 系统
    QRectF oldRect;
    if (auto *ri = qgraphicsitem_cast<RectItem *>(m_currentItem))
        oldRect = ri->rect();
    else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(m_currentItem))
        oldRect = ei->rect();
    else if (auto *ti = qgraphicsitem_cast<TextItem *>(m_currentItem))
        oldRect = ti->rect();
    else if (auto *ii = qgraphicsitem_cast<ImageItem *>(m_currentItem))
        oldRect = ii->rect();

    QRectF newRect(0, 0, w, h);

    if (oldRect.isValid() && oldRect != newRect)
        emit geometryChanged(m_currentItem, oldRect, newRect);
}

// ---- 圆角 ----
void PropertyPanel::onCornerRadiusChanged(double r)
{
    if (m_updating || !m_currentItem) return;
    auto *ri = qgraphicsitem_cast<RectItem *>(m_currentItem);
    if (!ri) return;

    qreal oldR = ri->cornerRadius();
    if (!qFuzzyCompare(oldR, r))
        emit cornerRadiusChanged(m_currentItem, oldR, r);
}

// ---- 旋转 ----
void PropertyPanel::onRotationChanged(double r)
{
    if (m_updating || !m_currentItem) return;

    qreal oldR = m_currentItem->rotation();
    if (!qFuzzyCompare(oldR, r))
        emit rotationChanged(m_currentItem, oldR, r);
}

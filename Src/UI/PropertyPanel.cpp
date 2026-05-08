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
    m_penColorBtn = new QPushButton;
    m_penColorBtn->setFixedSize(60, 24);
    m_penWidthSpin = new QSpinBox;
    m_penWidthSpin->setRange(0, 100);
    m_penStyleCombo = new QComboBox;
    m_penStyleCombo->addItems({tr("Solid"), tr("Dash"), tr("Dot"), tr("DashDot"), tr("DashDotDot"), tr("None")});
    penLayout->addRow(tr("Color:"), m_penColorBtn);
    penLayout->addRow(tr("Width:"), m_penWidthSpin);
    penLayout->addRow(tr("Style:"), m_penStyleCombo);
    mainLayout->addWidget(m_penGroup);

    // ---- 填充 ----
    m_brushGroup = new QGroupBox(tr("Fill"));
    auto *brushLayout = new QFormLayout(m_brushGroup);
    m_fillModeCombo = new QComboBox;
    m_fillModeCombo->addItems({tr("No Fill"), tr("Solid Color"), tr("Gradient")});
    m_brushColorBtn = new QPushButton;
    m_brushColorBtn->setFixedSize(60, 24);
    m_brushGradientBtn = new QPushButton(tr("Edit..."));
    m_brushGradientBtn->setFixedSize(60, 24);
    auto *brushBtnLayout = new QHBoxLayout;
    brushBtnLayout->addWidget(m_brushColorBtn);
    brushBtnLayout->addWidget(m_brushGradientBtn);
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
    m_textColorBtn = new QPushButton;
    m_textColorBtn->setFixedSize(60, 24);
    m_textBgColorBtn = new QPushButton;
    m_textBgColorBtn->setFixedSize(60, 24);
    m_textEdit = new QLineEdit;
    textLayout->addRow(tr("Font:"), m_fontCombo);
    textLayout->addRow(tr("Size:"), m_fontSizeSpin);
    textLayout->addRow(tr("Style:"), styleLayout);
    textLayout->addRow(tr("Color:"), m_textColorBtn);
    textLayout->addRow(tr("Background:"), m_textBgColorBtn);
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
    connect(m_penColorBtn, &QPushButton::clicked, this, &PropertyPanel::onPenColorClicked);
    connect(m_penWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &PropertyPanel::onPenWidthChanged);
    connect(m_penStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PropertyPanel::onPenStyleChanged);
    connect(m_brushColorBtn, &QPushButton::clicked, this, &PropertyPanel::onBrushColorClicked);
    connect(m_fillModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PropertyPanel::onFillModeChanged);
    connect(m_brushGradientBtn, &QPushButton::clicked, this, &PropertyPanel::onBrushGradientClicked);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this,
            &PropertyPanel::onFontFamilyChanged);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &PropertyPanel::onFontSizeChanged);
    connect(m_boldBtn, &QPushButton::toggled, this, &PropertyPanel::onBoldToggled);
    connect(m_italicBtn, &QPushButton::toggled, this, &PropertyPanel::onItalicToggled);
    connect(m_textColorBtn, &QPushButton::clicked, this, &PropertyPanel::onTextColorClicked);
    connect(m_textBgColorBtn, &QPushButton::clicked, this, &PropertyPanel::onTextBgColorClicked);
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

    // 判断 W/H 是否可编辑：仅 RectItem 和 EllipseItem 支持 setRect()
    bool canResizeRect = (qgraphicsitem_cast<RectItem *>(m_currentItem)
                          || qgraphicsitem_cast<EllipseItem *>(m_currentItem));
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
        ColorUtils::updateColorButton(m_penColorBtn, p.color());
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

        if (b.style() == Qt::LinearGradientPattern || b.style() == Qt::RadialGradientPattern
            || b.style() == Qt::ConicalGradientPattern) {
            m_fillModeCombo->setCurrentIndex(static_cast<int>(FillMode::Gradient));
            m_brushColorBtn->setEnabled(false);
            m_brushGradientBtn->setEnabled(true);
        } else if (b.style() == Qt::SolidPattern) {
            m_fillModeCombo->setCurrentIndex(static_cast<int>(FillMode::Solid));
            m_brushColorBtn->setEnabled(true);
            m_brushGradientBtn->setEnabled(false);
            QColor c = b.color();
            ColorUtils::updateColorButton(m_brushColorBtn, c);
        } else {
            m_fillModeCombo->setCurrentIndex(static_cast<int>(FillMode::NoFill));
            m_brushColorBtn->setEnabled(false);
            m_brushGradientBtn->setEnabled(false);
        }
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
        if (auto *ti = qgraphicsitem_cast<TextItem *>(m_currentItem)) {
            QColor textColor = ti->defaultTextColor();
            ColorUtils::updateColorButton(m_textColorBtn, textColor);
            QBrush bg = gi->itemBrush();
            if (bg.style() == Qt::NoBrush) {
                ColorUtils::updateColorButton(m_textBgColorBtn, QColor(), true);
            } else {
                ColorUtils::updateColorButton(m_textBgColorBtn, bg.color());
            }
            m_textColorBtn->setVisible(true);
            m_textBgColorBtn->setVisible(true);
        } else {
            m_textColorBtn->setVisible(false);
            m_textBgColorBtn->setVisible(false);
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
void PropertyPanel::onPenColorClicked()
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QPen oldPen = gi->itemPen();
    QColor c = ColorUtils::pickColor(this, oldPen.color(), tr("Border Color"));
    if (!c.isValid()) return;
    QPen newPen = oldPen;
    newPen.setColor(c);
    emit penChanged(m_currentItem, oldPen, newPen);
    ColorUtils::updateColorButton(m_penColorBtn, c);
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
void PropertyPanel::onBrushColorClicked()
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QBrush oldBrush = gi->itemBrush();
    QColor c = ColorUtils::pickColor(this, oldBrush.color(), tr("Fill Color"));
    if (!c.isValid()) return;
    QBrush newBrush(c);
    emit brushChanged(m_currentItem, oldBrush, newBrush);
    ColorUtils::updateColorButton(m_brushColorBtn, c);
}

void PropertyPanel::onFillModeChanged(int idx)
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    FillMode mode = static_cast<FillMode>(idx);
    QBrush oldBrush = gi->itemBrush();

    // 根据模式切换按钮可见性
    m_brushColorBtn->setEnabled(mode == FillMode::Solid);
    m_brushGradientBtn->setEnabled(mode == FillMode::Gradient);

    QBrush newBrush;
    switch (mode) {
    case FillMode::NoFill:
        newBrush = QBrush(Qt::NoBrush);
        break;
    case FillMode::Solid:
        // 使用旧颜色或默认白色
        newBrush = QBrush(oldBrush.color().isValid() ? oldBrush.color() : Qt::white);
        ColorUtils::updateColorButton(m_brushColorBtn, newBrush.color());
        break;
    case FillMode::Gradient: {
        // 默认渐变：白到黑
        QLinearGradient g(0, 0, 1, 0);
        g.setCoordinateMode(QGradient::ObjectBoundingMode);
        g.setColorAt(0.0, Qt::white);
        g.setColorAt(1.0, Qt::black);
        newBrush = QBrush(g);
        break;
    }
    }

    if (oldBrush != newBrush)
        emit brushChanged(m_currentItem, oldBrush, newBrush);
}

void PropertyPanel::onBrushGradientClicked()
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QBrush oldBrush = gi->itemBrush();

    // 提取现有渐变或创建默认渐变
    QLinearGradient currentGradient;
    if (oldBrush.style() == Qt::LinearGradientPattern) {
        const QGradient *g = oldBrush.gradient();
        QGradientStops stops = g->stops();
        currentGradient = QLinearGradient(0, 0, 1, 0);
        currentGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        currentGradient.setStops(stops);
    } else {
        currentGradient = QLinearGradient(0, 0, 1, 0);
        currentGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        currentGradient.setColorAt(0.0, Qt::white);
        currentGradient.setColorAt(1.0, Qt::black);
    }

    GradientDialog dlg(currentGradient, this);
    if (dlg.exec() == QDialog::Accepted) {
        QBrush newBrush(dlg.gradient());
        emit brushChanged(m_currentItem, oldBrush, newBrush);
    }
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

void PropertyPanel::onTextColorClicked()
{
    if (m_updating || !m_currentItem) return;
    auto *ti = qgraphicsitem_cast<TextItem *>(m_currentItem);
    if (!ti) return;

    QColor oldColor = ti->defaultTextColor();
    QColor c = ColorUtils::pickColor(this, oldColor, tr("Text Color"));
    if (!c.isValid()) return;

    // 通过 penChanged 信号传递文字颜色变更（TextItem 的 setItemPen 会设置 defaultTextColor）
    QPen oldPen = ti->itemPen();
    QPen newPen = oldPen;
    newPen.setColor(c);
    emit penChanged(m_currentItem, oldPen, newPen);
    ColorUtils::updateColorButton(m_textColorBtn, c);
}

void PropertyPanel::onTextBgColorClicked()
{
    if (m_updating || !m_currentItem) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_currentItem);
    if (!gi) return;

    QBrush oldBrush = gi->itemBrush();
    QColor oldColor = (oldBrush.style() == Qt::SolidPattern) ? oldBrush.color() : Qt::white;
    QColor c = ColorUtils::pickColor(this, oldColor, tr("Background Color"));
    if (!c.isValid()) return;

    QBrush newBrush(c);
    emit brushChanged(m_currentItem, oldBrush, newBrush);
    ColorUtils::updateColorButton(m_textBgColorBtn, c);
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

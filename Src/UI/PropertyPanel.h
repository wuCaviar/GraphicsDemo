#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include "GradientDialog.h"
#include "IGraphicsItem.h"
#include <QDockWidget>
#include <QGraphicsItem>

#include <QtColorWidgets/ColorSelector>

using namespace color_widgets;

class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLineEdit;
class QFontComboBox;
class QLabel;
class QGroupBox;
class QGraphicsItem;

// 填充模式
enum class FillMode {
    NoFill = 0,
    Solid = 1,
    Gradient = 2,
};

// 属性编辑面板，可编辑选中图元的边框/填充/文字/尺寸等属性
class PropertyPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget *parent = nullptr);

    void setItem(QGraphicsItem *item);
    QGraphicsItem *currentItem() const { return m_currentItem; }

signals:
    // 属性变更信号，由 MainWindow 接收并创建撤销命令
    void penChanged(QGraphicsItem *item, const QPen &oldPen, const QPen &newPen);
    void brushChanged(QGraphicsItem *item, const QBrush &oldBrush, const QBrush &newBrush);
    void fontChanged(QGraphicsItem *item, const QFont &oldFont, const QFont &newFont);
    void textChanged(QGraphicsItem *item, const QString &oldText, const QString &newText);
    void geometryChanged(QGraphicsItem *item, const QRectF &oldRect, const QRectF &newRect);
    void cornerRadiusChanged(QGraphicsItem *item, qreal oldR, qreal newR);
    void positionChanged(QGraphicsItem *item, const QPointF &oldPos, const QPointF &newPos);
    void rotationChanged(QGraphicsItem *item, qreal oldRotation, qreal newRotation);

private slots:
    void onPenColorSelected(const QColor& color);
    void onPenWidthChanged(int w);
    void onPenStyleChanged(int idx);
    void onFillModeChanged(int idx);
    void onBrushColorClicked(const QColor& color);
    void onBrushGradientPreviewed(const QBrush& brush);
    void onBrushGradientSelected(const QBrush& brush);
    void onBrushGradientCanceled(const QBrush& oldBrush);
    void onFontFamilyChanged(const QFont &f);
    void onFontSizeChanged(int s);
    void onBoldToggled(bool b);
    void onItalicToggled(bool b);
    void onTextColorClicked(const QColor& color);
    void onTextBgColorClicked(const QColor& color);
    void onTextChanged();
    void onGeometryChanged();
    void onCornerRadiusChanged(double r);
    void onRotationChanged(double r);

private:
    enum class ColorPreviewTarget {
        None,
        Border,
        Fill,
        Text,
        TextBackground,
        FillGradient,
    };

    void setupUI();
    void updatePanel();
    void beginColorPreview(ColorPreviewTarget target);
    void previewColorChange(ColorPreviewTarget target, const QColor &color);
    void commitColorChange(ColorPreviewTarget target, const QColor &color);
    void cancelColorPreview(ColorPreviewTarget target);
    void beginBrushPreview(ColorPreviewTarget target);
    void previewBrushChange(ColorPreviewTarget target, const QBrush &brush);
    void commitBrushChange(ColorPreviewTarget target, const QBrush &brush);
    void clearColorPreview();

    QGraphicsItem *m_currentItem = nullptr;
    bool m_updating = false; // 防止信号循环

    // 分组框
    QGroupBox *m_geomGroup = nullptr;
    QGroupBox *m_penGroup = nullptr;
    QGroupBox *m_brushGroup = nullptr;
    QGroupBox *m_textGroup = nullptr;
    QGroupBox *m_cornerGroup = nullptr;
    QGroupBox *m_rotationGroup = nullptr;

    // 无选中项占位标签
    QLabel *m_noSelectionLabel = nullptr;

    // 位置/尺寸
    QDoubleSpinBox *m_xSpin = nullptr;
    QDoubleSpinBox *m_ySpin = nullptr;
    QDoubleSpinBox *m_wSpin = nullptr;
    QDoubleSpinBox *m_hSpin = nullptr;

    // 边框
    ColorSelector *m_penColorSelector = nullptr;
    QSpinBox *m_penWidthSpin = nullptr;
    QComboBox *m_penStyleCombo = nullptr;

    // 填充
    QComboBox *m_fillModeCombo = nullptr; // NoFill / Solid / Gradient
    ColorSelector* m_brushSolid = nullptr;
    GradientPreview* m_brushGradient = nullptr;

    // 文字
    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QPushButton *m_boldBtn = nullptr;
    QPushButton *m_italicBtn = nullptr;
    ColorSelector* m_textColorSelector = nullptr;
    ColorSelector* m_textBgColorSelector = nullptr;

    QLineEdit *m_textEdit = nullptr;

    // 圆角
    QDoubleSpinBox *m_cornerSpin = nullptr;

    // 旋转
    QDoubleSpinBox *m_rotationSpin = nullptr;

    // 当前值缓存
    QPen m_oldPen;
    QBrush m_oldBrush;
    QFont m_oldFont;
    QString m_oldText;
    QRectF m_oldRect;
    qreal m_oldCornerRadius = 0;
    qreal m_oldRotation = 0;

    ColorPreviewTarget m_previewTarget = ColorPreviewTarget::None;
    QGraphicsItem *m_previewItem = nullptr;
    QPen m_previewOldPen;
    QBrush m_previewOldBrush;
};

#endif // PROPERTYPANEL_H

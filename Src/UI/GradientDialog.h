#ifndef GRADIENTDIALOG_H
#define GRADIENTDIALOG_H

#include <QDialog>
#include <QLinearGradient>

class QComboBox;
class QPushButton;
class QDoubleSpinBox;

// 渐变编辑对话框 — 编辑线性渐变的起止颜色和方向
class GradientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GradientDialog(const QLinearGradient &gradient = QLinearGradient(),
                            QWidget *parent = nullptr);

    QLinearGradient gradient() const;

private slots:
    void onStartColorClicked();
    void onEndColorClicked();
    void onDirectionChanged(int index);

private:
    void setupUI();
    void updatePreview();

    QLinearGradient m_gradient;
    QColor m_startColor;
    QColor m_endColor;

    QPushButton *m_startColorBtn = nullptr;
    QPushButton *m_endColorBtn = nullptr;
    QComboBox *m_directionCombo = nullptr;
    QWidget *m_previewWidget = nullptr;
};

#endif // GRADIENTDIALOG_H

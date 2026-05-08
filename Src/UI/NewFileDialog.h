#ifndef NEWFILEDIALOG_H
#define NEWFILEDIALOG_H

#include <QDialog>
#include <QSizeF>

class QComboBox;
class QDoubleSpinBox;

// 新建文件对话框 — 选择打印行业常用尺寸或自定义尺寸
class NewFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewFileDialog(QWidget *parent = nullptr);

    QSizeF selectedSize() const;
    QString selectedPresetName() const;

private slots:
    void onPresetChanged(int index);

private:
    void setupUI();

    QComboBox *m_presetCombo = nullptr;
    QDoubleSpinBox *m_widthSpin = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
};

#endif // NEWFILEDIALOG_H

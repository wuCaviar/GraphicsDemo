#ifndef RESIZECANVASDIALOG_H
#define RESIZECANVASDIALOG_H

#include <QDialog>
#include <QSizeF>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

class ResizeCanvasDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResizeCanvasDialog(QWidget *parent = nullptr);

    void setCurrentSize(const QSizeF &pixelSize, qreal ppi, bool isMmMode);
    QSizeF newPixelSize() const;
    int selectedDpi() const;

private:
    void setupUI();

    QDoubleSpinBox *m_widthSpin = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QComboBox *m_dpiCombo = nullptr;
    QLabel *m_infoLabel = nullptr;
    qreal m_ppi = 300;
    bool m_isMmMode = false;
};

#endif // RESIZECANVASDIALOG_H

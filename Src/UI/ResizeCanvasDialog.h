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

    void setCurrentSizeMM(const QSizeF &pixelSize, qreal ppi);
    QSizeF newPixelSize(qreal ppi) const;

private:
    void setupUI();

    QDoubleSpinBox *m_widthSpin = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QLabel *m_infoLabel = nullptr;
};

#endif // RESIZECANVASDIALOG_H

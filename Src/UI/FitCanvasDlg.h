#ifndef FITCANVASDLG_H
#define FITCANVASDLG_H

#include <QDialog>

namespace Ui {
class FitCanvasDlg;
}

enum FitCanvasType
{
    fctNone = 0,
    fctAdapt,
    fctWidth,
    fctHeight,
};

class FitCanvasDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FitCanvasDlg(QWidget *parent = nullptr);
    ~FitCanvasDlg();

    void setParam(const QSizeF &canvas, const QSizeF &image = QSizeF());

    QSizeF getResult() const;
    FitCanvasType fitType() const;
    double fitValue() const;

protected:
    void _initWidget();
    void _slotIndexChanged(int index);

private:
    Ui::FitCanvasDlg *ui;

    QSizeF m_sDefaultSize;
    QSizeF m_sImageSize;
};

#endif // FITCANVASDLG_H

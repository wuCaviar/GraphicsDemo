#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QImage>
#include <atomic>

QT_BEGIN_NAMESPACE
class QThread;
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TiffExportWorker : public QObject
{
    Q_OBJECT
public:
    explicit TiffExportWorker(QObject *parent = nullptr);

public slots:
    void exportTiff(const QImage &img);

signals:
    void finished();
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    void tiffExportRequested(const QImage &img);

private slots:
    void onTiffExportFinished();

protected:
    void _initWidget();
    void _slotSelectColor();

    void _updateColor();
    void _updateCMYKColor();
    void _updateRGBColor();
    void _editRGB();
    void _editCMYK();
    void _updateColorLabel();
    void _requestTiffExport();

private:
    Ui::MainWindow *ui;
    QButtonGroup *group;
    QColor color;

    int flag = 0; // 0-rgb2cmyk 1-cmyk2rgb

    QThread *m_workerThread = nullptr;
    std::atomic<bool> m_exportPending{false};
};
#endif // MAINWINDOW_H

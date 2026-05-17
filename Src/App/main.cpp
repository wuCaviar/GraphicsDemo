#include <QApplication>
#include <QTranslator>
#include <QLocale>

#include "mainwindow.h"
#include "SingleInstance.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 加载中文翻译
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "GraphicsDemo_" + QLocale(locale).name();
        if (translator.load(":/translations/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    QString name = "com.athc.darwingtools";
    SingleInstance instance;
    if (SingleInstance::hasPrevious(name)) {
        return EXIT_SUCCESS;
    }

    instance.listen(name);

    // Create and Show the app
    MainWindow window;
    window.showMaximized();

    // Bring the Notes window to the front
    QObject::connect(&instance, &SingleInstance::newInstance, &window,
                     [&]() { (&window)->setMainWindowVisibility(true); });

    return a.exec();
}

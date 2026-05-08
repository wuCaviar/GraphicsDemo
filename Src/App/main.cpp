#include <QApplication>
#include <QTranslator>
#include <QLocale>

#include "mainwindow.h"

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

    MainWindow window;
    window.show();

    return a.exec();
}

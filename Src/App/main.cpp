#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QTimer>

#include "mainwindow.h"
#include "SingleInstance.h"
#include "AppConfig.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationVersion(ATHC_VERSION_STR_MAJ_MIN_MIC);

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

    // 加载全局配置
    AppConfig::instance().loadConfig();

    QString name = "com.athc.darwingtools";
    SingleInstance instance;
    if (SingleInstance::hasPrevious(name))
        return EXIT_SUCCESS;

    instance.listen(name);

    // Create and Show the app
    MainWindow window;
    window.showMaximized();

    // 延迟获取工具信息，避免阻塞界面显示
    QTimer::singleShot(1500, &window, [&]() { (&window)->getToolInfo(); });

    // Bring the window to the front
    QObject::connect(&instance, &SingleInstance::newInstance, &window,
                     [&]() { (&window)->setMainWindowVisibility(true); });

    return a.exec();
}

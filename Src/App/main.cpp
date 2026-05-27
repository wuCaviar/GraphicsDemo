#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QTimer>

#include "mainwindow.h"
#include "SingleInstance.h"
#include "AppConfig.h"

class ComboBoxAdjuster : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::Show) {
            if (auto *combo = qobject_cast<QComboBox *>(obj))
                combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        }
        return QObject::eventFilter(obj, event);
    }
};

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

    // 全局 QComboBox 弹出框自适应内容宽度
    a.installEventFilter(new ComboBoxAdjuster(&a));

    // 加载qss
    QFile f(":/qdarkstyle/light/lightstyle.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(f.readAll());
        f.close();
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

#include <QCoreApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("TIF processing tool");
    parser.addHelpOption();
    parser.addVersionOption();

    // -i 选项可多次出现，每次带一个文件
    QCommandLineOption inputOption(QStringList() << "i",
                                   "Input TIF file (can be used multiple times).",
                                   "file");
    parser.addOption(inputOption);

    // -o 选项，带一个输出文件
    QCommandLineOption outputOption(QStringList() << "o",
                                    "Output TIF file.",
                                    "file");
    parser.addOption(outputOption);

    parser.process(a);

    // 获取所有输入文件
    QStringList inputFiles = parser.values("i");
    // 获取输出文件（如果必填，可以检查）
    QString outputFile = parser.value("o");

    if (inputFiles.isEmpty() || outputFile.isEmpty()) {
        qCritical() << "Usage: xxx -i input1.tif [-i input2.tif ...] -o output.tif";
        return 1;
    }

    qDebug() << "Input files:" << inputFiles;
    qDebug() << "Output file:" << outputFile;
    return QCoreApplication::exec();
}

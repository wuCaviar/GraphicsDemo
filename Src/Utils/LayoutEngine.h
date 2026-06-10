#ifndef LAYOUTENGINE_H
#define LAYOUTENGINE_H

#include <QObject>
#include <QStringList>

class LayoutEngine : public QObject
{
    Q_OBJECT

public:
    explicit LayoutEngine(QObject *parent = nullptr);
    ~LayoutEngine();

    void start(const QStringList &imagePaths, int paperWidth, int elementInterval, int loopCount,
               int operateCount);
    void cancel();

    bool isRunning() const { return m_running; }
    double progress() const { return m_progress; }

signals:
    void started();
    void progressChanged(double percent);
    void finished(bool success, const QStringList &outputPaths, const QString &error);

private:
    void runLayout();
    bool copyFileToDir(const QString &srcPath, const QString &dstDir, QString &outDstPath);
    bool ensureDir(const QString &path);

    QString m_tmpDir;
    QString m_inDir;
    QString m_outDir;
    QStringList m_inputPaths;
    int m_paperWidth = 1600;
    int m_interval = 5;
    int m_loopCount = 8;
    int m_operateCount = 10;
    bool m_running = false;
    bool m_cancelRequested = false;
    double m_progress = 0.0;
};

#endif // LAYOUTENGINE_H

#include "LayoutEngine.h"
#include "opencvtest.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QThread>

#include <thread>
#include <string>

LayoutEngine::LayoutEngine(QObject *parent) : QObject(parent) { }

LayoutEngine::~LayoutEngine()
{
    cancel();
}

void LayoutEngine::start(const QStringList &imagePaths, int paperWidth, int elementInterval,
                         int loopCount, int operateCount)
{
    if (m_running)
        return;

    m_inputPaths = imagePaths;
    m_paperWidth = paperWidth;
    m_interval = elementInterval;
    m_loopCount = loopCount;
    m_operateCount = operateCount;
    m_cancelRequested = false;
    m_progress = 0.0;
    m_running = true;

    emit started();

    // Run layout in background using QtConcurrent-style approach via std::thread
    // We use std::thread because LayoutDAO blocks the calling thread
    std::thread([this]() { runLayout(); }).detach();
}

void LayoutEngine::cancel()
{
    if (m_running)
        m_cancelRequested = true;
}

bool LayoutEngine::ensureDir(const QString &path)
{
    QDir dir(path);
    if (!dir.exists())
        return dir.mkpath(".");
    return true;
}

bool LayoutEngine::copyFileToDir(const QString &srcPath, const QString &dstDir, QString &outDstPath)
{
    QFileInfo fi(srcPath);
    if (!fi.exists() || !fi.isFile())
        return false;

    QString baseName = fi.fileName();
    QString dstPath = dstDir + "/" + baseName;

    // Avoid name collision
    int counter = 0;
    while (QFileInfo::exists(dstPath)) {
        dstPath = dstDir + "/" + QString::number(++counter) + "_" + baseName;
    }

    if (QFile::copy(srcPath, dstPath)) {
        outDstPath = QDir::toNativeSeparators(dstPath);
        return true;
    }
    return false;
}

void LayoutEngine::runLayout()
{
    // --- 1. Setup temp directories ---
    QString appDir = QCoreApplication::applicationDirPath();
    m_tmpDir = appDir + "/tmp_layout";
    m_inDir = m_tmpDir + "/in/";

    // Each run gets a unique output subdirectory to avoid mixing results
    // from previous layouts (out/ is persistent so ImageItems' filePath()
    // references remain valid for export).
    QString runId = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_outDir = m_tmpDir + "/out/" + runId + "/";

    // Clean only the input copy directory — output directory is persistent.
    QDir(m_inDir).removeRecursively();
    if (!ensureDir(m_inDir) || !ensureDir(m_outDir)) {
        emit finished(false, { }, tr("Failed to create temporary directories: %1").arg(m_tmpDir));
        m_running = false;
        return;
    }

    // --- 2. Copy input files ---
    QStringList copiedPaths;
    for (const auto &path : m_inputPaths) {
        if (m_cancelRequested)
            break;
        QString dstPath;
        if (copyFileToDir(path, m_inDir, dstPath))
            copiedPaths << dstPath;
    }

    if (m_cancelRequested || copiedPaths.isEmpty()) {
        emit finished(false, { },
                      copiedPaths.isEmpty() ? tr("No valid input images found") : tr("Cancelled"));
        m_running = false;
        return;
    }

    // --- 3. Switch working directory to executable dir (for config.ini / title_imp) ---
    QString savedCwd = QDir::currentPath();
    QDir::setCurrent(appDir);
    QDir(appDir).mkpath("title_imp");

    // --- 4. Prepare LayoutInfo ---
    LayoutInfo info;
    info.paper_width = m_paperWidth;
    info.element_interval = m_interval / 2.0; // SpinBox value (mm) -> half
    info.loop_count = m_loopCount;
    info.operate_count = m_operateCount;
    info.bool_RemoveFile = false;
    info.bool_run = false;
    info.bool_dll_run = false;
    info.double_progress_real = 0;

    // Encode paths with system locale (GBK on Chinese Windows)
    QByteArray inBytes = m_inDir.toLocal8Bit();
    QByteArray outBytes = m_outDir.toLocal8Bit();
    std::string inPath(inBytes.constData(), inBytes.size());
    std::string outPath(outBytes.constData(), outBytes.size());

    info.intput_path = inPath.c_str();
    info.intput_length = static_cast<int>(inPath.length());
    info.output_path = outPath.c_str();
    info.output_length = static_cast<int>(outPath.length());

    // --- 5. Run LayoutDAO in a separate thread ---
    std::thread worker([&]() { LayoutDAO(&info); });

    // Wait for LayoutDAO to start
    while (!info.bool_run && !m_cancelRequested)
        QThread::msleep(10);

    if (m_cancelRequested) {
        info.bool_run = false;
        worker.join();
        QDir::setCurrent(savedCwd);
        emit finished(false, { }, tr("Cancelled"));
        m_running = false;
        return;
    }

    // --- 6. Poll progress ---
    // Cap at 99% to avoid QProgressDialog auto-reset at 100% (range 0-100).
    // Only reach 100% when fully done.
    while (info.bool_run) {
        if (m_cancelRequested) {
            info.bool_run = false;
            break;
        }
        double rawPercent = info.double_progress_real * 100.0;
        m_progress = qMin(rawPercent, 99.0);
        emit progressChanged(m_progress);
        QThread::msleep(200);
    }

    // Wait for the LayoutDAO thread to fully finish (bool_dll_run=false after 3s cooldown)
    worker.join();

    // Wait a bit more for bool_dll_run
    int waitCount = 0;
    while (info.bool_dll_run && waitCount < 50) {
        QThread::msleep(100);
        waitCount++;
    }

    // --- 7. Restore working directory ---
    QDir::setCurrent(savedCwd);

    // --- 8. Check for errors ---
    QString errorMsg;
    if (!info.error_info.empty()) {
        errorMsg = QString::fromLocal8Bit(info.error_info.c_str());
    }

    // --- 9. Collect output files ---
    QStringList outputFiles;
    QDir outDir(m_outDir);
    QStringList nameFilters = { "*.jpg", "*.jpeg", "*.tif", "*.tiff", "*.bmp", "*.png" };
    auto entries = outDir.entryInfoList(nameFilters, QDir::Files, QDir::Name);

    for (const auto &entry : entries) {
        outputFiles << entry.absoluteFilePath();
    }

    // Also check subdirectories for output files
    for (const auto &subDir : outDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir sd(subDir.absoluteFilePath());
        for (const auto &entry : sd.entryInfoList(nameFilters, QDir::Files, QDir::Name)) {
            outputFiles << entry.absoluteFilePath();
        }
    }

    // --- 10. Emit result ---
    // NOTE: Do NOT clean up tmp dir here — the caller needs to load output
    // files first. Cleanup is handled in onAutoLayout() after import.
    bool success = !outputFiles.isEmpty();
    emit finished(success, outputFiles, errorMsg);
    m_running = false;
}

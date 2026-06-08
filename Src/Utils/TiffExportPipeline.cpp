#include "TiffExportPipeline.h"
#include "AppConfig.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

namespace ImageUtils {

// ============================================================================
//  execute() — 主调度循环
// ============================================================================

ExportWorkerResult StripPipeline::execute(const QString &outputPath,
                                          const QList<SourceTiffInput> &sources,
                                          QList<CmykOverlay> &&overlays,
                                          const QSize &outputSize,
                                          const TiffExportSettings &settings,
                                          ProgressCallback progress,
                                          std::atomic<bool> *cancelFlag)
{
    ExportWorkerResult result;
    result.filePath = outputPath;

    // ---- 参数校验 ----
    if (sources.isEmpty() && overlays.isEmpty()) {
        result.errorMessage = "No content to export";
        return result;
    }

    const int outW = outputSize.width();
    const int outH = outputSize.height();
    if (outW <= 0 || outH <= 0) {
        result.errorMessage = "Invalid output size";
        return result;
    }

    const int totalStrips =
        (outH + m_config.stripHeight - 1) / m_config.stripHeight;

    // ---- 0. 预打开所有源 TIFF (持久化，避免每 strip 重复 open/close) ----
    std::vector<SourceReader> readers(sources.size());
    for (int si = 0; si < sources.size(); ++si) {
        if (!readers[si].open(sources[si].filePath)) {
            result.errorMessage = readers[si].errorString();
            return result;
        }
    }

    // ---- 1. 创建环形缓冲区 ----
    std::vector<StripSlot> stripSlots(m_config.pipelineDepth);
    for (int i = 0; i < m_config.pipelineDepth; ++i)
        stripSlots[i].state.store(StripSlot::State::Done,
                                  std::memory_order_release);

    // 条件变量：各线程等待 slot 状态转换
    std::mutex cvMutex;
    std::condition_variable cv;

    // 流水线共享状态
    std::atomic<bool> pipelineError{ false };
    std::mutex errorMutex;
    std::string errorMsg;

    // 进度：三个阶段的 strip 序号
    std::atomic<int> producedStrips{ 0 };
    std::atomic<int> organizedStrips{ 0 };
    std::atomic<int> writtenStrips{ 0 };

    // ---- 进度报告集成 ----
    // 权重: Producer 0-30%, Organizer 30-80%, Consumer 80-100%
    auto reportProgress = [&](int produced, int organized, int written) {
        if (!progress)
            return;
        int pct = 0;
        if (produced > 0)
            pct = std::min(produced * 30 / totalStrips, 30);
        if (organized > 0)
            pct =
                std::max(pct, 30 + std::min(organized * 50 / totalStrips, 50));
        if (written > 0)
            pct = std::max(pct, 80 + std::min(written * 20 / totalStrips, 20));
        pct = std::min(pct, 100);
        progress(pct);
    };

    reportProgress(0, 0, 0);

    if (isCancelled(cancelFlag)) {
        result.errorMessage = "Export cancelled";
        return result;
    }

    // ---- 2. 打开输出 TIFF ----
    ScopedTiffHandle outTif(outputPath.toLocal8Bit().constData(), "w");
    if (!outTif) {
        result.errorMessage =
            QString("Cannot create output TIFF: %1").arg(outputPath);
        return result;
    }

    // ---- 3. 设置 TIFF tags ----
    TIFFSetField(outTif.get(), TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(outW));
    TIFFSetField(outTif.get(), TIFFTAG_IMAGELENGTH,
                 static_cast<uint32_t>(outH));
    TIFFSetField(outTif.get(), TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(outTif.get(), TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(outTif.get(), TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(outTif.get(), TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(outTif.get(), TIFFTAG_INKSET, INKSET_CMYK);
    TIFFSetField(outTif.get(), TIFFTAG_NUMBEROFINKS, 4);
    TIFFSetField(outTif.get(), TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(outTif.get(), TIFFTAG_COMPRESSION, settings.compression);
    TIFFSetField(outTif.get(), TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    TIFFSetField(outTif.get(), TIFFTAG_ROWSPERSTRIP,
                 TIFFDefaultStripSize(outTif.get(), -1));
    TIFFSetField(outTif.get(), TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(outTif.get(), TIFFTAG_XRESOLUTION,
                 static_cast<float>(settings.dpi));
    TIFFSetField(outTif.get(), TIFFTAG_YRESOLUTION,
                 static_cast<float>(settings.dpi));
    TIFFSetField(outTif.get(), TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(outTif.get(), TIFFTAG_SOFTWARE, "GraphicsDemo");
    TIFFSetField(outTif.get(), TIFFTAG_SUBFILETYPE, static_cast<uint32_t>(0));

    QString dateTime =
        QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    TIFFSetField(outTif.get(), TIFFTAG_DATETIME, dateTime.toUtf8().constData());

    // ICC Profile
    QString iccPath = settings.iccProfilePath;
    if (iccPath.isEmpty())
        iccPath = AppConfig::instance().cmykIccPath();
    if (!iccPath.isEmpty()) {
        QFile iccFile(iccPath);
        if (iccFile.open(QIODevice::ReadOnly)) {
            QByteArray iccData = iccFile.readAll();
            TIFFSetField(outTif.get(), TIFFTAG_ICCPROFILE,
                         static_cast<uint32_t>(iccData.size()),
                         iccData.constData());
            iccFile.close();
        }
    }

    // ---- 4. 启动流水线线程 ----

    // Producer 线程
    std::thread producer([&]() {
        for (int k = 0; k < totalStrips; ++k) {
            if (isCancelled(cancelFlag)
                || pipelineError.load(std::memory_order_acquire))
                break;

            // 寻找 Done 状态的 slot
            int slotIdx = -1;
            {
                std::unique_lock lock(cvMutex);
                cv.wait(lock, [&]() -> bool {
                    for (int i = 0; i < m_config.pipelineDepth; ++i) {
                        if (stripSlots[i].state.load(std::memory_order_acquire)
                            == StripSlot::State::Done)
                            return true;
                    }
                    return isCancelled(cancelFlag)
                           || pipelineError.load(std::memory_order_acquire);
                });
                if (isCancelled(cancelFlag)
                    || pipelineError.load(std::memory_order_acquire))
                    break;
                for (int i = 0; i < m_config.pipelineDepth; ++i) {
                    if (stripSlots[i].state.load(std::memory_order_acquire)
                        == StripSlot::State::Done) {
                        slotIdx = i;
                        break;
                    }
                }
            }
            if (slotIdx < 0)
                break;

            StripSlot &slot = stripSlots[slotIdx];
            slot.stripIndex = k;
            slot.startRow = k * m_config.stripHeight;
            slot.numRows = std::min(m_config.stripHeight, outH - slot.startRow);
            slot.state.store(StripSlot::State::Idle, std::memory_order_release);

            producerStage(slot, sources, outputSize, readers, cancelFlag,
                          pipelineError, errorMutex, errorMsg);

            if (isCancelled(cancelFlag))
                break;
            if (pipelineError.load(std::memory_order_acquire))
                break;

            producedStrips.store(k + 1, std::memory_order_release);
            slot.state.store(StripSlot::State::SourceReady,
                             std::memory_order_release);
            cv.notify_all();
        }
        cv.notify_all();
    });

    // Organizer 线程
    std::thread organizer([&]() {
        for (int k = 0; k < totalStrips; ++k) {
            if (isCancelled(cancelFlag)
                || pipelineError.load(std::memory_order_acquire))
                break;

            int slotIdx = -1;
            {
                std::unique_lock lock(cvMutex);
                cv.wait(lock, [&]() -> bool {
                    for (int i = 0; i < m_config.pipelineDepth; ++i) {
                        if (stripSlots[i].state.load(std::memory_order_acquire)
                            == StripSlot::State::SourceReady)
                            return true;
                    }
                    int produced =
                        producedStrips.load(std::memory_order_acquire);
                    return produced >= totalStrips || isCancelled(cancelFlag)
                           || pipelineError.load(std::memory_order_acquire);
                });
                if (isCancelled(cancelFlag)
                    || pipelineError.load(std::memory_order_acquire))
                    break;
                for (int i = 0; i < m_config.pipelineDepth; ++i) {
                    if (stripSlots[i].state.load(std::memory_order_acquire)
                        == StripSlot::State::SourceReady) {
                        slotIdx = i;
                        break;
                    }
                }
            }
            if (slotIdx < 0)
                break;

            StripSlot &slot = stripSlots[slotIdx];
            organizerStage(slot, overlays, outW, outH, cancelFlag);

            if (isCancelled(cancelFlag))
                break;
            if (pipelineError.load(std::memory_order_acquire))
                break;

            organizedStrips.store(k + 1, std::memory_order_release);
            slot.state.store(StripSlot::State::Composited,
                             std::memory_order_release);
            cv.notify_all();
        }
        cv.notify_all();
    });

    // Consumer 在主线程（调用方线程）中运行，避免 libtiff 多线程写入问题
    for (int k = 0; k < totalStrips; ++k) {
        if (isCancelled(cancelFlag)
            || pipelineError.load(std::memory_order_acquire))
            break;

        int slotIdx = -1;
        {
            std::unique_lock lock(cvMutex);
            cv.wait(lock, [&]() -> bool {
                for (int i = 0; i < m_config.pipelineDepth; ++i) {
                    if (stripSlots[i].state.load(std::memory_order_acquire)
                        == StripSlot::State::Composited)
                        return true;
                }
                int organized = organizedStrips.load(std::memory_order_acquire);
                return organized >= totalStrips || isCancelled(cancelFlag)
                       || pipelineError.load(std::memory_order_acquire);
            });
            if (isCancelled(cancelFlag)
                || pipelineError.load(std::memory_order_acquire))
                break;
            for (int i = 0; i < m_config.pipelineDepth; ++i) {
                if (stripSlots[i].state.load(std::memory_order_acquire)
                    == StripSlot::State::Composited) {
                    slotIdx = i;
                    break;
                }
            }
        }
        if (slotIdx < 0)
            break;

        StripSlot &slot = stripSlots[slotIdx];

        consumerStage(slot, outTif.get(), outW, outH, writtenStrips,
                      totalStrips, cancelFlag, pipelineError, errorMutex,
                      errorMsg, progress);

        if (pipelineError.load(std::memory_order_acquire))
            break;

        slot.state.store(StripSlot::State::Done, std::memory_order_release);
        cv.notify_all();

        // 报告进度
        reportProgress(producedStrips.load(std::memory_order_acquire),
                       organizedStrips.load(std::memory_order_acquire),
                       writtenStrips.load(std::memory_order_acquire));

        if (isCancelled(cancelFlag))
            break;
    }

    // ---- 5. 等待线程结束 ----
    producer.join();
    organizer.join();

    // ---- 6. 处理结果 ----
    if (isCancelled(cancelFlag)) {
        outTif.close();
        QFile::remove(outputPath);
        result.errorMessage = "Export cancelled";
        progress(0);
        return result;
    }

    if (pipelineError.load(std::memory_order_acquire)) {
        outTif.close();
        QFile::remove(outputPath);
        {
            std::lock_guard<std::mutex> lock(errorMutex);
            result.errorMessage = QString::fromStdString(errorMsg);
        }
        return result;
    }

    outTif.close();
    progress(100);

    result.success = true;
    return result;
}

// ============================================================================
//  producerStage() — 并行读取源 TIFF 的 strip 数据
// ============================================================================

void StripPipeline::producerStage(StripSlot &slot,
                                  const QList<SourceTiffInput> &sources,
                                  const QSize &outSize,
                                  std::vector<SourceReader> &readers,
                                  std::atomic<bool> *cancelFlag,
                                  std::atomic<bool> &pipelineError,
                                  std::mutex &errorMutex, std::string &errorMsg)
{
    const int outH = outSize.height();
    const int stripY0 = slot.startRow;
    const int stripY1 = slot.startRow + slot.numRows;

    slot.sourcesData.resize(sources.size());
    slot.sourcesExpected = static_cast<int>(sources.size());
    slot.sourcesCompleted.store(0, std::memory_order_release);

    QSemaphore sem(m_config.maxReaderThreads);
    QList<QFuture<void>> futures;

    for (int si = 0; si < sources.size(); ++si) {
        if (isCancelled(cancelFlag))
            return;

        futures.append(QtConcurrent::run([&, si]() {
            sem.acquire();

            auto &sd = slot.sourcesData[si];
            const auto &src = sources[si];

            // 检查该源是否覆盖本 strip
            int srcRectY0 =
                std::max(0, static_cast<int>(std::floor(src.outputRect.top())));
            int srcRectY1 = std::min(
                outH, static_cast<int>(std::ceil(src.outputRect.bottom())));
            if (srcRectY1 <= stripY0 || srcRectY0 >= stripY1) {
                sd.hasData = false;
                sd.zOrder = src.zOrder;
                sd.outputRect = src.outputRect;
                slot.sourcesCompleted.fetch_add(1, std::memory_order_release);
                sem.release();
                return;
            }

            // 使用持久化的 SourceReader，不再每 strip 重新打开
            SourceReader &reader = readers[si];
            if (!reader.isValid()) {
                std::lock_guard<std::mutex> lock(errorMutex);
                errorMsg = reader.errorString().toStdString();
                pipelineError.store(true, std::memory_order_release);
                slot.sourcesCompleted.fetch_add(1, std::memory_order_release);
                sem.release();
                return;
            }

            // 只读阶段：计算映射参数
            const int srcWidth = static_cast<int>(reader.width());
            const int srcRectW = static_cast<int>(std::ceil(src.outputRect.width()));
            const int srcRectH = static_cast<int>(std::ceil(src.outputRect.height()));

            // 输出行 → 源行映射（浮点缩放）
            double scaleY =
                static_cast<double>(reader.height()) / srcRectH;
            double scaleX =
                static_cast<double>(srcWidth) / srcRectW;

            int stripSrcY0 = std::max(stripY0, srcRectY0);
            int stripSrcY1 = std::min(stripY1, srcRectY1);

            // 映射到源图像行号（取整覆盖所有涉及的源行）
            // srcY1F 用 ceil 保证不遗漏上边界
            double srcY0F = (stripSrcY0 - srcRectY0) * scaleY;
            double srcY1F = (stripSrcY1 - srcRectY0) * scaleY;
            int sourceRow0 = std::clamp(static_cast<int>(std::floor(srcY0F)), 0,
                                        static_cast<int>(reader.height()) - 1);
            int sourceRow1 = std::clamp(static_cast<int>(std::ceil(srcY1F) - 1), 0,
                                        static_cast<int>(reader.height()) - 1);

            int numRows = sourceRow1 - sourceRow0 + 1;
            if (numRows <= 0) {
                sd.hasData = false;
                sd.zOrder = src.zOrder;
                sd.outputRect = src.outputRect;
                slot.sourcesCompleted.fetch_add(1, std::memory_order_release);
                sem.release();
                return;
            }

            // 分配该源的 strip 缓冲区
            try {
                sd.cmykRows.resize(static_cast<size_t>(reader.width()) * numRows
                                   * 4);
            } catch (const std::bad_alloc &) {
                std::lock_guard<std::mutex> lock(errorMutex);
                errorMsg = "Not enough memory for source strip data";
                pipelineError.store(true, std::memory_order_release);
                slot.sourcesCompleted.fetch_add(1, std::memory_order_release);
                sem.release();
                return;
            }

            sd.startSourceRow = sourceRow0;
            sd.numSourceRows = numRows;
            sd.hasData = true;
            sd.outputRect = src.outputRect;
            sd.zOrder = src.zOrder;
            sd.scaleX = scaleX;
            sd.scaleY = scaleY;

            // 逐行读取并转换为 CMYK（预分配行缓冲区，避免每行 malloc）
            size_t rowBytes = static_cast<size_t>(reader.width()) * 4;
            std::vector<uint8_t> rowBuf(reader.width() * 4); // 一次性分配
            for (int row = sourceRow0; row <= sourceRow1; ++row) {
                if (isCancelled(cancelFlag))
                    break;

                if (!reader.readAndConvertRow(row, rowBuf)) {
                    std::memset(rowBuf.data(), 0, rowBytes);
                }
                int localIdx = row - sourceRow0;
                std::memcpy(sd.cmykRows.data() + localIdx * rowBytes,
                            rowBuf.data(), rowBytes);
            }

            slot.sourcesCompleted.fetch_add(1, std::memory_order_release);
            sem.release();
        }));
    }

    // 等待所有 reader 完成
    for (auto &f : futures)
        f.waitForFinished();
}

// ============================================================================
//  organizerStage() — 逐行合成 strip
// ============================================================================

void StripPipeline::organizerStage(StripSlot &slot,
                                   const QList<CmykOverlay> &overlays,
                                   int outWidth, int outHeight,
                                   std::atomic<bool> *cancelFlag)
{
    const int numRows = slot.numRows;
    const int stripY0 = slot.startRow;

    // 分配合成输出缓冲区
    try {
        slot.compositedRows.resize(static_cast<size_t>(numRows) * outWidth * 4);
    } catch (const std::bad_alloc &) {
        slot.compositedRows.clear();
        return;
    }

    // 收集覆盖本 strip 的 overlays
    std::vector<const CmykOverlay *> activeOverlays;
    for (const auto &ov : overlays) {
        int ovY0 =
            std::max(0, static_cast<int>(std::floor(ov.outputRect.top())));
        int ovY1 = std::min(
            outHeight, static_cast<int>(std::ceil(ov.outputRect.bottom())));
        if (ovY1 <= stripY0 || ovY0 >= stripY0 + numRows)
            continue;
        activeOverlays.push_back(&ov);
    }

    // 构建排序后的合成参与者列表
    std::vector<CompositorParticipant> participants;

    // 源数据
    for (size_t si = 0; si < slot.sourcesData.size(); ++si) {
        if (!slot.sourcesData[si].hasData)
            continue;
        CompositorParticipant p;
        p.type = CompositorParticipant::Type::SourceReader;
        p.sourceData = &slot.sourcesData[si];
        p.readerIndex = static_cast<int>(si);
        p.zOrder = slot.sourcesData[si].zOrder;
        participants.push_back(p);
    }

    // Overlays
    for (const auto *ov : activeOverlays) {
        CompositorParticipant p;
        p.type = CompositorParticipant::Type::Overlay;
        p.overlay = ov;
        p.zOrder = ov->zOrder;
        participants.push_back(p);
    }

    // 按 z-order 排序
    std::sort(
        participants.begin(), participants.end(),
        [](const CompositorParticipant &a, const CompositorParticipant &b) {
            return a.zOrder < b.zOrder;
        });

    // 逐行合成
    for (int localY = 0; localY < numRows; ++localY) {
        if (isCancelled(cancelFlag))
            return;

        int globalY = stripY0 + localY;
        uint8_t *outRow = slot.compositedRows.data()
                          + static_cast<size_t>(localY) * outWidth * 4;

        // 初始化为白色 (CMYK 0,0,0,0)
        std::memset(outRow, 0, static_cast<size_t>(outWidth) * 4);

        // 按 z-order 叠加每个参与者
        for (const auto &p : participants) {
            if (p.type == CompositorParticipant::Type::SourceReader) {
                // ============ 源 TIFF 数据（带缩放映射） ============
                const auto *sd = p.sourceData;
                if (!sd || !sd->hasData)
                    continue;

                int sdRectY0 = std::max(
                    0, static_cast<int>(std::floor(sd->outputRect.top())));
                int sdRectY1 = std::min(
                    outHeight,
                    static_cast<int>(std::ceil(sd->outputRect.bottom())));
                if (globalY < sdRectY0 || globalY >= sdRectY1)
                    continue;

                // 输出行 → 源行映射（浮点，双线性插值）
                // srcY 是浮点源行号，映射到 localRow0..localRow1 范围
                double srcY =
                    (globalY - sd->outputRect.top()) * sd->scaleY;
                int srcY0 = static_cast<int>(std::floor(srcY));
                int srcY1 = std::min(srcY0 + 1,
                                     static_cast<int>(sd->startSourceRow
                                                      + sd->numSourceRows) - 1);
                double yFrac = srcY - srcY0;

                int localRow0 = std::clamp(
                    srcY0 - sd->startSourceRow, 0, sd->numSourceRows - 1);
                int localRow1 = std::clamp(
                    srcY1 - sd->startSourceRow, 0, sd->numSourceRows - 1);

                // 源图像像素宽度 (reader.width())，不是输出目标宽度
                // sd->cmykRows 每行 = reader.width() * 4 字节
                // sd->outputRect.width() 是输出像素坐标中的宽度

                int bx0 = std::max(
                    0, static_cast<int>(std::floor(sd->outputRect.left())));
                int bx1 = std::min(outWidth, static_cast<int>(std::ceil(
                                                 sd->outputRect.right())));

                // sd->cmykRows 中每行的像素数 = reader.width() (原始TIFF宽度)
                // 这里无法直接访问 reader，用 sd->scaleX 反推:
                // scaleX = reader.width() / outputRect.width()
                // => reader.width() = scaleX * outputRect.width()
                int storedWidth = static_cast<int>(
                    std::ceil(sd->outputRect.width() * sd->scaleX));

                const uint8_t *srcRowData0 =
                    sd->cmykRows.data()
                    + static_cast<size_t>(localRow0) * storedWidth * 4;
                const uint8_t *srcRowData1 =
                    (localRow1 == localRow0)
                        ? srcRowData0
                        : sd->cmykRows.data()
                            + static_cast<size_t>(localRow1) * storedWidth * 4;

                // 逐像素 X 方向双线性插值
                // sd->scaleX = originalTIFF.width / outputRect.width
                for (int x = bx0; x < bx1; ++x) {
                    double srcX = (x - sd->outputRect.left()) * sd->scaleX;
                    int srcX0 = static_cast<int>(std::floor(srcX));
                    int srcX1 = std::min(srcX0 + 1, storedWidth - 1);
                    double xFrac = srcX - srcX0;

                    int i0 = srcX0 * 4;
                    int i1 = srcX1 * 4;

                    for (int c = 0; c < 4; ++c) {
                        double v0 = srcRowData0[i0 + c] * (1.0 - xFrac)
                                    + srcRowData0[i1 + c] * xFrac;
                        double v1 = srcRowData1[i0 + c] * (1.0 - xFrac)
                                    + srcRowData1[i1 + c] * xFrac;
                        double v = v0 * (1.0 - yFrac) + v1 * yFrac;
                        outRow[x * 4 + c] = static_cast<uint8_t>(
                            qBound(0.0, v, 255.0));
                    }
                }
            } else {
                // ============ Overlay（1:1 直接拷贝） ============
                const auto *ov = p.overlay;
                if (!ov)
                    continue;
                if (globalY < ov->outputRect.top()
                    || globalY >= ov->outputRect.bottom())
                    continue;

                int bx0 = std::max(
                    0, static_cast<int>(std::floor(ov->outputRect.left())));
                int bx1 = std::min(outWidth, static_cast<int>(std::ceil(
                                                 ov->outputRect.right())));

                // 1:1 映射：overlay 渲染尺寸 == outputRect 尺寸
                int srcRow =
                    globalY
                    - static_cast<int>(std::floor(ov->outputRect.top()));
                const uint8_t *srcRowData =
                    ov->data.data()
                    + static_cast<size_t>(srcRow) * ov->width * 4;
                std::memcpy(
                    outRow + bx0 * 4,
                    srcRowData
                        + (bx0 - static_cast<int>(ov->outputRect.left())) * 4,
                    static_cast<size_t>(bx1 - bx0) * 4);
            }
        }
    }
}

// ============================================================================
//  consumerStage() — 写入合成行到输出 TIFF
// ============================================================================

void StripPipeline::consumerStage(StripSlot &slot, TIFF *tif, int outWidth,
                                  int /*outH*/, std::atomic<int> &writtenStrips,
                                  int /*totalStrips*/,
                                  std::atomic<bool> *cancelFlag,
                                  std::atomic<bool> &pipelineError,
                                  std::mutex &errorMutex, std::string &errorMsg,
                                  ProgressCallback /*progress*/)
{
    const size_t rowStride = static_cast<size_t>(outWidth) * 4;
    const int numRows = slot.numRows;

    for (int localY = 0; localY < numRows; ++localY) {
        if (isCancelled(cancelFlag))
            return;

        int globalY = slot.startRow + localY;
        const uint8_t *rowData =
            slot.compositedRows.data() + localY * rowStride;

        if (TIFFWriteScanline(tif, const_cast<uint8_t *>(rowData), globalY, 0)
            != 1) {
            std::lock_guard<std::mutex> lock(errorMutex);
            errorMsg = "Write error at row " + std::to_string(globalY);
            pipelineError.store(true, std::memory_order_release);
            return;
        }
    }

    // 写入完成，释放合成缓冲区
    slot.compositedRows.clear();
    slot.compositedRows.shrink_to_fit();

    // 释放源数据缓冲区
    for (auto &sd : slot.sourcesData) {
        sd.cmykRows.clear();
        sd.cmykRows.shrink_to_fit();
    }

    writtenStrips.fetch_add(1, std::memory_order_release);
}

// ============================================================================
//  validateAgainstLegacy() — 像素级对比验证（调试用）
// ============================================================================

ExportVerificationResult StripPipeline::validateAgainstLegacy(
    const QString &outputPath, const QList<SourceTiffInput> &sources,
    QList<CmykOverlay> overlays, const QSize &outputSize,
    const TiffExportSettings &settings, ProgressCallback progress)
{
    ExportVerificationResult vResult;

    if (progress)
        progress(0);

    // ---- Step 1: 复制两份 overlays (execute 和 exportTiff 都是右值消费) ----
    auto copyOverlays =
        [](const QList<CmykOverlay> &src) -> QList<CmykOverlay> {
        QList<CmykOverlay> dst;
        for (const auto &ov : src) {
            CmykOverlay copy;
            copy.data = ov.data; // 深拷贝像素
            copy.width = ov.width;
            copy.height = ov.height;
            copy.outputRect = ov.outputRect;
            copy.zOrder = ov.zOrder;
            dst.append(std::move(copy));
        }
        return dst;
    };

    QList<CmykOverlay> legacyOverlays = copyOverlays(overlays);
    // overlays 留给 pipeline.execute() 使用

    // ---- Step 2: Run pipeline → outputPath ----
    StripPipeline::Config cfg;
    StripPipeline pipeline(cfg);
    ExportWorkerResult pipeResult =
        pipeline.execute(outputPath, sources, std::move(overlays), outputSize,
                         settings, progress);
    if (!pipeResult.success) {
        vResult.errorMessage =
            QString("Pipeline failed: %1").arg(pipeResult.errorMessage);
        return vResult;
    }

    // ---- Step 3: Run legacy → outputPath + ".legacy.tif" ----
    QString legacyPath = outputPath + ".legacy.tif";
    ExportWorkerResult legacyResult =
        exportTiff(legacyPath, sources, std::move(legacyOverlays), outputSize,
                   settings, progress);
    if (!legacyResult.success) {
        vResult.errorMessage =
            QString("Legacy failed: %1").arg(legacyResult.errorMessage);
        QFile::remove(legacyPath);
        return vResult;
    }

    if (progress)
        progress(95);

    // ---- Step 4: 逐像素比较两个 TIFF 文件 ----
    QByteArray pipePathBytes = outputPath.toLocal8Bit();
    QByteArray legacyPathBytes = legacyPath.toLocal8Bit();
    TIFF *pipeTif = TIFFOpen(pipePathBytes.constData(), "r");
    TIFF *legacyTif = TIFFOpen(legacyPathBytes.constData(), "r");

    if (!pipeTif || !legacyTif) {
        if (pipeTif)
            TIFFClose(pipeTif);
        if (legacyTif)
            TIFFClose(legacyTif);
        vResult.errorMessage = "Cannot open output files for comparison";
        QFile::remove(legacyPath);
        return vResult;
    }

    uint32_t pipeW = 0, pipeH = 0, legacyW = 0, legacyH = 0;
    TIFFGetField(pipeTif, TIFFTAG_IMAGEWIDTH, &pipeW);
    TIFFGetField(pipeTif, TIFFTAG_IMAGELENGTH, &pipeH);
    TIFFGetField(legacyTif, TIFFTAG_IMAGEWIDTH, &legacyW);
    TIFFGetField(legacyTif, TIFFTAG_IMAGELENGTH, &legacyH);

    if (pipeW != legacyW || pipeH != legacyH) {
        vResult.errorMessage =
            QString("Dimension mismatch: pipeline %1x%2, legacy %3x%4")
                .arg(pipeW)
                .arg(pipeH)
                .arg(legacyW)
                .arg(legacyH);
        TIFFClose(pipeTif);
        TIFFClose(legacyTif);
        QFile::remove(legacyPath);
        return vResult;
    }

    vResult.totalPixels = static_cast<int>(pipeW) * static_cast<int>(pipeH);
    tsize_t scanlineSize = static_cast<tsize_t>(pipeW) * 4;
    std::vector<uint8_t> pipeBuf(scanlineSize);
    std::vector<uint8_t> legacyBuf(scanlineSize);

    for (uint32_t y = 0; y < pipeH; ++y) {
        if (TIFFReadScanline(pipeTif, pipeBuf.data(), y, 0) < 0)
            break;
        if (TIFFReadScanline(legacyTif, legacyBuf.data(), y, 0) < 0)
            break;

        for (int x = 0; x < static_cast<int>(pipeW); ++x) {
            bool diff = false;
            for (int c = 0; c < 4; ++c) {
                int off = x * 4 + c;
                int d = std::abs(static_cast<int>(pipeBuf[off])
                                 - static_cast<int>(legacyBuf[off]));
                if (d > 0) {
                    diff = true;
                    if (d > vResult.maxChannelDiff)
                        vResult.maxChannelDiff = d;
                }
            }
            if (diff)
                vResult.differentPixels++;
        }
    }

    TIFFClose(pipeTif);
    TIFFClose(legacyTif);

    // ---- Step 5: 报告结果 ----
    vResult.passed = (vResult.differentPixels == 0);

    if (vResult.passed) {
        // 验证通过，删除 legacy 文件
        QFile::remove(legacyPath);
    } else {
        vResult.errorMessage =
            QString("Mismatch: %1/%2 pixels differ, max channel diff=%3")
                .arg(vResult.differentPixels)
                .arg(vResult.totalPixels)
                .arg(vResult.maxChannelDiff);
    }

    if (progress)
        progress(100);
    return vResult;
}

} // namespace ImageUtils

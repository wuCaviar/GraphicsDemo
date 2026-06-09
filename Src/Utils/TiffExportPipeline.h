#ifndef TIFFEXPORTPIPELINE_H
#define TIFFEXPORTPIPELINE_H

#include "ImageWorker.h"
#include "SourceReader.h"

#include <QSemaphore>
#include <QThread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace ImageUtils {

// ============================================================================
// StripSlot — 流水线环形缓冲区中的一个 strip 工作单元
//
// 状态机: Idle → SourceReady → Composited → Done → (recycle to Idle)
// 每个状态只有一个写者（Producer/Organizer/Consumer），
// 转换使用 std::atomic 保证可见性，无需互斥锁。
// ============================================================================

struct StripSlot
{
    enum class State
    {
        Idle, // 空闲，可被 Producer 使用
        SourceReady, // Producer 完成（源数据就绪），Organizer 可处理
        Composited, // Organizer 完成（合成完毕），Consumer 可写入
        Done // Consumer 完成，可回收给 Producer 复用
    };

    int stripIndex = -1; // 全局 strip 序号 [0, totalStrips)
    int startRow = 0; // 输出起始行号
    int numRows = 0; // 本 strip 行数

    std::atomic<State> state; // 由 execute() 通过 store() 初始化

    // ---- Producer 产出：每个源的 strip 级数据 ----
    struct SourceStripData
    {
        std::vector<uint8_t> cmykRows; // sourcePixelWidth × numSourceRows × 4 bytes
        int startSourceRow = 0; // 该数据在源图像中的起始行
        int numSourceRows = 0; // 行数
        bool hasData = false; // 该源是否覆盖本 strip
        QString error;
        QRectF
            outputRect; // 该源在输出图像中的 pixel rect
        int zOrder = 0; // z-order 排序键
        double scaleX = 1.0; // 输出像素 → 源像素 X 方向缩放系数 (width_src / width_dst)
        double scaleY = 1.0; // 输出像素 → 源像素 Y 方向缩放系数 (height_src / height_dst)
    };
    std::vector<SourceStripData> sourcesData;

    // ---- Organizer 产出：合成后的输出行 ----
    std::vector<uint8_t> compositedRows; // numRows × outWidth × 4 bytes

    // ---- Producer 并行 reader 完成计数 ----
    std::atomic<int> sourcesCompleted{ 0 };
    int sourcesExpected = 0;
};

// ============================================================================
// ExportVerificationResult — 像素级验证结果（前向声明供 StripPipeline 使用）
// ============================================================================

struct ExportVerificationResult
{
    bool passed = false;
    int totalPixels = 0;
    int differentPixels = 0;
    int maxChannelDiff = 0;
    QString errorMessage;
};

// ============================================================================
// StripPipeline — Strip-Based 三级流水线调度器
//
// 三个独立线程 (Producer / Organizer / Consumer) 通过 StripSlot 环形缓冲
// 流水线处理输出 strip。内存占用由 stripHeight 和 pipelineDepth 控制。
// ============================================================================

class StripPipeline
{
public:
    struct Config
    {
        int stripHeight; // 每 strip 行数
        int pipelineDepth; // 环形缓冲区大小
        int maxReaderThreads; // Producer 阶段最大并行 reader 数

        Config()
        {
            stripHeight = 256; // 每 strip 行数
            pipelineDepth = 3; // 环形缓冲区大小
            maxReaderThreads = 4; // Producer 阶段最大并行 reader 数
        }
    };

    explicit StripPipeline(Config cfg = { }) : m_config(cfg) { }

    // 同步执行流水线导出。阻塞直到完成或出错或取消。
    ExportWorkerResult execute(const QString &outputPath, const QList<SourceTiffInput> &sources,
                               QList<CmykOverlay> &&overlays, const QSize &outputSize,
                               const TiffExportSettings &settings,
                               ProgressCallback progress = nullptr,
                               std::atomic<bool> *cancelFlag = nullptr);

private:
    Config m_config;

    bool isCancelled(std::atomic<bool> *cancelFlag) const
    {
        return cancelFlag && cancelFlag->load(std::memory_order_relaxed);
    }

    // ---- 阶段函数 ----
    // readers: execute() 生命周期内持久化的 SourceReader 数组，避免每 strip 重复 open
    void producerStage(StripSlot &slot, const QList<SourceTiffInput> &sources, const QSize &outSize,
                       std::vector<SourceReader> &readers, std::atomic<bool> *cancelFlag,
                       std::atomic<bool> &pipelineError, std::mutex &errorMutex,
                       std::string &errorMsg);

    void organizerStage(StripSlot &slot, const QList<CmykOverlay> &overlays, int outWidth,
                        int outHeight, std::atomic<bool> *cancelFlag);

    void consumerStage(StripSlot &slot, TIFF *tif, int outWidth, int outH,
                       std::atomic<int> &writtenStrips, int totalStrips,
                       std::atomic<bool> *cancelFlag, std::atomic<bool> &pipelineError,
                       std::mutex &errorMutex, std::string &errorMsg, ProgressCallback progress);

    // 像素级验证：与 Legacy exportTiff 全缓冲实现对比
    // pipeline 输出写入 outputPath，legacy 输出写入 outputPath + ".legacy.tif"
    // 然后逐像素比较两个文件的 CMYK 通道值
    static ExportVerificationResult
    validateAgainstLegacy(const QString &outputPath, const QList<SourceTiffInput> &sources,
                          QList<CmykOverlay> overlays, // 传值，不会消费原数据
                          const QSize &outputSize, const TiffExportSettings &settings,
                          ProgressCallback progress = nullptr);
};

// ============================================================================
// CompositorParticipant — 合成阶段的数据源（排序用）
// ============================================================================

struct CompositorParticipant
{
    enum class Type
    {
        SourceReader, // 来自 SourceReader 的 strip 级源数据
        Overlay // 主线程预渲染的 CMYK overlay 数据
    };

    Type type;
    const StripSlot::SourceStripData *sourceData = nullptr; // type==SourceReader
    const CmykOverlay *overlay = nullptr; // type==Overlay
    int zOrder = 0;
    int readerIndex = -1;
};

} // namespace ImageUtils

#endif // TIFFEXPORTPIPELINE_H

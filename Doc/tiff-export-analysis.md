# TIFF 导出流程分析与优化记录

> 分析日期：2026-06-01（基于 feature/dpi 分支）
> 涉及文件：`Src/UI/TiffExportEngine.cpp/h`、`Src/Utils/TiffExportPipeline.cpp/h`、`Src/Utils/SourceReader.cpp/h`、`Src/Utils/ImageWorker.cpp/h`、`Src/Utils/ScopedTiffHandle.h`、`Src/Utils/ImageUtils.cpp/h`、`Src/UI/ExportImageDialog.cpp/h`、`Libs/ColorTrans/colortransform.cpp/h`

---

## 一、架构总览

系统采用 **Engine + StripPipeline** 两层架构，支持多源 TIFF 合成 + 矢量 Overlay 叠加，输出 CMYK TIFF。

```
MainWindow::onExportImage()              [主线程，UI 交互]
  │
  ▼
TiffExportEngine::startExport()          [主线程]
  ├─ separateItems()                     分离 ImageItem / 非ImageItem
  ├─ determineExportRect()               从 CanvasItem 获取导出区域
  ├─ determineTargetDpi()                确定 DPI
  ├─ buildSources()                      构建 SourceTiffInput 列表
  ├─ batchRenderOverlays()               主线程批量渲染 overlay → QImage (BGRA)
  └─ launchExportWithBgraOverlays()      启动 QThread
       │
       ▼
     后台线程:
       ├─ BGRA → CMYK 转换 (blendAndConvertToCmyk)
       └─ StripPipeline::execute()
            ├─ 打开所有 SourceReader（持久化 TIFF 句柄）
            ├─ Producer 线程    并行读取 strip（QtConcurrent + QSemaphore）
            ├─ Organizer 线程   z-order 合成 strip
            └─ Consumer 线程    TIFFWriteScanline 逐行写入
```

### 关键类

| 类 | 文件 | 职责 |
|---|------|------|
| `TiffExportEngine` | UI/TiffExportEngine.h | 导出编排：场景检查、Overlay 渲染、后台线程管理 |
| `StripPipeline` | Utils/TiffExportPipeline.h | 三级流水线：Producer/Organizer/Consumer |
| `SourceReader` | Utils/SourceReader.h | 源 TIFF 流式读取 + 色域转换 + 滑动窗口缓存 |
| `ScopedTiffHandle` | Utils/ScopedTiffHandle.h | libtiff `TIFF*` RAII 包装 |
| `QATColorManager` | Libs/ColorTrans/colortransform.h | LCMS2 色域变换管理（单例） |
| `ExportImageDialog` | UI/ExportImageDialog.h | 导出参数对话框 |

### StripPipeline 配置

```cpp
struct Config {
    int stripHeight = 256;       // 每 strip 行数
    int pipelineDepth = 3;       // 环形缓冲区大小
    int maxReaderThreads = 4;    // Producer 并行 reader 数
};
```

---

## 二、导出流程详解

### 2.1 主线程阶段（TiffExportEngine::startExport）

```
1. 分离图元
   scene->items() → filterSelectableItems()
   → separateItems() → imageItems + nonImageItems

2. 确定导出参数
   exportRect: CanvasItem::rect() 或 itemsBoundingRect()
   targetDpi:  dpiOverride > CanvasItem::dpiLocked > firstImage.dpi

3. 构建源输入
   每个 ImageItem → SourceTiffInput { filePath, outputRect, zOrder }
   outputRect = sceneBoundingRect 相对于 exportRect 的偏移

4. 批量渲染 Overlay (P0 优化后)
   batchRenderOverlays():
     - 预隐藏所有图元
     - 逐个 overlay: setVisible → scene->render → setVisible(false)
     - 返回 QList<BgraOverlay> { QImage, outputRect, zOrder }

5. 启动后台线程
   launchExportWithBgraOverlays()
```

### 2.2 后台线程阶段

```
6. BGRA → CMYK 转换
   blendAndConvertToCmyk():
     - un-premultiply alpha + blend with white
     - LCMS2 convertBgra8ToCmyk8() 或 bgraToCmykFallback()

7. StripPipeline::execute()
   7a. 打开源 TIFF（SourceReader::open，持久化句柄）
   7b. 创建输出 TIFF（设置 tags: CMYK/LZW/ICC/DPI）
   7c. 三线程流水线：
       Producer:  QtConcurrent 并行读取每个源的 strip 行
       Organizer: 按 z-order 合成源数据 + overlay
       Consumer:  TIFFWriteScanline 写入
```

### 2.3 色域转换路径

| 源类型 | CONTIG 模式 | SEPARATE 模式 |
|--------|------------|--------------|
| CMYK | memcpy 直接拷贝 | 按平面读取 → interleave |
| RGB | RGB→BGRA 展开 → LCMS2→CMYK | 按平面读取 → BGRA 展开 → LCMS2→CMYK |
| Grayscale | 映射到 K 通道 (C=M=Y=0) | 同 CONTIG |

LCMS2 配置：sRGB → JapanColor2001Coated，INTENT_PERCEPTUAL，BLACKPOINTCOMPENSATION | HIGHRESPRECALC

---

## 三、已修复问题（P0）

### P0-1: Overlay 渲染 BSP 树重建导致 UI 卡死

**根因**：旧版 `renderOverlays()` 使用 `VisibilityScope` 逐个渲染 overlay。每次渲染前保存所有图元可见性 → 隐藏非目标 → 渲染 → 恢复所有。每次 `setVisible()` 触发 BSP 树标记脏，`scene->render()` 时重建。

N 个 overlay 的 BSP 树重建次数：**2N²**（每次渲染 2N 次 setVisible × N 次渲染）。

**修复**：`batchRenderOverlays()` 预隐藏所有图元一次，逐个 overlay 仅切换单个图元可见性。

BSP 树重建次数降至 **N+1**。

| overlay 数量 | 旧方案重建次数 | 新方案重建次数 |
|-------------|-------------|-------------|
| 5 | 50 | 6 |
| 10 | 200 | 11 |
| 50 | 5000 | 51 |

### P0-2: CMYK 转换阻塞主线程

**根因**：旧版在主线程完成 `scene->render()` + LCMS2 `convertBgra8ToCmyk8()` + 透明像素清零，主线程长时间阻塞。

**修复**：`scene->render()` 仅输出 QImage (BGRA)，CMYK 转换移到后台线程 `blendAndConvertToCmyk()`。主线程阻塞时间从 N × (render + convert) 降至 N × render。

`blendAndConvertToCmyk` 同时修正了 alpha 处理：旧版 `renderCmykOverlay` 对反锯齿像素按"非零 alpha 即不透明"处理，新版做正确的 un-premultiply + alpha blend with white。

---

## 四、现存问题分析

### 4.1 性能问题

#### P1: SourceReader RGB 源每行分配临时 vector

```cpp
// SourceReader.cpp:156 — RGB 源的 readAndConvertRow()
std::vector<uint8_t> bgraLine(static_cast<size_t>(w) * 4);
```

每次调用分配/释放 `w × 4` 字节。Producer 循环中 256 行 × N 源 = 大量堆操作。

**建议**：在 SourceReader 中预分配 `m_bgraBuf` 成员，`open()` 时分配一次，后续复用。

#### P1: Producer 每 strip 每源一次 QtConcurrent::run

```cpp
// TiffExportPipeline.cpp:371
futures.append(QtConcurrent::run([&, si]() { ... }));
```

10 源 × 100 strip = 1000 次线程池任务提交。`QtConcurrent::run()` 有任务队列锁和调度开销。

**建议**：改为 Producer 内部循环读取所有源，不使用 QtConcurrent。每个 strip 内顺序读取各源（已有 QSemaphore(4) 限制并发），减少任务提交次数。

#### P2: Organizer 每行 memset 白色初始化

```cpp
// TiffExportPipeline.cpp:539
std::memset(outRow, 0, static_cast<size_t>(outWidth) * 4);
```

每 strip 256 行 × `outWidth × 4` 字节。大部分区域被源图覆盖时，memset 后立即被 memcpy 覆盖。

**建议**：第一个 participant 直接 memcpy（覆盖整行），后续 participant 才做叠加。需要区分"首个覆盖整行的源"和"部分覆盖的源/overlay"。

#### P2: consumerStage 中 shrink_to_fit 导致内存碎片

```cpp
// TiffExportPipeline.cpp:637-644
slot.compositedRows.clear();
slot.compositedRows.shrink_to_fit();
for (auto &sd : slot.sourcesData) {
    sd.cmykRows.clear();
    sd.cmykRows.shrink_to_fit();
}
```

每个 strip 写入后立即 `shrink_to_fit()` 归还内存给 OS，下个 strip 又重新 `resize()` 分配同样大小。反复 alloc/dealloc 导致内存碎片。

**建议**：去掉 `shrink_to_fit()`，保留 vector 容量，让环形缓冲区自然复用。`clear()` 后 `size()` 为 0 但 `capacity()` 不变，下次 `resize()` 不触发重新分配。

#### P3: 条件变量 wait 中的全量扫描

```cpp
// TiffExportPipeline.cpp:165-171 — Producer 的 cv.wait predicate
cv.wait(lock, [&]() -> bool {
    for (int i = 0; i < m_config.pipelineDepth; ++i) {
        if (stripSlots[i].state.load(...) == StripSlot::State::Done)
            return true;
    }
    return isCancelled(cancelFlag) || pipelineError.load(...);
});
```

三个 stage 都在 `cv.wait()` 的 predicate 中遍历所有 slot。当 `pipelineDepth=3` 时开销可忽略，但 `notify_all()` 唤醒所有等待线程而实际只需唤醒一个。

**建议**：改为每 stage 使用独立的 condition_variable，或使用 `notify_one()` + 各 stage 检查自己的目标状态。

#### P3: stripHeight 硬编码 256

```cpp
// TiffExportEngine.cpp:468
pipelineCfg.stripHeight = 256;
```

小图（100×100）退化为单 strip 串行处理。超大图（50000×50000）产生 ~200 个 strip，每个 strip 的 Producer 为所有源分配缓冲区。

**建议**：根据 `outH` 动态调整 stripHeight，如 `min(256, max(64, outH / 8))`，保证至少 8 个 strip 以利用流水线，但不超过 256 行以控制单 strip 内存。

### 4.2 内存问题

#### P1: StripSlot sourcesData 并发分配

每个 strip slot 的每个源分配独立的 `cmykRows` 缓冲区。

`pipelineDepth=3` × `sources=10` × `sourceWidth × 256 × 4` 字节。

示例：10 个 8000×6000 源图 → 每个 source strip = 8MB → 30 个 = **240MB**。

**建议**：考虑源数据共享——如果多个 strip 的源数据来自同一源图的不同行范围，可使用引用计数的共享缓冲区，或直接从 SourceReader 的滑动窗口读取（需要修改 Organizer 为流式读取模式）。

#### P2: CmykOverlay 全生命周期驻留内存

所有 overlay 在整个导出过程中以 `const QList<CmykOverlay>&` 形式传给 `organizerStage()`，像素数据始终驻留内存。

**建议**：改为 per-strip 处理——organizer 阶段按需从 QImage 裁剪 + 转换，不需要预先转换全部 overlay 为 CMYK。这需要将 overlay 以 QImage 形式传入 pipeline，在 organizer 中按 strip 裁剪和转换。

#### P2: Overlay 按 bounding rect 全尺寸渲染

每个 overlay 的渲染尺寸 = 其在导出图中的 bounding rect 像素尺寸。

对于大面积 overlay（如背景矩形），渲染缓冲区接近导出图全尺寸。

**建议**：对超过阈值（如 50% 导出面积）的 overlay，考虑分 tile 渲染或直接在 organizer 中逐 strip 渲染。

### 4.3 线程安全问题

#### P3: pipelineError 与 errorMsg 的可见性

```cpp
// TiffExportPipeline.cpp:67-69
std::atomic<bool> pipelineError{ false };
std::mutex errorMutex;
std::string errorMsg;
```

`pipelineError` 的 `store()` 在 mutex 外执行。理论上可能出现：线程 A 设置 `pipelineError=true`，线程 B 看到 `true` 并读取 `errorMsg`，但线程 A 的 `errorMsg` 赋值还没对线程 B 可见。

实际由于 `pipelineError` 使用默认 `seq_cst`，问题不太可能发生。但代码结构不够严谨。

**建议**：将 `pipelineError.store()` 移到 mutex 保护区域内，或使用 `std::atomic<std::string*>` 模式。

### 4.4 功能缺失

#### P3: ExportImageDialog 选项未接入

`ExportImageDialog` 暴露的选项 vs `TiffExportSettings` 实际使用的字段：

| 对话框选项 | TiffExportSettings 字段 | 状态 |
|-----------|----------------------|------|
| DPI | `dpi` | ✅ 已接入 |
| Compression | `compression` | ✅ 已接入 |
| ICC Profile | `iccProfilePath` | ✅ 已接入 |
| Byte Order | - | ❌ 未接入 |
| Bit Depth (8/16) | - | ❌ 未接入 |
| Planar Config | - | ❌ 未接入 |
| Predictor | - | ❌ 未接入 |
| JPEG Quality | - | ❌ 未接入 |
| Embed ICC (bool) | - | ❌ 未接入 |
| Preserve Metadata | - | ❌ 未接入 |

**建议**：扩展 `TiffExportSettings` 结构体，将对话框选项传递到 `StripPipeline::execute()`。

#### P3: 源 TIFF 的 outputRect 假设 1:1 像素映射

```cpp
// TiffExportEngine.cpp:157-160
src.outputRect = QRectF(sceneRect.left() - exportRect.left(),
                        sceneRect.top() - exportRect.top(),
                        sceneRect.width(), sceneRect.height());
```

`outputRect` 宽高 = `sceneBoundingRect` 尺寸（场景坐标），不经过 DPI 转换。`organizerStage` 假设 "outputRect 尺寸 == 源图像像素尺寸"。

如果 ImageItem 在场景中被缩放（拖拽改变大小），导出时源像素 1:1 拷贝，不做缩放映射。

**建议**：在 `buildSources()` 中根据源图 DPI 和导出 DPI 计算正确的 outputRect 尺寸，或在 organizer 中支持缩放映射。

#### P3: 导出失败静默吞错

```cpp
// TiffExportPipeline.cpp:449-451
if (!reader.readAndConvertRow(row, rowBuf)) {
    std::memset(rowBuf.data(), 0, rowBytes); // 静默填充黑色
}
```

源 TIFF 读取失败时，该行被填充为 CMYK(0,0,0,0)（白色），用户不会收到任何警告。

**建议**：记录读取失败的行号，导出完成后通过 `exportFinished` 信号报告警告信息。

---

## 五、问题优先级总览

| 优先级 | 问题 | 类型 | 影响 |
|--------|------|------|------|
| ~~P0~~ | ~~Overlay BSP 树重建~~ | ~~性能~~ | ~~UI 卡死~~ ✅ 已修复 |
| ~~P0~~ | ~~CMYK 转换阻塞主线程~~ | ~~性能~~ | ~~UI 卡死~~ ✅ 已修复 |
| P1 | SourceReader RGB 每行分配 vector | 性能 | 堆碎片 + CPU |
| P1 | Producer 每 strip 每源 QtConcurrent::run | 性能 | 调度开销 |
| P1 | StripSlot sourcesData 并发分配 | 内存 | 峰值 240MB+ |
| P2 | Organizer 每行 memset 白色 | 性能 | 多余拷贝 |
| P2 | shrink_to_fit 内存碎片 | 内存 | 碎片化 |
| P2 | CmykOverlay 全生命周期驻留 | 内存 | 大 overlay 场景 |
| P2 | Overlay 按 bounding rect 全尺寸渲染 | 内存 | 大 overlay 场景 |
| P3 | cv.wait 全量扫描 + notify_all | 性能 | pipelineDepth 增大时 |
| P3 | stripHeight 硬编码 256 | 性能 | 极端尺寸场景 |
| P3 | pipelineError 可见性 | 线程安全 | 理论风险 |
| P3 | ExportImageDialog 选项未接入 | 功能 | 用户设置被忽略 |
| P3 | outputRect 1:1 像素映射 | 功能 | 缩放图元导出异常 |
| P3 | 导出失败静默吞错 | 功能 | 用户无感知 |

---

## 六、建议实施路径

### 第一阶段（低成本高收益）

1. **P1 SourceReader vector 复用**：添加 `m_bgraBuf` 成员，`open()` 时预分配
2. **P2 去掉 shrink_to_fit**：保留 vector 容量，环形缓冲区自然复用
3. **P3 ExportImageDialog 参数透传**：扩展 TiffExportSettings + 更新 pipeline

### 第二阶段（中等投入）

4. **P1 Producer 改为内部循环**：去掉 QtConcurrent::run，减少任务提交
5. **P2 Organizer 首行 memcpy 优化**：区分首个全覆盖 participant 和后续叠加
6. **P3 stripHeight 动态调整**：根据 outH 自适应

### 第三阶段（架构级改动）

7. **P2 Overlay per-strip 处理**：overlay 以 QImage 传入 pipeline，organizer 按需裁剪转换
8. **P1 源数据流式读取**：organizer 直接从 SourceReader 滑动窗口读取，去掉 strip 级缓冲区
9. **P3 outputRect DPI 映射**：修改 buildSources 支持源图缩放

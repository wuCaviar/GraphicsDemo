# 图片导入性能与可扩展性优化方案

## 1. 当前架构概览

### 1.1 导入流程

```
用户触发导入 (Ctrl+I / 工具栏)
  → QFileDialog::getOpenFileNames()           // 多选文件
  → 串行 QImageReader::size() 预检             // 串行，阻塞 UI
  → QMessageBox 缩放确认弹窗
  → QtConcurrent::mapped() 线程池
      → runImportWorker() 逐文件执行
          → loadImageFromFile()
              → QImageReader::read()           // 第一次解码（用于显示的 QImage）
              → 若为 TIFF: importTiffWithLibtiff() // 第二次打开文件 + 解码（原始 CMYK）
              → 若非 TIFF: imageToCmykBuffer()    // 逐像素 LCMS2 RGB→CMYK
          → 若需缩放: QImage::scaled()
  → resultReadyAt 信号（主线程）
      → QPixmap::fromImage()                   // GPU 上传，阻塞主线程
      → new ImageItem(pixmap)
      → undoStack.push(AddItemCommand)
```

### 1.2 关键数据结构

| 结构体 | 位置 | 用途 |
|---|---|---|
| `ImportResult` | `ImageUtils.h:39-49` | 持有 QImage（显示） + RawPixelBuffer（CMYK） + 元数据 |
| `RawPixelBuffer` | `ImageUtils.h:21-36` | 4 通道 8 位原始像素，`宽 × 高 × 4` 字节存于 QByteArray |
| `ImageItem` | `ImageItem.h:8-92` | QGraphicsPixmapItem + IGraphicsItem；持有 pixmap、rawTiffMat、rawCmykMat |
| `ImportWorkerResult` | `ImageWorker.h:91-97` | 线程池返回值，封装 ImportResult |
| `ImageImportPipeline` | `ImageWorker.h:36-45` | 可扩展的后处理器链（当前为空） |

### 1.3 每张 ImageItem 的内存模型

以 4000×3000 RGBA 图片为例：

| 缓冲区 | 格式 | 大小 | 分配时机 |
|---|---|---|---|
| `QGraphicsPixmapItem::pixmap()` | GPU/ARGB32 | ~48 MB | 导入时（主线程） |
| `m_rawTiffMat` | RGBA 原始数据 | ~48 MB | 仅在反序列化时（复制/粘贴） |
| `m_rawCmykMat` | CMYK 原始数据 | ~48 MB | 导入时（工作线程） |
| **合计** | | **~144 MB** | |

> **已发现 Bug：** `m_rawTiffMat` 在初始导入时**从未被赋值**——仅在剪贴板反序列化时填充。这意味着新导入的图片无法进行无损 TIFF 重导出。

---

## 2. 性能瓶颈分析

### 2.1 [P0] 逐像素 LCMS2 色彩转换

**位置：** `ImageUtils.cpp:75-104`（`imageToCmykBuffer`）、`ImageWorker.cpp:246-276`（`exportTiffCmykFromSnapshot`）

**根因：** 每个像素构造一个 `QColor` 临时对象，并调用 `cmsDoTransform(transform, src, dst, 1)`——一次只转换一个像素。当前 LCMS2 变换句柄签名为 `TYPE_RGB_8 → TYPE_CMYK_DBL`（3 字节输入，4 个 double 输出）。

```cpp
// 当前：4000×3000 的图片 = 1200 万次 QColor 构造 + 1200 万次 cmsDoTransform 调用
for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
        QATColorManager::Cmyk cmyk = cm.toCmyk(QColor(c)); // 逐像素
    }
}
```

**影响：** 比批量转换慢 20-50 倍。1200 万像素图片约需 2-5 秒，而批量方式仅需 50-100ms。

**修复方案：** 新增一个签名为 `TYPE_BGRA_8 → TYPE_CMYK_8` 的变换句柄，每次对整行调用 `cmsDoTransform()`（甚至可一次处理整张图）。变换创建开销可忽略（仅一次），LCMS2 内部会自动使用 SIMD 批量处理。

```cpp
// 改进后：每行仅一次 cmsDoTransform 调用
cmsHTRANSFORM batchXform = cmsCreateTransform(
    srgb_profile, TYPE_BGRA_8,
    cmyk_profile, TYPE_CMYK_8,
    INTENT_PERCEPTUAL,
    cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);

for (int y = 0; y < h; ++y) {
    const uint8_t *src = image.constScanLine(y);
    uint8_t *dst = buf.ptr(y);
    cmsDoTransform(batchXform, src, dst, w); // 一次处理整行
}
```

> **注意事项：** 小端序平台（ARM Mac）上 `QImage::Format_ARGB32` 的扫描线字节序为 BGRA，与 LCMS2 的 `TYPE_BGRA_8` 匹配。Alpha 通道会被 LCMS2 自动忽略（输出格式不含 Alpha）。

### 2.2 [P0] TIFF 文件双重打开与双重解码

**位置：** `ImageUtils.cpp:321-349`（`loadImageFromFile`）

**根因：** TIFF 文件被解码了两次：
1. `QImageReader::read()` —— 完整解码为 QImage 用于显示
2. `importTiffWithLibtiff()` —— 重新打开文件，读取扫描线以提取原始 CMYK 数据

对于 CMYK TIFF，第一次解码甚至可能失败或产生错误颜色（QImageReader 对 CMYK TIFF 支持不佳，常常返回空图或灰度图）。

**影响：** 所有 TIFF 文件导入耗时约翻倍。

**修复方案：** TIFF 文件统一走 libtiff 路径：
- **CMYK TIFF：** 读取原始 CMYK 扫描线 → 批量 LCMS2 CMYK→RGB 生成显示用 QImage + 保留原始 CMYK 作为 `rawCmykMat`
- **RGB/灰度 TIFF：** `TIFFReadRGBAImage()` → 直接构造 QImage（`importTiffWithLibtiff` 的非 CMYK 分支已实现此逻辑，但结果被弃用，转而使用 QImageReader 的结果）
- **非 TIFF 格式：** 保持现有 QImageReader 路径不变

### 2.3 [P1] 主线程 QPixmap 转换阻塞 UI

**位置：** `mainwindow.cpp:923-924`

```cpp
auto *item = new ImageItem(QPixmap::fromImage(result.importResult.image));
```

**根因：** `QPixmap::fromImage()` 在主线程执行 GPU 上传及可能的格式转换。1200 万像素图片耗时 50-200ms，期间 UI 完全冻结。批量导入时，多个文件快速连续完成会导致累计卡顿。

**修复方案：** 在工作线程中完成 QPixmap 转换。QPixmap 虽然渲染操作并非线程安全，但在隔离的工作线程中从 QImage 构造 QPixmap 是安全的（不涉及 GUI 操作）。构造完成后通过 `QFuture` 回传主线程。

备选方案：ImageItem 内部存储 QImage，首次 `paint()` 时延迟转换为 QPixmap。此方案同时减少导入时的内存占用（避免 QImage 与 QPixmap 共存）。

### 2.4 [P1] 串行尺寸预检

**位置：** `mainwindow.cpp:878-900`

**根因：** 异步加载开始前，串行打开所有选中文件并调用 `QImageReader(path).size()` 检查是否有图片超出画布尺寸。选择 50 个文件时，即使每次仅读取文件头，累计等待时间也相当可观。

**修复方案：** 将尺寸判断移入工作线程，每个 worker 根据自己的图片尺寸独立决定是否缩放。去掉预检循环和模态弹窗，改为非阻塞方式：
- 方案 A：始终缩放适配，改为可配置的开关选项
- 方案 B：使用 `QtConcurrent::blockingMapped` 并行化文件头读取（每次 <1ms，50 个文件约 50ms 完成）

### 2.5 [P1] QImageReader::setAllocationLimit(0) 禁用内存保护

**位置：** `ImageUtils.cpp:327`

```cpp
reader.setAllocationLimit(0); // 关闭 Qt 内存保护机制
```

**根因：** 设为 0 会禁用 Qt 内置的解压炸弹防护（例如一个 100KB 的恶意文件解压后可达 10GB）。这既是性能隐患（OOM 崩溃），也是安全漏洞。

**修复方案：** 设为合理上限（如 512 MB），超限时拒绝加载并向用户展示错误提示。

### 2.6 [P2] 未使用 QImageReader::setScaledSize() 解码器降采样

**位置：** `ImageWorker.cpp:142-143`

```cpp
importResult.image = importResult.image.scaled(
    qRound(iw * scale), qRound(ih * scale), ...);
```

**根因：** 缩放适配画布时，先解码全分辨率图片，再通过 `QImage::scaled()` 降采样。Qt 的 `QImageReader::setScaledSize()` 可让解码器在解码过程中直接降采样，速度更快且内存占用更低。

**修复方案：** `loadImageFromFile()` 中在 `reader.read()` 之前调用 `reader.setScaledSize(targetSize)`，将缩放参数传入。

### 2.7 [P2] 每张图片三份像素数据

**根因：** 每个 `ImageItem` 最多持有 3 份像素数据：
- `QPixmap`（GPU 内存）—— 始终存在，渲染必需
- `m_rawTiffMat` —— 仅在反序列化时填充，初始导入时为空（Bug）
- `m_rawCmykMat` —— 导入时始终填充，1200 万像素约 48MB

**修复方案：**
1. 修复 `m_rawTiffMat` 的填充：TIFF 导入时直接将 libtiff 读取的原始 RGBA 数据存入，而非仅存 CMYK 转换结果
2. 延迟 `m_rawCmykMat` 的计算：仅在用户触发 CMYK TIFF 导出时才转换，而非导入时
3. 考虑对 `RawPixelBuffer::data` 使用 zlib 压缩（QByteArray 支持 `qCompress`/`qUncompress`），以 CPU 换内存

### 2.8 [P2] 非 TIFF 格式无条件 CMYK 转换

**位置：** `ImageUtils.cpp:342-345`

```cpp
// PNG/JPEG/BMP：无条件执行 RGB→CMYK 转换
if (!result.image.isNull()) {
    result.rawCmykMat = imageToCmykBuffer(result.image);
}
```

**根因：** 每张导入的 PNG、JPEG、BMP 都执行完整的 RGB→CMYK 转换，而用户可能从不使用 CMYK 导出。浪费 CPU 且内存占用翻倍。

**修复方案：** 将 CMYK 转换推迟到导出时执行，或通过用户设置按需开启。

### 2.9 [P3] 序列化：未压缩的剪贴板数据

**位置：** `ImageItem.cpp:97-106`（序列化）、`mainwindow.cpp:1550-1577`（copyItemsToClipboard）

**根因：** 图片像素数据以未压缩原始字节序列化到剪贴板。单张 4000×3000 图片产生约 96MB 剪贴板数据（QImage ARGB32 + rawTiffMat）。选中多张图片时成倍增长。`setMimeData()` 前无任何大小检查。

**修复方案：**
1. 复制前添加大小阈值警告（如超过 100MB 提示用户）
2. 序列化时对 `RawPixelBuffer::data` 使用 `qCompress()` 压缩
3. 超过阈值的剪贴板操作考虑存储文件路径引用，替代原始像素数据传输

---

## 3. 可扩展性缺陷

### 3.1 无渐进式/延迟加载

大图（5000 万像素以上）解码期间工作线程阻塞数秒。即使缩放到缩略图大小，整张 QImage 仍以全分辨率保留在内存中。Qt Graphics View 不原生支持瓦片/渐进式图像渲染。

**建议：** 超过阈值（如 4096×4096）的图片，生成降采样代理图用于显示，仅在需要时（导出或高缩放比渲染）加载全分辨率数据。这是一个较大的架构改动，但对于处理专业摄影素材必不可少。

### 3.2 无内存预算管理

多张大图可无限制导入，没有任何内存上限控制。10 张 1200 万像素图片 ≈ 1.4GB 内存。无淘汰机制，无预警提示。

**建议：** 追踪图片总内存占用量。超过可配置预算（如 1GB）时，警告用户或使用 LRU 策略淘汰不可见图元的原始 CMYK/TIFF 缓冲区。原始缓冲区可在需要时从 QPixmap 重新生成或从磁盘重新加载。

### 3.3 无取消机制

`QtConcurrent::mapped()` 一旦启动就无法取消正在进行的导入。用户必须等待所有文件加载完成。

**建议：** 使用 `QFuture::cancel()` 配合取消令牌模式。在 `loadImageFromFile()` 的关键步骤之间（解码后、CMYK 转换后）检查令牌，提前返回。

### 3.4 无格式特定优化

- **JPEG：** Qt 的 libjpeg 后端支持 `setScaledSize()` 和 `setClipRect()` 区域解码 —— 未使用
- **PNG：** libpng 支持隔行/渐进式显示 —— 未使用
- **BMP：** RLE 压缩的 BMP 在内存中完全解压 —— 可改为流式处理

### 3.5 序列化粒度过大

复制/粘贴在 QDataStream 中将 `RawPixelBuffer` 序列化为一个整体的 `QByteArray`。1200 万像素图片意味着序列化和反序列化时各需在堆上分配一个约 48MB 的连续 `QByteArray`。无分块或流式处理。

**建议：** 超过阈值（如 16MB）的缓冲区，使用带长度前缀的分段写入，避免巨型连续内存分配。

---

## 4. 实施方案

### 第一阶段：快速收益（1-2 天）

| # | 改动 | 涉及文件 | 预期收益 |
|---|---|---|---|
| 1 | `imageToCmykBuffer()` 批量 LCMS2 转换 | `colortransform.h/cpp`、`ImageUtils.cpp:57-107` | CMYK 转换速度提升 20-50× |
| 2 | `exportTiffCmykFromSnapshot()` 批量 LCMS2 转换 | `ImageWorker.cpp:241-276` | 导出速度提升 20-50× |
| 3 | 缩放适配时使用 `reader.setScaledSize()` | `ImageUtils.cpp:321-349` | 缩放导入快 2-5× |
| 4 | 将 `setAllocationLimit(0)` 改为 512MB 上限 | `ImageUtils.cpp:327` | 防止 OOM |

**批量 LCMS2 需新增变换句柄**，输入为 `TYPE_BGRA_8`（匹配 `QImage::Format_ARGB32` 扫描线布局），输出为 `TYPE_CMYK_8`：

```cpp
// QATColorManager 新增成员：
cmsHTRANSFORM m_tRgbToCmykBatch = nullptr; // TYPE_BGRA_8 → TYPE_CMYK_8

// imageToCmykBuffer() 改造后：
const uchar *srcLine = img32.constScanLine(y);
uint8_t *dstLine = buf.ptr(y);
cmsDoTransform(batchXform, srcLine, dstLine, w); // 每行一次调用
```

### 第二阶段：核心优化（2-3 天）

| # | 改动 | 涉及文件 | 预期收益 |
|---|---|---|---|
| 5 | TIFF 统一走 libtiff 加载路径 | `ImageUtils.cpp:321-349` | TIFF 导入快 2× |
| 6 | `QPixmap::fromImage()` 移入工作线程 | `mainwindow.cpp:923`、`ImageWorker.cpp:122-151` | UI 不再冻结 |
| 7 | 并行化尺寸预检或直接取消预检 | `mainwindow.cpp:878-900` | 批量导入启动更快 |
| 8 | 非 TIFF 格式延迟 CMYK 转换 | `ImageUtils.cpp:342-345`、`ImageItem.cpp` | 内存减半，导入更快 |

**第 5 项（统一 TIFF 加载）实现示意：**

```cpp
ImportResult loadImageFromFile(const QString &path, const QSize &scaledSize = {})
{
    if (isTiffFile(path)) {
        return loadTiffWithLibtiff(path, scaledSize); // 新函数
    }
    // 非 TIFF：保持现有 QImageReader 路径
    ImportResult result;
    QImageReader reader(path);
    if (scaledSize.isValid()) reader.setScaledSize(scaledSize);
    reader.setAllocationLimit(512 * 1024 * 1024); // 512 MB
    result.image = reader.read();
    // ... 提取 DPI，不执行 CMYK 转换
    return result;
}
```

`loadTiffWithLibtiff()` 合并当前 `importTiffWithLibtiff()` 的逻辑与显示 QImage 的生成：
- CMYK TIFF：读取原始 CMYK → 存为 `rawCmykMat` → 批量 LCMS2 CMYK→RGB 生成显示 QImage
- 非 CMYK TIFF：`TIFFReadRGBAImage()` → 构造 QImage → 无需 CMYK 转换（延迟到导出）

### 第三阶段：内存与可扩展性（2-3 天）

| # | 改动 | 涉及文件 | 预期收益 |
|---|---|---|---|
| 9 | 延迟 CMYK 转换 —— 仅导出时执行 | `ImageUtils.cpp:342-345`、`ImageItem.h` | 每张图片减少 48MB 内存 |
| 10 | 新增导入取消支持 | `mainwindow.cpp:948`、`ImageWorker.cpp` | 用户体验提升 |
| 11 | 新增内存预算追踪与预警 | 新增工具类、`ImageItem` | 防止 OOM |
| 12 | 修复 `m_rawTiffMat` 导入时未填充 Bug | `ImageItem.cpp`、`mainwindow.cpp:927` | 无损 TIFF 重导出可用 |

### 第四阶段：长期架构演进（按需实施）

| # | 改动 | 说明 |
|---|---|---|
| 13 | 图片代理/缩略图系统 | 缩小时显示降采样缩略图，按需加载全分辨率 |
| 14 | 大图瓦片渲染 | >8K 图片切分为瓦片；仅渲染当前缩放比下的可见瓦片 |
| 15 | 剪贴板大小限制 | 超过 100MB 时警告；使用压缩；超大时回退为文件路径引用 |
| 16 | 流式序列化 | RawPixelBuffer 分块写入 QDataStream，避免巨型连续分配 |
| 17 | 格式特定解码路径 | JPEG 使用 libjpeg 流式解码，PNG 使用 libpng 渐进式 |

---

## 5. 风险评估

| 改动 | 风险 | 缓解措施 |
|---|---|---|
| 批量 LCMS2（第一阶段） | 大端序平台 BGRA 字节序错误 | 仅 ARM Mac（小端序）测试；添加 static_assert 校验字节序 |
| 统一 TIFF 加载（第二阶段） | CMYK→RGB 转换质量与 QImageReader 不一致 | 与参考图片对比输出；保留 QImageReader 作为可选回退路径 |
| 工作线程 Pixmap（第二阶段） | QPixmap 构造的线程安全性 | Qt 6 中 QPixmap(QImage) 在非 GUI 线程安全；添加 qDebug 守卫 |
| 延迟 CMYK（第二阶段） | 导出时原始数据缺失 | 添加回退：无缓存 CMYK 数据时导出时实时计算 |
| 取消机制（第三阶段） | TIFF/libjpeg 可能不支持解码中途中断 | 关闭 TIFF 句柄并返回部分结果；将图元标记为不完整 |
| 内存预算（第三阶段） | 淘汰原始数据导致无损导出失败 | 仅在所有引用释放后淘汰；可通过 QPixmap 重新转换恢复 |

---

## 6. 回归测试清单

### TIFF 变体
- [ ] CMYK TIFF（PHOTOMETRIC_SEPARATED，4 通道，CONTIG 交错）
- [ ] CMYK TIFF（PHOTOMETRIC_SEPARATED，4 通道，SEPARATE 分离平面）
- [ ] RGB TIFF（8 位，无压缩）
- [ ] RGBA TIFF（8 位，含透明通道）
- [ ] 灰度 TIFF（8 位）
- [ ] 16 位 TIFF
- [ ] LZW 压缩 TIFF
- [ ] ZIP 压缩 TIFF
- [ ] 瓦片 TIFF（Tiled）
- [ ] 多页 TIFF（应优雅处理）

### 非 TIFF 格式
- [ ] PNG（不透明）
- [ ] PNG（透明/Alpha 通道）
- [ ] JPEG（基线）
- [ ] JPEG（渐进式）
- [ ] BMP（无压缩）
- [ ] BMP（RLE 压缩）

### 场景
- [ ] 单张图片导入
- [ ] 批量导入（5 张）
- [ ] 批量导入（50 张）
- [ ] 缩放适配：图片大于画布
- [ ] 缩放适配：图片小于画布
- [ ] 导入后撤销
- [ ] 导入后重做
- [ ] 复制已导入图片到剪贴板
- [ ] 从剪贴板粘贴图片
- [ ] 导入后 CMYK TIFF 导出（验证色彩一致性）
- [ ] 批量导入中途取消（第三阶段后）
- [ ] 超大图片（8000×6000，4800 万像素）
- [ ] 含嵌入式 ICC Profile 的图片
- [ ] 含 EXIF 旋转元数据的图片

---

## 7. 性能度量与监控

添加耗时检测代码：

```cpp
// runImportWorker() 中：
auto t0 = std::chrono::steady_clock::now();
ImportResult r = loadImageFromFile(path);
auto t1 = std::chrono::steady_clock::now();
qDebug() << "loadImageFromFile" << path << ":"
         << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << "ms"
         << "size:" << r.image.size()
         << "isTIFF:" << isTiffFile(path);
```

长期跟踪指标：
- 按格式和图片尺寸分层的 P50/P95/P99 导入延迟
- 批量导入期间的内存峰值
- CMYK 转换时间占导入总时间的百分比
- 剪贴板数据大小分布

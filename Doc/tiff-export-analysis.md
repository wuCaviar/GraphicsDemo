# TIFF 导出流程分析与优化建议

> 分析日期：2026-05-29
> 涉及文件：`Src/UI/mainwindow.cpp`、`Src/Utils/ImageWorker.cpp`、`Src/Utils/ImageWorker.h`、`Src/ColorTrans/colortransform.cpp`

---

## 一、导出流程全景

```
用户点击导出 (Ctrl+E)
       │
       ▼
┌─────────────────────────────────┐
│  Step 1: 选择保存路径 (.prn)     │  mainwindow.cpp:1454
│  可选配置 RIP 分辨率             │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Step 2: 收集画布图元            │  mainwindow.cpp:1474
│  分离为 ImageItem / 非ImageItem  │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Step 3: 计算导出区域和目标 DPI   │  mainwindow.cpp:1491
│  exportRect + targetDpi          │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Step 4: 构建 SourceTiffInput    │  mainwindow.cpp:1502
│  每个 ImageItem → 文件路径 +     │
│  输出矩形 + zOrder               │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────────────┐
│  Step 5: 将非 ImageItem 渲染为 CMYK Overlay              │
│  mainwindow.cpp:1527                                     │
│                                                          │
│  ┌─ 有存储 CMYK 值 ─→ 渲染 alpha 蒙版 + 精确 CMYK 填充   │
│  │                       (跳过 LCMS2，保留精确色值)        │
│  └─ 无存储 CMYK ─→ 渲染 BGRA QImage                     │
│                    ─→ LCMS2 convertBgra8ToCmyk8()        │
│                    ─→ 透明像素清零                        │
└──────────────┬──────────────────────────────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Step 6: 配置 TiffExportSettings │  mainwindow.cpp:1821
│  dpi / compression / iccPath     │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────────────┐
│  Step 7: QThread::create() 后台线程                      │
│  mainwindow.cpp:1842                                     │
│  调用 ImageUtils::exportTiff()                           │
│  进度通过 QMetaObject::invokeMethod 回调主线程            │
│  完成后可选触发 RIP 处理                                  │
└─────────────────────────────────────────────────────────┘
```

---

## 二、`exportTiff()` 核心四阶段

```
exportTiff()  ImageWorker.cpp:365
     │
     ├─ Phase 1 (0→40%)  并行读取源 TIFF → CMYK 缓冲区
     │    ├─ QtConcurrent::run(readSourceToCmyk, src)  × N
     │    ├─ QFuture::takeResult() 移动语义，零拷贝
     │    └─ 错误时 waitForFinished() 防止线程池泄漏
     │
     ├─ Phase 1b          包装 CMYK Overlay → CmykBuffer
     │    └─ overlay.data 被拷贝（非移动）
     │
     ├─ Phase 2 (40%)     按 zOrder 排序所有 buffer
     │
     ├─ Phase 3 (40→90%)  并行合成
     │    ├─ 分配 outW × outH × 4 字节输出缓冲区
     │    ├─ 按 idealThreadCount() 分块
     │    ├─ 每块：初始化白色 → 逐层双线性插值合成
     │    └─ 等待所有块完成，回调进度
     │
     └─ Phase 4 (90→100%) 写入输出 TIFF
          ├─ TIFFOpen("w") 设置全部 tag
          ├─ 嵌入 ICC Profile (JapanColor2001Coated.icc)
          └─ TIFFWriteScanline() 逐行写入
```

---

## 三、源 TIFF 读取细节 (`readSourceToCmyk`)

| 源色彩类型 | 处理方式 | 关键代码位置 |
|-----------|---------|------------|
| CMYK (PHOTOMETRIC_SEPARATED) | 直接 `memcpy` 到输出缓冲区 | ImageWorker.cpp:197 |
| RGB (PHOTOMETRIC_RGB) | RGB→BGRA 展开，再经 LCMS2 `createBgraToCmyk8()` 转 CMYK | ImageWorker.cpp:200-243 |
| Grayscale (MINISBLACK/MINISWHITE) | 映射到 K 通道，C=M=Y=0 | ImageWorker.cpp:247-257 |

**LCMS2 配置**：
- 输入 Profile：sRGB IEC61966-2.1
- 输出 Profile：JapanColor2001Coated (CMYK)
- 渲染意图：`INTENT_PERCEPTUAL`
- 标志：`BLACKPOINTCOMPENSATION | HIGHRESPRECALC`

**Planar 支持**：
- `PLANARCONFIG_CONTIG`：逐行读取，直接处理
- `PLANARCONFIG_SEPARATE`：按平面读入临时缓冲区，再交错合并

---

## 四、合成阶段细节

```
输出缓冲区 (outW × outH × 4 bytes CMYK)
     │
     ├─ 按行分块，每块由独立线程处理
     │    numThreads = QThread::idealThreadCount()
     │    rowsPerChunk = ceil(outH / numThreads)
     │
     ├─ 每行初始化为白色 CMYK(0,0,0,0)
     │
     └─ 按 zOrder 逐层叠加：
          ├─ 跳过不在当前行范围内的 buffer
          ├─ 计算缩放因子 scaleX/scaleY
          ├─ 双线性插值 (p00, p10, p01, p11)
          └─ clamp 到 [0, 255]
```

---

## 五、输出 TIFF 写入

| Tag | 值 | 说明 |
|-----|---|------|
| SAMPLESPERPIXEL | 4 | CMYK 四通道 |
| BITSPERSAMPLE | 8 | 每通道 8 位 |
| PHOTOMETRIC | SEPARATED | 分色模式 |
| INKSET | CMYK | 青品黄黑 |
| PLANARCONFIG | CONTIG | 交错排列 |
| COMPRESSION | LZW (默认) | 无损压缩 |
| ROWSPERSTRIP | TIFFDefaultStripSize | libtiff 默认条带大小 |
| ICCPROFILE | JapanColor2001Coated.icc | 嵌入 ICC 配置文件 |

写入方式：`TIFFWriteScanline()` 逐行写入，每 128 行报告一次进度。

---

## 六、内存使用分析

### 当前峰值内存模型

```
峰值内存 = Σ(源TIFF缓冲区) + Σ(CmykOverlay缓冲区) + 输出缓冲区

其中：
  源TIFF缓冲区  = srcW × srcH × 4 bytes (CMYK)
  输出缓冲区    = outW × outH × 4 bytes (CMYK)
```

### 典型场景内存估算

| 场景 | 源图 (单张) | 输出图 | 峰值内存 |
|------|-----------|--------|---------|
| 小图 | 1000×1000 → 4 MB | 1000×1000 → 4 MB | ~8 MB |
| 中图 | 5000×5000 → 100 MB | 5000×5000 → 100 MB | ~200 MB |
| 大图 | 20000×20000 → 1.6 GB | 20000×20000 → 1.6 GB | ~3.2 GB |
| 极限 | 30000×30000 → 3.6 GB | 30000×30000 → 3.6 GB | ~7.2 GB |

> **关键问题**：源图和输出图同时驻留内存，且合成完成后才开始写入文件。对于大图场景，内存占用极高。

---

## 七、性能瓶颈分析

### 瓶颈 1：RGB 源图的逐行 BGRA 中间缓冲区

```cpp
// ImageWorker.cpp:202 — 每行分配一个临时 bgraLine
std::vector<uint8_t> bgraLine(w * 4);
```

对于 20000 宽的 RGB 源图，每行分配 80 KB 临时缓冲区，共 20000 次分配/释放。虽然单次不大，但高频分配带来堆管理开销。

### 瓶颈 2：双线性插值使用 double 浮点运算

```cpp
// ImageWorker.cpp:509-518 — 每像素每通道一次 double 乘法
double p00 = row0[ix0 * 4 + c];
double top = p00 + (p10 - p00) * fx;
double val = top + (bot - top) * fy;
outRow[off + c] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
```

对于 8 位 CMYK 数据，`double` 精度过剩。定点数运算可提速 2-4 倍。

### 瓶颈 3：合成阶段的 buffer 遍历开销

```cpp
// ImageWorker.cpp:472 — 每行遍历所有 buffer
for (int bi = 0; bi < buffers.size(); ++bi) {
```

即使 buffer 的输出矩形不覆盖当前行，仍需执行边界检查。对于大量小 buffer（如几十个矢量图元），每行的遍历开销累积显著。

### 瓶颈 4：整体合成后再写入

Phase 3 完成后，Phase 4 才开始逐行写入。这意味着：
- 合成期间：源缓冲区 + 输出缓冲区同时在内存
- 写入期间：输出缓冲区仍完整占用内存
- 无法利用"合成 N 行 → 写入 N 行 → 释放 N 行"的流式模式

### 瓶颈 5：Overlay 数据拷贝

```cpp
// ImageWorker.cpp:418 — overlay.data 被拷贝而非移动
buf.data = overlay.data; // copy pixel data
```

`CmykOverlay` 的 data 是 `std::vector<uint8_t>`，此处发生深拷贝。如果 overlay 不再使用，可改为移动语义。

### 瓶颈 6：LCMS2 变换句柄的创建/销毁

每个 RGB 源图创建一个 `cmsHTRANSFORM`，使用后销毁。如果多个源图使用相同的 Profile/Intent 组合，可复用句柄。

---

## 八、优化建议

### 建议 1：流式合成 + 写入（内存优化，优先级 ★★★★★）

**目标**：将峰值内存从 `源缓冲区 + 输出缓冲区` 降低为 `源缓冲区 + N 行条带缓冲区`。

**方案**：

```
当前：  [读取全部源图] → [合成全部行到 outBuf] → [逐行写入 outBuf]

优化后：[读取全部源图] → [合成 stripHeight 行] → [写入该 strip]
                         → [合成下一 strip]   → [写入该 strip]
                         → ... 循环直到完成
```

**实现要点**：
- 使用 libtiff 的 Tiled TIFF (`TIFFTAG_TILEWIDTH/TILELENGTH`) 或保持 Strip 模式
- 输出缓冲区仅分配 `stripHeight × outW × 4` 字节（如 stripHeight=64，20000 宽图仅需 5 MB）
- 每个 strip 合成完毕后立即写入并释放
- 源缓冲区仍需完整加载（因双线性插值需要相邻行），但可考虑源图也做分块读取

**预期收益**：大图场景内存降低 50%+（30000×30000 从 ~7.2 GB 降至 ~3.6 GB + 条带开销）

---

### 建议 2：定点数双线性插值（速度优化，优先级 ★★★★）

**目标**：用 8.8 定点数替代 `double` 浮点运算。

**方案**：

```cpp
// 当前 (double)：每像素每通道 ~6 次 double 运算
double top = p00 + (p10 - p00) * fx;
double val = top + (bot - top) * fy;

// 优化 (定点数)：每像素每通道 ~4 次整数运算
const int SHIFT = 8;
int fx_i = static_cast<int>(fx * 256);  // 0~255
int fy_i = static_cast<int>(fy * 256);
for (int c = 0; c < 4; ++c) {
    int top = (p00[c] << SHIFT) + (p10[c] - p00[c]) * fx_i;
    int bot = (p01[c] << SHIFT) + (p11[c] - p01[c]) * fx_i;
    int val = top + ((bot - top) * fy_i >> SHIFT);
    dst[c] = static_cast<uint8_t>(std::clamp(val >> SHIFT, 0, 255));
}
```

**预期收益**：合成阶段提速 2-3 倍（`double` 运算在现代 CPU 上虽不慢，但整数运算仍快且缓存友好）

---

### 建议 3：空间索引优化 buffer 遍历（速度优化，优先级 ★★★）

**目标**：避免每行遍历所有 buffer 做边界检查。

**方案 A — 行区间索引**：

```cpp
// 预处理：计算每个 buffer 覆盖的行范围
struct BufferSpan { int bi, yStart, yEnd; };
std::vector<BufferSpan> spans;
for (int bi = 0; bi < buffers.size(); ++bi) {
    int y0 = std::max(0, static_cast<int>(buffers[bi].outputRect.top()));
    int y1 = std::min(outH, static_cast<int>(buffers[bi].outputRect.bottom()));
    spans.push_back({bi, y0, y1});
}
// 按 yStart 排序
std::sort(spans.begin(), spans.end(), [](auto &a, auto &b){ return a.yStart < b.yStart; });

// 合成时：对每行，只遍历 yStart <= y < yEnd 的 buffer
// 可用二分查找定位起始位置
```

**方案 B — 对每行预计算 active buffer 列表**（适合 buffer 数量较多时）：

```cpp
// 预处理：对每行记录哪些 buffer 活跃
std::vector<std::vector<int>> rowActiveBuffers(outH);
for (int bi = 0; bi < buffers.size(); ++bi) {
    int y0 = ..., y1 = ...;
    for (int y = y0; y < y1; ++y)
        rowActiveBuffers[y].push_back(bi);
}
```

**预期收益**：buffer 数量 > 10 时有明显改善，大量小 overlay 场景尤佳

---

### 建议 4：Overlay 数据移动语义（内存优化，优先级 ★★★）

**当前问题**：

```cpp
// ImageWorker.cpp:418 — 深拷贝
buf.data = overlay.data;
```

`CmykOverlay` 在 `mainwindow.cpp` 中通过 `std::move(overlays)` 传入 `QThread::create` 的 lambda，之后不再使用。但 `exportTiff()` 接收的是 `const QList<CmykOverlay>&`，无法移动。

**方案**：

```cpp
// 接口改为接受右值引用
ExportWorkerResult exportTiff(const QString &outputPath,
                              QList<SourceTiffInput> &&sources,
                              QList<CmykOverlay> &&overlays, ...);

// 或在 exportTiff 内部对 data 使用移动
buf.data = std::move(const_cast<CmykOverlay&>(overlay).data); // hack
// 更好的做法：将 CmykOverlay 的 data 声明为 mutable 或提供 move 接口
```

**预期收益**：避免大量像素数据的深拷贝，overlay 越大收益越明显

---

### 建议 5：LCMS2 变换句柄复用（速度优化，优先级 ★★）

**当前问题**：每个 RGB 源图创建一个 `cmsHTRANSFORM`，使用后销毁。Profile/Intent/Flags 相同时句柄完全等价。

**方案**：

```cpp
// 在 exportTiff 中创建一个共享句柄（LCMS2 transform 线程安全用于只读）
cmsHTRANSFORM sharedTransform = nullptr;
if (QATColorManager::instance().isValid()) {
    sharedTransform = QATColorManager::instance().createBgraToCmyk8(
        INTENT_PERCEPTUAL, cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC);
}
// 传给所有 readSourceToCmyk 调用
// 最后统一销毁
```

**注意**：LCMS2 的 `cmsHTRANSFORM` 在只读使用时是线程安全的（`cmsDoTransform` 内部无状态修改），可安全共享。

**预期收益**：多个 RGB 源图时减少 transform 创建开销（通常较小，但属于免费优化）

---

### 建议 6：LCMS2 转换使用 SIMD 加速路径（速度优化，优先级 ★★）

**当前代码**：

```cpp
// ImageWorker.cpp:219 — 逐行调用 cmsDoTransform
QATColorManager::convertBgra8ToCmyk8(rgbToCmyk, bgraLine.data(), dst, static_cast<int>(w));
```

每行调用一次 `cmsDoTransform`，LCMS2 内部有优化但调用频繁。

**方案**：对 CONTIG 模式的 RGB 源图，读取完整缓冲区后一次性转换：

```cpp
// 读取完整 RGB 数据到临时缓冲区
std::vector<uint8_t> rgbBuf(w * h * samplesPerPixel);
// ... 读取所有行到 rgbBuf ...

// 展开为 BGRA
std::vector<uint8_t> bgraBuf(w * h * 4);
// ... 一次性展开 ...

// 一次性转换（利用 LCMS2 的批量优化）
cmsDoTransform(rgbToCmyk, bgraBuf.data(), buf.data(), w * h);
```

**预期收益**：减少 `cmsDoTransform` 调用次数（从 h 次降为 1 次），LCMS2 内部可更好地利用 SIMD

---

### 建议 7：LZW 压缩预估 + Predictor（速度优化，优先级 ★★）

**当前问题**：默认 LZW 压缩，未使用 Predictor。

**方案**：

```cpp
// 添加 Horizontal Differencing Predictor
// 对于 CMYK 图像，相邻像素通常相似，差分后压缩率更高、压缩更快
TIFFSetField(tif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
```

**预期收益**：
- 压缩率提升 10-30%（CMYK 图像相邻像素高度相关）
- 压缩速度提升（差分后的数据更易压缩）
- 写入 I/O 减少

---

### 建议 8：并行 TIFF 写入（速度优化，优先级 ★）

**方案**：使用 libtiff 的多条带并行写入（需 libtiff 4.x + 编译时开启并发支持）：

```cpp
// 设置较大的 strip 大小以减少 I/O 次数
tmsize_t stripSize = static_cast<tmsize_t>(outW) * 4 * 64; // 64 行一个 strip
TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 64);

// 多线程写入不同 strip（需 libtiff 编译时支持）
```

**注意**：libtiff 默认非线程安全写入，此优化需要额外的同步机制或使用 tiled TIFF + `TIFFWriteTile()`。

---

### 建议 9：源图分块读取（内存优化，优先级 ★★）

**当前问题**：每个源 TIFF 完整读入内存（`w × h × 4` 字节）。

**方案**：对大源图使用 libtiff 的 Tile API 分块读取：

```cpp
// 检查源 TIFF 是否为 tiled 格式
if (TIFFIsTiled(tif)) {
    uint32_t tileW, tileH;
    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileW);
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileH);
    // 按 tile 读取，合成时按需加载
}
```

**限制**：需要修改合成阶段为"按需读取源 tile"模式，改动较大。适合源图远大于输出图的裁剪场景。

---

### 建议 10：利用 ExportImageDialog 的丰富选项（功能完善，优先级 ★★★）

**当前问题**：`ExportImageDialog` 定义了丰富的导出参数（压缩类型、字节序、位深、平面配置、Predictor、JPEG 质量、ICC 嵌入、元数据保留），但 `TiffExportSettings` 仅使用了 `dpi`、`compression`、`iccProfilePath` 三个字段。

**方案**：将 `ExportImageDialog` 的参数完整传递到 `TiffExportSettings`：

```cpp
struct TiffExportSettings {
    int dpi = 300;
    uint16_t compression = COMPRESSION_LZW;
    uint16_t predictor = PREDICTOR_HORIZONTAL;  // 新增
    uint16_t bitsPerSample = 8;                  // 新增
    uint16_t sampleFormat = SAMPLEFORMAT_UINT;   // 新增
    uint16_t planarConfig = PLANARCONFIG_CONTIG;  // 新增
    int jpegQuality = 80;                         // 新增
    bool embedIcc = true;                         // 新增
    QString iccProfilePath;
};
```

---

## 九、优化优先级总览

| 优先级 | 建议 | 类型 | 难度 | 收益 |
|--------|------|------|------|------|
| ★★★★★ | 流式合成+写入 (建议1) | 内存 | 高 | 大图内存降低 50%+ |
| ★★★★ | 定点数插值 (建议2) | 速度 | 低 | 合成提速 2-3× |
| ★★★ | 空间索引 buffer 遍历 (建议3) | 速度 | 中 | 多 buffer 场景提速 |
| ★★★ | Overlay 移动语义 (建议4) | 内存 | 低 | 避免大缓冲区拷贝 |
| ★★★ | 导出参数透传 (建议10) | 功能 | 低 | 解锁已有 UI 选项 |
| ★★ | LCMS2 句柄复用 (建议5) | 速度 | 低 | 多源图微小提速 |
| ★★ | LCMS2 批量转换 (建议6) | 速度 | 中 | 减少函数调用开销 |
| ★★ | Predictor 优化 (建议7) | 速度/IO | 低 | 压缩率提升 10-30% |
| ★★ | 源图分块读取 (建议9) | 内存 | 高 | 裁剪场景内存大幅降低 |
| ★ | 并行 TIFF 写入 (建议8) | 速度 | 高 | I/O 密集场景提速 |

---

## 十、建议实施路径

### 第一阶段（低成本高收益）

1. **建议 4**：Overlay 移动语义 — 改接口签名，1-2 小时
2. **建议 7**：添加 Predictor — 一行代码改动
3. **建议 2**：定点数插值 — 替换合成循环中的 double 运算

### 第二阶段（中等投入）

4. **建议 3**：行区间索引 — 预处理 + 修改合成循环
5. **建议 6**：LCMS2 批量转换 — 重构 readSourceToCmyk 的 RGB 读取路径
6. **建议 10**：导出参数透传 — 扩展 TiffExportSettings + 更新 exportTiff

### 第三阶段（架构级改动）

7. **建议 1**：流式合成+写入 — 需要重构 Phase 3 和 Phase 4 为流式管线
8. **建议 9**：源图分块读取 — 需要重构 readSourceToCmyk + 合成逻辑

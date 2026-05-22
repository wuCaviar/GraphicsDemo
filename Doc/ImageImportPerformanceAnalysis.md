# 图片导入性能优化分析

## 1. 当前导入流程概览

```
用户点击导入 → 文件选择对话框 → 依次 QImageReader::size() 预检
    → 弹出缩放确认 → QtConcurrent::mapped() 线程池并行加载
        → 每个文件: loadImageFromFile() → runImportWorker()
    → resultReadyAt 信号 → 主线程创建 ImageItem 并加入场景
```

### 1.1 关键文件

| 文件 | 职责 |
|---|---|
| `Src/UI/mainwindow.cpp:859-954` | 导入入口 `onImportImage()` |
| `Src/Utils/ImageUtils.cpp:321-349` | `loadImageFromFile()` 核心加载 |
| `Src/Utils/ImageUtils.cpp:109-319` | `importTiffWithLibtiff()` TIFF 专用解析 |
| `Src/Utils/ImageUtils.cpp:57-107` | `imageToCmykBuffer()` RGB→CMYK 逐像素转换 |
| `Src/Utils/ImageWorker.cpp:122-151` | `runImportWorker()` 线程池入口 |
| `Src/Items/ImageItem.cpp` | ImageItem 构造与序列化 |

---

## 2. 性能瓶颈分析

### 瓶颈 1（严重）：逐像素调用 LCMS2 色彩转换

**位置**: `ImageUtils.cpp:57-107`，`imageToCmykBuffer()`

**问题**:
```cpp
for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
        QATColorManager::Cmyk cmyk = cm.toCmyk(QColor(c)); // 每次构造 QColor + 单像素 LCMS2 调用
    }
}
```

- 对每个像素都构造一个 `QColor` 临时对象，然后调用 `cmsDoTransform()` 转换 **单个像素**
- LittleCMS 的 `cmsDoTransform()` 设计为**批量处理**扫描线或整张图，每次函数调用有固定开销
- 对于 4000×3000（1200万像素）的图片，这意味着 **1200 万次** `QColor` 构造 + LCMS2 函数调用
- 批量调用 `cmsDoTransform()` 一次处理整行或整张图，内部使用 SIMD 优化

**影响**: 大图 RGB→CMYK 转换耗时可能是批量方式的 **20-50 倍**

**优化方案**:
```cpp
// 使用 cmsDoTransform 批量转换整个扫描线
cmsHTRANSFORM xform = cm.rgb2cmykTransform(); // 新增接口暴露 transform handle
QVector<quint8> inputLine(w * 3);   // RGB
QVector<quint8> outputLine(w * 4);  // CMYK
for (int y = 0; y < h; ++y) {
    // 填充 inputLine
    const QRgb *src = reinterpret_cast<const QRgb *>(img32.constScanLine(y));
    for (int x = 0; x < w; ++x) {
        inputLine[x*3+0] = qRed(src[x]);
        inputLine[x*3+1] = qGreen(src[x]);
        inputLine[x*3+2] = qBlue(src[x]);
    }
    cmsDoTransform(xform, inputLine.data(), outputLine.data(), w);
    // 复制 outputLine 到 RawPixelBuffer
}
```

---

### 瓶颈 2（严重）：TIFF 文件双重读取

**位置**: `ImageUtils.cpp:321-349`，`loadImageFromFile()`

**问题**:
```cpp
ImportResult loadImageFromFile(const QString &path)
{
    QImageReader reader(path);      // 第一次读取：解码为 QImage 用于显示
    result.image = reader.read();

    if (isTiffFile(path)) {
        importTiffWithLibtiff(path, &result);  // 第二次读取：libtiff 重新打开文件读原始数据
    }
}
```

- 对于 TIFF 文件，先用 `QImageReader` 完整解码一次（QImage），再用 `libtiff` 重新打开文件、解析条带/扫描线（CMYK 原始数据）
- **两次 I/O** + 两次解码，导入时间翻倍
- 非 TIFF 格式不受影响（PNG、JPEG、BMP 只读一次）

**影响**: TIFF 大文件（如 5000×5000 CMYK TIFF）导入耗时翻倍

**优化方案**:
- 对 TIFF 文件，**跳过** `QImageReader`，直接用 libtiff 读取：
  - RGB TIFF: `TIFFReadRGBAImage()` 得到显示用 RGBA 数据 → 构造 QImage
  - CMYK TIFF: 读原始 CMYK 扫描线 → 用批量 LCMS2 反向转换为 RGB QImage 用于显示
- 非 TIFF 格式保持 QImageReader 路径不变

```cpp
ImportResult loadImageFromFile(const QString &path)
{
    if (isTiffFile(path)) {
        return loadTiffWithLibtiff(path); // 一步到位，不再走 QImageReader
    }
    // 非 TIFF：原有 QImageReader 路径
    ...
}
```

---

### 瓶颈 3（中等）：尺寸预检串行执行

**位置**: `mainwindow.cpp:878-900`

**问题**:
```cpp
for (const QString &path : paths) {
    QImageReader reader(path);
    const QSize sz = reader.size();  // 串行读取每个文件的头部信息
    ...
}
```
- `QImageReader::size()` 只读文件头，不解码像素，单次很快
- 但选择 50 个文件时，串行执行意味着用户要等待 N×文件头读取时间才能看到进度条
- 可以**并行化**或合并到异步加载流程中

**优化方案**:
```cpp
// 方案 A: 使用 QtConcurrent::blockingMapped 并行预检
auto sizes = QtConcurrent::blockingMapped(paths, [](const QString &path) {
    return QImageReader(path).size();
});
```

或者直接去掉预检步骤，改为在 worker 内部判断是否需要缩放，避免两次读取文件头。

---

### 瓶颈 4（中等）：主线程 `QPixmap::fromImage()` 转换

**位置**: `mainwindow.cpp:923-924`

**问题**:
```cpp
auto *item = new ImageItem(QPixmap::fromImage(result.importResult.image));
```
- `QPixmap::fromImage()` 在**主线程**执行，将 QImage 转换为 GPU 可用的像素图
- 对于大图（如 4000×3000），这涉及隐式的格式转换和可能的深拷贝，耗时可达 50-200ms
- 批量导入时，每个文件完成都触发一次 `resultReadyAt`，主线程可能因连续的大图转换而卡顿

**优化方案**:
- 在 worker 线程中预先调用 `QPixmap::fromImage()`，将 `ImportWorkerResult` 中的 `QImage` 替换为 `QPixmap`（QPixmap 不能跨线程传递，但可以在 worker 中构造后通过 `QFuture` 返回给主线程使用——实际上 QPixmap 本身是线程不安全的，所以更好的做法是用 `QImage` 传输，但可以让 worker 预先完成格式转换）
- 更好的做法：延迟创建 QPixmap，在 `ImageItem::paint()` 首次调用时才转换，使用 `QImage` 作为中间存储

---

### 瓶颈 5（中等）：内存中多份像素数据共存

**问题**:
每个 ImageItem 在内存中持有：
- `QGraphicsPixmapItem::pixmap()` — 显示用 QPixmap（GPU 内存/共享内存）
- `m_rawTiffMat` — 原始 TIFF 像素（用于无损导出）
- `m_rawCmykMat` — CMYK 像素（用于 CMYK 导出覆写）

对于 4000×3000 的 RGBA 图片：
- QPixmap: ~48 MB（4000×3000×4）
- rawTiffMat: ~48 MB
- rawCmykMat: ~48 MB
- 总计: ~144 MB / 每张图

**优化方案**:
- `m_rawTiffMat` 在非 TIFF 来源时几乎不使用，可以延迟加载或仅在需要时才保留
- `m_rawCmykMat` 可以考虑使用压缩存储（zlib），访问时解压
- 引入 LRU 缓存：未选中/不可见的 ImageItem 释放原始数据，用户操作时重新加载

---

### 瓶颈 6（低）：`QImageReader` 未使用解码优化参数

**位置**: `ImageUtils.cpp:326-328`

**问题**:
```cpp
QImageReader reader(path);
reader.setAllocationLimit(0);  // 关闭内存保护，可能导致 OOM
result.image = reader.read();
```

**优化方案**:
- `setAllocationLimit(0)` 关闭了 Qt 的内存保护机制，建议设为合理值（如 512MB），超限时拒绝加载
- 当需要缩放时，使用 `reader.setScaledSize(targetSize)` 让解码器直接解码到目标分辨率，而非先解码全尺寸再 `QImage::scaled()`，省去一次完整的解码+缩放过程

```cpp
if (scaleToFit) {
    reader.setScaledSize(targetSize);  // 解码器内部降采样，更快更省内存
}
```

---

### 瓶颈 7（低）：非 TIFF 格式的 CMYK 转换必要性

**位置**: `ImageUtils.cpp:342-345`

**问题**:
```cpp
if (!result.image.isNull()) {
    result.rawCmykMat = imageToCmykBuffer(result.image);
}
```
- 对 PNG/JPEG/BMP 等所有非 TIFF 格式都无条件执行 RGB→CMYK 转换
- 如果用户从不导出 CMYK TIFF，这些转换完全浪费

**优化方案**:
- 延迟计算（lazy evaluation）：首次访问 CMYK 数据时才转换
- 或添加标志位，仅用户启用 CMYK 导出时才执行转换

---

## 3. 优化方案优先级汇总

| 优先级 | 瓶颈 | 方案 | 预期收益 | 实现难度 | 风险 |
|---|---|---|---|---|---|
| **P0** | #1 逐像素 LCMS2 | 批量 `cmsDoTransform()` | RGB→CMYK 提升 20-50x | 低（改几十行） | 低 |
| **P0** | #2 TIFF 双重读取 | 统一走 libtiff，跳过 QImageReader | TIFF 导入 2x 提升 | 中（需处理 CMYK→RGB 反向转换） | 中 |
| **P1** | #6 未用 scaledSize | QImageReader 解码时降采样 | 缩放场景 2-5x 提升 + 省内存 | 低（改几行） | 低 |
| **P1** | #4 主线程 pixmap 转换 | worker 线程预处理图像格式 | UI 流畅度提升 | 中 | 中（需确认 QImage 线程安全边界） |
| **P2** | #3 串行预检 | 并行预检或移入 worker | 批量导入延迟降低 | 低 | 低 |
| **P2** | #5 内存多份拷贝 | 延迟加载 rawTiffMat / rawCmykMat | 内存占用降低 50-66% | 中 | 中（需改动序列化） |
| **P3** | #7 无条件 CMYK 转换 | 延迟 CMYK 转换 | CPU 节省 | 低 | 低 |

---

## 4. 建议实施顺序

### 第一阶段（快速收益，1-2天）

1. **批量 LCMS2 转换** — 修改 `imageToCmykBuffer()` 和 `exportTiffCmykFromSnapshot()`，使用 `cmsDoTransform()` 整行批量处理
2. **QImageReader::setScaledSize()** — 缩放场景下让解码器直接降采样

### 第二阶段（核心优化，2-3天）

3. **TIFF 统一加载路径** — `loadImageFromFile()` 中 TIFF 文件直接走 libtiff，不再双重读取
4. **恢复合理的内存分配限制** — 替代 `setAllocationLimit(0)`

### 第三阶段（体验优化，1-2天）

5. **并行预检** — 消除串行等待
6. **延迟 CMYK 转换** — 仅当需要导出/显示 CMYK 信息时才转换

---

## 5. 实现注意事项

### 5.1 QATColorManager 接口扩展

需要在 `colortransform.h` 中暴露 `cmsHTRANSFORM` handle：

```cpp
class QATColorManager {
public:
    cmsHTRANSFORM rgb2cmykTransform() const;  // 批量转换用
    cmsHTRANSFORM cmyk2rgbTransform() const;  // CMYK TIFF → RGB 显示用
};
```

### 5.2 TIFF CMYK → RGB 显示转换

当跳过 QImageReader 直接用 libtiff 读取 CMYK TIFF 时，需要将 CMYK 原始数据转为 RGB QImage 用于屏幕显示：

```cpp
QImage cmykToDisplayRgb(const RawPixelBuffer &cmykMat) {
    QImage img(cmykMat.width, cmykMat.height, QImage::Format_ARGB32);
    cmsHTRANSFORM xform = QATColorManager::instance().cmyk2rgbTransform();
    // 批量 cmsDoTransform() 逐行转换
    return img;
}
```

### 5.3 回归测试要点

- TIFF 格式: CMYK / RGB / 灰度 / 16-bit / 压缩 (LZW/ZIP) / tiled / 多页
- 非 TIFF 格式: PNG (透明/不透明) / JPEG / BMP
- 缩放场景: 超大图缩放适配画布
- CMYK 导出: 验证导入图的 CMYK 数据在导出后色彩一致
- 序列化: 粘贴板复制粘贴、撤销/重做
- 批量导入: 多文件同时导入进度正确

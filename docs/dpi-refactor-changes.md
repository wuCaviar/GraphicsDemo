# DPI 解耦画布改造 — 修改记录

> 日期: 2026-06-08 | 分支: dev_wxy_dpi

## 核心设计变更

**之前：** 画布 DPI 被第一张导入的 TIFF 图片锁定，不同 DPI 图片被拒绝导入，图片禁止拖拽缩放，导出 DPI 自动检测。

**之后：** 画布以物理 mm 为单位，仅保留 displayPpi 用于 mm↔px 显示换算，多 DPI 图片按真实物理尺寸共存，图片可拖拽缩放，导出 DPI 由用户指定。

### 坐标模型

```
场景像素 = 物理mm × displayPpi / 25.4
图片导入: 物理mm = 原始像素 / 原始DPI × 25.4 → 场景像素 = 物理mm × displayPpi / 25.4
缩放后: scaleX = scaleX × (新宽/旧宽)
导出: 目标像素 = 原始物理mm × scale × exportDpi / 25.4
```

---

## 所有修改的文件（共11个）

### 1. `src/Items/CanvasItem.h` — 去 DPI 锁定，改 mm 坐标系

**删除：**
- `m_canvasDpiX`, `m_canvasDpiY`, `m_dpiLocked` 成员
- `setCanvasDpi()`, `canvasDpiX()`, `canvasDpiY()`, `isDpiLocked()`, `lockDpi()`, `unlockDpi()`, `updateEffectivePpi()` 方法

**新增：**
- `m_physicalWidthMm` (默认 210.0), `m_physicalHeightMm` (默认 297.0) — 画布物理尺寸
- `displayPpi()` — 返回当前显示 PPI（仅用于换算）
- `setDisplayPpi(qreal ppi)` — 设置显示 PPI 并重算像素 rect
- `setCanvasSizeMm(qreal wMm, qreal hMm)` — 按 mm 设置尺寸
- `canvasWidthMm()`, `canvasHeightMm()` — 查询物理尺寸
- 新增构造函数 `CanvasItem(qreal widthMm, qreal heightMm, qreal displayPpi = 300.0, ...)`
- `updatePixelRect()` 私有方法 — 从 mm + displayPpi 计算像素 rect
- 保留 `setPpi()` 作为 `setDisplayPpi()` 的别名（兼容旧调用方）

### 2. `src/Items/CanvasItem.cpp` — 对应实现

- `setDisplayPpi()`: 保持物理 mm 不变，仅重算像素 rect
- `setCanvasSizeMm()`: 存储 mm 值，调用 `updatePixelRect()`
- `updatePixelRect()`: `setRect(0, 0, mmW * ppi / 25.4, mmH * ppi / 25.4)`
- 移除 `setCanvasDpi()`, `lockDpi()`, `unlockDpi()`, `updateEffectivePpi()` 实现

### 3. `src/Items/ImageItem.h` — 启用缩放，添加比例追踪

**改动：**
- `isResizable()` → `return true`（之前 `false`）
- `propertyFlags()` → `return HasImage | HasRotation`（新增 `HasRotation`）
- 新增覆写: `supportsGeometryRect()` → true, `supportsSetGeometryRect()` → true, `setGeometryRect()`

**新增成员：**
- `m_scaleX = 1.0`, `m_scaleY = 1.0` — 累计缩放比例
- `m_originalPhysicalMm` — 导入时的原始物理尺寸 (mm)

**新增方法：**
- `scaleX()`, `scaleY()`, `setScale(sx, sy)`
- `originalPhysicalMm()`, `setOriginalPhysicalMm(mm)`
- `targetOutputPixels(int exportDpi)` — 计算导出像素尺寸

### 4. `src/Items/ImageItem.cpp` — 对应实现

- `cloneItem()`: 额外复制 `m_scaleX/Y`, `m_originalPhysicalMm`
- `setGeometryRect(r)`: 调用 `setRect(r)` 并更新 `m_scaleX` / `m_scaleY`
- `targetOutputPixels(exportDpi)`: `qMax(1, round(physW * scaleX / 25.4 * exportDpi))`
- `serialize()`: 写入标记字节 `2`，然后 `m_scaleX`, `m_scaleY`, `physW`, `physH`，最后 CMYK
- `deserialize()`: 读取标记字节；`2` = 新格式（有 scale+physMm），`1` = 旧格式 CMYK 标记；旧项目默认 scale=1.0 并从 `m_originalSize` + `m_dpiX/Y` 回退计算物理 mm

### 5. `src/Items/ResizeHandleItem.cpp` — 移除图片缩放屏蔽

**删除：**
- `containsImageItem()` 整个函数（递归检查函数）
- `updateHandlePositions()` 中的 `shouldSuppressHandles` 逻辑（3 个 if 分支）
- `applyResize()` 中的 `!isResizable()` 守卫（`if (igi && !igi->isResizable()) return;`）
- `applyGroupResize()` 中的 `containsImageItem()` 守卫

### 6. `src/UI/PropertyPanel.h` — 新增图片缩放信息标签

新增成员: `QLabel *m_imgScaleLabel`, `QLabel *m_imgEffectiveSizeLabel`

### 7. `src/UI/PropertyPanel.cpp` — 对应显示逻辑

- `setupUI()`: 在 Image Info 组中新增 "Scale:" 和 "Effective Size:" 两行
- `updatePanel()`: 图片选中时显示缩放比例（如 "1.50x / 1.50x"）和有效物理尺寸（`originalPhysicalMm × scale`）

### 8. `src/UI/mainwindow.h` — 移除 DPI 锁定声明

删除 `_tryLockCanvasDpi()`, `_unlockCanvasDpiIfNoImages()` 声明

### 9. `src/UI/mainwindow.cpp` — 主要业务流程改造

**删除：**
- `_tryLockCanvasDpi()` 整个函数（原来的画布 DPI 锁定/匹配/拒绝逻辑）
- `_unlockCanvasDpiIfNoImages()` 整个函数

**`_updateCanvasLabel()` 改造：**
- `canvas->ppi()` → `canvas->displayPpi()`
- 删除 `isDpiLocked()` 条件分支，始终显示 display PPI

**`onNew()` 改造：**
- 删除 `canvas->setCanvasDpi(0, 0)`
- 改用 `canvas->setDisplayPpi(kDefaultPpi)`

**`onOpenProject()` 改造：**
- `canvas->setPpi(ppi)` → `canvas->setDisplayPpi(ppi)`
- 删除 `canvas->setCanvasDpi()`, `canvas->lockDpi()` 调用

**`onSaveProject()` 改造：**
- `canvasInfo.dpi = canvas->displayPpi()`（之前是 `isDpiLocked() ? canvasDpiX() : 0`）

**`importSingleImage()` 改造（resultReadyAt lambda）：**
- 移除 `_tryLockCanvasDpi()` 调用
- 改为物理 mm 计算:
  ```cpp
  imgDpi = result.dpiX > 0 ? result.dpiX : 72
  physW = result.size.width() / imgDpi * 25.4
  pxW = physW * canvas->displayPpi() / 25.4
  item->setDpi(imgDpiX, imgDpiY)
  item->setOriginalPhysicalMm(physW, physH)
  item->setRect(QRectF(0, 0, pxW, pxH))
  ```
- 垂直偏移用 `pxH` 代替 `result.size.height()`

**`importMultipleImages()` 改造：**
- 删除 `commonDpi` 共享状态和 DPI 过滤逻辑
- 每个图片独立按物理尺寸导入（同上）
- 水平/垂直偏移用 `pxW`/`pxH`

**`onDelete()` 改造：**
- 删除 `_unlockCanvasDpiIfNoImages()` 调用

**`onExportImage()` 改造：**
- 删除条件判断（hasImages / isDpiLocked）
- 始终弹出 DPI 选择对话框（kDpiValues 预设，默认 index=2 即 300 DPI）
- 删除 DPI 覆盖时的画布缩放逻辑
- `dpiOverride` → `exportDpi`，传递给 `tiffEngine->startExport()`

**`onResizeCanvas()` 改造：**
- `canvas->ppi()` → `canvas->displayPpi()`

### 10. `src/UI/TiffExportEngine.h` — 导出 API 改造

- `startExport()`: `dpiOverride` 参数改为 `exportDpi`，移除默认值 `= 0`
- `determineTargetDpi()`: 改为 `static int determineTargetDpi(int exportDpi) { return exportDpi; }`（直接透传）
- `buildSources()`: 签名新增 `int exportDpi` 参数

### 11. `src/UI/TiffExportEngine.cpp` — 导出引擎实现

- 删除旧的 `determineTargetDpi()` 实现（含 `isDpiLocked()` 和 `imageItems.first()->dpiX()` 的自动检测逻辑）
- `buildSources()`: 对每个 ImageItem 计算 `targetOutputPixels(exportDpi)`，设置 `src.targetPixelSize` 和 `src.needsResample`
- `startExport()`: `targetDpi` 由 `determineTargetDpi(exportDpi)` 获取，删除 ≤0 时的错误提示

### 12. `src/Utils/ImageWorker.h` — 数据结构扩展

`SourceTiffInput` 新增:
```cpp
QSize targetPixelSize;      // 重采样目标像素尺寸（0x0 = 无需重采样）
bool needsResample = false; // 是否需从原始分辨率重采样
```

### 13. `src/Utils/ProjectFile.h` — 序列化兼容

`CanvasInfo` 新增:
```cpp
double canvasWidthMm = 210.0;   // 画布物理宽 (mm)
double canvasHeightMm = 297.0;  // 画布物理高 (mm)
```

### 14. `src/Utils/ProjectFile.cpp` — XML 序列化改造

**保存：**
- 旧 `<Dpi>` → 新 `<DisplayPpi>`（存储 displayPpi 值）
- 新增 `<WidthMm>`, `<HeightMm>` 元素

**加载：**
- 优先读 `<DisplayPpi>`，回退读旧 `<Dpi>` 标签
- 读取 `<WidthMm>`, `<HeightMm>`（可选，旧文件不存在时用默认值）
- `canvas.dpi` 始终存储 displayPpi（不再为 0）

---

## 待完成工作

1. **stb_image_resize2 重采样集成** — 导出时对 `needsResample = true` 的源图像进行重采样。需在 `ImageWorker.cpp` 或 `SourceReader.cpp` 中添加 CMYK 四通道独立重采样逻辑，支持 strip 分块处理超大图像
2. **显示 PPI 切换** — 当用户通过设置改变显示 PPI 时，重缩放所有图元（pos 和 m_rect）按 `newPpi/oldPpi` 比例
3. **实际运行测试** — 导入不同 DPI 图片、拖拽缩放、撤销/重做、导出验证

## 之后的补充修改 (2026-06-08 第二轮)

### 修复导出 TIFF 画布比图片大 4 倍

**根因：** `startExport()` 用 `exportRect.size().toSize()` 作为输出像素尺寸（场景 px ≠ 导出 px），`buildSources()` 硬编码了 `exportDpi / 300.0`。

**修复：** 从 CanvasItem 获取真实 displayPpi，统一用 `场景像素 × exportDpi / displayPpi` 换算所有导出坐标。
- `buildSources()` 签名新增 `qreal displayPpi` 参数
- `startExport()` 提前获取 displayPpi，同时传给 `buildSources()` 和用于计算 `outSize`

### 状态栏隐藏 DPI

- `_updateCanvasLabel()`: 只显示 `"Canvas: W × H mm"`，去掉 `Display: N PPI`

### 导入简化

- 删除 `importSingleImage()` — 合并到 `importMultipleImages()`
- 去掉 `ImageArrangementDialog` — 不再按 DPI 分组筛选，直接导入所有文件
- 默认垂直排列

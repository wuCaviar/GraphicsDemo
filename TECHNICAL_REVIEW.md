# GraphicsDemo 项目技术评审报告

**评审日期：** 2026-05-10
**Qt 版本：** 6.11
**构建路径：** /Volumes/Caviar/Test/GraphicsDemo/Build

---

## 审查概览

| 维度 | 发现问题数 | 严重/高/中/低 |
|------|-----------|--------------|
| 核心架构 (Items/Commands) | 16 | 6 / 4 / 4 / 2 |
| UI 层 | 18 | 5 / 3 / 6 / 4 |
| 构建系统/工具链/测试 | 20 | 0 / 6 / 8 / 6 |
| **合计** | **54** | **11 / 13 / 18 / 12** |

---

## 一、严重问题 (Critical) — 已修复

### 1. Command 析构函数存在 Use-After-Free 风险 [已修复]

**文件：** `Src/Commands/Commands.cpp:19-27`, `Commands.h`

`AddItemCommand`、`RemoveItemsCommand`、`PasteItemsCommand` 的析构函数忽略了 `m_owned` 标志。当 item 被外部删除（非通过命令系统）时，`m_item` 变成悬空指针，析构函数会访问已释放内存。

**修复方案：** 析构函数检查 `m_owned` 标志。`RemoveItemsCommand` 和 `PasteItemsCommand` 新增 `m_owned` 成员，在 `undo()`/`redo()` 中正确维护所有权状态。

### 2. RotationChangeCommand 构造函数修改活跃对象状态

**文件：** `Src/Commands/Commands.cpp:358-383`

构造函数临时修改 item 的旋转角度来计算补偿位置，然后恢复。如果构造期间发生异常，item 会停留在错误状态。

**建议修复：** 使用纯数学计算旋转补偿，不修改 item 状态：

```cpp
QTransform t;
t.rotate(newRotation);
QPointF newCenter = t.map(oldCenter - pivot) + pivot;
```

### 3. ImageItem::setItemImage() 缺少 prepareGeometryChange() [已修复]

**文件：** `Src/Items/ImageItem.cpp:35-44`

修改 `m_rect` 后只调用 `update()`，没有调用 `prepareGeometryChange()`。违反 `QGraphicsItem` 契约，会导致渲染残留和碰撞检测错误。

**修复方案：** 在修改 `m_rect` 之前添加 `prepareGeometryChange()` 调用。

### 4. 所有 deserialize() 不检查流状态 [已修复]

**文件：** 全部 7 个 Item 类的 `.cpp` 文件

所有 `deserialize()` 方法从 `QDataStream` 读取但从不检查 `in.status()`。剪贴板数据损坏会导致 item 处于不一致状态。

**修复方案：** 接口改为 `bool deserialize(QDataStream &in)`，读取后检查流状态，失败返回 `false`。调用方 `pasteItemsFromClipboard()` 检查返回值，失败时删除 item。

### 5. 序列化版本号 kSerializationVersion 是死代码

**文件：** `Src/Items/IGraphicsItem.h:17`

定义了 `kSerializationVersion = 1`，但没有任何 serialize/deserialize 方法写入或读取版本号。向前兼容不可能实现。

**建议修复：** 在每个 `serialize()` 开头写入版本，`deserialize()` 开头读取并校验。

### 6. 导出对话框取消被忽略 [已修复]

**文件：** `Src/UI/mainwindow.cpp:693-698`

用户点击"取消"后，导出仍然使用默认参数继续执行。

**修复方案：** 添加 `if (dialog.exec() != QDialog::Accepted) return;`

### 7. TIFFWriteScanline 返回值未检查 [已修复]

**文件：** `Src/UI/mainwindow.cpp:808`

磁盘满或写入错误时，函数继续写入损坏数据并返回 `true` 表示"成功"。

**修复方案：** 检查返回值，失败时中断循环、关闭文件、返回 `false`。

---

## 二、高优先级问题 (High)

### 8. ImageUtils 重复读取图片

**文件：** `Src/Utils/ImageUtils.cpp:174-187`

非 TIFF 图片被 `QImageReader::read()` 读取两次。第一次读取消耗了流，第二次读取必然失败或读到空数据。

**建议修复：** 删除第二次读取，使用第一次读取结果。

### 9. TIFF JPEG 压缩参数设置错误

**文件：** `Src/Utils/ImageUtils.cpp:308-309`

将 JPEG quality (1-100) 传给了 `QImageWriter::setCompression()`，而 TIFF 的 `setCompression()` 期望的是压缩方案枚举 (0=None, 1=LZW)。

**建议修复：** 对 TIFF+JPEG 组合，应使用 `TIFFSetField(tif, TIFFTAG_JPEGQUALITY, quality)` 而非 `writer.setCompression()`。

### 10. applyDpiToImage 是空函数

**文件：** `Src/Utils/ImageUtils.cpp:331-338`

所有参数标记 `Q_UNUSED`，函数体为空，但 Header 中有声明和文档。这是未完成的功能。

**建议修复：** 要么实现功能，要么删除声明并标注 TODO。

### 11. cloneItem() 缺少 transformOriginPoint 复制

**文件：** 所有 7 个 Item 的 `cloneItem()` 方法

复制了 `pos()`、`rotation()`、`transform()`，但遗漏了 `transformOriginPoint()`。用户设置自定义旋转中心后，克隆体旋转行为会与原件不同。

**建议修复：** 在每个 `cloneItem()` 中添加 `item->setTransformOriginPoint(transformOriginPoint())`。

### 12. 构建系统缺少编译器警告标志

**文件：** `Src/Src.pro`

没有任何 `-Wall`、`-Wextra`、`-Wpedantic` 配置。项目使用 `reinterpret_cast`、`dynamic_cast` 和底层 TIFF I/O，缺少警告等于盲飞。

**建议修复：**
```qmake
QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
```

### 13. 主构建未显式指定 C++ 标准

**文件：** `Src/Src.pro`

没有 `CONFIG += c++17`，而测试项目有。依赖 Qt 默认行为是不可靠的。

**建议修复：** 添加 `CONFIG += c++17`。

### 14. "ZIP Horizontal Differencing" 复选框是死 UI

**文件：** `Src/UI/ExportImageDialog.cpp:90-91`

复选框创建为局部变量，值从未被读取。用户交互完全无效。

**建议修复：** 将复选框存储为成员变量，在 `getParameters()` 中读取其值。

---

## 三、中优先级问题 (Medium)

| # | 问题 | 位置 |
|---|------|------|
| 15 | Command 中并行列表访问无范围检查，尺寸不匹配时越界 | Commands.cpp:110,276,314 |
| 16 | PropertyChangeCommand Geometry 分支用 qgraphicsitem_cast 类型分发，违反开闭原则 | Commands.cpp:166-175 |
| 17 | 每次 undo/redo 都 dynamic_cast，应缓存结果 | Commands.cpp:157,190 |
| 18 | cloneItem() 返回裸指针，违反 RAII | 所有 Item.cpp |
| 19 | filterSelectableItems() 逻辑在 6 个位置重复 | mainwindow/qatgraphicsview/AlignLayoutDialog |
| 20 | QDataStream 未设置显式版本号 | mainwindow.cpp:1309,1349 |
| 21 | 分布算法 gap 为负数时未处理 | AlignmentUtils.cpp:191,235 |
| 22 | TIFF 尺寸上限 50000 允许 10GB 分配 | ImageUtils.cpp:69 |
| 23 | PropertyPanel 同一操作发出两个 undo 命令 | PropertyPanel.cpp:720-752 |
| 24 | RulerBar 硬编码 macOS 字体 "SF Pro Display" | RulerBar.cpp:213,272,349 |
| 25 | closeEvent 和析构函数重复清理逻辑 | mainwindow.cpp:1490-1508, 100-116 |
| 26 | ExportImageDialog::getSelectedFormat() 始终返回空字符串 | ExportImageDialog.cpp:144-148 |
| 27 | preserveICCProfile 收集但从未使用 | ExportImageDialog.cpp:118 |
| 28 | 硬编码 /opt/homebrew 路径（仅 ARM Mac） | Common.pri:5-6 |
| 29 | .gitignore 中 Build* 不匹配小写 build/ | .gitignore:93 |
| 30 | .DS_Store 和构建产物被 git 跟踪 | Tests/ |

---

## 四、低优先级问题 (Low)

| # | 问题 | 位置 |
|---|------|------|
| 31 | ResizeHandleItem 析构函数可改为 = default | ResizeHandleItem.cpp:31 |
| 32 | CanvasItem 硬编码 Z 值 -9999 | CanvasItem.cpp:12 |
| 33 | CanvasItem::setPpi() 不通知依赖方 | CanvasItem.cpp:31 |
| 34 | .clang-tidy 禁用了过多安全检查 | .clang-tidy |
| 35 | ExportImageDialog::onFormatChanged 与 onCompressionChanged 冲突 | ExportImageDialog.cpp |
| 36 | 菜单栏和工具栏重复注册 Cut/Copy/Paste 快捷键 | mainwindow.cpp:195-211, 320-335 |
| 37 | GradientPreview 使用裸 new/delete 而非 unique_ptr | GradientDialog.cpp:28,36 |
| 38 | AlignLayoutDialog 使用 sender() 模式，不如 lambda 安全 | AlignLayoutDialog.cpp:255-269 |
| 39 | TIFF 元数据写入耦合在 MainWindow 而非 ImageUtils | mainwindow.cpp:734,816 |
| 40 | .qm 文件在 .gitignore 但运行时需要 | translations.qrc |
| 41 | ExportImageDialog 成员指针缺少默认初始化 | ExportImageDialog.h:35-44 |
| 42 | AlignLayoutDialog 静态成员持久化状态 | AlignLayoutDialog.h:75-78 |

---

## 五、UI 层详细问题

### 5.1 潜在空指针解引用

| 位置 | 问题 |
|------|------|
| `qatgraphicsview.cpp:451-453` | `m_resizeHandle->targetItem()` 在 mouseMoveEvent 中无空检查 |
| `PropertyPanel.h:149` | `m_previewItem` 在颜色对话框打开时可能悬空 |
| `AlignLayoutDialog.cpp:295,323` | `m_undoStack->push()` 无空检查 |

### 5.2 信号/槽连接问题

- `toolChanged` 信号和 `onToolTriggered` 导致工具栏双重同步
- `keyPressEvent` 吞噬的按键与工具栏快捷键重叠，其中一组是死代码
- 缩放滑块/标签初始化顺序依赖（`_initConnections` 在 `_initStatusBar` 之前）

### 5.3 代码重复

`filterSelectableItems()` 逻辑在 6 个位置重复实现：

- `MainWindow::filterSelectableItems()` (mainwindow.cpp:1295-1303)
- `MainWindow::onDelete()` (mainwindow.cpp:963-971)
- `MainWindow::onSelectionChanged()` (mainwindow.cpp:1143-1148)
- `MainWindow::onSelectAll()` (mainwindow.cpp:980-984)
- `QAtGraphicsView::updateResizeHandle()` (qatgraphicsview.cpp:233-238)
- `AlignLayoutDialog::filterSelectableItems()` (AlignLayoutDialog.cpp:240-250)

### 5.4 过长方法

| 方法 | 行数 | 建议拆分 |
|------|------|----------|
| `_initMenuBar()` | 130 行 | `_initFileMenu()`, `_initEditMenu()`, `_initArrangeMenu()`, `_initViewMenu()` |
| `_initToolBar()` | 112 行 | `_initFileEditToolBar()`, `_initDrawingToolBar()`, `_initAlignToolBar()` |
| `_initConnections()` | 78 行 | 按子系统分组 |
| `RulerBar::paintEvent()` | 190 行 | `paintHorizontalRuler()`, `paintVerticalRuler()` |
| `PropertyPanel::updatePanel()` | 147 行 | `updateGeometryPanel()`, `updatePenPanel()`, `updateBrushPanel()` 等 |

### 5.5 魔术数字

| 位置 | 值 | 建议常量名 |
|------|----|-----------|
| `mainwindow.cpp:85` | `1200, 800` | `kDefaultWindowSize` |
| `mainwindow.cpp:944` | `20, 20` | `kPasteOffset` |
| `mainwindow.cpp:293,376` | `QSize(20, 20)` | `kToolbarIconSize` |
| `mainwindow.cpp:137` | `30, 30` | `kRulerCornerSize` |
| `qatgraphicsview.cpp:52` | `793.7, 1122.5` | `kDefaultA4SizePx` |
| `qatgraphicsview.cpp:81` | `-500, 1000` | `kScenePadding` |
| `qatgraphicsview.cpp:128` | `0.1, 5.0` | `kMinZoom`, `kMaxZoom` |
| `qatgraphicsview.cpp:158` | `0.9` | `kFitMarginFactor` |
| `qatgraphicsview.cpp:639` | `1.15` | `kZoomStepFactor` |
| `qatgraphicsview.cpp:669-677` | `3, 6` | `kShadowOffset`, `kShadowWidth` |
| `qatgraphicsview.cpp:753` | `3` | `kMinDrawSize` |

### 5.6 线程安全

TIFF 导出和场景渲染都在主线程同步执行，大图片会阻塞 UI。

**建议：** 将 TIFF 导出移到 `QThread` 或 `QtConcurrent::run()`，添加进度条。

---

## 六、构建系统与工程规范

### 6.1 .gitignore 修复建议

```gitignore
# 添加以下规则
build/
build*/
.DS_Store
*.qm
Tests/tst_alignment
Tests/*.o
Tests/*.moc
Tests/moc_predefs.h
```

### 6.2 Common.pri 跨平台修复

```qmake
macx {
    exists(/opt/homebrew/include/tiff.h) {
        INCLUDEPATH += /opt/homebrew/include
        LIBS += -L/opt/homebrew/lib
    } else:exists(/usr/local/include/tiff.h) {
        INCLUDEPATH += /usr/local/include
        LIBS += -L/usr/local/lib
    }
    LIBS += -ltiff
}
```

### 6.3 编译器警告

```qmake
# Src/Src.pro 添加
CONFIG += c++17
QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
```

### 6.4 .clang-tidy 收紧

恢复以下重要检查：

- `bugprone-narrowing-conversions` — 与 ImageUtils 中 uint32_t→int 转换相关
- `bugprone-implicit-widening-of-multiplication-result` — 整数扩宽警告
- `clang-analyzer-cplusplus.NewDeleteLeaks` — 内存泄漏检测

### 6.5 测试覆盖

当前只有 `AlignmentUtils` 有测试（1 个文件，16 个用例）。以下模块零测试：

- 所有 Item 的 serialize/deserialize 往返测试
- 所有 Command 的 undo/redo 正确性测试
- ImageUtils 的图片加载/导出测试
- `createItemByType()` 工厂函数测试
- CanvasItem 的 ppi、canvas 尺寸测试
- ResizeHandleItem 的手柄位置、光标形状测试

### 6.6 CI 配置

无 CI/CD 配置文件。建议添加 `.github/workflows/build.yml`。

---

## 七、架构改进建议

### 7.1 命令系统安全性

当前裸指针 + 手动生命周期管理存在悬空指针风险。`QGraphicsItem` 不是 `QObject`，无法用 `QPointer`。

**建议：** 在 `QGraphicsScene` 上连接 `itemAboutToBeRemoved()` 信号，让 Command 在 item 被移除时自动失效。

### 7.2 序列化框架

当前每个 Item 自行管理序列化格式，没有统一的版本/类型标记。

**建议格式：**
```
[版本号 (int16)] [ItemType (int16)] [Item数据...] [校验和 (可选)]
```

### 7.3 职责分离

`mainwindow.cpp` 中 `exportTiffLossless()` 和 `writeTiffMetadata()` 直接操作 libtiff，应移入 `ImageUtils`。主窗口不应包含 `#include <tiff.h>`。

### 7.4 PropertyChangeCommand 类型分发

Geometry 分支用 `qgraphicsitem_cast` 链判断具体类型，违反开闭原则。

**建议：** 在 `IGraphicsItem` 接口中添加 `virtual void setGeometryRect(const QRectF &rect)` 方法。

---

## 八、修复优先级路线图

```
第一周（阻塞级）：                                        状态
  ✦ 修复 Command 析构函数 m_owned 检查                    ✅ 已完成
  ✦ 修复导出对话框取消逻辑                                 ✅ 已完成
  ✦ 修复 ImageItem prepareGeometryChange                  ✅ 已完成
  ✦ 修复 TIFFWriteScanline 错误检查                       ✅ 已完成
  ✦ 添加 deserialize 流状态检查                            ✅ 已完成

第二周（高质量）：
  ✦ 修复 cloneItem transformOriginPoint
  ✦ 修复 ImageUtils 双重读取
  ✦ 修复 TIFF 压缩参数
  ✦ 添加编译器警告标志
  ✦ 修复 .gitignore

第三周（架构改进）：
  ✦ 序列化版本号机制
  ✦ 提取 filterSelectableItems 为共享函数
  ✦ TIFF I/O 从 MainWindow 移到 ImageUtils
  ✦ 启用 clang-tidy 安全检查

持续：
  ✦ 为 Item serialize/deserialize 添加单元测试
  ✦ 为 Command undo/redo 添加单元测试
  ✦ 为 ImageUtils 添加单元测试
  ✦ 建立 CI 自动构建+测试
```

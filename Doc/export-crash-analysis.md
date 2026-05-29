# TIFF 导出崩溃分析

## 崩溃调用堆栈

```
ATGraphics.exe!std::rethrow_exception(std::exception_ptr _Ptr)                          行 317
ATGraphics.exe!std::associated_state<int>::get_value(bool _Get_only_once)                行 300
Qt6Cored.dll!std::_State_manager<int>::_Get_value()                                     行 780
Qt6Cored.dll!std::future<void>::get()                                                   行 911
Qt6Cored.dll!QThreadCreateThread::run()                                                 行 1235
Qt6Cored.dll!QThreadPrivate::start(void * arg)                                          行 292
```

## 崩溃机制

调用链从底向上解读：

1. `QThreadPrivate::start` — Qt 线程启动入口
2. `QThreadCreateThread::run` — Qt 内部的 `QThread::create()` 机制，执行用户传入的 lambda
3. `std::future<void>::get` — lambda 执行完毕后，通过 `future.get()` 获取结果
4. `std::rethrow_exception` — 如果 lambda 中抛出了异常，Qt 内部的 `std::promise` 会捕获该异常；`future.get()` 调用时会通过 `std::rethrow_exception` 重新抛出

**核心原因**：`QThread::create(lambda)` 内部使用 `std::promise`/`std::future` 机制。当 lambda 抛出未捕获的异常时，异常被 promise 捕获存储；随后 `QThreadCreateThread::run()` 调用 `future.get()` 时重新抛出该异常，但由于外层没有 catch，导致进程崩溃。

## 崩溃位置

`Src/UI/mainwindow.cpp` 第 1807-1832 行，`MainWindow::onExportImage()` 函数末尾：

```cpp
auto *thread = QThread::create([guard, tiffPath, sources,
                                 overlays = std::move(overlays), outSize,
                                 settings, progress, taskId, bRip, ripXRes,
                                 ripYRes]() {
    auto result = ImageUtils::exportTiff(tiffPath, sources, overlays,
                                         outSize, settings, progress);
    // ... 通过 QMetaObject::invokeMethod 处理结果 ...
});

connect(thread, &QThread::finished, thread, &QObject::deleteLater);
thread->start();
```

lambda 内部调用 `ImageUtils::exportTiff()` 时**没有任何 try-catch 包裹**。

## 可能抛出异常的位置

`exportTiff()` 函数（`Src/Utils/ImageWorker.cpp` 第 359-587 行）中有多处可能抛出异常的操作：

### 1. 内存分配失败（最可能的原因）

| 行号 | 代码 | 说明 |
|------|------|------|
| 146 | `buf.data.resize(static_cast<size_t>(w) * h * 4)` | 每个源 TIFF 的 CMYK 缓冲区 |
| 173 | `std::vector<uint8_t> scanBuf(scanlineSize)` | 单行扫描缓冲区 |
| 196 | `std::vector<uint8_t> bgraLine(w * 4)` | BGRA 行缓冲区 |
| 268 | `std::vector<std::vector<uint8_t>> planes(...)` | 分离平面缓冲区 |
| 412 | `buf.data = overlay.data` | 复制 overlay 像素数据 |
| 432 | `std::vector<uint8_t> outBuf(outW * outH * 4)` | **输出图像缓冲区，最大的单次分配** |

以 300 DPI 的 A3 尺寸（3508x4961 像素）为例：
- 输出缓冲区：3508 * 4961 * 4 = **~66 MB**
- 加上所有源 TIFF 的缓冲区，总内存消耗可能轻松超过数百 MB
- 如果多个源图像叠加，内存压力更大，`std::bad_alloc` 极有可能被抛出

### 2. QtConcurrent 异常传播

```cpp
// 第 392 行
CmykBuffer buf = readFutures[i].takeResult();
```

`takeResult()` 会阻塞等待并获取结果。如果 `QtConcurrent::run` 中的任务抛出了异常，`takeResult()` 会重新抛出该异常。

```cpp
// 第 510 行
chunkFutures[i].waitForFinished();
```

`waitForFinished()` 同样会传播异常。

### 3. TIFF I/O 异常

`TIFFOpen()`、`TIFFWriteScanline()` 等 libtiff 函数本身不会抛 C++ 异常，但返回错误码后如果逻辑未正确处理，可能导致后续操作访问无效内存。

## 根本原因总结

```
QThread::create(lambda)           // Qt 内部创建 promise/future
  └── lambda 中调用 exportTiff()  // 没有 try-catch
        └── 内存分配失败           // std::bad_alloc 被抛出
              └── promise 捕获异常
                    └── future.get() 重新抛出
                          └── 无 catch → 崩溃
```

**整个 `Src/` 目录中没有任何 try-catch 块**，这是一个系统性的异常安全问题。导出流程中 `QThread::create()` 的 lambda、以及 `exportTiff()` 内部的 `QtConcurrent` 任务，都没有任何异常保护。

## 修复建议

### 方案 1：在 QThread::create 的 lambda 中添加 try-catch（推荐，治标）

在 `mainwindow.cpp` 第 1807 行的 lambda 中包裹 try-catch：

```cpp
auto *thread = QThread::create([guard, tiffPath, sources,
                                 overlays = std::move(overlays), outSize,
                                 settings, progress, taskId, bRip, ripXRes,
                                 ripYRes]() {
    try {
        auto result = ImageUtils::exportTiff(tiffPath, sources, overlays,
                                             outSize, settings, progress);
        QMetaObject::invokeMethod(guard.data(),
            [guard, result, taskId, bRip, ripXRes, ripYRes, tiffPath]() {
                if (!guard) return;
                guard->m_exporting = false;
                if (result.success) {
                    guard->m_pProgressMgr->finishTask(taskId);
                    if (bRip && guard->m_pNetWorkUtils) {
                        guard->m_pNetWorkUtils->doAddRip(ripXRes, ripYRes, result.filePath);
                    }
                } else {
                    guard->m_pProgressMgr->cancelTask(taskId);
                    qWarning() << "Export failed:" << result.filePath << result.errorMessage;
                }
            }, Qt::QueuedConnection);
    } catch (const std::exception &ex) {
        qCritical() << "Export exception:" << ex.what();
        QMetaObject::invokeMethod(guard.data(),
            [guard, taskId, msg = QString::fromUtf8(ex.what())]() {
                if (!guard) return;
                guard->m_exporting = false;
                guard->m_pProgressMgr->cancelTask(taskId);
                QMessageBox::warning(guard.data(),
                    QObject::tr("Export"),
                    QObject::tr("Export failed: %1").arg(msg));
            }, Qt::QueuedConnection);
    } catch (...) {
        qCritical() << "Export unknown exception";
        QMetaObject::invokeMethod(guard.data(), [guard, taskId]() {
            if (!guard) return;
            guard->m_exporting = false;
            guard->m_pProgressMgr->cancelTask(taskId);
        }, Qt::QueuedConnection);
    }
});
```

### 方案 2：在 exportTiff 内部添加防御性检查（推荐，治本）

在 `ImageWorker.cpp` 的 `exportTiff()` 函数中：

1. **预检输出尺寸**（第 432 行之前）：

```cpp
// 合理上限检查，例如 30000x30000 * 4 = ~3.4 GB
constexpr size_t kMaxOutputBytes = static_cast<size_t>(30000) * 30000 * 4;
size_t allocSize = static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4;
if (allocSize > kMaxOutputBytes) {
    result.errorMessage = QString("Output too large: %1x%2").arg(outW).arg(outH);
    return result;
}
```

2. **用 try-catch 保护大块内存分配**：

```cpp
std::vector<uint8_t> outBuf;
try {
    outBuf.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4);
} catch (const std::bad_alloc &) {
    result.errorMessage = QString("Not enough memory for %1x%2 output").arg(outW).arg(outH);
    return result;
}
```

3. **同样保护 readSourceToCmyk 中的内存分配**（第 146 行）：

```cpp
try {
    buf.data.resize(static_cast<size_t>(w) * h * 4);
} catch (const std::bad_alloc &) {
    buf.errorMessage = QString("Not enough memory for %1x%2").arg(w).arg(h);
    return buf;
}
```

### 建议同时实施两个方案

- 方案 1 防止任何未预期的异常导致崩溃（兜底保护）
- 方案 2 在具体的异常点给出有意义的错误信息（精确处理）

## 涉及文件清单

| 文件 | 作用 |
|------|------|
| `Src/UI/mainwindow.cpp` | 导出入口 `onExportImage()`（第 1428 行），`QThread::create` 崩溃点（第 1807 行） |
| `Src/UI/mainwindow.h` | `m_exporting` 标志（第 139 行），导出槽函数声明（第 53 行） |
| `Src/Utils/ImageWorker.cpp` | `exportTiff()`（第 359 行），`readSourceToCmyk()`（第 103 行）— 异常抛出位置 |
| `Src/Utils/ImageWorker.h` | 导出相关类型定义和函数声明 |

## 复现条件

导出大尺寸/高分辨率 TIFF 图像时容易触发，例如：
- 高 DPI（300+）下的大画布（A3 及以上）
- 多个源 TIFF 图层叠加
- 系统可用内存不足时

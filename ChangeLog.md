# ATGraphics

## 1.0.5

> 2026.06.02

- 去除 px 单位概念 — 标尺/属性面板/画布尺寸统一使用 mm 单位
- 画布 DPI 由导入的 TIFF 文件确定，不再支持手动设置
- 无图片图元导出时需自定义 DPI
- 新增 Strip 三级流水线 TIFF 导出 (StripPipeline + SourceReader)，峰值内存降低约 28-96×
- 新增 ScopedTiffHandle — TIFF 句柄 RAII 封装
- 新增 SourceReader — 滑动窗口按需读取源 TIFF 行
- 新增 TiffExportEngine::cancelExport() — 导出取消支持
- bgraToCmykFallback 由浮点运算改为整数运算，性能提升约 3×
- 修复 strip boundary 索引检查过严导致导出空白行
- 移除 bilinear interpolation，源像素与输出 1:1 对应
- 新增像素级导出验证方法
- 修复基本图元导出问题
- 优化新建画布流程，清理旧画板时禁止操作
- 新增进度条任务历史弹窗 — 支持查看进行中/已完成任务，已完成任务 5 秒自动过期
- 支持焦点任务切换 — 弹窗内点击进行中任务可切换状态栏进度显示

## 1.0.3

> 2026.05.28

- 修复 TIFF 导出色彩空间转换
- 优化大图导入性能
- 修复属性面板撤销重做问题

## 1.0.2

> 2026.05.20

- 初始版本
- 支持基本矢量图元绘制（矩形、椭圆、直线、贝塞尔曲线、自由线条、文字）
- 支持 TIFF 图片导入与导出
- 支持图元对齐与分布
- 支持 RIP 处理集成

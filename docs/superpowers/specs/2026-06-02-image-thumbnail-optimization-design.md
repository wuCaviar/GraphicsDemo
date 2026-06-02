# Image Thumbnail Optimization Design

## Problem

Images are loaded at full resolution and stored as full QPixmap in ImageItem. For a 600 DPI A3 image (7016x9921 px), this is ~279MB of GPU/CPU memory. The export pipeline re-reads source TIFFs from disk via SourceReader, so the in-memory QPixmap is only used for screen display — a significant waste.

## Design Principle

**Thumbnail is a pure display optimization.** Original image metadata (DPI, pixel dimensions, file path, CMYK flag) remains unchanged. The thumbnail's physical dimensions must exactly match the original image's physical dimensions, so that `sceneBoundingRect()` is correct for export positioning.

## Architecture

### Core Invariant

```
m_rect = original image pixel dimensions (always)
pixmap = thumbnail (possibly smaller than m_rect)
boundingRect() → m_rect → correct scene size → correct export positioning
```

### Data Flow

```
Source TIFF (disk)
    ↓ [full decode for metadata]
QImage (full resolution)
    ↓ [downscale if sourceDPI > canvasDPI]
Thumbnail QImage
    ↓ [QPixmap::fromImage]
pixmap (thumbnail, for display only)
    +
m_rect = source pixel dimensions (for bounding rect)
m_originalSize = source pixel dimensions (metadata)
m_dpiX/Y = source DPI (metadata)
m_filePath = source path (for export re-read)
```

## Changes

### 1. ImageWorker.cpp — Generate thumbnail during import

**`runImportWorker()`**: Add `canvasDpi` parameter. After full decode, if source DPI > canvas DPI, downscale proportionally.

```cpp
ImportWorkerResult runImportWorker(const QString &filePath, int canvasDpi) {
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    reader.setAllocationLimit(0);
    QImage image = reader.read();

    // Read DPI
    int srcDpiX = 72, srcDpiY = 72;
    if (isTiffFile(filePath)) {
        readTiffDpi(filePath, srcDpiX, srcDpiY);
        result.size = readTiffSize(filePath);
    } else {
        result.size = image.size();
    }

    // Generate thumbnail: downscale if source DPI > canvas DPI
    if (canvasDpi > 0 && srcDpiX > canvasDpi) {
        double scale = static_cast<double>(canvasDpi) / srcDpiX;
        QSize thumbSize(
            qMax(1, qRound(image.width() * scale)),
            qMax(1, qRound(image.height() * scale))
        );
        image = image.scaled(thumbSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    result.pixmap = QPixmap::fromImage(image);
    result.dpiX = srcDpiX;
    result.dpiY = srcDpiY;
    // ...
}
```

### 2. ImageItem.h — Add constructor overload

```cpp
// New constructor: accepts source pixel size separately from pixmap (thumbnail)
ImageItem(const QPixmap &pixmap, const QSize &sourcePixelSize, QGraphicsItem *parent = nullptr);
```

No new members needed. `m_rect` already stores the source pixel dimensions.

### 3. ImageItem.cpp — Decouple pixmap from boundingRect

**New constructor**: Set `m_rect` to source pixel dimensions (not pixmap size):

```cpp
ImageItem::ImageItem(const QPixmap &pixmap, const QSize &sourcePixelSize, QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    m_rect = QRectF(QPointF(0, 0), sourcePixelSize);
}
```

Existing constructor unchanged — backward compatible for non-thumbnail usage.

**paint()**: Scale thumbnail to fill `m_rect`:

```cpp
void ImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                      QWidget *widget)
{
    if (pixmap().isNull() || !m_rect.isValid()) return;
    painter->drawPixmap(m_rect, pixmap(), pixmap().rect());
}
```

This works for both cases:
- Thumbnail: pixmap smaller than m_rect → upscaled to fill m_rect
- Full image: pixmap == m_rect size → 1:1 copy, no quality loss

**boundingRect()**: Unchanged — returns `m_rect` which is always source pixel dimensions.

### 4. MainWindow.cpp — Pass canvasDpi to worker

In `importSingleImage()` and `importMultipleImages()`:

```cpp
int canvasDpi = m_canvasItem ? m_canvasItem->canvasDpiX() : 0;
auto result = ImageUtils::runImportWorker(filePath, canvasDpi);
```

After import, set `m_rect` to source pixel dimensions (via constructor or setRect).

### 5. Serialization — Save thumbnail, preserve metadata

`serialize()` already saves `pixmap()` and `m_rect` separately. With this design:
- `pixmap()` = thumbnail (smaller)
- `m_rect` = source pixel dimensions (correct for export)

No change needed to serialize/deserialize logic. `m_rect` is already serialized.

**Backward compatibility**: Old .atp files contain full-resolution pixmaps. On load, `m_rect` is restored from the stream, and pixmap is the full image. The `paint()` override using `drawPixmap(m_rect, pixmap(), pixmap().rect())` handles both cases: if pixmap == m_rect size, it draws 1:1; if pixmap < m_rect, it upscales.

### 6. PropertyPanel.cpp — No changes needed

`originalSize()`, `dpiX()`, `dpiY()` all return original metadata, unchanged.

## Memory Savings

| Scenario | Source | Thumbnail | Savings |
|----------|--------|-----------|---------|
| 600 DPI A3, canvas 300 DPI | 7016x9921 (~279MB) | 3508x4961 (~69MB) | 75% |
| 450 DPI A3, canvas 300 DPI | 5263x7440 (~156MB) | 3508x4961 (~69MB) | 55% |
| 300 DPI A3, canvas 300 DPI | 3508x4961 (~69MB) | no change | 0% |
| 72 DPI PNG | original | no change | 0% |

Note: First image (which locks canvas DPI) won't be downscaled. Subsequent higher-DPI images benefit.

## Risks and Mitigations

### 1. Visual quality on high zoom
**Risk**: Zooming beyond thumbnail resolution shows interpolated pixels.
**Mitigation**: User confirmed this is acceptable. Export reads from disk at full resolution.

### 2. Backward compatibility with existing .atp files
**Risk**: Old files contain full-resolution pixmaps.
**Mitigation**: paint() handles both cases. Old files load correctly; re-saving upgrades to thumbnail.

### 3. Memory spike during import
**Risk**: Full image still decoded briefly for downscaling.
**Mitigation**: One-time cost on worker thread. QImage freed immediately after QPixmap conversion.

### 4. CMYK color space preservation
**Risk**: QImage operations might alter CMYK data.
**Mitigation**: Check QImage format before scaling. For CMYK images, ensure format is preserved through the scaling operation. The isCmykSource flag and filePath are preserved for export.

### 5. Non-TIFF images without DPI metadata
**Risk**: DPI defaults to 72, canvas DPI also 72 → no downscaling.
**Mitigation**: Expected behavior. Non-TIFF images are typically screen-resolution already.

### 6. Canvas DPI not yet locked at import time
**Risk**: If canvas DPI is 0 (not locked), no downscaling occurs.
**Mitigation**: First image always imports at full resolution (locks DPI). Subsequent images benefit. This is the expected behavior.

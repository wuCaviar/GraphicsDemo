#ifndef ATHCPRES_H
#define ATHCPRES_H

#include <QtGlobal>

// 打印行业常用尺寸 (单位: mm)
struct CanvasSizePreset
{
    QString name;
    qreal widthMM;
    qreal heightMM;
};

static const CanvasSizePreset kCanvasPresets[] = {
    { "A3 (297 × 420 mm)", 297, 420 },
    { "A4 (210 × 297 mm)", 210, 297 },
    { "A5 (148 × 210 mm)", 148, 210 },
    { "B3 (353 × 500 mm)", 353, 500 },
    { "B4 (250 × 353 mm)", 250, 353 },
    { "B5 (176 × 250 mm)", 176, 250 },
    { "16K (195 × 270 mm)", 195, 270 },
    { "32K (130 × 185 mm)", 130, 185 },
    { "8K (270 × 390 mm)", 270, 390 },
    { "Letter (216 × 279 mm)", 215.9, 279.4 },
    { "Legal (216 × 356 mm)", 215.9, 355.6 },
    { "Photo 3R (89 × 127 mm)", 89, 127 },
    { "Photo 4R (102 × 152 mm)", 102, 152 },
    { "Photo 5R (127 × 178 mm)", 127, 178 },
    { "Photo 6R (152 × 203 mm)", 152, 203 },
    { "Photo 8R (203 × 254 mm)", 203, 254 },
    { "Custom", 0, 0 },
};

// 固定 DPI 预设
static const int kDpiValues[] = { 150, 240, 300, 360, 397, 450, 508, 600, 720 };

// RIP固定DPI预设
static const int X_DPIValues[] = { 240, 360, 720 };
static const int Y_DPIValues[] = { 600, 1200, 1800, 2400, 3000, 3600 };

#endif // ATHCPRES_H

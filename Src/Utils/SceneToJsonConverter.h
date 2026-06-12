#ifndef SCENETOJSONCONVERTER_H
#define SCENETOJSONCONVERTER_H

#include <QString>

class QGraphicsScene;

// ============================================================================
// SceneToJsonConverter — 将 ATGraphics 场景转换为 ExportEngine JSON 格式
//
// 遍历 GraphicsScene 中的所有图元（递归展开分组），按 z-order 排列，
// 输出符合 ExportEngine JsonSceneParser 要求的 JSON 字符串。
//
// 坐标系统：输出的 x/y/width/height 均为画布像素坐标（与 CanvasItem::rect 对齐）。
// ExportEngine 使用 unit/dpi 进行 mm↔px 转换；本转换器始终输出 unit="px"，
// 所有坐标均为像素值，不依赖 unit 转换。
//
// 颜色：优先使用图元存储的 CMYK 值（0-100%），否则从 Qt RGBA（0-255）转换。
// 渐变：从 QBrush 中提取渐变类型、归一化坐标和色标。
// 文字：fontSize 使用点值（pt），与 EE FontEngine 一致。
// 精度：全程使用 qreal（double），不做整数截断。
//
// 参见 ExportEngine: Doc/、ExportEngine/Core/SceneData.h、JsonSceneParser.cpp
// ============================================================================
class SceneToJsonConverter
{
public:
    struct ConvertResult
    {
        QString json;
        bool success = false;
        QString errorMessage;
    };

    // 将 scene 转换为 EE JSON 字符串
    // rgbProfile / cmykProfile / grayProfile: ICC 配置文件路径（相对于 EE 的 ICC Profile 目录）
    // exportDpi: 导出 DPI（>0 时覆盖画布 DPI，所有像素坐标按比例缩放；0 表示使用画布原生 DPI）
    static ConvertResult convert(QGraphicsScene *scene, const QString &rgbProfile = {},
                                 const QString &cmykProfile = {}, const QString &grayProfile = {},
                                 int exportDpi = 0);
};

#endif // SCENETOJSONCONVERTER_H

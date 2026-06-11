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
// 颜色：优先使用图元存储的 CMYK 值，否则从 Qt RGBA 转换。
// 渐变：从 QBrush 中提取渐变类型、方向和色标。
// 文字：fontSize 使用点值（point size），与 EE 测试用例一致。
// 精度：全程使用 qreal（double），不做整数截断。
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
    static ConvertResult convert(QGraphicsScene *scene, const QString &rgbProfile = { },
                                 const QString &cmykProfile = { },
                                 const QString &grayProfile = { });
};

#endif // SCENETOJSONCONVERTER_H

#ifndef COLORTRANSFORM_H
#define COLORTRANSFORM_H

#include <QColor>
#include <QObject>
#include <QString>
#include <lcms2.h>

// CMYK 颜色值（0-100），用于图元导出时保留精确 CMYK
struct CmykColor
{
    double c = 0.0, m = 0.0, y = 0.0, k = 0.0;
    bool valid = false;
};

class QATColorManager
{
public:
    struct Cmyk
    {
        double c = 0.0, m = 0.0, y = 0.0, k = 0.0;
    };

    // 自适应转换阈值（像素数）：低于此值一次性 cmsDoTransform，高于此值逐行处理
    static constexpr int kSingleShotPixelThreshold = 1024 * 1024; // 1MP

    static QATColorManager &instance();

    bool initialize();

    // ---- 单像素转换（主线程 UI 使用，每次调用内部创建/销毁独立变换句柄） ----
    Cmyk toCmyk(const QColor &rgb, cmsUInt32Number intent = INTENT_PERCEPTUAL,
                cmsUInt32Number flags = cmsFLAGS_BLACKPOINTCOMPENSATION
                                        | cmsFLAGS_HIGHRESPRECALC) const;
    QColor toRgb(const Cmyk &cmyk, cmsUInt32Number intent = INTENT_PERCEPTUAL,
                 cmsUInt32Number flags = cmsFLAGS_BLACKPOINTCOMPENSATION
                                         | cmsFLAGS_HIGHRESPRECALC) const;

    bool isValid() const;
    QString errorString() const;

    // ---- ICC Profile 访问（只读，初始化后线程安全） ----
    cmsHPROFILE srgbProfile() const { return m_srgb; }
    cmsHPROFILE cmykProfile() const { return m_cmyk; }

    // ---- 批量变换工厂方法（每次调用创建新句柄，调用方负责 cmsDeleteTransform） ----
    // 线程安全：每次返回独立的变换句柄，各线程可各自持有、并发使用
    // BGRA_8 → CMYK_8：输入 QImage::Format_ARGB32 扫描线（小端序 BGRA 字节序），输出 CMYK
    cmsHTRANSFORM createBgraToCmyk8(cmsUInt32Number intent,
                                    cmsUInt32Number flags) const;
    // CMYK_8 → BGRA_8：输入 CMYK 4 通道，输出 BGRA 扫描线
    cmsHTRANSFORM createCmyk8ToBgra(cmsUInt32Number intent,
                                    cmsUInt32Number flags) const;

    // ---- 批量转换静态方法（需传入线程独立的变换句柄） ----

    // 单行转换：src/dst 均为 width * 4 字节
    static void convertBgra8ToCmyk8(cmsHTRANSFORM xform,
                                    const unsigned char *src,
                                    unsigned char *dst, int pixelCount);
    static void convertCmyk8ToBgra8(cmsHTRANSFORM xform,
                                    const unsigned char *src,
                                    unsigned char *dst, int pixelCount);

    // 自适应整图转换：低于阈值一次性 cmsDoTransform，高于阈值逐行处理
    // src/dst 均为 width * height * 4 字节的连续缓冲区（无行填充）
    static void
    convertBgra8ToCmyk8(cmsHTRANSFORM xform, const unsigned char *src,
                        unsigned char *dst, int width, int height,
                        int singleShotThreshold = kSingleShotPixelThreshold);
    static void
    convertCmyk8ToBgra8(cmsHTRANSFORM xform, const unsigned char *src,
                        unsigned char *dst, int width, int height,
                        int singleShotThreshold = kSingleShotPixelThreshold);

private:
    QATColorManager();
    ~QATColorManager();
    Q_DISABLE_COPY(QATColorManager)

    static void errorLogger(cmsContext context, cmsUInt32Number code,
                            const char *error);
    bool loadProfiles();
    void cleanup();

    // 仅持有 Profile（初始化后只读，线程安全）
    // 所有 cmsHTRANSFORM 由调用方各自创建和持有，避免跨线程共享
    cmsHPROFILE m_srgb = nullptr;
    cmsHPROFILE m_cmyk = nullptr;

    bool m_ok = false;
    static QString m_err;
};

#endif // COLORTRANSFORM_H

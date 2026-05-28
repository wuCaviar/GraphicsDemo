#ifndef COLORTRANSFORM_H
#define COLORTRANSFORM_H

#include <QColor>
#include <QObject>
#include <QString>
#include <lcms2.h>

#include "Common/ColorTypes.h"
#include "Common/IColorTransform.h"

class QATColorManager : public IColorTransform
{
public:
    struct Cmyk
    {
        double c = 0.0, m = 0.0, y = 0.0, k = 0.0;
    };

    // 自适应转换阈值（像素数）：低于此值一次性 cmsDoTransform，高于此值逐行处理
    static constexpr int kSingleShotPixelThreshold = 1024 * 1024; // 1MP

    static QATColorManager &instance();

    bool initialize(const QString &srgbIccPath, const QString &cmykIccPath);

    // ---- IColorTransform 接口实现 ----
    bool isValid() const override;
    QColor toRgb(double c, double m, double y, double k) override;
    void toCmyk(const QColor &rgb, double &c, double &m, double &y, double &k) override;

    // ---- 单像素转换（保留原有接口用于内部批量操作） ----
    Cmyk toCmyk(const QColor &rgb, cmsUInt32Number intent = INTENT_PERCEPTUAL,
                cmsUInt32Number flags = cmsFLAGS_BLACKPOINTCOMPENSATION
                                        | cmsFLAGS_HIGHRESPRECALC) const;
    QColor toRgb(const Cmyk &cmyk, cmsUInt32Number intent = INTENT_PERCEPTUAL,
                 cmsUInt32Number flags = cmsFLAGS_BLACKPOINTCOMPENSATION
                                         | cmsFLAGS_HIGHRESPRECALC) const;

    QString errorString() const;

    // ---- ICC Profile 访问（只读，初始化后线程安全） ----
    cmsHPROFILE srgbProfile() const { return m_srgb; }
    cmsHPROFILE cmykProfile() const { return m_cmyk; }

    // ---- 批量变换工厂方法（每次调用创建新句柄，调用方负责 cmsDeleteTransform） ----
    cmsHTRANSFORM createBgraToCmyk8(cmsUInt32Number intent,
                                    cmsUInt32Number flags) const;
    cmsHTRANSFORM createCmyk8ToBgra(cmsUInt32Number intent,
                                    cmsUInt32Number flags) const;

    // ---- 批量转换静态方法（需传入线程独立的变换句柄） ----
    static void convertBgra8ToCmyk8(cmsHTRANSFORM xform,
                                    const unsigned char *src,
                                    unsigned char *dst, int pixelCount);
    static void convertCmyk8ToBgra8(cmsHTRANSFORM xform,
                                    const unsigned char *src,
                                    unsigned char *dst, int pixelCount);

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
    bool loadProfiles(const QString &srgbIccPath, const QString &cmykIccPath);
    void cleanup();

    cmsHPROFILE m_srgb = nullptr;
    cmsHPROFILE m_cmyk = nullptr;

    bool m_ok = false;
    static QString m_err;
};

#endif // COLORTRANSFORM_H

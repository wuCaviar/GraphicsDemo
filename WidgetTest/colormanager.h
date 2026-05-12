#ifndef COLORMANAGER_H
#define COLORMANAGER_H

#include <QColor>
#include <QString>
#include <lcms2.h>

class ColorManager
{
public:
    struct Cmyk { double c = 0, m = 0, y = 0, k = 0; };

    static ColorManager &instance();

           // cmykProfilePath: Japan Color 2001 Coated ICC 文件路径，为空则自动搜索
    bool initialize();

           // sRGB → CMYK
    Cmyk  toCmyk(const QColor &rgb) const;

           // CMYK → sRGB
    QColor toRgb(const Cmyk &cmyk) const;

           // 软打样: sRGB → CMYK → sRGB
    QColor softProof(const QColor &rgb) const;

    bool    isValid()     const;
    QString errorString() const;

    bool buildRGB2CMYKTransforms(cmsUInt32Number Intent, cmsUInt32Number dwFlags);
    bool buildCMYK2RGBTransforms(cmsUInt32Number Intent, cmsUInt32Number dwFlags);


private:
    ColorManager();
    ~ColorManager();
    Q_DISABLE_COPY(ColorManager)

    bool loadProfiles();
    void cleanup();

    static cmsToneCurve *buildDotGain15Curve();

    cmsHPROFILE m_srgb = nullptr;
    cmsHPROFILE m_cmyk = nullptr;

    cmsHTRANSFORM m_tRgbToCmyk = nullptr;
    cmsHTRANSFORM m_tCmykToRgb = nullptr;

    bool    m_ok  = false;
    QString m_err;
};

#endif // COLORMANAGER_H

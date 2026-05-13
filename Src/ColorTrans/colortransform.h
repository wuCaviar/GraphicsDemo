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

    static QATColorManager &instance();

    bool initialize();

    // sRGB → CMYK
    Cmyk toCmyk(const QColor &rgb) const;

    // CMYK → sRGB
    QColor toRgb(const Cmyk &cmyk) const;

    bool isValid() const;
    QString errorString() const;

    bool buildRGB2CMYKTransforms(cmsUInt32Number Intent, cmsUInt32Number dwFlags);
    bool buildCMYK2RGBTransforms(cmsUInt32Number Intent, cmsUInt32Number dwFlags);

private:
    QATColorManager();
    ~QATColorManager();
    Q_DISABLE_COPY(QATColorManager)

    static void errorLogger(cmsContext context, cmsUInt32Number code, const char *error);
    bool loadProfiles();
    void cleanup();

    cmsHPROFILE m_srgb = nullptr;
    cmsHPROFILE m_cmyk = nullptr;

    cmsHTRANSFORM m_tRgbToCmyk = nullptr;
    cmsHTRANSFORM m_tCmykToRgb = nullptr;

    bool m_ok = false;
    static QString m_err;
};

#endif // COLORTRANSFORM_H

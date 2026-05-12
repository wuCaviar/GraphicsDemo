#include "colormanager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>
#include <cmath>

// ================================================================
//  内部辅助: QColor ↔ lcms2 BGRA8
// ================================================================

void errorLogger(
    cmsContext context,
    cmsUInt32Number code,
    const char* error)
{
    fprintf(stderr,
            "[LCMS Error] Code: %u, Message: %s\n",
            (unsigned)code,
            error);
}

static inline void qcolorToRGB(const QColor &c, uchar out[3])
{
    out[0] = static_cast<uchar>(c.red());
    out[1] = static_cast<uchar>(c.green());
    out[2] = static_cast<uchar>(c.blue());
}

static inline QColor rgbToQcolor(const uchar in[3])
{
    return QColor(in[0], in[1], in[2]);
}

// ================================================================
//  构造 / 析构
// ================================================================

ColorManager::ColorManager()  = default;
ColorManager::~ColorManager() { cleanup(); }

ColorManager &ColorManager::instance()
{
    static ColorManager s;
    return s;
}

// ================================================================
//  初始化
// ================================================================

bool ColorManager::initialize()
{
    cmsSetLogErrorHandler(errorLogger);

    if (m_ok) return true;
    cleanup();

    if (!loadProfiles())   return false;

    m_ok = true;
    qInfo().noquote()
        << "[ColorManager] 就绪"
        << "\n  RGB:  sRGB IEC61966-2.1"
        << "\n  CMYK: Japan Color 2001 Coated";
    return true;
}

bool    ColorManager::isValid()     const { return m_ok; }
QString ColorManager::errorString() const { return m_err; }

// ================================================================
//  ICC Profile 加载
// ================================================================

bool ColorManager::loadProfiles()
{
    m_srgb = cmsOpenProfileFromFile(
        "/Volumes/Caviar/Test/sRGB IEC61966-2.1.icc", "r");
    if (!m_srgb)
    {
        m_ok = false;
        return false;
    }

    m_cmyk = cmsOpenProfileFromFile(
        "/Volumes/Caviar/Test/Japan Color 2001 Coated.icc", "r");
    if (!m_cmyk) {
        cmsCloseProfile(m_srgb);
        m_srgb = nullptr;
        m_ok = false;
        return false;
    }
    m_ok = true;

    return m_ok;
}

// ================================================================
//  色彩变换
// ================================================================

bool ColorManager::buildRGB2CMYKTransforms(cmsUInt32Number Intent, cmsUInt32Number dwFlags)
{
    m_tRgbToCmyk = cmsCreateTransform(
        m_srgb, TYPE_RGB_8, m_cmyk, TYPE_CMYK_8,
        Intent, dwFlags);

    return m_tRgbToCmyk != nullptr;
}

bool ColorManager::buildCMYK2RGBTransforms(cmsUInt32Number Intent, cmsUInt32Number dwFlags)
{
    m_tCmykToRgb = cmsCreateTransform(
        m_cmyk, TYPE_CMYK_8, m_srgb, TYPE_RGB_8,
        Intent, dwFlags);

    return m_tCmykToRgb != nullptr;
}


// ================================================================
//  Dot Gain 15% 曲线
//
//    网点面积:  D(x) = x + 0.15 · sin(πx)
//    ICC TRC:   Y(x) = 1 − D(x)        (反射率)
// ================================================================

cmsToneCurve *ColorManager::buildDotGain15Curve()
{
    constexpr int   N = 4096;
    constexpr float G = 0.15f;

    float buf[N];
    for (int i = 0; i < N; ++i) {
        const float x = float(i) / (N - 1);
        buf[i] = 1.0f - qBound(0.0f, x + G * sinf(M_PI * x), 1.0f);
    }
    return cmsBuildTabulatedToneCurveFloat(nullptr, N, buf);
}

// ================================================================
//  转换接口
// ================================================================

ColorManager::Cmyk ColorManager::toCmyk(const QColor &rgb) const
{
    Q_ASSERT(m_ok);
    uchar src[3], dst[4];
    qcolorToRGB(rgb, src);
    cmsDoTransform(m_tRgbToCmyk, src, dst, 1);
    return { dst[0] / 255.0 * 100, dst[1] / 255.0 * 100,
            dst[2] / 255.0 * 100, dst[3] / 255.0 * 100 };
}

QColor ColorManager::toRgb(const Cmyk &cmyk) const
{
    Q_ASSERT(m_ok);
    uchar src[4] = {
        uchar(cmyk.c / 100.0 * 255),
        uchar(cmyk.m / 100.0 * 255),
        uchar(cmyk.y / 100.0 * 255),
        uchar(cmyk.k / 100.0 * 255)
    };
    uchar dst[3];
    cmsDoTransform(m_tCmykToRgb, src, dst, 1);
    return rgbToQcolor(dst);
}

QColor ColorManager::softProof(const QColor &rgb) const
{
    // return toRgb(toCmyk(rgb));
    return QColor();
}

// ================================================================
//  清理
// ================================================================

void ColorManager::cleanup()
{
    if (m_tRgbToCmyk) { cmsDeleteTransform(m_tRgbToCmyk); m_tRgbToCmyk = nullptr; }
    if (m_tCmykToRgb) { cmsDeleteTransform(m_tCmykToRgb); m_tCmykToRgb = nullptr; }
    if (m_srgb)       { cmsCloseProfile(m_srgb); m_srgb = nullptr; }
    if (m_cmyk)       { cmsCloseProfile(m_cmyk); m_cmyk = nullptr; }
    m_ok = false;
}

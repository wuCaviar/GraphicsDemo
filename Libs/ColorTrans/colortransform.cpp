#include "colortransform.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>

QString QATColorManager::m_err = "";

// ================================================================
//  内部辅助: QColor ↔ lcms2
// ================================================================
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

QATColorManager::QATColorManager() = default;
QATColorManager::~QATColorManager()
{
    cleanup();
}

QATColorManager &QATColorManager::instance()
{
    static QATColorManager s;
    return s;
}

// ================================================================
//  初始化
// ================================================================

bool QATColorManager::initialize(const QString &srgbIccPath, const QString &cmykIccPath)
{
    cmsSetLogErrorHandler(errorLogger);

    if (m_ok)
        return true;
    cleanup();

    if (!loadProfiles(srgbIccPath, cmykIccPath))
        return false;

    m_ok = true;
    qInfo().noquote() << "[QATColorManager] 就绪"
                      << "\n  RGB:  sRGB IEC61966-2.1"
                      << "\n  CMYK: Japan Color 2001 Coated";
    return true;
}

bool QATColorManager::isValid() const
{
    return m_ok;
}
QString QATColorManager::errorString() const
{
    return m_err;
}

// ================================================================
//  IColorTransform 接口实现
// ================================================================

QColor QATColorManager::toRgb(double c, double m, double y, double k)
{
    return toRgb(Cmyk{c, m, y, k});
}

void QATColorManager::toCmyk(const QColor &rgb, double &c, double &m, double &y, double &k)
{
    Cmyk cmyk = toCmyk(rgb);
    c = cmyk.c;
    m = cmyk.m;
    y = cmyk.y;
    k = cmyk.k;
}

// ================================================================
//  ICC Profile 加载
// ================================================================

bool QATColorManager::loadProfiles(const QString &srgbIccPath, const QString &cmykIccPath)
{
    m_srgb = cmsOpenProfileFromFile(srgbIccPath.toStdString().c_str(), "r");
    if (!m_srgb) {
        m_ok = false;
        return false;
    }

    m_cmyk = cmsOpenProfileFromFile(cmykIccPath.toStdString().c_str(), "r");
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
//  单像素转换（每次调用创建独立变换句柄，避免共享状态）
//  仅供主线程 UI 使用，调用频率低，变换创建开销可接受
// ================================================================

QATColorManager::Cmyk QATColorManager::toCmyk(const QColor &rgb,
                                              cmsUInt32Number intent,
                                              cmsUInt32Number flags) const
{
    Q_ASSERT(m_ok);
    uchar src[3];
    double dst[4];
    qcolorToRGB(rgb, src);

    cmsHTRANSFORM xform = cmsCreateTransform(m_srgb, TYPE_RGB_8, m_cmyk,
                                             TYPE_CMYK_DBL, intent, flags);
    cmsDoTransform(xform, src, dst, 1);
    cmsDeleteTransform(xform);

    return { dst[0], dst[1], dst[2], dst[3] };
}

QColor QATColorManager::toRgb(const Cmyk &cmyk, cmsUInt32Number intent,
                              cmsUInt32Number flags) const
{
    Q_ASSERT(m_ok);
    double src[4] = { cmyk.c, cmyk.m, cmyk.y, cmyk.k };
    uchar dst[3];

    cmsHTRANSFORM xform = cmsCreateTransform(m_cmyk, TYPE_CMYK_DBL, m_srgb,
                                             TYPE_RGB_8, intent, flags);
    cmsDoTransform(xform, src, dst, 1);
    cmsDeleteTransform(xform);

    return rgbToQcolor(dst);
}

// ================================================================
//  批量变换工厂方法（每次调用创建新句柄，调用方管理生命周期）
// ================================================================

cmsHTRANSFORM QATColorManager::createBgraToCmyk8(cmsUInt32Number intent,
                                                 cmsUInt32Number flags) const
{
    Q_ASSERT(m_ok);
    return cmsCreateTransform(m_srgb, TYPE_BGRA_8, m_cmyk, TYPE_CMYK_8, intent,
                              flags);
}

cmsHTRANSFORM QATColorManager::createCmyk8ToBgra(cmsUInt32Number intent,
                                                 cmsUInt32Number flags) const
{
    Q_ASSERT(m_ok);
    return cmsCreateTransform(m_cmyk, TYPE_CMYK_8, m_srgb, TYPE_BGRA_8, intent,
                              flags);
}

// ================================================================
//  批量转换静态方法（需传入线程独立的变换句柄）
// ================================================================

void QATColorManager::convertBgra8ToCmyk8(cmsHTRANSFORM xform,
                                          const unsigned char *src,
                                          unsigned char *dst, int pixelCount)
{
    Q_ASSERT(xform);
    cmsDoTransform(xform, src, dst, pixelCount);
}

void QATColorManager::convertCmyk8ToBgra8(cmsHTRANSFORM xform,
                                          const unsigned char *src,
                                          unsigned char *dst, int pixelCount)
{
    Q_ASSERT(xform);
    cmsDoTransform(xform, src, dst, pixelCount);
}

void QATColorManager::convertBgra8ToCmyk8(cmsHTRANSFORM xform,
                                          const unsigned char *src,
                                          unsigned char *dst, int width,
                                          int height, int singleShotThreshold)
{
    Q_ASSERT(xform);
    const int totalPixels = width * height;
    const int rowBytes = width * 4;

    if (totalPixels <= singleShotThreshold) {
        cmsDoTransform(xform, src, dst, totalPixels);
    } else {
        for (int y = 0; y < height; ++y) {
            cmsDoTransform(xform, src + y * rowBytes, dst + y * rowBytes,
                           width);
        }
    }
}

void QATColorManager::convertCmyk8ToBgra8(cmsHTRANSFORM xform,
                                          const unsigned char *src,
                                          unsigned char *dst, int width,
                                          int height, int singleShotThreshold)
{
    Q_ASSERT(xform);
    const int totalPixels = width * height;
    const int rowBytes = width * 4;

    if (totalPixels <= singleShotThreshold) {
        cmsDoTransform(xform, src, dst, totalPixels);
    } else {
        for (int y = 0; y < height; ++y) {
            cmsDoTransform(xform, src + y * rowBytes, dst + y * rowBytes,
                           width);
        }
    }
}

// ================================================================
//  清理（仅清理 Profile，变换句柄由各调用方管理）
// ================================================================

void QATColorManager::cleanup()
{
    if (m_srgb) {
        cmsCloseProfile(m_srgb);
        m_srgb = nullptr;
    }
    if (m_cmyk) {
        cmsCloseProfile(m_cmyk);
        m_cmyk = nullptr;
    }
    m_ok = false;
}

void QATColorManager::errorLogger(cmsContext context, cmsUInt32Number code,
                                  const char *error)
{
    fprintf(stderr, "[LCMS Error] Code: %u, Message: %s\n", (unsigned)code,
            error);

    m_err =
        QString("[LCMS Error] Code: %1, Message: %2\n").arg(code).arg(error);
}

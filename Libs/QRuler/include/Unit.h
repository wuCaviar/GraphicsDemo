#ifndef UNIT_H
#define UNIT_H

#include "qruler_export.h"

#include <QString>
#include <QMetaType>
#include <QStringList>
#include <QLocale>

#include <math.h>

// 1 inch ^= 72 pt
// 1 inch ^= 25.399956 mm
// 1 pt = 1/12 pi
// 1 pt ^= 0.0077880997 cc
// 1 cc = 12 dd

constexpr qreal POINT_TO_MM(qreal px) { return (px) * 0.352777167; }
constexpr qreal MM_TO_POINT(qreal mm) { return mm * 2.83465058; }
constexpr qreal POINT_TO_CM(qreal px) { return (px) * 0.0352777167; }
constexpr qreal CM_TO_POINT(qreal cm) { return (cm) * 28.3465058; }
constexpr qreal POINT_TO_DM(qreal px) { return (px) * 0.00352777167; }
constexpr qreal DM_TO_POINT(qreal dm) { return (dm) * 283.465058; }
constexpr qreal POINT_TO_INCH(qreal px) { return (px) * 0.01388888888889; }
constexpr qreal INCH_TO_POINT(qreal inch) { return (inch) * 72.0; }
constexpr qreal MM_TO_INCH(qreal mm) { return (mm) * 0.039370147; }
constexpr qreal INCH_TO_MM(qreal inch) { return (inch) * 25.399956; }
constexpr qreal POINT_TO_PI(qreal px) { return (px) * 0.083333333; }
constexpr qreal POINT_TO_CC(qreal px) { return (px) * 0.077880997; }
constexpr qreal PI_TO_POINT(qreal pi) { return (pi) * 12; }
constexpr qreal CC_TO_POINT(qreal cc) { return (cc) * 12.840103; }

static const qreal PT_ROUNDING { 1000.0 };
static const qreal CM_ROUNDING { 10000.0 };
static const qreal DM_ROUNDING { 10000.0 };
static const qreal MM_ROUNDING { 10000.0 };
static const qreal IN_ROUNDING { 100000.0 };
static const qreal PI_ROUNDING { 100000.0 };
static const qreal CC_ROUNDING { 100000.0 };

/**
 * Length unit type for the ruler.
 * Simplified standalone version of KoUnit.
 */
class QRULER_EXPORT Unit
{
public:
    enum Type {
        Millimeter = 0,
        Point,       ///< Postscript point, 1/72th of an Inch
        Inch,
        Centimeter,
        Decimeter,
        Pica,
        Cicero,
        Pixel,
        TypeCount    ///< @internal
    };

    /// Used to control the scope of the unit types listed in the UI
    enum ListOption {
        ListAll = 0,
        HidePixel = 1,
        HideMask = HidePixel
    };
    Q_DECLARE_FLAGS(ListOptions, ListOption)

    static Unit fromListForUi(int index, ListOptions listOptions = ListAll, qreal factor = 1.0);
    static Unit fromSymbol(const QString &symbol, bool *ok = nullptr);

    explicit Unit(Type unit = Point, qreal factor = 1.0)
    {
        m_type = unit;
        m_pixelConversion = factor;
    }

    Unit &operator=(Type unit)
    {
        m_type = unit;
        m_pixelConversion = 1.0;
        return *this;
    }

    bool operator==(const Unit &other) const
    {
        return m_type == other.m_type
            && (m_type != Pixel || qFuzzyCompare(m_pixelConversion, other.m_pixelConversion));
    }

    bool operator!=(const Unit &other) const
    {
        return !(*this == other);
    }

    Unit::Type type() const { return m_type; }

    void setFactor(qreal factor) { m_pixelConversion = factor; }

    static qreal convertFromUnitToUnit(const qreal value, const Unit &fromUnit, const Unit &toUnit, qreal factor = 1.0);

    qreal toUserValueRounded(const qreal value) const;
    qreal toUserValuePrecise(const qreal ptValue) const;
    qreal toUserValue(qreal ptValue, bool rounding = true) const;

    QString toUserStringValue(qreal ptValue) const;

    qreal fromUserValue(qreal value) const;
    qreal fromUserValue(const QString &value, bool *ok = nullptr) const;

    static QString unitDescription(Unit::Type type);
    QString symbol() const;

    static QStringList listOfUnitNameForUi(ListOptions listOptions = ListAll);
    int indexInListForUi(ListOptions listOptions = ListAll) const;

    static qreal parseValue(const QString &value, qreal defaultVal = 0.0);
    static qreal parseAngle(const QString &value, qreal defaultVal = 0.0);

    QString toString() const { return symbol(); }

    static qreal approxTransformScale(const QTransform &t);
    void adjustByPixelTransform(const QTransform &t);

private:
    Type m_type;
    qreal m_pixelConversion;
};

#ifndef QT_NO_DEBUG_STREAM
QRULER_EXPORT QDebug operator<<(QDebug, const Unit &);
#endif

Q_DECLARE_METATYPE(Unit)
Q_DECLARE_OPERATORS_FOR_FLAGS(Unit::ListOptions)

#endif // UNIT_H

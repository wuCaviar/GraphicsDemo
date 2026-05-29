#ifndef ICOLORTRANSFORM_H
#define ICOLORTRANSFORM_H

#include <QColor>
#include "ColorTypes.h"
#include "colortrans_global.h"

class COLORTRANS_EXPORT IColorTransform
{
public:
    virtual ~IColorTransform() = default;

    virtual bool isValid() const = 0;
    virtual QColor toRgb(double c, double m, double y, double k) = 0;
    virtual void toCmyk(const QColor &rgb, double &c, double &m, double &y, double &k) = 0;
};

#endif // ICOLORTRANSFORM_H

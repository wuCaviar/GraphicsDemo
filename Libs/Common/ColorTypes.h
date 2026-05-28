#ifndef COLORTYPES_H
#define COLORTYPES_H

// CMYK 颜色值（0-100），用于图元导出时保留精确 CMYK
struct CmykColor
{
    double c = 0.0, m = 0.0, y = 0.0, k = 0.0;
    bool valid = false;
};

#endif // COLORTYPES_H

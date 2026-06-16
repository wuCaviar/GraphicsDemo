#ifndef AT_MATH_CORE_H
#define AT_MATH_CORE_H

#include <cmath>
#include <algorithm>
#include <limits>

namespace AtMath {

// =====================================================================
// 1. 基础常量与工具函数
// =====================================================================
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double HALF_PI = PI / 2.0;

// 印刷精度阈值 (1微米级别)
constexpr double EPSILON = 1e-9;
constexpr double ZERO_TOLERANCE = 1e-6;

inline bool isEqual(double a, double b, double eps = EPSILON)
{
    return std::fabs(a - b) < eps;
}
inline bool isZero(double val, double eps = EPSILON)
{
    return std::fabs(val) < eps;
}
inline double toRadians(double degrees)
{
    return degrees * (PI / 180.0);
}
inline double toDegrees(double radians)
{
    return radians * (180.0 / PI);
}

template<typename T>
inline T clamp(T val, T minVal, T maxVal)
{
    return std::max(minVal, std::min(val, maxVal));
}

// =====================================================================
// 2. 印刷物理单位换算系统
// 内部核心数据模型强烈建议统一使用 毫米 作为基准单位
// =====================================================================
namespace Units {
constexpr double MM_PER_INCH = 25.4;
constexpr double POINTS_PER_INCH = 72.0; // PostScript 点 (DTP行业标准)
constexpr double CM_PER_MM = 0.1;
constexpr double MM_PER_CM = 10.0;

// --- 纯物理单位互转 (编译期计算) ---

inline double mmToInch(double mm)
{
    return mm / MM_PER_INCH;
}
inline double inchToMm(double inch)
{
    return inch * MM_PER_INCH;
}

inline double mmToPt(double mm)
{
    return mm * POINTS_PER_INCH / MM_PER_INCH;
}
inline double ptToMm(double pt)
{
    return pt * MM_PER_INCH / POINTS_PER_INCH;
}

inline double mmToCm(double mm)
{
    return mm * CM_PER_MM;
}
inline double cmToMm(double cm)
{
    return cm * MM_PER_CM;
}

// --- 动态 DPI 上下文转换器 ---
// 在矢量软件中，同一个物理尺寸在不同导出DPI下，对应的像素数不同。
// 此类用于在特定的 DPI 环境下进行计算。
class DPIContext
{
private:
    double m_dpi;

public:
    explicit DPIContext(double dpi = 72.0) : m_dpi(dpi) { }

    double getDpi() const { return m_dpi; }
    void setDpi(double dpi) { m_dpi = dpi; }

    // 物理尺寸 -> 屏幕像素
    inline double mmToPx(double mm) const { return mm * m_dpi / MM_PER_INCH; }
    inline double cmToPx(double cm) const { return mmToPx(cm * MM_PER_CM); }
    inline double inchToPx(double inch) const { return inch * m_dpi; }
    inline double ptToPx(double pt) const { return pt * m_dpi / POINTS_PER_INCH; }

    // 屏幕像素 -> 物理尺寸
    inline double pxToMm(double px) const { return px * MM_PER_INCH / m_dpi; }
    inline double pxToCm(double px) const { return pxToMm(px) * CM_PER_MM; }
    inline double pxToInch(double px) const { return px / m_dpi; }
    inline double pxToPt(double px) const { return px * POINTS_PER_INCH / m_dpi; }
};
} // namespace Units

// =====================================================================
// 3. 二维向量 - 与之前相同，略作精简
// =====================================================================
struct Vec2
{
    double x = 0.0;
    double y = 0.0;
    constexpr Vec2() noexcept = default;
    constexpr Vec2(double x, double y) noexcept : x(x), y(y) { }

    bool operator==(const Vec2 &o) const { return isEqual(x, o.x) && isEqual(y, o.y); }
    Vec2 operator+(const Vec2 &o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2 &o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(double s) const { return { x * s, y * s }; }
    Vec2 operator/(double s) const { return isZero(s) ? Vec2{ 0, 0 } : Vec2{ x / s, y / s }; }

    double dot(const Vec2 &o) const { return x * o.x + y * o.y; }
    double cross(const Vec2 &o) const { return x * o.y - y * o.x; }
    double lengthSquared() const { return x * x + y * y; }
    double length() const { return std::sqrt(lengthSquared()); }
    Vec2 normalized() const
    {
        double l = length();
        return isZero(l) ? Vec2{ 0, 0 } : Vec2{ x / l, y / l };
    }
    double distanceTo(const Vec2 &o) const { return (*this - o).length(); }
};

// =====================================================================
// 4. 二维仿射变换矩阵 - 与之前相同
// =====================================================================
class Matrix3x3
{
public:
    double m11 = 1.0, m12 = 0.0, m13 = 0.0;
    double m21 = 0.0, m22 = 1.0, m23 = 0.0;
    double m31 = 0.0, m32 = 0.0, m33 = 1.0;

    static Matrix3x3 Identity() { return Matrix3x3(); }
    static Matrix3x3 Translation(double tx, double ty)
    {
        Matrix3x3 m;
        m.m13 = tx;
        m.m23 = ty;
        return m;
    }
    static Matrix3x3 Scaling(double sx, double sy)
    {
        Matrix3x3 m;
        m.m11 = sx;
        m.m22 = sy;
        return m;
    }
    static Matrix3x3 Rotation(double deg)
    {
        double r = toRadians(deg), c = std::cos(r), s = std::sin(r);
        Matrix3x3 m;
        m.m11 = c;
        m.m12 = -s;
        m.m21 = s;
        m.m22 = c;
        return m;
    }

    Matrix3x3 operator*(const Matrix3x3 &o) const
    {
        Matrix3x3 r;
        r.m11 = m11 * o.m11 + m12 * o.m21 + m13 * o.m31;
        r.m12 = m11 * o.m12 + m12 * o.m22 + m13 * o.m32;
        r.m13 = m11 * o.m13 + m12 * o.m23 + m13 * o.m33;
        r.m21 = m21 * o.m11 + m22 * o.m21 + m23 * o.m31;
        r.m22 = m21 * o.m12 + m22 * o.m22 + m23 * o.m32;
        r.m23 = m21 * o.m13 + m22 * o.m23 + m23 * o.m33;
        r.m31 = m31 * o.m11 + m32 * o.m21 + m33 * o.m31;
        r.m32 = m31 * o.m12 + m32 * o.m22 + m33 * o.m32;
        r.m33 = m31 * o.m13 + m32 * o.m23 + m33 * o.m33;
        return r;
    }

    Vec2 mapPoint(const Vec2 &p) const
    {
        return { m11 * p.x + m12 * p.y + m13, m21 * p.x + m22 * p.y + m23 };
    }
    Vec2 mapVector(const Vec2 &v) const { return { m11 * v.x + m12 * v.y, m21 * v.x + m22 * v.y }; }

    bool invert(Matrix3x3 &inv) const
    {
        double det = m11 * (m22 * m33 - m23 * m32) - m12 * (m21 * m33 - m23 * m31)
                     + m13 * (m21 * m32 - m22 * m31);
        if (isZero(det))
            return false;
        double id = 1.0 / det;
        inv.m11 = (m22 * m33 - m23 * m32) * id;
        inv.m12 = (m13 * m32 - m12 * m33) * id;
        inv.m13 = (m12 * m23 - m13 * m22) * id;
        inv.m21 = (m23 * m31 - m21 * m33) * id;
        inv.m22 = (m11 * m33 - m13 * m31) * id;
        inv.m23 = (m13 * m21 - m11 * m23) * id;
        inv.m31 = (m21 * m32 - m22 * m31) * id;
        inv.m32 = (m12 * m31 - m11 * m32) * id;
        inv.m33 = (m11 * m22 - m12 * m21) * id;
        return true;
    }
    Matrix3x3 inverted() const
    {
        Matrix3x3 inv;
        invert(inv);
        return inv;
    }
};

// =====================================================================
// 5. 轴对齐包围盒 - 与之前相同
// =====================================================================
struct Rect
{
    Vec2 minPoint;
    Vec2 maxPoint;
    static Rect Empty()
    {
        double inf = std::numeric_limits<double>::infinity();
        return { { inf, inf }, { -inf, -inf } };
    }
    static Rect fromXYWH(double x, double y, double w, double h)
    {
        return { { x, y }, { x + w, y + h } };
    }

    Rect normalized() const
    {
        return { { std::min(minPoint.x, maxPoint.x), std::min(minPoint.y, maxPoint.y) },
                 { std::max(minPoint.x, maxPoint.x), std::max(minPoint.y, maxPoint.y) } };
    }
    double width() const { return std::fabs(maxPoint.x - minPoint.x); }
    double height() const { return std::fabs(maxPoint.y - minPoint.y); }
    bool isEmpty() const
    {
        auto n = normalized();
        return n.width() < EPSILON || n.height() < EPSILON;
    }
    Vec2 center() const
    {
        return { (minPoint.x + maxPoint.x) * 0.5, (minPoint.y + maxPoint.y) * 0.5 };
    }
    bool contains(const Vec2 &p) const
    {
        auto n = normalized();
        return p.x >= n.minPoint.x - EPSILON && p.x <= n.maxPoint.x + EPSILON
               && p.y >= n.minPoint.y - EPSILON && p.y <= n.maxPoint.y + EPSILON;
    }
    bool intersects(const Rect &o) const
    {
        auto a = normalized(), b = o.normalized();
        return a.minPoint.x <= b.maxPoint.x && a.maxPoint.x >= b.minPoint.x
               && a.minPoint.y <= b.maxPoint.y && a.maxPoint.y >= b.minPoint.y;
    }
    Rect united(const Rect &o) const
    {
        return { { std::min(minPoint.x, o.minPoint.x), std::min(minPoint.y, o.minPoint.y) },
                 { std::max(maxPoint.x, o.maxPoint.x), std::max(maxPoint.y, o.maxPoint.y) } };
    }
    Rect intersected(const Rect &o) const
    {
        if (!intersects(o))
            return Empty();
        return { { std::max(minPoint.x, o.minPoint.x), std::max(minPoint.y, o.minPoint.y) },
                 { std::min(maxPoint.x, o.maxPoint.x), std::min(maxPoint.y, o.maxPoint.y) } };
    }
};

// =====================================================================
// 6. 几何算法
// =====================================================================

inline Vec2 cubicBezierPoint(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3,
                             double t)
{
    double mt = 1.0 - t, mt2 = mt * mt, mt3 = mt2 * mt;
    double t2 = t * t, t3 = t2 * t;
    return { mt3 * p0.x + 3 * mt2 * t * p1.x + 3 * mt * t2 * p2.x + t3 * p3.x,
             mt3 * p0.y + 3 * mt2 * t * p1.y + 3 * mt * t2 * p2.y + t3 * p3.y };
}

// 【已修复】精确计算三次贝塞尔曲线的包围盒
// 算法：B(t) = at^3 + bt^2 + ct + d。对其求导 B'(t) = 3at^2 + 2bt + c = 0
// 解一元二次方程求极值点 t，仅保留 (0, 1) 区间内的 t，代入原方程求极值
inline Rect cubicBezierBoundingBox(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3)
{
    auto calcAxisBounds = [](double v0, double v1, double v2, double v3, double &minVal,
                             double &maxVal) {
        // 初始化为端点值
        minVal = std::min(v0, v3);
        maxVal = std::max(v0, v3);

        // 计算多项式系数 B(t) = at^3 + bt^2 + ct + d
        double a = v3 - 3.0 * v2 + 3.0 * v1 - v0;
        double b = 3.0 * v2 - 6.0 * v1 + 3.0 * v0;
        double c = 3.0 * v1 - 3.0 * v0;
        // d = v0 (求导不需要)

        // 求导: B'(t) = 3at^2 + 2bt + c = 0
        if (isZero(a)) {
            // 退化为二次或一次曲线
            if (isZero(b))
                return; // 退化为直线，极值在端点，直接返回
            double t = -c / (2.0 * b);
            if (t > 0.0 && t < 1.0) {
                double val = ((a * t + b) * t + c) * t + v0;
                minVal = std::min(minVal, val);
                maxVal = std::max(maxVal, val);
            }
            return;
        }

        // 标准一元二次方程求根公式: t = (-b ± sqrt(b^2 - 4ac)) / 2a
        // 注意这里的系数对应: A=3a, B=2b, C=c
        double discriminant =
            b * b - 3.0 * a * c; // 这里其实是 (2b)^2 - 4*(3a)*c 的简化版 4b^2 - 12ac = 4(b^2 - 3ac)

        if (discriminant < 0.0)
            return; // 无实根，无极值点

        double sqrtDisc = std::sqrt(discriminant);

        // 根 1
        double t1 = (-b + sqrtDisc) / (3.0 * a);
        if (t1 > 0.0 && t1 < 1.0) {
            double val = ((a * t1 + b) * t1 + c) * t1 + v0; // 使用霍纳法则计算，减少乘法次数
            minVal = std::min(minVal, val);
            maxVal = std::max(maxVal, val);
        }

        // 根 2
        double t2 = (-b - sqrtDisc) / (3.0 * a);
        if (t2 > 0.0 && t2 < 1.0) {
            double val = ((a * t2 + b) * t2 + c) * t2 + v0;
            minVal = std::min(minVal, val);
            maxVal = std::max(maxVal, val);
        }
    };

    Rect bbox;
    calcAxisBounds(p0.x, p1.x, p2.x, p3.x, bbox.minPoint.x, bbox.maxPoint.x);
    calcAxisBounds(p0.y, p1.y, p2.y, p3.y, bbox.minPoint.y, bbox.maxPoint.y);
    return bbox;
}

inline double pointToSegmentDistance(const Vec2 &p, const Vec2 &a, const Vec2 &b)
{
    Vec2 ab = b - a, ap = p - a;
    double abLenSq = ab.lengthSquared();
    if (isZero(abLenSq))
        return ap.length();
    double t = clamp(ap.dot(ab) / abLenSq, 0.0, 1.0);
    return p.distanceTo(a + ab * t);
}

inline bool isPointNearSegment(const Vec2 &p, const Vec2 &a, const Vec2 &b, double tolerance)
{
    return pointToSegmentDistance(p, a, b) <= tolerance;
}

} // namespace AtMath

#endif // AT_MATH_CORE_H
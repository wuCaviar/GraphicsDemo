#include <QtTest>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <QtMath>

// ============================================================
// CMYK result struct (QColor::cyanF() is unreliable for stored CMYK)
// ============================================================

struct Cmyk {
    int c, m, y, k; // 0-100
};

// ============================================================
// Conversion helpers (static, test-local)
// ============================================================

/// sRGB gamma decode: 0-255 -> 0.0-1.0 linear
static double srgbToLinear(int v)
{
    double s = v / 255.0;
    if (s <= 0.04045)
        return s / 12.92;
    return qPow((s + 0.055) / 1.055, 2.4);
}

/// Linear -> sRGB encode: 0.0-1.0 -> 0-255
static int linearToSrgb(double v)
{
    double s;
    if (v <= 0.0031308)
        s = v * 12.92;
    else
        s = 1.055 * qPow(v, 1.0 / 2.4) - 0.055;
    return qBound(0, qRound(s * 255.0), 255);
}

/// Naive RGB->CMYK (matches current color_dialog.cpp)
static Cmyk naiveRgbToCmyk(int r, int g, int b)
{
    double R = r / 255.0;
    double G = g / 255.0;
    double B = b / 255.0;

    double K = 1.0 - qMax(R, qMax(G, B));
    if (K >= 1.0)
        return {0, 0, 0, 100};

    double C = (1.0 - R - K) / (1.0 - K);
    double M = (1.0 - G - K) / (1.0 - K);
    double Y = (1.0 - B - K) / (1.0 - K);

    return {qBound(0, qRound(C * 100.0), 100),
            qBound(0, qRound(M * 100.0), 100),
            qBound(0, qRound(Y * 100.0), 100),
            qBound(0, qRound(K * 100.0), 100)};
}

/// Naive CMYK->RGB (matches current color_dialog.cpp)
static QColor naiveCmykToRgb(int c, int m, int y, int k)
{
    double C = c / 100.0;
    double M = m / 100.0;
    double Y = y / 100.0;
    double K = k / 100.0;

    int r = qRound(255.0 * (1.0 - C) * (1.0 - K));
    int g = qRound(255.0 * (1.0 - M) * (1.0 - K));
    int bb = qRound(255.0 * (1.0 - Y) * (1.0 - K));

    return QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, bb, 255));
}

/// Photoshop-like RGB->CMYK with sRGB gamma + GCR + ink limit
/// @param gcrFactor 0.0 (no black) to 1.0 (maximum black replacement)
/// @param inkLimit  total ink limit in % (default 300)
static Cmyk photoshopRgbToCmyk(int r, int g, int b,
                               double gcrFactor = 0.5,
                               int inkLimit = 300)
{
    // Step 1: sRGB -> linear
    double R = srgbToLinear(r);
    double G = srgbToLinear(g);
    double B = srgbToLinear(b);

    // Step 2: linear RGB -> CMY (raw)
    double C_raw = 1.0 - R;
    double M_raw = 1.0 - G;
    double Y_raw = 1.0 - B;

    // Step 3: black generation with GCR
    double K_base = qMin(C_raw, qMin(M_raw, Y_raw));
    double K = K_base * gcrFactor;

    // Step 4: adjust CMY
    double C, M, Y;
    if (K >= 1.0) {
        C = M = Y = 0.0;
    } else {
        C = (C_raw - K) / (1.0 - K);
        M = (M_raw - K) / (1.0 - K);
        Y = (Y_raw - K) / (1.0 - K);
    }

    // Step 5: total ink limit
    double limit = inkLimit / 100.0;
    double total = C + M + Y + K;
    if (total > limit) {
        double scale = limit / total;
        C *= scale;
        M *= scale;
        Y *= scale;
        K *= scale;
    }

    return {qBound(0, qRound(C * 100.0), 100),
            qBound(0, qRound(M * 100.0), 100),
            qBound(0, qRound(Y * 100.0), 100),
            qBound(0, qRound(K * 100.0), 100)};
}

// ============================================================
// Test class
// ============================================================

class TestColorConversion : public QObject
{
    Q_OBJECT

private slots:
    // ---- sRGB gamma ----

    void testSrgbGammaDecode_Black()
    {
        QCOMPARE(srgbToLinear(0), 0.0);
    }

    void testSrgbGammaDecode_White()
    {
        QCOMPARE(srgbToLinear(255), 1.0);
    }

    void testSrgbGammaDecode_MidGray()
    {
        // 128 sRGB -> ~0.216 linear (NOT 0.5!)
        double lin = srgbToLinear(128);
        QVERIFY(lin > 0.20 && lin < 0.23);
    }

    void testSrgbGammaDecode_Roundtrip()
    {
        for (int v = 0; v <= 255; v += 17) {
            double lin = srgbToLinear(v);
            int back = linearToSrgb(lin);
            QVERIFY2(qAbs(back - v) <= 1,
                     qPrintable(QString("roundtrip failed for %1: got %2").arg(v).arg(back)));
        }
    }

    // ---- Naive CMYK (baseline, matches color_dialog.cpp) ----

    void testNaiveRgbToCmyk_Black()
    {
        Cmyk c = naiveRgbToCmyk(0, 0, 0);
        QCOMPARE(c.c, 0);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 100);
    }

    void testNaiveRgbToCmyk_White()
    {
        Cmyk c = naiveRgbToCmyk(255, 255, 255);
        QCOMPARE(c.c, 0);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 0);
    }

    void testNaiveRgbToCmyk_Red()
    {
        Cmyk c = naiveRgbToCmyk(255, 0, 0);
        QCOMPARE(c.c, 0);
        QCOMPARE(c.m, 100);
        QCOMPARE(c.y, 100);
        QCOMPARE(c.k, 0);
    }

    void testNaiveRgbToCmyk_Green()
    {
        Cmyk c = naiveRgbToCmyk(0, 255, 0);
        QCOMPARE(c.c, 100);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 100);
        QCOMPARE(c.k, 0);
    }

    void testNaiveRgbToCmyk_Blue()
    {
        Cmyk c = naiveRgbToCmyk(0, 0, 255);
        QCOMPARE(c.c, 100);
        QCOMPARE(c.m, 100);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 0);
    }

    void testNaiveRgbToCmyk_Cyan()
    {
        Cmyk c = naiveRgbToCmyk(0, 255, 255);
        QCOMPARE(c.c, 100);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 0);
    }

    void testNaiveRgbToCmyk_MidGray()
    {
        Cmyk c = naiveRgbToCmyk(128, 128, 128);
        QCOMPARE(c.c, 0);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 50);
    }

    void testNaiveCmykToRgb_Roundtrip()
    {
        struct { int r, g, b; } colors[] = {
            {0, 0, 0}, {255, 255, 255}, {255, 0, 0},
            {0, 255, 0}, {0, 0, 255}, {128, 128, 128}
        };
        for (auto &col : colors) {
            Cmyk cmyk = naiveRgbToCmyk(col.r, col.g, col.b);
            QColor rgb = naiveCmykToRgb(cmyk.c, cmyk.m, cmyk.y, cmyk.k);
            QVERIFY2(qAbs(rgb.red()   - col.r) <= 2,
                     qPrintable(QString("R roundtrip: %1->%2").arg(col.r).arg(rgb.red())));
            QVERIFY2(qAbs(rgb.green() - col.g) <= 2,
                     qPrintable(QString("G roundtrip: %1->%2").arg(col.g).arg(rgb.green())));
            QVERIFY2(qAbs(rgb.blue()  - col.b) <= 2,
                     qPrintable(QString("B roundtrip: %1->%2").arg(col.b).arg(rgb.blue())));
        }
    }

    // ---- Photoshop-like CMYK (gamma + GCR) ----

    void testPhotoshopRgbToCmyk_Black()
    {
        // With GCR=1.0, pure black -> K=100, C=M=Y=0
        Cmyk c = photoshopRgbToCmyk(0, 0, 0, 1.0);
        QCOMPARE(c.c, 0);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 100);

        // With GCR=0.5, pure black distributes across CMY+K, capped at ink limit
        Cmyk c2 = photoshopRgbToCmyk(0, 0, 0, 0.5, 300);
        QVERIFY(c2.k > 0);
        QCOMPARE(c2.c, c2.m);
        QCOMPARE(c2.m, c2.y);
        int total = c2.c + c2.m + c2.y + c2.k;
        QVERIFY(total <= 301);
    }

    void testPhotoshopRgbToCmyk_White()
    {
        Cmyk c = photoshopRgbToCmyk(255, 255, 255);
        QCOMPARE(c.c, 0);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 0);
    }

    void testPhotoshopRgbToCmyk_Red()
    {
        Cmyk c = photoshopRgbToCmyk(255, 0, 0, 0.5);
        QCOMPARE(c.c, 0);
        QCOMPARE(c.m, 100);
        QCOMPARE(c.y, 100);
        QCOMPARE(c.k, 0);
    }

    void testPhotoshopRgbToCmyk_Green()
    {
        Cmyk c = photoshopRgbToCmyk(0, 255, 0, 0.5);
        QCOMPARE(c.c, 100);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 100);
        QCOMPARE(c.k, 0);
    }

    void testPhotoshopRgbToCmyk_Blue()
    {
        Cmyk c = photoshopRgbToCmyk(0, 0, 255, 0.5);
        QCOMPARE(c.c, 100);
        QCOMPARE(c.m, 100);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 0);
    }

    void testPhotoshopRgbToCmyk_Cyan()
    {
        Cmyk c = photoshopRgbToCmyk(0, 255, 255, 0.5);
        QCOMPARE(c.c, 100);
        QCOMPARE(c.m, 0);
        QCOMPARE(c.y, 0);
        QCOMPARE(c.k, 0);
    }

    void testPhotoshopRgbToCmyk_MidGray()
    {
        // With gamma correction, 128 sRGB -> ~0.216 linear
        // C_raw = M_raw = Y_raw = 1 - 0.216 = 0.784
        // GCR medium (0.5): K = 0.784 * 0.5 = 0.392
        // After adjust: C = M = Y = (0.784 - 0.392) / (1 - 0.392) = 0.645
        Cmyk c = photoshopRgbToCmyk(128, 128, 128, 0.5);
        QVERIFY(c.k > 0);
        QCOMPARE(c.c, c.m);
        QCOMPARE(c.m, c.y);
        // K should be around 39 (not 50 as in naive)
        QVERIFY2(c.k > 30 && c.k < 45,
                 qPrintable(QString("K=%1").arg(c.k)));
    }

    void testPhotoshopGcrNone()
    {
        Cmyk c = photoshopRgbToCmyk(128, 128, 128, 0.0);
        QCOMPARE(c.k, 0);
    }

    void testPhotoshopGcrHeavy()
    {
        Cmyk c = photoshopRgbToCmyk(128, 128, 128, 1.0);
        QVERIFY(c.k > 70);
        QCOMPARE(c.c, c.m);
        QCOMPARE(c.m, c.y);
    }

    void testPhotoshopTotalInkLimit()
    {
        Cmyk c = photoshopRgbToCmyk(10, 10, 10, 0.5, 300);
        int total = c.c + c.m + c.y + c.k;
        QVERIFY2(total <= 301,
                 qPrintable(QString("total ink = %1").arg(total)));
    }

    void testPhotoshopVsNaive_Difference()
    {
        struct { int r, g, b; const char *name; } colors[] = {
            {128, 128, 128, "mid-gray"},
            {64,  64,  64,  "dark-gray"},
            {192, 128, 64,  "warm-tone"},
            {0,   128, 255, "sky-blue"},
        };

        for (auto &col : colors) {
            Cmyk n = naiveRgbToCmyk(col.r, col.g, col.b);
            Cmyk p = photoshopRgbToCmyk(col.r, col.g, col.b, 0.5);

            bool differs = (n.c != p.c || n.m != p.m || n.y != p.y || n.k != p.k);
            QVERIFY2(differs,
                     qPrintable(QString("%1: naive and photoshop should differ").arg(col.name)));
        }
    }

    // ---- Full conversion table printout -> markdown file ----

    void testPrintConversionTable()
    {
        struct { int r, g, b; const char *name; } colors[] = {
            {  0,   0,   0, "Black"},
            {255, 255, 255, "White"},
            {255,   0,   0, "Red"},
            {  0, 255,   0, "Green"},
            {  0,   0, 255, "Blue"},
            {255, 255,   0, "Yellow"},
            {  0, 255, 255, "Cyan"},
            {255,   0, 255, "Magenta"},
            {128, 128, 128, "MidGray"},
            { 64,  64,  64, "DarkGray"},
            {192, 192, 192, "LightGray"},
            {255, 128,   0, "Orange"},
            {128,   0, 128, "Purple"},
            {  0, 128,   0, "DarkGreen"},
            {  0, 128, 255, "SkyBlue"},
            {192, 128,  64, "WarmTone"},
            { 64,  64, 192, "Navy"},
            {192,  64,  64, "Brick"},
            { 64, 192,  64, "Lime"},
            {100, 100, 100, "Gray40"},
            {150, 150, 150, "Gray59"},
            {200, 200, 200, "Gray78"},
            { 32,   32,  32, "NearBlack"},
            {220, 220, 220, "NearWhite"},
        };
        const int count = sizeof(colors) / sizeof(colors[0]);

        QFile file(QStringLiteral("cmyk_conversion_report.md"));
        QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text),
                 qPrintable(file.errorString()));
        QTextStream out(&file);

        // Title
        out << "# RGB -> CMYK Conversion Report\n\n";
        out << "Comparison: **Naive** (current `color_dialog.cpp`) vs **Photoshop-like** "
               "(sRGB gamma decode + GCR + total ink limit)\n\n";

        // ---- sRGB gamma section ----
        out << "## sRGB Gamma Decode\n\n";
        out << "The sRGB transfer function decodes gamma ~2.2 before conversion.\n";
        out << "This means mid-gray (128) is actually **0.216** linear, not 0.5.\n\n";
        out << "| sRGB | Linear |\n";
        out << "|-----:|-------:|\n";
        for (int v = 0; v <= 255; v += 32)
            out << "| " << v << " | " << QString::number(srgbToLinear(v), 'f', 4) << " |\n";
        out << "\n";

        // ---- Main comparison table ----
        out << "## Conversion Comparison Table\n\n";
        out << "**Columns:**\n";
        out << "- **Naive (N)**: `K = 1 - max(R,G,B)`, no gamma correction\n";
        out << "- **GCR 0.5 (P5)**: sRGB gamma + GCR factor 0.5 + 300% ink limit\n";
        out << "- **GCR 1.0 (P1)**: sRGB gamma + GCR factor 1.0 (heavy black) + 300% ink limit\n";
        out << "- **GCR 0.0 (P0)**: sRGB gamma + no black generation + 300% ink limit\n\n";

        out << "| Color | R | G | B | linR | linG | linB | N(C) | N(M) | N(Y) | N(K) |";
        out << " P5(C) | P5(M) | P5(Y) | P5(K) |";
        out << " P1(C) | P1(M) | P1(Y) | P1(K) |";
        out << " P0(C) | P0(M) | P0(Y) | P0(K) |";
        out << " K diff |\n";

        out << "|-------|--:|--:|--:|-----:|-----:|-----:|-----:|-----:|-----:|-----:|";
        out << "------:|------:|------:|------:|";
        out << "------:|------:|------:|------:|";
        out << "------:|------:|------:|------:|";
        out << "--------|\n";

        for (int i = 0; i < count; i++) {
            int r = colors[i].r, g = colors[i].g, b = colors[i].b;
            double sR = srgbToLinear(r), sG = srgbToLinear(g), sB = srgbToLinear(b);

            Cmyk n  = naiveRgbToCmyk(r, g, b);
            Cmyk p  = photoshopRgbToCmyk(r, g, b, 0.5, 300);
            Cmyk ph = photoshopRgbToCmyk(r, g, b, 1.0, 300);
            Cmyk pn = photoshopRgbToCmyk(r, g, b, 0.0, 300);

            QString kDiff = (n.k == p.k)
                ? QStringLiteral("same")
                : QString("%1->%2").arg(n.k).arg(p.k);

            // Color preview swatch (hex)
            QColor preview(r, g, b);

            out << "| " << colors[i].name
                << " | " << r << " | " << g << " | " << b
                << " | " << QString::number(sR, 'f', 3)
                << " | " << QString::number(sG, 'f', 3)
                << " | " << QString::number(sB, 'f', 3)
                << " | " << n.c << " | " << n.m << " | " << n.y << " | " << n.k
                << " | " << p.c << " | " << p.m << " | " << p.y << " | " << p.k
                << " | " << ph.c << " | " << ph.m << " | " << ph.y << " | " << ph.k
                << " | " << pn.c << " | " << pn.m << " | " << pn.y << " | " << pn.k
                << " | " << kDiff << " |\n";
        }
        out << "\n";

        // ---- Key observations ----
        out << "## Key Observations\n\n";

        // Neutral grays
        out << "### Neutral Grays\n\n";
        out << "| sRGB | Naive K | Photoshop K (GCR=0.5) | Delta |\n";
        out << "|-----:|--------:|----------------------:|------:|\n";
        int grays[] = {0, 32, 64, 100, 128, 150, 192, 200, 220, 255};
        for (int v : grays) {
            Cmyk n = naiveRgbToCmyk(v, v, v);
            Cmyk p = photoshopRgbToCmyk(v, v, v, 0.5);
            int delta = p.k - n.k;
            out << "| " << v
                << " | " << n.k
                << " | " << p.k
                << " | " << (delta >= 0 ? "+" : "") << delta << " |\n";
        }
        out << "\n";

        // Dark color ink limit
        out << "### Ink Limit Test (dark colors, GCR=0.5, limit=300%)\n\n";
        out << "| RGB | C | M | Y | K | Total | Capped? |\n";
        out << "|-----|--:|--:|--:|--:|------:|---------|\n";
        int darks[][3] = {{10,10,10}, {20,20,20}, {30,30,30}, {40,40,40}, {50,50,50}};
        for (auto &d : darks) {
            Cmyk c = photoshopRgbToCmyk(d[0], d[1], d[2], 0.5, 300);
            int total = c.c + c.m + c.y + c.k;
            out << "| (" << d[0] << "," << d[1] << "," << d[2] << ")"
                << " | " << c.c << " | " << c.m << " | " << c.y << " | " << c.k
                << " | " << total
                << " | " << (total > 300 ? "YES" : "no") << " |\n";
        }
        out << "\n";

        // GCR factor comparison
        out << "### GCR Factor Comparison (128,128,128)\n\n";
        out << "| GCR Factor | C | M | Y | K | Total |\n";
        out << "|-----------:|--:|--:|--:|--:|------:|\n";
        double gcrs[] = {0.0, 0.25, 0.5, 0.75, 1.0};
        for (double gcr : gcrs) {
            Cmyk c = photoshopRgbToCmyk(128, 128, 128, gcr);
            out << "| " << QString::number(gcr, 'f', 2)
                << " | " << c.c << " | " << c.m << " | " << c.y << " | " << c.k
                << " | " << (c.c + c.m + c.y + c.k) << " |\n";
        }
        out << "\n";

        // Primary colors identity check
        out << "### Primary Colors (should be identical)\n\n";
        out << "| Color | Naive C/M/Y/K | Photoshop C/M/Y/K | Match? |\n";
        out << "|-------|---------------|-------------------|--------|\n";
        struct { int r,g,b; const char *n; } primaries[] = {
            {255,0,0,"Red"}, {0,255,0,"Green"}, {0,0,255,"Blue"},
            {0,255,255,"Cyan"}, {255,0,255,"Magenta"}, {255,255,0,"Yellow"}
        };
        for (auto &p : primaries) {
            Cmyk n = naiveRgbToCmyk(p.r, p.g, p.b);
            Cmyk ps = photoshopRgbToCmyk(p.r, p.g, p.b, 0.5);
            bool match = (n.c == ps.c && n.m == ps.m && n.y == ps.y && n.k == ps.k);
            out << "| " << p.n
                << " | " << n.c << "/" << n.m << "/" << n.y << "/" << n.k
                << " | " << ps.c << "/" << ps.m << "/" << ps.y << "/" << ps.k
                << " | " << (match ? "YES" : "**NO**") << " |\n";
        }
        out << "\n";

        // Algorithm description
        out << "## Algorithm Description\n\n";
        out << "### Naive (current `color_dialog.cpp`)\n\n";
        out << "```text\n";
        out << "K = 1 - max(R, G, B)          // R,G,B in 0.0-1.0\n";
        out << "C = (1 - R - K) / (1 - K)\n";
        out << "M = (1 - G - K) / (1 - K)\n";
        out << "Y = (1 - B - K) / (1 - K)\n";
        out << "```\n\n";
        out << "### Photoshop-like\n\n";
        out << "```text\n";
        out << "// Step 1: sRGB gamma decode\n";
        out << "if (V <= 0.04045) V_lin = V / 12.92\n";
        out << "else              V_lin = ((V + 0.055) / 1.055) ^ 2.4\n\n";
        out << "// Step 2: Invert to CMY\n";
        out << "C_raw = 1 - R_lin;  M_raw = 1 - G_lin;  Y_raw = 1 - B_lin\n\n";
        out << "// Step 3: Black generation (GCR)\n";
        out << "K = min(C_raw, M_raw, Y_raw) * gcrFactor\n\n";
        out << "// Step 4: Adjust CMY\n";
        out << "C = (C_raw - K) / (1 - K)\n";
        out << "M = (M_raw - K) / (1 - K)\n";
        out << "Y = (Y_raw - K) / (1 - K)\n\n";
        out << "// Step 5: Total ink limit (default 300%)\n";
        out << "if (C + M + Y + K > 3.0) { scale = 3.0 / total; C*=s; M*=s; Y*=s; K*=s }\n";
        out << "```\n\n";

        out << "---\n\n";
        out << "*Generated by `tst_colorconversion`\n";

        file.close();
        QVERIFY(true);
    }
};

QTEST_MAIN(TestColorConversion)
#include "tst_colorconversion.moc"

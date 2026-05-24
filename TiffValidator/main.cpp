#include <tiff.h>
#include <tiffio.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Helper: tag name → string
// ---------------------------------------------------------------------------
static const char *photometricName(uint16_t v) {
    switch (v) {
    case PHOTOMETRIC_MINISWHITE: return "MinIsWhite (0)";
    case PHOTOMETRIC_MINISBLACK: return "MinIsBlack (1)";
    case PHOTOMETRIC_RGB:        return "RGB (2)";
    case PHOTOMETRIC_PALETTE:    return "Palette (3)";
    case PHOTOMETRIC_MASK:       return "Mask (4)";
    case PHOTOMETRIC_SEPARATED:  return "Separated/CMYK (5)";
    case PHOTOMETRIC_YCBCR:      return "YCbCr (6)";
    case PHOTOMETRIC_CIELAB:     return "CIELab (8)";
    case PHOTOMETRIC_ICCLAB:     return "ICCLab (9)";
    case PHOTOMETRIC_ITULAB:     return "ITULab (10)";
    case PHOTOMETRIC_LOGL:       return "LogL (32844)";
    case PHOTOMETRIC_LOGLUV:     return "LogLuv (32845)";
    default:                     return "UNKNOWN";
    }
}

static const char *compressionName(uint16_t v) {
    switch (v) {
    case COMPRESSION_NONE:       return "None (1)";
    case COMPRESSION_CCITTRLE:   return "CCITT RLE (2)";
    case COMPRESSION_CCITTFAX3:  return "CCITT Fax3/T.4 (3)";
    case COMPRESSION_CCITTFAX4:  return "CCITT Fax4/T.6 (4)";
    case COMPRESSION_LZW:        return "LZW (5)";
    case COMPRESSION_OJPEG:      return "Old JPEG (6)";
    case COMPRESSION_JPEG:       return "JPEG (7)";
    case COMPRESSION_ADOBE_DEFLATE: return "Adobe Deflate/ZIP (8)";
    case COMPRESSION_PACKBITS:   return "PackBits (32773)";
    default:                     return "UNKNOWN";
    }
}

static const char *sampleFormatName(uint16_t v) {
    switch (v) {
    case SAMPLEFORMAT_UINT:      return "UINT (1)";
    case SAMPLEFORMAT_INT:       return "INT (2)";
    case SAMPLEFORMAT_IEEEFP:    return "IEEEFP (3)";
    case SAMPLEFORMAT_VOID:      return "VOID (4)";
    default:                     return "UNKNOWN";
    }
}

static const char *planarName(uint16_t v) {
    switch (v) {
    case PLANARCONFIG_CONTIG:    return "Contig/Chunky (1)";
    case PLANARCONFIG_SEPARATE:  return "Separate/Planar (2)";
    default:                     return "UNKNOWN";
    }
}

static const char *resUnitName(uint16_t v) {
    switch (v) {
    case RESUNIT_NONE:       return "None (1)";
    case RESUNIT_INCH:       return "Inch (2)";
    case RESUNIT_CENTIMETER: return "Centimeter (3)";
    default:                 return "UNKNOWN";
    }
}

static const char *inkSetName(uint16_t v) {
    switch (v) {
    case INKSET_CMYK:        return "CMYK (1)";
    case INKSET_MULTIINK:    return "MultiInk (2)";
    default:                 return "UNKNOWN";
    }
}

static const char *extraSampleName(uint16_t v) {
    switch (v) {
    case EXTRASAMPLE_UNSPECIFIED:  return "Unspecified";
    case EXTRASAMPLE_ASSOCALPHA:   return "Associated Alpha";
    case EXTRASAMPLE_UNASSALPHA:   return "Unassociated Alpha";
    default:                       return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Print helpers
// ---------------------------------------------------------------------------
static void printTagStr(const char *name, const char *value) {
    printf("  %-30s %s\n", name, value);
}

static void printTagUInt(const char *name, unsigned int val) {
    printf("  %-30s %u\n", name, val);
}

static void printTagFloat(const char *name, double val) {
    printf("  %-30s %.2f\n", name, val);
}

static void printTagLong(const char *name, long long val) {
    printf("  %-30s %lld\n", name, val);
}

// ---------------------------------------------------------------------------
// Check if a file exists
// ---------------------------------------------------------------------------
static bool fileExists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// ---------------------------------------------------------------------------
// Validate a single TIFF page
// ---------------------------------------------------------------------------
struct PageIssues {
    int errors = 0;
    int warnings = 0;
};

static PageIssues validatePage(TIFF *tif, int pageNum)
{
    PageIssues issues;

    auto warn = [&](const char *msg) {
        printf("  [WARN]  %s\n", msg);
        issues.warnings++;
    };
    auto err = [&](const char *msg) {
        printf("  [ERROR] %s\n", msg);
        issues.errors++;
    };

    uint32_t width = 0, height = 0;
    uint16_t bitsPerSample = 0, samplesPerPixel = 0;
    uint16_t photometric = 0, planarConfig = 0;
    uint16_t compression = 0, sampleFormat = 0;
    uint16_t orientation = 0, resUnit = 0;
    float xRes = 0, yRes = 0;
    uint16_t inkSet = 0, numberOfInks = 0;
    uint32_t rowsPerStrip = 0;
    uint32_t subfileType = 0;
    uint16_t pageNumVal = 0, pageTotal = 0;
    char *dateTime = nullptr;
    char *software = nullptr;
    char *artist = nullptr;
    char *documentName = nullptr;
    char *imageDescription = nullptr;
    uint32_t iccSize = 0;
    void *iccData = nullptr;
    uint16_t *bpsArray = nullptr;
    uint16_t bpsCount = 0;
    uint16_t *extraSampleTypes = nullptr;
    uint16_t extraSamplesCount = 0;
    uint32_t tileWidth = 0, tileHeight = 0;
    bool isTiled = false;

    // Required fields
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
    TIFFGetFieldDefaulted(tif, TIFFTAG_COMPRESSION, &compression);
    TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
    TIFFGetFieldDefaulted(tif, TIFFTAG_XRESOLUTION, &xRes);
    TIFFGetFieldDefaulted(tif, TIFFTAG_YRESOLUTION, &yRes);
    TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resUnit);
    TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

    // Get BPS array (call scalar first to warm up the tag, then array)
    (void)bitsPerSample;
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bpsCount, &bpsArray);

    // Optional tags
    TIFFGetFieldDefaulted(tif, TIFFTAG_SUBFILETYPE, &subfileType);
    TIFFGetField(tif, TIFFTAG_DATETIME, &dateTime);
    TIFFGetField(tif, TIFFTAG_SOFTWARE, &software);
    TIFFGetField(tif, TIFFTAG_ARTIST, &artist);
    TIFFGetField(tif, TIFFTAG_DOCUMENTNAME, &documentName);
    TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &imageDescription);
    TIFFGetField(tif, TIFFTAG_INKSET, &inkSet);
    TIFFGetField(tif, TIFFTAG_NUMBEROFINKS, &numberOfInks);
    TIFFGetField(tif, TIFFTAG_ICCPROFILE, &iccSize, &iccData);
    TIFFGetField(tif, TIFFTAG_PAGENUMBER, &pageNumVal, &pageTotal);
    TIFFGetField(tif, TIFFTAG_EXTRASAMPLES, &extraSamplesCount, &extraSampleTypes);

    isTiled = TIFFIsTiled(tif);
    if (isTiled) {
        TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileHeight);
    }

    // -----------------------------------------------------------------------
    // Print summary
    // -----------------------------------------------------------------------
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Page %d\n", pageNum);
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  %-30s %u x %u\n", "Dimensions:", width, height);

    if (bpsArray && bpsCount > 0) {
        printf("  BitsPerSample:       [");
        for (uint16_t i = 0; i < bpsCount; ++i) {
            if (i > 0) printf(", ");
            printf("%u", bpsArray[i]);
        }
        printf("] (count=%u)\n", bpsCount);
    } else {
        printTagUInt("BitsPerSample:", bitsPerSample);
    }
    printTagUInt("SamplesPerPixel:",  samplesPerPixel);
    printTagStr("Photometric:",   photometricName(photometric));
    printTagStr("Compression:",   compressionName(compression));
    printTagStr("PlanarConfig:",  planarName(planarConfig));
    printTagStr("SampleFormat:",  sampleFormatName(sampleFormat));
    printTagStr("Orientation:",   "see below");
    printTagUInt("  Orientation:", (unsigned)orientation);
    printTagFloat("XResolution:",      xRes);
    printTagFloat("YResolution:",      yRes);
    printTagStr("ResolutionUnit:", resUnitName(resUnit));
    printTagUInt("RowsPerStrip:",     rowsPerStrip);
    printTagUInt("SubfileType:",      subfileType);
    char pageNumBuf[64];
    snprintf(pageNumBuf, sizeof(pageNumBuf), "%u / %u", pageNumVal, pageTotal);
    printTagStr("PageNumber:", pageNumBuf);

    if (software)        printTagStr("Software:",     software);
    if (dateTime)        printTagStr("DateTime:",     dateTime);
    if (artist)          printTagStr("Artist:",       artist);
    if (documentName)    printTagStr("DocumentName:", documentName);
    if (imageDescription) printTagStr("ImageDesc:",   imageDescription);

    if (photometric == PHOTOMETRIC_SEPARATED) {
        if (inkSet) printTagStr("InkSet:",          inkSetName(inkSet));
        printTagUInt("NumberOfInks:", numberOfInks);
    }

    if (iccData)         printTagUInt("ICC Profile:",    iccSize);

    if (extraSamplesCount > 0 && extraSampleTypes) {
        printf("  ExtraSamples: %u\n", extraSamplesCount);
        for (uint16_t i = 0; i < extraSamplesCount; ++i)
            printf("    [%u] %s\n", i, extraSampleName(extraSampleTypes[i]));
    }

    if (isTiled) {
        printf("  [TILED] Tile size: %u x %u\n", tileWidth, tileHeight);
    }

    printf("\n  Strip/Tile info:\n");
    if (isTiled) {
        uint32_t nTiles = TIFFNumberOfTiles(tif);
        printTagUInt("  TileCount:", nTiles);
        tmsize_t ts = TIFFTileSize(tif);
        printTagLong("  TileSize:", (long long)ts);
    } else {
        uint32_t nStrips = TIFFNumberOfStrips(tif);
        printTagUInt("  StripCount:", nStrips);
        tmsize_t ss = TIFFStripSize(tif);
        printTagLong("  StripSize:", (long long)ss);
    }

    // ---- Validation checks ----
    printf("\n  Validation:\n");

    // 1. Check dimensions
    if (width == 0 || height == 0)
        err("Width or height is zero");
    if (width > 50000 || height > 50000)
        warn("Dimensions are very large");

    // 2. Check required tags
    if (photometric == 0 || photometric > 20)
        err("Invalid or missing Photometric tag");

    if (samplesPerPixel == 0 || samplesPerPixel > 16)
        err("Invalid SamplesPerPixel");

    if (bpsArray && bpsCount > 0) {
        for (uint16_t i = 0; i < bpsCount; ++i) {
            if (bpsArray[i] == 0 || bpsArray[i] > 64)
                err("Invalid BitsPerSample value");
        }
        if (bpsCount != samplesPerPixel)
            err("BitsPerSample count does not match SamplesPerPixel");
    } else if (bitsPerSample == 0 || bitsPerSample > 64) {
        err("Invalid BitsPerSample value");
    }

    // 3. CMYK-specific checks
    if (photometric == PHOTOMETRIC_SEPARATED) {
        if (samplesPerPixel < 4)
            err("CMYK photometric but SamplesPerPixel < 4");
        if (inkSet == 0)
            warn("CMYK but no InkSet tag");
        if (numberOfInks == 0)
            warn("CMYK but no NumberOfInks tag");
        if (numberOfInks > 0 && numberOfInks < 4)
            err("NumberOfInks < 4 for CMYK");
    }

    // 4. RGB-specific checks
    if (photometric == PHOTOMETRIC_RGB) {
        if (samplesPerPixel < 3)
            err("RGB photometric but SamplesPerPixel < 3");
    }

    // 5. Check planar config consistency
    if (planarConfig != PLANARCONFIG_CONTIG && planarConfig != PLANARCONFIG_SEPARATE)
        warn("Unrecognized PlanarConfig value");

    // 6. Check SOFTWARE tag (our own tool should write this)
    if (!software)
        warn("Missing SOFTWARE tag (recommended)");

    // 7. Check DPI
    if (xRes <= 0 || yRes <= 0)
        warn("Non-positive resolution values");
    if (resUnit == RESUNIT_NONE && (xRes != 0 || yRes != 0))
        warn("Resolution values set but ResolutionUnit is NONE");

    // 8. Check BPS array count matches SamplesPerPixel
    if (bpsCount > 0 && bpsCount != samplesPerPixel)
        err("BITSPERSAMPLE count does not match SamplesPerPixel");

    // 9. Try to read all pixel data
    printf("\n  Reading pixel data...\n");
    bool readOk = true;
    tdata_t buf = nullptr;
    tmsize_t scanlineSize = TIFFScanlineSize(tif);

    if (scanlineSize <= 0) {
        err("ScanlineSize is zero or negative — file may be truncated");
        printf("  → This usually means the file is corrupted or TIFFClose was not called properly\n");
        readOk = false;
    } else {
        buf = _TIFFmalloc(scanlineSize);
        if (!buf) {
            err("Failed to allocate scanline buffer");
            readOk = false;
        }
    }

    if (buf) {
        int readErrors = 0;
        if (planarConfig == PLANARCONFIG_CONTIG) {
            for (uint32_t row = 0; row < height && readOk; ++row) {
                if (TIFFReadScanline(tif, buf, row, 0) < 0) {
                    readErrors++;
                    if (readErrors <= 3) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "TIFFReadScanline failed at row %u", row);
                        err(msg);
                    }
                    readOk = false;
                }
            }
        } else {
            for (uint16_t s = 0; s < samplesPerPixel && readOk; ++s) {
                for (uint32_t row = 0; row < height && readOk; ++row) {
                    if (TIFFReadScanline(tif, buf, row, s) < 0) {
                        readErrors++;
                        if (readErrors <= 3) {
                            char msg[128];
                            snprintf(msg, sizeof(msg),
                                     "TIFFReadScanline failed at row %u sample %u", row, s);
                            err(msg);
                        }
                        readOk = false;
                    }
                }
            }
        }
        if (readErrors > 3) {
            char msg[64];
            snprintf(msg, sizeof(msg), "... and %d more read errors", readErrors - 3);
            err(msg);
        }
        _TIFFfree(buf);
    }

    if (readOk) {
        printf("  [OK]    All scanlines read successfully (%u rows)\n", height);
    }

    // 10. Check pixel data validity (sample first and last row)
    buf = _TIFFmalloc(scanlineSize > 0 ? scanlineSize : 1);
    if (buf && scanlineSize > 0) {
        bool hasNonZero = false;
        if (TIFFReadScanline(tif, buf, 0, 0) >= 0) {
            for (tmsize_t i = 0; i < scanlineSize; ++i) {
                if (static_cast<uint8_t *>(buf)[i] != 0) { hasNonZero = true; break; }
            }
        }
        if (height > 1 && TIFFReadScanline(tif, buf, height - 1, 0) >= 0) {
            for (tmsize_t i = 0; i < scanlineSize && !hasNonZero; ++i) {
                if (static_cast<uint8_t *>(buf)[i] != 0) { hasNonZero = true; break; }
            }
        }
        if (!hasNonZero)
            warn("First and last rows are all zero — image may be blank");
        _TIFFfree(buf);
    }

    // Summary
    printf("\n  ─────────────────────────────────────────────\n");
    if (issues.errors == 0 && issues.warnings == 0)
        printf("  Result: ALL OK\n");
    else
        printf("  Result: %d error(s), %d warning(s)\n", issues.errors, issues.warnings);

    return issues;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("TiffValidator — validate TIFF file structure and data integrity\n");
        printf("\n");
        printf("Usage: TiffValidator <file.tif>\n");
        printf("\n");
        printf("Checks:\n");
        printf("  - All standard tags present and consistent\n");
        printf("  - Pixel data readable without errors\n");
        printf("  - CMYK-specific tags (InkSet, NumberOfInks)\n");
        printf("  - Multi-page structure\n");
        printf("  - ICC profile presence\n");
        printf("  - BITSPERSAMPLE array count matches SamplesPerPixel\n");
        return 1;
    }

    const char *filePath = argv[1];

    if (!fileExists(filePath)) {
        printf("ERROR: File not found: %s\n", filePath);
        return 1;
    }

    // Get file size
    struct stat st;
    stat(filePath, &st);
    printf("File: %s\n", filePath);
    printf("Size: %lld bytes\n", (long long)st.st_size);

    TIFF *tif = TIFFOpen(filePath, "r");
    if (!tif) {
        printf("ERROR: Cannot open as TIFF (not a valid TIFF or corrupt)\n");
        printf("  → libtiff TIFFOpen() returned NULL\n");
        printf("  → The file may be truncated, have a corrupt header, or not be a TIFF\n");
        return 1;
    }

    printf("Format: Valid TIFF (libtiff opened successfully)\n");

    bool isMultiPage = !TIFFLastDirectory(tif);
    if (isMultiPage)
        printf("Type: Multi-page TIFF\n");
    else
        printf("Type: Single-page TIFF\n");

    int totalErrors = 0, totalWarnings = 0;
    int pageNum = 0;

    // Reset to first directory
    TIFFSetDirectory(tif, 0);

    do {
        TIFFSetDirectory(tif, pageNum);

        PageIssues pi = validatePage(tif, pageNum);
        totalErrors += pi.errors;
        totalWarnings += pi.warnings;
        pageNum++;
    } while (TIFFReadDirectory(tif));

    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  FINAL SUMMARY\n");
    printf("  Pages:    %d\n", pageNum);
    printf("  Errors:   %d\n", totalErrors);
    printf("  Warnings: %d\n", totalWarnings);
    printf("  Verdict:  %s\n",
           totalErrors == 0 ? (totalWarnings == 0 ? "PASS" : "PASS (with warnings)") : "FAIL");
    printf("═══════════════════════════════════════════════════════════\n");

    TIFFClose(tif);

    return totalErrors > 0 ? 1 : 0;
}

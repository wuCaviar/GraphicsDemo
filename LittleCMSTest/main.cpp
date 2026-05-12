#include <QCoreApplication>

#include <lcms2.h>

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

cmsHPROFILE sRGBProfile = nullptr, CMYKProfile = nullptr;

bool readProfile()
{
    sRGBProfile = cmsOpenProfileFromFile(
        "/Volumes/Caviar/Test/sRGB IEC61966-21.icc", "r");
    if (!sRGBProfile) return false;

    CMYKProfile = cmsOpenProfileFromFile(
        "/Volumes/Caviar/Test/Japan Color 2001 Coated.icc", "r");
    if (!CMYKProfile) {
        cmsCloseProfile(sRGBProfile);
        sRGBProfile = nullptr;
        return false;
    }
    return true;
}

void closeFile()
{
    cmsCloseProfile(sRGBProfile);
    sRGBProfile = nullptr;

    cmsCloseProfile(CMYKProfile);
    CMYKProfile = nullptr;
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    cmsSetLogErrorHandler(errorLogger);

    if (!readProfile()) {
        fprintf(stderr, "[Error] Failed to load profiles\n");
        closeFile();
        return -1;
    }

    cmsHTRANSFORM transform = cmsCreateTransform(
        sRGBProfile, TYPE_RGB_8,
        CMYKProfile, TYPE_CMYK_8,
        INTENT_PERCEPTUAL,
        cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_LOWRESPRECALC);

    if (!transform) {
        fprintf(stderr, "[Error] Failed to create transform\n");
        closeFile();
        return -1;
    }

    uchar src[3] = {48, 174, 134};
    uchar dst[4];

    cmsDoTransform(transform, src, dst, 1);

    fprintf(stdout, "C: %.1f%%  M: %.1f%%  Y: %.1f%%  K: %.1f%%\n",
            dst[0] * 100.0 / 255.0,
            dst[1] * 100.0 / 255.0,
            dst[2] * 100.0 / 255.0,
            dst[3] * 100.0 / 255.0);

    cmsDeleteTransform(transform);
    closeFile();

    return 0;
}

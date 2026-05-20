#ifndef VERSION_H
#define VERSION_H

#define ATHC_VERSION 20260520

#define ATHC_MAJOR_VERSION 1
#define ATHC_MINOR_VERSION 0
#define ATHC_MICRO_VERSION 0

#define ATHC_VERSION_STR_MAJ_MIN_MIC "1.0.0"

#define ATHC_AT_LEAST(major, minor, micro) \
    (ATHC_MAJOR_VERSION > (major) || \
     (ATHC_MAJOR_VERSION == (major) && ATHC_MINOR_VERSION > (minor)) || \
     (ATHC_MAJOR_VERSION == (major) && ATHC_MINOR_VERSION == (minor) && \
      ATHC_MICRO_VERSION >= (micro)))

#endif // VERSION_H

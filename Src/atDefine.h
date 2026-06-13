#ifndef ATDEFINE_H
#define ATDEFINE_H

#if NDEBUG // Release mode
#    define AT_DEBUG_FILE 0

#else
#    define AT_DEBUG_FILE 1

#endif

#include "atDebug.h"
#include "atVersion.h"
#include "atPresets.h"

#endif // ATDEFINE_H
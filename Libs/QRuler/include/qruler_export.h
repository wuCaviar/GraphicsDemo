#ifndef QRULER_EXPORT_H
#define QRULER_EXPORT_H

#include <QtCore/QtGlobal>

#ifdef QRULER_STATICLIB
#    define QRULER_EXPORT
#else
#    ifdef QRULER_LIBRARY
#        define QRULER_EXPORT Q_DECL_EXPORT
#    else
#        define QRULER_EXPORT Q_DECL_IMPORT
#    endif
#endif

#endif // QRULER_EXPORT_H

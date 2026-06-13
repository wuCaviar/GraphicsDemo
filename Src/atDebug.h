/*
 * A per-file debug stream based on qDebug()
 * Extracted from Scribus scdebug.h
 *
 * Define AT_DEBUG_FILE to zero (debugging disabled) or non-zero
 * (debugging enabled) before including this file. Not defining it
 * means enabling debug support unless QT_NO_DEBUG_OUTPUT is defined.
 */

#ifndef ATDEBUG_H
#define ATDEBUG_H

#if defined(QT_NO_DEBUG_OUTPUT) || (defined(AT_DEBUG_FILE) && AT_DEBUG_FILE == 0)

class AtNoDebug
{
public:
    inline AtNoDebug() { }
    inline ~AtNoDebug() { }
};

template<typename T>
inline AtNoDebug operator<<(AtNoDebug debug, const T &)
{
    return debug;
}

inline AtNoDebug atDebug()
{
    return AtNoDebug();
}

#else

#    include <QDebug>
#    include <QTime>
inline QDebug atDebug()
{
    return QDebug(QtDebugMsg) << QTime::currentTime().toString("[hh:mm:ss.zzz]");
}

#endif

#endif // ATDEBUG_H

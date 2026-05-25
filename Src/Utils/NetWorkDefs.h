#ifndef NETWORKDEFS_H
#define NETWORKDEFS_H

#include <QtGlobal>
#include <QString>
#include <QCoreApplication>

#define USE_NEW_RIP

#if defined(Q_OS_MACOS)
#    define RIP_PROGRESS_MAX 100
#elif defined(Q_OS_WIN)
#    if defined(USE_NEW_RIP)
#        define RIP_PROGRESS_MAX 10000
#    else
#        define RIP_PROGRESS_MAX 100
#    endif
#endif

#define NETWORK_ROOT "http://127.0.0.1:9201"

#define NETWORK_ROOT_HELPABOUT NETWORK_ROOT "/helpabout"
#define NETWORK_ROOT_ADDRIP NETWORK_ROOT "/addrip"
#define NETWORK_ROOT_RIPSTATUS NETWORK_ROOT "/ripstatus"
#define NETWORK_ROOT_RIPVERSION NETWORK_ROOT "/ripVersion"

enum NetworkRequestType
{
    RequestHelpAbout,
    RequestAddRip,
    RequestRipStatus,
    RequestRipVersion
};

#endif // NETWORKDEFS_H

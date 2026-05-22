#ifndef NETWORKDEFS_H
#define NETWORKDEFS_H

#include <QtGlobal>
#include <QString>

#define USE_NEW_RIP

#if defined(Q_OS_MACOS)
static const QString RIP_EXE_PATH = QStringLiteral("");

static const QString ConfigPath =
    QStringLiteral("/Volumes/Caviar/Test/GraphicsDemo/Bin/ripconfig.xml");

#define RIP_PROGRESS_MAX 100

#elif defined(Q_OS_WIN)

#    if defined(USE_NEW_RIP)
static const QString RIP_EXE_PATH =
    QStringLiteral("D:/WorkSpace/Caviar/FileRip/FileRIP0521.exe");
#        define RIP_PROGRESS_MAX 10000
#    else
static const QString RIP_EXE_PATH =
    QStringLiteral("D:/WorkSpace/Caviar/FileRip/FileRIP.exe");
#        define RIP_PROGRESS_MAX 100
#    endif

static const QString ConfigPath =
    QStringLiteral("D:/WorkSpace/Caviar/FileRip/ripconfig.xml");

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

#ifndef NETWORKDEFS_H
#define NETWORKDEFS_H

#include <QtGlobal>
#include <QString>

#if defined(Q_OS_MACOS)
static const QString ExePath = QStringLiteral("");

static const QString ConfigPath =
    QStringLiteral("/Volumes/Caviar/Test/GraphicsDemo/Bin/ripconfig.xml");
#elif defined(Q_OS_WIN)
static const QString ExePath =
    QStringLiteral("D:/WorkSpace/Caviar/FileRip/FileRIP.exe");

static const QString ConfigPath =
    QStringLiteral("D:/WorkSpace/Caviar/FileRip/ripconfig.xml");

#endif

#define NETWORK_ROOT                        "http://127.0.0.1:9201"

#define NETWORK_ROOT_HELPABOUT              NETWORK_ROOT "/helpabout"
#define NETWORK_ROOT_ADDRIP                 NETWORK_ROOT "/addrip"
#define NETWORK_ROOT_RIPSTATUS              NETWORK_ROOT "/ripstatus"
#define NETWORK_ROOT_RIPVERSION             NETWORK_ROOT "/ripVersion"

#endif // NETWORKDEFS_H

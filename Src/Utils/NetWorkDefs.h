#ifndef NETWORKDEFS_H
#define NETWORKDEFS_H

#include <QtGlobal>
#include <QString>
#include <QCoreApplication>

#define RIP_PROGRESS_MAX 10000 // 进度值上限，表示 100%，允许更细粒度的进度更新

#define NETWORK_ROOT "http://127.0.0.1:9201"

#define NETWORK_ROOT_HELPABOUT NETWORK_ROOT "/helpabout"
#define NETWORK_ROOT_ADDRIP NETWORK_ROOT "/addrip"
#define NETWORK_ROOT_RIPSTATUS NETWORK_ROOT "/ripstatus"
#define NETWORK_ROOT_RIPVERSION NETWORK_ROOT "/ripVersion"
#define NETWORK_ROOT_CANCELRIP NETWORK_ROOT "/cancelRip"
#define NETWORK_ROOT_EXIT NETWORK_ROOT "/exit"

enum NetworkRequestType
{
    RequestHelpAbout,
    RequestAddRip,
    RequestRipStatus,
    RequestRipVersion,
    RequestCancelRip,
    RequestExit
};

#endif // NETWORKDEFS_H

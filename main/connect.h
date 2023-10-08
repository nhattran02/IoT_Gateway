#ifndef _CONNECT_H_
#define _CONNECT_H_

#include "connect_wifi.h"

typedef struct connect
{
    bool isWifiConnected;
    bool isWifiOn;
    bool isLTEConnected;
    bool isLTEOn;
    bool isBLEConnected;
    bool isBLEOn;
}connect_status_t;

extern connect_status_t connectStatus;


#endif // _CONNECT_H_
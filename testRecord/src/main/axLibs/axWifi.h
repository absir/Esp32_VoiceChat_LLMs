#ifndef AX_WIFI_H
#define AX_WIFI_H

#include <WiFi.h>
#include "axBle.h"

extern bool axWifiConnected;

// wifi连接
void axWifiConn(const char *ssid, const char *passwd);
void axWifiInit();
void axWifiLoop();
// 连接保持
bool axWifiConnKeep(long wait);

#endif
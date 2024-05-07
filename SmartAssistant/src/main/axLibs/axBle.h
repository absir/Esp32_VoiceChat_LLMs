#ifndef AX_BLE_H
#define AX_BLE_H
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <Preferences.h>

Preferences axPreferences;

bool axBleConnected = false;
BLEServer *axBleServer;
BLEService *axBleService;
BLECharacteristic *axBleCharacteristic;

typedef void (*axBleOnCmd)(size_t lc, uint8_t *data);

typedef enum
{
    axBleCmdWifi,

    axBleCmdCount,

} axBleCmd;

// 蓝牙命令回调
void axBleReg(axBleCmd cmd, axBleOnCmd *onCmd);
// 蓝牙初始化
void axBleInit(bool allowDiscover);
// 蓝牙步进循环
void axBleLoop();
// 蓝牙发送
void axBleSend(axBleCmd cmd, size_t lc, uint8_t *data);

#endif
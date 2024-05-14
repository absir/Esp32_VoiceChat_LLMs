#ifndef AX_BLE_H
#define AX_BLE_H
#include "_axDefine.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <Preferences.h>

static Preferences axPreferences;

static bool axBleConnected = false;
static BLEServer *axBleServer;
static BLEService *axBleService;
static BLECharacteristic *axBleCharacteristic;

typedef void (*axBleOnCmd)(size_t lc, uint8_t *data);

// 蓝牙命令回调
void axBleReg(axBleCmd cmd, axBleOnCmd *onCmd);
// 蓝牙发送
void axBleSend(axBleCmd cmd, size_t lc, uint8_t *data);
// 蓝牙初始化
void axBleInit(bool allowDiscover);
// 蓝牙步进循环
void axBleLoop();

#endif
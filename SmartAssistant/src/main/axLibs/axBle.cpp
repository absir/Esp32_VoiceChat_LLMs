#include "axBle.h"

#define DEVICE_NAME "YYChat"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define PRE_KEY_BLE_CONNED "ble.conned"

// BLE_CMD_PRE 前缀特征码
const uint8_t BLE_CMD_PRE = 0x7f;

int _init = -1;
bool _inited = false;

class axServerCallbacks : public BLEServerCallbacks
{
	void onConnect(BLEServer *pServer)
	{
		if (pServer != axBleServer)
		{
			return;
		}

		axBleConnected = true;
		if (_init == 0)
		{
			_init = 1;
			axPreferences.putBool(PRE_KEY_BLE_CONNED, true);
		}
	}

	void onDisconnect(BLEServer *pServer)
	{
		axBleConnected = false;
	}
};

typedef struct axCharacteristicCallbacksBuff
{
	// 考虑双指令缓存
	uint8_t data[1024];
	size_t dataI;
};

axBleOnCmd **axBleOnCmds = nullptr;

struct axCharacteristicCallbacksBuff *axCbuff = nullptr;

class axCharacteristicCallbacks : public BLECharacteristicCallbacks
{
public:
	axCharacteristicCallbacks()
	{
		if (!axCbuff)
		{
			axCbuff = new axCharacteristicCallbacksBuff();
		}

		axCbuff->dataI = 0;
	}

	void onWrite(BLECharacteristic *pCharacteristic)
	{
		if (pCharacteristic != axBleCharacteristic)
		{
			return;
		}

		size_t dataLen = pCharacteristic->getLength();
		memcpy(axCbuff->data + axCbuff->dataI, pCharacteristic->getData(), dataLen);
		axCbuff->dataI += dataLen;

		while (axCbuff->dataI >= 4)
		{
			if (axCbuff->data[0] != BLE_CMD_PRE)
			{
				// 过滤非指令特征
				axCbuff->dataI--;
				memcpy(axCbuff->data, axCbuff->data + 1, axCbuff->dataI);
				continue;
			}

			size_t lc = axCbuff->data[2] * 128 + axCbuff->data[3];
			size_t allLc = 4 + lc;
			if (axCbuff->dataI < allLc)
			{
				// 数据长度不足
				break;
			}

			// 分发指令
			onCmd(axCbuff->data[1], lc, axCbuff->data + 4);

			// 一条指令读取完成
			axCbuff->dataI -= allLc;
			memcpy(axCbuff->data, axCbuff->data + allLc, axCbuff->dataI);
		}
	}

	void onCmd(uint8_t cmd, size_t lc, uint8_t *data)
	{
		axBleOnCmd *onCmd = axBleOnCmds && cmd >= 0 && cmd < axBleCmdCount ? axBleOnCmds[cmd] : nullptr;
		if (!onCmd)
		{
			Serial.println("onCmd unReg " + String(cmd));
			return;
		}

		(*onCmd)(lc, data);
	}
};

void axBleReg(axBleCmd cmd, axBleOnCmd *onCmd)
{
	if (!axBleOnCmds)
	{
		axBleOnCmds = new axBleOnCmd *[axBleCmdCount];
	}

	axBleOnCmds[cmd] = onCmd;
}

void axBleInit(bool allowDiscover)
{
	if (_init < 0)
	{
		_init = axPreferences.getBool(PRE_KEY_BLE_CONNED) ? 1 : 0;
	}

	if (!allowDiscover && _init <= 0)
	{
		allowDiscover = true;
	}

	if (_inited)
	{
		BLEDevice::deinit(true);
	}

	axBleConnected = false;
	BLEDevice::init(DEVICE_NAME);
	axBleServer = BLEDevice::createServer();
	axBleServer->setCallbacks(new axServerCallbacks());
	axBleService = axBleServer->createService(SERVICE_UUID);
	uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE;
	if (allowDiscover)
	{
		properties |= BLECharacteristic::PROPERTY_BROADCAST;
	}

	axBleCharacteristic = axBleService->createCharacteristic(CHARACTERISTIC_UUID, properties);
	axBleCharacteristic->setCallbacks(new axCharacteristicCallbacks());

	axBleService->start();

	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	pAdvertising->addServiceUUID(SERVICE_UUID);
	pAdvertising->setScanResponse(allowDiscover);
	pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
	pAdvertising->setMinPreferred(0x12);
	BLEDevice::startAdvertising();

	Serial.println("axBleInit complete " + String(allowDiscover));
}

void axBleLoop()
{
}

void axBleSend(axBleCmd cmd, size_t lc, uint8_t *data)
{
	size_t allLc = 4 + lc;
	uint8_t *data = new uint8_t(4 + lc);
	data[0] = BLE_CMD_PRE;
	data[1] = cmd;
	data[2] = lc / 128;
	data[3] = lc % 128;
	if (lc > 0)
	{
		memcpy(data + 4, data, lc);
	}

	axBleCharacteristic->setValue(data, allLc);
	axBleCharacteristic->notify();
}

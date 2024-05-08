#include "axBle.h"

#define PRE_KEY_BLE_CONNED "ble.conned"

int _axBleInit = -1;
bool _axBleInited = false;

class axServerCallbacks : public BLEServerCallbacks
{
	void onConnect(BLEServer *pServer)
	{
		if (pServer != axBleServer)
		{
			return;
		}

		axBleConnected = true;
		if (_axBleInit == 0)
		{
			_axBleInit = 1;
			axPreferences.putBool(PRE_KEY_BLE_CONNED, true);
		}
	}

	void onDisconnect(BLEServer *pServer)
	{
		axBleConnected = false;
	}
};

struct axCharacteristicCallbacksBuff
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
			if (axCbuff->data[0] != AX_BLE_CMD_PRE)
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

void axBleSend(axBleCmd cmd, size_t lc, uint8_t *data)
{
	size_t allLc = 4 + lc;
	uint8_t *allData = new uint8_t(4 + lc);
	allData[0] = AX_BLE_CMD_PRE;
	allData[1] = cmd;
	allData[2] = lc / 128;
	allData[3] = lc % 128;
	if (lc > 0)
	{
		memcpy(allData + 4, data, lc);
	}

	axBleCharacteristic->setValue(allData, allLc);
	axBleCharacteristic->notify();
}

void axBleInit(bool allowDiscover)
{
	if (_axBleInit < 0)
	{
		_axBleInit = axPreferences.getBool(PRE_KEY_BLE_CONNED) ? 1 : 0;
	}

	if (!allowDiscover && _axBleInit <= 0)
	{
		allowDiscover = true;
	}

	if (_axBleInited)
	{
		BLEDevice::deinit(true);
	}

	axBleConnected = false;
	BLEDevice::init(AX_BLE_DEVICE_NAME);
	axBleServer = BLEDevice::createServer();
	axBleServer->setCallbacks(new axServerCallbacks());
	axBleService = axBleServer->createService(AX_BLE_SERVICE_UUID);
	uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE;
	if (allowDiscover)
	{
		properties |= BLECharacteristic::PROPERTY_BROADCAST;
	}

	axBleCharacteristic = axBleService->createCharacteristic(AX_BLE_CHARACTERISTIC_UUID, properties);
	axBleCharacteristic->setCallbacks(new axCharacteristicCallbacks());

	axBleService->start();

	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	pAdvertising->addServiceUUID(AX_BLE_SERVICE_UUID);
	pAdvertising->setScanResponse(allowDiscover);
	pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
	pAdvertising->setMinPreferred(0x12);
	BLEDevice::startAdvertising();

	Serial.println("axBleInit complete " + String(allowDiscover));
}

void axBleLoop()
{
}

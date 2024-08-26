#include "axBle.h"
#include <BLE2902.h>

#define PRE_KEY_BLE_CONNED "ble.conned"

Preferences axPreferences;
bool axBleConnected = false;
BLEServer *axBleServer;
BLEService *axBleService;
BLECharacteristic *axBleCharacteristic;
BLECharacteristic *axBleCharacteristicRec;
// BLEAdvertising *pAdvertising;

int _axBleInit = -1;
bool _axBleInited = false;
// 代码互斥
SemaphoreHandle_t _axBleMutex = nullptr;

struct axCharacteristicCallbacksBuff
{
	// 考虑双指令缓存
	uint8_t data[2048];
	size_t dataI;
	uint8_t sendBuff[4];
};

struct axCharacteristicCallbacksBuff *axCbuff = nullptr;

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
			axPreferences.begin(AX_PRE_NAMESPACE);
			axPreferences.putBool(PRE_KEY_BLE_CONNED, true);
			axPreferences.end();
		}

		if (axCbuff)
		{
			axCbuff->dataI = 0;
		}
	}

	void onDisconnect(BLEServer *pServer)
	{
		axBleConnected = false;
		// 重新可见
		BLEDevice::startAdvertising();
	}
};

axBleOnCmd *axBleOnCmds = nullptr;

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

	void onRead(BLECharacteristic *pCharacteristic)
	{
		// onRecieve(pCharacteristic);
	}

	void onWrite(BLECharacteristic *pCharacteristic)
	{
		onRecieve(pCharacteristic);
	}

	void onNotify(BLECharacteristic *pCharacteristic)
	{
		// onRecieve(pCharacteristic);
	}

	void onRecieve(BLECharacteristic *pCharacteristic)
	{
		// Serial.println("ble onRecieve " + String(pCharacteristic->getUUID().toString().c_str()));
		if (pCharacteristic != axBleCharacteristicRec)
		{
			return;
		}

		size_t dataLen = pCharacteristic->getLength();
		// Serial.println("ble onRecieve dataLen = " + String(dataLen));
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
		// Serial.println("onCmd: " + String(cmd) + ", " + String(lc));
		axBleOnCmd onCmd = axBleOnCmds && cmd >= 0 && cmd < axBleCmdCount ? axBleOnCmds[cmd] : nullptr;
		if (!onCmd)
		{
			Serial.println("onCmd unReg " + String(cmd));
			return;
		}

		onCmd(lc, data);
	}
};

void axBleReg(axBleCmd cmd, axBleOnCmd onCmd)
{
	if (!axBleOnCmds)
	{
		axBleOnCmds = new axBleOnCmd[axBleCmdCount];
	}

	axBleOnCmds[cmd] = onCmd;
}

void axBleSend(axBleCmd cmd, const char *data)
{
	// Serial.println("axBleSend const char *" + String(cmd));
	axBleSend(cmd, data == nullptr ? 0 : strlen(data), (uint8_t *)data);
}

void axBleSendWaitDone()
{
	delay(80);
}

void axBleSend(axBleCmd cmd, size_t lc, uint8_t *data)
{
	if (axBleCharacteristic == nullptr)
	{
		return;
	}

	xSemaphoreTake(_axBleMutex, portMAX_DELAY);
	Serial.println("axBleSend: " + String(cmd) + ", " + String(lc));
	if (!axCbuff)
	{
		axCbuff = new axCharacteristicCallbacksBuff();
	}

	uint8_t *sendBuff = axCbuff->sendBuff;
	sendBuff[0] = AX_BLE_CMD_PRE;
	sendBuff[1] = cmd;
	sendBuff[2] = lc / 128;
	sendBuff[3] = lc % 128;
	axBleCharacteristic->setValue(sendBuff, 4);
	axBleCharacteristic->notify();
	axBleSendWaitDone();

	int i = 0;
	while (i < lc)
	{
		int max = i + 20;
		bool end = max >= lc;
		axBleCharacteristic->setValue(data + i, (end ? lc : max) - i);
		axBleCharacteristic->notify();
		axBleSendWaitDone();
		if (end)
		{
			break;
		}

		i = max;
	}

	xSemaphoreGive(_axBleMutex);
}

void axBleInit(bool allowDiscover)
{
	_axBleMutex = xSemaphoreCreateMutex();

	if (_axBleInit < 0)
	{
		axPreferences.begin(AX_PRE_NAMESPACE);
		_axBleInit = axPreferences.getBool(PRE_KEY_BLE_CONNED) ? 1 : 0;
		axPreferences.end();
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
	// BLECharacteristic::PROPERTY_READ |
	// BLECharacteristic::PROPERTY_WRITE |
	// | BLECharacteristic::PROPERTY_INDICATE

	// notify Characteristic
	uint32_t properties = BLECharacteristic::PROPERTY_NOTIFY;
	axBleCharacteristic = axBleService->createCharacteristic(AX_BLE_CHARACTERISTIC_UUID_NOTIFY, properties);
	axBleCharacteristic->addDescriptor(new BLE2902());

	// write Characteristic
	properties = BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR;
	if (allowDiscover)
	{
		properties |= BLECharacteristic::PROPERTY_BROADCAST;
	}

	axBleCharacteristicRec = axBleService->createCharacteristic(AX_BLE_CHARACTERISTIC_UUID, properties);
	axBleCharacteristicRec->addDescriptor(new BLE2902());
	axBleCharacteristicRec->setCallbacks(new axCharacteristicCallbacks());

	axBleService->start();

	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	pAdvertising->addServiceUUID(AX_BLE_SERVICE_UUID);
	pAdvertising->setScanResponse(allowDiscover);
	pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
	pAdvertising->setMinPreferred(0x12);
	BLEDevice::startAdvertising();

	// 发送资源提前占用堆栈
	axBleSend(axBleCmdCount, nullptr);
	Serial.println("axBleInit complete " + String(allowDiscover));
}

void axBleLoop()
{
}

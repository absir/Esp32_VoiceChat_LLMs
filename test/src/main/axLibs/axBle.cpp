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

#define axBleBuffLen 2048

struct axCharacteristicCallbacksBuff
{
	// 考虑双指令缓存
	uint8_t data[axBleBuffLen];
	size_t dataI;
	uint8_t sendBuff[20];
};

struct axCharacteristicCallbacksBuff *axCbuff = nullptr;
axBleOnCmd *axBleOnCmds = nullptr;

void axBleOnCmdCall(uint8_t cmd, size_t lc, uint8_t *data)
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

bool axBleReadCmdNext(bool onCmd)
{
	if (axCbuff->dataI < 4)
	{
		return false;
	}

	if (axCbuff->data[0] != AX_BLE_CMD_PRE)
	{
		// 过滤非指令特征
		axCbuff->dataI--;
		memmove(axCbuff->data, axCbuff->data + 1, axCbuff->dataI);
		return true;
	}

	size_t lc = axCbuff->data[2] * 128 + axCbuff->data[3];
	size_t allLc = 4 + lc;
	if (axCbuff->dataI < allLc)
	{
		// 数据长度不足
		return false;
	}

	if (onCmd)
	{
		// 分发指令
		axBleOnCmdCall(axCbuff->data[1], lc, axCbuff->data + 4);
	}

	// Serial.println("axBleReadCmdNext " + String(axCbuff->dataI) + " < " + String(axCbuff->data[1]) + ", " + String(lc));
	// 一条指令读取完成
	axCbuff->dataI -= allLc;
	memmove(axCbuff->data, axCbuff->data + allLc, axCbuff->dataI);
	return true;
}

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
		if (dataLen <= 0)
		{
			return;
		}

		// 数据安全锁
		xSemaphoreTake(_axBleMutex, portMAX_DELAY);
		while ((axCbuff->dataI + dataLen) > axBleBuffLen)
		{
			if (!axBleReadCmdNext(false))
			{
				// 超出长度, 清空缓冲区
				axCbuff->dataI = 0;
				break;
			}
		}

		uint8_t *data = pCharacteristic->getData();
		if (axCbuff->dataI > 0 || data[0] == AX_BLE_CMD_PRE)
		{
			// 数据添加到缓冲区
			memmove(axCbuff->data + axCbuff->dataI, data, dataLen);
			axCbuff->dataI += dataLen;
		}

		// Serial.println("axBle onRecieve " + String(dataLen) + ", " + String(axCbuff->dataI) + " < " + String(data[0]));
		xSemaphoreGive(_axBleMutex);
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

	// // Serial.println("axBleSend notify 0, " + String(lc));
	// uint32_t core_id = xPortGetCoreID(); // 获取当前任务所在的CPU核心编号
	// // 获取当前线程的初始堆栈大小
	// uint32_t remaining_stack = uxTaskGetStackHighWaterMark(NULL); // 获取任务堆栈的剩余大小
	// printf("Current CPU Core: %u\n", core_id);
	// printf("Remaining Stack Size: %u bytes\n", remaining_stack);

	Serial.println("axBleSend notify 0, " + String(cmd));
	axBleCharacteristic->setValue(sendBuff, 4);
	Serial.println("axBleSend notify 1, " + String(cmd));
	axBleCharacteristic->notify();
	axBleSendWaitDone();

	int i = 0;
	while (i < lc)
	{
		int max = i + 20;
		bool end = max >= lc;
		size_t len = (end ? lc : max) - i;
		Serial.println("axBleSend notify 2, " + String(cmd) + ", " + String(len));
		axBleCharacteristic->setValue(data + i, len);
		// memcpy(sendBuff, data + i, len);
		// Serial.println("axBleSend notify 3, " + String(cmd));
		// axBleCharacteristic->setValue(sendBuff, len);
		Serial.println("axBleSend notify 4, " + String(cmd));
		axBleCharacteristic->notify();
		axBleSendWaitDone();
		if (end)
		{
			break;
		}

		i = max;
	}
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
	// 蓝牙默认配置
	// esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	// bt_cfg.controller_task_stack_size = 20480; // 设置 BTC_TASK 任务的栈空间大小
	// esp_bt_controller_init(&bt_cfg);
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
	if (!axCbuff)
	{
		return;
	}

	// 数据安全锁
	xSemaphoreTake(_axBleMutex, portMAX_DELAY);
	// 执行指令
	while (axBleReadCmdNext(true))
	{
	}
	xSemaphoreGive(_axBleMutex);
}

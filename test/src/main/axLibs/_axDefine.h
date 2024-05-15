#ifndef _AX_DEFINE
#define _AX_DEFINE

#define AX_BLE_DEVICE_NAME "YYChat"
#define AX_BLE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define AX_BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// AX_BLE_CMD_PRE 前缀特征码
#define AX_BLE_CMD_PRE 0x7f
// axBleCmd指令
typedef enum
{
    axBleCmdWifi,

    axBleCmdCount,

} axBleCmd;

#define AX_WIFI_CHECK_INTERVAL 2000
#define AX_WIFI_RECONN_INTERVAL 30000
#define AX_WIFI_CONN_KEEP_WAIT 1000

#define AX_MIC_SAMPLE_RATE (8000)
#define AX_MIC_PIN_I2S_BCLK 14
#define AX_MIC_PIN_I2S_LRC 15
#define AX_MIC_PIN_I2S_DIN 32
#define AX_MIC_CONTINUE_NOISE 50

// DIN
#define AX_I2S_DOUT 27
// BCLK
#define AX_I2S_BCLK 26
// LRC
#define AX_I2S_LRC 25

#define AX_VOL_DEFAULT 10
#define AX_VOL_MAX_STEPS 15
#define AX_VOL_TIMEOUT_MS 500
#define AX_VOL_TIMEOUT_SSL_MS 2700

#endif
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

// Audio PIN
#define AX_AUDIO_DOUT 27
#define AX_AUDIO_BCLK 26
#define AX_AUDIO_LRC 25

#define AX_VOL_DEFAULT 50
#define AX_VOL_MAX_STEPS 15
#define AX_VOL_TIMEOUT_MS 500
#define AX_VOL_TIMEOUT_SSL_MS 2700

// Mic PIN
#define AX_MIC_SAMPLE_RATE 8000
#define AX_MIC_DOUT_SD_IN 23
#define AX_MIC_LRCL_WS 22
#define AX_MIC_BCLK_SCK 21
#define AX_MIC_CONTINUE_RMS_MIN 50
// mic功放
#define AX_MIC_AMPLIFY_FACTOR 10

#endif
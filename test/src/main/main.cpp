#include "./axLibs/axBle.h"
#include "./axLibs/axWifi.h"
#include "./axLibs/axAudio.h"
#include "./axLibs/axMic.h"
#include "./axLibs/axClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// 定义配置
// 信号灯
#define ledPin 2
#define loopDelayDefault 10
#define lowPin 18
#define micPin 19
#define resetPin 5

// 状态
enum MainStatus
{
    MainStatusSetUp,
    MainStatusConning,
    MainStatusConned,
    MainStatusIdle,
    MainStatusPlaying,
    MainStatusMicing,
};

uint32_t loopDelay = 0;
MainStatus status;
bool netConned = false;
bool audioPlayed = false;

// 麦克风
AxMic axMic(AX_MIC_SAMPLE_RATE, AX_MIC_BCLK_SCK, AX_MIC_LRCL_WS, AX_MIC_DOUT_SD_IN);
#define axMicBuffLen 512000
#define axMicBuffStep 512
#define axMicSilenceMax 6
char axMicBuff[axMicBuffStep + 16];
int micState = 0;
int axMicConLen = 0;
int axMicSilence = 0;

// http请求
AxClient axHttp;
// http://192.168.36.10:8787/S/spoken/1
//
#define axSpokenUrl "http://192.168.36.10:8787/S/spoken/4"
// json解析
DynamicJsonDocument jsonDoc(256);

void play(const char *path)
{
    if (!axAudio->connecttoFS(SPIFFS, path))
    {
        Serial.println("play fail, " + String(path));
    }
}

void setup()
{
    // 初始化 SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS initFail");
    }

    // 信号引脚
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    pinMode(lowPin, OUTPUT);
    digitalWrite(lowPin, LOW);
    pinMode(micPin, INPUT_PULLUP);
    pinMode(resetPin, INPUT_PULLUP);

    // 模块初始化
    // axBleInit(false);

    axWifiInit();
    axWifiConn("yuanjiuyan", "88889999");

    axAudioInit();
}

void micStart()
{
    Serial.println("micStart");
    micState = 1;
    axMicConLen = 0;
    axMicSilence = 0;
    // 高亮
    digitalWrite(ledPin, HIGH);
    // 停止播放
    if (axAudio->isRunning())
    {
        axAudio->stopSong();
    }
    // 录音清理
    axMic.clear();
    axMic.read(axMicBuff, axMicBuffStep);
}

void micEnd(bool cancel)
{
    Serial.println("micEnd, " + String(cancel) + ", " + String(axMicConLen));
    micState = 4;
    // 结束
    digitalWrite(ledPin, LOW);
    if (axMicConLen != 0)
    {
        // 有录音数据|请求
        if (cancel || axMicConLen < 1024)
        {
            axHttp.end();
        }
        else
        {
            axHttp.setTimeout(30000);
            int repErr = axHttp.chunkedRespone();
            if (repErr != 0 && repErr != 200)
            {
                Serial.println("micEnd chunkedRespone err " + String(repErr));
                play("/netFail.mp3");
            }
            else
            {
                String response = axHttp.getString();
                Serial.println("micEnd Response: " + response);
                deserializeJson(jsonDoc, response);
                const char *tUrl = jsonDoc["tUrl"];
                if (tUrl)
                {
                    axAudio->connecttohost(tUrl);
                }
                else
                {
                    play("/netFail.mp3");
                }
            }

            axHttp.end();
        }
    }
}

void loop()
{
    if (digitalRead(resetPin) == LOW)
    {
        // 重置
        axPreferences.clear();
        ESP.restart();
        return;
    }

    // loop状态
    if (loopDelay > 0)
        delay(loopDelay);
    loopDelay = loopDelayDefault;

    // WIFI
    axWifiLoop();
    if (!axWifiConnected)
    {
        status = status == MainStatusSetUp ? MainStatusConning : MainStatusSetUp;
        digitalWrite(ledPin, status == MainStatusSetUp ? HIGH : LOW);
        if (netConned)
        {
            netConned = false;
            play("/netFail.mp3");
        }

        return;
    }
    else if (!netConned)
    {
        netConned = true;
        play("/netOk.mp3");
    }

    // MIC
    bool micOpen = digitalRead(micPin) == LOW;
    if ((micOpen && micState < 3) || micState == 1)
    {
        if (micState == 0)
        {
            micStart();
            // 一直按着录音
            micState = 2;
        }

        Serial.println("recordContiue, " + String(axMicConLen) + ", " + String(axMicSilence));
        loopDelay = 0;
        bool axMicConFirst = axMicConLen == 0;
        int recordLen = axMic.recordContiue(&axMicConLen, false, axMicBuff, axMicBuffLen, axMicBuffStep, micState == 2 ? nullptr : &axMicSilence, axMicSilenceMax, AX_MIC_CONTINUE_RMS_MIN);
        if (recordLen <= 0)
        {
            micState = 3;
        }
        else
        {
            if (axMicConFirst)
            {
                axHttp.begin(axSpokenUrl);
                // 设置连接超时时间为10秒
                axHttp.setConnectTimeout(10000);
                axHttp.addHeader("Content-Type", "application/octet-stream");
                axHttp.chunkedConn("POST");
            }

            // Serial.println("calculateRMS =" + String(AxMic::calculateRMS(axMicBuff, recordLen)) + ", " + axMicSilence + ", " + axMicConLen);
            // 发送HTTP POST请求并上传数据
            int sendErr = axHttp.chunkedSend(axMicBuff, recordLen);
            if (sendErr != 0)
            {
                Serial.println("axMic chunkedSend err " + String(sendErr) + ", " + String(recordLen));
                axMicConLen = -1;
                micState = 3;
                play("/netFail.mp3");
            }
        }
    }
    else if (micState == 4)
    {
        if (!micOpen)
        {
            // 录音状态回归
            micState = 0;
        }
    }
    else if (micState >= 2)
    {
        micEnd(false);
    }

    // 播放
    axAudio->loop();
    if (!audioPlayed)
    {
        audioPlayed = true;
        // axAudio->connecttohost("https://p2.dev.yiyiny.com/a/tts.mp3");
        play("/boot.mp3");
    }

    if (axAudio->isRunning())
    {
        if (status != MainStatusPlaying)
        {
            status = MainStatusPlaying;
            digitalWrite(ledPin, HIGH);
        }

        loopDelay = 0;
        return;
    }

    // 空闲
    if (status != MainStatusIdle)
    {
        status = MainStatusIdle;
        digitalWrite(ledPin, LOW);
    }
}
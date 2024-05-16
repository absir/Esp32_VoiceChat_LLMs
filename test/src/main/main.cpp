#include "./axLibs/axBle.h"
#include "./axLibs/axWifi.h"
#include "./axLibs/axAudio.h"
#include "./axLibs/axMic.h"
#include "./axLibs/axClient.h"
#include <HTTPClient.h>

// 定义配置
// 信号灯
#define ledPin 2
#define loopDelayDefault 10
#define lowPin 23
#define micPin 22

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
int micState = 0;
int axMicConLen = 0;
int axMicSilence = 0;

bool audioPlayed = false;

// 麦克风
AxMic axMic(AX_MIC_SAMPLE_RATE, AX_MIC_BCK, AX_MIC_WS, AX_MIC_SD);
#define axMicBuffLen 512000
#define axMicBuffStep 5120
char axMicBuff[axMicBuffStep];

// http请求
AxClient axHttp;
#define axSpokenUrl "http://vHuman.dev.yiyiny.com/S/spoken/1"

void setup()
{
    // 信号引脚
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    pinMode(lowPin, OUTPUT);
    digitalWrite(lowPin, LOW);
    pinMode(micPin, INPUT_PULLUP);

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
    axMicSilence = 6;
    // 高亮
    digitalWrite(ledPin, HIGH);
    // 停止播放
    if (axAudio->isRunning())
    {
        axAudio->stopSong();
    }
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
        if (cancel || axMicConLen < 0)
        {
            axHttp.end();
        }
        else
        {
            int repErr = axHttp.chunkedRespone();
            if (repErr != 0)
            {
                Serial.println("axMic chunkedRespone err " + String(repErr));
            }
            else
            {
                String response = axHttp.getString();
                Serial.println("HTTP Response: " + response);
            }

            axHttp.end();
        }
    }
}

void loop()
{
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
        return;
    }

    // MIC
    bool micOpen = digitalRead(micPin) == LOW;
    if ((micOpen && micState < 3) || micState == 1)
    {
        if (micState == 0)
        {
            micStart();
            // 一直按着录音
            // micState = 2;
        }

        Serial.println("recordContiue, " + String(axMicConLen) + ", " + String(axMicSilence));
        loopDelay = 0;
        bool axMicConFirst = axMicConLen == 0;
        int recordLen = axMic.recordContiue(&axMicConLen, false, axMicBuff, axMicBuffLen, axMicBuffStep, true, &axMicSilence, AX_MIC_CONTINUE_NOISE);
        if (recordLen <= 0)
        {
            micState = 3;
        }
        else
        {
            if (axMicConFirst)
            {
                axHttp.begin(axSpokenUrl);
                axHttp.addHeader("Content-Type", "application/octet-stream");
                // axHttp.handleHeaderResponse();
                axHttp.chunkedConn("POST");
            }

            // 发送HTTP POST请求并上传数据
            int sendErr = axHttp.chunkedSend(axMicBuff, recordLen);
            if (sendErr != 0)
            {
                Serial.println("axMic chunkedSend err " + String(sendErr) + ", " + String(recordLen));
                axMicConLen = -1;
                micState = 3;
            }
        }

        return;
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
        axAudio->connecttohost("https://p2.dev.yiyiny.com/a/tts.mp3");
    }

    if (axAudio->isRunning())
    {
        if (status != MainStatusPlaying)
        {
            status = MainStatusPlaying;
            digitalWrite(ledPin, HIGH);
        }

        return;
    }

    // 空闲
    if (status != MainStatusIdle)
    {
        status = MainStatusIdle;
        digitalWrite(ledPin, LOW);
    }
}
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
#define lowPin 4
#define micPin 5
#define resetPin 6

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
int netConnSeq = 0;
bool loopBooted = false;

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
// json解析
DynamicJsonDocument jsonDoc(512);

DynamicJsonDocument jsonDocPlayList(2048);
JsonArray *playList = nullptr;
int playIndex = 0;
int playIndexed = -1;

// 参数
#define API "http://192.168.36.10:8787/S/spoken/4"
String api = axPreferences.getString("api", API);

// ble常量
const char *onBleCmdOk = "{\"code\":0}";
const char *onBleCmdFail = "{\"err\":\"fail\"}";

void micEnd(bool cancel);

void playListPrepare()
{
    if (micState != 0 && micState != 4)
    {
        micEnd(true);
    }

    if (axAudio->isRunning())
    {
        axAudio->stopSong();
    }

    playIndexed = -1;
}

void playListStop()
{
    if (axAudio->isRunning())
    {
        axAudio->stopSong();
    }

    if (playList != nullptr)
    {
        playList = nullptr;
        // stop
        axBleSend(axBleCmdPlayState, "4");
    }
}

void playListSend(bool setPos)
{
    if (playList != nullptr && playIndex >= 0 && playIndex < playList->size())
    {
        uint32_t duration = axAudio->getAudioFileDuration();
        if (duration > 0)
        {
            playIndexed = playIndex;
            JsonObject playData = (*playList)[playIndex];
            if (setPos && playData.containsKey("pos"))
            {
                axAudio->setAudioPlayPosition(playData["pos"]);
            }

            jsonDoc.clear();
            jsonDoc["duration"] = duration;
            jsonDoc["current"] = axAudio->getAudioCurrentTime();
            jsonDoc["data"] = playData;
            if (jsonDocPlayList.containsKey("id"))
            {
                jsonDoc["id"] = jsonDocPlayList["id"];
            }

            String jsonString;
            serializeJson(jsonDoc, jsonString);
            axBleSend(axBleCmdPlayList, jsonString.c_str());
        }
    }
}

void play(const char *path)
{
    playListStop();
    if (!axAudio->connecttoFS(SPIFFS, path))
    {
        Serial.println("play fail, " + String(path));
    }
}

void playHost(const char *host)
{
    playListStop();
    axAudio->connecttohost(host);
}

void onBleCmdWifi(size_t lc, uint8_t *data)
{
    // Serial.println("onBleCmdWifi");
    Serial.println(String((const char *)data));
    deserializeJson(jsonDoc, (const char *)data);
    // Serial.println("onBleCmdWifi deserializeJson did");
    axWifiConn(jsonDoc["ssid"], jsonDoc["password"]);
    // axWifiConn("yuanjiuyan", "88889999");
    Serial.println("onBleCmdWifi axWifiConn did");
    axBleSend(axBleCmdWifi, onBleCmdOk);
}

void onBleCmdStatus(size_t lc, uint8_t *data)
{
    jsonDoc.clear();
    jsonDoc["api"] = api;
    jsonDoc["volume"] = axAudio->getVolume();
    jsonDoc["running"] = axAudio->isRunning();
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    axBleSend(axBleCmdStatus, jsonString.c_str());
    // 发送当前播放列表信息
    playListSend(false);
}

void onBleCmdSet(size_t lc, uint8_t *data)
{
    deserializeJson(jsonDoc, (const char *)data);
    if (jsonDoc.containsKey("api"))
    {
        api = String((const char *)jsonDoc["api"]);
        if (api.length() <= 1)
        {
            api = API;
            axPreferences.remove("api");
        }
        else
        {
            axPreferences.putString("api", api);
        }
    }

    if (jsonDoc.containsKey("volume"))
    {
        int volume = jsonDoc["volume"];
        axAudioSetVolume(volume);
    }
}

void onBleCmdPlayState(size_t lc, uint8_t *data)
{
    int state = atoi((const char *)data);
    switch (state)
    {
    case 1:
        // 播放
        if (!axAudio->isRunning())
        {
            axAudio->pauseResume();
        }
        break;
    case 2:
        // 暂停
        if (axAudio->isRunning())
        {
            axAudio->pauseResume();
        }
        break;
    case 3:
        // 播放|暂停
        axAudio->pauseResume();
        break;
    case 4:
        // 停止
        playListStop();
        break;
    }
}

void onBleCmdPlayList(size_t lc, uint8_t *data)
{
    deserializeJson(jsonDocPlayList, (const char *)data);
    if (!jsonDocPlayList.containsKey("list"))
    {
        return;
    }

    JsonArray list = jsonDocPlayList["list"];
    playList = nullptr;
    playListPrepare();
    playList = &list;
    JsonArray array = jsonDoc.as<JsonArray>();
    playIndex = jsonDocPlayList.containsKey("index") ? jsonDocPlayList["index"] : 0;
    if (playIndex >= 0 && playIndex < playList->size())
    {
        JsonObject playData = (*playList)[playIndex];
        if (playData.containsKey("url"))
        {
            // 播放，待同步
            playIndexed = -1;
            axAudio->connecttohost(playData["url"]);
            return;
        }
    }

    playList = nullptr;
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

    axAudioInit();

    axWifiInit();
    // axWifiConn("yuanjiuyan", "88889999");

    // 模块初始化
    axBleReg(axBleCmdWifi, onBleCmdWifi);
    axBleReg(axBleCmdStatus, onBleCmdStatus);
    axBleReg(axBleCmdSet, onBleCmdSet);
    axBleReg(axBleCmdPlayState, onBleCmdPlayState);
    axBleReg(axBleCmdPlayList, onBleCmdPlayList);
    axBleInit(true);
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
    playListStop();
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
            axMicConLen = 0;
            axHttp.end();
        }
        else
        {
            axMicConLen = 0;
            axHttp.setTimeout(30000);
            int repErr = axHttp.chunkedRespone();
            if (repErr != 0 && repErr != 200)
            {
                Serial.println("micEnd chunkedRespone err " + String(repErr));
                play("/reqFail.mp3");
            }
            else
            {
                String response = axHttp.getString();
                Serial.println("micEnd Response: " + response);
                deserializeJson(jsonDoc, response);
                const char *tUrl = jsonDoc["tUrl"];
                if (tUrl)
                {
                    playHost(tUrl);
                }
                else
                {
                    play("/reqFail.mp3");
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
    axBleLoop();
    axWifiLoop();
    if (!axWifiConnected)
    {
        status = status == MainStatusSetUp ? MainStatusConning : MainStatusSetUp;
        digitalWrite(ledPin, status == MainStatusSetUp ? HIGH : LOW);
        if (netConned || netConnSeq != axWifiConnSeq)
        {
            netConned = false;
            netConnSeq = axWifiConnSeq;
            play("/netFail.mp3");
        }

        // return;
    }
    else if (!netConned || netConnSeq != axWifiConnSeq)
    {
        netConned = true;
        netConnSeq = axWifiConnSeq;
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
                axHttp.begin(api);
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
                play("/reqFail.mp3");
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
    bool axAudioRunning = axAudio->isRunning();
    axAudio->loop();
    if (!loopBooted)
    {
        loopBooted = true;
        Serial.println("loopBoot");
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
        if (playIndexed == -1 && playIndexed != playIndex)
        {
            playListSend(true);
        }

        return;
    }
    else
    {
        if (axAudioRunning && playList != nullptr)
        {
            // 自动播放下一曲
            playIndex++;
            if (playIndex >= 0 && playIndex < playList->size())
            {
                JsonObject playData = (*playList)[playIndex];
                if (playData.containsKey("url"))
                {
                    playIndexed = -1;
                    axAudio->connecttohost(playData["url"]);
                    return;
                }
            }

            playList = nullptr;
        }
    }

    // 空闲
    if (status != MainStatusIdle)
    {
        status = MainStatusIdle;
        digitalWrite(ledPin, LOW);
    }
}
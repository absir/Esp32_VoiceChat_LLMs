#include "./axLibs/axBle.h"
#include "./axLibs/axWifi.h"
#include "./axLibs/axAudio.h"

bool audioPlayed = false;

void setup()
{
    Serial.begin(115200);
    // axBleInit(false);
    
    axWifiInit();
    axWifiConn("yuanjiuyan", "88889999");

    axAudioInit();
}

void loop()
{
    delay(10);
    axWifiLoop();
    if (!axWifiConnected)
    {
        return;
    }

    axAudio->loop();
    if (!audioPlayed && !axAudio->isRunning())
    {
        audioPlayed = true;
        axAudio->connecttohost("https://p2.dev.yiyiny.com/a/tts.mp3");
    }
}
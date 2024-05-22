#include "axAudio.h"

#define PRE_KEY_VOLUME "ble.conned"

Audio audio(false, 3, I2S_NUM_0);
Audio *axAudio = nullptr;

void axAudioInit()
{
    if (axAudio != nullptr)
    {
        return;
    }

    axAudio = &audio;
    if (!axAudio->setPinout(AX_AUDIO_BCLK, AX_AUDIO_LRC, AX_AUDIO_DOUT))
    {
        Serial.println("axAudioInit failed.");
    }

    axAudio->setVolume(axPreferences.getInt(PRE_KEY_VOLUME, AX_VOL_DEFAULT));
    axAudio->setConnectionTimeout(AX_VOL_TIMEOUT_MS, AX_VOL_TIMEOUT_SSL_MS);
}
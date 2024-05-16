#include "axAudio.h"

#define PRE_KEY_VOLUME "ble.conned"

Audio audio(false, 3, I2S_NUM_1);
Audio *axAudio = nullptr;

void axAudioInit()
{
    if (axAudio != nullptr)
    {
        return;
    }

    axAudio = &audio;
    axAudio->setPinout(AX_I2S_BCLK, AX_I2S_LRC, AX_I2S_DOUT);
    axAudio->setVolume(axPreferences.getInt(PRE_KEY_VOLUME, AX_VOL_DEFAULT));
    axAudio->setConnectionTimeout(AX_VOL_TIMEOUT_MS, AX_VOL_TIMEOUT_SSL_MS);
}
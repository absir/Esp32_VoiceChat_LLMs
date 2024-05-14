#include "axAudio.h"

#define PRE_KEY_VOLUME "ble.conned"

void axAudioInit()
{
    axAudio.setPinout(AX_I2S_BCLK, AX_I2S_LRC, AX_I2S_DOUT, -1);
    axAudio.setVolumeSteps(AX_VOL_MAX_STEPS + 1);
    axAudio.setVolume(axPreferences.getInt(PRE_KEY_VOLUME, AX_VOL_DEFAULT));
    axAudio.setConnectionTimeout(AX_VOL_TIMEOUT_MS, AX_VOL_TIMEOUT_SSL_MS);
}
#ifndef AX_MIC_H
#define AX_MIC_H
#include <Arduino.h>
#include "driver/i2s.h"
#include "esp_system.h"

enum MicType
{
  ADMP441,
  ICS43434,
  M5GO,
  M5STACKFIRE
};

class AxMic
{
public:
  AxMic(uint32_t sampleRate, int pinBclk, int pinLrc, int pinDin);
  void clear();

  int read(char *data, int numData);
  // 连续录音
  int recordContiue(int *conLen, bool conAppend, char *data, int dataLen, int numStep, int *silence, int silenceMax, int rmsMin);

  static float calculateRMS(char *data, int dataSize);
};

#endif

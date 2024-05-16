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
  i2s_bits_per_sample_t BITS_PER_SAMPLE;

public:
  AxMic(uint32_t sampleRate, int pinBclk, int pinLrc, int pinDin);
  int read(char *data, int numData);
  int getBitPerSample();
  void clear();

  // data 修正直取左声道, 返回data numLeft长度
  int recordLeft(char *data, int numData);

  // 连续录音
  int recordContiue(int *conLen, bool conAppend, char *data, int dataLen, int numStep, bool left, int *silence, int noise);

  static float calculateRMS(char *data, int dataSize);
};

#endif

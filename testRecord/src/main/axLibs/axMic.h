#ifndef AX_MIC_H
#define AX_MIC_H
#include <Arduino.h>
#include "driver/i2s.h"

enum MicType
{
  ADMP441,
  ICS43434,
  M5GO,
  M5STACKFIRE
};

class AxMic
{
protected:
  int _sampleRate;
  i2s_port_t _i2sPort;

public:
  AxMic(uint32_t sampleRate, int pinBck, int pinWs, int pinIn, i2s_port_t i2sPort = I2S_NUM_0);
  i2s_port_t i2sPort()
  {
    return _i2sPort;
  };
  void clear();
  void start();
  void stop();

  int read(char *data, int numData);
  // 连续录音
  int recordContiue(int *conLen, bool conAppend, char *data, int dataLen, int numStep, int *silence, int silenceMax, int rmsMin);

  static float calculateRMS(char *data, int dataSize);
};

#endif

#include "axMic.h"

// This I2S specification :
//  -   LRC high is channel 2 (right).
//  -   LRC signal transitions once each word.
//  -   DATA is valid on the CLOCK rising edge.
//  -   Data bits are MSB first.
//  -   DATA bits are left-aligned with respect to LRC edge.
//  -   DATA bits are right-shifted by one with respect to LRC edges.
AxMic::AxMic(uint32_t sampleRate, int pinBck, int pinWs, int pinIn, i2s_port_t i2sPort)
{
  // sampleRate = 44100;
  i2s_bits_per_sample_t bitsPreSample = I2S_BITS_PER_SAMPLE_32BIT;
  _sampleRate = sampleRate;
  _i2sPort = i2sPort;
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 接收模式
      .sample_rate = sampleRate,
      .bits_per_sample = bitsPreSample,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                                                     // 单声道 I2S_CHANNEL_FMT_ONLY_RIGHT
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB), // i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB)
      .intr_alloc_flags = 0,
      .dma_buf_count = 16,
      .dma_buf_len = 60,
      .use_apll = false
      // ,
      // .tx_desc_auto_clear = false,
      // .fixed_mclk = 0
      };

  esp_err_t err;

  err = i2s_driver_install(i2sPort, &i2s_config, 0, NULL);
  if (err != ESP_OK)
  {
    Serial.printf("AxMic Failed installing driver: %d\n", err);
  }

  i2s_pin_config_t pin_config = {
      .bck_io_num = pinBck,              // BCKL
      .ws_io_num = pinWs,                // LRCL
      .data_out_num = I2S_PIN_NO_CHANGE, // not used (only for speakers)
      .data_in_num = pinIn               // DOUT
  };

  err = i2s_set_pin(i2sPort, &pin_config);
  if (err != ESP_OK)
  {
    Serial.printf("AxMic Failed setting pin: %d\n", err);
  }

  err = i2s_set_clk(i2sPort, sampleRate, bitsPreSample, I2S_CHANNEL_STEREO);
  if (err != ESP_OK)
  {
    Serial.printf("AxMic Failed setting pin: %d\n", err);
  }

  Serial.println("AxMic I2S driver installed.");
}

int AxMic::read(char *data, int numData)
{
  size_t bytesRead = 0;
  i2s_read(this->_i2sPort, data, numData, &bytesRead, portMAX_DELAY);
  if (bytesRead > 0)
  {
    numData = bytesRead / 8;
    for (size_t i = 0; i < numData; i++)
    {
      data[i * 2] = data[i * 8 + 2];
      data[i * 2 + 1] = data[i * 8 + 3];
    }

    bytesRead = numData * 2;
    Serial.println("read " + String((int8_t)data[0]) + ", " + String((int8_t)data[bytesRead - 1]));
    return bytesRead;
  }

  return bytesRead;
}

void AxMic::clear()
{
  i2s_zero_dma_buffer(this->_i2sPort);
}

void AxMic::start()
{
  i2s_start(this->_i2sPort);
}

void AxMic::stop()
{
  i2s_stop(this->_i2sPort);
}

// 连续录音
int AxMic::recordContiue(int *conLen, bool conAppend, char *data, int dataLen, int numStep, int *silence, int silenceMax, int rmsMin)
{
  if (silence != nullptr && *silence > silenceMax)
  {
    return -1;
  }

  if (conAppend)
  {
    if ((*conLen + numStep) > dataLen)
    {
      return -1;
    }

    data = data + *conLen;
  }
  else
  {
    if (*conLen >= dataLen)
    {
      return -1;
    }
  }

  numStep = read(data, numStep);
  if (silence != nullptr)
  {
    if (calculateRMS(data, numStep) <= rmsMin)
    {
      *silence = *silence + 1;
    }
    else
    {
      *silence = 0;
    }
  }

  if (numStep <= 0)
  {
    return -1;
  }

  *conLen += numStep;
  return numStep;
}

float AxMic::calculateRMS(char *data, int dataSize)
{
  if (dataSize <= 0)
  {
    return 0;
  }

  float sum = 0;
  int16_t sample;

  // 每次迭代处理两个字节（16位）
  for (int i = 0; i < dataSize; i += 2)
  {
    // 从缓冲区中获取16位样本，考虑到字节顺序
    sample = (data[i + 1] << 8) | data[i];

    // 计算样本的平方并累加
    sum += sample * sample;
  }

  // 计算平均值
  sum /= (dataSize / 2);

  // 返回RMS值
  return sqrt(sum);
}
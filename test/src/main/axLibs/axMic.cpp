#include "axMic.h"
// #define PIN_I2S_DOUT 25

const i2s_port_t I2S_PORT = I2S_NUM_0;
const int BLOCK_SIZE = 128;
// This I2S specification :
//  -   LRC high is channel 2 (right).
//  -   LRC signal transitions once each word.
//  -   DATA is valid on the CLOCK rising edge.
//  -   Data bits are MSB first.
//  -   DATA bits are left-aligned with respect to LRC edge.
//  -   DATA bits are right-shifted by one with respect to LRC edges.
AxMic::AxMic(uint32_t sampleRate, int bckPin, int wsPin, int sdPin)
{
  i2s_bits_per_sample_t bitsPreSample = I2S_BITS_PER_SAMPLE_32BIT;
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 接收模式
      .sample_rate = sampleRate,
      .bits_per_sample = bitsPreSample,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // 单声道
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 128};

  i2s_pin_config_t pin_config;
  pin_config.bck_io_num = bckPin;
  pin_config.ws_io_num = wsPin;
  pin_config.data_out_num = I2S_PIN_NO_CHANGE;
  pin_config.data_in_num = sdPin;
  pin_config.mck_io_num = GPIO_NUM_0; // Set MCLK to GPIO0

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_set_clk(I2S_NUM_0, sampleRate, bitsPreSample, I2S_CHANNEL_MONO);
}

int AxMic::read(char *data, int numData)
{
  size_t bytesRead;
  i2s_read(I2S_NUM_0, (char *)data, numData, &bytesRead, portMAX_DELAY);
  if (bytesRead > 0)
  {
    numData = bytesRead / 4;
    for (int i = 0; i < numData; ++i)
    {
      data[2 * i] = data[4 * i + 2];
      data[2 * i + 1] = data[4 * i + 3];
    }

    return numData * 2;
  }

  return bytesRead;
}

void AxMic::clear()
{
  i2s_zero_dma_buffer(I2S_NUM_0);
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
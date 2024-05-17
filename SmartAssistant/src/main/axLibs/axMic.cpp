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
axMic::axMic(uint32_t sampleRate, int pinBclk, int pinLrc, int pinDin)
{
  BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = sampleRate,
      .bits_per_sample = BITS_PER_SAMPLE,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = 0,
      .dma_buf_count = 16,
      .dma_buf_len = 60};
  i2s_pin_config_t pin_config;
  pin_config.bck_io_num = pinBclk;
  pin_config.ws_io_num = pinLrc;
  pin_config.data_out_num = I2S_PIN_NO_CHANGE;
  pin_config.data_in_num = pinDin;
  pin_config.mck_io_num = GPIO_NUM_0; // Set MCLK to GPIO0
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_set_clk(I2S_NUM_0, sampleRate, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO);
}

int axMic::read(char *data, int numData)
{
  size_t bytesRead;
  i2s_read(I2S_NUM_0, (char *)data, numData, &bytesRead, portMAX_DELAY);
  return bytesRead;
}

int axMic::getBitPerSample()
{
  return (int)BITS_PER_SAMPLE;
}

void axMic::clear()
{
  i2s_zero_dma_buffer(I2S_NUM_0);
}

int axMic::recordLeft(char *data, int numData)
{
  numData = read(data, numData);
  int numLeft = numData / 8;
  for (int i = 0; i < numLeft; ++i)
  {
    data[2 * i] = data[8 * i + 2];
    data[2 * i + 1] = data[8 * i + 3];
  }

  return numLeft * 2;
}

// 连续录音
int axMic::recordContiue(int *conLen, char *data, int dataLen, int numStep, bool left, int *silence, int noise)
{
  if (silence != nullptr && *silence == 0)
  {
    return -1;
  }

  if ((*conLen + numStep) > dataLen)
  {
    return -1;
  }

  data = data + *conLen;
  numStep = left ? recordLeft(data, numStep) : read(data, numStep);
  if (silence != nullptr && *silence > 0)
  {
    if (calculateRMS(data, numStep) <= noise)
    {
      *silence--;
    }
  }

  *conLen += numStep;
  return numStep;
}

float axMic::calculateRMS(char *data, int dataSize)
{
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
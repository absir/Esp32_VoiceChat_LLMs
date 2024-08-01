#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include "./axLibs/axClient.h"
// #include <ArduinoJson.h>

// WiFi和MQTT服务器详情
const char *ssid = "yuanjiuyan";   // WiFi名称
const char *password = "88889999"; // WiFi密码

// MQTT客户端设置
// WiFiClient espClient;

// 音频缓冲区大小
const size_t bufferSize = 1024;
uint8_t buffer[bufferSize];

// http请求
AxClient axHttp;
// http://192.168.36.10:8787/S/spoken/1
//
#define axSpokenUrl "http://192.168.36.10:8787/S/spoken/4"

// 信号灯
#define ledPin 2
int axMicConLen = 0;

// 函数原型
void setup_wifi();
void connect();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void i2s_init();
size_t i2s_read(uint8_t *data, size_t size);

#define FOR_LILYGO_TCAMERAPLUS

// INMP441 I2S pin assignment
#if defined(FOR_LILYGO_TCAMERAPLUS)
// for the mic built-in to LiLyGO TCamerPlus
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_SAMPLE_BIT_COUNT 32
#define SOUND_SAMPLE_RATE 8000
#define SOUND_CHANNEL_COUNT 1
#define I2S_PORT I2S_NUM_0
#elif defined(FOR_LILYGO_TSIMCAM)
// for the mic built-in to LiLyGO TSimCam
#define I2S_WS 42
#define I2S_SD 2
#define I2S_SCK 41
#define I2S_SAMPLE_BIT_COUNT 16
#define SOUND_SAMPLE_RATE 16000
#define SOUND_CHANNEL_COUNT 1
#define I2S_PORT I2S_NUM_0
#elif defined(FOR_LILYGO_TWATCH)
// experimental
#define I2S_WS 0
#define I2S_SD 2
#define I2S_SCK I2S_PIN_NO_CHANGE
#define I2S_SAMPLE_BIT_COUNT 16
#define SOUND_SAMPLE_RATE 16000
#define SOUND_CHANNEL_COUNT 1
#define I2S_PORT I2S_NUM_0
#elif defined(FOR_VCC_S3EYE)
#define I2S_WS 42  // 0
#define I2S_SD 2   // 2
#define I2S_SCK 41 // I2S_PIN_NO_CHANGE
#define I2S_SAMPLE_BIT_COUNT 16
#define SOUND_SAMPLE_RATE 16000
#define SOUND_CHANNEL_COUNT 1
#define I2S_PORT I2S_NUM_0
#else
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_SAMPLE_BIT_COUNT 16
#define SOUND_SAMPLE_RATE 8000
#define SOUND_CHANNEL_COUNT 1
#define I2S_PORT I2S_NUM_0
#endif

// name of recorded WAV file; since only a single name; hence new one will always overwrite old one
const char *SoundName = "recorded_sound";

const int I2S_DMA_BUF_COUNT = 8;
const int I2S_DMA_BUF_LEN = 1024;

#if I2S_SAMPLE_BIT_COUNT == 32
const int StreamBufferNumBytes = 512;
const int StreamBufferLen = StreamBufferNumBytes / 4;
int32_t StreamBuffer[StreamBufferLen];
#else
#if SOUND_SAMPLE_RATE == 16000
// for 16 bits ... 16000 sample per second (32000 bytes per second; since 16 bits per sample) ==> 512 bytes = 16 ms per read
const int StreamBufferNumBytes = 512;
#else
// for 16 bits ... 8000 sample per second (16000 bytes per second; since 16 bits per sample) ==> 256 bytes = 16 ms per read
const int StreamBufferNumBytes = 256;
#endif
const int StreamBufferLen = StreamBufferNumBytes / 2;
int16_t StreamBuffer[StreamBufferLen];
#endif

// sound sample (16 bits) amplification
const int MaxAmplifyFactor = 20;
const int DefAmplifyFactor = 10;
int amplifyFactor = DefAmplifyFactor; // 10;

esp_err_t i2s_install()
{
  uint32_t mode = I2S_MODE_MASTER | I2S_MODE_RX;
#if I2S_SCK == I2S_PIN_NO_CHANGE
  mode |= I2S_MODE_PDM;
#endif
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(mode /*I2S_MODE_MASTER | I2S_MODE_RX*/),
      .sample_rate = SOUND_SAMPLE_RATE,
      .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BIT_COUNT),
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = 0,
      .dma_buf_count = I2S_DMA_BUF_COUNT /*8*/,
      .dma_buf_len = I2S_DMA_BUF_LEN /*1024*/,
      .use_apll = false};
  return i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

esp_err_t i2s_setpin()
{
  const i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE /*-1*/,
      .data_in_num = I2S_SD};
  return i2s_set_pin(I2S_PORT, &pin_config);
}

void setup()
{
  Serial.begin(115200);
  // 连接WiFi
  setup_wifi();
  // 初始化I2S以进行音频处理
  // set up I2S
  if (i2s_install() != ESP_OK)
  {
    Serial.println("XXX failed to install I2S");
  }
  if (i2s_setpin() != ESP_OK)
  {
    Serial.println("XXX failed to set I2S pins");
  }
  if (i2s_zero_dma_buffer(I2S_PORT) != ESP_OK)
  {
    Serial.println("XXX failed to zero I2S DMA buffer");
  }
  if (i2s_start(I2S_PORT) != ESP_OK)
  {
    Serial.println("XXX failed to start I2S");
  }

  Serial.println("... DONE SETUP MIC");
}

int32_t calculateRMS(int16_t *data, int16_t dataSize)
{
  if (dataSize <= 0)
  {
    return 0;
  }

  float dataSum = 0;
  for (int i = 0; i < dataSize; i++)
  {
    dataSum += data[i];
  }

  int32_t dataMean = (int32_t)(dataSum / dataSize);
  dataMean = dataMean < 0 ? dataMean : 0;

  dataSum = 0;
  for (int i = 0; i < dataSize; i++)
  {
    int32_t sample = data[i] - dataMean;
    dataSum += (sample * sample);
  }

  // 返回RMS值
  return (int32_t)sqrt(dataSum / dataSize);
}

void loop()
{
  if (axMicConLen < 0)
  {
    return;
  }

  if (axMicConLen == 0)
  {
    digitalWrite(ledPin, HIGH);
    axHttp.begin(axSpokenUrl);
    // 设置连接超时时间为10秒
    axHttp.setConnectTimeout(10000);
    axHttp.addHeader("Content-Type", "application/octet-stream");
    axHttp.chunkedConn("POST");
  }

  axMicConLen++;
  // 记录并发送音频数据
  // read I2S data and place in data buffer
  size_t bytesRead = 0;
  esp_err_t result = i2s_read(I2S_PORT, &StreamBuffer, StreamBufferNumBytes, &bytesRead, portMAX_DELAY);

  int samplesRead = 0;
#if I2S_SAMPLE_BIT_COUNT == 32
  int16_t sampleStreamBuffer[StreamBufferLen];
#else
  int16_t *sampleStreamBuffer = StreamBuffer;
#endif
  if (result == ESP_OK)
  {
#if I2S_SAMPLE_BIT_COUNT == 32
    samplesRead = bytesRead / 4; // 32 bit per sample
#else
    samplesRead = bytesRead / 2; // 16 bit per sample
#endif
    if (samplesRead > 0)
    {
      // find the samples mean ... and amplify the sound sample, by simply multiple it by some "amplify factor"
      float sumVal = 0;
      for (int i = 0; i < samplesRead; ++i)
      {
        int32_t val = StreamBuffer[i];
#if I2S_SAMPLE_BIT_COUNT == 32
        val = val / 0x0000ffff;
        // val = val >> 16;
        // val += 32767;
#endif
        if (amplifyFactor > 1)
        {
          val = amplifyFactor * val;
          if (val > 32700)
          {
            val = 32700;
          }
          else if (val < -32700)
          {
            val = -32700;
          }
          // StreamBuffer[i] = val;
        }
        sampleStreamBuffer[i] = val;
        sumVal += val;
      }

      float meanVal = sumVal / samplesRead;
      Serial.println("samplesRead = " + String(samplesRead) + " , " + String(meanVal) + " . " + String(calculateRMS(sampleStreamBuffer, samplesRead)) + " : " + String(sampleStreamBuffer[0]) + " , " + String(((byte *)sampleStreamBuffer)[3 * 2]) + " , " + String(((byte *)sampleStreamBuffer)[3 * 2 + 1]));

      axHttp.chunkedSend((char *)sampleStreamBuffer, samplesRead * 2);

      byte pre[8];
      memcpy(pre, sampleStreamBuffer, 8);
      pre[7] = 0;

      Serial.println(String((char *)pre) + " . " + "calculateRMS = " + String(calculateRMS((int16_t *)((char *)sampleStreamBuffer + 3 + 2), samplesRead)));
    }
  }

  if (axMicConLen > 600)
  {
    axMicConLen = -1;
    axHttp.setTimeout(30000);
    int repErr = axHttp.chunkedRespone();
    if (repErr != 0 && repErr != 200)
    {
      Serial.println("micEnd chunkedRespone err " + String(repErr));
    }
    else
    {
      String response = axHttp.getString();
      Serial.println("micEnd Response: " + response);
    }

    axHttp.end();
  }
}

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("正在连接到WIFI ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(600);
    Serial.print("-");
  }
  Serial.println("WiFi 已连接");
  Serial.println("IP 地址: ");
  Serial.println(WiFi.localIP());
}
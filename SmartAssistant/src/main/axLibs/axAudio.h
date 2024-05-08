#ifndef AX_AUDIO_H
#define AX_AUDIO_H

#pragma once
#pragma GCC optimize("Ofast")
#include <vector>
#include <Arduino.h>
#include <libb64/cencode.h>
#include <esp32-hal-log.h>
#include <SPI.h>
#include <vector>
#include <driver/i2s.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPIFFS.h>
#include <FS.h>
#include <FFat.h>

using namespace std;

extern __attribute__((weak)) void audio_info(const char *);
extern __attribute__((weak)) void audio_id3data(const char *);                                      // ID3 metadata
extern __attribute__((weak)) void audio_id3image(File &file, const size_t pos, const size_t size);  // ID3 metadata image
extern __attribute__((weak)) void audio_id3lyrics(File &file, const size_t pos, const size_t size); // ID3 metadata lyrics
extern __attribute__((weak)) void audio_eof_mp3(const char *);                                      // end of mp3 file
extern __attribute__((weak)) void audio_showstreamtitle(const char *);
extern __attribute__((weak)) void audio_showstation(const char *);
extern __attribute__((weak)) void audio_bitrate(const char *);
extern __attribute__((weak)) void audio_commercial(const char *);
extern __attribute__((weak)) void audio_icyurl(const char *);
extern __attribute__((weak)) void audio_icydescription(const char *);
extern __attribute__((weak)) void audio_lasthost(const char *);
extern __attribute__((weak)) void audio_eof_speech(const char *);
extern __attribute__((weak)) void audio_eof_stream(const char *);                                       // The webstream comes to an end
extern __attribute__((weak)) void audio_process_extern(int16_t *buff, uint16_t len, bool *continueI2S); // record audiodata or send via BT
extern __attribute__((weak)) void audio_process_i2s(uint32_t *sample, bool *continueI2S);               // record audiodata or send via BT

class axAudioBuffer
{
    // AudioBuffer will be allocated in PSRAM, If PSRAM not available or has not enough space AudioBuffer will be
    // allocated in FlashRAM with reduced size
    //
    //  m_buffer            m_readPtr                 m_writePtr                 m_endPtr
    //   |                       |<------dataLength------->|<------ writeSpace ----->|
    //   ??                       ??                         ??                         ??
    //   ---------------------------------------------------------------------------------------------------------------
    //   |                     <--m_buffSize-->                                      |      <--m_resBuffSize -->     |
    //   ---------------------------------------------------------------------------------------------------------------
    //   |<-----freeSpace------->|                         |<------freeSpace-------->|
    //
    //
    //
    //   if the space between m_readPtr and buffend < m_resBuffSize copy data from the beginning to resBuff
    //   so that the mp3/aac/flac frame is always completed
    //
    //  m_buffer                      m_writePtr                 m_readPtr        m_endPtr
    //   |                                 |<-------writeSpace------>|<--dataLength-->|
    //   ??                                 ??                         ??                ??
    //   ---------------------------------------------------------------------------------------------------------------
    //   |                        <--m_buffSize-->                                    |      <--m_resBuffSize -->     |
    //   ---------------------------------------------------------------------------------------------------------------
    //   |<---  ------dataLength--  ------>|<-------freeSpace------->|
    //
    //

public:
    axAudioBuffer(size_t maxBlockSize = 0); // constructor
    ~axAudioBuffer();                       // frees the buffer
    size_t init();                        // set default values
    bool isInitialized() { return m_f_init; };
    void setBufsize(int ram, int psram);
    void changeMaxBlockSize(uint16_t mbs); // is default 1600 for mp3 and aac, set 16384 for FLAC
    uint16_t getMaxBlockSize();            // returns maxBlockSize
    size_t freeSpace();                    // number of free bytes to overwrite
    size_t writeSpace();                   // space fom writepointer to bufferend
    size_t bufferFilled();                 // returns the number of filled bytes
    void bytesWritten(size_t bw);          // update writepointer
    void bytesWasRead(size_t br);          // update readpointer
    uint8_t *getWritePtr();                // returns the current writepointer
    uint8_t *getReadPtr();                 // returns the current readpointer
    uint32_t getWritePos();                // write position relative to the beginning
    uint32_t getReadPos();                 // read position relative to the beginning
    void resetBuffer();                    // restore defaults
    bool havePSRAM() { return m_f_psram; };

protected:
    size_t m_buffSizePSRAM = UINT16_MAX * 10; // most webstreams limit the advance to 100...300Kbytes
    size_t m_buffSizeRAM = 1600 * 10;
    size_t m_buffSize = 0;
    size_t m_freeSpace = 0;
    size_t m_writeSpace = 0;
    size_t m_dataLength = 0;
    size_t m_resBuffSizeRAM = 1600;       // reserved buffspace, >= one mp3  frame
    size_t m_resBuffSizePSRAM = 4096 * 4; // reserved buffspace, >= one flac frame
    size_t m_maxBlockSize = 1600;
    uint8_t *m_buffer = NULL;
    uint8_t *m_writePtr = NULL;
    uint8_t *m_readPtr = NULL;
    uint8_t *m_endPtr = NULL;
    bool m_f_start = true;
    bool m_f_init = false;
    bool m_f_psram = false; // PSRAM is available (and used...)
};



#endif
#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>
#include <FS.h>      // Pour File (SD, SPIFFS…)


typedef struct {
  uint16_t audioFormat;   // 1 = PCM
  uint16_t channels;      // 1 = mono, 2 = stéréo
  uint32_t sampleRate;    // ex: 44100
  uint16_t bitsPerSample; // 16 attendu ici
  uint32_t dataSize;      // taille du chunk "data"
} WavHeader;

extern float g_volume;  

// TODO: remplace ces GPIO par les tiens
#define I2S_BCLK_PIN    6    // BCLK
#define I2S_LRCLK_PIN   7    // LRCLK / WS
#define I2S_DOUT_PIN    5    // DIN du MAX99357
#define AMP_SD_MODE_PIN 8    // SD_MODE (shutdown / gain)


#define SD_MISO_PIN     13
#define SD_MOSI_PIN     11
#define SD_SCK_PIN      12
#define SD_CS_PIN       10

#define LED_TIM 14


bool initI2S(uint32_t sampleRate);
bool readWavHeader(File &f, WavHeader &hdr);
int16_t applyVolume(int16_t sample);


#endif // AUDIO_H
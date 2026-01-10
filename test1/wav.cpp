#include <Arduino.h>
#include "wav.h"
#include "driver/i2s.h"   // <-- indispensable pour i2s_config_t, etc.
// =======================
// Appliquer volume logiciel
// =======================
int16_t applyVolume(int16_t sample) {
  // Volume logiciel simple : float -> int16
  float s = (float)sample * g_volume;

  // Saturation pour éviter overflow
  if (s > 32767.0f) s = 32767.0f;
  if (s < -32768.0f) s = -32768.0f;

  return (int16_t)s;
}
// =======================
// Init I2S pour MAX99357
// =======================
bool initI2S(uint32_t sampleRate) {
  static bool i2sAlreadyInit = false;

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_LRCLK_PIN,
    .data_out_num = I2S_DOUT_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  if (!i2sAlreadyInit) {
    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
      Serial.println("i2s_driver_install failed");
      return false;
    }
    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
      Serial.println("i2s_set_pin failed");
      return false;
    }
    i2sAlreadyInit = true;
  }

  if (i2s_set_sample_rates(I2S_NUM_0, sampleRate) != ESP_OK) {
    Serial.println("i2s_set_sample_rates failed");
    return false;
  }

  return true;
}

// =======================
// Lecture du header WAV
// =======================
bool readWavHeader(File &f, WavHeader &hdr) {
  // Repartir du début du fichier
  f.seek(0);

  char riff[4];
  char wave[4];
  uint32_t riffSize;

  // Lire "RIFF"
  if (f.read((uint8_t *)riff, 4) != 4) return false;
  if (f.read((uint8_t *)&riffSize, 4) != 4) return false;
  if (f.read((uint8_t *)wave, 4) != 4) return false;

  if (strncmp(riff, "RIFF", 4) != 0) return false;
  if (strncmp(wave, "WAVE", 4) != 0) return false;

  bool fmtFound  = false;
  bool dataFound = false;

  // Parcourt les sous-chunks jusqu'à trouver "fmt " et "data"
  while (f.available()) {
    char  subId[4];
    uint32_t subSize;

    // Lire ID du chunk (4 octets) + taille (4 octets)
    if (f.read((uint8_t *)subId, 4) != 4) return false;
    if (f.read((uint8_t *)&subSize, 4) != 4) return false;

    // ---- Chunk "fmt " ----
    if (strncmp(subId, "fmt ", 4) == 0) {
      if (subSize < 16) {
        // Chunk fmt trop petit → pas un WAV PCM standard
        return false;
      }

      uint8_t fmtBuf[16];
      if (f.read(fmtBuf, 16) != 16) return false;

      // Tout est little-endian
      hdr.audioFormat   = (uint16_t)(fmtBuf[0] | (fmtBuf[1] << 8));
      hdr.channels      = (uint16_t)(fmtBuf[2] | (fmtBuf[3] << 8));
      hdr.sampleRate    = (uint32_t)(fmtBuf[4] |
                                     (fmtBuf[5] << 8) |
                                     (fmtBuf[6] << 16) |
                                     (fmtBuf[7] << 24));
      // byteRate = fmtBuf[8..11] (pas forcément utilisé)
      // blockAlign = fmtBuf[12..13]
      hdr.bitsPerSample = (uint16_t)(fmtBuf[14] | (fmtBuf[15] << 8));

      // Si le chunk fmt est plus grand que 16 octets → on saute le reste
      if (subSize > 16) {
        uint32_t toSkip = subSize - 16;
        f.seek(f.position() + toSkip);
      }

      fmtFound = true;
    }
    // ---- Chunk "data" ----
    else if (strncmp(subId, "data", 4) == 0) {
      hdr.dataSize = subSize;
      dataFound = true;
      // On s'arrête ici : le prochain octet à lire = début des samples PCM
      break;
    }
    // ---- Chunk inconnu → on saute ----
    else {
      // On saute la taille indiquée
      f.seek(f.position() + subSize);
    }
  }

  if (!fmtFound || !dataFound) {
    return false;
  }

  // On vérifie juste que c'est bien du PCM classique, le reste sera filtré dans le code appelant
  if (hdr.audioFormat != 1) {
    // 1 = PCM non compressé
    Serial.print("Format audio non PCM, code = ");
    Serial.println(hdr.audioFormat);
    return false;
  }

  return true;
}

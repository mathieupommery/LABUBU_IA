#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "driver/i2s.h"
#include "wav.h"

// =======================
// Configuration MAX99357
// =======================

// =======================
// Configuration Carte SD
// =======================
// TODO: remplace ces GPIO par ceux de ton bus SD

SPIClass sdSPI(FSPI);   // bus SPI pour la SD sur ESP32-S3 (FSPI)

void playbackTask(void *parameter);


// Nom du fichier WAV (chemin sur la SD)
const char *wavFileName = "/redsuninthesky.wav";

// Délai entre deux lectures (en ms)
const uint32_t LOOP_DELAY_MS = 200;

// Volume logiciel (0.0 = muet, 1.0 = plein volume)
float g_volume = 1.0f;   // tu peux régler ça


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== WAV Player ESP32-S3 + MAX99357 ===");

  // Ampli OFF au démarrage (pas de bruit parasite)
  pinMode(AMP_SD_MODE_PIN, OUTPUT);
  digitalWrite(AMP_SD_MODE_PIN, LOW);

  // Init SPI pour la carte SD
  Serial.println("Init SPI SD...");
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  Serial.print("Init SD...");
  if (!SD.begin(SD_CS_PIN, sdSPI)) {
    Serial.println(" ECHEC !");
    while (true) {
      Serial.println("Erreur SD.begin(). Vérifie SCK/MISO/MOSI/CS.");
      delay(2000);
    }
  }
  Serial.println(" OK.");

  Serial.print("Fichier WAV utilisé : ");
  Serial.println(wavFileName);

  // Tâche de lecture sur un seul cœur (core 1 ici)
  xTaskCreatePinnedToCore(playbackTask,"playbackTask",8192,nullptr,1,nullptr,1);
}

// =======================
// LOOP vide (tout en FreeRTOS)
// =======================
void loop() {
  vTaskDelay(portMAX_DELAY);
}



// =======================
// Tâche de lecture WAV
// =======================
void playbackTask(void *parameter) {
  (void)parameter;

  while (true) {
    File wavFile = SD.open(wavFileName);
    if (!wavFile) {
      Serial.print("Impossible d'ouvrir : ");
      Serial.println(wavFileName);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    WavHeader hdr;
    if (!readWavHeader(wavFile, hdr)) {
      Serial.println("Header WAV invalide / non supporté.");
      wavFile.close();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    Serial.println("\n=== Info WAV ===");
    Serial.print("Sample rate   : "); Serial.println(hdr.sampleRate);
    Serial.print("Bits per samp : "); Serial.println(hdr.bitsPerSample);
    Serial.print("Canaux        : "); Serial.println(hdr.channels);
    Serial.print("Data size     : "); Serial.println(hdr.dataSize);

    if (hdr.bitsPerSample != 16) {
      Serial.println("Seulement 16 bits supportés.");
      wavFile.close();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (hdr.channels != 1 && hdr.channels != 2) {
      Serial.println("Seulement mono ou stéréo supportés.");
      wavFile.close();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (!initI2S(hdr.sampleRate)) {
      Serial.println("Erreur init I2S.");
      wavFile.close();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // Activer l'ampli juste avant de jouer (évite les bruits au repos)
    digitalWrite(AMP_SD_MODE_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(5));  // petite marge

    const size_t BUF_SIZE = 1024;
    uint8_t buf[BUF_SIZE];

    Serial.println("Lecture du WAV...");

    size_t bytesRead;
    while ((bytesRead = wavFile.read(buf, BUF_SIZE)) > 0) {

      if (hdr.channels == 1) {
        // --- MONO 16 bits -> dupliqué en stéréo + volume ---
        size_t samplesMono = bytesRead / 2;       // nb d'échantillons mono
        static int16_t tempStereo[BUF_SIZE];      // 1024 int16 -> 2048 bytes

        size_t stereoCount = 0;
        int16_t *src = (int16_t *)buf;

        for (size_t i = 0; i < samplesMono; i++) {
          int16_t s = applyVolume(src[i]);
          tempStereo[stereoCount++] = s; // gauche
          tempStereo[stereoCount++] = s; // droite
        }

        size_t bytesToWrite = stereoCount * 2;
        size_t written = 0;
        while (written < bytesToWrite) {
          size_t w = 0;
          i2s_write(I2S_NUM_0,
                    ((uint8_t *)tempStereo) + written,
                    bytesToWrite - written,
                    &w,
                    portMAX_DELAY);
          written += w;
        }

      } else {
        // --- STÉRÉO 16 bits -> volume sur chaque échantillon ---
        size_t samplesStereo = bytesRead / 2; // nombre de int16
        int16_t *st = (int16_t *)buf;

        for (size_t i = 0; i < samplesStereo; i++) {
          st[i] = applyVolume(st[i]);
        }

        size_t written = 0;
        while (written < bytesRead) {
          size_t w = 0;
          i2s_write(I2S_NUM_0,
                    buf + written,
                    bytesRead - written,
                    &w,
                    portMAX_DELAY);
          written += w;
        }
      }
    }

    // Fin du fichier
    wavFile.close();
    Serial.println("Fin du fichier, fade-out & mute ampli...");

    // Petit fade-out / flush de quelques zéros pour stabiliser
    for (int i = 0; i < 256; i++) {
      int16_t frame[2] = {0, 0};
      size_t written;
      i2s_write(I2S_NUM_0, frame, sizeof(frame), &written, portMAX_DELAY);
    }

    // Couper l'ampli pour éviter les bruits parasites au repos
    digitalWrite(AMP_SD_MODE_PIN, LOW);

    // Pause entre deux lectures (bouclage)
    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
  }
}


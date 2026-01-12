#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <USBMSC.h>
#include <USB.h>
#include "driver/i2s.h"
#include "wav.h"


#define BTN_PIN 9      // change si besoin

#define LONG_PRESS_MS 400

static const uint16_t SECTOR_SIZE = 512;

USBMSC usbMsc;
SPIClass sdSPI(FSPI);   // bus SPI pour la SD sur ESP32-S3 (FSPI)

// =======================
// Paramètres WAV & volume
// =======================
std::vector<String> wavList;

// Volume logiciel (0.0 = muet, 1.0 = plein volume)
float g_volume = 1.0f;   // tu peux régler ça



// =======================
// Prototypes
// =======================
void playbackTask(void *parameter);
void buttonTask(void *parameter);




void setupAudioMode() {
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
  
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("Impossible d'ouvrir /");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = file.name();
      String lower = name;
      lower.toLowerCase();

      if (lower.endsWith(".wav")) {
        wavList.push_back(name);
      }
    }
    file = root.openNextFile();
  }

  Serial.printf("Trouvé %u fichier(s) WAV :\n", wavList.size());

  for (size_t i = 0; i < wavList.size(); i++) {
    Serial.printf("  [%u] %s\n", i, wavList[i].c_str());
  }
  g_wavCmdQueue = xQueueCreate(4, sizeof(WavCommand));

  xTaskCreatePinnedToCore(playbackTask,"playbackTask",8192,nullptr,2,nullptr,1);
  xTaskCreatePinnedToCore(buttonTask,"buttonTask",2048,nullptr,1,nullptr,1);
}



// ======== CALLBACK LECTURE SECTEURS ========
static int32_t myMscRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  (void)offset;
  uint8_t *buf = (uint8_t *)buffer;
  uint32_t nSectors = bufsize / SECTOR_SIZE;

  for (uint32_t i = 0; i < nSectors; ++i) {
    if (!SD.readRAW(buf + (i * SECTOR_SIZE), lba + i)) {
      return -1;  // erreur de lecture
    }
  }
  return (int32_t)bufsize;
}

// ======== CALLBACK ÉCRITURE SECTEURS ========
static int32_t myMscWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  (void)offset;
  uint8_t *buf = (uint8_t *)buffer;
  uint32_t nSectors = bufsize / SECTOR_SIZE;

  for (uint32_t i = 0; i < nSectors; ++i) {
    if (!SD.writeRAW(buf + (i * SECTOR_SIZE), lba + i)) {
      return -1;  // erreur d'écriture
    }
  }
  return (int32_t)bufsize;
}

// ======== CALLBACK START/STOP (éjection) ========
static bool myMscStartStop(uint8_t power_condition, bool start, bool load_eject)
{
  (void)power_condition;
  Serial.printf("MSC StartStop: start=%d eject=%d\n", start, load_eject);
  return true;
}


void setupMassStorageMode() {
  // ---- SD INIT ----
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSPI)) {
    Serial.println("SD init FAIL");
    while (true) delay(1000);
  }
  

  uint32_t sectorCount = SD.numSectors();   // OK avec le core ESP32

  // ---- Config USB MSC ----
  usbMsc.vendorID("ESP32");
  usbMsc.productID("MICROSD");
  usbMsc.productRevision("1.0");

  usbMsc.onRead(myMscRead);
  usbMsc.onWrite(myMscWrite);
  usbMsc.onStartStop(myMscStartStop);
  usbMsc.mediaPresent(true);

  USB.begin();
  if (!usbMsc.begin(sectorCount, SECTOR_SIZE)) {
    Serial.println("usbMsc.begin() FAILED");
  } else {
    Serial.println("MSC READY : la carte doit apparaître comme un disque USB.");
  }

}




// SETUP (simplifié)
// =======================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== WAV Player ESP32-S3 + MAX99357 ===");

    // Si le bouton est appuyé au boot → mode MSC
  if (digitalRead(BTN_PIN) == HIGH) {
    Serial.println("Mode MSC (clé USB)");
    setupMassStorageMode();
  } 
  else {
    Serial.println("Mode Audio (lecteur WAV)");
    setupAudioMode();
  }

  pinMode(AMP_SD_MODE_PIN, OUTPUT);
  digitalWrite(AMP_SD_MODE_PIN, LOW);
}

void loop() {

  vTaskDelay(10000);
}


// =======================
// Task de playback avec queue
// =======================
void playbackTask(void *parameter) {
  (void)parameter;

  for (;;) {
    if (g_wavCmdQueue == NULL) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    WavCommand cmd;
    // On attend une commande
    if (xQueueReceive(g_wavCmdQueue, &cmd, portMAX_DELAY) == pdTRUE) {

      if (cmd.cmd == 'p') {
        Serial.print("CMD PLAY: ");
        Serial.println(cmd.path);
        playWav(cmd.path, false);
      }
      else if (cmd.cmd == 'l') {
        Serial.print("CMD LOOP: ");
        Serial.println(cmd.path);
        playWav(cmd.path, true);
      }
      else if (cmd.cmd == 's') {
        Serial.println("CMD STOP en idle (ignorée ici).");
      }
    }
  }
}

void buttonTask(void *parameter) {
  (void)parameter;

  uint16_t wav_index = 0;

  bool pressed       = false;
  uint32_t pressTime = 0;

  bool isplaying=false;
  WavCommand cmd;

  pinMode(BTN_PIN, INPUT_PULLDOWN);

  for (;;) {

    // Si pas de queue ou pas de fichier, on ne fait rien
    if (g_wavCmdQueue == nullptr || wavList.empty()) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    int state = digitalRead(BTN_PIN);
    uint32_t now = millis();

    // --- Front HAUT -> BAS = début d'appui ---
    if (!pressed && state == HIGH) {
      pressed   = true;
      pressTime = now;
    }

    // --- Front BAS -> HAUT = relâchement ---
    if (pressed && state == LOW) {
      pressed = false;
      uint32_t duration = now - pressTime;

      if (duration >= LONG_PRESS_MS) {
        Serial.println(">>> APPUI LONG détecté <<<");

        wav_index = (wav_index + 1);
        if (wav_index >= wavList.size()) {
          wav_index = 0;
        }
        cmd.cmd = 's';
        xQueueSend(g_wavCmdQueue, &cmd, 0);
        cmd.cmd = 'p';
        String fullPath = "/" + wavList[wav_index];
        strncpy(cmd.path, fullPath.c_str(), sizeof(cmd.path));
        cmd.path[sizeof(cmd.path) - 1] = '\0';
        xQueueSend(g_wavCmdQueue, &cmd, 0);

      } 
      else {
        Serial.println(">>> APPUI COURT détecté <<<");

        if(isplaying){
        isplaying = false;
        cmd.cmd = 's';
        xQueueSend(g_wavCmdQueue, &cmd, 0);
        }
        else{
        isplaying = true;
        cmd.cmd = 'p';
        String fullPath = "/" + wavList[wav_index];
        strncpy(cmd.path, fullPath.c_str(), sizeof(cmd.path));
        cmd.path[sizeof(cmd.path) - 1] = '\0';
        xQueueSend(g_wavCmdQueue, &cmd, 0);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // scan 100 Hz
  }
}


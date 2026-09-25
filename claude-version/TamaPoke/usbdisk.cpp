#include "usbdisk.h"
#include <Arduino.h>

#if SOC_USB_OTG_SUPPORTED && !ARDUINO_USB_MODE
#include <USB.h>
#include <USBMSC.h>
#include "esp32-hal-tinyusb.h"  // tud_disconnect / tud_connect
#include <SD_MMC.h>
#include "sdmmc_cmd.h"  // sdmmc_read_sectors / sdmmc_write_sectors
#include <atomic>
#include "sd_lock.h"
#include "sdmon.h"
#include "audio.h"

// Objeto global: su constructor registra la interfaz MSC antes de que el core
// arranque el USB (antes de setup()). Sin medio hasta usbDiskStart().
static USBMSC msc;
static std::atomic<bool> gActive{false}, gSeen{false}, gEjected{false};
// Los callbacks corren en la tarea de TinyUSB: este mutex deja que usbDiskStop()
// espere a la lectura/escritura en curso antes de desmontar la tarjeta.
static SemaphoreHandle_t ioMutex = nullptr;

// ko5.1: la tarjeta entera por bloque (varios sectores en un solo comando SD).
// Con readRAW() se leia de 512 en 512: cada peticion de 4 KB del PC eran 8
// comandos, y Windows, que al abrir la unidad lee decenas de MB (FAT, huecos
// libres), se quedaba "cargando". _card es protected en SDMMCFS: una clase
// derivada puede nombrarlo con un puntero a miembro (C++ valido, sin trucos).
struct SdCardPeek : fs::SDMMCFS {
  static sdmmc_card_t *card(fs::SDMMCFS &fs) { return fs.*(&SdCardPeek::_card); }
};
static std::atomic<uint32_t> gReadKB{0}, gWriteKB{0}, gErrors{0};

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (!gActive.load() || xSemaphoreTake(ioMutex, portMAX_DELAY) != pdTRUE) return -1;
  int32_t r = bufsize;
  sdmmc_card_t *c = SdCardPeek::card(SD_MMC);
  if (!gActive.load() || !c || bufsize % 512) r = -1;
  else if (sdmmc_read_sectors(c, buffer, lba, bufsize / 512) != ESP_OK) r = -1;
  xSemaphoreGive(ioMutex);
  if (r >= 0) { gSeen = true; gReadKB += bufsize / 1024; }
  else gErrors++;
  return r;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  if (!gActive.load() || xSemaphoreTake(ioMutex, portMAX_DELAY) != pdTRUE) return -1;
  int32_t r = bufsize;
  sdmmc_card_t *c = SdCardPeek::card(SD_MMC);
  if (!gActive.load() || !c || bufsize % 512) r = -1;
  else if (sdmmc_write_sectors(c, buffer, lba, bufsize / 512) != ESP_OK) r = -1;
  xSemaphoreGive(ioMutex);
  if (r >= 0) gWriteKB += bufsize / 1024;
  else gErrors++;
  return r;
}

// ko5: el core no avisa al PC de que "se metio la tarjeta" (sin UNIT ATTENTION),
// y Windows se quedaba con lo que leyo al enchufar: una unidad sin medio, sin
// tamano y que no se abria. Desconectar y reconectar el USB obliga al PC a
// volver a enumerar y a leer la capacidad de verdad. (El puerto serie se
// corta un momento; vuelve solo.)
static void usbReenumerate() {
  tud_disconnect();
  delay(400);
  tud_connect();
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  if (load_eject && !start && gActive.load()) gEjected = true;  // "Expulsar" en el PC
  return true;
}

bool usbDiskSupported() { return true; }

bool usbDiskStart() {
  if (gActive.load() || !sdReady || !sdMutex) return false;
  if (!ioMutex && !(ioMutex = xSemaphoreCreateMutex())) return false;
  // la musica cierra su fichero antes de ceder la tarjeta
  if (!audioPauseForUpload()) { audioResumeAfterUpload(); return false; }
  sdExternal = true;
  // vacia a quien tuviera la SD en este momento; los siguientes ven sdExternal
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  xSemaphoreGive(sdMutex);
  uint32_t sectors = SD_MMC.numSectors(), sec = SD_MMC.sectorSize();
  if (!sectors || sec != 512) {
    sdExternal = false;
    audioResumeAfterUpload();
    return false;
  }
  msc.vendorID("TamaPoke");
  msc.productID("SD card");
  msc.productRevision("1.0");
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.onStartStop(onStartStop);
  msc.isWritable(true);
  msc.begin(sectors, sec);
  gSeen = false;
  gEjected = false;
  gReadKB = gWriteKB = gErrors = 0;
  gActive = true;
  msc.mediaPresent(true);
  usbReenumerate();
  Serial.printf("USBDISK on: %lu sectors\n", (unsigned long)sectors);
  return true;
}

void usbDiskStop() {
  if (!gActive.load()) return;
  msc.mediaPresent(false);  // el PC ve la unidad sin medio
  gActive = false;
  usbReenumerate();          // y la unidad desaparece limpia del explorador
  xSemaphoreTake(ioMutex, portMAX_DELAY);  // espera la E/S en curso del PC
  xSemaphoreGive(ioMutex);
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  bool ok = sdRemount();
  sdExternal = false;
  xSemaphoreGive(sdMutex);
  gEjected = false;
  audioResumeAfterUpload();
  if (ok) audioLoadMusic();  // vuelve a abrir bgm.wav / battle_wild.wav nuevos
  Serial.printf("USBDISK off: SD %s\n", ok ? "remontada" : "NO remontada");
}

bool usbDiskActive() { return gActive.load(); }
bool usbDiskHostSeen() { return gSeen.load(); }
bool usbDiskEjected() { return gEjected.load(); }
void usbDiskStats(uint32_t *readKB, uint32_t *writeKB, uint32_t *errors) {
  *readKB = gReadKB.load(); *writeKB = gWriteKB.load(); *errors = gErrors.load();
}

#else  // USB CDC/JTAG por hardware: sin MSC

bool usbDiskSupported() { return false; }
bool usbDiskStart() { return false; }
void usbDiskStop() {}
bool usbDiskActive() { return false; }
bool usbDiskHostSeen() { return false; }
bool usbDiskEjected() { return false; }
void usbDiskStats(uint32_t *readKB, uint32_t *writeKB, uint32_t *errors) { *readKB = *writeKB = *errors = 0; }

#endif

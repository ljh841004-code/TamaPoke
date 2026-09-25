#include "usbdisk.h"
#include <Arduino.h>

#if SOC_USB_OTG_SUPPORTED && !ARDUINO_USB_MODE
#include <USB.h>
#include <USBMSC.h>
#include "esp32-hal-tinyusb.h"  // tud_disconnect / tud_connect
#include <SD_MMC.h>
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

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (!gActive.load() || xSemaphoreTake(ioMutex, portMAX_DELAY) != pdTRUE) return -1;
  int32_t r = bufsize;
  uint32_t sec = SD_MMC.sectorSize();
  if (!gActive.load() || !sec) r = -1;
  for (uint32_t x = 0; r >= 0 && x < bufsize / sec; x++)
    if (!SD_MMC.readRAW((uint8_t *)buffer + x * sec, lba + x)) r = -1;
  xSemaphoreGive(ioMutex);
  if (r >= 0) gSeen = true;
  return r;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  if (!gActive.load() || xSemaphoreTake(ioMutex, portMAX_DELAY) != pdTRUE) return -1;
  int32_t r = bufsize;
  uint32_t sec = SD_MMC.sectorSize();
  if (!gActive.load() || !sec || sec > 512) r = -1;
  static uint8_t blk[512];  // copia alineada, como el ejemplo SD2USBMSC del core
  for (uint32_t x = 0; r >= 0 && x < bufsize / sec; x++) {
    memcpy(blk, buffer + x * sec, sec);
    if (!SD_MMC.writeRAW(blk, lba + x)) r = -1;
  }
  xSemaphoreGive(ioMutex);
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

#else  // USB CDC/JTAG por hardware: sin MSC

bool usbDiskSupported() { return false; }
bool usbDiskStart() { return false; }
void usbDiskStop() {}
bool usbDiskActive() { return false; }
bool usbDiskHostSeen() { return false; }
bool usbDiskEjected() { return false; }

#endif

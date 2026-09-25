#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

extern SemaphoreHandle_t sdMutex;
// fork KO: true mientras el PC usa la SD como unidad USB (usbdisk.cpp). Nadie en
// la placa puede tocar el sistema de ficheros entonces: el lock falla siempre.
extern std::atomic<bool> sdExternal;
class SdCardLock {
  bool held;
public:
  explicit SdCardLock(TickType_t wait = portMAX_DELAY)
      : held(!sdExternal.load() && sdMutex && xSemaphoreTake(sdMutex, wait) == pdTRUE) {
    // quien esperaba el lock mientras se activaba la unidad USB lo obtiene tras
    // el vaciado de usbDiskStart(): se vuelve a mirar ya dentro
    if (held && sdExternal.load()) { xSemaphoreGive(sdMutex); held = false; }
  }
  ~SdCardLock() { if (held) xSemaphoreGive(sdMutex); }
  explicit operator bool() const { return held; }
  SdCardLock(const SdCardLock&) = delete;
  SdCardLock& operator=(const SdCardLock&) = delete;
};

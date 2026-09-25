#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t sdMutex;
class SdCardLock {
  bool held;
public:
  explicit SdCardLock(TickType_t wait = portMAX_DELAY)
      : held(sdMutex && xSemaphoreTake(sdMutex, wait) == pdTRUE) {}
  ~SdCardLock() { if (held) xSemaphoreGive(sdMutex); }
  explicit operator bool() const { return held; }
  SdCardLock(const SdCardLock&) = delete;
  SdCardLock& operator=(const SdCardLock&) = delete;
};

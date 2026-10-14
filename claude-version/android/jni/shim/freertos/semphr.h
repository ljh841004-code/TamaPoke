#pragma once
#include "FreeRTOS.h"
#include <mutex>
#include <chrono>
// candado recursivo de verdad (el de la SD lo cogen el juego y el hilo del audio)
typedef std::recursive_timed_mutex *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new std::recursive_timed_mutex(); }
static inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return new std::recursive_timed_mutex(); }
static inline int xSemaphoreTake(SemaphoreHandle_t m, TickType_t t) {
  if (!m) return pdFALSE;
  if (t == portMAX_DELAY) { m->lock(); return pdTRUE; }
  return m->try_lock_for(std::chrono::milliseconds(t)) ? pdTRUE : pdFALSE;
}
static inline int xSemaphoreGive(SemaphoreHandle_t m) { if (m) m->unlock(); return pdTRUE; }

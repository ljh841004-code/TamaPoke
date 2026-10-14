#pragma once
#include "FreeRTOS.h"
#include <thread>
#include <chrono>
typedef void *TaskHandle_t;
static inline void vTaskDelay(TickType_t t) { std::this_thread::sleep_for(std::chrono::milliseconds(t ? t : 1)); }
static inline BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *, uint32_t, void *arg, UBaseType_t, TaskHandle_t *, int) {
  std::thread(fn, arg).detach();
  return pdPASS;
}

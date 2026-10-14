#pragma once
#include "FreeRTOS.h"
#include <mutex>
#include <deque>
#include <vector>
#include <string.h>
struct HostQueue {
  std::mutex m;
  std::deque<std::vector<uint8_t>> q;
  size_t cap, item;
};
typedef HostQueue *QueueHandle_t;
static inline QueueHandle_t xQueueCreate(UBaseType_t n, UBaseType_t sz) { auto *h = new HostQueue(); h->cap = n; h->item = sz; return h; }
static inline void vQueueDelete(QueueHandle_t h) { delete h; }
static inline BaseType_t xQueueSend(QueueHandle_t h, const void *p, TickType_t) {
  std::lock_guard<std::mutex> g(h->m);
  if (h->q.size() >= h->cap) return pdFALSE;
  h->q.emplace_back((const uint8_t *)p, (const uint8_t *)p + h->item);
  return pdTRUE;
}
static inline BaseType_t xQueueReceive(QueueHandle_t h, void *p, TickType_t) {
  std::lock_guard<std::mutex> g(h->m);
  if (h->q.empty()) return pdFALSE;
  memcpy(p, h->q.front().data(), h->item);
  h->q.pop_front();
  return pdTRUE;
}
static inline UBaseType_t uxQueueSpacesAvailable(QueueHandle_t h) { std::lock_guard<std::mutex> g(h->m); return (UBaseType_t)(h->cap - h->q.size()); }

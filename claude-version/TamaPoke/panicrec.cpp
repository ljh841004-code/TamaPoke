// ko11.9.3: cuando la placa se cae (panic), guarda en la RTC la direccion exacta
// del codigo y unas cuantas del backtrace (con el gancho del core de Arduino,
// set_arduino_panic_handler). Al arrancar se pasan a la NVS/SD y se ven en
// "재부팅 기록"; con el .elf de esa version, addr2line dice la linea.
#include "panicrec.h"
#ifdef ESP_PLATFORM
#include <Arduino.h>
#include <esp_attr.h>

RTC_NOINIT_ATTR uint32_t gPanicMagic;
RTC_NOINIT_ATTR uint32_t gPanicN;
RTC_NOINIT_ATTR uint32_t gPanicPc[PANIC_PCS];

static void IRAM_ATTR onPanic(arduino_panic_info_t *info, void *) {
  uint32_t n = 0;
  gPanicPc[n++] = (uint32_t)info->pc;
  // backtrace[0] suele ser el mismo pc: saltarlo si coincide
  for (unsigned i = 0; i < info->backtrace_len && n < PANIC_PCS; i++) {
    if (i == 0 && info->backtrace[0] == (unsigned)info->pc) continue;
    gPanicPc[n++] = info->backtrace[i];
  }
  gPanicN = n;
  gPanicMagic = PANIC_MAGIC;
}

void panicRecBegin() { set_arduino_panic_handler(onPanic, nullptr); }

bool panicTake(uint32_t *pcs, uint8_t *n, uint32_t *exc) {
  if (gPanicMagic != PANIC_MAGIC) return false;
  gPanicMagic = 0;
  uint32_t k = gPanicN > PANIC_PCS ? PANIC_PCS : gPanicN;
  for (uint32_t i = 0; i < k; i++) pcs[i] = gPanicPc[i];
  *n = (uint8_t)k;
  *exc = 0;
  return k > 0;
}
#else
void panicRecBegin() {}
bool panicTake(uint32_t *, uint8_t *n, uint32_t *) { *n = 0; return false; }
#endif

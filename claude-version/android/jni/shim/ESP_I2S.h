#pragma once
// Android: la "I2S" del audio.cpp escribe en un anillo que vacia AAudio (16 kHz, estereo, 16 bits)
#include <stdint.h>
#include <stddef.h>
#define I2S_MODE_STD 0
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_SLOT_MODE_STEREO 2
#define I2S_STD_SLOT_BOTH 3
bool hostAudioStart(uint32_t rate);
size_t hostAudioWrite(const uint8_t *b, size_t n);  // bloquea hasta ~50 ms si el anillo esta lleno
struct I2SClass {
  void setPins(int, int, int, int, int) {}
  bool begin(int, uint32_t rate, int, int, int) { return hostAudioStart(rate); }
  size_t write(const uint8_t *b, size_t n) { return hostAudioWrite(b, n); }
};

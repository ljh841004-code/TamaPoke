// Shim de Arduino.h para renderizar pantallas en el PC (test/render).
// Solo lo que usan el sketch, Arduino_GFX y sdmon.cpp. Nada de hardware.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <string>
#include <algorithm>
#include "Print.h"

typedef uint8_t byte;
typedef bool boolean;
#define PROGMEM
#define IRAM_ATTR
#define PI 3.14159265358979323846
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define FALLING 2
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define pgm_read_word(a) (*(const uint16_t *)(a))
#define pgm_read_dword(a) (*(const uint32_t *)(a))
#define pgm_read_pointer(a) (*(void *const *)(a))
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define _BV(b) (1UL << (b))
using std::min;
using std::max;
using std::abs;

uint32_t millis();
uint32_t micros();
void delay(uint32_t);
void yield();
long random(long howbig);
long random(long lo, long hi);
void randomSeed(unsigned long);
uint32_t esp_random();
void *ps_malloc(size_t n);
void pinMode(int, int);
void digitalWrite(int, int);
int digitalRead(int);
int digitalPinToInterrupt(int);
void attachInterrupt(int, void (*)(), int);
extern uint32_t gMockMillis;



struct MockSerial : public Print {
  size_t write(uint8_t) override { return 1; }
  void begin(unsigned long) {}
  int available() { return 0; }
  String readStringUntil(char) { return String(""); }
  void setRxBufferSize(size_t) {}
  void setTxTimeoutMs(uint32_t) {}
  void setTimeout(uint32_t) {}
  size_t readBytes(uint8_t *, size_t) { return 0; }
  int printf(const char *, ...) { return 0; }
};
extern MockSerial Serial;

struct MockEsp {
  uint32_t getFreeHeap() { return 200000; }
  uint32_t getMinFreeHeap() { return 150000; }
  void restart() { exit(0); }
};
extern MockEsp ESP;

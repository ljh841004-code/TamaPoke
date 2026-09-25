#pragma once
#include <Arduino.h>
struct TwoWire {
  void begin(int = 0, int = 0) {}
  void setTimeOut(int) {}
  void beginTransmission(int) {}
  int endTransmission(bool = true) { return 0; }
  void write(uint8_t) {}
  int requestFrom(int, int) { return 0; }
  int available() { return 0; }
  int read() { return 0; }
};
extern TwoWire Wire;

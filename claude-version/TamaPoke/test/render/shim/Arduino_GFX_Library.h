#pragma once
// Solo el lienzo en memoria: el panel y el bus son de mentira (test/render)
#include "Arduino_GFX.h"
#include "canvas/Arduino_Canvas.h"
struct Arduino_ESP32QSPI {
  Arduino_ESP32QSPI(int, int, int, int, int, int) {}
};
typedef Arduino_ESP32QSPI Arduino_DataBus_Fake;
class Arduino_CO5300 : public Arduino_G {
public:
  Arduino_CO5300(Arduino_ESP32QSPI *, int, int, int w, int h, int, int, int, int) : Arduino_G(w, h) {}
  bool begin(int32_t = GFX_NOT_DEFINED) override { return true; }
  void drawBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t, uint16_t, uint16_t) override {}
  void drawIndexedBitmap(int16_t, int16_t, uint8_t *, uint16_t *, int16_t, int16_t, int16_t = 0) override {}
  void draw3bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
  void draw16bitRGBBitmap(int16_t, int16_t, uint16_t *, int16_t, int16_t) override {}
  void draw24bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
  void setBrightness(uint8_t) {}
};

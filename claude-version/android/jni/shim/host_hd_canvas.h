#pragma once
// Android (modo HD): el lienzo del juego sigue hablando en coordenadas de 466x466, pero dibuja en un
// lienzo de verdad HDK veces mas grande (1398x1398): rectangulos, circulos, triangulos, lineas y
// arcos salen con bordes finos en vez de escalones de 3 px. TamaPoke.ino usa esta clase en lugar de
// su TPCanvas cuando se compila con HOST_HD (el firmware de la placa no cambia).
//
// Lo que escribe directamente en el framebuffer (efectos de la SD, cache del combate...) recibe un
// "proxy" de 466x466: al pedir getFramebuffer() se reduce el HD al proxy, y en la siguiente llamada
// de dibujo los pixeles que cambiaron se copian (ampliados) al HD. Texto y sombras van por su camino
// HD propio (host_hd.cpp: hdText, shadeRR).
#include "Arduino_GFX.h"
#include "canvas/Arduino_Canvas.h"
#include <string.h>
#include <stdlib.h>

#define HDK 3

// host_hd.cpp
void hdSetFont(const uint8_t *d, size_t n);
bool hdFontLoaded();
bool hdGlyph(uint32_t cp, int x, int base, int px, int adv, uint16_t col);
bool hdArtOn();  // [메뉴] "HD 그림" (정지 그림): por defecto no, el PMD animado
bool hdPetDraw(int16_t dex, bool shiny, uint8_t act, int x, int ground, uint32_t t, bool hasPmd);

class TPCanvas : public Print {
public:
  Arduino_Canvas *c;
  int16_t lw, lh;  // tamano logico (466)
  uint16_t textcolor = 0xFFFF;
  int16_t curX = 0, curY = 0;
  uint8_t tsize = 1;

  TPCanvas(int16_t w, int16_t h, Arduino_G *out) : lw(w), lh(h) { c = new Arduino_Canvas(w * HDK, h * HDK, out); }
  bool begin(int32_t speed = GFX_NOT_DEFINED) {
    bool ok = c->begin(speed);
    proxy = (uint16_t *)calloc((size_t)lw * lh, 2);
    snap = (uint16_t *)calloc((size_t)lw * lh, 2);
    return ok;
  }
  uint16_t *hdfb() { sync(); return c->getFramebuffer(); }
  int hdw() const { return lw * HDK; }
  int hdh() const { return lh * HDK; }

  // ---- framebuffer "logico" (proxy) ----
  uint16_t *getFramebuffer() {
    sync(true);
    uint16_t *fb = c->getFramebuffer();
    if (!fb || !proxy) return nullptr;
    int W = lw * HDK;
    for (int y = 0; y < lh; y++) {
      const uint16_t *src = fb + (size_t)(y * HDK + 1) * W + 1;
      uint16_t *dst = proxy + (size_t)y * lw;
      for (int x = 0; x < lw; x++) dst[x] = src[x * HDK];
    }
    memcpy(snap, proxy, (size_t)lw * lh * 2);
    proxyOut = proxyEver = true;
    return proxy;
  }
  uint16_t *fbuf() { return getFramebuffer(); }
  void setFbuf(uint16_t *) {}
  // force: comparar aunque ya se hubiera sincronizado (alguien guardo el puntero del proxy y siguio escribiendo
  // despues de dibujar otra cosa): se hace en flush y al volver a pedir el framebuffer
  void sync(bool force = false) {
    if (!proxyOut && !(force && proxyEver)) return;
    proxyOut = false;
    uint16_t *fb = c->getFramebuffer();
    int W = lw * HDK;
    for (int y = 0; y < lh; y++) {
      const uint16_t *p = proxy + (size_t)y * lw;
      uint16_t *s = snap + (size_t)y * lw;
      for (int x = 0; x < lw; x++) {
        if (p[x] == s[x]) continue;
        uint16_t v = p[x];
        s[x] = v;
        uint16_t *d = fb + (size_t)y * HDK * W + x * HDK;
        for (int j = 0; j < HDK; j++, d += W)
          for (int i = 0; i < HDK; i++) d[i] = v;
      }
    }
  }

  // ---- texto (estado; el dibujo coreano va por hdText) ----
  uint16_t ink() const { return textcolor; }
  void setTextColor(uint16_t col) { textcolor = col; c->setTextColor(col); }
  void setTextColor(uint16_t col, uint16_t bg) { textcolor = col; c->setTextColor(col, bg); }
  void setTextSize(uint8_t s) { tsize = s; }
  void setCursor(int16_t x, int16_t y) { curX = x; curY = y; }
  int16_t getCursorX() const { return curX; }
  int16_t getCursorY() const { return curY; }
  void setFont(const uint8_t *f) { c->setFont(f); }
  void setFont(const GFXfont *f = NULL) { c->setFont(f); }
  void setUTF8Print(bool on) { c->setUTF8Print(on); }
  void getTextBounds(const char *s, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
    c->setTextSize(tsize);
    c->getTextBounds(s, x, y, x1, y1, w, h);
  }
  using Print::write;
  size_t write(uint8_t ch) override {  // fuente clasica / u8g2 (idiomas no coreanos, arranque)
    sync();
    c->setTextSize(tsize * HDK);
    c->setCursor(curX * HDK, curY * HDK);
    c->write(ch);
    curX = c->getCursorX() / HDK;
    curY = c->getCursorY() / HDK;
    return 1;
  }

  // ---- primitivas (coordenadas logicas) ----
  static int16_t S(int v) { return (int16_t)(v * HDK); }
  static int16_t C(int v) { return (int16_t)(v * HDK + HDK / 2); }  // centro del pixel logico
  void fillScreen(uint16_t col) { sync(); proxyOut = false; c->fillScreen(col); }
  void flush(bool force = false) { sync(true); c->flush(force); }
  void fillRectHD(int X, int Y, int W, int H, uint16_t col) { sync(); c->fillRect(X, Y, W, H, col); }  // en pixeles HD
  void drawPixel(int16_t x, int16_t y, uint16_t col) { sync(); c->fillRect(S(x), S(y), HDK, HDK, col); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    sync(); c->fillRect(S(x), S(y), S(w), S(h), col);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t col) { fillRect(x, y, w, 1, col); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t col) { fillRect(x, y, 1, h, col); }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    fillRect(x, y, w, 1, col); fillRect(x, y + h - 1, w, 1, col);
    fillRect(x, y, 1, h, col); fillRect(x + w - 1, y, 1, h, col);
  }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t col) {
    sync();
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    for (int o = -(HDK / 2); o <= HDK / 2; o++) {
      int ox = steep ? o : 0, oy = steep ? 0 : o;
      c->drawLine(C(x0) + ox, C(y0) + oy, C(x1) + ox, C(y1) + oy, col);
    }
  }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t col) { sync(); c->fillCircle(C(x), C(y), S(r) + HDK / 2, col); }
  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t col) {
    sync();
    for (int i = -(HDK / 2); i <= HDK / 2; i++) if (S(r) + i >= 0) c->drawCircle(C(x), C(y), S(r) + i, col);
  }
  void fillEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry, uint16_t col) {
    sync(); c->fillEllipse(C(x), C(y), S(rx) + HDK / 2, S(ry) + HDK / 2, col);
  }
  void drawEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry, uint16_t col) {
    sync();
    for (int i = -(HDK / 2); i <= HDK / 2; i++) c->drawEllipse(C(x), C(y), S(rx) + i, S(ry) + i, col);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t col) {
    sync(); c->fillTriangle(C(x0), C(y0), C(x1), C(y1), C(x2), C(y2), col);
  }
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t col) {
    drawLine(x0, y0, x1, y1, col); drawLine(x1, y1, x2, y2, col); drawLine(x2, y2, x0, y0, col);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    sync(); c->fillRoundRect(S(x), S(y), S(w), S(h), S(r), col);
  }
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    sync();
    for (int i = 0; i < HDK; i++) c->drawRoundRect(S(x) + i, S(y) + i, S(w) - 2 * i, S(h) - 2 * i, S(r) - i > 0 ? S(r) - i : 0, col);
  }
  void fillArc(int16_t x, int16_t y, int16_t r1, int16_t r2, float a0, float a1, uint16_t col) {
    sync(); c->fillArc(C(x), C(y), S(r1) + HDK / 2, S(r2) > HDK / 2 ? S(r2) - HDK / 2 : 0, a0, a1, col);
  }

  // ---- HD: sombra / velo de rectangulo redondeado (uiShade), a resolucion completa ----
  void shadeRR(int x, int y, int w, int h, int r, int a16) {
    uint16_t *fb = hdfb();
    if (!fb || w <= 0 || h <= 0) return;
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    int X = S(x), Y = S(y), Wd = S(w), Ht = S(h), R = S(r), W = hdw(), H = hdh();
    int k = 16 - a16;
    for (int yy = 0; yy < Ht; yy++) {
      int py = Y + yy;
      if (py < 0 || py >= H) continue;
      int dy = yy < R ? R - yy : (yy >= Ht - R ? yy - (Ht - R - 1) : 0);
      int in = 0;
      if (dy > 0) { int dx2 = R * R - dy * dy; int dx = 0; while ((dx + 1) * (dx + 1) <= dx2) dx++; in = R - dx; }
      int x0 = X + in, x1 = X + Wd - in;
      if (x0 < 0) x0 = 0;
      if (x1 > W) x1 = W;
      uint16_t *p = fb + (size_t)py * W;
      for (int px = x0; px < x1; px++) {
        uint32_t v = p[px];
        uint32_t s = (v | (v << 16)) & 0x07E0F81Fu;
        s = (s * (uint32_t)k >> 4) & 0x07E0F81Fu;
        p[px] = (uint16_t)(s | (s >> 16));
      }
    }
  }

private:
  uint16_t *proxy = nullptr, *snap = nullptr;
  bool proxyOut = false, proxyEver = false;
};

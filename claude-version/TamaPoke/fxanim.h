#pragma once
#include <Arduino.h>

// ko11.30: efectos de ataque de la SD (mons/fx/fTTSV.bin o suelto en mons/, tools/pack_fx.py).
// Los fotogramas ya vienen compuestos en coordenadas de pantalla para los dos
// sentidos (lado 0 = ataca el mio, lado 1 = ataca el rival); aqui solo se pegan
// con mezcla alfa. Si no hay archivo, el firmware usa su propio efecto.
struct FxAnim {
  uint8_t *data = nullptr;
  uint32_t size = 0;
  uint16_t key = 0xFFFF;   // tipo*9 + fase*3 + variante del archivo cargado
  uint16_t frameMs = 50;
  bool load(uint8_t type, uint8_t tier, uint8_t var);
  bool loadId(uint8_t id);   // ko11.31: por id de movimiento (los que no son de tipo: mNNN.bin)
  bool isId(uint8_t id) const { return data && mid == id; }
  uint8_t mid = 0;
  void unload();
  bool ok() const { return data != nullptr; }
  uint16_t frames(uint8_t side) const;
  uint32_t durMs(uint8_t side) const { return (uint32_t)frames(side) * frameMs; }
  // fondo (antes de los Pokemon) y graficos (encima). dx/dy = temblor de pantalla
  void drawBg(uint16_t *fb, uint8_t side, uint32_t t, int dx, int dy) const;
  void drawFg(uint16_t *fb, uint8_t side, uint32_t t, int dx, int dy) const;
private:
  const uint8_t *frame(uint8_t side, uint32_t t) const;
  const uint8_t *bg(uint8_t i) const;
  const uint8_t *table(uint8_t side) const;
};

// ko11.31: los efectos de los movimientos que pueden salir (los mios y los del rival) se
// leen todos de una vez al empezar; durante el combate solo se buscan (sin tocar la SD)
void fxPreload(const uint8_t *ids, uint8_t n);
const FxAnim *fxFind(uint8_t id);

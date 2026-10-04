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
// leen en segundo plano (fxPreloadAsync + fxPump en el bucle); al usarlos solo se buscan
const FxAnim *fxFind(uint8_t id);
// ko11.31: lo mismo sin parar la pantalla (la ficha del Pokedex): fxPump() lee un trozo cada vez
void fxPreloadAsync(const uint8_t *ids, uint8_t n);
void fxPump(uint8_t chunks = 1);
void fxWant(uint8_t id);  // ko11.31.4: leer este antes que los demas de la cola
bool fxQueued(uint8_t id);  // ko12.2.1: aun en la cola de lectura (llegara pronto)
bool fxLoading();           // ko12.3.2: queda algo por leer de la cola

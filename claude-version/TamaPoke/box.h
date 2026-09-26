#pragma once
// fork KO (ko4): bogwanham (caja) y registro de la Pokedex. Logica pura sobre
// Preferences: compila en el PC para los tests (test/test_box.cpp).
//
//   - Box: los Pokemon ganados o capturados en batalla. Al terminar el ciclo
//     de la forma final (despedida), el siguiente a criar sale de aqui al azar.
//   - DexLog: historial por especie (primera vez visto, veces visto/capturado).
//     "Descubierto" = visto en batalla, capturado o criado alguna vez.
#include <Arduino.h>
#include <Preferences.h>

#define BOX_MAX 60

enum : uint8_t { BOXF_SHINY = 1, BOXF_CAUGHT = 2 };  // CAUGHT: con pokeball (si no, ganado)

struct __attribute__((packed)) BoxMon {
  int16_t dex;
  uint16_t lvl;
  uint8_t flags;
  uint8_t geneAtk, geneDef, geneSpe;
  uint32_t epoch;  // cuando llego a la caja (0 = sin reloj)
};

class Box {
public:
  void begin();                 // carga de la NVS (espacio "tpbox")
  uint8_t count() const { return n; }
  bool full() const { return n >= BOX_MAX; }
  const BoxMon &at(uint8_t i) const { return mons[i < n ? i : 0]; }
  // false si esta llena o el dex no es valido; los genes se sortean (90-110)
  bool add(int16_t dex, uint16_t lvl, bool shiny, bool caught, uint32_t epoch);
  bool take(uint8_t i, BoxMon &out);  // saca el i-esimo (para criarlo)
  bool release(uint8_t i);            // lo suelta
  int pickRandom() const;             // indice al azar, -1 si vacia
  void wipe();                        // fork KO (ko8): [nuevo comienzo]
private:
  Preferences prefs;
  BoxMon mons[BOX_MAX];
  uint8_t n = 0;
  void save();
};

class DexLog {
public:
  void begin();  // espacio "tpdex"
  void seen(int16_t dex, uint32_t epoch);   // aparece en batalla / tongsin
  void caught(int16_t dex, uint32_t epoch); // capturado (tambien cuenta como visto)
  uint32_t firstSeen(int16_t dex) const { return ok(dex) ? first[dex - 1] : 0; }
  uint16_t seenCount(int16_t dex) const { return ok(dex) ? seenN[dex - 1] : 0; }
  uint16_t caughtCount(int16_t dex) const { return ok(dex) ? caughtN[dex - 1] : 0; }
  bool wasSeen(int16_t dex) const { return seenCount(dex) > 0 || caughtCount(dex) > 0; }
  void wipe();   // fork KO (ko8): [nuevo comienzo]
private:
  Preferences prefs;
  uint32_t first[151];
  uint16_t seenN[151], caughtN[151];
  static bool ok(int16_t dex) { return dex >= 1 && dex <= 151; }
  void mark(int16_t dex, uint32_t epoch);
  void save();
};

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
#define HALL_MAX 251   // ko10.5: salon de la fama (criados hasta el final): 1 por familia de sobra
#define BOX_CAP_MAX HALL_MAX

enum : uint8_t { BOXF_SHINY = 1, BOXF_CAUGHT = 2, BOXF_RAISED = 4 };  // CAUGHT: con pokeball (si no, ganado)
// ko10.5: RAISED = criado hasta el final (despedida/soltado): corona en la caja

struct __attribute__((packed)) BoxMon {
  int16_t dex;
  uint16_t lvl;
  uint8_t flags;
  uint8_t geneAtk, geneDef, geneSpe;
  uint32_t epoch;  // cuando llego a la caja (0 = sin reloj)
};

class Box {
public:
  // ko10.5: la misma clase sirve para la caja ("tpbox", 60) y el salon de la
  // fama con corona ("tphall", 251)
  explicit Box(const char *ns = "tpbox", uint8_t cap = BOX_MAX) : ns_(ns), cap_(cap) {}
  void begin();                 // carga de la NVS (espacio ns_)
  uint8_t count() const { return n; }
  uint8_t capacity() const { return cap_; }
  bool full() const { return n >= cap_; }
  const BoxMon &at(uint8_t i) const { return mons[i < n ? i : 0]; }
  // false si esta llena o el dex no es valido; los genes se sortean (90-110)
  bool add(int16_t dex, uint16_t lvl, bool shiny, bool caught, uint32_t epoch);
  // ko10.5: el que se acaba de criar, con sus genes y la marca de criado
  bool addRaised(int16_t dex, uint16_t lvl, bool shiny, uint8_t gA, uint8_t gD, uint8_t gS, uint32_t epoch);
  bool take(uint8_t i, BoxMon &out);  // saca el i-esimo (para criarlo)
  bool release(uint8_t i);            // lo suelta
  int pickRandom() const;             // indice al azar, -1 si vacia
  void wipe();                        // fork KO (ko8): [nuevo comienzo]
private:
  Preferences prefs;
  const char *ns_;
  uint8_t cap_;
  BoxMon mons[BOX_CAP_MAX];
  uint8_t n = 0;
  void save();
};

class DexLog {
public:
  // ko10: 251 especies (gen 1 + 2). Los guardados de 151 se leen y se amplian.
  static const int N = 251;
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
  uint32_t first[N];
  uint16_t seenN[N], caughtN[N];
  static bool ok(int16_t dex) { return dex >= 1 && dex <= N; }
  void mark(int16_t dex, uint32_t epoch);
  void save();
};

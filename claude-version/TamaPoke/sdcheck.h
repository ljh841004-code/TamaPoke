#pragma once
// ko12.8: "SD 파일 점검": que ficheros de /mons (carpeta o mons.pak) hay y cuales faltan.
// Logica pura (compila en el PC): se le pasan los nombres relativos a /mons/ ("p001.bin",
// "fx/f0000.bin") y cuenta por categorias. El recorrido de la SD esta en pak.cpp.
#include <stddef.h>
#include <stdint.h>

#define SDC_DEX 251
#define SDC_FX_N 171   // ataques 1..171 (fTTSV.bin los de tipo, mNNN.bin el resto)
#define SDC_MUSIC_N 14
enum : uint8_t { SDC_SPR = 0, SDC_SPRS, SDC_BAT, SDC_BATS, SDC_CRY, SDC_FX, SDC_THUMB, SDC_STORY, SDC_MUSIC, SDC_CATS };

struct SdInv {
  uint8_t bits[SDC_CATS][32] = {};  // un bit por fichero esperado (dex 1..251 -> bit 0..250; fx id 1..171)
  void add(const char *rel);
  uint16_t have(uint8_t cat) const;
  static uint16_t need(uint8_t cat);
  bool complete(uint8_t cat) const { return have(cat) >= need(cat); }
  // nombre (relativo a /mons/) del primer fichero que falta de esa categoria; false si no falta ninguno
  bool firstMissing(uint8_t cat, char *out, size_t n) const;
};
extern const char *const SDC_MUSIC_FILES[SDC_MUSIC_N];

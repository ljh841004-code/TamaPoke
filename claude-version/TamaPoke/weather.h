#pragma once
// fork KO (ko10.1): estacion y tiempo segun la fecha y hora reales (las del WiFi/RTC).
// Logica pura (sin pantalla): test/test_box.cpp la prueba en el PC.
//
//   - lluvia: de vez en cuando, en bloques de 3 horas (mas en verano: monzon)
//   - nieve: SOLO en invierno (diciembre, enero, febrero); en invierno lo que
//     seria lluvia cae como nieve
//   - sol radiante: los dias despejados de verano (junio, julio, agosto)
//   - (ko10.1) petalos de cerezo: a ratos en primavera (marzo-mayo)
//   - (ko10.1) hojas de otono: a ratos en otono (septiembre-noviembre)
// El tiempo sale de la fecha (hash del bloque de 3 horas): no cambia al
// reiniciar ni parpadea, y dos TamaPoke a la misma hora ven lo mismo.
// Sin reloj (epoch 0) siempre despejado.
#include <stdint.h>

enum : uint8_t { WX_CLEAR = 0, WX_RAIN, WX_SNOW, WX_SUNNY, WX_BLOSSOM, WX_LEAVES };
enum : uint8_t { SEASON_SPRING = 0, SEASON_SUMMER, SEASON_AUTUMN, SEASON_WINTER };

#define WX_BLOCK_S (3u * 3600u)
#define WX_DRIFT_CHANCE 35  // % de bloques secos de primavera/otono con petalos/hojas

// fecha (ano, mes 1..12, dia 1..31) y dia de la semana (0 = domingo)
static inline void wxDate(uint32_t epoch, int *y, uint8_t *m, uint8_t *d, uint8_t *wday) {
  int32_t days = (int32_t)(epoch / 86400u);
  int32_t z = days + 719468;
  int32_t era = z / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  uint8_t mm = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
  if (y) *y = (int)yoe + era * 400 + (mm <= 2 ? 1 : 0);
  if (m) *m = mm;
  if (d) *d = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
  if (wday) *wday = (uint8_t)((days + 4) % 7);  // 1-1-1970 fue jueves
}

// ko10.4: al reves, dia 0 = 1-1-1970 (para poner la fecha a mano)
static inline uint32_t wxDaysFromDate(int y, uint8_t m, uint8_t d) {
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = (uint32_t)(y - era * 400);
  uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (uint32_t)(era * 146097 + (int32_t)doe - 719468);
}

static inline uint8_t wxDaysInMonth(int y, uint8_t m) {
  static const uint8_t D[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return D[(m - 1) % 12];
}

// mes 1..12 de una fecha en segundos (hora local, como pet.lastSeenEpoch)
static inline uint8_t wxMonth(uint32_t epoch) {
  uint8_t m;
  wxDate(epoch, nullptr, &m, nullptr, nullptr);
  return m;
}

static inline uint8_t wxSeason(uint8_t month) {
  if (month >= 3 && month <= 5) return SEASON_SPRING;
  if (month >= 6 && month <= 8) return SEASON_SUMMER;
  if (month >= 9 && month <= 11) return SEASON_AUTUMN;
  return SEASON_WINTER;
}

// probabilidad (%) de que un bloque de 3 horas sea de lluvia/nieve
static inline uint8_t wxWetChance(uint8_t season) {
  static const uint8_t P[4] = { 15, 25, 12, 20 };  // primavera, verano, otono, invierno
  return P[season & 3];
}

static inline uint32_t wxHash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU;
  x ^= x >> 15; x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

static inline uint8_t weatherAt(uint32_t epoch) {
  if (!epoch) return WX_CLEAR;
  uint8_t season = wxSeason(wxMonth(epoch));
  bool wet = wxHash(epoch / WX_BLOCK_S) % 100 < wxWetChance(season);
  if (wet) return season == SEASON_WINTER ? WX_SNOW : WX_RAIN;
  if (season == SEASON_SUMMER) return WX_SUNNY;
  // ko10.1: petalos en primavera, hojas en otono (tirada aparte, solo en su estacion)
  bool drift = wxHash(epoch / WX_BLOCK_S ^ 0x5eed1234u) % 100 < WX_DRIFT_CHANCE;
  if (drift && season == SEASON_SPRING) return WX_BLOSSOM;
  if (drift && season == SEASON_AUTUMN) return WX_LEAVES;
  return WX_CLEAR;
}

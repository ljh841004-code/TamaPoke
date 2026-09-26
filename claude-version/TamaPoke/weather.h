#pragma once
// fork KO (ko10.1): estacion y tiempo segun la fecha y hora reales (las del WiFi/RTC).
// Logica pura (sin pantalla): test/test_box.cpp la prueba en el PC.
//
//   - lluvia: de vez en cuando, en bloques de 3 horas (mas en verano: monzon)
//   - nieve: SOLO en invierno (diciembre, enero, febrero); en invierno lo que
//     seria lluvia cae como nieve
//   - sol radiante: los dias despejados de verano (junio, julio, agosto)
// El tiempo sale de la fecha (hash del bloque de 3 horas): no cambia al
// reiniciar ni parpadea, y dos TamaPoke a la misma hora ven lo mismo.
// Sin reloj (epoch 0) siempre despejado.
#include <stdint.h>

enum : uint8_t { WX_CLEAR = 0, WX_RAIN, WX_SNOW, WX_SUNNY };
enum : uint8_t { SEASON_SPRING = 0, SEASON_SUMMER, SEASON_AUTUMN, SEASON_WINTER };

#define WX_BLOCK_S (3u * 3600u)

// mes 1..12 de una fecha en segundos (hora local, como pet.lastSeenEpoch)
static inline uint8_t wxMonth(uint32_t epoch) {
  // "civil_from_days" de H. Hinnant, solo el mes
  int32_t z = (int32_t)(epoch / 86400u) + 719468;
  int32_t era = z / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  return (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
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
  return season == SEASON_SUMMER ? WX_SUNNY : WX_CLEAR;
}

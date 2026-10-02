// Batallas por turnos: ver battle.h. Solo enteros (determinista entre placas).
#include "battle.h"
#include "dex.h"
#include "weather.h"
#include "moves_data.h"
#include <string.h>

// Tabla de tipos de gen 2 (atacante x defensor), en mitades: 0 inmune, 1 poco
// eficaz, 2 normal, 4 muy eficaz. Solo hay tipo primario y no existe "volador"
// (los voladores son normales aqui). ko10: + SINIESTRO y ACERO, y los cambios de
// gen 2 (fantasma -> psiquico x2, bicho <-> veneno, hielo -> fuego x0,5).
// Orden: NOR FUE AGU PLA ELE HIE LUC VEN TIE PSI BIC ROC FAN DRA SIN ACE
static const uint8_t TYPE_CHART[PT_COUNT][PT_COUNT] = {
  /* NOR */ { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 2, 2, 1 },
  /* FUE */ { 2, 1, 1, 4, 2, 4, 2, 2, 2, 2, 4, 1, 2, 1, 2, 4 },
  /* AGU */ { 2, 4, 1, 1, 2, 2, 2, 2, 4, 2, 2, 4, 2, 1, 2, 2 },
  /* PLA */ { 2, 1, 4, 1, 2, 2, 2, 1, 4, 2, 1, 4, 2, 1, 2, 1 },
  /* ELE */ { 2, 2, 4, 1, 1, 2, 2, 2, 0, 2, 2, 2, 2, 1, 2, 2 },
  /* HIE */ { 2, 1, 1, 4, 2, 1, 2, 2, 4, 2, 2, 2, 2, 4, 2, 1 },
  /* LUC */ { 4, 2, 2, 2, 2, 4, 2, 1, 2, 1, 1, 4, 0, 2, 4, 4 },
  /* VEN */ { 2, 2, 2, 4, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 0 },
  /* TIE */ { 2, 4, 2, 1, 4, 2, 2, 4, 2, 2, 1, 4, 2, 2, 2, 4 },
  /* PSI */ { 2, 2, 2, 2, 2, 2, 4, 4, 2, 1, 2, 2, 2, 2, 0, 1 },
  /* BIC */ { 2, 1, 2, 4, 2, 2, 1, 1, 2, 4, 2, 2, 1, 2, 4, 1 },
  /* ROC */ { 2, 4, 2, 2, 2, 4, 1, 2, 1, 2, 4, 2, 2, 2, 2, 1 },
  /* FAN */ { 0, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 2, 1, 1 },
  /* DRA */ { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 1 },
  /* SIN */ { 2, 2, 2, 2, 2, 2, 1, 2, 2, 4, 2, 2, 4, 2, 1, 1 },
  /* ACE */ { 2, 1, 1, 2, 1, 4, 2, 2, 2, 2, 2, 4, 2, 2, 2, 1 },
};

uint8_t typeEff(uint8_t atkType, uint8_t defType) {
  if (atkType >= PT_COUNT || defType >= PT_COUNT) return 2;
  return TYPE_CHART[atkType][defType];
}

static uint16_t lvlCap(uint16_t lvl) { return lvl > 100 ? 100 : (lvl < 1 ? 1 : lvl); }

uint16_t battleHp(uint8_t baseHp, uint16_t lvl) {
  // x2 sobre la formula clasica: con 1 pantalla de golpe a golpe, las peleas de
  // 2-3 turnos se hacian cortisimas; asi duran 4-6 turnos
  uint32_t L = lvlCap(lvl);
  return (uint16_t)(((uint32_t)baseHp * 2 * L / 100 + L + 10) * 2);
}

Battler makeBattler(int16_t dex, uint16_t lvl, uint16_t atk, uint16_t def, uint16_t spe) {
  Battler b;
  if (dex < 1 || dex > DEX_COUNT) dex = 1;
  b.dex = dex;
  b.lvl = lvl ? lvl : 1;
  b.maxHp = b.hp = battleHp(DEX_TBL[dex].bHp, b.lvl);
  b.atk = atk ? atk : 1;
  b.def = def ? def : 1;
  b.spe = spe ? spe : 1;
  b.type = DEX_TBL[dex].ptype;
  b.guard = false;
  movesDefault(dex, b.lvl, b.mv);  // ko11.31: los suyos (el que crias los cambia despues)
  movesFillPP(b);
  return b;
}

uint32_t expForLevel(uint16_t lvl) {
  if (lvl <= 1) return 0;
  if (lvl > LEVEL_MAX) lvl = LEVEL_MAX;
  return (uint32_t)lvl * lvl * lvl;
}

uint16_t levelForExp(uint32_t exp) {
  uint16_t l = 1;
  while (l < LEVEL_MAX && expForLevel(l + 1) <= exp) l++;
  return l;
}

uint32_t battleExp(int16_t foeDex, uint16_t foeLvl) {
  if (foeDex < 1 || foeDex > DEX_COUNT) return 0;
  const DexEntry &e = DEX_TBL[foeDex];
  uint32_t yield = ((uint32_t)e.bHp + e.bAtk + e.bDef + e.bSpe) / 3;  // ~50..200
  uint32_t x = yield * lvlCap(foeLvl) * 3 / 16;  // ko10.2: x0,75 (antes /4)
  return x ? x : 1;
}

uint32_t careMinutesForLevel(uint16_t lvl) {
  if (lvl < 1) lvl = 1;
  return lvl >= LEVEL_MAX ? 0 : CARE_MIN_PER_LVL * lvl;
}

static bool hasPreEvo(int16_t dex) { return dexPrevo(dex) != 0; }

int16_t dexFirstForm(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return dex;
  for (int guard = 0; guard < 4; guard++) {
    int16_t p = dexPrevo(dex);
    if (!p) break;
    dex = p;
  }
  return dex;
}

uint8_t moveVarFor(int16_t dex, uint16_t lvl) {
  uint32_t a = (uint32_t)dex * 2654435761u ^ (uint32_t)(lvl / 5) * 40503u;
  a ^= a >> 15; a *= 0x2c1b3c6dU; a ^= a >> 12;
  return (uint8_t)(a % 3);
}

uint8_t moveTier(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  bool pre = hasPreEvo(dex), post = DEX_TBL[dex].evolvesTo != 0;
  if (!pre && !post) return DEX_TBL[dex].rarity == R_LEGENDARIO ? 2 : 1;
  if (!pre) return 0;          // primera fase (incluye bebes: Pichu)
  return post ? 1 : 2;         // intermedia / ultima
}

uint8_t evoLevel(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  const DexEntry &e = DEX_TBL[dex];
  if (!e.evolvesTo) return 0;
  bool nextEvolves = DEX_TBL[e.evolvesTo].evolvesTo != 0;  // ramas (Eevee...): todas finales
  if (nextEvolves) return 16;                          // base de 3 fases
  if (hasPreEvo(dex)) return e.evolveLevel < 20 ? 20 : e.evolveLevel;  // intermedia
  return e.evolveLevel;                                // linea de 2 fases
}

static uint16_t wildStat(uint8_t base, uint8_t gene, uint16_t lvl) {
  return (uint16_t)((uint32_t)base * gene / 100 + lvl);
}

// sube por su linea evolutiva segun el nivel y monta el combatiente
static Battler wildBattler(int16_t dex, int lv, BRng &rng) {
  for (int guard = 0; guard < 3 && DEX_TBL[dex].evolvesTo; guard++) {
    if (lv < evoLevel(dex)) break;
    int16_t opts[8];
    int k = dexEvoOptions(dex, opts);
    dex = opts[k > 1 ? rng.below(k) : 0];  // ramas: una al azar
  }
  const DexEntry &e = DEX_TBL[dex];
  uint8_t gA = 90 + rng.below(21), gD = 90 + rng.below(21), gS = 90 + rng.below(21);
  return makeBattler(dex, (uint16_t)lv, wildStat(e.bAtk, gA, lv), wildStat(e.bDef, gD, lv),
                     wildStat(e.bSpe, gS, lv));
}

static int wildLevel(uint16_t petLvl, BRng &rng) {
  int lv = (int)petLvl - 4 + (int)rng.below(6);  // -4 .. +1
  if (lv < 2) lv = 2;
  if (lv > LEVEL_MAX) lv = LEVEL_MAX;
  return lv;
}

Battler makeWild(uint16_t petLvl, BRng &rng) {
  int lv = wildLevel(petLvl, rng);
  // rareza del encuentro
  uint32_t r = rng.below(100);
  uint8_t want = (r < 70) ? (uint8_t)R_COMUN : (r < 98 || petLvl < 40) ? (uint8_t)R_RARO : (uint8_t)R_LEGENDARIO;
  int16_t pool[DEX_COUNT];
  int n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].rarity == want) pool[n++] = d;
  return wildBattler(n ? pool[rng.below(n)] : 16, lv, rng);
}

// ---------------------------------------------------------------- ko10.1: regiones
// 0 pradera 1 playa 2 bosque 3 volcan 4 montana 5 nieve 6 central 7 dojo
// 8 pantano 9 desierto 10 ruinas 11 jardin 12 cementerio 13 valle 14 ciudad 15 mina

// 1. comunes de cualquier sitio
// ko11.16: los "comunes" (el ~30 % que no es de hora ni de region) van ahora por
// region y con sentido: antes eran los mismos en todas partes (Pidgey, Rattata,
// Spearow, Sentret, Caterpie, Weedle, Meowth, Doduo) y en la playa salian Beedrill,
// Furret o Fearow. Formas base: con nivel alto evolucionan (wildBattler)
#define WILD_COMMON_N 8
static const int16_t WILD_COMMON[REGION_COUNT][WILD_COMMON_N] = {
  { 16, 19, 161, 21, 29, 32, 84, 187 },    // 0 pradera
  { 72, 98, 116, 90, 120, 54, 118, 170 },  // 1 playa
  { 10, 13, 43, 46, 165, 167, 16, 48 },    // 2 bosque
  { 37, 58, 74, 27, 66, 218, 21, 19 },     // 3 volcan
  { 74, 66, 21, 27, 41, 56, 231, 16 },     // 4 montana
  { 86, 220, 215, 225, 238, 41, 19, 16 },  // 5 nieve
  { 81, 100, 179, 19, 88, 109, 41, 52 },   // 6 central electrica
  { 66, 56, 19, 21, 52, 16, 161, 84 },     // 7 dojo
  { 60, 194, 88, 109, 23, 118, 54, 43 },   // 8 pantano
  { 27, 50, 23, 74, 104, 21, 19, 231 },    // 9 desierto
  { 41, 74, 63, 96, 177, 19, 52, 92 },     // 10 ruinas
  { 43, 69, 187, 191, 165, 16, 10, 48 },   // 11 jardin de flores
  { 92, 41, 19, 104, 88, 96, 109, 52 },    // 12 cementerio (Murkrow y Hoothoot: solo de noche)
  { 116, 21, 41, 129, 74, 84, 16, 118 },   // 13 valle del dragon
  { 19, 52, 16, 88, 109, 81, 58, 209 },    // 14 ciudad
  { 74, 41, 50, 27, 66, 81, 19, 52 },      // 15 mina
};

// regiones con pocos de su tipo: se completan con vecinos (peso x2)
static const int16_t REGION_EXTRA[REGION_COUNT][4] = {
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  { 92, 200, 0 },          // cementerio: mas fantasmas
  { 116, 129, 118, 223 },  // valle del dragon: agua dulce (Horsea, Magikarp...)
  { 0 },
  { 74, 95, 81, 0 },       // mina: Geodude, Onix, Magnemite
};

// 3. por hora (dentro de la region)
struct WildTime { uint8_t region, slots; int16_t dex; };
static const WildTime WILD_TIME[] = {
  { 0, WS_MORNING, 165 }, { 0, WS_NIGHT, 163 }, { 0, WS_NIGHT, 52 },               // pradera
  { 1, WS_MORNING, 72 }, { 1, WS_NIGHT, 170 }, { 1, WS_NIGHT, 120 },                // playa
  { 2, WS_MORNING, 191 }, { 2, WS_MORNING, 187 }, { 2, WS_NIGHT, 43 },              // bosque
  { 2, WS_NIGHT, 167 }, { 2, WS_NIGHT, 48 },
  { 3, WS_DAY, 218 }, { 3, WS_NIGHT, 228 },                                          // volcan
  { 4, WS_DAY, 74 }, { 4, WS_NIGHT, 41 },                                            // montana
  { 5, WS_DAY, 220 }, { 5, WS_NIGHT, 215 },                                          // nieve
  { 6, WS_MORNING, 179 }, { 6, WS_NIGHT, 100 },                                      // central
  { 7, WS_MORNING, 66 }, { 7, WS_MORNING, 236 }, { 7, WS_DAY, 56 },                  // dojo
  { 8, WS_DAY, 23 }, { 8, WS_DAY, 88 }, { 8, WS_NIGHT, 41 }, { 8, WS_NIGHT, 109 },   // pantano
  { 9, WS_DAY, 27 }, { 9, WS_DAY, 50 }, { 9, WS_NIGHT, 104 },                        // desierto
  { 10, WS_MORNING, 177 }, { 10, WS_NIGHT, 96 }, { 10, WS_NIGHT, 63 },               // ruinas
  { 11, WS_MORNING, 165 }, { 11, WS_MORNING, 10 }, { 11, WS_NIGHT, 167 }, { 11, WS_NIGHT, 48 },  // jardin
  { 12, WS_NIGHT, 92 }, { 12, WS_NIGHT, 200 }, { 12, WS_NIGHT, 198 },                // cementerio
  { 13, WS_MORNING, 147 }, { 13, WS_NIGHT, 116 },                                    // valle
  { 14, WS_DAY, 19 }, { 14, WS_NIGHT, 198 }, { 14, WS_NIGHT, 228 }, { 14, WS_NIGHT, 52 },  // ciudad
  { 15, WS_DAY, 81 }, { 15, WS_DAY, 95 }, { 15, WS_NIGHT, 41 },                      // mina
};

// 4. raros con condicion. region/wx/season 0xFF = cualquiera. permil = por mil
#define W_ANY 0xFF
struct WildRare { uint8_t region, slots, wx, season; int16_t dex; uint8_t permil; };
static const WildRare WILD_RARE[] = {
  // legendarios (0,5 %; y solo con tu Pokemon a nivel 40 o mas)
  { 6, WS_ANY, WX_RAIN, W_ANY, 243, 5 },            // Raikou: central + lluvia
  { 6, WS_DAY, W_ANY, W_ANY, 145, 5 },              // Zapdos: central de dia
  { 3, WS_ANY, WX_SUNNY, W_ANY, 244, 5 },           // Entei: volcan + sol de verano
  { 3, WS_ANY, W_ANY, SEASON_SUMMER, 146, 5 },      // Moltres: volcan en verano
  { 1, WS_ANY, WX_RAIN, W_ANY, 245, 5 },            // Suicune: playa + lluvia
  { 1, WS_NIGHT, W_ANY, W_ANY, 249, 5 },            // Lugia: playa de noche
  { 5, WS_ANY, WX_SNOW, W_ANY, 144, 5 },            // Articuno: nieve + nevando
  { W_ANY, WS_MORNING, WX_SUNNY, W_ANY, 250, 5 },   // Ho-Oh: mananas de sol de verano
  { 2, WS_ANY, WX_BLOSSOM, W_ANY, 251, 5 },         // Celebi: bosque + cerezos
  { 10, WS_NIGHT, W_ANY, W_ANY, 151, 5 },           // Mew: ruinas de noche
  { 15, WS_NIGHT, W_ANY, W_ANY, 150, 5 },           // Mewtwo: mina de noche
  // raros (1 %)
  { 1, WS_ANY, W_ANY, W_ANY, 131, 10 },             // Lapras: playa
  { 0, WS_DAY, W_ANY, W_ANY, 143, 10 },             // Snorlax: pradera de dia
  { 0, WS_MORNING, W_ANY, W_ANY, 113, 10 },         // Chansey: pradera por la manana
  { 0, WS_DAY, W_ANY, W_ANY, 241, 10 },             // Miltank: pradera de dia
  { 11, WS_MORNING, W_ANY, W_ANY, 123, 10 },        // Scyther: jardin por la manana
  { 11, WS_DAY, W_ANY, W_ANY, 127, 10 },            // Pinsir: jardin de dia
  { 2, WS_MORNING, W_ANY, W_ANY, 214, 10 },         // Heracross: bosque por la manana
  { 4, WS_ANY, W_ANY, W_ANY, 246, 10 },             // Larvitar: montana
  { 4, WS_ANY, W_ANY, W_ANY, 142, 10 },             // Aerodactyl: montana
  { 15, WS_ANY, W_ANY, W_ANY, 227, 10 },            // Skarmory: mina
  { 14, WS_NIGHT, W_ANY, W_ANY, 137, 10 },          // Porygon: ciudad de noche
  { 14, WS_ANY, W_ANY, W_ANY, 132, 10 },            // Ditto: ciudad
  { 14, WS_DAY, W_ANY, W_ANY, 133, 10 },            // Eevee: ciudad de dia
  { 13, WS_ANY, WX_RAIN, W_ANY, 147, 10 },          // Dratini: valle + lluvia (ademas de al alba)
  { 7, WS_ANY, W_ANY, W_ANY, 106, 10 },             // Hitmonlee: dojo
  { 7, WS_ANY, W_ANY, W_ANY, 107, 10 },             // Hitmonchan: dojo
};

uint8_t wildSlot(uint8_t hour) {
  if (hour < 6 || hour >= 20) return WS_NIGHT;
  if (hour < 10) return WS_MORNING;
  return WS_DAY;
}

static bool rareOk(const WildRare &r, uint8_t region, uint16_t petLvl, uint8_t slot, uint8_t wx,
                   uint8_t season) {
  if (r.region != W_ANY && r.region != region) return false;
  if (!(r.slots & slot)) return false;
  if (r.wx != W_ANY && r.wx != wx) return false;
  if (r.season != W_ANY && r.season != season) return false;
  if (DEX_TBL[r.dex].rarity == R_LEGENDARIO && petLvl < WILD_LEGEND_MIN_LVL) return false;
  return true;
}

// peso de "dex" dentro del grupo de su region (0 = no esta)
static uint8_t regionWeight(int16_t d, uint8_t region) {
  uint8_t w = 0;
  const DexEntry &e = DEX_TBL[d];
  if (e.biome == region) w = e.rarity == R_COMUN ? 3 : e.rarity == R_RARO ? 1 : 0;
  for (int16_t x : REGION_EXTRA[region]) if (x == d && w < 2) w = 2;
  return w;
}

static int timeCount(uint8_t region, uint8_t slot) {
  int n = 0;
  for (const WildTime &t : WILD_TIME) if (t.region == region && (t.slots & slot)) n++;
  return n;
}

// ko11.7: luna llena. Luna nueva de referencia: 6-1-2000 18:14 UTC; mes sinodico
// 29,530589 dias. Llena = edad 13,8..15,8 dias (el dia de la llena y el siguiente)
bool fullMoon(uint32_t epoch) {
  if (epoch < 947182440u) return false;
  uint64_t ageMs = ((uint64_t)(epoch - 947182440u) * 1000ull) % 2551442877ull;  // mes sinodico en ms
  uint32_t ageMin = (uint32_t)(ageMs / 60000ull);  // minutos desde la luna nueva
  return ageMin >= 19872 && ageMin <= 22752;         // 13,8 .. 15,8 dias
}

DayEvent dayEvent(uint32_t e) {
  DayEvent ev = { DEV_NONE, 0, false };
  if (!e) return ev;
  static const int8_t WD_TYPE[7] = { -1, PT_WATER, PT_FIRE, PT_GRASS, PT_ELECTRIC, PT_PSYCHIC, -1 };
  uint8_t wd = (uint8_t)((e / 86400u + 4) % 7);  // 0 = domingo
  if (wd == 0 || wd == 6) ev.kind = DEV_SHINY;
  else { ev.kind = DEV_TYPE; ev.ptype = (uint8_t)WD_TYPE[wd]; }
  uint8_t h = (uint8_t)(e / 3600 % 24);
  ev.moonNight = (h >= 20 || h < 6) && fullMoon(e);
  return ev;
}

Battler makeWildIn(uint8_t region, uint16_t petLvl, uint8_t hour, uint8_t wx, uint8_t season,
                   BRng &rng, uint8_t *group, const DayEvent *ev) {
  if (region >= REGION_COUNT) region = 0;
  uint8_t slot = wildSlot(hour);
  int lv = wildLevel(petLvl, rng);
  // 1. raros (ko11.7: luna llena de noche = legendarios x3)
  uint32_t roll = rng.below(1000), acc = 0;
  for (const WildRare &r : WILD_RARE) {
    if (!rareOk(r, region, petLvl, slot, wx, season)) continue;
    uint32_t pm = r.permil;
    if (ev && ev->moonNight && DEX_TBL[r.dex].rarity == R_LEGENDARIO) pm *= EVENT_MOON_MULT;
    acc += pm;
    if (roll < acc) { if (group) *group = WG_RARE; return wildBattler(r.dex, lv, rng); }
  }
  // ko11.7: dia de un tipo: 1 de cada 4, una forma base de ese tipo (no legendaria)
  if (ev && ev->kind == DEV_TYPE && rng.below(100) < EVENT_TYPE_PCT) {
    int16_t pool[DEX_COUNT];
    int n = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const DexEntry &e = DEX_TBL[d];
      if (e.ptype == ev->ptype && e.rarity != R_LEGENDARIO && !hasPreEvo(d)) pool[n++] = d;
    }
    if (n) { if (group) *group = WG_REGION; return wildBattler(pool[rng.below(n)], lv, rng); }
  }
  uint32_t p = rng.below(100);
  // 2. por hora
  int nt = timeCount(region, slot);
  if (p < WILD_TIME_PCT && nt) {
    int k = (int)rng.below(nt);
    for (const WildTime &t : WILD_TIME)
      if (t.region == region && (t.slots & slot) && k-- == 0) {
        if (group) *group = WG_TIME;
        return wildBattler(t.dex, lv, rng);
      }
  }
  // 3. de la region
  if (p < WILD_TIME_PCT + WILD_REGION_PCT) {
    uint32_t tot = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) tot += regionWeight(d, region);
    if (tot) {
      uint32_t k = rng.below(tot);
      for (int16_t d = 1; d <= DEX_COUNT; d++) {
        uint8_t w = regionWeight(d, region);
        if (k < w) { if (group) *group = WG_REGION; return wildBattler(d, lv, rng); }
        k -= w;
      }
    }
  }
  // 4. comunes
  if (group) *group = WG_COMMON;
  return wildBattler(WILD_COMMON[region][rng.below(WILD_COMMON_N)], lv, rng);
}

uint16_t wildPermil(int16_t dex, uint8_t region, uint16_t petLvl, uint8_t hour, uint8_t wx,
                    uint8_t season) {
  if (region >= REGION_COUNT || dex < 1 || dex > DEX_COUNT) return 0;
  uint8_t slot = wildSlot(hour);
  // fraccion en millonesimas para no perder precision
  uint32_t rareTot = 0, rareMe = 0;
  for (const WildRare &r : WILD_RARE)
    if (rareOk(r, region, petLvl, slot, wx, season)) { rareTot += r.permil; if (r.dex == dex) rareMe += r.permil; }
  uint32_t rest = 1000 - rareTot;  // por mil que no es raro
  uint64_t num = (uint64_t)rareMe * 100 * 1000;  // escala: por mil * 100 (pct) * 1000
  int nt = timeCount(region, slot);
  uint32_t timePct = nt ? WILD_TIME_PCT : 0;
  if (nt) {
    int mine = 0;
    for (const WildTime &t : WILD_TIME) if (t.region == region && (t.slots & slot) && t.dex == dex) mine++;
    num += (uint64_t)rest * timePct * 1000 * mine / nt;
  }
  uint32_t tot = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) tot += regionWeight(d, region);
  uint32_t regPct = WILD_TIME_PCT + WILD_REGION_PCT - timePct;
  if (tot) num += (uint64_t)rest * regPct * 1000 * regionWeight(dex, region) / tot;
  else regPct = 0;
  uint32_t comPct = 100 - timePct - regPct;
  for (int i = 0; i < WILD_COMMON_N; i++)
    if (WILD_COMMON[region][i] == dex) num += (uint64_t)rest * comPct * 1000 / WILD_COMMON_N;
  return (uint16_t)(num / (100 * 1000));
}

// ---------------------------------------------------------------- ko10.4: gimnasios
// region: 4 montana, 1 playa, 6 central, 2 bosque, 8 pantano, 10 ruinas, 3 volcan, 9 desierto
const GymDef GYMS[GYM_COUNT] = {
  { 4, 2, { 74, 95, 0 }, { 12, 14, 0 } },      // Brock: Geodude, Onix
  { 1, 2, { 120, 121, 0 }, { 18, 21, 0 } },    // Misty: Staryu, Starmie
  { 6, 2, { 100, 26, 0 }, { 21, 24, 0 } },     // Lt. Surge: Voltorb, Raichu
  { 2, 2, { 114, 45, 0 }, { 29, 32, 0 } },     // Erika: Tangela, Vileplume
  { 8, 3, { 109, 89, 110 }, { 37, 39, 43 } },  // Koga: Koffing, Muk, Weezing
  { 10, 3, { 64, 122, 65 }, { 38, 37, 43 } },  // Sabrina: Kadabra, Mr. Mime, Alakazam
  { 3, 3, { 77, 78, 59 }, { 40, 42, 47 } },    // Blaine: Ponyta, Rapidash, Arcanine
  { 9, 3, { 111, 34, 112 }, { 45, 45, 50 } },  // Giovanni: Rhyhorn, Nidoking, Rhydon
};

Battler makeTrainerMon(int16_t dex, uint16_t lvl) {
  if (dex < 1 || dex > DEX_COUNT) dex = 16;
  const DexEntry &e = DEX_TBL[dex];
  return makeBattler(dex, lvl, wildStat(e.bAtk, 105, lvl), wildStat(e.bDef, 105, lvl), wildStat(e.bSpe, 105, lvl));
}

ExpReward expeditionReward(uint8_t hours, uint16_t lvl, BRng &rng) {
  ExpReward r = { 0, 0, 0, 0, false };
  uint8_t u = hours >= 8 ? 4 : hours >= 4 ? 2 : 1;  // "unidades" de 2 h
  r.candy = (uint8_t)(u * 3 + (lvl >= 30 ? u : 0));  // de su familia (mas si es fuerte)
  r.balls = u;
  r.potions = (uint8_t)((u + 1) / 2);
  uint8_t rarePct = u >= 4 ? 30 : u >= 2 ? 10 : 3;
  if (rng.below(100) < rarePct) r.rare = 1;
  r.newMon = rng.below(100) < (uint32_t)(10 * u);  // 10 / 20 / 40 %
  return r;
}

uint8_t badgeCount(uint8_t badges) {
  uint8_t n = 0;
  for (; badges; badges >>= 1) n += badges & 1;
  return n;
}

// abiertas al principio: pradera, playa, bosque, montana, central, pantano, jardin,
// ciudad. Cada medalla abre una mas (las de los gimnasios 6-8 llegan a tiempo)
static const uint8_t REGION_UNLOCK_ORDER[GYM_COUNT] = { 7, 12, 5, 15, 10, 3, 9, 13 };

uint8_t regionBadgesNeeded(uint8_t region) {
  for (uint8_t i = 0; i < GYM_COUNT; i++)
    if (REGION_UNLOCK_ORDER[i] == region) return i + 1;
  return 0;
}

bool regionUnlocked(uint8_t region, uint8_t badges) {
  return badgeCount(badges) >= regionBadgesNeeded(region);
}

// ---------------------------------------------------------------- ko10.4: reto del dia
static uint32_t dayHash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}

uint8_t dailyRegion(uint32_t day) { return (uint8_t)(dayHash(day * 2654435761u) % REGION_COUNT); }

void dailyTeam(uint32_t day, uint16_t petLvl, Battler out[DAILY_TEAM]) {
  BRng rng(dayHash(day ^ 0xDA11u) | 1);
  uint8_t region = dailyRegion(day);
  uint8_t season = wxSeason(wxMonth(day * 86400u));
  for (int i = 0; i < DAILY_TEAM; i++) {
    // un poco mas fuertes cada vez: nivel +1, +2, +3 (antes de la variacion)
    uint16_t lv = petLvl + 1 + i;
    if (lv > LEVEL_MAX) lv = LEVEL_MAX;
    uint8_t hour = (uint8_t)(7 + i * 7);  // manana, tarde, noche: grupos de hora distintos
    out[i] = makeWildIn(region, lv, hour, WX_CLEAR, season, rng, nullptr);
  }
}

// ---- ko10.4: el tiempo afecta a las batallas (salvajes, gimnasios, reto del dia)
static uint8_t sBattleWx = WX_CLEAR;
void battleSetWeather(uint8_t wx) { sBattleWx = wx; }
uint8_t battleWeather() { return sBattleWx; }

// multiplicador del tiempo en medios: 3 = x1,5, 1 = x0,5, 2 = normal
uint8_t weatherMul(uint8_t wx, uint8_t moveType) {
  if (wx == WX_RAIN) return moveType == PT_WATER ? 3 : moveType == PT_FIRE ? 1 : 2;
  if (wx == WX_SUNNY) return moveType == PT_FIRE ? 3 : moveType == PT_WATER ? 1 : 2;
  if (wx == WX_SNOW) return moveType == PT_ICE ? 3 : 2;
  return 2;
}

// ---- ko11.31: movimientos
static const MoveDef STRUGGLE_DEF = { PT_NORMAL, 50, 100, 1, 0, MF_RECOIL, 0, 0, 0, 0, 0, 0, 0, 25 };

const MoveDef &moveDef(uint8_t id) {
  if (id == MOVE_STRUGGLE) return STRUGGLE_DEF;
  return MOVE_TBL[id < MOVE_N ? id : 0];
}
uint8_t moveIdTyped(uint8_t type, uint8_t tier, uint8_t var) {
  if (type >= PT_COUNT || tier > 2 || var > 2) return 0;
  return (uint8_t)(1 + type * 9 + tier * 3 + var);
}
bool moveValid(uint8_t id) { return id >= 1 && id < MOVE_N; }
bool moveIsTyped(uint8_t id) { return id >= 1 && id <= 144; }
bool moveIsStatus(uint8_t id) { return id && id != MOVE_STRUGGLE && (moveDef(id).flags & MF_STATUS); }
uint8_t moveType(uint8_t id) { return moveDef(id).type; }
uint8_t movePP(uint8_t id) { return id == MOVE_STRUGGLE ? 1 : moveDef(id).pp; }
bool moveDecode(uint8_t id, uint8_t *type, uint8_t *tier, uint8_t *var) {
  if (!moveIsTyped(id)) return false;
  uint8_t k = (uint8_t)(id - 1);
  if (type) *type = k / 9;
  if (tier) *tier = (k % 9) / 3;
  if (var) *var = k % 3;
  return true;
}

bool moveCanLearn(int16_t dex, uint8_t id) {
  if (dex < 1 || dex > DEX_COUNT || id == 0 || id >= MOVE_N) return false;
  if (id == MOVE_TACKLE) return true;
  uint8_t t, s;
  if (moveDecode(id, &t, &s, nullptr)) {
    if (s > moveTier(dex)) return false;  // los fuertes, cuando llegue a esa fase
    if (t == DEX_TBL[dex].ptype) return true;
  }
  return (MOVE_LEARN[dex][id / 8] >> (id % 8)) & 1;
}

uint8_t movePool(int16_t dex, uint8_t *out, uint8_t max) {
  uint8_t n = 0;
  for (uint16_t id = 1; id < MOVE_N && n < max; id++)
    if (moveCanLearn(dex, (uint8_t)id)) out[n++] = (uint8_t)id;
  return n;
}

uint8_t moveCount(const uint8_t mv[4]) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 4; i++) if (mv[i]) n++;
  return n;
}
bool movesHas(const uint8_t mv[4], uint8_t id) {
  for (uint8_t i = 0; i < 4; i++) if (mv[i] == id) return true;
  return false;
}
void movesFillPP(Battler &b) {
  for (uint8_t i = 0; i < 4; i++) b.pp[i] = b.mv[i] ? movePP(b.mv[i]) : 0;
}

void movesDefault(int16_t dex, uint16_t lvl, uint8_t out[4]) {
  memset(out, 0, 4);
  if (dex < 1 || dex > DEX_COUNT) return;
  uint8_t type = DEX_TBL[dex].ptype, tier = moveTier(dex);
  uint32_t h = (uint32_t)dex * 2246822519u ^ (uint32_t)(lvl / 5) * 3266489917u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  uint8_t n = 0;
  auto add = [&](uint8_t id) { if (id && n < 4 && !movesHas(out, id)) out[n++] = id; };
  add(moveIdTyped(type, tier, moveVarFor(dex, lvl)));  // el de su tipo y fase (como antes)
  // de otro tipo, de los que aprende SUBIENDO DE NIVEL en los juegos (las MT solo
  // las aprende el que crias): uno de los 3 mas fuertes de su fase o menor
  auto lvLearn = [&](uint8_t id) { return (MOVE_LEARN_LV[dex][id / 8] >> (id % 8)) & 1; };
  uint8_t cov[3] = { 0, 0, 0 };
  for (uint8_t id = 1; id <= 144; id++) {
    uint8_t t, s;
    moveDecode(id, &t, &s, nullptr);
    if (t == type || s > tier || !lvLearn(id) || !moveDef(id).pow) continue;
    for (uint8_t k = 0; k < 3; k++)
      if (!cov[k] || moveDef(id).pow > moveDef(cov[k]).pow) {
        for (uint8_t q = 2; q > k; q--) cov[q] = cov[q - 1];
        cov[k] = id;
        break;
      }
  }
  uint8_t nc = cov[2] ? 3 : cov[1] ? 2 : cov[0] ? 1 : 0;
  if (nc) add(cov[(h >> 4) % nc]);
  // uno de estado (si aprende alguno), 2 de cada 3
  uint8_t sts[MOVE_N - MOVE_STATUS0];
  uint8_t ns = 0;
  for (uint16_t id = MOVE_STATUS0; id < MOVE_N; id++)
    if (lvLearn((uint8_t)id)) sts[ns++] = (uint8_t)id;
  if (ns && (h >> 9) % 3) add(sts[(h >> 11) % ns]);
  // el resto: los de su tipo (su fase y las anteriores) y placaje
  for (int s = tier; s >= 0 && n < 4; s--)
    for (uint8_t v = 0; v < 3 && n < 4; v++) add(moveIdTyped(type, (uint8_t)s, (uint8_t)((v + h) % 3)));
  add(MOVE_TACKLE);
}

uint8_t movesMain(const Battler &b) {
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t t;
    if (moveDecode(b.mv[i], &t, nullptr, nullptr) && t == b.type) return b.mv[i];
  }
  return moveIdTyped(b.type, moveTier(b.dex), 0);
}

int8_t battleSlot(const Battler &b, BAct a) {
  if (a >= BA_M0 && a <= BA_M3) return b.mv[a - BA_M0] ? (int8_t)(a - BA_M0) : -1;
  uint8_t want = a == BA_TACKLE ? (uint8_t)MOVE_TACKLE : a == BA_TYPE ? movesMain(b) : 0;
  for (uint8_t i = 0; i < 4; i++) if (want && b.mv[i] == want) return (int8_t)i;
  return -1;
}

bool battleHasPP(const Battler &b) {
  if (!moveCount(b.mv)) return true;  // sin lista (combatientes viejos): placaje / su tipo
  for (uint8_t i = 0; i < 4; i++) if (b.mv[i] && b.pp[i]) return true;
  return false;
}

// ko11.23.3: en la historia no hay tipo volador (un tipo por Pokemon; Pidgey es normal). Para que,
// como en el original, la familia Pidgey pueda con la hierba: SOLO el mio (gStoryFlyAt = &bMe),
// su ataque de tipo hace x2 a los de hierba. El Pidgeot del rival no lo tiene
const Battler *gStoryFlyAt = nullptr;
static bool storyFly(const Battler &at, const Battler &df, uint8_t mtype) {
  return gStoryFlyAt == &at && mtype == at.type && at.dex >= 16 && at.dex <= 18 && df.type == PT_GRASS;
}

static uint32_t stgMul(uint32_t v, int8_t s) { return s >= 0 ? v * (2 + s) / 2 : v * 2 / (2 - s); }
static uint32_t effSpe(const Battler &b) {
  uint32_t v = stgMul(b.spe, b.stg[2]);
  return b.st == ST_PAR ? v / 2 : v;
}

uint8_t moveEffAgainst(uint8_t id, const Battler &df) {
  if (id == MOVE_STRUGGLE) return 2;
  return typeEff(moveDef(id).type, df.type);
}

// dano base de un movimiento (sin aleatorio ni critico), para la IA y el calculo
static uint32_t rawDamageId(const Battler &at, const Battler &df, uint8_t mid, uint8_t *effOut) {
  const MoveDef &m = moveDef(mid);
  uint8_t mtype = m.type;
  uint8_t eff = mid == MOVE_STRUGGLE ? 2 : typeEff(mtype, df.type);
  if (storyFly(at, df, mtype)) eff = 4;  // ko11.23.3
  if (effOut) *effOut = eff;
  if (m.flags & MF_STATUS) return 0;
  if (m.flags & MF_FIXLVL) return eff ? lvlCap(at.lvl) : 0;
  if (m.flags & MF_FIX) return eff ? m.pow : 0;
  uint32_t L = lvlCap(at.lvl);
  uint32_t atk = stgMul(at.atk, at.stg[0]);
  if (at.st == ST_BRN) atk /= 2;
  uint32_t def = stgMul(df.def, df.stg[1]);
  uint32_t d = ((2 * L / 5 + 2) * m.pow * atk / (def ? def : 1)) / 50 + 2;
  if (mtype == at.type && mid != MOVE_STRUGGLE) d = d * 3 / 2;  // STAB
  d = d * weatherMul(sBattleWx, mtype) / 2;  // ko10.4: lluvia / sol / nieve
  return d * eff / 2;
}

// accion -> movimiento que se usa (y su casilla, -1 = fuera de la lista: sin PP)
static uint8_t actMove(const Battler &b, BAct a, int8_t *slot) {
  // BA_TACKLE / BA_TYPE (codigo y tests de antes): ese movimiento, sin gastar PP
  if (a == BA_TACKLE || a == BA_TYPE) { *slot = -1; return a == BA_TACKLE ? (uint8_t)MOVE_TACKLE : movesMain(b); }
  int8_t k = battleSlot(b, a);
  *slot = k;
  if (k >= 0) {
    if (b.pp[k]) return b.mv[k];
    if (!battleHasPP(b)) return MOVE_STRUGGLE;
    for (uint8_t i = 0; i < 4; i++)  // sin PP en ese: el primero que tenga
      if (b.mv[i] && b.pp[i]) { *slot = (int8_t)i; return b.mv[i]; }
  }
  if (moveCount(b.mv) && !battleHasPP(b)) return MOVE_STRUGGLE;
  return a == BA_TACKLE ? (uint8_t)MOVE_TACKLE : movesMain(b);
}

static uint32_t aiDamage(const Battler &self, const Battler &foe, uint8_t id) {
  const MoveDef &m = moveDef(id);
  uint32_t d = rawDamageId(self, foe, id, nullptr) * m.acc;
  if (rawDamageId(self, foe, id, nullptr) >= foe.hp) d *= 2;  // lo deja fuera de combate
  return d;
}

static bool statusImmune(uint8_t st, const Battler &df) {
  switch (st) {
    case ST_PSN: return df.type == PT_POISON || df.type == PT_STEEL;
    case ST_BRN: return df.type == PT_FIRE;
    case ST_PAR: return df.type == PT_ELECTRIC;
    case ST_FRZ: return df.type == PT_ICE;
    default: return false;
  }
}
// los polvos no hacen nada a los de planta; onda trueno, nada a los de tierra
static bool statusMoveBlocked(uint8_t id, const Battler &df) {
  if ((id == 158 || id == 162 || id == 164 || id == 167) && df.type == PT_GRASS) return true;
  if (id == 161 && typeEff(PT_ELECTRIC, df.type) == 0) return true;
  return false;
}
static bool canAfflict(uint8_t st, const Battler &df) {
  if (st == ST_CNF) return df.cnf == 0;
  return df.st == ST_NONE && !statusImmune(st, df);
}

BAct battleAi(const Battler &self, const Battler &foe, BRng &rng, uint8_t whim) {
  if (self.hp * 100u < self.maxHp * 30u && rng.below(100) < 25) return BA_GUARD;
  if (!moveCount(self.mv)) {  // combatiente sin lista: como antes (placaje o su tipo)
    uint32_t tackle = rawDamageId(self, foe, MOVE_TACKLE, nullptr) * MOVE_TACKLE_ACC;
    uint32_t typed = rawDamageId(self, foe, movesMain(self), nullptr) * MOVE_TYPE_ACC;
    BAct best = (typed >= tackle) ? BA_TYPE : BA_TACKLE;
    if (rng.below(100) < whim) best = (best == BA_TYPE) ? BA_TACKLE : BA_TYPE;
    if (best == BA_TYPE && typed == 0 && tackle > 0) best = BA_TACKLE;
    if (best == BA_TACKLE && tackle == 0 && typed > 0) best = BA_TYPE;
    return best;
  }
  if (!battleHasPP(self)) return BA_M0;  // forcejeo
  uint32_t sc[4] = { 0, 0, 0, 0 }, best = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (!self.mv[i] || !self.pp[i] || moveIsStatus(self.mv[i])) continue;
    sc[i] = aiDamage(self, foe, self.mv[i]);
    if (sc[i] > best) best = sc[i];
  }
  uint32_t base = best ? best : 100;
  bool foeLow = foe.hp * 100u < foe.maxHp * 35u;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t id = self.mv[i];
    if (!id || !self.pp[i] || !moveIsStatus(id)) continue;
    const MoveDef &m = moveDef(id);
    if (foeLow) continue;  // ya casi lo tiene: a pegar
    if (m.ail) {
      if (!canAfflict(m.ail, foe) || statusMoveBlocked(id, foe)) continue;
      uint8_t w = m.ail == ST_SLP ? 90 : m.ail == ST_PAR ? 70 : m.ail == ST_CNF ? 50 : 60;
      sc[i] = base * w / 100 * m.acc / 100;
    } else if (m.stD > 0 && m.stSelf) {
      if (self.stg[m.st] >= 2 || self.hp * 100u < self.maxHp * 60u) continue;
      sc[i] = base * 50 / 100;
    } else if (m.stD < 0) {
      if (foe.stg[m.st] <= -2) continue;
      sc[i] = base * 35 / 100 * m.acc / 100;
    }
  }
  uint8_t pick = 0xFF;
  if (rng.below(100) < whim) {  // capricho: cualquiera que sirva
    uint8_t ok[4], k = 0;
    for (uint8_t i = 0; i < 4; i++) if (sc[i]) ok[k++] = i;
    if (k) pick = ok[rng.below(k)];
  }
  if (pick == 0xFF)
    for (uint8_t i = 0; i < 4; i++)
      if (self.mv[i] && self.pp[i] && (pick == 0xFF || sc[i] > sc[pick])) pick = i;
  return (BAct)(BA_M0 + (pick == 0xFF ? 0 : pick));
}

static void push(BEvent *ev, int maxEv, int &n, uint8_t side, uint8_t kind, uint8_t move,
                 uint8_t eff, bool crit, uint16_t dmg, const Battler &a, const Battler &b,
                 uint8_t mid = 0, int8_t val = 0) {
  if (!ev || n >= maxEv) { n++; return; }
  BEvent &e = ev[n++];
  memset(&e, 0, sizeof(e));  // sin bytes de relleno al azar: los eventos se comparan tal cual
  e.side = side; e.kind = kind; e.move = move; e.eff = eff; e.crit = crit;
  e.dmg = dmg; e.hpA = a.hp; e.hpB = b.hp; e.mid = mid; e.val = val;
}

// estado o cambio de caracteristica sobre "who" (lado ws)
static void afflict(Battler &who, uint8_t ws, uint8_t st, BRng &rng, BEvent *ev, int maxEv, int &n,
                    Battler &a, Battler &b, uint8_t mid) {
  if (st == ST_CNF) who.cnf = (uint8_t)(2 + rng.below(4));
  else {
    who.st = st;
    if (st == ST_SLP) who.stT = (uint8_t)(1 + rng.below(3));
  }
  push(ev, maxEv, n, ws, EV_STATUS, BA_M0, 2, false, 0, a, b, mid, (int8_t)st);
}
static void statChange(Battler &who, uint8_t ws, uint8_t idx, int8_t d, BEvent *ev, int maxEv, int &n,
                       Battler &a, Battler &b, uint8_t mid, bool quiet) {
  if (idx > 2) return;
  int v = who.stg[idx] + d;
  if (v > 6) v = 6;
  if (v < -6) v = -6;
  int8_t real = (int8_t)(v - who.stg[idx]);
  who.stg[idx] = (int8_t)v;
  if (real || !quiet) push(ev, maxEv, n, ws, EV_STAT, BA_M0, idx, false, 0, a, b, mid, real);
}

// ¿puede moverse este turno? (sueno, congelado, retroceso, paralisis, confusion)
static bool canAct(Battler &me, uint8_t s, BRng &rng, BEvent *ev, int maxEv, int &n, Battler &a, Battler &b) {
  if (me.recharge) {  // tras un golpe como hiperrayo: descansa un turno
    me.recharge = false;
    push(ev, maxEv, n, s, EV_CANT, BA_M0, 2, false, 0, a, b, 0, ST_RECHARGE);
    return false;
  }
  if (me.st == ST_SLP) {
    if (me.stT) me.stT--;
    if (me.stT) { push(ev, maxEv, n, s, EV_CANT, BA_M0, 2, false, 0, a, b, 0, ST_SLP); return false; }
    me.st = ST_NONE;
    push(ev, maxEv, n, s, EV_CURE, BA_M0, 2, false, 0, a, b, 0, ST_SLP);
  } else if (me.st == ST_FRZ) {
    if (rng.below(100) >= 20) { push(ev, maxEv, n, s, EV_CANT, BA_M0, 2, false, 0, a, b, 0, ST_FRZ); return false; }
    me.st = ST_NONE;
    push(ev, maxEv, n, s, EV_CURE, BA_M0, 2, false, 0, a, b, 0, ST_FRZ);
  }
  if (me.flinch) { push(ev, maxEv, n, s, EV_CANT, BA_M0, 2, false, 0, a, b, 0, ST_FLINCH); return false; }
  if (me.st == ST_PAR && rng.below(100) < 25) {
    push(ev, maxEv, n, s, EV_CANT, BA_M0, 2, false, 0, a, b, 0, ST_PAR);
    return false;
  }
  if (me.cnf) {
    me.cnf--;
    if (!me.cnf) push(ev, maxEv, n, s, EV_CURE, BA_M0, 2, false, 0, a, b, 0, ST_CNF);
    else if (rng.below(100) < 33) {
      uint32_t L = lvlCap(me.lvl);
      uint32_t d = ((2 * L / 5 + 2) * 40 * me.atk / (me.def ? me.def : 1)) / 50 + 2;
      if (d > me.hp) d = me.hp;
      me.hp -= (uint16_t)d;
      push(ev, maxEv, n, s, EV_CONFHIT, BA_M0, 2, false, (uint16_t)d, a, b);
      if (!me.hp) push(ev, maxEv, n, s, EV_FAINT, BA_M0, 2, false, 0, a, b);
      return false;
    }
  }
  return true;
}

// un movimiento de "at" (lado side) a "df"; devuelve true si alguno se debilito
static bool doMove(Battler &at, Battler &df, BAct act, uint8_t side, bool first, BRng &rng,
                   BEvent *ev, int maxEv, int &n, Battler &a, Battler &b, bool *landed = nullptr) {
  if (landed) *landed = false;
  if (!canAct(at, side, rng, ev, maxEv, n, a, b)) return at.hp == 0;
  int8_t slot;
  uint8_t mid = actMove(at, act, &slot);
  if (slot >= 0 && at.pp[slot] && mid != MOVE_STRUGGLE) at.pp[slot]--;
  const MoveDef &m = moveDef(mid);
  uint8_t mv = (act == BA_TACKLE || act == BA_TYPE) ? (uint8_t)act : (uint8_t)BA_M0;
  if (moveIsStatus(mid)) {
    push(ev, maxEv, n, side, EV_USE, mv, 2, false, 0, a, b, mid);
    if (rng.below(100) >= m.acc) { push(ev, maxEv, n, side, EV_MISS, mv, 2, false, 0, a, b, mid); return false; }
    if (m.ail) {
      if (df.guard || statusMoveBlocked(mid, df) || !canAfflict(m.ail, df)) {
        push(ev, maxEv, n, side ^ 1, EV_NOEFFECT, mv, 2, false, 0, a, b, mid);
        return false;
      }
      afflict(df, side ^ 1, m.ail, rng, ev, maxEv, n, a, b, mid);
    } else if (m.stD) {
      if (m.stSelf) statChange(at, side, m.st, m.stD, ev, maxEv, n, a, b, mid, false);
      else if (df.guard) push(ev, maxEv, n, side ^ 1, EV_NOEFFECT, mv, 2, false, 0, a, b, mid);
      else statChange(df, side ^ 1, m.st, m.stD, ev, maxEv, n, a, b, mid, false);
    }
    return false;
  }
  uint8_t acc = m.acc;
  if (rng.below(100) >= acc) {
    push(ev, maxEv, n, side, EV_MISS, mv, 2, false, 0, a, b, mid);
    return false;
  }
  uint8_t eff;
  uint32_t d = rawDamageId(at, df, mid, &eff);
  bool crit = false;
  bool fixed = (m.flags & (MF_FIX | MF_FIXLVL)) != 0;
  if (eff) {
    if (!fixed) {
      crit = rng.below((m.flags & MF_HICRIT) ? 8 : 16) == 0;
      if (crit) d = d * 3 / 2;
      d = d * (85 + rng.below(16)) / 100;
    }
    if (df.guard) d /= 2;
    if (d < 1) d = 1;
  } else {
    d = 0;
  }
  if (d > df.hp) d = df.hp;
  df.hp -= (uint16_t)d;
  if (landed) *landed = d > 0;
  push(ev, maxEv, n, side, EV_HIT, mv, eff, crit, (uint16_t)d, a, b, mid);
  if (m.flags & MF_RECHARGE) at.recharge = true;
  // drenaje / retroceso
  if (d && (m.flags & MF_DRAIN) && at.hp < at.maxHp) {
    uint32_t h = d * m.drain / 100;
    if (h < 1) h = 1;
    if (at.hp + h > at.maxHp) h = at.maxHp - at.hp;
    at.hp += (uint16_t)h;
    push(ev, maxEv, n, side, EV_DRAIN, mv, 2, false, (uint16_t)h, a, b, mid);
  }
  if (d && (m.flags & MF_RECOIL)) {
    uint32_t r = mid == MOVE_STRUGGLE ? at.maxHp / 4 : d * m.drain / 100;
    if (r < 1) r = 1;
    if (r > at.hp) r = at.hp;
    at.hp -= (uint16_t)r;
    push(ev, maxEv, n, side, EV_RECOIL, mv, 2, false, (uint16_t)r, a, b, mid);
  }
  if (df.hp == 0) {
    push(ev, maxEv, n, side ^ 1, EV_FAINT, mv, 2, false, 0, a, b, mid);
    if (at.hp == 0) push(ev, maxEv, n, side, EV_FAINT, mv, 2, false, 0, a, b, mid);
    return true;
  }
  if (at.hp == 0) {
    push(ev, maxEv, n, side, EV_FAINT, mv, 2, false, 0, a, b, mid);
    return true;
  }
  // efectos secundarios (solo si el golpe entro)
  if (d) {
    if (m.ail && m.ailCh && rng.below(100) < m.ailCh && !df.guard && canAfflict(m.ail, df))
      afflict(df, side ^ 1, m.ail, rng, ev, maxEv, n, a, b, mid);
    if (m.stD && m.stCh && rng.below(100) < m.stCh) {
      if (m.stSelf) statChange(at, side, m.st, m.stD, ev, maxEv, n, a, b, mid, true);
      else statChange(df, side ^ 1, m.st, m.stD, ev, maxEv, n, a, b, mid, true);
    }
    if (first && m.flinch && rng.below(100) < m.flinch) df.flinch = true;
  }
  return false;
}

uint8_t catchChance(const Battler &foe) {
  uint32_t hpPct = foe.maxHp ? (uint32_t)foe.hp * 100 / foe.maxHp : 100;
  if (hpPct > 100) hpPct = 100;
  int ch = 85 - (int)hpPct * 65 / 100;   // vida llena 20%, casi debilitado ~85%
  uint8_t rar = (foe.dex >= 1 && foe.dex <= DEX_COUNT) ? DEX_TBL[foe.dex].rarity : (uint8_t)R_COMUN;
  if (rar == R_RARO) ch = ch * 2 / 3;
  else if (rar == R_LEGENDARIO) ch = ch / 4;
  if (foe.st == ST_SLP || foe.st == ST_FRZ) ch = ch * 3 / 2;  // ko11.31: dormido o congelado, mas facil
  else if (foe.st) ch = ch * 5 / 4;
  if (ch < 3) ch = 3;
  if (ch > 90) ch = 90;
  return (uint8_t)ch;
}

// ko11.8: contraataque tras protegerse con exito: placaje a media potencia,
// sin fallo ni critico (sigue contando el tipo: un fantasma no lo nota)
uint16_t counterDamage(const Battler &at, const Battler &df, BRng &rng, uint8_t *effOut) {
  uint8_t eff;
  uint32_t d = rawDamageId(at, df, MOVE_TACKLE, &eff);
  if (effOut) *effOut = eff;
  if (!eff) return 0;
  d = d * (85 + rng.below(16)) / 100 / 2;
  if (d < 1) d = 1;
  return (uint16_t)(d > df.hp ? df.hp : d);
}

static bool wildOnly(BAct a) { return a == BA_RUN || a == BA_POTION || a == BA_BALL; }
static bool isAttack(BAct a) { return a == BA_TACKLE || a == BA_TYPE || (a >= BA_M0 && a <= BA_M3); }

// fin del turno: veneno y quemadura
static void endOfTurn(Battler &a, Battler &b, BEvent *ev, int maxEv, int &n) {
  Battler *side[2] = { &a, &b };
  for (uint8_t s = 0; s < 2; s++) {
    Battler &me = *side[s];
    me.flinch = false;
    if (!me.hp || !a.hp || !b.hp || (me.st != ST_PSN && me.st != ST_BRN)) continue;
    uint32_t d = me.maxHp / (me.st == ST_PSN ? 8 : 16);
    if (d < 1) d = 1;
    if (d > me.hp) d = me.hp;
    me.hp -= (uint16_t)d;
    push(ev, maxEv, n, s, EV_STDMG, BA_M0, 2, false, (uint16_t)d, a, b, 0, (int8_t)me.st);
    if (!me.hp) push(ev, maxEv, n, s, EV_FAINT, BA_M0, 2, false, 0, a, b);
  }
}

int battleTurn(Battler &a, Battler &b, BAct actA, BAct actB, BRng &rng,
               BEvent *ev, int maxEv, bool canRun) {
  int n = 0;
  if (actA >= BA_COUNT) actA = BA_TACKLE;
  if (actB >= BA_COUNT) actB = BA_TACKLE;
  if (!canRun) {
    if (wildOnly(actA)) actA = BA_TACKLE;
    if (wildOnly(actB)) actB = BA_TACKLE;
  }
  if (a.hp == 0 || b.hp == 0) return 0;
  Battler *side[2] = { &a, &b };
  BAct act[2] = { actA, actB };

  // prioridad: huir y protegerse van antes que cualquier ataque
  for (uint8_t s = 0; s < 2; s++) {
    Battler &me = *side[s], &op = *side[s ^ 1];
    if (act[s] == BA_RUN) {
      int ch = 50 + ((int)effSpe(me) - (int)effSpe(op)) / 2;
      if (ch < 25) ch = 25;
      if (ch > 95) ch = 95;
      if ((int)rng.below(100) < ch) {
        push(ev, maxEv, n, s, EV_RUN_OK, BA_RUN, 2, false, 0, a, b);
        return n < maxEv ? n : maxEv;
      }
      push(ev, maxEv, n, s, EV_RUN_FAIL, BA_RUN, 2, false, 0, a, b);
    } else if (act[s] == BA_POTION) {  // fork KO: cura la mitad de la vida
      uint16_t heal = me.maxHp / 2;
      if (heal < 1) heal = 1;
      if (me.hp + heal > me.maxHp) heal = me.maxHp - me.hp;
      me.hp += heal;
      push(ev, maxEv, n, s, EV_HEAL, BA_POTION, 2, false, heal, a, b);
    } else if (act[s] == BA_BALL) {    // fork KO: intento de captura
      if (rng.below(100) < catchChance(op)) {
        push(ev, maxEv, n, s, EV_CATCH, BA_BALL, 2, false, 0, a, b);
        return n < maxEv ? n : maxEv;
      }
      push(ev, maxEv, n, s, EV_BREAK, BA_BALL, 2, false, 0, a, b);
    } else if (act[s] == BA_GUARD) {
      me.guard = true;
      uint16_t heal = me.maxHp / 10;
      if (heal < 1) heal = 1;
      if (me.hp + heal > me.maxHp) heal = me.maxHp - me.hp;
      me.hp += heal;
      push(ev, maxEv, n, s, EV_GUARD, BA_GUARD, 2, false, heal, a, b);
    }
  }

  // ataques: primero la prioridad del movimiento, luego el mas rapido (empate: a suertes)
  int8_t pr[2];
  for (uint8_t s = 0; s < 2; s++) {
    int8_t k;
    pr[s] = isAttack(act[s]) ? moveDef(actMove(*side[s], act[s], &k)).prio : 0;
  }
  uint32_t sa = effSpe(a), sb = effSpe(b);
  uint8_t first = pr[0] != pr[1] ? (pr[0] > pr[1] ? 0 : 1)
                : (sa > sb) ? 0 : (sb > sa) ? 1 : (uint8_t)rng.below(2);
  bool over = false;
  for (uint8_t k = 0; k < 2 && !over; k++) {
    uint8_t s = k ? (first ^ 1) : first;
    if (!isAttack(act[s])) continue;
    Battler &me = *side[s], &op = *side[s ^ 1];
    if (me.hp == 0) continue;
    bool landed = false;
    if (doMove(me, op, act[s], s, k == 0, rng, ev, maxEv, n, a, b, &landed)) { over = true; break; }
    // ko11.8: el otro se protegia y el golpe entro: devuelve un golpe (los dos lados igual)
    if (landed && op.guard && op.hp > 0 && me.hp > 0) {
      uint8_t eff;
      uint16_t d = counterDamage(op, me, rng, &eff);
      if (eff) {
        me.hp -= d;
        push(ev, maxEv, n, s ^ 1, EV_COUNTER, BA_TACKLE, eff, false, d, a, b, MOVE_TACKLE);
        if (me.hp == 0) {
          push(ev, maxEv, n, s, EV_FAINT, BA_TACKLE, 2, false, 0, a, b);
          over = true;
        }
      }
    }
  }
  if (!over && a.hp && b.hp) endOfTurn(a, b, ev, maxEv, n);
  a.flinch = b.flinch = false;
  a.guard = b.guard = false;
  return n < maxEv ? n : maxEv;
}

// ko11.20: cambio voluntario: el rival ataca y yo no hago nada
int battleFoeOnly(Battler &a, Battler &b, BAct actB, BRng &rng, BEvent *ev, int maxEv) {
  int n = 0;
  if (a.hp == 0 || b.hp == 0) return 0;
  if (!isAttack(actB)) actB = BA_TACKLE;
  bool landed = false;
  doMove(b, a, actB, 1, true, rng, ev, maxEv, n, a, b, &landed);
  a.flinch = b.flinch = false;
  a.guard = b.guard = false;
  return n < maxEv ? n : maxEv;
}

// ko11.31: al acabar la batalla se pasan los estados y los cambios de caracteristicas
void battleClearVolatile(Battler &b) {
  b.st = b.stT = b.cnf = 0;
  b.recharge = false;
  b.flinch = b.guard = false;
  b.stg[0] = b.stg[1] = b.stg[2] = 0;
}

uint8_t battleAuto(Battler a, Battler b, uint32_t seed, BEvent *ev, int maxEv, int *nEv) {
  // tongsin: los dos aparatos simulan la misma batalla; su reloj puede no
  // coincidir, asi que aqui siempre hace buen tiempo (si no, se desincronizan)
  uint8_t keepWx = sBattleWx;
  sBattleWx = WX_CLEAR;
  BRng rng(seed);
  int n = 0;
  for (int t = 0; t < BATTLE_AUTO_TURNS && a.hp && b.hp; t++) {
    BAct x = battleAi(a, b, rng);
    BAct y = battleAi(b, a, rng);
    BEvent tmp[BATTLE_MAX_EVENTS];
    int k = battleTurn(a, b, x, y, rng, tmp, BATTLE_MAX_EVENTS, false);
    for (int i = 0; i < k; i++) {
      if (ev && n < maxEv) ev[n] = tmp[i];
      n++;
    }
  }
  if (nEv) *nEv = (n < maxEv) ? n : maxEv;
  sBattleWx = keepWx;
  if (a.hp == 0) return 1;
  if (b.hp == 0) return 0;
  // tope de turnos: gana quien conserve mas vida (en proporcion)
  uint32_t pa = (uint32_t)a.hp * 1000 / a.maxHp, pb = (uint32_t)b.hp * 1000 / b.maxHp;
  if (pa != pb) return pa > pb ? 0 : 1;
  return (uint8_t)(seed & 1);
}

// ---- ko10.11: revanchas y liga
const uint8_t GYM_TYPE[GYM_COUNT] = { PT_ROCK, PT_WATER, PT_ELECTRIC, PT_GRASS,
                                      PT_POISON, PT_PSYCHIC, PT_FIRE, PT_GROUND };

static uint16_t trainerLvl(int32_t lv) { return (uint16_t)(lv < 3 ? 3 : lv > LEVEL_MAX ? LEVEL_MAX : lv); }
static uint16_t baseTotal(int16_t d) {
  const DexEntry &e = DEX_TBL[d];
  return (uint16_t)e.bHp + e.bAtk + e.bDef + e.bSpe;
}

uint8_t gymRematchTeam(uint8_t gym, uint16_t petLvl, uint32_t seed, Battler out[REMATCH_MAX]) {
  if (gym >= GYM_COUNT) gym = 0;
  uint8_t type = GYM_TYPE[gym];
  int16_t pool[DEX_COUNT];
  uint16_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].ptype == type && DEX_TBL[d].rarity != R_LEGENDARIO) pool[n++] = d;
  BRng rng(seed | 1);
  uint8_t k = (uint8_t)(3 + rng.below(2));  // 3 o 4
  if (k > n) k = (uint8_t)n;
  int16_t pick[REMATCH_MAX];
  for (uint8_t i = 0; i < k; i++) {  // sin repetir (Fisher-Yates parcial)
    uint16_t j = (uint16_t)(i + rng.below(n - i));
    int16_t t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    pick[i] = pool[i];
  }
  // el mas fuerte, al final (el "as" del lider)
  for (uint8_t i = 0; i < k; i++)
    for (uint8_t j = i + 1; j < k; j++)
      if (baseTotal(pick[j]) < baseTotal(pick[i])) { int16_t t = pick[i]; pick[i] = pick[j]; pick[j] = t; }
  for (uint8_t i = 0; i < k; i++) {
    int32_t lv = (int32_t)petLvl - 1 + i + (i + 1 == k ? 1 : 0);
    out[i] = makeTrainerMon(pick[i], trainerLvl(lv));
  }
  return k;
}

void championTeam(uint16_t petLvl, uint32_t seed, Battler out[CHAMP_TEAM]) {
  int16_t pool[DEX_COUNT];
  uint16_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    const DexEntry &e = DEX_TBL[d];
    if (e.rarity != R_LEGENDARIO && e.evolvesTo == 0 && baseTotal(d) >= 330) pool[n++] = d;
  }
  BRng rng(seed | 1);
  int16_t pick[CHAMP_TEAM];
  uint8_t k = 0;
  uint16_t typesUsed = 0;
  for (int guard = 0; k < CHAMP_TEAM && guard < 400; guard++) {
    int16_t d = pool[rng.below(n)];
    bool dup = false;
    for (uint8_t i = 0; i < k; i++) if (pick[i] == d) dup = true;
    // tipos distintos mientras se pueda (las primeras 300 tiradas)
    if (dup || (guard < 300 && (typesUsed & (1u << DEX_TBL[d].ptype)))) continue;
    typesUsed |= (uint16_t)(1u << DEX_TBL[d].ptype);
    pick[k++] = d;
  }
  for (uint8_t i = 0; i < k; i++)
    for (uint8_t j = i + 1; j < k; j++)
      if (baseTotal(pick[j]) < baseTotal(pick[i])) { int16_t t = pick[i]; pick[i] = pick[j]; pick[j] = t; }
  for (uint8_t i = 0; i < CHAMP_TEAM; i++)
    out[i] = makeTrainerMon(pick[i < k ? i : 0], trainerLvl((int32_t)petLvl + 2 + (i * 4) / (CHAMP_TEAM - 1)));
}

// ---- ko11.20: equipo contra entrenadores
Battler makeBoxBattler(int16_t dex, uint16_t lvl, uint16_t petLvl, uint8_t gA, uint8_t gD, uint8_t gS) {
  if (dex < 1 || dex > DEX_COUNT) dex = 16;
  if (lvl > petLvl) lvl = petLvl;
  if (lvl < 1) lvl = 1;
  const DexEntry &e = DEX_TBL[dex];
  return makeBattler(dex, lvl, wildStat(e.bAtk, gA, lvl), wildStat(e.bDef, gD, lvl), wildStat(e.bSpe, gS, lvl));
}

uint8_t pickNextFoe(const Battler *team, uint8_t from, uint8_t n, uint8_t myType) {
  uint8_t best = from;
  int bs = -10;
  for (uint8_t i = from; i < n; i++) {
    int sc = typeMatch(team[i].type, myType);
    if (sc > bs) { bs = sc; best = i; }
  }
  return best;
}

int8_t typeMatch(uint8_t mine, uint8_t foe) {
  uint8_t out = typeEff(mine, foe), in = typeEff(foe, mine);
  if (out > 2 && in <= 2) return 1;
  if (in > 2 && out <= 2) return -1;
  if (out < 2 && in >= 2) return -1;  // mis golpes no le hacen casi nada
  return 0;
}

// ---- ko10.11: salvajes de tu talla
uint32_t battlerPower(const Battler &b) { return (uint32_t)b.maxHp + b.atk + b.def + b.spe; }

// ko11: la forma que toca a ese nivel (Caterpie Lv57 -> Butterfree, Dragonite Lv25 -> Dratini)
static int16_t formForLevel(int16_t dex, uint16_t lv, BRng &rng) {
  for (int g = 0; g < 3; g++) {  // bajar mientras no llegue al nivel de su evolucion
    int16_t pre = dexPrevo(dex);
    if (pre <= 0) break;
    uint8_t need = evoLevel(pre);
    if (need && lv < need) dex = pre;
    else break;
  }
  for (int g = 0; g < 3 && DEX_TBL[dex].evolvesTo; g++) {  // subir si ya le toca
    uint8_t need = evoLevel(dex);
    if (!need || lv < need) break;
    int16_t opts[8];
    int k = dexEvoOptions(dex, opts);
    dex = opts[k > 1 ? rng.below(k) : 0];
  }
  return dex;
}

static int bestLevelFor(int16_t dex, uint32_t target, int lo, int hi) {
  const DexEntry &e = DEX_TBL[dex];
  int best = hi;
  uint32_t bestDiff = 0xFFFFFFFFu;
  for (int lv = lo; lv <= hi; lv++) {
    if (lv < 2 || lv > LEVEL_MAX) continue;
    uint32_t p = (uint32_t)battleHp(e.bHp, (uint16_t)lv) + wildStat(e.bAtk, 100, (uint16_t)lv) +
                 wildStat(e.bDef, 100, (uint16_t)lv) + wildStat(e.bSpe, 100, (uint16_t)lv);
    uint32_t d = p > target ? p - target : target - p;
    if (d < bestDiff) { bestDiff = d; best = lv; }
  }
  return best;
}

void wildMatchPower(Battler &foe, const Battler &me, BRng &rng) {
  uint32_t target = battlerPower(me) * (90 + rng.below(16)) / 100;  // 90-105 %
  int anchor = foe.lvl;
  int16_t dex = foe.dex;
  int best = anchor;
  // ko11: nivel de tu talla y luego la forma de ese nivel; si la forma cambia,
  // se vuelve a ajustar el nivel (siempre dentro de -6..+10 del original).
  // ko11.12: y nunca mas de 3 niveles por encima del tuyo (un Pichu Lv9 muy
  // entrenado veia salvajes Lv17-20); lo que falte de fuerza va a las stats
  int hi = anchor + 10;
  if (hi > (int)me.lvl + WILD_LVL_OVER) hi = (int)me.lvl + WILD_LVL_OVER;
  int lo = anchor - 6;
  if (lo > hi) lo = hi;
  for (int pass = 0; pass < 3; pass++) {
    best = bestLevelFor(dex, target, lo, hi);
    int16_t nd = formForLevel(dex, (uint16_t)best, rng);
    if (nd == dex) break;
    dex = nd;
  }
  const DexEntry &e = DEX_TBL[dex];
  uint8_t gA = 90 + rng.below(21), gD = 90 + rng.below(21), gS = 90 + rng.below(21);
  foe = makeBattler(dex, (uint16_t)best, wildStat(e.bAtk, gA, (uint16_t)best),
                    wildStat(e.bDef, gD, (uint16_t)best), wildStat(e.bSpe, gS, (uint16_t)best));
  // ko11.12: con el nivel topado, las stats se escalan hasta tu talla (x0,6..x2)
  uint32_t p = battlerPower(foe);
  if (p && target) {
    uint32_t f = target * 100 / p;
    if (f > 200) f = 200;
    if (f < 60) f = 60;
    if (f > 104 || f < 96) {
      auto sc = [f](uint16_t v) -> uint16_t { uint32_t r = (uint32_t)v * f / 100; return r < 1 ? 1 : (r > 60000 ? 60000 : (uint16_t)r); };
      foe.maxHp = foe.hp = sc(foe.maxHp);
      foe.atk = sc(foe.atk);
      foe.def = sc(foe.def);
      foe.spe = sc(foe.spe);
    }
  }
}

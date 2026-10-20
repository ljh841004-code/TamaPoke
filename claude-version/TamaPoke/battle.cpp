// Batallas por turnos: ver battle.h. Solo enteros (determinista entre placas).
#include "battle.h"
#include "dex.h"
#include "weather.h"
#include "moves_data.h"
#include "dex_special.h"
#include "dex_types.h"
#include "sdcheck.h"
#include <string.h>
#include <stdio.h>
static_assert(SDC_FX_N == MOVE_N - 1, "sdcheck.h: SDC_FX_N = numero de movimientos");
#include <string.h>

// ko12.10: tabla de tipos de 2a generacion con VOLADOR (atacante x defensor), en mitades: 0 inmune,
// 1 poco eficaz, 2 normal, 4 muy eficaz. Con dos tipos se multiplican (cuartos: 0..16, normal 4).
// Orden: NOR FUE AGU PLA ELE HIE LUC VEN TIE PSI BIC ROC FAN DRA SIN ACE VOL
static const uint8_t TYPE_CHART[PT_BATTLE_N][PT_BATTLE_N] = {
  /* NOR */ { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 2, 2, 1, 2 },
  /* FUE */ { 2, 1, 1, 4, 2, 4, 2, 2, 2, 2, 4, 1, 2, 1, 2, 4, 2 },
  /* AGU */ { 2, 4, 1, 1, 2, 2, 2, 2, 4, 2, 2, 4, 2, 1, 2, 2, 2 },
  /* PLA */ { 2, 1, 4, 1, 2, 2, 2, 1, 4, 2, 1, 4, 2, 1, 2, 1, 1 },
  /* ELE */ { 2, 2, 4, 1, 1, 2, 2, 2, 0, 2, 2, 2, 2, 1, 2, 2, 4 },
  /* HIE */ { 2, 1, 1, 4, 2, 1, 2, 2, 4, 2, 2, 2, 2, 4, 2, 1, 4 },
  /* LUC */ { 4, 2, 2, 2, 2, 4, 2, 1, 2, 1, 1, 4, 0, 2, 4, 4, 1 },
  /* VEN */ { 2, 2, 2, 4, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 0, 2 },
  /* TIE */ { 2, 4, 2, 1, 4, 2, 2, 4, 2, 2, 1, 4, 2, 2, 2, 4, 0 },
  /* PSI */ { 2, 2, 2, 2, 2, 2, 4, 4, 2, 1, 2, 2, 2, 2, 0, 1, 2 },
  /* BIC */ { 2, 1, 2, 4, 2, 2, 1, 1, 2, 4, 2, 2, 1, 2, 4, 1, 1 },
  /* ROC */ { 2, 4, 2, 2, 2, 4, 1, 2, 1, 2, 4, 2, 2, 2, 2, 1, 4 },
  /* FAN */ { 0, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 2, 1, 1, 2 },
  /* DRA */ { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 1, 2 },
  /* SIN */ { 2, 2, 2, 2, 2, 2, 1, 2, 2, 4, 2, 2, 4, 2, 1, 1, 2 },
  /* ACE */ { 2, 1, 1, 2, 1, 4, 2, 2, 2, 2, 2, 4, 2, 2, 2, 1, 2 },
  /* VOL */ { 2, 2, 2, 4, 1, 2, 4, 2, 2, 2, 4, 1, 2, 2, 2, 1, 2 },
};

uint8_t typeEff(uint8_t atkType, uint8_t defType) {
  if (atkType >= PT_BATTLE_N || defType >= PT_BATTLE_N) return 2;
  return TYPE_CHART[atkType][defType];
}
uint8_t typeEffQ(uint8_t atkType, uint8_t t1, uint8_t t2) {
  uint8_t a = typeEff(atkType, t1);
  return (t2 < PT_BATTLE_N && t2 != t1) ? a * typeEff(atkType, t2) : a * 2;
}
uint8_t effCat(uint8_t q) { return q == 0 ? 0 : q < 4 ? 1 : q == 4 ? 2 : 4; }
bool battlerHasType(const Battler &b, uint8_t t) { return b.type == t || b.type2 == t; }

static uint16_t lvlCap(uint16_t lvl) { return lvl > LEVEL_MAX ? LEVEL_MAX : (lvl < 1 ? 1 : lvl); }  // ko12.9.23: hasta 150

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
  b.type2 = DEX_T2[dex] == b.type ? 0xFF : DEX_T2[dex];  // ko12.10: segundo tipo (volador...)
  b.guard = false;
  movesDefault(dex, b.lvl, b.mv);  // ko11.31: los suyos (el que crias los cambia despues)
  movesFillPP(b);
  // ko12.10: poder oculto de la especie (el que crias: de sus genes, en la UI). Nunca normal
  b.hpType = (uint8_t)(1 + ((uint32_t)dex * 2654435761u >> 24) % (PT_BATTLE_N - 1));
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
  uint32_t x = yield * lvlCap(foeLvl) / 12;  // ko12.10: x4/9 (antes 3/16): tope 100 en lo mismo que antes 150
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
  return e.evolveLevel;  // ko12.10: el de los juegos (Caterpie 7, Pidgey 18, Dratini 30...; piedra 30, cambio 40)
}

uint16_t battleStat(uint8_t base, uint8_t gene, uint16_t lvl, uint8_t tr) {
  uint32_t L = lvlCap(lvl);
  uint32_t iv = gene <= 90 ? 0 : gene >= 110 ? 31 : (uint32_t)(gene - 90) * 31 / 20;
  uint32_t ev = (uint32_t)(tr > 100 ? 100 : tr) * 63 / 100;  // EV/4
  return (uint16_t)((2u * base + iv + ev) * L / 100 + 5);
}
static uint16_t wildStat(uint8_t base, uint8_t gene, uint16_t lvl) { return battleStat(base, gene, lvl, 0); }

// sube por su linea evolutiva segun el nivel y monta el combatiente
// ko12.9.6: a veces (WILD_STAY_PCT) se queda en una fase anterior aunque el nivel ya diera para
// evolucionar: asi las formas intermedias (Metapod, Dragonair...) salen tambien con el bicho alto
static Battler wildBattler(int16_t dex, int lv, BRng &rng) {
  int16_t chain[4];
  int nc = 0;
  chain[nc++] = dex;
  for (int guard = 0; guard < 3 && DEX_TBL[dex].evolvesTo; guard++) {
    if (lv < evoLevel(dex)) break;
    int16_t opts[8];
    int k = dexEvoOptions(dex, opts);
    dex = opts[k > 1 ? rng.below(k) : 0];  // ramas: una al azar
    chain[nc++] = dex;
  }
  if (nc > 1 && rng.below(100) < WILD_STAY_PCT) dex = chain[rng.below(nc - 1)];
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
#define W_WET 0xFE  // ko12.9.6: lluvia o nieve (cualquier estacion; en invierno la lluvia es nieve)
#define W_DRY 0xFD  // ko12.9.6: sin lluvia ni nieve (despejado, sol, petalos, hojas)
struct WildRare { uint8_t region, slots, wx, season; int16_t dex; uint8_t permil; };
static const WildRare WILD_RARE[] = {
  // legendarios (0,5 %; y solo con tu Pokemon a nivel 40 o mas)
  // ko12.9.6: ninguno depende de la estacion (antes Moltres/Entei/Ho-Oh solo en verano, Articuno en
  // invierno y Celebi en primavera): W_WET = lluvia o nieve, W_DRY = sin lluvia ni nieve
  { 6, WS_ANY, W_WET, W_ANY, 243, 5 },              // Raikou: central + lluvia/nieve
  { 6, WS_DAY, W_ANY, W_ANY, 145, 5 },              // Zapdos: central de dia
  { 3, WS_ANY, W_DRY, W_ANY, 244, 5 },              // Entei: volcan sin lluvia
  { 3, WS_DAY, W_ANY, W_ANY, 146, 5 },              // Moltres: volcan de dia
  { 1, WS_ANY, W_WET, W_ANY, 245, 5 },              // Suicune: playa + lluvia/nieve
  { 1, WS_NIGHT, W_ANY, W_ANY, 249, 5 },            // Lugia: playa de noche
  { 5, WS_ANY, W_WET, W_ANY, 144, 5 },              // Articuno: nieve + lluvia/nieve
  { W_ANY, WS_MORNING, W_DRY, W_ANY, 250, 5 },      // Ho-Oh: mananas sin lluvia
  { 2, WS_ANY, W_DRY, W_ANY, 251, 5 },              // Celebi: bosque sin lluvia
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
  { 13, WS_ANY, W_WET, W_ANY, 147, 10 },            // Dratini: valle + lluvia/nieve (ademas de al alba)
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
  if (r.wx == W_WET) { if (wx != WX_RAIN && wx != WX_SNOW) return false; }
  else if (r.wx == W_DRY) { if (wx == WX_RAIN || wx == WX_SNOW) return false; }
  else if (r.wx != W_ANY && r.wx != wx) return false;
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

uint8_t wildTier(int16_t dex, bool rareGroup, bool shiny) {
  if (shiny || (dex >= 1 && dex <= DEX_COUNT && DEX_TBL[dex].rarity == R_LEGENDARIO)) return WT_SPECIAL;
  return rareGroup ? WT_RARE : WT_NORMAL;
}

uint8_t wildJoinPct(uint8_t tier) {
  return tier >= WT_SPECIAL ? WILD_JOIN_SPECIAL_PCT : tier == WT_RARE ? WILD_JOIN_RARE_PCT : WILD_JOIN_PCT;
}

uint32_t wildRareMult(int16_t dex, const DayEvent *ev) {
  if (!ev) return 1;
  bool legend = DEX_TBL[dex].rarity == R_LEGENDARIO;
  uint32_t m = 1;
  if (ev->moonNight && legend) m *= EVENT_MOON_MULT;   // ko11.7: luna llena de noche = legendarios x3
  if (ev->lure) {                                      // ko12.9.11
    m *= legend ? LURE_LEGEND_MULT : LURE_RARE_MULT;
    if (ev->isNew && ev->isNew(dex)) m *= LURE_NEW_MULT;
    uint32_t cap = legend ? LURE_LEGEND_CAP : LURE_RARE_CAP;
    if (m > cap) m = cap;
  }
  return m;
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
    uint32_t pm = r.permil * wildRareMult(r.dex, ev);
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

// ---------------------------------------------------------------- ko12.9.6: pistas de la pokedex
static int hintAdd(WildHint *out, int n, int max, uint8_t region, uint8_t slots, uint8_t wx, bool rare) {
  if (slots == WS_ANY && wx == W_ANY) {  // siempre en ese sitio: sobran las otras pistas de ese sitio
    int m = 0;
    for (int i = 0; i < n; i++) if (out[i].region != region) out[m++] = out[i];
    n = m;
    if (n < max) out[n++] = { region, WS_ANY, W_ANY, rare };
    return n;
  }
  for (int i = 0; i < n; i++) {
    WildHint &h = out[i];
    if (h.region != region) continue;
    if (h.slots == WS_ANY && h.wx == W_ANY) return n;  // ya sale ahi siempre
    if (slots == WS_ANY && wx == W_ANY) { h.slots = WS_ANY; h.wx = W_ANY; h.rare = rare && h.rare; return n; }
    if (h.wx == wx) { h.slots |= slots; h.rare = rare && h.rare; return n; }
  }
  if (n < max) out[n++] = { region, slots, wx, rare };
  return n;
}

static int hintsDirect(int16_t d, WildHint *out, int max) {
  int n = 0;
  for (const WildRare &r : WILD_RARE)
    if (r.dex == d) n = hintAdd(out, n, max, r.region, r.slots, r.wx, true);
  for (const WildTime &t : WILD_TIME)
    if (t.dex == d) n = hintAdd(out, n, max, t.region, t.slots, W_ANY, false);
  for (uint8_t reg = 0; reg < REGION_COUNT; reg++) {
    bool any = regionWeight(d, reg) > 0;
    for (int i = 0; i < WILD_COMMON_N && !any; i++) any = WILD_COMMON[reg][i] == d;
    if (any) n = hintAdd(out, n, max, reg, WS_ANY, W_ANY, false);
  }
  return n;
}

int wildHints(int16_t dex, WildHint *out, int max, int16_t *viaBase, uint8_t *minLvl) {
  if (viaBase) *viaBase = 0;
  if (minLvl) *minLvl = 0;
  if (dex < 1 || dex > DEX_COUNT || max <= 0) return 0;
  uint8_t need = DEX_TBL[dex].rarity == R_LEGENDARIO ? WILD_LEGEND_MIN_LVL : 0;
  int16_t d = dex;
  for (int guard = 0; guard < 4; guard++) {
    int n = hintsDirect(d, out, max);
    if (n) {
      if (viaBase && d != dex) *viaBase = d;
      if (minLvl) *minLvl = need;
      return n;
    }
    int16_t p = dexPrevo(d);
    if (!p) break;
    // el salvaje sale con el nivel de tu bicho -4..+1: para verlo ya evolucionado hace falta
    // que tu bicho tenga (nivel de evolucion - 1) o mas
    uint8_t el = evoLevel(p);
    if (d == dex && el > 1) need = (uint8_t)(el - 1);
    d = p;
  }
  return 0;
}

// ---------------------------------------------------------------- ko10.4: gimnasios
// region: 4 montana, 1 playa, 6 central, 2 bosque, 8 pantano, 10 ruinas, 3 volcan, 9 desierto
// ko12.10: con los movimientos originales (Raichu y Starmie ya saben golpes fuertes a nivel 1, Alakazam
// pega con psiquico) algunos niveles bajan 1-2 (test/sim: como antes, ~30-50 % solo y sin pociones)
const GymDef GYMS[GYM_COUNT] = {
  { 4, 2, { 74, 95, 0 }, { 12, 14, 0 } },      // Brock: Geodude, Onix
  { 1, 2, { 120, 121, 0 }, { 16, 18, 0 } },    // Misty: Staryu, Starmie (ko12.8: -1/-2 nv: con at. especial Starmie pega mucho mas)
  { 6, 2, { 100, 26, 0 }, { 18, 19, 0 } },     // Lt. Surge: Voltorb, Raichu
  { 2, 2, { 114, 45, 0 }, { 29, 32, 0 } },     // Erika: Tangela, Vileplume
  { 8, 3, { 109, 89, 110 }, { 36, 38, 41 } },  // Koga: Koffing, Muk, Weezing
  { 10, 3, { 64, 122, 65 }, { 36, 35, 41 } },  // Sabrina: Kadabra, Mr. Mime, Alakazam
  { 3, 3, { 77, 78, 59 }, { 40, 42, 47 } },    // Blaine: Ponyta, Rapidash, Arcanine
  { 9, 3, { 111, 34, 112 }, { 43, 43, 48 } },  // Giovanni: Rhyhorn, Nidoking, Rhydon
};

Battler makeTrainerMon(int16_t dex, uint16_t lvl) {
  if (dex < 1 || dex > DEX_COUNT) dex = 16;
  const DexEntry &e = DEX_TBL[dex];
  return makeBattler(dex, lvl, wildStat(e.bAtk, 98, lvl), wildStat(e.bDef, 98, lvl), wildStat(e.bSpe, 98, lvl));  // ko12.10: genes 98 (antes 105): con la formula de los juegos los genes pesan mas
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
// ---- ko10.4: tiempo de la batalla. ko12.10: danza lluvia / dia soleado / tormenta arena lo cambian 5 turnos
static uint8_t sBattleWx = WX_CLEAR;   // el de verdad (reloj / internet)
static uint8_t sWxOv = WX_CLEAR, sWxT = 0;  // el de los movimientos y los turnos que le quedan
void battleSetWeather(uint8_t wx) { sBattleWx = wx; sWxT = 0; }  // (el de los movimientos va en Battler::fieldWx*)
uint8_t battleWeather() { return sWxT ? sWxOv : sBattleWx; }

// multiplicador del tiempo en medios: 3 = x1,5, 1 = x0,5, 2 = normal
uint8_t weatherMul(uint8_t wx, uint8_t moveType) {
  if (wx == WX_RAIN) return moveType == PT_WATER ? 3 : moveType == PT_FIRE ? 1 : 2;
  if (wx == WX_SUNNY) return moveType == PT_FIRE ? 3 : moveType == PT_WATER ? 1 : 2;
  if (wx == WX_SNOW) return moveType == PT_ICE ? 3 : 2;
  return 2;
}

// ---- ko11.31: movimientos. ko12.10: los 251 originales (moves_data.h)
const MoveDef &moveDef(uint8_t id) { return MOVE_TBL[id < MOVE_N ? id : 0]; }
static_assert(sizeof(MOVE_TBL) / sizeof(MOVE_TBL[0]) == MOVE_N, "moves_data.h");
static_assert(TM_N == TM_COUNT && TM_HM0 == TM_HM_FIRST, "moves_data.h: MT/MO");
const char *moveNameKo(uint8_t id) { return MOVE_KO[id < MOVE_N ? id : 0]; }
const char *moveNameEn(uint8_t id) { return MOVE_EN[id < MOVE_N ? id : 0]; }
const char *moveDescKo(uint8_t id) { return MOVE_DESC_KO[id < MOVE_N ? id : 0]; }
const char *moveDescEn(uint8_t id) { return MOVE_DESC_EN[id < MOVE_N ? id : 0]; }
uint8_t tmMove(uint8_t tm) { return tm < TM_N ? TM_MOVE[tm] : 0; }
bool tmCompatible(int16_t dex, uint8_t tm) {
  return dex >= 1 && dex <= DEX_COUNT && tm < TM_N && ((TM_BITS[dex][tm / 8] >> (tm % 8)) & 1);
}
const char *tmLabel(uint8_t tm, bool ko) { return tm < TM_N ? (ko ? TM_NAME[tm] : TM_NAME_EN[tm]) : ""; }
int8_t tmOfMove(uint8_t id) {
  for (uint8_t i = 0; i < TM_N; i++) if (TM_MOVE[i] == id) return (int8_t)i;
  return -1;
}
bool moveValid(uint8_t id) { return id >= 1 && id < MOVE_N && id != MOVE_STRUGGLE; }
bool moveIsStatus(uint8_t id) { return id && id < MOVE_N && (moveDef(id).flags & MF_STATUS); }
uint8_t moveType(uint8_t id) { return moveDef(id).type; }
// ko12.6: PP x2 (aqui casi siempre lucha uno solo contra varios)
uint8_t movePP(uint8_t id) {
  if (id == MOVE_STRUGGLE) return 1;
  uint16_t p = (uint16_t)moveDef(id).pp * PP_MULT;
  return (uint8_t)(p > 99 ? 99 : p);
}

bool moveCanLearn(int16_t dex, uint8_t id) {
  if (dex < 1 || dex > DEX_COUNT || !moveValid(id)) return false;
  return (MOVE_LEARN[dex][id / 8] >> (id % 8)) & 1;
}
uint8_t movePool(int16_t dex, uint8_t *out, uint8_t max) {
  uint8_t n = 0;
  for (uint16_t id = 1; id < MOVE_N && n < max; id++)
    if (moveCanLearn(dex, (uint8_t)id)) out[n++] = (uint8_t)id;
  return n;
}
uint8_t moveLearnset(int16_t dex, uint8_t *lv, uint8_t *mv, uint8_t max) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  uint8_t n = 0;
  for (uint16_t i = MOVE_LS_OFF[dex - 1]; i < MOVE_LS_OFF[dex] && n < max; i++, n++) {
    if (lv) lv[n] = MOVE_LS[i][0];
    if (mv) mv[n] = MOVE_LS[i][1];
  }
  return n;
}
uint8_t moveAtLevel(int16_t dex, uint16_t lvl, uint8_t k) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  for (uint16_t i = MOVE_LS_OFF[dex - 1]; i < MOVE_LS_OFF[dex]; i++)
    if (MOVE_LS[i][0] == lvl && !k--) return MOVE_LS[i][1];
  return 0;
}

// ko12.10.15: el primer golpe que aprende despues de lvl (el suyo o, si antes evoluciona, el de la
// forma nueva). 0 = ninguno. *atLv = a que nivel
uint8_t moveNextAttack(int16_t dex, uint16_t lvl, uint8_t *atLv) {
  for (uint16_t L = lvl + 1; dex >= 1 && dex <= DEX_COUNT && L <= LEVEL_MAX; L++) {
    for (uint8_t k = 0; k < 8; k++) {
      uint8_t id = moveAtLevel(dex, L, k);
      if (!id) break;
      if (!moveIsStatus(id)) { if (atLv) *atLv = (uint8_t)L; return id; }
    }
    if (DEX_TBL[dex].evolvesTo && evoLevel(dex) == L) {
      int16_t nx = DEX_TBL[dex].evolvesTo;
      uint8_t mv[4];
      movesDefault(nx, L, mv);
      for (uint8_t i = 0; i < 4; i++)
        if (mv[i] && !moveIsStatus(mv[i])) { if (atLv) *atLv = (uint8_t)L; return mv[i]; }
      dex = nx;
    }
  }
  return 0;
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

// ko12.10: 2a generacion: especial o fisico segun el TIPO (fuego, agua, planta, electrico, hielo,
// psiquico, dragon y siniestro: especiales)
static bool typeIsSpecial(uint8_t t) {
  return t == PT_FIRE || t == PT_WATER || t == PT_GRASS || t == PT_ELECTRIC || t == PT_ICE ||
         t == PT_PSYCHIC || t == PT_DRAGON || t == PT_DARK;
}
bool moveIsSpecial(uint8_t id) { return id && id < MOVE_N && !moveIsStatus(id) && typeIsSpecial(moveDef(id).type); }

// ko12.8: hay en mv otro del mismo tipo (y clase) que lo deja sin sentido
bool moveDominated(const uint8_t mv[4], uint8_t id) {
  if (!id || moveIsStatus(id)) return false;
  const MoveDef &c = moveDef(id);
  if (c.flags & ~MF_NOMISS) return false;
  if (c.ail || c.stD || c.flinch || c.prio || c.fx || c.hits || !c.pow) return false;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t o = mv[i];
    if (!o || o == id || moveIsStatus(o)) continue;
    const MoveDef &e = moveDef(o);
    if (e.type != c.type || !e.pow) continue;
    if (e.flags & (MF_RECOIL | MF_RECHARGE)) continue;
    if (e.fx) continue;
    if (e.stSelf && e.stD < 0) continue;
    if (e.pow >= c.pow && e.acc >= c.acc) return true;
  }
  return false;
}

// ko12.10: lo de antes -> el original. Si esa especie no lo puede aprender: uno que si pueda, del mismo
// tipo y potencia parecida (si es de estado, uno de estado); si no hay, 0
uint8_t moveMigrate(int16_t dex, uint8_t oldId) {
  if (oldId == 255) return 0;  // forcejeo de antes
  if (oldId >= MOVE_OLD_N) return 0;
  uint8_t id = MOVE_OLD2NEW[oldId];
  if (!id || moveCanLearn(dex, id)) return id;
  const MoveDef &w = moveDef(id);
  bool st = moveIsStatus(id);
  uint8_t best = 0;
  int bs = 1 << 30;
  for (uint16_t k = 1; k < MOVE_N; k++) {
    if (!moveCanLearn(dex, (uint8_t)k) || moveIsStatus((uint8_t)k) != st) continue;
    const MoveDef &m = moveDef((uint8_t)k);
    int sc = (m.type == w.type ? 0 : 1000) + (int)(m.pow > w.pow ? m.pow - w.pow : w.pow - m.pow);
    if (st) sc = (m.type == w.type ? 0 : 1000) + (m.ail == w.ail ? 0 : 100) + (m.stD == w.stD ? 0 : 50);
    if (sc < bs) { bs = sc; best = (uint8_t)k; }
  }
  return bs < 1000 ? best : 0;  // de otro tipo no: mejor que lo rellene su lista
}

// como en los juegos: los 4 ultimos que aprende subiendo de nivel hasta ese nivel (sin repetir).
// ko12.10: lo que aprendio de pequeno cuenta (Raichu Lv19 sabe lo de Pikachu); lo que su forma evolucionada
// trae "a nivel 1" lo aprende al evolucionar (Raichu: rayo, a 30). Y al menos un golpe que haga dano
void movesDefault(int16_t dex, uint16_t lvl, uint8_t out[4]) {
  memset(out, 0, 4);
  if (dex < 1 || dex > DEX_COUNT) return;
  struct E { uint8_t lv, id; };
  E all[96];
  uint8_t n = 0;
  int16_t d = dex;
  for (int g = 0; g < 3 && d; g++) {
    int16_t pre = dexPrevo(d);
    uint8_t at1 = pre ? evoLevel(pre) : 1;  // los "de nivel 1" de una forma evolucionada: al evolucionar
    for (uint16_t i = MOVE_LS_OFF[d - 1]; i < MOVE_LS_OFF[d] && n < 96; i++) {
      uint8_t lv = MOVE_LS[i][0];
      if (lv <= 1 && pre && at1 > 1) lv = at1;
      all[n++] = { lv, MOVE_LS[i][1] };
    }
    d = pre;
  }
  // por nivel (a igual nivel, el orden de la lista)
  for (uint8_t i = 1; i < n; i++)
    for (uint8_t j = i; j > 0 && all[j - 1].lv > all[j].lv; j--) { E t = all[j]; all[j] = all[j - 1]; all[j - 1] = t; }
  uint8_t k = 0;
  for (int i = (int)n - 1; i >= 0 && k < 4; i--) {
    if (all[i].lv > lvl || movesHas(out, all[i].id)) continue;
    out[k++] = all[i].id;
  }
  bool dmg = false;
  for (uint8_t i = 0; i < k; i++) if (!moveIsStatus(out[i])) dmg = true;
  if (!dmg) {  // el ultimo golpe que sepa, en lugar del de estado mas antiguo
    for (int i = (int)n - 1; i >= 0; i--)
      if (all[i].lv <= lvl && !moveIsStatus(all[i].id) && !movesHas(out, all[i].id)) {
        if (k < 4) out[k++] = all[i].id;
        else out[3] = all[i].id;
        break;
      }
  }
  // orden de aprendizaje (el mas antiguo primero, como en la pantalla del juego)
  for (uint8_t i = 0; i < k / 2; i++) { uint8_t t = out[i]; out[i] = out[k - 1 - i]; out[k - 1 - i] = t; }
  if (!k) out[0] = MOVE_TACKLE;
}

bool moveFxInfo(uint8_t id, uint8_t *type, uint8_t *tier, uint8_t *var) {
  if (!id || id >= MOVE_N || moveIsStatus(id)) return false;
  const MoveDef &m = moveDef(id);
  uint8_t p = m.pow ? m.pow : 70;
  if (type) *type = m.type < PT_COUNT ? m.type : (uint8_t)0xFF;
  if (tier) *tier = p >= 100 ? 2 : p >= 65 ? 1 : 0;
  if (var) *var = (uint8_t)(id % 3);
  return true;
}
uint8_t moveTypeFor(const Battler &at, uint8_t id) {
  if (moveDef(id).fx == MX_HIDDENPOWER) return at.hpType;
  return moveDef(id).type;
}

uint8_t movesMainOf(int16_t dex, const uint8_t mv[4]) {
  uint8_t ty = (dex >= 1 && dex <= DEX_COUNT) ? DEX_TBL[dex].ptype : 0, t2 = dexType2(dex);
  uint8_t best = 0, bestAny = 0;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t id = mv[i];
    if (!id || moveIsStatus(id)) continue;
    uint8_t t = moveDef(id).type, p = moveDef(id).pow ? moveDef(id).pow : 60;
    if ((t == ty || t == t2) && (!best || p > moveDef(best).pow)) best = id;
    if (!bestAny || p > moveDef(bestAny).pow) bestAny = id;
  }
  if (best) return best;
  if (bestAny) return bestAny;
  return mv[0] ? mv[0] : (uint8_t)MOVE_TACKLE;
}
uint8_t movesMain(const Battler &b) { return movesMainOf(b.dex, b.mv); }

int8_t battleSlot(const Battler &b, BAct a) {
  if (a >= BA_M0 && a <= BA_M3) return b.mv[a - BA_M0] ? (int8_t)(a - BA_M0) : -1;
  uint8_t want = a == BA_TACKLE ? (uint8_t)MOVE_TACKLE : a == BA_TYPE ? movesMain(b) : 0;
  for (uint8_t i = 0; i < 4; i++) if (want && b.mv[i] == want) return (int8_t)i;
  return -1;
}

bool battleHasPP(const Battler &b) {
  if (!moveCount(b.mv)) return true;  // sin lista (combatientes viejos): placaje / su tipo
  for (uint8_t i = 0; i < 4; i++) if (b.mv[i] && b.pp[i] && !(b.disT && b.mv[i] == b.disMv)) return true;
  return false;
}

// ko11.23.3: en la historia la familia Pidgey (solo la mia) x2 contra hierba (ko12.10: ya vuela de verdad)
const Battler *gStoryFlyAt = nullptr;
static bool storyFly(const Battler &at, const Battler &df, uint8_t mtype) {
  return gStoryFlyAt == &at && mtype == at.type && at.dex >= 16 && at.dex <= 18 && df.type == PT_GRASS;
}

static uint32_t stgMul(uint32_t v, int8_t s) { return s >= 0 ? v * (2 + s) / 2 : v * 2 / (2 - s); }
static uint32_t effSpe(const Battler &b) {
  uint32_t v = stgMul(b.spe, b.stg[STG_SPE]);
  return b.st == ST_PAR ? v / 2 : v;
}

// eficacia en cuartos de un tipo contra el defensor (profecia: el fantasma recibe normal y lucha)
static uint8_t effQ(uint8_t t, const Battler &df) {
  uint8_t t1 = df.type, t2 = df.type2;
  if (df.foresight && (t == PT_NORMAL || t == PT_FIGHT)) {
    if (t1 == PT_GHOST) t1 = t2 != 0xFF ? t2 : (uint8_t)PT_NORMAL;
    if (t2 == PT_GHOST) t2 = 0xFF;
  }
  return typeEffQ(t, t1, t2);
}

uint8_t moveEffAgainst(uint8_t id, const Battler &df) {
  if (id == MOVE_STRUGGLE || !id) return 2;
  return effCat(effQ(moveDef(id).type, df));
}

// ko12.8: ataque / defensa especial (de su ataque / defensa en la proporcion de su especie, a su nivel)
uint32_t spStat(const Battler &b, uint32_t v, bool defense) {
  int16_t d = b.xform ? b.xform : b.dex;
  if (d < 1 || d > DEX_COUNT) return v;
  uint32_t L = lvlCap(b.lvl);
  uint32_t phys = defense ? DEX_TBL[d].bDef : DEX_TBL[d].bAtk;
  uint32_t sp = defense ? DEX_SPD[d] : DEX_SPA[d];
  uint32_t r = v * (sp + L) / (phys + L);
  return r ? r : 1;
}

static bool sFleeOk = false;    // ko12.10: rugido / remolino / teletransporte acaban la batalla (solo salvajes)
void battleSetFleeMoves(bool ok) { sFleeOk = ok; }
static uint8_t sMagLv = 4;      // magnitud que salio (4..10)
static uint8_t sPresent = 40;   // presente: 40 / 80 / 120
static bool sPursuitX2 = false; // persecucion contra el que se va
static uint8_t sHitK = 1;       // triple patada: golpe numero k

// potencia de ese movimiento ahora (los de potencia variable)
static uint16_t movePower(const Battler &at, const Battler &df, uint8_t id) {
  const MoveDef &m = moveDef(id);
  switch (m.fx) {
    case MX_FLAIL: {
      uint32_t r = at.maxHp ? (uint32_t)at.hp * 48 / at.maxHp : 48;
      return r <= 1 ? 200 : r <= 4 ? 150 : r <= 9 ? 100 : r <= 16 ? 80 : r <= 32 ? 40 : 20;
    }
    case MX_RETURN: { uint16_t p = (uint16_t)(at.bond * 2 / 5); return p ? p : 1; }
    case MX_FRUSTRATION: { uint16_t p = (uint16_t)((255 - at.bond) * 2 / 5); return p ? p : 1; }
    case MX_HIDDENPOWER: return 70;
    case MX_MAGNITUDE: {
      static const uint8_t P[11] = { 0, 0, 0, 0, 10, 30, 50, 70, 90, 110, 150 };
      uint16_t p = P[sMagLv <= 10 ? sMagLv : 7];
      return df.invul == 2 ? p * 2 : p;
    }
    case MX_PRESENT: return sPresent;
    case MX_ROLLOUT: {
      uint16_t p = (uint16_t)(m.pow << (at.rollT < 5 ? at.rollT : 4));
      return at.curled ? p * 2 : p;
    }
    case MX_FURYCUTTER: return (uint16_t)(m.pow << (at.furyN < 2 ? at.furyN : 2));  // 40, 80, 160
    case MX_TRIPLEKICK: return (uint16_t)(m.pow * sHitK);
    case MX_TWISTER: return df.invul == 1 ? m.pow * 2 : m.pow;
    case MX_QUAKE: return df.invul == 2 ? m.pow * 2 : m.pow;
    case MX_STOMP: return df.minimized ? m.pow * 2 : m.pow;
    case MX_PURSUIT: return sPursuitX2 ? m.pow * 2 : m.pow;
    case MX_SOLAR: { uint8_t w = battleWeather(); return (w == WX_RAIN || w == WX_SAND) ? m.pow / 2 : m.pow; }
    default: return m.pow;
  }
}
uint16_t movePowerShown(const Battler &at, uint8_t id) {
  Battler df;
  if (moveIsStatus(id)) return 0;
  if (moveDef(id).fx == MX_MAGNITUDE || moveDef(id).fx == MX_PRESENT) return 0;
  return movePower(at, df, id);
}

// dano base (sin aleatorio). crit: como en 2a gen, x2 y sin contar los cambios que le van mal ni las pantallas
static uint32_t rawDamage(const Battler &at, const Battler &df, uint8_t mid, uint8_t *effOut, bool crit,
                          uint32_t psyRoll = 0) {
  const MoveDef &m = moveDef(mid);
  uint8_t mtype = moveTypeFor(at, mid);
  uint8_t q = mid == MOVE_STRUGGLE || m.fx == MX_BIDE || m.fx == MX_FUTURESIGHT ? 4 : effQ(mtype, df);
  if (m.fx == MX_FUTURESIGHT) q = 4;  // 2a gen: premonicion no tiene tipo
  if (storyFly(at, df, mtype) && q < 8) q = 8;  // ko11.23.3
  uint8_t eff = effCat(q);
  if (effOut) *effOut = eff;
  if (m.flags & MF_STATUS) return 0;
  uint32_t L = lvlCap(at.lvl);
  switch (m.fx) {  // dano fijo (cuenta la inmunidad, no la eficacia)
    case MX_SONICBOOM: return eff ? 20 : 0;
    case MX_DRAGONRAGE: return eff ? 40 : 0;
    case MX_LEVEL: return eff ? L : 0;
    case MX_PSYWAVE: return eff ? (psyRoll ? psyRoll : L) : 0;
    case MX_SUPERFANG: return eff ? (df.hp / 2 ? df.hp / 2 : 1) : 0;
    case MX_OHKO: return eff ? df.hp : 0;
    default: break;
  }
  uint32_t pw = movePower(at, df, mid);
  if (!pw) return 0;
  bool sp = typeIsSpecial(mtype);  // ko12.8 / 2a gen: segun el tipo
  int8_t sa = at.stg[sp ? STG_SPA : STG_ATK], sd = df.stg[sp ? STG_SPD : STG_DEF];
  if (crit) { if (sa < 0) sa = 0; if (sd > 0) sd = 0; }
  uint32_t atk = stgMul(sp ? spStat(at, at.atk, false) : at.atk, sa);
  if (at.st == ST_BRN && !sp) atk /= 2;
  uint32_t def = stgMul(sp ? spStat(df, df.def, true) : df.def, sd);
  if (!crit && ((sp && df.lscrT) || (!sp && df.reflT))) def *= 2;
  if (m.fx == MX_SELFKO) def = def / 2 ? def / 2 : 1;  // 2a gen: autodestruccion / explosion
  uint32_t d = ((2 * L / 5 + 2) * pw * atk / (def ? def : 1)) / 50 + 2;
  if (crit) d *= 2;
  if (battlerHasType(at, mtype) && mid != MOVE_STRUGGLE && m.fx != MX_BEATUP) d = d * 3 / 2;  // STAB
  d = d * weatherMul(battleWeather(), mtype) / 2;  // lluvia / sol / nieve
  return d * q / 4;
}

bool battleResting(const Battler &b, uint8_t id) { return id && b.restT && b.restMv == id; }

uint8_t battleLockedMove(const Battler &b) {
  if (b.chargeMv) return b.chargeMv;
  if (b.rampT) return b.rampMv;
  if (b.rollT) return 205;  // rodar
  if (b.bideT) return 117;  // venganza
  return 0;
}
bool battleLocked(const Battler &b) { return battleLockedMove(b) != 0; }
bool battleCanEscape(const Battler &b) { return !b.trapT && !b.meanLook; }

static bool usable(const Battler &b, uint8_t i) {
  return b.mv[i] && b.pp[i] && !battleResting(b, b.mv[i]) && !(b.disT && b.mv[i] == b.disMv);
}

// accion -> movimiento que se usa (y su casilla, -1 = sin gastar PP). cont = sigue con uno que ya empezo
static uint8_t actMove(const Battler &b, BAct a, int8_t *slot, bool *cont = nullptr) {
  if (cont) *cont = false;
  uint8_t lk = battleLockedMove(b);
  if (lk) { *slot = -1; if (cont) *cont = true; return lk; }
  // otra vez: repite ese (si le quedan PP)
  if (b.encT && (a == BA_TACKLE || a == BA_TYPE || (a >= BA_M0 && a <= BA_M3)))
    for (uint8_t i = 0; i < 4; i++)
      if (b.mv[i] == b.encMv && b.pp[i]) { *slot = (int8_t)i; return b.encMv; }
  // BA_TACKLE / BA_TYPE (codigo y tests de antes): ese movimiento, sin gastar PP
  if (a == BA_TACKLE || a == BA_TYPE) {
    *slot = -1;
    if (a == BA_TACKLE || battleResting(b, movesMain(b))) return MOVE_TACKLE;
    return movesMain(b);
  }
  int8_t k = battleSlot(b, a);
  *slot = k;
  if (k >= 0) {
    if (usable(b, (uint8_t)k)) return b.mv[k];
    if (!battleHasPP(b)) { *slot = -1; return MOVE_STRUGGLE; }
    for (uint8_t i = 0; i < 4; i++)  // sin PP / anulado / descansando: el primero que se pueda
      if (usable(b, i)) { *slot = (int8_t)i; return b.mv[i]; }
    *slot = -1;  // solo le queda el que descansa: placaje sin gastar PP
    return MOVE_TACKLE;
  }
  if (moveCount(b.mv) && !battleHasPP(b)) return MOVE_STRUGGLE;
  return a == BA_TACKLE ? (uint8_t)MOVE_TACKLE : movesMain(b);
}

static bool statusImmune(uint8_t st, const Battler &df) {
  switch (st) {
    case ST_PSN: return battlerHasType(df, PT_POISON) || battlerHasType(df, PT_STEEL);
    case ST_BRN: return battlerHasType(df, PT_FIRE);
    case ST_PAR: return false;  // 2a gen: los electricos si se paralizan (onda trueno no les hace nada a los de tierra)
    case ST_FRZ: return battlerHasType(df, PT_ICE);
    default: return false;
  }
}
// onda trueno no hace nada a los de tierra (es electrica); drenadoras, a los de planta
static bool statusMoveBlocked(uint8_t id, const Battler &df) {
  const MoveDef &m = moveDef(id);
  if (m.ail && m.type == PT_ELECTRIC && effQ(PT_ELECTRIC, df) == 0) return true;
  if (m.fx == MX_LEECHSEED && battlerHasType(df, PT_GRASS)) return true;
  return false;
}
static bool canAfflict(uint8_t st, const Battler &df) {
  if (st == ST_CNF) return df.cnf == 0 && !df.safeT;
  return df.st == ST_NONE && !statusImmune(st, df) && !df.safeT;
}

// ---- IA
static int stgSum(const Battler &b) {
  int s = 0;
  for (uint8_t i = 0; i < STG_N; i++) s += b.stg[i];
  return s;
}
static uint32_t aiDamage(const Battler &self, const Battler &foe, uint8_t id) {
  const MoveDef &m = moveDef(id);
  uint32_t raw;
  if (m.fx == MX_MAGNITUDE || m.fx == MX_PRESENT) {
    Battler t = self;
    sMagLv = 7; sPresent = 60;
    raw = rawDamage(t, foe, id, nullptr, false);
  } else {
    raw = rawDamage(self, foe, id, nullptr, false);
  }
  if (m.hits) raw = raw * ((m.hits >> 4) + (m.hits & 15)) / 2;
  if (m.fx == MX_TRIPLEKICK) raw = raw * 6;  // 10 + 20 + 30
  uint32_t acc = m.acc ? m.acc : 100;
  if (m.fx == MX_THUNDER && battleWeather() == WX_RAIN) acc = 100;
  uint32_t d = raw * acc;
  switch (m.fx) {
    case MX_OHKO: return foe.lvl > self.lvl ? 0 : (uint32_t)foe.hp * 30;
    case MX_DREAMEATER: if (foe.st != ST_SLP) return 0; break;
    case MX_SNORE: if (self.st != ST_SLP) return 0; break;
    case MX_COUNTER: case MX_MIRRORCOAT: return (uint32_t)foe.atk * 25;  // a ciegas: algo
    case MX_BIDE: return raw ? raw : (uint32_t)self.maxHp / 4 * 40;
    case MX_FUTURESIGHT: if (foe.futT) return 0; d = d * 70 / 100; break;
    case MX_SELFKO: if (self.hp * 100u > self.maxHp * 30u && raw < foe.hp) return d / 5; break;
    case MX_CHARGE: d = d * 55 / 100; break;
    case MX_SOLAR: if (battleWeather() != WX_SUNNY) d = d * 60 / 100; break;
    case MX_FLY: case MX_DIG: d = d * 75 / 100; break;
    case MX_FALSESWIPE: if (raw >= foe.hp) return 1; break;
    case MX_CRASH: d = d * 90 / 100; break;
    default: break;
  }
  if (raw >= foe.hp) return d * 2;  // lo deja fuera de combate: sin pegas que valgan
  if (m.flags & MF_RECHARGE) d = d * 85 / 100;   // ko12.8: el turno siguiente no lo puede repetir
  if (m.fx == MX_RAMPAGE) d = d * 85 / 100;      // acaba confuso
  if (m.flags & MF_RECOIL) d = d * 90 / 100;
  if (m.stSelf && m.stD < 0 && m.stCh >= 100) d = d * 85 / 100;  // se baja algo a si mismo
  if (m.ail && m.ailCh && canAfflict(m.ail, foe)) d = d * (100 + m.ailCh / 2) / 100;  // y puede dejarle un estado
  return d;
}

// puntos de un movimiento de estado (0 = no sirve ahora). base = el mejor golpe que tiene
static uint32_t aiStatus(const Battler &self, const Battler &foe, uint8_t id, uint32_t base) {
  const MoveDef &m = moveDef(id);
  bool low = self.hp * 100u < self.maxHp * 50u, foeLow = foe.hp * 100u < foe.maxHp * 35u;
  uint32_t acc = m.acc ? m.acc : 100;
  auto P = [&](uint32_t w) { return base * w / 100 * acc / 100; };
  if (foe.subHp && !(m.flags & MF_SELF)) return 0;
  switch (m.fx) {
    case MX_HEAL: case MX_HEALWX:  // ko12.10: como la IA del juego, sin abusar (si no, Starmie se curaba sin fin)
      return self.hp * 100u < self.maxHp * 40u ? P(40 + (100 - self.hp * 100u / self.maxHp) / 2) : 0;
    case MX_REST: return (self.hp * 100u < self.maxHp * 40u && self.st != ST_SLP) ? P(85) : 0;
    case MX_TOXIC: return canAfflict(ST_PSN, foe) && !foeLow ? P(75) : 0;
    case MX_LEECHSEED: return !foe.seeded && !battlerHasType(foe, PT_GRASS) && !foeLow ? P(55) : 0;
    case MX_PROTECT: case MX_ENDURE: return (!self.protN && (foe.chargeMv || foe.rampT || low)) ? P(30) : 0;
    case MX_REFLECT: return !self.reflT && foe.atk >= foe.def ? P(35) : 0;
    case MX_LIGHTSCREEN: return !self.lscrT ? P(30) : 0;
    case MX_MIST: return !self.mistT ? P(15) : 0;
    case MX_SAFEGUARD: return !self.safeT && !self.st ? P(20) : 0;
    case MX_SUBSTITUTE: return !self.subHp && self.hp * 100u > self.maxHp * 55u ? P(40) : 0;
    case MX_RAINDANCE: return battleWeather() != WX_RAIN && battlerHasType(self, PT_WATER) ? P(45) : 0;
    case MX_SUNNYDAY: return battleWeather() != WX_SUNNY && (battlerHasType(self, PT_FIRE) || movesHas(self.mv, 76)) ? P(45) : 0;
    case MX_SANDSTORM: return battleWeather() != WX_SAND && (battlerHasType(self, PT_ROCK) || battlerHasType(self, PT_GROUND) || battlerHasType(self, PT_STEEL)) ? P(35) : 0;
    case MX_SPIKES: return !foe.spikes ? P(15) : 0;
    case MX_CURSE:
      if (battlerHasType(self, PT_GHOST)) return !foe.cursed && self.hp * 100u > self.maxHp * 60u ? P(45) : 0;
      return self.stg[STG_ATK] < 2 && !low ? P(45) : 0;
    case MX_BELLYDRUM: return self.stg[STG_ATK] < 2 && self.hp * 100u > self.maxHp * 70u ? P(70) : 0;
    case MX_PERISH: return low && !foe.perish ? P(25) : 0;
    case MX_MEANLOOK: return !foe.meanLook ? P(5) : 0;
    case MX_DISABLE: return foe.lastMv && !foe.disT ? P(30) : 0;
    case MX_ENCORE: return foe.lastMv && !foe.encT && moveIsStatus(foe.lastMv) ? P(45) : 0;
    case MX_SPITE: return foe.lastMv ? P(15) : 0;
    case MX_LOCKON: {
      for (uint8_t i = 0; i < 4; i++) if (self.mv[i] && moveDef(self.mv[i]).acc && moveDef(self.mv[i]).acc < 80 && !moveIsStatus(self.mv[i])) return !self.lockon ? P(40) : 0;
      return 0;
    }
    case MX_FOCUS: return !self.focus ? P(25) : 0;
    case MX_HAZE: return stgSum(foe) - stgSum(self) >= 2 ? P(60) : 0;
    case MX_PSYCHUP: return stgSum(foe) - stgSum(self) >= 2 ? P(50) : 0;
    case MX_TRANSFORM: return !self.xform && !foe.xform ? base * 2 + 100 : 0;
    case MX_CONVERSION: case MX_CONVERSION2: return P(15);
    case MX_ATTRACT: return !foe.attract && !foeLow ? P(30) : 0;
    case MX_SWAGGER: return canAfflict(ST_CNF, foe) && !foeLow ? P(35) : 0;
    case MX_PAINSPLIT: return (self.hp * 100u / self.maxHp + 25 < foe.hp * 100u / foe.maxHp) ? P(60) : 0;
    case MX_HEALBELL: return self.st ? P(55) : 0;
    case MX_METRONOME: return base ? base / 2 : 50;
    case MX_MIRRORMOVE: return foe.lastMv && !moveIsStatus(foe.lastMv) ? P(50) : 0;
    case MX_SLEEPTALK: return self.st == ST_SLP ? base * 9 / 10 + 50 : 0;
    case MX_NIGHTMARE: return foe.st == ST_SLP && !foe.nightmare ? P(55) : 0;
    case MX_DESTINYBOND: return self.hp * 100u < self.maxHp * 25u ? P(50) : 0;
    case MX_FORESIGHT: return battlerHasType(foe, PT_GHOST) && !foe.foresight ? P(40) : 0;
    case MX_MIMIC: return foe.lastMv && !movesHas(self.mv, foe.lastMv) ? P(30) : 0;
    case MX_TELEPORT: case MX_ROAR: return 1;  // salvaje sin otra cosa (Abra): se va
    case MX_SPLASH: case MX_BATONPASS: return 0;
    default: break;
  }
  if (foeLow && !m.stSelf) return 0;  // ya casi lo tiene: a pegar
  if (m.ail) {
    if (!canAfflict(m.ail, foe) || statusMoveBlocked(id, foe)) return 0;
    uint8_t w = m.ail == ST_SLP ? 90 : m.ail == ST_PAR ? 70 : m.ail == ST_CNF ? 50 : 60;
    return P(w);
  }
  if (m.stD > 0 && m.stSelf) {
    if (self.stg[m.st] >= 2 || self.hp * 100u < self.maxHp * 60u) return 0;
    return P(m.st == STG_EVA ? 40 : m.st == STG_ACC ? 20 : 50);
  }
  if (m.stD < 0) {
    if (foe.stg[m.st] <= -2 || foe.mistT) return 0;
    return P(35);
  }
  return 0;
}

BAct battleAi(const Battler &self, const Battler &foe, BRng &rng, uint8_t whim) {
  if (battleLocked(self)) return BA_M0;  // sigue con el mismo (el motor lo elige)
  if (self.hp * 100u < self.maxHp * 30u && rng.below(100) < 25) return BA_GUARD;
  if (!moveCount(self.mv)) {  // combatiente sin lista: como antes (placaje o su tipo)
    uint32_t tackle = rawDamage(self, foe, MOVE_TACKLE, nullptr, false) * 95;
    uint32_t typed = rawDamage(self, foe, movesMain(self), nullptr, false) * 88;
    BAct best = (typed >= tackle) ? BA_TYPE : BA_TACKLE;
    if (rng.below(100) < whim) best = (best == BA_TYPE) ? BA_TACKLE : BA_TYPE;
    if (best == BA_TYPE && typed == 0 && tackle > 0) best = BA_TACKLE;
    if (best == BA_TACKLE && tackle == 0 && typed > 0) best = BA_TYPE;
    return best;
  }
  if (!battleHasPP(self)) return BA_M0;  // forcejeo
  if (self.encT) for (uint8_t i = 0; i < 4; i++) if (self.mv[i] == self.encMv && self.pp[i]) return (BAct)(BA_M0 + i);
  uint32_t sc[4] = { 0, 0, 0, 0 }, best = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (!usable(self, i) || moveIsStatus(self.mv[i])) continue;
    sc[i] = aiDamage(self, foe, self.mv[i]);
    if (sc[i] > best) best = sc[i];
  }
  uint32_t base = best ? best : 100;
  for (uint8_t i = 0; i < 4; i++)
    if (usable(self, i) && moveIsStatus(self.mv[i])) sc[i] = aiStatus(self, foe, self.mv[i], base);
  uint8_t pick = 0xFF;
  if (rng.below(100) < whim) {  // capricho: cualquiera que sirva
    uint8_t ok[4], k = 0;
    for (uint8_t i = 0; i < 4; i++) if (sc[i]) ok[k++] = i;
    if (k) pick = ok[rng.below(k)];
  }
  if (pick == 0xFF)
    for (uint8_t i = 0; i < 4; i++)
      if (usable(self, i) && (pick == 0xFF || sc[i] > sc[pick])) pick = i;
  return (BAct)(BA_M0 + (pick == 0xFF ? 0 : pick));
}

// ---- eventos
static void push(BEvent *ev, int maxEv, int &n, uint8_t side, uint8_t kind, uint8_t move,
                 uint8_t eff, bool crit, uint16_t dmg, const Battler &a, const Battler &b,
                 uint8_t mid = 0, int8_t val = 0) {
  if (!ev || n >= maxEv) { n++; return; }
  BEvent &e = ev[n++];
  memset(&e, 0, sizeof(e));  // sin bytes de relleno al azar: los eventos se comparan tal cual
  e.side = side; e.kind = kind; e.move = move; e.eff = eff; e.crit = crit;
  e.dmg = dmg; e.hpA = a.hp; e.hpB = b.hp; e.mid = mid; e.val = val;
}
// contexto de un turno (para no pasar 8 cosas a cada funcion)
struct TurnCtx {
  Battler *s[2];
  BRng *rng;
  BEvent *ev;
  int maxEv, n;
  bool canRun, ended;
  void msg(uint8_t side, uint8_t code, uint8_t mid = 0, uint8_t num = 0, uint16_t dmg = 0) {
    push(ev, maxEv, n, side, EV_MSG, BA_M0, num, false, dmg, *s[0], *s[1], mid, (int8_t)code);
  }
  void ev2(uint8_t side, uint8_t kind, uint8_t mid = 0, int8_t val = 0, uint16_t dmg = 0, uint8_t eff = 2, bool crit = false) {
    push(ev, maxEv, n, side, kind, BA_M0, eff, crit, dmg, *s[0], *s[1], mid, val);
  }
  // quita vida (tope: la que tenga); true si lo debilita
  bool hurt(uint8_t side, uint16_t d) {
    Battler &b = *s[side];
    if (d > b.hp) d = b.hp;
    b.hp -= d;
    return b.hp == 0;
  }
  uint16_t heal(uint8_t side, uint32_t h) {
    Battler &b = *s[side];
    if (h < 1) h = 1;
    if (b.hp + h > b.maxHp) h = b.maxHp - b.hp;
    b.hp += (uint16_t)h;
    return (uint16_t)h;
  }
  void faint(uint8_t side, uint8_t mid = 0) { ev2(side, EV_FAINT, mid); }
};

static void afflictC(TurnCtx &c, uint8_t ws, uint8_t st, uint8_t mid, bool toxic = false) {
  Battler &who = *c.s[ws];
  if (st == ST_CNF) who.cnf = (uint8_t)(2 + c.rng->below(4));
  else {
    who.st = st;
    who.tox = (st == ST_PSN && toxic) ? 1 : 0;
    if (st == ST_SLP) who.stT = (uint8_t)(1 + c.rng->below(3));
    if (st == ST_SLP) who.rampT = who.rollT = who.bideT = 0;
  }
  c.ev2(ws, EV_STATUS, mid, (int8_t)(toxic ? (uint8_t)ST_TOX : st));
}
static void statChangeC(TurnCtx &c, uint8_t ws, uint8_t idx, int8_t d, uint8_t mid, bool quiet, bool byFoe) {
  if (idx >= STG_N || !d) return;
  Battler &who = *c.s[ws];
  if (byFoe && d < 0 && who.mistT) { if (!quiet) c.msg(ws, MSG_MISTED, mid); return; }
  int v = who.stg[idx] + d;
  if (v > 6) v = 6;
  if (v < -6) v = -6;
  int8_t real = (int8_t)(v - who.stg[idx]);
  who.stg[idx] = (int8_t)v;
  if (real || !quiet) c.ev2(ws, EV_STAT, mid, real, 0, idx);
}

// ¿puede moverse este turno? (sueno, congelado, retroceso, paralisis, confusion, enamorado, anulado)
static bool canActC(TurnCtx &c, uint8_t s, uint8_t mid) {
  Battler &me = *c.s[s];
  const MoveDef &m = moveDef(mid);
  if (me.st == ST_SLP) {
    if (me.stT) me.stT--;
    if (me.stT) {
      if (m.fx == MX_SNORE || m.fx == MX_SLEEPTALK) return true;  // dormido, pero esos si
      c.ev2(s, EV_CANT, 0, ST_SLP);
      return false;
    }
    me.st = ST_NONE;
    me.nightmare = false;
    c.ev2(s, EV_CURE, 0, ST_SLP);
  } else if (me.st == ST_FRZ) {
    if ((m.flags & MF_THAW) || c.rng->below(100) < 20) {
      me.st = ST_NONE;
      c.ev2(s, EV_CURE, 0, ST_FRZ);
    } else {
      c.ev2(s, EV_CANT, 0, ST_FRZ);
      return false;
    }
  }
  if (me.flinch) { c.ev2(s, EV_CANT, 0, ST_FLINCH); return false; }
  if (me.cnf) {
    me.cnf--;
    if (!me.cnf) c.ev2(s, EV_CURE, 0, ST_CNF);
    else if (c.rng->below(100) < 33) {
      uint32_t L = lvlCap(me.lvl);
      uint32_t d = ((2 * L / 5 + 2) * 40 * stgMul(me.atk, me.stg[STG_ATK]) / (me.def ? stgMul(me.def, me.stg[STG_DEF]) : 1)) / 50 + 2;
      bool down = c.hurt(s, (uint16_t)(d > 65535 ? 65535 : d));
      c.ev2(s, EV_CONFHIT, 0, 0, (uint16_t)d);
      if (down) c.faint(s);
      return false;
    }
  }
  if (me.attract && c.rng->below(100) < 50) { c.ev2(s, EV_CANT, 0, ST_LOVE); return false; }
  if (me.st == ST_PAR && c.rng->below(100) < 25) { c.ev2(s, EV_CANT, 0, ST_PAR); return false; }
  if (me.disT && mid == me.disMv) { c.ev2(s, EV_CANT, mid, ST_DISABLED); return false; }
  return true;
}

// lo que se acaba al dejar de hacer ese movimiento seguido
static void breakLocks(Battler &b) {
  b.chargeMv = 0; b.invul = 0; b.rampT = 0; b.rollT = 0; b.bideT = 0;
}

// movimiento elegido al azar por metronomo (ni el mismo, ni los que llaman a otros)
static uint8_t metronomePick(BRng &rng) {
  for (int k = 0; k < 64; k++) {
    uint8_t id = (uint8_t)(1 + rng.below(MOVE_N - 1));
    uint8_t fx = moveDef(id).fx;
    if (id == MOVE_STRUGGLE || fx == MX_METRONOME || fx == MX_SLEEPTALK || fx == MX_MIRRORMOVE ||
        fx == MX_MIMIC || fx == MX_TRANSFORM || fx == MX_PROTECT || fx == MX_ENDURE || fx == MX_DESTINYBOND ||
        fx == MX_COUNTER || fx == MX_MIRRORCOAT || fx == MX_THIEF) continue;
    return id;
  }
  return MOVE_TACKLE;
}

static void selfKo(TurnCtx &c, uint8_t side, uint8_t mid) {
  Battler &at = *c.s[side];
  if (!at.hp) return;
  at.hp = 0;
  c.faint(side, mid);
}

// los de estado (y sus efectos especiales). Devuelve true si alguien se debilito
static bool doStatus(TurnCtx &c, uint8_t side, uint8_t mid) {
  Battler &at = *c.s[side], &df = *c.s[side ^ 1];
  uint8_t fs = side ^ 1;
  const MoveDef &m = moveDef(mid);
  bool foeTgt = !(m.flags & MF_SELF);
  c.ev2(side, EV_USE, mid);
  if (foeTgt) {
    if (df.protect) { c.msg(fs, MSG_PROTECTED, mid); return false; }
    if (df.invul && m.acc) { c.ev2(side, EV_MISS, mid); return false; }
    if (m.acc) {
      uint32_t acc = m.acc;
      static const uint16_t AS[13] = { 33, 36, 43, 50, 60, 75, 100, 133, 166, 200, 233, 266, 300 };
      int s = at.stg[STG_ACC] - (df.foresight && df.stg[STG_EVA] > 0 ? 0 : df.stg[STG_EVA]);
      if (s < -6) s = -6;
      if (s > 6) s = 6;
      acc = acc * AS[s + 6] / 100;
      if (at.lockon) { at.lockon = false; acc = 100; }
      if (c.rng->below(100) >= acc) { c.ev2(side, EV_MISS, mid); return false; }
    }
    if (df.subHp && m.fx != MX_PERISH) { c.msg(side, MSG_FAIL, mid); return false; }
  }
  auto fail = [&]() { c.msg(side, MSG_FAIL, mid); return false; };
  switch (m.fx) {
    case MX_NONE:
    case MX_TOXIC: {
      if (m.ail) {
        if (df.guard || statusMoveBlocked(mid, df) || !canAfflict(m.ail, df)) {
          if (df.safeT) c.msg(fs, MSG_SAFEGUARDED, mid);
          else c.ev2(fs, EV_NOEFFECT, mid);
          return false;
        }
        afflictC(c, fs, m.ail, mid, m.fx == MX_TOXIC);
        return false;
      }
      if (!m.stD && !m.stD2) return fail();
      uint8_t who = m.stSelf ? side : fs;
      if (!m.stSelf && df.guard) { c.ev2(fs, EV_NOEFFECT, mid); return false; }
      statChangeC(c, who, m.st, m.stD, mid, false, !m.stSelf);
      if (m.stD2) statChangeC(c, who, m.st2, m.stD2, mid, false, !m.stSelf);
      if (mid == 107) at.minimized = true;   // chiquitolina: pisoton le hace el doble
      if (mid == 111) at.curled = true;      // rizo defensa: rodar el doble
      return false;
    }
    case MX_HEAL: case MX_HEALWX: {
      if (at.hp >= at.maxHp) return fail();
      uint32_t h = at.maxHp / 2;
      if (m.fx == MX_HEALWX) {
        uint8_t w = battleWeather();
        h = w == WX_SUNNY ? at.maxHp : (w == WX_RAIN || w == WX_SAND || w == WX_SNOW) ? at.maxHp / 4 : at.maxHp / 2;
      }
      uint16_t got = c.heal(side, h);
      c.ev2(side, EV_HEAL, mid, 0, got);
      return false;
    }
    case MX_REST: {
      if (at.hp >= at.maxHp || at.st == ST_SLP) return fail();
      at.hp = at.maxHp;
      at.st = ST_SLP; at.stT = 3; at.tox = 0; at.nightmare = false;
      c.msg(side, MSG_REST, mid);
      return false;
    }
    case MX_LEECHSEED:
      if (df.seeded || statusMoveBlocked(mid, df)) return fail();
      df.seeded = true;
      c.msg(fs, MSG_SEEDED, mid);
      return false;
    case MX_DISABLE: {
      if (!df.lastMv || df.disT || !movesHas(df.mv, df.lastMv)) return fail();
      df.disMv = df.lastMv;
      df.disT = (uint8_t)(2 + c.rng->below(6));
      c.msg(fs, MSG_DISABLE, df.disMv);
      return false;
    }
    case MX_MIST: if (at.mistT) return fail(); at.mistT = 5; c.msg(side, MSG_MIST, mid); return false;
    case MX_LIGHTSCREEN: if (at.lscrT) return fail(); at.lscrT = 5; c.msg(side, MSG_LSCREEN, mid); return false;
    case MX_REFLECT: if (at.reflT) return fail(); at.reflT = 5; c.msg(side, MSG_REFLECT, mid); return false;
    case MX_SAFEGUARD: if (at.safeT) return fail(); at.safeT = 5; c.msg(side, MSG_SAFEGUARD, mid); return false;
    case MX_HAZE:
      memset(at.stg, 0, sizeof(at.stg));
      memset(df.stg, 0, sizeof(df.stg));
      c.msg(side, MSG_HAZE, mid);
      return false;
    case MX_FOCUS: if (at.focus) return fail(); at.focus = true; c.msg(side, MSG_FOCUS, mid); return false;
    case MX_TRANSFORM: {
      if (at.xform || df.xform) return fail();
      at.oType = at.type; at.oType2 = at.type2; at.oAtk = at.atk; at.oDef = at.def; at.oSpe = at.spe;
      memcpy(at.oMv, at.mv, 4); memcpy(at.oPp, at.pp, 4);
      at.xform = df.dex;
      at.type = df.type; at.type2 = df.type2;
      at.atk = df.atk; at.def = df.def; at.spe = df.spe;
      memcpy(at.stg, df.stg, sizeof(at.stg));
      for (uint8_t i = 0; i < 4; i++) { at.mv[i] = df.mv[i]; at.pp[i] = df.mv[i] ? 5 : 0; }
      c.msg(side, MSG_TRANSFORM, mid);
      return false;
    }
    case MX_SPLASH: c.msg(side, MSG_SPLASH, mid); return false;
    case MX_CONVERSION: {
      uint8_t opts[4], k = 0;
      for (uint8_t i = 0; i < 4; i++) {
        if (!at.mv[i] || at.mv[i] == mid) continue;
        uint8_t t = moveDef(at.mv[i]).type;
        if (!battlerHasType(at, t)) opts[k++] = t;
      }
      if (!k) return fail();
      at.type = opts[c.rng->below(k)];
      at.type2 = 0xFF;
      c.msg(side, MSG_CONVERSION, mid, at.type);
      return false;
    }
    case MX_CONVERSION2: {
      if (!df.lastMv) return fail();
      uint8_t lt = moveTypeFor(df, df.lastMv), opts[PT_BATTLE_N], k = 0;
      for (uint8_t t = 0; t < PT_BATTLE_N; t++) if (typeEff(lt, t) < 2 && !battlerHasType(at, t)) opts[k++] = t;
      if (!k) return fail();
      at.type = opts[c.rng->below(k)];
      at.type2 = 0xFF;
      c.msg(side, MSG_CONVERSION, mid, at.type);
      return false;
    }
    case MX_SUBSTITUTE: {
      uint16_t q = at.maxHp / 4;
      if (at.subHp || at.hp <= q || !q) return fail();
      at.hp -= q;
      at.subHp = (uint16_t)(q + 1);
      c.msg(side, MSG_SUB, mid, 0, q);
      return false;
    }
    case MX_MIMIC: {
      uint8_t lm = df.lastMv;
      if (!lm || lm == MOVE_STRUGGLE || movesHas(at.mv, lm) || moveDef(lm).fx == MX_MIMIC ||
          moveDef(lm).fx == MX_METRONOME || moveDef(lm).fx == MX_TRANSFORM) return fail();
      for (uint8_t i = 0; i < 4; i++)
        if (at.mv[i] == mid) {
          at.mv[i] = lm;
          at.pp[i] = mid == 166 ? movePP(lm) : 5;
          if (mid == 166) { at.sketched = lm; c.msg(side, MSG_SKETCH, lm); }
          else c.msg(side, MSG_MIMIC, lm);
          return false;
        }
      return fail();
    }
    case MX_MEANLOOK: if (df.meanLook) return fail(); df.meanLook = true; c.msg(fs, MSG_MEANLOOK, mid); return false;
    case MX_LOCKON: at.lockon = true; c.msg(side, MSG_LOCKON, mid); return false;
    case MX_NIGHTMARE:
      if (df.st != ST_SLP || df.nightmare) return fail();
      df.nightmare = true;
      c.msg(fs, MSG_NIGHTMARE, mid);
      return false;
    case MX_CURSE: {
      if (battlerHasType(at, PT_GHOST)) {
        if (df.cursed) return fail();
        df.cursed = true;
        uint16_t q = at.maxHp / 2 ? at.maxHp / 2 : 1;
        bool down = c.hurt(side, q);
        c.msg(fs, MSG_CURSE, mid, 0, q);
        if (down) { c.faint(side, mid); return true; }
        return false;
      }
      statChangeC(c, side, STG_SPE, -1, mid, true, false);
      statChangeC(c, side, STG_ATK, 1, mid, false, false);
      statChangeC(c, side, STG_DEF, 1, mid, false, false);
      return false;
    }
    case MX_SPITE: {
      for (uint8_t i = 0; i < 4; i++)
        if (df.lastMv && df.mv[i] == df.lastMv && df.pp[i]) {
          uint8_t k = (uint8_t)(2 + c.rng->below(4));
          if (k > df.pp[i]) k = df.pp[i];
          df.pp[i] -= k;
          c.msg(fs, MSG_SPITE, df.lastMv, k);
          return false;
        }
      return fail();
    }
    case MX_PROTECT: case MX_ENDURE: {
      uint32_t den = 1u << (at.protN < 5 ? at.protN : 5);  // 2a gen: cada vez seguida, la mitad
      if (c.rng->below(den) != 0) { at.protN = 0; return fail(); }
      at.protN++;
      if (m.fx == MX_PROTECT) { at.protect = true; c.msg(side, MSG_PROTECT, mid); }
      else { at.endure = true; c.msg(side, MSG_ENDURE, mid); }
      return false;
    }
    case MX_BELLYDRUM: {
      uint16_t q = at.maxHp / 2;
      if (at.hp <= q || at.stg[STG_ATK] >= 6) return fail();
      at.hp -= q;
      at.stg[STG_ATK] = 6;
      c.msg(side, MSG_BELLYDRUM, mid, 0, q);
      return false;
    }
    case MX_SPIKES: if (df.spikes) return fail(); df.spikes = 1; c.msg(fs, MSG_SPIKES, mid); return false;
    case MX_FORESIGHT: df.foresight = true; c.msg(fs, MSG_FORESIGHT, mid); return false;
    case MX_DESTINYBOND: at.dbond = true; c.msg(side, MSG_DBOND, mid); return false;
    case MX_PERISH: {
      bool any = false;
      if (!at.perish) { at.perish = 5; any = true; }  // fin de turno: 3, 2, 1, 0 (se debilita)
      if (!df.perish) { df.perish = 5; any = true; }
      if (!any) return fail();
      c.msg(side, MSG_PERISH_SING, mid, 3);
      return false;
    }
    case MX_SANDSTORM: case MX_RAINDANCE: case MX_SUNNYDAY: {
      uint8_t w = m.fx == MX_SANDSTORM ? WX_SAND : m.fx == MX_RAINDANCE ? WX_RAIN : WX_SUNNY;
      if (battleWeather() == w) return fail();
      sWxOv = w; sWxT = 5;
      c.msg(side, w == WX_SAND ? MSG_WX_SAND : w == WX_RAIN ? MSG_WX_RAIN : MSG_WX_SUN, mid);
      return false;
    }
    case MX_SWAGGER: {
      statChangeC(c, fs, STG_ATK, 2, mid, false, false);
      if (canAfflict(ST_CNF, df)) afflictC(c, fs, ST_CNF, mid);
      else if (df.safeT) c.msg(fs, MSG_SAFEGUARDED, mid);
      return false;
    }
    case MX_ATTRACT: if (df.attract) return fail(); df.attract = true; c.msg(fs, MSG_LOVE, mid); return false;
    case MX_HEALBELL: {
      if (!at.st) return fail();
      at.st = ST_NONE; at.tox = 0; at.nightmare = false;
      c.msg(side, MSG_HEALBELL, mid);
      return false;
    }
    case MX_PAINSPLIT: {
      uint16_t avg = (uint16_t)(((uint32_t)at.hp + df.hp) / 2);
      at.hp = avg < at.maxHp ? avg : at.maxHp;
      df.hp = avg < df.maxHp ? avg : df.maxHp;
      if (!df.hp) df.hp = 1;
      c.msg(side, MSG_PAINSPLIT, mid);
      return false;
    }
    case MX_ENCORE: {
      uint8_t lm = df.lastMv;
      if (!lm || df.encT || !movesHas(df.mv, lm) || lm == MOVE_STRUGGLE || moveDef(lm).fx == MX_ENCORE ||
          moveDef(lm).fx == MX_MIRRORMOVE) return fail();
      df.encMv = lm;
      df.encT = (uint8_t)(2 + c.rng->below(5));
      c.msg(fs, MSG_ENCORE, lm);
      return false;
    }
    case MX_PSYCHUP: memcpy(at.stg, df.stg, sizeof(at.stg)); c.msg(side, MSG_PSYCHUP, mid); return false;
    case MX_ROAR: case MX_TELEPORT: {
      // salvaje: el rival sale volando (rugido / remolino) o se va el que lo usa (teletransporte). Entrenadores: falla
      if (!sFleeOk) return fail();
      c.ev2(m.fx == MX_ROAR ? fs : side, EV_RUN_OK, mid, m.fx == MX_ROAR ? 1 : 2);
      c.ended = true;
      return true;
    }
    case MX_BATONPASS: return fail();  // aqui no hay a quien pasar el relevo en medio del turno
    default: return fail();
  }
}

// un movimiento de "at" (lado side) a "df"; devuelve true si alguno se debilito (o se acabo la batalla)
static bool doMoveC(TurnCtx &c, uint8_t side, BAct act, bool first, bool *landed) {
  Battler &at = *c.s[side], &df = *c.s[side ^ 1];
  uint8_t fs = side ^ 1;
  if (landed) *landed = false;
  int8_t slot;
  bool cont;
  uint8_t mid = actMove(at, act, &slot, &cont);
  at.dbond = false;  // mismodestino dura hasta que vuelve a moverse
  if (!canActC(c, side, mid)) {
    breakLocks(at);
    at.furyN = 0;
    at.rage = false;
    return at.hp == 0;
  }
  if (slot >= 0 && at.pp[slot] && mid != MOVE_STRUGGLE && !cont) at.pp[slot]--;
  if (at.encT && mid != at.encMv) at.encT = 0;
  // los que llaman a otro
  uint8_t sel = mid;
  if (moveDef(mid).fx == MX_METRONOME || moveDef(mid).fx == MX_MIRRORMOVE || moveDef(mid).fx == MX_SLEEPTALK) {
    uint8_t called = 0;
    uint8_t fx = moveDef(mid).fx;
    if (fx == MX_METRONOME) called = metronomePick(*c.rng);
    else if (fx == MX_MIRRORMOVE) called = (df.lastMv && moveDef(df.lastMv).fx != MX_MIRRORMOVE) ? df.lastMv : 0;
    else if (at.st == ST_SLP) {
      uint8_t ok[4], k = 0;
      for (uint8_t i = 0; i < 4; i++) {
        uint8_t o = at.mv[i], ofx = moveDef(o).fx;
        if (o && o != mid && ofx != MX_CHARGE && ofx != MX_SOLAR && ofx != MX_FLY && ofx != MX_DIG && ofx != MX_BIDE &&
            ofx != MX_METRONOME && ofx != MX_MIRRORMOVE) ok[k++] = o;
      }
      if (k) called = ok[c.rng->below(k)];
    }
    if (!called) {
      c.ev2(side, EV_USE, mid);
      c.msg(side, MSG_FAIL, mid);
      at.lastMv = sel;
      return false;
    }
    c.ev2(side, EV_USE, mid);
    c.msg(side, fx == MX_METRONOME ? MSG_METRONOME : fx == MX_MIRRORMOVE ? MSG_MIRRORMOVE : MSG_SLEEPTALK, called);
    mid = called;
  }
  const MoveDef &m = moveDef(mid);
  if (m.fx != MX_PROTECT && m.fx != MX_ENDURE) at.protN = 0;
  if (m.fx != MX_FURYCUTTER) at.furyN = 0;
  at.rage = m.fx == MX_RAGE;
  at.lastMv = sel;

  if (m.flags & MF_STATUS) {
    bool r = doStatus(c, side, mid);
    if (at.hp == 0 && !r) return true;
    return r;
  }

  // ---- cargas (2 turnos): el primero solo carga (rayo solar con sol: al momento)
  if ((m.fx == MX_CHARGE || m.fx == MX_SOLAR || m.fx == MX_FLY || m.fx == MX_DIG) && at.chargeMv != mid) {
    if (!(m.fx == MX_SOLAR && battleWeather() == WX_SUNNY)) {
      at.chargeMv = mid;
      if (m.fx == MX_FLY) { at.invul = 1; c.msg(side, MSG_FLY, mid); }
      else if (m.fx == MX_DIG) { at.invul = 2; c.msg(side, MSG_DIG, mid); }
      else c.msg(side, MSG_CHARGE, mid);
      if (mid == 130) statChangeC(c, side, STG_DEF, 1, mid, true, false);  // cabezazo: sube la defensa al cargar
      return false;
    }
  }
  bool charged = at.chargeMv == mid;
  if (charged) { at.chargeMv = 0; at.invul = 0; }

  // ---- venganza: aguanta 2 turnos y devuelve el doble
  if (m.fx == MX_BIDE) {
    if (!at.bideT) {
      at.bideT = 2; at.bideDmg = 0;
      c.ev2(side, EV_USE, mid);
      c.msg(side, MSG_BIDE, mid);
      return false;
    }
    if (--at.bideT) { c.msg(side, MSG_BIDE, mid); return false; }
    c.msg(side, MSG_BIDE_GO, mid);
    if (!at.bideDmg) { c.msg(side, MSG_FAIL, mid); return false; }
  }

  // ---- premonicion: el golpe llega dentro de 2 turnos
  if (m.fx == MX_FUTURESIGHT) {
    if (df.futT) { c.ev2(side, EV_USE, mid); c.msg(side, MSG_FAIL, mid); return false; }
    uint32_t d = rawDamage(at, df, mid, nullptr, false);
    d = d * (85 + c.rng->below(16)) / 100;
    df.futT = 3;
    df.futDmg = (uint16_t)(d < 1 ? 1 : d > 65535 ? 65535 : d);
    c.ev2(side, EV_USE, mid);
    c.msg(side, MSG_FUTURE, mid);
    return false;
  }

  // ---- se protegio (proteccion / detectar)
  if (df.protect) {
    c.msg(fs, MSG_PROTECTED, mid);
    if (m.fx == MX_CRASH) {
      uint16_t q = at.maxHp / 8 ? at.maxHp / 8 : 1;
      bool down = c.hurt(side, q);
      c.msg(side, MSG_CRASH, mid, 0, q);
      if (down) { c.faint(side, mid); return true; }
    }
    if (m.fx == MX_SELFKO) { selfKo(c, side, mid); return true; }
    at.rollT = 0;
    return false;
  }

  // ---- condiciones
  if (m.fx == MX_DREAMEATER && df.st != ST_SLP) { c.ev2(side, EV_USE, mid); c.ev2(fs, EV_NOEFFECT, mid); return false; }
  if (m.fx == MX_SNORE && at.st != ST_SLP) { c.ev2(side, EV_USE, mid); c.msg(side, MSG_FAIL, mid); return false; }
  uint16_t fixedD = 0;
  if (m.fx == MX_COUNTER || m.fx == MX_MIRRORCOAT) {
    uint16_t got = m.fx == MX_COUNTER ? at.lastDmgP : at.lastDmgS;
    if (!got) { c.ev2(side, EV_USE, mid); c.msg(side, MSG_FAIL, mid); return false; }
    fixedD = (uint16_t)(got * 2u > 65535 ? 65535 : got * 2u);
  }
  if (m.fx == MX_BIDE) fixedD = (uint16_t)(at.bideDmg * 2u > 65535 ? 65535 : at.bideDmg * 2u);
  if (m.fx == MX_OHKO && df.lvl > at.lvl) { c.ev2(side, EV_USE, mid); c.msg(side, MSG_FAIL, mid); return false; }

  // ---- acierta?
  bool hit = true;
  if (df.invul == 1 && !(m.fx == MX_TWISTER || m.fx == MX_THUNDER || mid == 18)) hit = false;
  else if (df.invul == 2 && !(m.fx == MX_QUAKE || m.fx == MX_MAGNITUDE || mid == 90)) hit = false;
  else if (at.lockon) { at.lockon = false; }
  else if (!(m.flags & MF_NOMISS) && m.acc && m.fx != MX_BIDE) {
    uint32_t acc = m.acc;
    if (m.fx == MX_THUNDER) { uint8_t w = battleWeather(); if (w == WX_RAIN) acc = 255; else if (w == WX_SUNNY) acc = 50; }
    if (m.fx == MX_OHKO) acc = 30 + (uint32_t)(at.lvl - df.lvl) * 2;
    else if (acc < 255) {
      static const uint16_t AS[13] = { 33, 36, 43, 50, 60, 75, 100, 133, 166, 200, 233, 266, 300 };
      int s = at.stg[STG_ACC] - (df.foresight && df.stg[STG_EVA] > 0 ? 0 : df.stg[STG_EVA]);
      if (s < -6) s = -6;
      if (s > 6) s = 6;
      acc = acc * AS[s + 6] / 100;
    }
    hit = c.rng->below(100) < acc;
  }
  if (!hit) {
    c.ev2(side, EV_MISS, mid);
    if (m.fx == MX_CRASH) {  // patada salto: se estrella
      uint16_t q = at.maxHp / 8 ? at.maxHp / 8 : 1;
      bool down = c.hurt(side, q);
      c.msg(side, MSG_CRASH, mid, 0, q);
      if (down) { c.faint(side, mid); return true; }
    }
    if (m.fx == MX_SELFKO) { selfKo(c, side, mid); return true; }
    at.rollT = 0;
    at.furyN = 0;
    if (at.rampT && !--at.rampT) { c.msg(side, MSG_RAMPAGE_END, mid); if (canAfflict(ST_CNF, at)) afflictC(c, side, ST_CNF, mid); }
    return false;
  }

  // ---- tiradas antes del golpe
  if (m.fx == MX_MAGNITUDE) {
    uint32_t r = c.rng->below(100);
    sMagLv = r < 5 ? 4 : r < 15 ? 5 : r < 35 ? 6 : r < 65 ? 7 : r < 85 ? 8 : r < 95 ? 9 : 10;
    c.msg(side, MSG_MAGNITUDE, mid, sMagLv);
  }
  if (m.fx == MX_PRESENT) {
    uint32_t r = c.rng->below(100);
    if (r < 20) {  // le cura un cuarto
      uint16_t h = df.hp < df.maxHp ? c.heal(fs, df.maxHp / 4) : 0;
      c.ev2(side, EV_USE, mid);
      c.msg(fs, MSG_PRESENT_HEAL, mid, 0, h);
      return false;
    }
    sPresent = r < 60 ? 40 : r < 90 ? 80 : 120;
  }
  uint32_t psy = 0;
  if (m.fx == MX_PSYWAVE) { uint32_t L = lvlCap(at.lvl); psy = 1 + c.rng->below(L * 3 / 2 ? L * 3 / 2 : 1); }

  // ---- golpes
  uint8_t nh = 1;
  if (m.fx == MX_TRIPLEKICK) nh = 3;
  else if (m.hits) {
    uint8_t lo = m.hits & 15, hi = m.hits >> 4;
    if (lo == hi) nh = lo;
    else {  // 2-5: 3/8 dos, 3/8 tres, 1/8 cuatro, 1/8 cinco (como en los juegos)
      uint32_t r = c.rng->below(8);
      nh = r < 3 ? 2 : r < 6 ? 3 : r < 7 ? 4 : 5;
      if (hi < 5 && nh > hi) nh = hi;
    }
  }
  uint8_t crRate = (uint8_t)(((m.flags & MF_HICRIT) ? 2 : 0) + (at.focus ? 1 : 0));
  static const uint8_t CR[4] = { 17, 32, 64, 85 };  // de 256 (2a gen)
  bool sp = typeIsSpecial(moveTypeFor(at, mid));
  uint32_t total = 0;
  uint8_t eff = 2, done = 0;
  bool anyCrit = false, subHit = false;
  for (uint8_t h = 0; h < nh; h++) {
    sHitK = (uint8_t)(h + 1);
    if (m.fx == MX_TRIPLEKICK && h && c.rng->below(100) >= m.acc) break;  // cada patada puede fallar
    bool crit = false;
    uint32_t d;
    bool fixed = fixedD || m.fx == MX_SONICBOOM || m.fx == MX_DRAGONRAGE || m.fx == MX_LEVEL ||
                 m.fx == MX_PSYWAVE || m.fx == MX_SUPERFANG || m.fx == MX_OHKO;
    if (fixedD) {
      d = fixedD;
      rawDamage(at, df, mid, &eff, false);
      if (m.fx == MX_BIDE) eff = 2;
      if (!eff) d = 0;
    } else {
      crit = !fixed && c.rng->below(256) < CR[crRate < 3 ? crRate : 3];
      d = rawDamage(at, df, mid, &eff, crit, psy);
      if (eff && !fixed) d = d * (85 + c.rng->below(16)) / 100;
    }
    if (eff) {
      if (df.guard) d /= 2;
      if (d < 1) d = 1;
    } else d = 0;
    if (!eff) { if (!h) { c.ev2(side, EV_HIT, mid, 0, 0, 0, false); } break; }
    if (d > 65535) d = 65535;
    // sustituto: se lleva el golpe
    if (df.subHp && m.fx != MX_FUTURESIGHT) {
      uint16_t sd = (uint16_t)(d > df.subHp ? df.subHp : d);
      df.subHp -= sd;
      subHit = true;
      c.msg(fs, MSG_SUBHIT, mid, 0, sd);
      if (!df.subHp) c.msg(fs, MSG_SUBBREAK, mid);
      total += sd;
      done++;
      anyCrit |= crit;
      if (!df.subHp) break;
      continue;
    }
    bool endured = false;
    if (d >= df.hp && (df.endure || m.fx == MX_FALSESWIPE)) { d = df.hp > 1 ? df.hp - 1 : 0; endured = df.endure; }
    if (d > df.hp) d = df.hp;
    df.hp -= (uint16_t)d;
    if (sp) df.lastDmgS = (uint16_t)(df.lastDmgS + d); else df.lastDmgP = (uint16_t)(df.lastDmgP + d);
    if (df.bideT) df.bideDmg = (uint16_t)(df.bideDmg + d > 65535 ? 65535 : df.bideDmg + d);
    total += d;
    done++;
    anyCrit |= crit;
    bool rest = (m.flags & MF_RECHARGE) && df.hp;
    if (rest) { at.restMv = mid; at.restT = 2; }
    c.ev2(side, EV_HIT, mid, (int8_t)((df.guard && d ? HIT_GUARDED : 0) | (rest ? HIT_REST : 0)), (uint16_t)d, eff, crit);
    if (m.fx == MX_OHKO) c.msg(fs, MSG_OHKO, mid);
    if (endured) c.msg(fs, MSG_ENDURED, mid);
    if (df.rage && df.hp && d) { statChangeC(c, fs, STG_ATK, 1, 99, true, false); c.msg(fs, MSG_RAGE, 99); }
    if (!df.hp) break;
  }
  if (nh > 1 && done) c.msg(fs, MSG_HITS, mid, done);
  if (landed) *landed = total > 0 && !subHit;
  if (!eff && !total) {
    // inmune: lo que encadena se corta
    at.rollT = 0; at.furyN = 0;
    if (m.fx == MX_SELFKO) { selfKo(c, side, mid); return true; }
    return false;
  }
  // drenaje / retroceso
  if (total && (m.flags & MF_DRAIN) && at.hp < at.maxHp) {
    uint16_t h = c.heal(side, total * m.drain / 100);
    c.ev2(side, EV_DRAIN, mid, 0, h);
  }
  if (total && (m.flags & MF_RECOIL)) {
    uint32_t r = total * m.drain / 100;
    if (r < 1) r = 1;
    c.hurt(side, (uint16_t)(r > 65535 ? 65535 : r));
    c.ev2(side, EV_RECOIL, mid, 0, (uint16_t)r);
  }
  if (m.fx == MX_SELFKO) selfKo(c, side, mid);
  // cadenas
  if (m.fx == MX_ROLLOUT) { at.rollT++; if (at.rollT >= 5) at.rollT = 0; }
  if (m.fx == MX_FURYCUTTER && at.furyN < 4) at.furyN++;
  if (df.hp == 0) {
    c.faint(fs, mid);
    if (df.dbond && at.hp) { at.hp = 0; c.msg(fs, MSG_DBOND_HIT, 194); }
    if (at.hp == 0 && m.fx != MX_SELFKO) c.faint(side, mid);
    return true;
  }
  if (at.hp == 0) {
    if (m.fx != MX_SELFKO) c.faint(side, mid);
    return true;
  }
  if (m.fx == MX_RAMPAGE) {
    if (!at.rampT) { at.rampMv = mid; at.rampT = (uint8_t)(1 + c.rng->below(2)); }  // 2-3 turnos en total
    else if (!--at.rampT) {
      c.msg(side, MSG_RAMPAGE_END, mid);
      if (canAfflict(ST_CNF, at)) afflictC(c, side, ST_CNF, mid);
    }
  }
  // efectos secundarios (solo si el golpe entro y no fue al sustituto)
  if (total && !subHit) {
    uint8_t ail = m.ail, ch = m.ailCh;
    if (m.fx == MX_TRIATTACK) { static const uint8_t T[3] = { ST_BRN, ST_FRZ, ST_PAR }; ail = T[c.rng->below(3)]; }
    if (ail && ch && c.rng->below(100) < ch && !df.guard && canAfflict(ail, df)) afflictC(c, fs, ail, mid);
    if (df.st == ST_FRZ && moveTypeFor(at, mid) == PT_FIRE && ail != ST_FRZ) { df.st = ST_NONE; c.ev2(fs, EV_CURE, 0, ST_FRZ); }
    if (m.stD && m.stCh && c.rng->below(100) < m.stCh) {
      if (m.stSelf) statChangeC(c, side, m.st, m.stD, mid, true, false);
      else statChangeC(c, fs, m.st, m.stD, mid, true, true);
    }
    if (m.fx == MX_ALLUP && c.rng->below(100) < m.stCh)
      for (uint8_t k = 0; k < 5; k++) statChangeC(c, side, k, 1, mid, true, false);
    if (first && m.flinch && c.rng->below(100) < m.flinch) df.flinch = true;
    if (m.fx == MX_TRAP && !df.trapT) {
      df.trapT = (uint8_t)(2 + c.rng->below(4));
      df.trapMv = mid;
      c.msg(fs, MSG_TRAPPED, mid);
    }
    if (m.fx == MX_PAYDAY) c.msg(side, MSG_PAYDAY, mid);
  }
  if (m.fx == MX_RAPIDSPIN && (at.trapT || at.seeded || at.spikes)) {
    at.trapT = 0; at.seeded = false; at.spikes = 0;
    c.msg(side, MSG_RAPIDSPIN, mid);
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
  uint32_t d = rawDamage(at, df, MOVE_TACKLE, &eff, false);
  if (effOut) *effOut = eff;
  if (!eff) return 0;
  d = d * (85 + rng.below(16)) / 100 / 2;
  if (d < 1) d = 1;
  return (uint16_t)(d > df.hp ? df.hp : d);
}

static bool wildOnly(BAct a) { return a == BA_RUN || a == BA_POTION || a == BA_BALL; }
static bool isAttack(BAct a) { return a == BA_TACKLE || a == BA_TYPE || (a >= BA_M0 && a <= BA_M3); }

// fin del turno (orden de 2a gen): premonicion, tiempo, atrapado, veneno/quemadura, drenadoras,
// pesadilla, maldicion, canto mortal y lo que se acaba con los turnos
static void endOfTurnC(TurnCtx &c) {
  Battler &a = *c.s[0], &b = *c.s[1];
  auto alive = [&]() { return a.hp && b.hp; };
  for (uint8_t s = 0; s < 2 && alive(); s++) {
    Battler &me = *c.s[s];
    if (me.futT && !--me.futT) {
      uint16_t d = me.futDmg;
      if (me.subHp) { me.subHp = 0; c.msg(s, MSG_FUTURE_HIT, 248, 0, 0); c.msg(s, MSG_SUBBREAK, 248); }
      else {
        bool down = c.hurt(s, d);
        c.msg(s, MSG_FUTURE_HIT, 248, 0, d);
        if (down) { c.faint(s, 248); return; }
      }
    }
  }
  // tiempo de los movimientos: arena hace dano; y se acaba
  if (alive() && battleWeather() == WX_SAND) {
    for (uint8_t s = 0; s < 2 && alive(); s++) {
      Battler &me = *c.s[s];
      if (battlerHasType(me, PT_ROCK) || battlerHasType(me, PT_GROUND) || battlerHasType(me, PT_STEEL) || me.invul == 2) continue;
      uint16_t d = me.maxHp / 8 ? me.maxHp / 8 : 1;
      bool down = c.hurt(s, d);
      c.msg(s, MSG_SANDDMG, 201, 0, d);
      if (down) { c.faint(s); return; }
    }
  }
  if (sWxT && !--sWxT) c.msg(0, MSG_WX_END, 0, battleWeather());
  for (uint8_t s = 0; s < 2 && alive(); s++) {
    Battler &me = *c.s[s];
    Battler &op = *c.s[s ^ 1];
    me.flinch = false;
    if (me.trapT) {
      if (!--me.trapT) c.msg(s, MSG_TRAPEND, me.trapMv);
      else {
        uint16_t d = me.maxHp / 16 ? me.maxHp / 16 : 1;
        bool down = c.hurt(s, d);
        c.msg(s, MSG_TRAPDMG, me.trapMv, 0, d);
        if (down) { c.faint(s); return; }
      }
    }
    // ko11.31: el veneno, la quemadura y la paralisis pueden pasarse solos (1 de cada 6 turnos)
    if ((me.st == ST_PSN || me.st == ST_BRN || me.st == ST_PAR) && !me.tox && c.rng->below(100) < STATUS_CURE_PCT) {
      uint8_t was = me.st;
      me.st = ST_NONE;
      c.ev2(s, EV_CURE, 0, (int8_t)was);
    } else if (me.st == ST_PSN || me.st == ST_BRN) {
      uint32_t d = me.maxHp / (me.st == ST_PSN ? 8 : 16);
      if (me.tox) { d = (uint32_t)me.maxHp * me.tox / 16; if (me.tox < 15) me.tox++; }
      if (d < 1) d = 1;
      bool down = c.hurt(s, (uint16_t)d);
      c.ev2(s, EV_STDMG, 0, (int8_t)(me.tox ? (uint8_t)ST_TOX : me.st), (uint16_t)d);
      if (down) { c.faint(s); return; }
    }
    if (me.seeded && op.hp) {
      uint16_t d = me.maxHp / 8 ? me.maxHp / 8 : 1;
      if (d > me.hp) d = me.hp;
      bool down = c.hurt(s, d);
      if (op.hp < op.maxHp) c.heal(s ^ 1, d);
      c.msg(s, MSG_SEEDDMG, 73, 0, d);
      if (down) { c.faint(s); return; }
    }
    if (me.nightmare) {
      if (me.st != ST_SLP) me.nightmare = false;
      else {
        uint16_t d = me.maxHp / 4 ? me.maxHp / 4 : 1;
        bool down = c.hurt(s, d);
        c.msg(s, MSG_NIGHTMAREDMG, 171, 0, d);
        if (down) { c.faint(s); return; }
      }
    }
    if (me.cursed) {
      uint16_t d = me.maxHp / 4 ? me.maxHp / 4 : 1;
      bool down = c.hurt(s, d);
      c.msg(s, MSG_CURSEDMG, 174, 0, d);
      if (down) { c.faint(s); return; }
    }
  }
  // canto mortal: los dos a la vez
  for (uint8_t s = 0; s < 2; s++) {
    Battler &me = *c.s[s];
    if (!me.perish || !me.hp) continue;
    me.perish--;
    c.msg(s, MSG_PERISH, 195, (uint8_t)(me.perish - 1));
  }
  for (uint8_t s = 0; s < 2; s++) {
    Battler &me = *c.s[s];
    if (me.perish == 1 && me.hp) { me.perish = 0; me.hp = 0; c.faint(s, 195); }
  }
  // lo que dura unos turnos
  for (uint8_t s = 0; s < 2; s++) {
    Battler &me = *c.s[s];
    if (me.reflT && !--me.reflT) c.msg(s, MSG_SCREEN_END, 115, 1);
    if (me.lscrT && !--me.lscrT) c.msg(s, MSG_SCREEN_END, 113, 0);
    if (me.mistT && !--me.mistT) c.msg(s, MSG_SCREEN_END, 54, 2);
    if (me.safeT && !--me.safeT) c.msg(s, MSG_SCREEN_END, 219, 3);
    if (me.encT && !--me.encT) c.msg(s, MSG_ENCORE_END, me.encMv);
    if (me.disT && !--me.disT) c.msg(s, MSG_DISABLE_END, me.disMv);
  }
}

static void restTick(Battler &b) {
  if (b.restT && !--b.restT) b.restMv = 0;
}
static void turnEnd(Battler &a, Battler &b) {
  a.fieldWx = b.fieldWx = sWxOv;  // ko12.10: el tiempo de los movimientos viaja con los combatientes
  a.fieldWxT = b.fieldWxT = sWxT;
  a.flinch = b.flinch = false;
  a.guard = b.guard = false;
  a.protect = b.protect = false;
  a.endure = b.endure = false;
  restTick(a); restTick(b);
}
// al empezar el turno: las puas al que acaba de entrar
static void turnStart(TurnCtx &c) {
  // ko12.10: el tiempo de los movimientos sale de los combatientes (asi dos simulaciones en el mismo
  // aparato, o en los dos del tongsin, no se pisan); el del reloj sigue en sBattleWx
  sWxOv = c.s[0]->fieldWx;
  sWxT = c.s[0]->fieldWxT;
  for (uint8_t s = 0; s < 2; s++) {
    Battler &me = *c.s[s];
    me.lastDmgP = me.lastDmgS = 0;
    if (me.spkPend) {
      me.spkPend = false;
      if (me.spikes && me.hp && !battlerHasType(me, PT_FLYING)) {
        uint16_t d = me.maxHp / 8 ? me.maxHp / 8 : 1;
        bool down = c.hurt(s, d);
        c.msg(s, MSG_SPIKEDMG, 191, 0, d);
        if (down) c.faint(s);
      }
    }
  }
}

int runChance(uint32_t speMe, uint32_t speFoe) {
  int ch = RUN_BASE + ((int)speMe - (int)speFoe) / 2;
  if (ch < RUN_MIN) ch = RUN_MIN;
  if (ch > RUN_MAX) ch = RUN_MAX;
  return ch;
}

int battleTurn(Battler &a, Battler &b, BAct actA, BAct actB, BRng &rng,
               BEvent *ev, int maxEv, bool canRun) {
  if (actA >= BA_COUNT) actA = BA_TACKLE;
  if (actB >= BA_COUNT) actB = BA_TACKLE;
  if (!canRun) {
    if (wildOnly(actA)) actA = BA_TACKLE;
    if (wildOnly(actB)) actB = BA_TACKLE;
  }
  if (a.hp == 0 || b.hp == 0) return 0;
  TurnCtx c;
  c.s[0] = &a; c.s[1] = &b;
  c.rng = &rng; c.ev = ev; c.maxEv = maxEv; c.n = 0; c.canRun = canRun; c.ended = false;
  Battler *side[2] = { &a, &b };
  BAct act[2] = { actA, actB };
  // ko12.10: el que sigue con un movimiento (carga, enfado, rodar, venganza) no puede hacer otra cosa
  for (uint8_t s = 0; s < 2; s++) if (battleLocked(*side[s])) act[s] = BA_M0;
  turnStart(c);
  if (!a.hp || !b.hp) { turnEnd(a, b); return c.n < maxEv ? c.n : maxEv; }

  // prioridad: huir y protegerse van antes que cualquier ataque
  for (uint8_t s = 0; s < 2; s++) {
    Battler &me = *side[s], &op = *side[s ^ 1];
    if (act[s] == BA_RUN) {
      if (!battleCanEscape(me)) {  // ko12.10: atrapado / mal de ojo
        c.msg(s, MSG_NOESCAPE, me.trapT ? me.trapMv : 212);
        continue;
      }
      int ch = runChance(effSpe(me), effSpe(op));
      if ((int)rng.below(100) < ch) {
        push(ev, maxEv, c.n, s, EV_RUN_OK, BA_RUN, 2, false, 0, a, b);
        turnEnd(a, b);
        return c.n < maxEv ? c.n : maxEv;
      }
      // ko12.9.13: al fallar, el rival le da un golpe flojo por la espalda (a suertes; nunca lo debilita)
      uint16_t d = 0;
      if (rng.below(100) < RUN_HIT_PCT && me.hp > 1) {
        uint16_t q = me.maxHp / RUN_HIT_DIV;
        d = (uint16_t)(q + rng.below((uint32_t)q + 1));
        if (d < 1) d = 1;
        if (d >= me.hp) d = me.hp - 1;
        me.hp -= d;
      }
      push(ev, maxEv, c.n, s, EV_RUN_FAIL, BA_RUN, 2, false, d, a, b, 0, (int8_t)(d ? RUN_FAIL_HIT : RUN_FAIL_DODGE));
    } else if (act[s] == BA_POTION) {  // fork KO: cura la mitad de la vida
      uint16_t heal = me.maxHp / 2;
      if (heal < 1) heal = 1;
      if (me.hp + heal > me.maxHp) heal = me.maxHp - me.hp;
      me.hp += heal;
      push(ev, maxEv, c.n, s, EV_HEAL, BA_POTION, 2, false, heal, a, b);
    } else if (act[s] == BA_BALL) {    // fork KO: intento de captura
      if (rng.below(100) < catchChance(op)) {
        push(ev, maxEv, c.n, s, EV_CATCH, BA_BALL, 2, false, 0, a, b);
        turnEnd(a, b);
        return c.n < maxEv ? c.n : maxEv;
      }
      push(ev, maxEv, c.n, s, EV_BREAK, BA_BALL, 2, false, 0, a, b);
    } else if (act[s] == BA_GUARD) {
      me.guard = true;
      uint16_t heal = me.maxHp / 10;
      if (heal < 1) heal = 1;
      if (me.hp + heal > me.maxHp) heal = me.maxHp - me.hp;
      me.hp += heal;
      push(ev, maxEv, c.n, s, EV_GUARD, BA_GUARD, 2, false, heal, a, b);
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
    if (doMoveC(c, s, act[s], k == 0, &landed) || c.ended) { over = true; break; }
    // ko11.8: el otro se protegia y el golpe entro: devuelve un golpe (los dos lados igual)
    if (landed && op.guard && op.hp > 0 && me.hp > 0) {
      uint8_t eff;
      uint16_t d = counterDamage(op, me, rng, &eff);
      if (eff) {
        me.hp -= d;
        push(ev, maxEv, c.n, s ^ 1, EV_COUNTER, BA_TACKLE, eff, false, d, a, b, MOVE_TACKLE);
        if (me.hp == 0) {
          push(ev, maxEv, c.n, s, EV_FAINT, BA_TACKLE, 2, false, 0, a, b);
          over = true;
        }
      }
    }
  }
  if (!over && a.hp && b.hp) endOfTurnC(c);
  turnEnd(a, b);
  return c.n < maxEv ? c.n : maxEv;
}

// ko11.20: cambio voluntario: el rival ataca y yo no hago nada (persecucion: el doble)
int battleFoeOnly(Battler &a, Battler &b, BAct actB, BRng &rng, BEvent *ev, int maxEv) {
  if (a.hp == 0 || b.hp == 0) return 0;
  if (!isAttack(actB)) actB = BA_TACKLE;
  TurnCtx c;
  c.s[0] = &a; c.s[1] = &b;
  c.rng = &rng; c.ev = ev; c.maxEv = maxEv; c.n = 0; c.canRun = false; c.ended = false;
  turnStart(c);
  bool landed = false;
  sPursuitX2 = true;
  if (a.hp && b.hp) doMoveC(c, 1, actB, true, &landed);
  sPursuitX2 = false;
  turnEnd(a, b);
  return c.n < maxEv ? c.n : maxEv;
}

static void clearBattleOnly(Battler &b) {
  if (b.xform) {  // vuelve a ser el de antes
    b.type = b.oType; b.type2 = b.oType2; b.atk = b.oAtk; b.def = b.oDef; b.spe = b.oSpe;
    memcpy(b.mv, b.oMv, 4); memcpy(b.pp, b.oPp, 4);
    b.xform = 0;
  }
  memset(b.stg, 0, sizeof(b.stg));
  b.cnf = 0;
  b.flinch = b.guard = false;
  b.trapT = b.trapMv = 0;
  b.chargeMv = b.invul = 0;
  b.protN = 0;
  b.disMv = b.disT = b.encMv = b.encT = 0;
  b.lastMv = 0;
  b.rampMv = b.rampT = b.rollT = b.furyN = b.bideT = 0;
  b.futT = 0; b.futDmg = 0;
  b.perish = 0;
  b.subHp = 0;
  b.lastDmgP = b.lastDmgS = 0;
  b.bideDmg = 0;
  b.protect = b.endure = b.focus = b.seeded = b.lockon = b.foresight = false;
  b.nightmare = b.cursed = b.dbond = b.attract = b.meanLook = b.rage = false;
  b.curled = b.spkPend = b.minimized = false;
}

// ko11.31: al acabar la batalla se pasan los estados y los cambios de caracteristicas
void battleClearVolatile(Battler &b) {
  clearBattleOnly(b);
  b.st = b.stT = 0;
  b.tox = 0;
  b.restMv = b.restT = 0;
  b.reflT = b.lscrT = b.mistT = b.safeT = 0;
  b.spikes = 0;
  b.fieldWx = b.fieldWxT = 0;
}

void battleSwitchIn(Battler &out, Battler &in) {
  if (&out == &in) return;
  in.fieldWx = out.fieldWx; in.fieldWxT = out.fieldWxT;
  in.spikes = out.spikes;
  in.reflT = out.reflT; in.lscrT = out.lscrT; in.mistT = out.mistT; in.safeT = out.safeT;
  in.spkPend = in.spikes != 0;
  clearBattleOnly(out);
  if (out.tox) out.tox = 1;  // 2a gen: el toxico vuelve a empezar
}

bool linkSideAlive(const LinkSide &s) {
  for (uint8_t i = 0; i < s.n && i < LINK_TEAM; i++) if (s.mon[i].hp) return true;
  return false;
}
bool linkSideCanSwap(const LinkSide &s) {
  for (uint8_t i = 0; i < s.n && i < LINK_TEAM; i++) if (i != s.cur && s.mon[i].hp) return true;
  return false;
}
bool linkApplySwap(LinkSide &s, uint8_t act) {
  uint8_t j = 255;
  if (act >= LA_SWAP && act < LA_SWAP + LINK_TEAM) j = (uint8_t)(act - LA_SWAP);
  if (j >= s.n || j == s.cur || !s.mon[j].hp) {
    if (s.act().hp) return false;  // cambio no valido: se queda el que estaba
    j = 255;
    for (uint8_t i = 0; i < s.n && i < LINK_TEAM; i++) if (s.mon[i].hp) { j = i; break; }
    if (j == 255) return false;
  }
  battleSwitchIn(s.act(), s.mon[j]);  // ko12.10: el que se va pierde lo de la batalla; puas al que entra
  s.cur = j;
  return true;
}
int linkAttacks(LinkSide &A, LinkSide &B, uint8_t actA, uint8_t actB, BRng &rng, BEvent *ev, int maxEv) {
  // ko12.9.16: en tongsin vale la pocion (la gasta cada uno de su bolsa al elegir); huir y la ball no
  if (actA == BA_RUN || actA == BA_BALL) actA = BA_TACKLE;
  if (actB == BA_RUN || actB == BA_BALL) actB = BA_TACKLE;
  bool atkA = actA < BA_COUNT, atkB = actB < BA_COUNT;
  Battler &a = A.act(), &b = B.act();
  if (!a.hp || !b.hp) return 0;
  if (atkA != atkB && (actA == BA_POTION || actB == BA_POTION)) {  // solo uno actua y se cura
    int n = 0;
    uint8_t s = actA == BA_POTION ? 0 : 1;
    Battler &me = s ? b : a;
    uint16_t heal = me.maxHp / 2;
    if (heal < 1) heal = 1;
    if (me.hp + heal > me.maxHp) heal = me.maxHp - me.hp;
    me.hp += heal;
    push(ev, maxEv, n, s, EV_HEAL, BA_POTION, 2, false, heal, a, b);
    return n < maxEv ? n : maxEv;
  }
  if (atkA != atkB) {  // uno cambia y el otro se protege: no pasa nada (protegerse solo vale contra un golpe)
    if ((atkA && actA == BA_GUARD) || (atkB && actB == BA_GUARD)) return 0;
  }
  if (atkA && atkB) return battleTurn(a, b, (BAct)actA, (BAct)actB, rng, ev, maxEv, true);  // true: deja la pocion
  if (atkB) return battleFoeOnly(a, b, (BAct)actB, rng, ev, maxEv);
  if (atkA) {  // solo A: se simula al reves y se dan la vuelta los eventos
    int n = battleFoeOnly(b, a, (BAct)actA, rng, ev, maxEv);
    for (int i = 0; ev && i < n; i++) {
      ev[i].side ^= 1;
      uint16_t t = ev[i].hpA; ev[i].hpA = ev[i].hpB; ev[i].hpB = t;
    }
    return n;
  }
  return 0;
}

uint8_t battleAuto(Battler a, Battler b, uint32_t seed, BEvent *ev, int maxEv, int *nEv) {
  // tongsin: los dos aparatos simulan la misma batalla; su reloj puede no
  // coincidir, asi que aqui siempre hace buen tiempo (si no, se desincronizan)
  uint8_t keepWx = sBattleWx;
  battleSetWeather(WX_CLEAR);
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
  battleSetWeather(keepWx);
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
    out[i] = makeTrainerMon(pick[i < k ? i : 0], trainerLvl((int32_t)petLvl + (i * 4) / (CHAMP_TEAM - 1)));  // ko12.5.1: +0..+4 (antes +2..+6)
}

// ---- ko11.20: equipo contra entrenadores
Battler makeBoxBattler(int16_t dex, uint16_t lvl, uint16_t petLvl, uint8_t gA, uint8_t gD, uint8_t gS) {
  if (dex < 1 || dex > DEX_COUNT) dex = 16;
  if (lvl > petLvl) lvl = petLvl;
  if (lvl < 1) lvl = 1;
  const DexEntry &e = DEX_TBL[dex];
  return makeBattler(dex, lvl, wildStat(e.bAtk, gA, lvl), wildStat(e.bDef, gD, lvl), wildStat(e.bSpe, gS, lvl));
}

uint8_t pickNextFoe(const Battler *team, uint8_t from, uint8_t n, int16_t myDex) {
  uint8_t best = from;
  int bs = -10;
  for (uint8_t i = from; i < n; i++) {
    int sc = typeMatchD(team[i].dex, myDex);  // ko12.10: dos tipos
    if (sc > bs) { bs = sc; best = i; }
  }
  return best;
}

// ko12.10: con los dos tipos de cada especie: mi mejor tipo contra el, y su mejor tipo contra mi
uint8_t dexType2(int16_t dex) { return dex >= 1 && dex <= DEX_COUNT ? DEX_T2[dex] : 0xFF; }
// ko12.10: "불꽃/비행" (un tipo: "불꽃")
const char *typeName(uint8_t type);
const char *dexTypesStr(int16_t dex, char *buf, int n) {
  if (dex < 1 || dex > DEX_COUNT) { if (n) buf[0] = 0; return buf; }
  uint8_t t2 = DEX_T2[dex];
  if (t2 < PT_BATTLE_N && t2 != DEX_TBL[dex].ptype) snprintf(buf, n, "%s/%s", typeName(DEX_TBL[dex].ptype), typeName(t2));
  else snprintf(buf, n, "%s", typeName(DEX_TBL[dex].ptype));
  return buf;
}
int8_t typeMatchD(int16_t mine, int16_t foe) {
  if (mine < 1 || mine > DEX_COUNT || foe < 1 || foe > DEX_COUNT) return 0;
  return typeMatchTT(DEX_TBL[mine].ptype, DEX_T2[mine], DEX_TBL[foe].ptype, DEX_T2[foe]);
}
static uint8_t bestEffQ(const Battler &at, const Battler &df, bool *any) {
  uint8_t best = 0;
  *any = false;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t id = at.mv[i];
    if (!id || moveIsStatus(id)) continue;
    const MoveDef &m = moveDef(id);
    if (m.fx == MX_SONICBOOM || m.fx == MX_DRAGONRAGE || m.fx == MX_LEVEL || m.fx == MX_SUPERFANG ||
        m.fx == MX_COUNTER || m.fx == MX_MIRRORCOAT || m.fx == MX_PSYWAVE) continue;  // dano fijo: el tipo no cuenta
    *any = true;
    uint8_t q = typeEffQ(moveTypeFor(at, id), df.type, df.type2);
    if (battlerHasType(at, moveTypeFor(at, id))) q = (uint8_t)(q * 3 / 2);  // STAB pesa como en el dano
    if (q > best) best = q;
  }
  return best;
}
int8_t typeMatchMoves(const Battler &me, const Battler &foe) {
  bool am, af;
  uint8_t qm = bestEffQ(me, foe, &am), qf = bestEffQ(foe, me, &af);
  if (!am || !af) return typeMatchD(me.dex, foe.dex);
  if (qm >= 12 && qf < 12) return 1;      // x2 con STAB (o x4) contra nada de eso
  if (qf >= 12 && qm < 12) return -1;
  if (qm >= 8 && qf <= 4) return 1;
  if (qf >= 8 && qm <= 4) return -1;
  return 0;
}
int8_t typeMatchTT(uint8_t m1, uint8_t m2, uint8_t f1, uint8_t f2) {
  uint8_t out = typeEffQ(m1, f1, f2), in = typeEffQ(f1, m1, m2);
  if (m2 < PT_BATTLE_N) { uint8_t o = typeEffQ(m2, f1, f2); if (o > out) out = o; }
  if (f2 < PT_BATTLE_N) { uint8_t o = typeEffQ(f2, m1, m2); if (o > in) in = o; }
  if (out > 4 && in <= 4) return 1;
  if (in > 4 && out <= 4) return -1;
  if (out < 4 && in >= 4) return -1;
  return 0;
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
static int16_t formForLevel(int16_t dex, uint16_t lv, BRng &rng, bool allowUp = true) {
  for (int g = 0; g < 3; g++) {  // bajar mientras no llegue al nivel de su evolucion
    int16_t pre = dexPrevo(dex);
    if (pre <= 0) break;
    uint8_t need = evoLevel(pre);
    if (need && lv < need) dex = pre;
    else break;
  }
  for (int g = 0; g < 3 && allowUp && DEX_TBL[dex].evolvesTo; g++) {  // subir si ya le toca
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

void wildMatchPower(Battler &foe, const Battler &me, BRng &rng, bool legend) {
  // ko12.9.22: un legendario no es "de tu talla": 125-140 % de tu fuerza y hasta 8 niveles por encima
  // ko12.9.23 (test/sim): el raro un poco mas fuerte que el comun (salia MAS facil), los primeros niveles
  // algo mas suaves, y el tipo pesa menos (con desventaja se perdia ~70 %): el salvaje que te gana por
  // tipo baja su fuerza y el que pierde por tipo la sube un poco
  uint32_t pct;
  if (legend) pct = WILD_POW_LEGEND - 12 + (me.lvl > 100 ? 100 : me.lvl) * 12 / 100 + rng.below(16);  // Lv40 117..132, Lv100 125..140
  else if (DEX_TBL[foe.dex].rarity == R_RARO) pct = (me.lvl < 20 ? WILD_POW_RARE - 6 : WILD_POW_RARE) + rng.below(16);
  else pct = (me.lvl < 20 ? WILD_POW_LOW : WILD_POW_NORMAL) + rng.below(16);
  int8_t tm = typeMatchMoves(me, foe);  // ko12.10: con sus movimientos (el tipo de la especie ya no basta)
  if (tm < 0) pct = pct * WILD_POW_TYPE_BAD / 100;
  else if (tm > 0) pct = pct * WILD_POW_TYPE_GOOD / 100;
  uint32_t target = battlerPower(me) * pct / 100;
  int anchor = foe.lvl;
  int16_t dex = foe.dex;
  int best = anchor;
  // ko12.9.8: si makeWildIn lo dejo sin evolucionar a proposito (WILD_STAY_PCT: ya tenia nivel para
  // evolucionar), aqui no se vuelve a evolucionar (antes formForLevel lo subia y casi nunca se veian)
  uint8_t el = evoLevel(dex);
  bool keepLow = DEX_TBL[dex].evolvesTo && el && foe.lvl >= el;
  // ko11: nivel de tu talla y luego la forma de ese nivel; si la forma cambia,
  // se vuelve a ajustar el nivel (siempre dentro de -6..+10 del original).
  // ko11.12: y nunca mas de 3 niveles por encima del tuyo (un Pichu Lv9 muy
  // entrenado veia salvajes Lv17-20); lo que falte de fuerza va a las stats
  int hi = anchor + 10;
  int over = legend ? 8 : WILD_LVL_OVER;
  if (hi > (int)me.lvl + over) hi = (int)me.lvl + over;
  int lo = anchor - 6;
  if (lo > hi) lo = hi;
  for (int pass = 0; pass < 3; pass++) {
    best = bestLevelFor(dex, target, lo, hi);
    int16_t nd = formForLevel(dex, (uint16_t)best, rng, !keepLow);
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

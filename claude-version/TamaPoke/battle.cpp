// Batallas por turnos: ver battle.h. Solo enteros (determinista entre placas).
#include "battle.h"
#include "dex.h"
#include "weather.h"
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
static const int16_t WILD_COMMON[] = { 16, 19, 21, 161, 10, 13, 52, 84 };

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

Battler makeWildIn(uint8_t region, uint16_t petLvl, uint8_t hour, uint8_t wx, uint8_t season,
                   BRng &rng, uint8_t *group) {
  if (region >= REGION_COUNT) region = 0;
  uint8_t slot = wildSlot(hour);
  int lv = wildLevel(petLvl, rng);
  // 1. raros
  uint32_t roll = rng.below(1000), acc = 0;
  for (const WildRare &r : WILD_RARE) {
    if (!rareOk(r, region, petLvl, slot, wx, season)) continue;
    acc += r.permil;
    if (roll < acc) { if (group) *group = WG_RARE; return wildBattler(r.dex, lv, rng); }
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
  const int nc = sizeof(WILD_COMMON) / sizeof(WILD_COMMON[0]);
  return wildBattler(WILD_COMMON[rng.below(nc)], lv, rng);
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
  const int nc = sizeof(WILD_COMMON) / sizeof(WILD_COMMON[0]);
  for (int i = 0; i < nc; i++) if (WILD_COMMON[i] == dex) num += (uint64_t)rest * comPct * 1000 / nc;
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

// dano base de un movimiento (sin aleatorio ni critico), para la IA y el calculo
static uint32_t rawDamage(const Battler &at, const Battler &df, uint8_t move, uint8_t *effOut) {
  uint8_t mtype = (move == BA_TYPE) ? at.type : (uint8_t)PT_NORMAL;
  uint32_t pow = (move == BA_TYPE) ? MOVE_TYPE_POW : MOVE_TACKLE_POW;
  uint32_t L = lvlCap(at.lvl);
  uint32_t d = ((2 * L / 5 + 2) * pow * at.atk / (df.def ? df.def : 1)) / 50 + 2;
  if (mtype == at.type) d = d * 3 / 2;  // STAB
  d = d * weatherMul(sBattleWx, mtype) / 2;  // ko10.4: lluvia / sol / nieve
  uint8_t eff = typeEff(mtype, df.type);
  if (effOut) *effOut = eff;
  return d * eff / 2;
}

BAct battleAi(const Battler &self, const Battler &foe, BRng &rng, uint8_t whim) {
  if (self.hp * 100u < self.maxHp * 30u && rng.below(100) < 25) return BA_GUARD;
  uint32_t tackle = rawDamage(self, foe, BA_TACKLE, nullptr) * MOVE_TACKLE_ACC;
  uint32_t typed = rawDamage(self, foe, BA_TYPE, nullptr) * MOVE_TYPE_ACC;
  BAct best = (typed >= tackle) ? BA_TYPE : BA_TACKLE;
  if (rng.below(100) < whim) best = (best == BA_TYPE) ? BA_TACKLE : BA_TYPE;  // capricho
  // nunca elige a proposito un golpe que no hace nada si el otro si hace
  if (best == BA_TYPE && typed == 0 && tackle > 0) best = BA_TACKLE;
  if (best == BA_TACKLE && tackle == 0 && typed > 0) best = BA_TYPE;
  return best;
}

static void push(BEvent *ev, int maxEv, int &n, uint8_t side, uint8_t kind, uint8_t move,
                 uint8_t eff, bool crit, uint16_t dmg, const Battler &a, const Battler &b) {
  if (!ev || n >= maxEv) { n++; return; }
  BEvent &e = ev[n++];
  memset(&e, 0, sizeof(e));  // sin bytes de relleno al azar: los eventos se comparan tal cual
  e.side = side; e.kind = kind; e.move = move; e.eff = eff; e.crit = crit;
  e.dmg = dmg; e.hpA = a.hp; e.hpB = b.hp;
}

// un ataque de "at" a "df"; devuelve true si df se debilito
static bool doAttack(Battler &at, Battler &df, uint8_t move, uint8_t side, BRng &rng,
                     BEvent *ev, int maxEv, int &n, Battler &a, Battler &b) {
  uint8_t acc = (move == BA_TYPE) ? MOVE_TYPE_ACC : MOVE_TACKLE_ACC;
  if (rng.below(100) >= acc) {
    push(ev, maxEv, n, side, EV_MISS, move, 2, false, 0, a, b);
    return false;
  }
  uint8_t eff;
  uint32_t d = rawDamage(at, df, move, &eff);
  bool crit = false;
  if (eff) {
    crit = (rng.below(16) == 0);
    if (crit) d = d * 3 / 2;
    d = d * (85 + rng.below(16)) / 100;
    if (df.guard) d /= 2;
    if (d < 1) d = 1;
  } else {
    d = 0;
  }
  if (d > df.hp) d = df.hp;
  df.hp -= (uint16_t)d;
  push(ev, maxEv, n, side, EV_HIT, move, eff, crit, (uint16_t)d, a, b);
  if (df.hp == 0) {
    push(ev, maxEv, n, side ^ 1, EV_FAINT, move, 2, false, 0, a, b);
    return true;
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
  if (ch < 3) ch = 3;
  if (ch > 90) ch = 90;
  return (uint8_t)ch;
}

static bool wildOnly(BAct a) { return a == BA_RUN || a == BA_POTION || a == BA_BALL; }

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
      int ch = 50 + ((int)me.spe - (int)op.spe) / 2;
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

  // ataques: primero el mas rapido (empate: a suertes)
  uint8_t first = (a.spe > b.spe) ? 0 : (b.spe > a.spe) ? 1 : (uint8_t)rng.below(2);
  for (uint8_t k = 0; k < 2; k++) {
    uint8_t s = k ? (first ^ 1) : first;
    if (act[s] != BA_TACKLE && act[s] != BA_TYPE) continue;
    Battler &me = *side[s], &op = *side[s ^ 1];
    if (me.hp == 0) continue;
    if (doAttack(me, op, act[s], s, rng, ev, maxEv, n, a, b)) break;
  }
  a.guard = b.guard = false;
  return n < maxEv ? n : maxEv;
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

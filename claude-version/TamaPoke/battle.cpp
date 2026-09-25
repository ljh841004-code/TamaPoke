// Batallas por turnos: ver battle.h. Solo enteros (determinista entre placas).
#include "battle.h"
#include "dex.h"
#include <string.h>

// Tabla de tipos gen 1 (atacante x defensor), en mitades: 0 inmune, 1 poco
// eficaz, 2 normal, 4 muy eficaz. Solo hay tipo primario (dex_data.py no lleva
// segundo tipo) y no existe "volador": los voladores de gen 1 son normales aqui.
// Orden: NOR FUE AGU PLA ELE HIE LUC VEN TIE PSI BIC ROC FAN DRA
static const uint8_t TYPE_CHART[PT_COUNT][PT_COUNT] = {
  /* NOR */ { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 2 },
  /* FUE */ { 2, 1, 1, 4, 2, 4, 2, 2, 2, 2, 4, 1, 2, 1 },
  /* AGU */ { 2, 4, 1, 1, 2, 2, 2, 2, 4, 2, 2, 4, 2, 1 },
  /* PLA */ { 2, 1, 4, 1, 2, 2, 2, 1, 4, 2, 1, 4, 2, 1 },
  /* ELE */ { 2, 2, 4, 1, 1, 2, 2, 2, 0, 2, 2, 2, 2, 1 },
  /* HIE */ { 2, 2, 1, 4, 2, 1, 2, 2, 4, 2, 2, 2, 2, 4 },
  /* LUC */ { 4, 2, 2, 2, 2, 4, 2, 1, 2, 1, 1, 4, 0, 2 },
  /* VEN */ { 2, 2, 2, 4, 2, 2, 2, 1, 1, 2, 4, 1, 1, 2 },
  /* TIE */ { 2, 4, 2, 1, 4, 2, 2, 4, 2, 2, 1, 4, 2, 2 },
  /* PSI */ { 2, 2, 2, 2, 2, 2, 4, 4, 2, 1, 2, 2, 2, 2 },
  /* BIC */ { 2, 1, 2, 4, 2, 2, 1, 4, 2, 4, 2, 2, 1, 2 },
  /* ROC */ { 2, 4, 2, 2, 2, 4, 1, 2, 1, 2, 4, 2, 2, 2 },
  /* FAN */ { 0, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 2 },
  /* DRA */ { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 },
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
  uint32_t x = yield * lvlCap(foeLvl) / 4;
  return x ? x : 1;
}

uint32_t careExp(uint16_t lvl) {
  if (lvl >= LEVEL_MAX) return 0;
  uint32_t x = (expForLevel(lvl + 1) - expForLevel(lvl)) / 4;
  return x ? x : 1;
}

static bool hasPreEvo(int16_t dex) {
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].evolvesTo == dex && d != DEX_EEVEE) return true;
  return false;
}

uint8_t evoLevel(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  const DexEntry &e = DEX_TBL[dex];
  if (!e.evolvesTo) return 0;
  bool nextEvolves = dex != DEX_EEVEE && DEX_TBL[e.evolvesTo].evolvesTo != 0;
  if (nextEvolves) return 16;                          // base de 3 fases
  if (hasPreEvo(dex)) return e.evolveLevel < 20 ? 20 : e.evolveLevel;  // intermedia
  return e.evolveLevel;                                // linea de 2 fases
}

static uint16_t wildStat(uint8_t base, uint8_t gene, uint16_t lvl) {
  return (uint16_t)((uint32_t)base * gene / 100 + lvl);
}

Battler makeWild(uint16_t petLvl, BRng &rng) {
  int lv = (int)petLvl - 4 + (int)rng.below(6);  // -4 .. +1
  if (lv < 2) lv = 2;
  if (lv > LEVEL_MAX) lv = LEVEL_MAX;
  // rareza del encuentro
  uint32_t r = rng.below(100);
  uint8_t want = (r < 70) ? (uint8_t)R_COMUN : (r < 98 || petLvl < 40) ? (uint8_t)R_RARO : (uint8_t)R_LEGENDARIO;
  int16_t pool[DEX_COUNT];
  int n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].rarity == want) pool[n++] = d;
  int16_t dex = n ? pool[rng.below(n)] : 16;
  // sube por su linea evolutiva segun el nivel
  for (int guard = 0; guard < 3 && DEX_TBL[dex].evolvesTo; guard++) {
    if (lv < evoLevel(dex)) break;
    dex = (dex == DEX_EEVEE) ? (int16_t)(134 + rng.below(3)) : (int16_t)DEX_TBL[dex].evolvesTo;
  }
  const DexEntry &e = DEX_TBL[dex];
  uint8_t gA = 90 + rng.below(21), gD = 90 + rng.below(21), gS = 90 + rng.below(21);
  return makeBattler(dex, (uint16_t)lv, wildStat(e.bAtk, gA, lv), wildStat(e.bDef, gD, lv),
                     wildStat(e.bSpe, gS, lv));
}

// dano base de un movimiento (sin aleatorio ni critico), para la IA y el calculo
static uint32_t rawDamage(const Battler &at, const Battler &df, uint8_t move, uint8_t *effOut) {
  uint8_t mtype = (move == BA_TYPE) ? at.type : (uint8_t)PT_NORMAL;
  uint32_t pow = (move == BA_TYPE) ? MOVE_TYPE_POW : MOVE_TACKLE_POW;
  uint32_t L = lvlCap(at.lvl);
  uint32_t d = ((2 * L / 5 + 2) * pow * at.atk / (df.def ? df.def : 1)) / 50 + 2;
  if (mtype == at.type) d = d * 3 / 2;  // STAB
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
  if (a.hp == 0) return 1;
  if (b.hp == 0) return 0;
  // tope de turnos: gana quien conserve mas vida (en proporcion)
  uint32_t pa = (uint32_t)a.hp * 1000 / a.maxHp, pb = (uint32_t)b.hp * 1000 / b.maxHp;
  if (pa != pb) return pa > pb ? 0 : 1;
  return (uint8_t)(seed & 1);
}

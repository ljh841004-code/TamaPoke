// Tests de las batallas (battle.cpp) y del intercambio/premios (pet.cpp).
#include "framework.h"
#include "shim/Arduino.h"
#include "../battle.h"
#include "../pet.h"
#include "../dex.h"
#include <string.h>

static void hatchPet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap(); p.eggTap(); p.eggTap();
}

// ---------------------------------------------------------------- tipos
TEST(battle, tabla_de_tipos_clasica) {
  CHECK_EQ(typeEff(PT_WATER, PT_FIRE), (uint8_t)4);
  CHECK_EQ(typeEff(PT_FIRE, PT_WATER), (uint8_t)1);
  CHECK_EQ(typeEff(PT_GRASS, PT_WATER), (uint8_t)4);
  CHECK_EQ(typeEff(PT_ELECTRIC, PT_GROUND), (uint8_t)0);
  CHECK_EQ(typeEff(PT_NORMAL, PT_GHOST), (uint8_t)0);
  CHECK_EQ(typeEff(PT_NORMAL, PT_NORMAL), (uint8_t)2);
  CHECK_EQ(typeEff(PT_DRAGON, PT_DRAGON), (uint8_t)4);
  CHECK_EQ(typeEff(200, 3), (uint8_t)2);  // fuera de rango: neutro, sin leer fuera
}

TEST(battle, todas_las_especies_tienen_tipo_valido) {
  for (int d = 1; d <= DEX_COUNT; d++) CHECK(DEX_TBL[d].ptype < PT_COUNT);
  CHECK_EQ(DEX_TBL[4].ptype, (uint8_t)PT_FIRE);
  CHECK_EQ(DEX_TBL[25].ptype, (uint8_t)PT_ELECTRIC);
  CHECK_EQ(DEX_TBL[94].ptype, (uint8_t)PT_GHOST);
}

TEST(battle, hp_crece_con_el_nivel_y_se_topa_en_100) {
  CHECK(battleHp(45, 10) < battleHp(45, 50));
  CHECK_EQ(battleHp(45, 100), battleHp(45, 999));
  Battler b = makeBattler(4, 20, 60, 50, 70);
  CHECK_EQ(b.hp, b.maxHp);
  CHECK_EQ(b.type, (uint8_t)PT_FIRE);
  Battler bad = makeBattler(999, 0, 0, 0, 0);  // datos basura: no revienta
  CHECK_EQ(bad.dex, (int16_t)1);
  CHECK(bad.atk >= 1 && bad.def >= 1 && bad.lvl >= 1);
}

// ---------------------------------------------------------------- salvajes
TEST(battle, salvaje_acorde_al_nivel) {
  BRng rng(1234);
  for (int i = 0; i < 500; i++) {
    Battler w = makeWild(20, rng);
    CHECK(w.dex >= 1 && w.dex <= DEX_COUNT);
    CHECK(w.lvl >= 16 && w.lvl <= 21);
    CHECK(w.hp == w.maxHp && w.hp > 0);
    CHECK_MSG(DEX_TBL[w.dex].rarity != R_LEGENDARIO, "sin legendarios por debajo de Nv.40");
    // a nivel 17-22 no puede salir una forma que exija evolucionar a 36
    int16_t d = w.dex;
    for (int16_t b = 1; b <= DEX_COUNT; b++)
      if (DEX_TBL[b].evolvesTo == d && b != DEX_EEVEE) CHECK(DEX_TBL[b].evolveLevel <= w.lvl);
  }
  Battler low = makeWild(1, rng);
  CHECK(low.lvl >= 2);
}

// ---------------------------------------------------------------- turnos
TEST(battle, el_mas_rapido_pega_primero) {
  Battler a = makeBattler(25, 30, 80, 60, 150);  // Pikachu rapido
  Battler b = makeBattler(74, 30, 80, 60, 20);   // Geodude lento
  BRng rng(7);
  BEvent ev[BATTLE_MAX_EVENTS];
  int n = battleTurn(a, b, BA_TACKLE, BA_TACKLE, rng, ev, BATTLE_MAX_EVENTS, true);
  CHECK(n >= 2);
  CHECK_EQ(ev[0].side, (uint8_t)0);
}

TEST(battle, inmune_no_hace_dano) {
  Battler a = makeBattler(25, 30, 200, 60, 150);  // electrico
  Battler b = makeBattler(74, 30, 80, 60, 20);    // tierra... pero Geodude es roca aqui
  b.type = PT_GROUND;
  BRng rng(3);
  for (int t = 0; t < 30; t++) {
    BEvent ev[BATTLE_MAX_EVENTS];
    uint16_t before = b.hp;
    int n = battleTurn(a, b, BA_TYPE, BA_GUARD, rng, ev, BATTLE_MAX_EVENTS, false);
    for (int i = 0; i < n; i++)
      if (ev[i].side == 0 && ev[i].kind == EV_HIT) {
        CHECK_EQ(ev[i].eff, (uint8_t)0);
        CHECK_EQ(ev[i].dmg, (uint16_t)0);
      }
    CHECK(b.hp >= before);
  }
}

TEST(battle, proteger_reduce_el_dano_y_cura) {
  uint32_t sinGuard = 0, conGuard = 0;
  for (uint32_t s = 1; s <= 200; s++) {
    Battler a = makeBattler(6, 40, 120, 90, 50), b = makeBattler(9, 40, 90, 120, 150);
    BRng r1(s);
    BEvent ev[BATTLE_MAX_EVENTS];
    battleTurn(a, b, BA_TACKLE, BA_TACKLE, r1, ev, BATTLE_MAX_EVENTS, false);
    sinGuard += a.maxHp - a.hp;
    Battler c = makeBattler(6, 40, 120, 90, 50), d = makeBattler(9, 40, 90, 120, 150);
    BRng r2(s);
    battleTurn(c, d, BA_GUARD, BA_TACKLE, r2, ev, BATTLE_MAX_EVENTS, false);
    conGuard += c.maxHp - c.hp;
    CHECK(!c.guard && !d.guard);  // la guardia dura solo un turno
  }
  CHECK(conGuard < sinGuard);
}

TEST(battle, huir_termina_el_turno_sin_ataques) {
  Battler a = makeBattler(25, 30, 80, 60, 300), b = makeBattler(74, 30, 80, 60, 1);
  BRng rng(11);
  BEvent ev[BATTLE_MAX_EVENTS];
  int n = battleTurn(a, b, BA_RUN, BA_TACKLE, rng, ev, BATTLE_MAX_EVENTS, true);
  bool ok = false;
  for (int i = 0; i < n; i++) if (ev[i].kind == EV_RUN_OK) ok = true;
  if (ok) CHECK_EQ(a.hp, a.maxHp);
  // en tongsin no se puede huir: cuenta como placaje
  Battler c = makeBattler(25, 30, 80, 60, 300), d = makeBattler(74, 30, 80, 60, 1);
  n = battleTurn(c, d, BA_RUN, BA_TACKLE, rng, ev, BATTLE_MAX_EVENTS, false);
  for (int i = 0; i < n; i++) CHECK(ev[i].kind != EV_RUN_OK && ev[i].kind != EV_RUN_FAIL);
}

TEST(battle, el_hp_de_los_eventos_cuadra) {
  BRng rng(99);
  Battler a = makeBattler(1, 25, 70, 70, 60), b = makeBattler(4, 25, 75, 60, 70);
  for (int t = 0; t < 60 && a.hp && b.hp; t++) {
    BEvent ev[BATTLE_MAX_EVENTS];
    int n = battleTurn(a, b, battleAi(a, b, rng), battleAi(b, a, rng), rng, ev, BATTLE_MAX_EVENTS, false);
    CHECK(n <= BATTLE_MAX_EVENTS);
    if (n) {
      CHECK_EQ(ev[n - 1].hpA, a.hp);
      CHECK_EQ(ev[n - 1].hpB, b.hp);
    }
    CHECK(a.hp <= a.maxHp && b.hp <= b.maxHp);
  }
}

// la batalla de tongsin se simula en las dos placas: misma semilla, mismo final
TEST(battle, batalla_automatica_determinista) {
  Battler a = makeBattler(7, 30, 70, 90, 60), b = makeBattler(4, 30, 85, 60, 80);
  for (uint32_t seed = 1; seed < 300; seed++) {
    BEvent e1[160], e2[160];
    int n1 = 0, n2 = 0;
    uint8_t w1 = battleAuto(a, b, seed, e1, 160, &n1);
    uint8_t w2 = battleAuto(a, b, seed, e2, 160, &n2);
    CHECK_EQ(w1, w2);
    CHECK_EQ(n1, n2);
    CHECK(memcmp(e1, e2, sizeof(BEvent) * n1) == 0);
    CHECK(w1 <= 1);
    uint8_t w3 = battleAuto(a, b, seed, nullptr, 0, nullptr);  // sin buffer
    CHECK_EQ(w1, w3);
  }
}

TEST(battle, ventaja_de_tipo_se_nota) {
  // Squirtle contra Charmander parejos: el agua debe ganar la gran mayoria
  int winsA = 0;
  for (uint32_t seed = 1; seed <= 200; seed++) {
    Battler a = makeBattler(7, 30, 80, 80, 70), b = makeBattler(4, 30, 80, 80, 70);
    if (battleAuto(a, b, seed, nullptr, 0, nullptr) == 0) winsA++;
  }
  CHECK_MSG(winsA > 150, "el tipo agua deberia imponerse al fuego");
}

TEST(battle, fantasma_contra_normal_no_se_eterniza) {
  // ni el normal puede tocar al fantasma ni al reves con su tipo: el tope de
  // turnos tiene que cerrar la batalla con un ganador
  Battler a = makeBattler(92, 30, 60, 60, 60), b = makeBattler(19, 30, 60, 60, 60);
  uint8_t w = battleAuto(a, b, 5, nullptr, 0, nullptr);
  CHECK(w <= 1);
}

// ---------------------------------------------------------------- premios
TEST(battle, ganar_entrena_y_cuenta) {
  Pet p;
  hatchPet(p, 4);
  p.energy = 80; p.joy = 50;
  uint8_t a0 = p.trAtk;
  p.battleResult(BATTLE_WILD, true, false);
  CHECK_EQ(p.wildWins, (uint16_t)1);
  CHECK(p.trAtk > a0);
  CHECK(p.joy > 50);
  CHECK(p.energy < 80);
  p.battleResult(BATTLE_LINK, false, false);
  CHECK_EQ(p.linkBattles, (uint16_t)1);
  CHECK_EQ(p.linkWins, (uint16_t)0);
  // y se guarda
  Pet q;
  q.begin();
  CHECK_EQ(q.wildWins, (uint16_t)1);
  CHECK_EQ(q.linkBattles, (uint16_t)1);
}

TEST(battle, huir_no_da_premio) {
  Pet p;
  hatchPet(p, 4);
  uint8_t a0 = p.trAtk;
  p.battleResult(BATTLE_WILD, false, true);
  CHECK_EQ(p.trAtk, a0);
  CHECK_EQ(p.wildWins, (uint16_t)0);
}

TEST(battle, el_huevo_no_pelea) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(!p.canBattle());
  p.battleResult(BATTLE_WILD, true, false);
  CHECK_EQ(p.wildWins, (uint16_t)0);
}

// ---------------------------------------------------------------- intercambio
TEST(trade, exporta_e_importa) {
  Pet a;
  hatchPet(a, 7);
  a.rename("TORTU");
  a.trAtk = 30;
  TradePet t;
  a.exportTrade(t);
  CHECK_EQ(t.dex, (int16_t)7);
  CHECK(strcmp(t.nick, "TORTU") == 0);

  Pet b;
  hatchPet(b, 4);
  b.bond = 50;
  CHECK(b.importTrade(t));
  CHECK_EQ(b.speciesId, (int16_t)7);
  CHECK_EQ(b.trAtk, (uint8_t)30);
  CHECK_EQ(b.bond, (uint8_t)0);
  CHECK(strcmp(b.nick, "TORTU") == 0);
  CHECK(b.isRegistered(7));
  CHECK_EQ(b.trades, (uint16_t)1);
}

TEST(trade, evoluciona_por_intercambio) {
  Pet a;
  hatchPet(a, 4);
  TradePet t;
  a.exportTrade(t);
  t.dex = 64;  // Kadabra
  CHECK(a.importTrade(t));
  CHECK_EQ(a.speciesId, (int16_t)65);  // Alakazam
  CHECK_EQ(a.prevSpeciesId, (int16_t)64);
  CHECK(a.evolving());
  CHECK(a.isRegistered(64));
  CHECK(a.isRegistered(65));
}

TEST(trade, rechaza_datos_basura) {
  Pet a;
  hatchPet(a, 4);
  TradePet t;
  a.exportTrade(t);
  t.dex = 0;
  CHECK(!a.importTrade(t));
  t.dex = 300;
  CHECK(!a.importTrade(t));
  CHECK_EQ(a.speciesId, (int16_t)4);
  // valores fuera de rango se acotan; apodo con bytes raros se filtra
  t.dex = 25;
  t.geneAtk = 255; t.geneDef = 0; t.trAtk = 250; t.weight = 200;
  t.ageMinutes = 0xFFFFFFFF;
  memset(t.nick, 0xFF, sizeof(t.nick));  // sin terminador
  CHECK(a.importTrade(t));
  CHECK_EQ(a.geneAtk, (uint8_t)110);
  CHECK_EQ(a.geneDef, (uint8_t)90);
  CHECK_EQ(a.trAtk, (uint8_t)100);
  CHECK_EQ(a.weight, (uint8_t)100);
  CHECK(a.level() <= 999);
  CHECK_EQ(a.nick[0], (char)0);
}

// Tests de las batallas (battle.cpp) y del intercambio/premios (pet.cpp).
#include "framework.h"
#include "shim/Arduino.h"
#include "../battle.h"
#include "../weather.h"
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
    // a nivel 16-21 no puede salir una forma que exija evolucionar a 36 (las
    // que salen directas, como Pikachu con su bebe Pichu, no cuentan: ko10)
    int16_t d = w.dex;
    int16_t b = dexPrevo(d);
    if (b && DEX_TBL[d].rarity == R_EVO) CHECK(evoLevel(b) <= w.lvl);
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

// ko11.8: protegerse y recibir el golpe -> contraataque (a los dos lados igual)
TEST(battle, contraataque_tras_protegerse) {
  int counters = 0, misses = 0;
  for (uint32_t s = 1; s <= 300; s++) {
    for (int side = 0; side < 2; side++) {
      Battler a = makeBattler(6, 40, 120, 90, 50), b = makeBattler(9, 40, 90, 120, 150);
      BRng rng(s);
      BEvent ev[BATTLE_MAX_EVENTS];
      BAct actA = side ? BA_TACKLE : BA_GUARD, actB = side ? BA_GUARD : BA_TACKLE;
      int n = battleTurn(a, b, actA, actB, rng, ev, BATTLE_MAX_EVENTS, false);
      CHECK(n <= BATTLE_MAX_EVENTS);
      uint8_t guarder = side ? 1 : 0;
      bool hit = false, miss = false, counter = false;
      for (int i = 0; i < n; i++) {
        if (ev[i].kind == EV_HIT && ev[i].side != guarder && ev[i].dmg) hit = true;
        if (ev[i].kind == EV_MISS) miss = true;
        if (ev[i].kind == EV_COUNTER) {
          counter = true;
          CHECK_EQ(ev[i].side, guarder);          // devuelve el que se protegia
          CHECK(!ev[i].crit);
          CHECK(ev[i].dmg >= 1);
          CHECK(i > 0 && ev[i - 1].kind == EV_HIT);  // justo despues del golpe recibido
          CHECK_EQ(ev[i].hpA, a.hp);  // (ultimo evento del turno en este caso)
        }
      }
      CHECK_EQ(counter, hit);  // solo si el golpe entro
      if (counter) counters++;
      if (miss) { misses++; CHECK(!counter); }
    }
  }
  CHECK(counters > 300);
  CHECK(misses > 0);
}

TEST(battle, contraataque_mas_flojo_que_un_placaje) {
  Battler a = makeBattler(6, 40, 120, 90, 50), b = makeBattler(9, 40, 90, 120, 150);
  uint32_t full = 0, cnt = 0;
  for (uint32_t s = 1; s <= 200; s++) {
    BRng r(s);
    uint8_t eff;
    cnt += counterDamage(a, b, r, &eff);
    CHECK_EQ(eff, (uint8_t)2);
  }
  for (uint32_t s = 1; s <= 200; s++) {  // placajes normales que entran (sin critico)
    Battler x = a, y = b;
    BRng r(s);
    BEvent ev[BATTLE_MAX_EVENTS];
    int n = battleTurn(x, y, BA_TACKLE, BA_GUARD, r, ev, BATTLE_MAX_EVENTS, false);
    for (int i = 0; i < n; i++)
      if (ev[i].kind == EV_HIT && ev[i].side == 0 && !ev[i].crit) full += ev[i].dmg * 2u;  // x2: el escudo lo partio
  }
  CHECK(cnt * 10 < full * 7);  // ~ la mitad de un placaje (los fallos bajan 'full' un poco)
  CHECK(cnt * 10 > full * 3);
  Battler g = makeBattler(92, 40, 90, 90, 90);  // fantasma: el placaje no le hace nada
  BRng r(1);
  uint8_t eff = 9;
  CHECK_EQ(counterDamage(a, g, r, &eff), (uint16_t)0);
  CHECK_EQ(eff, (uint8_t)0);
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
  CHECK(a.level() <= LEVEL_MAX);
  CHECK_EQ(a.nick[0], (char)0);
}

// fork KO (ko7): el salvaje sigue al nivel de la mascota y se topa en 100
TEST(battle, salvaje_nivel_topado_en_100) {
  BRng rng(99);
  for (int i = 0; i < 200; i++) {
    Battler w = makeWild(100, rng);
    CHECK(w.lvl >= 96 && w.lvl <= 100);
    Battler v = makeWild(5, rng);
    CHECK(v.lvl >= 2 && v.lvl <= 6);
  }
}

TEST(battle, exp_de_batalla_crece_con_el_nivel_del_rival) {
  CHECK(battleExp(16, 10) > battleExp(16, 5));
  CHECK(battleExp(150, 50) > battleExp(16, 50));  // Mewtwo rinde mas que Pidgey
  CHECK_EQ(battleExp(0, 10), (uint32_t)0);
  CHECK(battleExp(129, 1) >= 1);
  CHECK_EQ(levelForExp(expForLevel(37)), (uint16_t)37);
  CHECK_EQ(careMinutesForLevel(100), (uint32_t)0);
  CHECK_EQ(careMinutesForLevel(1), (uint32_t)15);   // ko10.2: 15 min x nivel
  CHECK_EQ(careMinutesForLevel(2), (uint32_t)30);
  CHECK_EQ(careMinutesForLevel(35), (uint32_t)525);
  // ko10.2: EXP de batalla x0,75 (Pidgey Lv10: (40+45+40+56)/3 = 60 -> 60*10*3/16 = 112)
  CHECK_EQ(battleExp(16, 10), (uint32_t)112);
}

// ---------------------------------------------------------------- ko10.1: regiones
TEST(region, pikachu_mucho_mas_frecuente_en_la_central) {
  // 13:00, despejado, otono, nivel 20 (sin legendarios)
  uint16_t en6 = wildPermil(25, 6, 20, 13, WX_CLEAR, SEASON_AUTUMN);
  uint16_t en0 = wildPermil(25, 0, 20, 13, WX_CLEAR, SEASON_AUTUMN);
  CHECK_RANGE((int)en6, 60, 120);  // ~8-11 % (de dia la central no tiene grupo de hora)
  CHECK_EQ((int)en0, 0);           // fuera de su region no sale
  // la simulacion cuadra con la cuenta (nivel 5: nadie evoluciona, ni Pichu)
  uint16_t lo = wildPermil(25, 6, 5, 13, WX_CLEAR, SEASON_AUTUMN);
  BRng rng(1234);
  int hits = 0, N = 20000;
  for (int i = 0; i < N; i++) {
    Battler b = makeWildIn(6, 5, 13, WX_CLEAR, SEASON_AUTUMN, rng, nullptr);
    if (b.dex == 25) hits++;
  }
  CHECK_RANGE(hits * 1000 / N, (int)lo - 15, (int)lo + 15);
}

TEST(region, la_hora_decide_los_de_hora) {
  // Murkrow (198) en el cementerio: solo de noche (no es de su region)
  CHECK(wildPermil(198, 12, 20, 23, WX_CLEAR, SEASON_SPRING) > 0);
  CHECK_EQ((int)wildPermil(198, 12, 20, 13, WX_CLEAR, SEASON_SPRING), 0);
  // Gastly sale en el cementerio a cualquier hora (grupo de region)
  CHECK(wildPermil(92, 12, 20, 13, WX_CLEAR, SEASON_SPRING) > 100);
  // Hoothoot (163) solo de noche en la pradera (ademas de su grupo de region)
  CHECK(wildPermil(163, 0, 20, 2, WX_CLEAR, SEASON_SPRING) > wildPermil(163, 0, 20, 12, WX_CLEAR, SEASON_SPRING));
  CHECK_EQ((int)wildSlot(5), (int)WS_NIGHT);
  CHECK_EQ((int)wildSlot(6), (int)WS_MORNING);
  CHECK_EQ((int)wildSlot(10), (int)WS_DAY);
  CHECK_EQ((int)wildSlot(20), (int)WS_NIGHT);
}

TEST(region, legendarios_solo_con_su_condicion_y_nivel) {
  // Raikou: central + lluvia + nivel 40
  CHECK_EQ((int)wildPermil(243, 6, 45, 13, WX_RAIN, SEASON_SPRING), 5);
  CHECK_EQ((int)wildPermil(243, 6, 45, 13, WX_CLEAR, SEASON_SPRING), 0);
  CHECK_EQ((int)wildPermil(243, 6, 30, 13, WX_RAIN, SEASON_SPRING), 0);   // nivel bajo
  CHECK_EQ((int)wildPermil(243, 1, 45, 13, WX_RAIN, SEASON_SPRING), 0);   // otra region
  // Articuno solo nevando, Celebi con cerezos, Ho-Oh mananas de sol
  CHECK_EQ((int)wildPermil(144, 5, 45, 13, WX_SNOW, SEASON_WINTER), 5);
  CHECK_EQ((int)wildPermil(144, 5, 45, 13, WX_CLEAR, SEASON_WINTER), 0);
  CHECK_EQ((int)wildPermil(251, 2, 45, 13, WX_BLOSSOM, SEASON_SPRING), 5);
  CHECK_EQ((int)wildPermil(250, 9, 45, 8, WX_SUNNY, SEASON_SUMMER), 5);
  CHECK_EQ((int)wildPermil(250, 9, 45, 13, WX_SUNNY, SEASON_SUMMER), 0);
  // en ningun caso sale un legendario sin su condicion
  BRng rng(99);
  for (int i = 0; i < 20000; i++) {
    uint8_t reg = (uint8_t)(i % REGION_COUNT), g = 0;
    Battler b = makeWildIn(reg, 60, 13, WX_CLEAR, SEASON_AUTUMN, rng, &g);
    if (DEX_TBL[b.dex].rarity == R_LEGENDARIO) {
      CHECK_EQ((int)g, (int)WG_RARE);
      CHECK(b.dex == 145);  // de dia, despejado, otono: solo Zapdos en la central
      CHECK_EQ((int)reg, 6);
    }
  }
}

TEST(region, todas_las_regiones_dan_especies_validas_y_suman_mil) {
  static const uint8_t WXS[] = { WX_CLEAR, WX_RAIN, WX_SNOW, WX_SUNNY, WX_BLOSSOM, WX_LEAVES };
  for (uint8_t reg = 0; reg < REGION_COUNT; reg++)
    for (uint8_t h : { 7, 13, 23 })
      for (uint8_t wx : WXS) {
        uint32_t sum = 0;
        for (int16_t d = 1; d <= DEX_COUNT; d++) sum += wildPermil(d, reg, 50, h, wx, SEASON_SPRING);
        CHECK_RANGE((int)sum, 950, 1000);  // cada especie redondea por abajo
        // el grupo de la region existe (al menos 2 especies distintas)
        int n = 0;
        for (int16_t d = 1; d <= DEX_COUNT; d++)
          if (wildPermil(d, reg, 50, h, wx, SEASON_SPRING) >= 20) n++;
        CHECK(n >= 3);
      }
  BRng rng(7);
  for (int i = 0; i < 5000; i++) {
    Battler b = makeWildIn((uint8_t)(i % 16), (uint16_t)(2 + i % 99), (uint8_t)(i % 24), WX_RAIN,
                           SEASON_SUMMER, rng, nullptr);
    CHECK(b.dex >= 1 && b.dex <= DEX_COUNT);
    CHECK(b.lvl >= 2 && b.lvl <= LEVEL_MAX);
  }
}

// ---------------------------------------------------------------- ko10.4: tiempo, gimnasios, reto
TEST(weather_battle, lluvia_y_sol_cambian_el_dano) {
  Battler a = makeBattler(7, 30, 60, 60, 60);   // Squirtle (agua)
  Battler b = makeBattler(19, 30, 60, 60, 60);  // Rattata
  auto dmg = [&](uint8_t wx) {
    battleSetWeather(wx);
    Battler x = a, y = b;
    BRng r(5);
    BEvent ev[BATTLE_MAX_EVENTS];
    int n = battleTurn(x, y, BA_TYPE, BA_GUARD, r, ev, BATTLE_MAX_EVENTS, true);
    for (int i = 0; i < n; i++) if (ev[i].kind == EV_HIT && ev[i].side == 0) return (int)ev[i].dmg;
    return -1;
  };
  int clear = dmg(WX_CLEAR), rain = dmg(WX_RAIN), sun = dmg(WX_SUNNY);
  battleSetWeather(WX_CLEAR);
  CHECK(clear > 0);
  CHECK(rain > clear);   // agua x1,5 con lluvia
  CHECK(sun < clear);    // agua x0,5 con sol
  CHECK_EQ((int)weatherMul(WX_SNOW, PT_ICE), 3);
  CHECK_EQ((int)weatherMul(WX_SNOW, PT_FIRE), 2);
  // tongsin (battleAuto): siempre buen tiempo, y no cambia el de fuera
  battleSetWeather(WX_RAIN);
  int n1 = 0, n2 = 0;
  uint8_t w1 = battleAuto(a, b, 77, nullptr, 0, &n1);
  battleSetWeather(WX_CLEAR);
  uint8_t w2 = battleAuto(a, b, 77, nullptr, 0, &n2);
  CHECK_EQ((int)w1, (int)w2);
  battleSetWeather(WX_RAIN);
  battleAuto(a, b, 77, nullptr, 0, &n1);
  CHECK_EQ((int)battleWeather(), (int)WX_RAIN);
  battleSetWeather(WX_CLEAR);
}

TEST(gym, ocho_gimnasios_y_regiones_por_medalla) {
  for (int i = 0; i < GYM_COUNT; i++) {
    const GymDef &g = GYMS[i];
    CHECK(g.n >= 1 && g.n <= GYM_MAX_TEAM);
    for (int j = 0; j < g.n; j++) CHECK(g.dex[j] >= 1 && g.dex[j] <= DEX_COUNT);
    // la region del gimnasio i ya esta abierta con i medallas (las necesarias para llegar)
    CHECK_MSG(regionBadgesNeeded(g.region) <= i, "gimnasio en region aun cerrada");
    if (i) CHECK(g.lv[g.n - 1] >= GYMS[i - 1].lv[GYMS[i - 1].n - 1]);  // cada vez mas fuerte
  }
  int open0 = 0;
  for (uint8_t r = 0; r < REGION_COUNT; r++) if (regionUnlocked(r, 0)) open0++;
  CHECK_EQ(open0, 8);
  CHECK(regionUnlocked(13, 0xFF));
  CHECK(!regionUnlocked(13, 0x7F));  // el valle del dragon, con la 8a
  CHECK_EQ((int)badgeCount(0xA5), 4);
  Battler t = makeTrainerMon(95, 14);
  CHECK_EQ((int)t.dex, 95);
  CHECK_EQ((int)t.lvl, 14);
}

TEST(daily, igual_todo_el_dia_y_cambia_otro_dia) {
  uint32_t day = 1790343900u / 86400u;
  Battler a[DAILY_TEAM], b[DAILY_TEAM], c[DAILY_TEAM];
  dailyTeam(day, 20, a);
  dailyTeam(day, 20, b);
  for (int i = 0; i < DAILY_TEAM; i++) { CHECK_EQ((int)a[i].dex, (int)b[i].dex); CHECK_EQ((int)a[i].lvl, (int)b[i].lvl); }
  int diff = 0;
  for (uint32_t d = day + 1; d < day + 8; d++) {
    dailyTeam(d, 20, c);
    for (int i = 0; i < DAILY_TEAM; i++) if (c[i].dex != a[i].dex) diff++;
    CHECK(dailyRegion(d) < REGION_COUNT);
  }
  CHECK(diff > 0);
  for (int i = 0; i < DAILY_TEAM; i++) CHECK(a[i].lvl >= 17 && a[i].lvl <= 24);
}

TEST(moves, fase_del_ataque_segun_evolucion) {
  CHECK_EQ((int)moveTier(7), 0);    // Squirtle: Pistola Agua
  CHECK_EQ((int)moveTier(8), 1);    // Wartortle: Hidropulso
  CHECK_EQ((int)moveTier(9), 2);    // Blastoise: Hidrobomba
  CHECK_EQ((int)moveTier(172), 0);  // Pichu (bebe)
  CHECK_EQ((int)moveTier(25), 1);   // Pikachu
  CHECK_EQ((int)moveTier(26), 2);   // Raichu
  CHECK_EQ((int)moveTier(129), 0);  // Magikarp
  CHECK_EQ((int)moveTier(130), 2);  // Gyarados
  CHECK_EQ((int)moveTier(128), 1);  // Tauros (sin evoluciones)
  CHECK_EQ((int)moveTier(150), 2);  // Mewtwo (legendario)
  CHECK_EQ((int)moveTier(134), 2);  // Vaporeon (rama de Eevee)
  for (int d = 1; d <= DEX_COUNT; d++) CHECK(moveTier(d) <= 2);
}

// ko10.11: revancha = 3-4 del tipo del gimnasio, sin legendarios, a tu nivel
TEST(gym, revancha_del_tipo_del_gimnasio_a_tu_nivel) {
  for (uint8_t g = 0; g < GYM_COUNT; g++) {
    for (uint32_t seed = 1; seed < 40; seed++) {
      Battler t[REMATCH_MAX];
      uint8_t n = gymRematchTeam(g, 50, seed, t);
      CHECK(n >= 3 && n <= REMATCH_MAX);
      for (uint8_t i = 0; i < n; i++) {
        CHECK_EQ((int)DEX_TBL[t[i].dex].ptype, (int)GYM_TYPE[g]);
        CHECK(DEX_TBL[t[i].dex].rarity != R_LEGENDARIO);
        CHECK(t[i].lvl >= 49 && t[i].lvl <= 53);
        for (uint8_t j = 0; j < i; j++) CHECK(t[i].dex != t[j].dex);
      }
    }
  }
  // varia de una vez a otra (no siempre los mismos)
  Battler a[REMATCH_MAX], b[REMATCH_MAX];
  gymRematchTeam(1, 30, 11, a);
  bool diff = false;
  for (uint32_t s = 12; s < 30 && !diff; s++) { gymRematchTeam(1, 30, s, b); diff = b[0].dex != a[0].dex; }
  CHECK(diff);
  // niveles con tope
  gymRematchTeam(0, 100, 7, a);
  CHECK(a[0].lvl <= LEVEL_MAX);
}

// ko10.11: liga = 6 formas finales fuertes, sin legendarios ni repetidos, nivel +2..+6
TEST(gym, liga_seis_fuertes_sin_repetir) {
  for (uint32_t seed = 1; seed < 60; seed++) {
    Battler t[CHAMP_TEAM];
    championTeam(40, seed, t);
    for (uint8_t i = 0; i < CHAMP_TEAM; i++) {
      const DexEntry &e = DEX_TBL[t[i].dex];
      CHECK(e.rarity != R_LEGENDARIO);
      CHECK_EQ((int)e.evolvesTo, 0);
      CHECK(t[i].lvl >= 42 && t[i].lvl <= 46);
      for (uint8_t j = 0; j < i; j++) CHECK(t[i].dex != t[j].dex);
    }
  }
}

// ko10.11: el salvaje sale con una fuerza parecida a la tuya
TEST(wild, salvaje_de_tu_talla) {
  Battler me = makeBattler(9, 50, 120, 130, 110);  // un Blastoise Lv50 entrenado
  uint32_t mp = battlerPower(me);
  int ok = 0, total = 0;
  for (uint32_t s = 1; s < 200; s++) {
    BRng r(s);
    int16_t d = (int16_t)(1 + r.below(DEX_COUNT));
    Battler f = makeBattler(d, 47, 60, 60, 60);
    wildMatchPower(f, me, r);
    CHECK(f.lvl >= 41 && f.lvl <= 57);
    CHECK_EQ((int)DEX_FAM[f.dex], (int)DEX_FAM[d]);  // misma familia (ko11: forma segun nivel)
    uint32_t fp = battlerPower(f);
    total++;
    if (fp >= mp * 70 / 100 && fp <= mp * 125 / 100) ok++;
  }
  CHECK_MSG(ok * 100 / total >= 70, "la mayoria cerca de tu fuerza (las muy flojas/fuertes topan en el rango)");
  // un Caterpie frente a un Lv50 sale con mas nivel que un Dragonite
  BRng r1(3), r2(3);
  Battler c = makeBattler(10, 47, 1, 1, 1), g = makeBattler(149, 47, 1, 1, 1);
  wildMatchPower(c, me, r1);
  wildMatchPower(g, me, r2);
  CHECK(c.lvl > g.lvl);
  // ko11: la forma cuadra con el nivel: nada de un Caterpie Lv50 ni un Dragonite Lv20
  for (uint32_t sd = 1; sd < 300; sd++) {
    BRng r(sd);
    int16_t d = (int16_t)(1 + r.below(DEX_COUNT));
    Battler f = makeBattler(d, (uint16_t)(5 + r.below(80)), 50, 50, 50);
    Battler m = makeBattler(9, (uint16_t)(5 + r.below(90)), (uint16_t)(20 + r.below(200)), 100, 100);
    wildMatchPower(f, m, r);
    if (DEX_TBL[f.dex].evolvesTo && evoLevel(f.dex)) CHECK(f.lvl < evoLevel(f.dex));
    int16_t pre = dexPrevo(f.dex);
    if (pre > 0 && evoLevel(pre)) CHECK(f.lvl >= evoLevel(pre));
  }
}

// ko11.7: eventos del dia
TEST(event, dias_y_luna) {
  // 2026-09-28 lunes 10:00 (hora local) -> agua
  DayEvent e = dayEvent(1790589600u);
  CHECK_EQ(e.kind, (uint8_t)DEV_TYPE);
  CHECK_EQ(e.ptype, (uint8_t)PT_WATER);
  // 2026-09-27 domingo -> shiny x2
  CHECK_EQ(dayEvent(1790503200u).kind, (uint8_t)DEV_SHINY);
  CHECK_EQ(dayEvent(0).kind, (uint8_t)DEV_NONE);
  // luna llena 2026-09-26 16:49 UTC; luna nueva 2024-01-11
  CHECK(fullMoon(1790441340u));
  CHECK(!fullMoon(1704974220u));
  // de noche con luna llena
  CHECK(dayEvent(1790463600u).moonNight);   // 2026-09-26 23:00
  CHECK(!dayEvent(1790434800u).moonNight);  // 2026-09-26 15:00 (de dia)
}

TEST(event, dia_de_tipo_sube_ese_tipo) {
  DayEvent ev = { DEV_TYPE, PT_FIRE, false };
  int withEv = 0, without = 0;
  for (int i = 0; i < 3000; i++) {
    BRng a(1000 + i), b(1000 + i);
    if (DEX_TBL[makeWildIn(0, 20, 12, 0, 0, a, nullptr, &ev).dex].ptype == PT_FIRE) withEv++;
    if (DEX_TBL[makeWildIn(0, 20, 12, 0, 0, b, nullptr).dex].ptype == PT_FIRE) without++;
  }
  CHECK(withEv > without + 300);
}

// ko11.7: expediciones: mas horas, mas premio
TEST(expedition, premio_crece_con_las_horas) {
  int newMon[3] = {0, 0, 0}, rare[3] = {0, 0, 0};
  const uint8_t H[3] = {2, 4, 8};
  for (int k = 0; k < 3; k++)
    for (int i = 0; i < 2000; i++) {
      BRng r(77 + i);
      ExpReward e = expeditionReward(H[k], 20, r);
      newMon[k] += e.newMon; rare[k] += e.rare;
      if (i == 0) CHECK(e.candy >= 3 && e.balls >= 1);
    }
  CHECK(newMon[0] < newMon[1] && newMon[1] < newMon[2]);
  CHECK(rare[0] < rare[2]);
  BRng r(1);
  CHECK(expeditionReward(8, 50, r).candy > expeditionReward(2, 50, r).candy);
}

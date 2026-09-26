// Tests de la logica de la mascota (pet.cpp) sobre los shims del host.
#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#include "../dex.h"
#include <string.h>

// ---------------------------------------------------------------- helpers
static void advance(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

// mascota recien eclosionada de la especie pedida, con la NVS limpia
static void makePet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap();
  p.eggTap();
  p.eggTap();  // 3 toques = eclosion inmediata
}

static void setStats(Pet &p, uint8_t f, uint8_t j, uint8_t e, uint8_t h) {
  p.fullness = f; p.joy = j; p.energy = e; p.hygiene = h;
}

// ---------------------------------------------------------------- huevo
TEST(egg, primera_partida_pide_inicial) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  CHECK(p.awaitingStarter());
  CHECK_EQ(p.eggCracks(), (uint8_t)0);
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

// regresion v1.3: el huevo eclosionaba solo a los 3 min mientras el jugador
// aun estaba eligiendo inicial, y se perdia la eleccion
TEST(egg, no_eclosiona_mientras_se_elige_inicial) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.awaitingStarter());
  advance(p, 600);  // 10 horas de juego
  CHECK_MSG(p.isEgg(), "el huevo NO debe eclosionar durante la eleccion");
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  p.chooseStarter(7);
  CHECK(!p.awaitingStarter());
  advance(p, 3);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)7);
}

TEST(egg, eclosiona_sola_a_los_3_minutos) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(1);
  advance(p, 2);
  CHECK(p.isEgg());
  advance(p, 1);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)1);
}

TEST(egg, tres_toques_eclosionan) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(4);
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)1);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)2);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, eclosionar_sortea_genes_y_resetea_al_individuo) {
  Pet p;
  makePet(p, 4);
  CHECK_RANGE(p.geneAtk, 90, 110);
  CHECK_RANGE(p.geneDef, 90, 110);
  CHECK_RANGE(p.geneSpe, 90, 110);
  CHECK_EQ(p.trAtk, (uint8_t)0);
  CHECK_EQ(p.trDef, (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_EQ(p.bond, (uint8_t)0);
  CHECK_EQ(p.berryKnown, false);
  CHECK_STREQ(p.nick, "");
  CHECK_MSG(p.isRegistered(4), "criar = registrar en la pokedex");
  CHECK_EQ(p.registeredCount(), (uint16_t)1);
}

TEST(egg, tocar_huevo_no_hace_nada_si_ya_eclosiono) {
  Pet p;
  makePet(p, 4);
  uint8_t cracks = p.eggCracks();
  p.eggTap();
  CHECK_EQ(p.eggCracks(), cracks);
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, rareza_del_huevo_es_valida) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_RANGE(p.eggRarity(), (uint8_t)R_COMUN, (uint8_t)R_LEGENDARIO);
}

// ---------------------------------------------------------------- comida
TEST(feed, baya_normal_sube_25_y_topa_en_100) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  uint8_t notLoved = (uint8_t)((p.speciesId + 1) % 3);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)75);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)100);  // 100 en vez de 105
}

TEST(feed, baya_favorita_sube_mas_y_da_corazon) {
  Pet p;
  makePet(p, 1);  // ko9: base 1 % 4 == 1 -> le gusta la azul
  CHECK(p.lovesBerry(1));
  CHECK(!p.lovesBerry(0));
  CHECK(!p.lovesBerry(2));
  p.fullness = 10;
  p.joy = 10;
  p.feedBerry(1);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK_EQ(p.joy, (uint8_t)20);
  CHECK(p.berryKnown);
  CHECK(p.showHeart());
  CHECK(p.eating());
}

// ko9: favorita entre 4 (tambien la chuche) y la misma en toda la linea
TEST(feed, favorita_fija_al_evolucionar_y_puede_ser_la_chuche) {
  Pet p;
  makePet(p, 4);                   // CHARMANDER: base 4 % 4 = 0 (roja)
  CHECK_EQ(p.favFood(), (uint8_t)0);
  p.speciesId = 6;                 // CHARIZARD: misma linea, misma favorita
  CHECK_EQ(p.favFood(), (uint8_t)0);
  p.speciesId = 135;               // JOLTEON: sale de EEVEE (133 % 4 = 1)
  CHECK_EQ(p.favFood(), (uint8_t)1);
  Pet q;
  makePet(q, 7);                   // SQUIRTLE: 7 % 4 = 3 -> la chuche
  CHECK(q.lovesBerry(3));
  q.fullness = 10;
  q.feedCandy();
  CHECK_EQ(q.fullness, (uint8_t)45);  // como una baya favorita
  CHECK(q.berryKnown);
  CHECK(q.showHeart());
  int n = 0;
  for (int d = 1; d <= 151; d++) {
    Pet r;
    r.speciesId = (int16_t)d;
    if (r.favFood() == 3) n++;
  }
  CHECK(n > 20);                   // una parte buena de especies prefiere la chuche
}

TEST(feed, chuche_engorda) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  p.joy = 50;
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)60);
  CHECK_EQ(p.joy, (uint8_t)62);
  CHECK_EQ(p.weight, (uint8_t)12);
}

TEST(feed, dormido_o_huevo_no_come) {
  Pet p;
  makePet(p, 4);
  p.fullness = 40;
  p.toggleLight();  // a dormir
  CHECK(p.sleeping);
  p.feedBerry(1);
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)40);

  Pet e;
  mockNvsReset();
  e.begin();
  e.chooseStarter(4);
  e.fullness = 40;
  e.feedBerry(0);
  CHECK_EQ(e.fullness, (uint8_t)40);
}

// ---------------------------------------------------------------- tick
TEST(tick, despierto_baja_comida_2_y_energia_1_por_minuto) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 100, 100);
  p.poops = 0;
  mockForceRandom(99);  // random(100) < 15 falso: sin cacas
  advance(p, 10);
  mockClearForcedRandom();
  CHECK_EQ(p.fullness, (uint8_t)80);
  CHECK_EQ(p.energy, (uint8_t)90);
  CHECK_EQ(p.hygiene, (uint8_t)90);
  CHECK_EQ(p.joy, (uint8_t)90);
  CHECK_EQ(p.ageMinutes, (uint32_t)10);
  CHECK_EQ(p.poops, (uint8_t)0);
}

TEST(tick, las_barras_nunca_bajan_de_cero) {
  Pet p;
  makePet(p, 4);
  setStats(p, 5, 3, 2, 1);
  advance(p, 30);
  CHECK_EQ(p.fullness, (uint8_t)0);
  CHECK_EQ(p.joy, (uint8_t)0);
  CHECK_EQ(p.energy, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)0);
}

TEST(tick, dormir_recupera_energia_con_suelos) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 10, 100);
  p.toggleLight();
  advance(p, 20);
  CHECK_MSG(p.energy == 100, "durmiendo la energia sube 6/min hasta 100");
  CHECK_MSG(p.fullness > 80, "durmiendo el hambre baja ~4x mas lento");
  advance(p, 2000);  // dormir muchisimo: los suelos protegen
  CHECK_MSG(p.fullness >= 30, "suelo de comida durmiendo = 30");
  CHECK_MSG(p.joy >= 35, "suelo de felicidad durmiendo = 35");
  CHECK_MSG(p.hygiene >= 45, "suelo de higiene durmiendo = 45");
  CHECK_EQ(p.careMistakes, (uint8_t)0);
}

TEST(tick, sobrepeso_se_quema_solo) {
  Pet p;
  makePet(p, 4);
  p.weight = 30;
  advance(p, 30);
  CHECK_EQ(p.weight, (uint8_t)20);  // -1 cada 3 minutos
}

TEST(tick, descuido_cuenta_una_vez_por_hora) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.careMistakes, (uint8_t)1);
  advance(p, 50);
  CHECK_MSG(p.careMistakes == 1, "el enfriamiento evita contar el mismo descuido cada minuto");
  advance(p, 11);
  CHECK_EQ(p.careMistakes, (uint8_t)2);
}

// regresion v1.4: el descuido restaba 3 de vinculo cada 30 min y era imposible
// recuperarlo; ahora resta 1 y nunca lo deja en 0
TEST(tick, descuido_enfria_el_vinculo_sin_arrasarlo) {
  Pet p;
  makePet(p, 4);
  p.bond = 50;
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.bond, (uint8_t)49);
  p.bond = 1;
  advance(p, 61);
  CHECK_MSG(p.bond == 1, "el vinculo no cae por debajo de 1");
}

TEST(tick, buen_cuidado_12h_forja_defensa) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trDef, (uint8_t)0);
  for (int h = 0; h < 720; h++) {  // mantener todo >= 40 durante 12 h
    setStats(p, 100, 100, 100, 100);
    advance(p, 1);
  }
  CHECK_EQ(p.trDef, (uint8_t)1);
}

// ko10.6: 12 h seguidas bien cuidado perdonan un descuido (y la DEF sigue subiendo)
TEST(tick, buen_cuidado_12h_perdona_un_descuido) {
  Pet p;
  makePet(p, 4);
  p.careMistakes = 2;
  for (int m = 0; m < GOOD_CARE_TICKS - 1; m++) { setStats(p, 100, 100, 100, 100); advance(p, 1); }
  CHECK_EQ(p.careMistakes, (uint8_t)2);
  CHECK_EQ(p.goodCareTicks(), (uint16_t)(GOOD_CARE_TICKS - 1));
  setStats(p, 100, 100, 100, 100); advance(p, 1);
  CHECK_EQ(p.careMistakes, (uint8_t)1);
  CHECK_EQ(p.trDef, (uint8_t)1);
  CHECK_EQ(p.goodCareTicks(), (uint16_t)0);
  // bajar de 40 reinicia la cuenta
  for (int m = 0; m < 600; m++) { setStats(p, 100, 100, 100, 100); advance(p, 1); }
  setStats(p, 100, 100, 100, 35); p.poops = 0; advance(p, 1);  // < 40 pero sin descuido
  CHECK_EQ(p.goodCareTicks(), (uint16_t)0);
  CHECK_EQ(p.careMistakes, (uint8_t)1);
  // sin descuidos no baja de 0
  p.careMistakes = 0;
  for (int m = 0; m < GOOD_CARE_TICKS; m++) { setStats(p, 100, 100, 100, 100); advance(p, 1); }
  CHECK_EQ(p.careMistakes, (uint8_t)0);
}

// ko10.6: la racha se guarda (un reinicio no la borra)
TEST(tick, racha_de_buen_cuidado_se_guarda) {
  Pet p;
  makePet(p, 4);
  for (int m = 0; m < 300; m++) { setStats(p, 100, 100, 100, 100); advance(p, 1); }
  p.saveNow();
  Pet q;
  q.begin();
  CHECK_EQ(q.goodCareTicks(), (uint16_t)300);
}

// ---------------------------------------------------------------- niveles
// fork KO (ko7): el nivel sale de la EXP (curva n^3, tope 100), no de la edad
TEST(level, sale_de_la_exp_con_curva_cubica) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.level(), (uint16_t)1);
  p.exp = 7;
  CHECK_EQ(p.level(), (uint16_t)1);
  p.exp = 8;
  CHECK_EQ(p.level(), (uint16_t)2);
  p.exp = expForLevel(16) - 1;
  CHECK_EQ(p.level(), (uint16_t)15);
  p.exp = expForLevel(16);
  CHECK_EQ(p.level(), (uint16_t)16);
  p.ageMinutes = 999999;  // la edad ya no sube el nivel
  CHECK_EQ(p.level(), (uint16_t)16);
}

TEST(level, tope_en_100) {
  Pet p;
  makePet(p, 4);
  p.exp = 0xFFFFFFF0u;
  CHECK_EQ(p.level(), (uint16_t)100);
  p.exp = expForLevel(100) - 5;
  p.addExp(0xFFFFFFFFu);  // no desborda
  CHECK_EQ(p.exp, expForLevel(100));
  CHECK_EQ(p.level(), (uint16_t)100);
}

TEST(level, addexp_cuenta_los_niveles_subidos) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.addExp(7), (uint16_t)0);
  CHECK_EQ(p.addExp(1), (uint16_t)1);                // 8 = Lv2
  CHECK_EQ(p.addExp(expForLevel(5) - 8), (uint16_t)3);  // Lv5
  Pet e;
  mockNvsReset();
  e.begin();
  CHECK_EQ(e.addExp(1000), (uint16_t)0);             // el huevo no gana EXP
  CHECK_EQ(e.exp, (uint32_t)0);
}

// ko10.2: solo con el tiempo, Lv1 -> 2 en 15 min, Lv2 -> 3 en 30 min, Lv3 -> 4 en 45 min
// (antes 30 min x nivel). Vale para cualquier Pokemon, no depende de la especie
TEST(level, tiempo_de_crianza_15_min_por_nivel) {
  for (int16_t dex : { 4, 7, 147 }) {
    Pet p;
    makePet(p, dex);
    CHECK_EQ(p.careMinutesLeft(), (uint32_t)15);
    for (int i = 0; i < 14; i++) { setStats(p, 100, 100, 100, 100); advance(p, 1); }
    CHECK_EQ(p.level(), (uint16_t)1);
    CHECK_EQ(p.careMinutesLeft(), (uint32_t)1);
    setStats(p, 100, 100, 100, 100); advance(p, 1);
    CHECK_EQ(p.level(), (uint16_t)2);
    CHECK_EQ(p.careMinutesLeft(), (uint32_t)30);
    for (int i = 0; i < 30; i++) { setStats(p, 100, 100, 100, 100); advance(p, 1); }
    CHECK_EQ(p.level(), (uint16_t)3);
    CHECK_EQ(p.careMinutesLeft(), (uint32_t)45);
    // dormido tambien cuenta
    p.toggleLight();
    for (int i = 0; i < 45; i++) { setStats(p, 100, 100, 100, 100); advance(p, 1); }
    CHECK_EQ(p.level(), (uint16_t)4);
  }
  // descuido total (una barra <= 10): ese tiempo no cuenta
  Pet q;
  makePet(q, 4);
  for (int i = 0; i < 60; i++) { setStats(q, 5, 100, 100, 100); advance(q, 1); }
  CHECK_EQ(q.exp, (uint32_t)0);
  // la EXP de batalla adelanta: tras una victoria falta menos tiempo
  Pet r;
  makePet(r, 4);
  r.exp = expForLevel(10);
  uint32_t m0 = r.careMinutesLeft();
  CHECK_EQ(m0, (uint32_t)150);
  r.addExp((expForLevel(11) - expForLevel(10)) / 2);
  CHECK(r.careMinutesLeft() >= 74 && r.careMinutesLeft() <= 76);  // la mitad del camino
}

// ko10.2: un Pokemon a medio nivel con la cuenta de crianza de antes no pierde nada
TEST(level, cuenta_de_crianza_antigua_sigue_valiendo) {
  Pet p;
  makePet(p, 7);
  p.exp = expForLevel(16);
  // con la regla vieja (30 min) a Lv16 llevaba 20 min acumulados; ahora sube antes
  uint32_t span = expForLevel(17) - expForLevel(16);
  p.careAcc = span * 20;
  setStats(p, 100, 100, 100, 100); advance(p, 1);
  CHECK_EQ(p.level(), (uint16_t)16);
  CHECK(p.careMinutesLeft() <= 15u * 16 - 20);
}

TEST(level, ganar_una_batalla_da_exp_y_perder_no) {
  Pet p;
  makePet(p, 4);
  p.battleResult(BATTLE_WILD, true, false, false, 16, 5);
  CHECK_EQ(p.exp, battleExp(16, 5));
  CHECK_EQ(p.lastExpGain, battleExp(16, 5));
  CHECK(p.lastLvlUp > 0);
  uint32_t e = p.exp;
  p.battleResult(BATTLE_WILD, false, false, false, 16, 5);
  CHECK_EQ(p.exp, e);
  CHECK_EQ(p.lastExpGain, (uint32_t)0);
  p.battleResult(BATTLE_WILD, false, true, false, 16, 5);  // huir
  CHECK_EQ(p.exp, e);
  p.battleResult(BATTLE_WILD, false, false, true, 16, 5);  // capturar
  CHECK_EQ(p.exp, e + battleExp(16, 5));
  e = p.exp;
  p.battleResult(BATTLE_LINK, true, false, false, 16, 5);  // tongsin: la mitad
  CHECK_EQ(p.exp, e + battleExp(16, 5) / 2);
}

// el Lv338 de ko6: guardado sin clave "exp" -> Lv1 con la misma especie
TEST(level, guardado_antiguo_empieza_en_nivel_1) {
  Pet p;
  makePet(p, 5);
  p.ageMinutes = 20160;
  p.exp = 0;
  p.saveNow();
  Pet q;
  q.begin();
  CHECK_EQ(q.speciesId, (int16_t)5);
  CHECK_EQ(q.level(), (uint16_t)1);
  CHECK_EQ(q.ageMinutes, (uint32_t)20160);
}

// ---------------------------------------------------------------- evolucion
TEST(evolve, requiere_nivel_y_buen_cuidado) {
  Pet p;
  makePet(p, 4);  // CHARMANDER evoluciona a nivel 16
  p.exp = expForLevel(15);  // nivel 15
  CHECK(!p.canEvolveNow());
  p.exp = expForLevel(16);  // nivel 16
  CHECK(p.canEvolveNow());
  p.fullness = 39;  // una barra por debajo de 40 lo bloquea
  CHECK(!p.canEvolveNow());
  p.fullness = 40;
  CHECK(p.canEvolveNow());
}

TEST(evolve, cada_descuido_retrasa_un_nivel) {
  Pet p;
  makePet(p, 4);
  p.exp = expForLevel(16);  // nivel 16
  p.careMistakes = 1;
  CHECK(!p.canEvolveNow());
  p.exp = expForLevel(17);  // nivel 17
  CHECK(p.canEvolveNow());
}

// Arreglado en v1.8: el cast a uint8_t de (evolveLevel + careMistakes) desbordaba
TEST(evolve, muchos_descuidos_no_adelantan_la_evolucion) {
  Pet p;
  makePet(p, 4);       // CHARMANDER: evoluciona a nivel 16
  p.careMistakes = 250;  // 250 descuidos: deberia estar lejisimos de evolucionar
  p.exp = expForLevel(10);  // nivel 10
  CHECK_MSG(!p.canEvolveNow(), "con 250 descuidos no puede evolucionar a nivel 10");
}

TEST(evolve, no_evoluciona_dormido_ni_de_huevo_ni_en_ceremonia) {
  Pet p;
  makePet(p, 4);
  p.exp = expForLevel(21);
  CHECK(p.canEvolveNow());
  p.toggleLight();
  CHECK(!p.canEvolveNow());
  p.toggleLight();
  p.startFarewell();
  CHECK(!p.canEvolveNow());

  Pet e;
  mockNvsReset();
  e.begin();
  CHECK(!e.canEvolveNow());
}

TEST(evolve, transforma_registra_y_anima) {
  Pet p;
  makePet(p, 4);
  p.exp = expForLevel(21);
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)5);
  CHECK_EQ(p.prevSpeciesId, (int16_t)4);
  CHECK(p.isRegistered(5));
  CHECK(p.evolving());
  CHECK_RANGE(p.evolveT(), 0.0f, 1.0f);
  mockAdvanceMillis(EVOLVE_ANIM_MS + 1);
  CHECK(!p.evolving());
}

TEST(evolve, forma_final_no_evoluciona) {
  Pet p;
  makePet(p, 6);  // CHARIZARD
  p.exp = expForLevel(201);
  CHECK(!p.canEvolveNow());
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)6);
}

TEST(evolve, eevee_se_ramifica_en_134_136) {
  for (int seed = 1; seed <= 12; seed++) {
    randomSeed(seed);
    Pet p;
    makePet(p, DEX_EEVEE);
    p.exp = expForLevel(41);
    CHECK(p.canEvolveNow());
    p.evolve();
    // ko10: 5 ramas (Vaporeon, Jolteon, Flareon, Espeon, Umbreon)
    int16_t s = p.speciesId;
    CHECK(s == 134 || s == 135 || s == 136 || s == 196 || s == 197);
    CHECK(p.isRegistered(p.speciesId));
  }
}

TEST(evolve, boton_de_evolucion_se_pospone_al_declinar) {
  Pet p;
  makePet(p, 4);
  p.exp = expForLevel(16);  // nivel 16
  CHECK(p.wantEvolveButton());
  p.declineEvolve();
  CHECK_MSG(!p.wantEvolveButton(), "'mantener forma' quita el boton");
  CHECK_MSG(p.canEvolveNow(), "pero la evolucion sigue disponible");
  p.exp = expForLevel(17);  // sube de nivel: se re-ofrece
  CHECK(p.wantEvolveButton());
}

// ---------------------------------------------------------------- stats
TEST(stats, formula_base_por_genes_mas_nivel_y_entreno) {
  Pet p;
  makePet(p, 4);  // CHARMANDER: atk 52, def 43, spe 65
  p.geneAtk = p.geneDef = p.geneSpe = 100;
  p.trAtk = p.trDef = p.trSpe = 0;
  p.exp = 0;  // nivel 1
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 1));
  CHECK_EQ(p.defStat(), (uint16_t)(43 + 1));
  CHECK_EQ(p.speStat(), (uint16_t)(65 + 1));
  p.exp = expForLevel(10);  // nivel 10
  p.trAtk = 20;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 10 + 20));
  p.geneAtk = 110;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 * 110 / 100 + 10 + 20));
}

TEST(stats, el_huevo_no_tiene_stats) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_EQ(p.atkStat(), (uint16_t)0);
  CHECK_EQ(p.defStat(), (uint16_t)0);
  CHECK_EQ(p.speStat(), (uint16_t)0);
}

TEST(train, saco_da_un_punto_cada_4_golpes_con_tope) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trainStrength(0, 0), (uint8_t)0);
  CHECK_EQ(p.trainStrength(40, 4), (uint8_t)10);
  CHECK_EQ(p.trAtk, (uint8_t)10);
  CHECK_MSG(p.trainStrength(1000, 25) == 18, "tope de 18 por sesion");
  CHECK_EQ(p.strHi, (uint16_t)25);  // ko10.7: el record son los sacos rotos
}

// ko10.7: entrenar siempre da EXP (5% del nivel); batir el record, 20% + 1 caramelo
TEST(train, premio_de_sesion_y_mas_si_bate_el_record) {
  Pet p;
  makePet(p, 4);
  uint16_t L = p.level();
  uint32_t step = expForLevel(L + 1) - expForLevel(L);
  uint16_t c0 = p.candyOf(p.speciesId);
  p.trainDefense(10);  // primer intento: record (0 -> 10)
  CHECK_EQ(p.lastTrainExp, step * TRAIN_EXP_PCT_HI / 100 ? step * TRAIN_EXP_PCT_HI / 100 : 1);
  CHECK_EQ(p.lastTrainCandy, (uint8_t)1);
  CHECK_EQ(p.candyOf(p.speciesId), (uint16_t)(c0 + 1));
  p.energy = 100;
  L = p.level();
  step = expForLevel(L + 1) - expForLevel(L);
  p.trainDefense(8);   // peor que el record: solo el premio pequeno
  CHECK_EQ(p.lastTrainExp, step * TRAIN_EXP_PCT / 100 ? step * TRAIN_EXP_PCT / 100 : 1);
  CHECK_EQ(p.lastTrainCandy, (uint8_t)0);
  CHECK_EQ(p.candyOf(p.speciesId), (uint16_t)(c0 + 1));
  p.trainSpeed(0, 0);  // no hizo nada: sin premio
  CHECK_EQ(p.lastTrainExp, (uint32_t)0);
  p.trainStrength(30, 2);  // record de sacos (0 -> 2): caramelo
  CHECK_EQ(p.lastTrainCandy, (uint8_t)1);
  CHECK_EQ(p.strHi, (uint16_t)2);
}

// Arreglado al portar la bateria: trainStrength() devolvia la subida teorica
TEST(train, el_saco_anuncia_lo_que_de_verdad_sube) {
  Pet p;
  makePet(p, 4);
  p.trAtk = 95;
  uint8_t before = p.trAtk;
  uint8_t gain = p.trainStrength(72, 3);  // 72/4 = 18, pero solo caben 5
  CHECK_EQ(gain, (uint8_t)(p.trAtk - before));
}

TEST(train, entrenamiento_topa_en_100) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 20; i++) p.trainStrength(80, 3);
  CHECK_EQ(p.trAtk, (uint8_t)100);
}

TEST(train, el_saco_cansa_y_quema_peso) {
  Pet p;
  makePet(p, 4);
  p.weight = 40;
  p.energy = 80;
  p.fullness = 80;
  p.trainStrength(30, 3);
  CHECK_EQ(p.energy, (uint8_t)68);
  CHECK_EQ(p.fullness, (uint8_t)75);
  CHECK_EQ(p.weight, (uint8_t)30);
  p.energy = 3;  // el suelo protege de dejarlo a cero
  p.trainStrength(30, 3);
  CHECK_EQ(p.energy, (uint8_t)3);
}

// fork KO (ko4): la pelota ya no entrena la velocidad (tiene su juego propio)
// ko9.2: solo el record premia (animo + energia); sin record, ni premio ni cansancio
TEST(play, minijuego_record_da_animo_y_energia_y_si_no_poca_energia) {
  Pet p;
  makePet(p, 4);
  p.joy = 10;
  p.energy = 40;
  p.fullness = 60;
  p.weight = 50;
  CHECK(p.playResult(20));
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_EQ(p.joy, (uint8_t)50);     // +10 +30 por marcar mas de 15
  CHECK_EQ(p.energy, (uint8_t)60);  // +10 +10
  CHECK_EQ(p.fullness, (uint8_t)60);  // no da hambre
  CHECK_EQ(p.weight, (uint8_t)10);  // 50 - 20*2
  CHECK_EQ(p.gameHi, (uint16_t)20);
  uint8_t j = p.joy, e = p.energy;
  CHECK(!p.playResult(5));
  CHECK_MSG(p.gameHi == 20, "un resultado peor no baja el record");
  CHECK_EQ(p.joy, j);                                  // ko10.3: sin record, animo igual
  CHECK_EQ(p.energy, (uint8_t)(e + GAME_SMALL_ENERGY));  // ... pero un poco de energia
  CHECK(!p.playResult(20));         // igualar no es batir
  CHECK_EQ(p.joy, j);
  CHECK_EQ(p.energy, (uint8_t)(e + 2 * GAME_SMALL_ENERGY));
  e = p.energy;
  CHECK(!p.playResult(0));          // sin ningun toque: nada
  CHECK_EQ(p.energy, e);
}

TEST(play, jugar_sube_felicidad_y_cansa) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.play();
  CHECK_EQ(p.joy, (uint8_t)75);
  CHECK_EQ(p.energy, (uint8_t)40);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK(p.showHeart());
}

TEST(care, limpiar_quita_cacas_y_deja_higiene_a_tope) {
  Pet p;
  makePet(p, 4);
  p.poops = 3;
  p.hygiene = 10;
  p.clean();
  CHECK_EQ(p.poops, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)100);
}

TEST(care, mimar_sube_felicidad_y_vinculo) {
  Pet p;
  makePet(p, 4);
  p.joy = 50;
  p.caress();
  CHECK_EQ(p.joy, (uint8_t)55);
  CHECK_EQ(p.bond, (uint8_t)1);
  CHECK(p.showHeart());
}

TEST(care, el_vinculo_tiene_tope_diario) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 50; i++) p.play();  // +2 de vinculo cada vez
  CHECK_MSG(p.bond <= 20, "tope de 20 puntos de vinculo al dia");
  CHECK_EQ(p.bond, (uint8_t)20);
}

TEST(mood, refleja_el_estado) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 80);
  CHECK_EQ((int)p.mood(), (int)MOOD_HAPPY);
  p.joy = 20;
  CHECK_EQ((int)p.mood(), (int)MOOD_SAD);
  p.joy = 80;
  p.feedBerry(0);
  CHECK_EQ((int)p.mood(), (int)MOOD_EATING);
  mockAdvanceMillis(EAT_ANIM_MS + 1);
  p.toggleLight();
  CHECK_EQ((int)p.mood(), (int)MOOD_SLEEPING);
}

// ---------------------------------------------------------------- pokedex
TEST(dexreg, bitmap_cubre_1_a_151) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0, sizeof(p.dexReg));
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  for (int16_t d = 1; d <= 151; d++) {
    p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
    CHECK(p.isRegistered(d));
  }
  CHECK_EQ(p.registeredCount(), (uint16_t)151);
}

TEST(dexreg, fuera_de_rango_no_esta_registrado) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0xFF, sizeof(p.dexReg));
  CHECK(!p.isRegistered(0));
  CHECK(!p.isRegistered(-1));
  CHECK(p.isRegistered(251));     // ko10: gen 2
  CHECK(!p.isRegistered(252));
  CHECK(!p.isRegistered(32767));
  CHECK_EQ(p.registeredCount(), (uint16_t)DEX_COUNT);
}

TEST(dexreg, linea_incompleta_se_detecta) {
  Pet p;
  makePet(p, 1);  // BULBASAUR -> IVYSAUR -> VENUSAUR
  CHECK(p.lineHasUnregistered(1));
  p.dexReg[(2 - 1) >> 3] |= (uint8_t)(1 << ((2 - 1) & 7));
  p.dexReg[(3 - 1) >> 3] |= (uint8_t)(1 << ((3 - 1) & 7));
  CHECK_MSG(!p.lineHasUnregistered(1), "linea 1-2-3 completa");
}

TEST(dexreg, la_rama_de_eevee_cuenta_las_tres) {
  Pet p;
  makePet(p, DEX_EEVEE);
  CHECK(p.lineHasUnregistered(DEX_EEVEE));
  for (int16_t d = 134; d <= 136; d++)
    p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
  CHECK_MSG(p.lineHasUnregistered(DEX_EEVEE), "ko10: faltan Espeon y Umbreon");
  for (int16_t d = 196; d <= 197; d++)
    p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
  CHECK(!p.lineHasUnregistered(DEX_EEVEE));
}

TEST(egg, el_sorteo_siempre_da_una_especie_valida) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 400; i++) {
    randomSeed(i * 7919 + 1);
    int16_t d = p.pickEggSpecies();
    CHECK_RANGE(d, (int16_t)1, (int16_t)DEX_COUNT);
    CHECK_MSG(DEX_TBL[d].rarity != R_EVO, "de un huevo nunca sale una forma evolucionada");
  }
}

TEST(egg, el_primer_huevo_es_un_inicial_clasico) {
  for (int seed = 1; seed <= 20; seed++) {
    randomSeed(seed);
    mockNvsReset();
    Pet p;
    p.begin();
    int16_t d = p.pickEggSpecies();
    bool classic = false;
    for (int i = 0; i < NUM_CLASSIC_DEX; i++)
      if (CLASSIC_DEX[i] == d) classic = true;
    CHECK_MSG(classic, "sin pokedex, solo salen iniciales clasicos");
  }
}

TEST(egg, una_escapada_castiga_la_rareza) {
  Pet p;
  makePet(p, 4);
  p.lastEnd = CER_RUNAWAY;
  for (int seed = 1; seed <= 60; seed++) {
    randomSeed(seed);
    int16_t d = p.pickEggSpecies();
    CHECK_MSG(DEX_TBL[d].rarity == R_COMUN, "tras una escapada solo salen comunes");
  }
}

// ---------------------------------------------------------------- racha
TEST(streak, el_primer_cuidado_del_dia_suma) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_EQ(p.bestStreak, (uint16_t)1);
  p.caress();
  CHECK_MSG(p.streak == 1, "el segundo cuidado del mismo dia no suma");
  p.setClock(101u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)2);
}

TEST(streak, un_dia_perdido_reinicia) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  p.setClock(101u * 86400);
  p.caress();
  p.setClock(105u * 86400);  // hueco de dias
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_MSG(p.bestStreak == 2, "el record de racha se conserva");
}

TEST(streak, sin_reloj_no_hay_racha) {
  Pet p;
  makePet(p, 4);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)0);
}

TEST(streak, celebra_los_hitos) {
  Pet p;
  makePet(p, 4);
  for (uint32_t d = 100; d < 103; d++) {
    p.setClock(d * 86400);
    p.caress();
  }
  CHECK_EQ(p.streak, (uint16_t)3);
  CHECK_MSG(p.showMilestone(), "hito de 3 dias celebrado");
  CHECK_EQ(p.lastMilestone, (uint16_t)3);
}

TEST(bonus, racha_y_vinculo_mejoran_el_huevo) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.careBonus(), 0);
  p.streak = 30;
  p.bond = 100;
  CHECK_EQ(p.careBonus(), 14);
  p.streak = 300;  // la racha se capa a 30
  CHECK_EQ(p.careBonus(), 14);
}

// ---------------------------------------------------------------- medallas
TEST(medals, nivel_10_da_medalla) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.hasMedal(MED_LV10));
  p.exp = expForLevel(10) - 1;
  p.addExp(1);
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.showMedal());
}

// sin sobrepeso, nivel 5 y cero descuidos
TEST(medals, en_forma_llega_al_nivel_5) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.exp = expForLevel(5) - 1;
  p.addExp(1);
  CHECK(p.hasMedal(MED_FIT));
  CHECK_EQ(p.totalMedals, (uint16_t)1);

  Pet q;
  makePet(q, 4);
  q.careMistakes = 1;   // un descuido la deja fuera
  q.exp = expForLevel(5) - 1;
  q.addExp(1);
  CHECK(!q.hasMedal(MED_FIT));
}

TEST(medals, cada_medalla_nueva_suma_al_total) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.exp = expForLevel(10) - 1;
  p.addExp(1);  // cruza nivel 10: MED_LV10 + MED_FIT de golpe
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.hasMedal(MED_FIT));
  CHECK_MSG(p.totalMedals == 2, "el total cuenta cada bit ganado, no cada evento");
}

TEST(medals, forma_final_al_nacer) {
  Pet p;
  makePet(p, 143);  // SNORLAX ya es forma final
  CHECK(p.hasMedal(MED_FINAL));
  CHECK_EQ(p.totalMedals, (uint16_t)1);
}

TEST(medals, la_baya_favorita_da_medalla) {
  Pet p;
  makePet(p, 1);
  p.feedBerry(1);  // su favorita
  CHECK(p.berryKnown);
  advance(p, 1);
  CHECK(p.hasMedal(MED_BERRY));
}

TEST(medals, el_total_acumula_entre_crianzas) {
  Pet p;
  makePet(p, 143);
  CHECK_EQ(p.totalMedals, (uint16_t)1);
  p.newEgg();
  p.chooseStarter(144);  // ARTICUNO, otra forma final
  p.eggTap(); p.eggTap(); p.eggTap();
  CHECK_MSG(p.medals == MED_FINAL, "las medallas del individuo se reinician");
  CHECK_MSG(p.totalMedals == 2, "el total del jugador se conserva");
}

// ---------------------------------------------------------------- ceremonias
TEST(ceremony, despedida_solo_en_forma_final_y_a_los_3_dias) {
  Pet p;
  makePet(p, 6);  // CHARIZARD, forma final
  p.ageMinutes = FAREWELL_AGE_MIN - 1;
  CHECK(!p.canFarewellNow());
  p.ageMinutes = FAREWELL_AGE_MIN;
  CHECK(p.canFarewellNow());

  Pet q;
  makePet(q, 4);  // CHARMANDER no es forma final
  q.ageMinutes = FAREWELL_AGE_MIN * 3;
  CHECK(!q.canFarewellNow());
}

TEST(ceremony, la_despedida_deja_un_huevo_nuevo) {
  Pet p;
  makePet(p, 6);
  p.ageMinutes = FAREWELL_AGE_MIN;
  p.startFarewell();
  CHECK_EQ(p.ceremony, (uint8_t)CER_FAREWELL);
  CHECK_RANGE(p.ceremonyT(), 0.0f, 1.0f);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_FAREWELL);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

TEST(ceremony, escaparse_pide_una_hora_de_abandono_total) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  CHECK(!p.canRunawayNow());
  advance(p, RUNAWAY_TICKS - 1);
  CHECK(!p.canRunawayNow());
  advance(p, 1);
  CHECK(p.canRunawayNow());
}

TEST(ceremony, un_solo_cuidado_salva_del_abandono) {
  Pet p;
  makePet(p, 4);
  p.dbgRunawayReady();
  CHECK(p.canRunawayNow());
  p.feedBerry(0);
  advance(p, 1);
  CHECK_MSG(!p.canRunawayNow(), "comer resetea el contador de abandono");
}

TEST(ceremony, durante_la_ceremonia_no_se_puede_interactuar) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.poops = 2;
  p.startRunaway();
  p.feedBerry(0);
  p.feedCandy();
  p.play();
  p.clean();
  p.caress();
  p.toggleLight();
  CHECK_EQ(p.fullness, (uint8_t)50);
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.poops, (uint8_t)2);
  CHECK(!p.sleeping);
}

TEST(ceremony, soltar_marca_el_final_como_liberacion) {
  Pet p;
  makePet(p, 4);
  p.release();
  CHECK_EQ(p.ceremony, (uint8_t)CER_RELEASE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_RELEASE);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
}

TEST(ceremony, un_huevo_no_se_despide_ni_se_suelta) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.startFarewell();
  p.release();
  p.startRunaway();
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
}

// ---------------------------------------------------------------- offline
TEST(offline, aplica_el_tiempo_apagado_con_suelo_de_15) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600 * 60);  // 600 minutos apagado
  CHECK_EQ(p.ageMinutes, (uint32_t)600);
  CHECK_MSG(p.fullness == 15, "suelo de comida offline = 15");
  CHECK_MSG(p.energy == 15, "suelo de energia offline = 15");
  CHECK_MSG(p.joy == 15, "suelo de felicidad offline = 15");
  CHECK_MSG(p.hygiene == 15, "suelo de higiene offline = 15");
  CHECK_MSG(p.careMistakes == 0, "en ausencia no se acumulan descuidos");
  CHECK_EQ(p.poops, (uint8_t)2);  // una caca cada 4 h
}

TEST(offline, tope_de_dos_semanas) {
  Pet p;
  makePet(p, 4);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90u * 86400);  // 90 dias
  CHECK_EQ(p.ageMinutes, (uint32_t)(14u * 24 * 60));
}

TEST(offline, menos_de_dos_minutos_no_hace_nada) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}

TEST(offline, el_huevo_eclosiona_en_tu_ausencia) {
  Pet p;
  makePet(p, 4);
  p.newEgg();  // ya hay pokedex: no pide inicial
  CHECK(!p.awaitingStarter());
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600);  // 10 minutos
  CHECK(!p.isEgg());
  CHECK_RANGE(p.speciesId, (int16_t)1, (int16_t)DEX_COUNT);
}

TEST(offline, durmiendo_se_descansa_tambien_apagado) {
  Pet p;
  makePet(p, 4);
  setStats(p, 90, 90, 20, 90);
  p.toggleLight();
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 120 * 60);
  CHECK_EQ(p.energy, (uint8_t)100);
  CHECK_MSG(p.fullness >= 30, "suelos de sueno tambien offline");
  CHECK_MSG(p.poops == 0, "durmiendo no ensucia");
}

TEST(offline, sin_reloj_no_se_aplica_nada) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.syncClock(0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}

// ---------------------------------------------------------------- guardado
TEST(save, el_estado_sobrevive_a_un_reinicio) {
  Pet a;
  makePet(a, 25);  // PIKACHU
  a.ageMinutes = 1234;
  a.streak = 9;
  a.bestStreak = 11;
  a.bond = 44;
  a.trAtk = 17;
  a.weight = 33;
  a.gameHi = 21;
  a.rename("SPARKY");  // rename persiste todo el estado

  Pet b;
  b.begin();  // mismo "NVS": simula un reinicio
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.ageMinutes, (uint32_t)1234);
  CHECK_EQ(b.streak, (uint16_t)9);
  CHECK_EQ(b.bestStreak, (uint16_t)11);
  CHECK_EQ(b.bond, (uint8_t)44);
  CHECK_EQ(b.trAtk, (uint8_t)17);
  CHECK_EQ(b.weight, (uint8_t)33);
  CHECK_EQ(b.gameHi, (uint16_t)21);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK(b.isRegistered(25));
  CHECK(!b.awaitingStarter());
}

// ko8: 18 bytes (6 silabas hangul), sin partir un caracter
TEST(save, el_apodo_se_recorta_a_18_bytes) {
  Pet p;
  makePet(p, 4);
  p.rename("NOMBRE-LARGUISIMO-QUE-NO-CABE");
  CHECK_EQ((int)strlen(p.nick), 18);
  CHECK_STREQ(p.nick, "NOMBRE-LARGUISIMO-");
  p.rename("가나다라마바사");  // 7 silabas: la ultima no cabe
  CHECK_STREQ(p.nick, "가나다라마바");
  p.rename("AB가나다라마바");  // 20 bytes: se corta antes de "바", no en medio
  CHECK_STREQ(p.nick, "AB가나다라마");
  Pet q;
  q.begin();  // y se guarda
  CHECK_STREQ(q.nick, "AB가나다라마");
}

TEST(save, borrado_de_fabrica_deja_todo_como_nuevo) {
  Pet a;
  makePet(a, 4);
  a.factoryReset();
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK(b.awaitingStarter());
  CHECK_EQ(b.registeredCount(), (uint16_t)0);
}

TEST(save, el_autoguardado_se_marca_y_se_vuelca) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.savePending());
  advance(p, 5);
  CHECK_MSG(p.savePending(), "cada 5 ticks queda un guardado pendiente");
  p.flushSave();
  CHECK(!p.savePending());
}

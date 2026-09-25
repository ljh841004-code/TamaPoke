// fork KO (ko4): caja (bogwanham), registro de pokedex, objetos de batalla,
// captura y el siguiente Pokemon tras la despedida.
#include "framework.h"
#include "shim/Arduino.h"
#include "../box.h"
#include "../battle.h"
#include "../pet.h"
#include "../dex.h"
#include <string.h>

static void freshPet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap(); p.eggTap(); p.eggTap();
}

// ---------------------------------------------------------------- caja
TEST(box, guarda_y_recupera_tras_reiniciar) {
  mockNvsReset();
  Box b;
  b.begin();
  CHECK_EQ(b.count(), (uint8_t)0);
  CHECK_EQ(b.pickRandom(), -1);
  CHECK(b.add(25, 12, true, true, 1000));
  CHECK(b.add(1, 3, false, false, 2000));
  Box c;  // "reinicio": lee la NVS
  c.begin();
  CHECK_EQ(c.count(), (uint8_t)2);
  CHECK_EQ(c.at(0).dex, (int16_t)25);
  CHECK_EQ(c.at(0).lvl, (uint16_t)12);
  CHECK(c.at(0).flags & BOXF_SHINY);
  CHECK(c.at(0).flags & BOXF_CAUGHT);
  CHECK_EQ(c.at(1).dex, (int16_t)1);
  CHECK(!(c.at(1).flags & BOXF_CAUGHT));
  CHECK_RANGE(c.at(0).geneAtk, 90, 110);
}

TEST(box, rechaza_dex_invalido_y_se_llena) {
  mockNvsReset();
  Box b;
  b.begin();
  CHECK(!b.add(0, 5, false, false, 0));
  CHECK(!b.add(152, 5, false, false, 0));
  for (int i = 0; i < BOX_MAX; i++) CHECK(b.add(1 + i % 151, 5, false, false, 0));
  CHECK(b.full());
  CHECK(!b.add(4, 5, false, false, 0));  // llena: no pisa a nadie
  CHECK_EQ(b.count(), (uint8_t)BOX_MAX);
}

TEST(box, sacar_y_soltar_mantienen_el_orden) {
  mockNvsReset();
  Box b;
  b.begin();
  b.add(10, 1, false, false, 0);
  b.add(20, 2, false, false, 0);
  b.add(30, 3, false, false, 0);
  BoxMon m;
  CHECK(b.take(1, m));
  CHECK_EQ(m.dex, (int16_t)20);
  CHECK_EQ(b.count(), (uint8_t)2);
  CHECK_EQ(b.at(1).dex, (int16_t)30);
  CHECK(!b.take(5, m));
  CHECK(b.release(0));
  CHECK_EQ(b.at(0).dex, (int16_t)30);
  Box c;
  c.begin();
  CHECK_EQ(c.count(), (uint8_t)1);
  CHECK_EQ(c.at(0).dex, (int16_t)30);
}

TEST(box, nvs_danada_no_mete_especies_invalidas) {
  mockNvsReset();
  Preferences p;
  p.begin("tpbox", false);
  BoxMon bad[2];
  memset(bad, 0, sizeof(bad));
  bad[0].dex = 999;
  bad[1].dex = 7;
  p.putBytes("mons", bad, sizeof(bad));
  p.putUChar("n", 2);
  Box b;
  b.begin();
  CHECK_EQ(b.count(), (uint8_t)1);
  CHECK_EQ(b.at(0).dex, (int16_t)7);
}

// ---------------------------------------------------------------- pokedex
TEST(dexlog, primera_vez_y_contadores) {
  mockNvsReset();
  DexLog d;
  d.begin();
  CHECK(!d.wasSeen(25));
  d.seen(25, 5000);
  d.seen(25, 9000);
  CHECK(d.wasSeen(25));
  CHECK_EQ(d.firstSeen(25), (uint32_t)5000);  // la primera, no la ultima
  CHECK_EQ(d.seenCount(25), (uint16_t)2);
  d.caught(4, 7000);
  CHECK(d.wasSeen(4));
  CHECK_EQ(d.caughtCount(4), (uint16_t)1);
  CHECK_EQ(d.firstSeen(4), (uint32_t)7000);
  d.seen(0, 1);  // fuera de rango: se ignora
  d.seen(152, 1);
  DexLog e;
  e.begin();
  CHECK_EQ(e.seenCount(25), (uint16_t)2);
  CHECK_EQ(e.caughtCount(4), (uint16_t)1);
  CHECK_EQ(e.seenCount(0), (uint16_t)0);
}

// ---------------------------------------------------------------- batalla
TEST(items, pocion_cura_la_mitad_sin_pasarse) {
  Battler a = makeBattler(4, 20, 60, 50, 70), b = makeBattler(1, 20, 50, 50, 40);
  a.hp = a.maxHp / 4;
  uint16_t antes = a.hp;
  BRng rng(7);
  BEvent ev[BATTLE_MAX_EVENTS];
  int n = battleTurn(a, b, BA_POTION, BA_GUARD, rng, ev, BATTLE_MAX_EVENTS, true);
  CHECK(n >= 1);
  CHECK_EQ(ev[0].kind, (uint8_t)EV_HEAL);
  CHECK_EQ(ev[0].dmg, (uint16_t)(a.maxHp / 2));
  CHECK_EQ(a.hp, (uint16_t)(antes + a.maxHp / 2));
  // casi llena: cura solo lo que falta
  a.hp = a.maxHp - 1;
  n = battleTurn(a, b, BA_POTION, BA_GUARD, rng, ev, BATTLE_MAX_EVENTS, true);
  CHECK_EQ(ev[0].dmg, (uint16_t)1);
  CHECK_EQ(a.hp, a.maxHp);
}

TEST(items, en_tongsin_pocion_y_bola_son_placaje) {
  Battler a = makeBattler(4, 20, 60, 50, 70), b = makeBattler(1, 20, 50, 50, 40);
  BRng rng(3);
  BEvent ev[BATTLE_MAX_EVENTS];
  int n = battleTurn(a, b, BA_BALL, BA_POTION, rng, ev, BATTLE_MAX_EVENTS, false);
  for (int i = 0; i < n; i++) {
    CHECK(ev[i].kind != EV_HEAL);
    CHECK(ev[i].kind != EV_CATCH);
    CHECK(ev[i].kind != EV_BREAK);
  }
}

TEST(items, captura_mas_facil_con_poca_vida_y_dificil_si_es_raro) {
  Battler full = makeBattler(16, 10, 30, 30, 30);  // Pidgey: comun
  Battler weak = full;
  weak.hp = 1;
  CHECK(catchChance(weak) > catchChance(full));
  CHECK_RANGE(catchChance(full), 15, 25);
  CHECK(catchChance(weak) >= 80);
  Battler leg = makeBattler(150, 60, 150, 100, 130);  // Mewtwo
  leg.hp = 1;
  CHECK(catchChance(leg) < catchChance(weak) / 2);
  CHECK(catchChance(leg) >= 3);
}

TEST(items, bola_atrapa_o_falla_y_el_rival_ataca) {
  int atrapados = 0, fallos = 0;
  for (uint32_t seed = 1; seed <= 400; seed++) {
    Battler a = makeBattler(4, 20, 60, 50, 70), b = makeBattler(16, 18, 40, 40, 50);
    b.hp = b.maxHp / 2;
    BRng rng(seed);
    BEvent ev[BATTLE_MAX_EVENTS];
    int n = battleTurn(a, b, BA_BALL, BA_TACKLE, rng, ev, BATTLE_MAX_EVENTS, true);
    CHECK(n >= 1);
    if (ev[0].kind == EV_CATCH) {
      atrapados++;
      CHECK_EQ(n, 1);  // atrapado: el turno acaba ahi, el rival no ataca
    } else {
      CHECK_EQ(ev[0].kind, (uint8_t)EV_BREAK);
      fallos++;
      CHECK(n >= 2);  // se escapa y el rival actua
    }
  }
  CHECK(atrapados > 100 && fallos > 100);  // ~52% con media vida
}

// ---------------------------------------------------------------- premios
TEST(items, victoria_salvaje_da_2_bolas_y_2_pociones) {
  Pet p;
  freshPet(p, 4);
  uint8_t b0 = p.balls, p0 = p.potions;
  p.battleResult(BATTLE_WILD, true, false);
  CHECK_EQ(p.balls, (uint8_t)(b0 + 2));
  CHECK_EQ(p.potions, (uint8_t)(p0 + 2));
  CHECK_EQ(p.wildWins, (uint16_t)1);
  p.balls = 98; p.potions = 99;
  p.battleResult(BATTLE_WILD, true, false);
  CHECK_EQ(p.balls, (uint8_t)99);  // tope 99
  CHECK_EQ(p.potions, (uint8_t)99);
}

TEST(items, perder_o_huir_no_castiga) {
  Pet p;
  freshPet(p, 4);
  p.energy = 60; p.fullness = 60; p.joy = 60;
  uint8_t b0 = p.balls;
  p.battleResult(BATTLE_WILD, false, false);
  CHECK_EQ(p.energy, (uint8_t)60);
  CHECK_EQ(p.fullness, (uint8_t)60);
  CHECK_EQ(p.joy, (uint8_t)60);
  p.battleResult(BATTLE_WILD, false, true);
  CHECK_EQ(p.energy, (uint8_t)60);
  CHECK_EQ(p.balls, b0);
}

TEST(items, capturar_premia_sin_objetos) {
  Pet p;
  freshPet(p, 4);
  uint8_t b0 = p.balls;
  uint8_t atk0 = p.trAtk;
  p.battleResult(BATTLE_WILD, false, false, true);
  CHECK_EQ(p.balls, b0);
  CHECK(p.trAtk > atk0);
  CHECK_EQ(p.wildWins, (uint16_t)0);
}

TEST(items, gastar_bolas_y_pociones_persiste) {
  Pet p;
  freshPet(p, 4);
  CHECK_EQ(p.balls, (uint8_t)5);  // kit inicial
  CHECK_EQ(p.potions, (uint8_t)2);
  CHECK(p.usePotion());
  CHECK(p.usePotion());
  CHECK(!p.usePotion());
  CHECK(p.useBall());
  Pet q;  // reinicio
  q.begin();
  CHECK_EQ(q.balls, (uint8_t)4);
  CHECK_EQ(q.potions, (uint8_t)0);
}

// ---------------------------------------------------------------- entrenamiento
TEST(train, defensa_y_velocidad_suben_con_tope_y_record) {
  Pet p;
  freshPet(p, 4);
  uint8_t g = p.trainDefense(10);
  CHECK_EQ(g, (uint8_t)5);
  CHECK_EQ(p.trDef, (uint8_t)5);
  CHECK_EQ(p.defHi, (uint16_t)10);
  g = p.trainDefense(100);
  CHECK_EQ(g, (uint8_t)18);  // tope por sesion
  g = p.trainSpeed(12);
  CHECK_EQ(g, (uint8_t)12);
  CHECK_EQ(p.speHi, (uint16_t)12);
  p.trainSpeed(3);
  CHECK_EQ(p.speHi, (uint16_t)12);  // un resultado peor no baja el record
  Pet q;
  q.begin();
  CHECK_EQ(q.defHi, (uint16_t)100);
  CHECK_EQ(q.speHi, (uint16_t)12);
}

// ---------------------------------------------------------------- siguiente
static Box *gHookBox = nullptr;
static bool fromBox(Pet &pet) {
  int i = gHookBox->pickRandom();
  BoxMon m;
  if (i < 0 || !gHookBox->take((uint8_t)i, m)) return false;
  pet.adoptMon(m.dex, m.lvl, m.flags & BOXF_SHINY, m.geneAtk, m.geneDef, m.geneSpe);
  return true;
}

TEST(next, tras_la_despedida_sale_uno_de_la_caja) {
  Pet p;
  freshPet(p, 4);
  Box b;
  b.begin();
  b.add(25, 21, false, true, 0);
  gHookBox = &b;
  p.nextPetHook = fromBox;
  p.startFarewell();
  mockAdvanceMillis(CEREMONY_MS + 10);
  p.update(millis());
  CHECK_EQ(p.speciesId, (int16_t)25);
  CHECK_EQ(p.level(), (uint16_t)21);
  CHECK(p.ceremony == CER_NONE);
  CHECK(p.isRegistered(25));
  CHECK_EQ(b.count(), (uint8_t)0);  // sale de la caja
}

TEST(next, caja_vacia_o_escapada_dan_huevo) {
  Pet p;
  freshPet(p, 4);
  Box b;
  b.begin();
  gHookBox = &b;
  p.nextPetHook = fromBox;
  p.startFarewell();
  mockAdvanceMillis(CEREMONY_MS + 10);
  p.update(millis());
  CHECK(p.isEgg());  // caja vacia
  // escapada: huevo aunque haya Pokemon en la caja
  p.eggTap(); p.eggTap(); p.eggTap();
  b.add(25, 21, false, true, 0);
  p.startRunaway();
  mockAdvanceMillis(CEREMONY_MS + 10);
  p.update(millis());
  CHECK(p.isEgg());
  CHECK_EQ(b.count(), (uint8_t)1);
}

// ---------------------------------------------------------------- guardado
TEST(save, cada_minuto_queda_guardado_y_sobrevive_a_un_corte) {
  Pet p;
  freshPet(p, 4);
  p.fullness = 90;
  p.saveNow();
  uint32_t age0 = p.ageMinutes;
  mockAdvanceMillis(PET_TICK_MS);  // un minuto de juego
  p.update(millis());
  CHECK_MSG(p.savePending(), "ko4: basta un tick para tener guardado pendiente");
  p.flushSave();
  CHECK(!p.savePending());
  // "corte de luz": otra instancia lee la NVS sin que nadie llame a save()
  Pet q;
  q.begin();
  CHECK_EQ(q.ageMinutes, age0 + 1);
  CHECK_EQ(q.fullness, p.fullness);
  CHECK(q.fullness < 90);  // bajo con el tick y eso tambien se guardo
}

// ---------------------------------------------------------------- ko5
#include "../sdupdate.h"
#include <vector>

static std::vector<uint8_t> readFile(const char *path) {
  std::vector<uint8_t> v;
  FILE *f = fopen(path, "rb");
  if (!f) return v;
  uint8_t b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof(b), f)) > 0) v.insert(v.end(), b, b + n);
  fclose(f);
  return v;
}

TEST(sdupdate, clasifica_cabeceras) {
  std::vector<uint8_t> app(0x9000, 0);
  app[0] = 0xE9; app[12] = 9;  // app de ESP32-S3
  CHECK_EQ(updClassify(app.data(), app.size(), 1800000), UPD_OK);
  std::vector<uint8_t> full = app;  // imagen fusionada de 0x0
  full[0x8000] = 0xAA; full[0x8001] = 0x50;
  CHECK_EQ(updClassify(full.data(), full.size(), 1900000), UPD_FULLIMG);
  std::vector<uint8_t> bad = app;
  bad[0] = 0x00;
  CHECK_EQ(updClassify(bad.data(), bad.size(), 1800000), UPD_BAD);
  std::vector<uint8_t> c3 = app;  // otro chip (ESP32-C3 = 5)
  c3[12] = 5;
  CHECK_EQ(updClassify(c3.data(), c3.size(), 1800000), UPD_BAD);
  CHECK_EQ(updClassify(app.data(), app.size(), 100), UPD_BAD);                // demasiado pequeno
  CHECK_EQ(updClassify(app.data(), app.size(), 4 * 1024 * 1024), UPD_BAD);    // no cabe en 3 MB
  CHECK_EQ(updClassify(app.data(), 8, 1800000), UPD_BAD);                     // cabecera cortada
}

TEST(sdupdate, los_bin_publicados_se_clasifican_bien) {
  // los ficheros reales de la carpeta claude-version (si estan)
  std::vector<uint8_t> app = readFile("../../update.bin");
  std::vector<uint8_t> full = readFile("../../tamapoke-ko-v1.17-ko6.2.bin");
  if (app.empty() || full.empty()) return;
  size_t ha = app.size() < UPD_HEAD_LEN ? app.size() : UPD_HEAD_LEN;
  size_t hf = full.size() < UPD_HEAD_LEN ? full.size() : UPD_HEAD_LEN;
  CHECK_EQ(updClassify(app.data(), ha, (uint32_t)app.size()), UPD_OK);
  CHECK_EQ(updClassify(full.data(), hf, (uint32_t)full.size()), UPD_FULLIMG);
}

TEST(sdupdate, busca_la_marca_de_version) {
  const char blob[] = "xxTPVER:\0yyyyTPVER:1.17-ko6.2\0zz";
  const uint8_t *b = (const uint8_t *)blob;
  int at = updFindTag(b, sizeof(blob));
  CHECK_EQ(at, 2);  // la primera es la cadena de busqueda, sin version detras
  int next = updFindTag(b + at + 1, sizeof(blob) - at - 1);
  CHECK(next >= 0);
  CHECK(!memcmp(b + at + 1 + next + 6, "1.17-ko6.2", 10));
  CHECK_EQ(updFindTag((const uint8_t *)"TPVE", 4), -1);  // cortada: no
}

// fork KO (ko7): la caja topa en 100 y los Lv300+ de ko6 vuelven a Lv5
TEST(box, niveles_viejos_vuelven_a_5) {
  mockNvsReset();
  Box b;
  b.begin();
  b.add(25, 250, false, true, 0);
  CHECK_EQ(b.at(0).lvl, (uint16_t)100);
  BoxMon old = b.at(0);
  old.lvl = 338;  // como lo guardaba ko6
  Preferences raw;
  raw.begin("tpbox", false);
  raw.putBytes("mons", &old, sizeof(old));
  Box c;
  c.begin();
  CHECK_EQ(c.count(), (uint8_t)1);
  CHECK_EQ(c.at(0).lvl, (uint16_t)5);
  Box d;  // y queda guardado
  d.begin();
  CHECK_EQ(d.at(0).lvl, (uint16_t)5);
}

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

// ---------------------------------------------------------------- ko8: WiFi
#include "../net_pick.h"

TEST(net, guardadas_visibles_primero_por_senal_luego_abiertas) {
  char saved[NET_MAX_SAVED][33] = { "HOME", "OFFICE", "PHONE" };
  NetSeen seen[] = {
    { "CAFE_FREE", -40, true }, { "OFFICE", -70, false }, { "PHONE", -50, false },
    { "SUBWAY", -60, true }, { "CAFE_FREE", -45, true }, { "LOCKED", -30, false },
    { "", -20, true },
  };
  NetCand c[NET_MAX_CAND];
  int n = netPickCandidates(saved, 3, seen, 7, true, c, NET_MAX_CAND);
  CHECK_EQ(n, 5);
  CHECK_EQ(c[0].saved, (int8_t)2);   // PHONE -50
  CHECK_EQ(c[1].saved, (int8_t)1);   // OFFICE -70
  CHECK_EQ(c[2].saved, (int8_t)0);   // HOME no se ve: se intenta igual
  CHECK_EQ(c[2].seen, (int8_t)-1);
  CHECK_EQ(c[3].saved, (int8_t)-1);  // abierta mas fuerte, sin repetir
  CHECK_EQ(std::string(seen[c[3].seen].ssid), std::string("CAFE_FREE"));
  CHECK_EQ(std::string(seen[c[4].seen].ssid), std::string("SUBWAY"));
  // sin permiso para abiertas
  CHECK_EQ(netPickCandidates(saved, 3, seen, 7, false, c, NET_MAX_CAND), 3);
  // nada guardado: solo abiertas (maximo 3)
  NetSeen many[] = { { "A", -40, true }, { "B", -50, true }, { "C", -60, true }, { "D", -30, true } };
  n = netPickCandidates(saved, 0, many, 4, true, c, NET_MAX_CAND);
  CHECK_EQ(n, NET_MAX_OPEN);
  CHECK_EQ(std::string(many[c[0].seen].ssid), std::string("D"));
  CHECK_EQ(netPickCandidates(saved, 0, many, 0, true, c, NET_MAX_CAND), 0);
}

TEST(net, recordar_sube_arriba_y_topa_en_5) {
  char s[NET_MAX_SAVED][33] = {};
  char p[NET_MAX_SAVED][65] = {};
  uint8_t n = 0;
  netRememberFront(s, p, n, "A", "1");
  netRememberFront(s, p, n, "B", "2");
  netRememberFront(s, p, n, "A", "9");  // ya estaba: sube y cambia la clave
  CHECK_EQ(n, (uint8_t)2);
  CHECK_EQ(std::string(s[0]), std::string("A"));
  CHECK_EQ(std::string(p[0]), std::string("9"));
  CHECK_EQ(std::string(s[1]), std::string("B"));
  for (const char *x : { "C", "D", "E", "F" }) netRememberFront(s, p, n, x, "");
  CHECK_EQ(n, (uint8_t)5);
  CHECK_EQ(std::string(s[0]), std::string("F"));
  CHECK_EQ(std::string(s[4]), std::string("A"));  // B era la menos reciente: se perdio
  netForget(s, p, n, 0);
  CHECK_EQ(n, (uint8_t)4);
  CHECK_EQ(std::string(s[0]), std::string("E"));
  netForget(s, p, n, 9);  // fuera de rango: nada
  CHECK_EQ(n, (uint8_t)4);
}

// ---------------------------------------------------------------- ko8: reset
TEST(reset, borra_partida_y_conserva_ajustes) {
  mockNvsReset();
  Preferences raw;
  raw.begin("tamapoke", false);
  raw.putUChar("lang", 7);
  raw.putBool("snd", false);
  raw.putUChar("volBgm", 30);
  Pet p;
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap(); p.eggTap(); p.eggTap();
  p.wildWins = 9;
  p.saveNow();
  Box b; b.begin(); b.add(25, 10, false, true, 0);
  DexLog d; d.begin(); d.seen(16, 100);
  p.wipeGameKeepSettings();
  b.wipe();
  d.wipe();
  CHECK_EQ(raw.getUChar("lang", 0), (uint8_t)7);
  CHECK_EQ(raw.getBool("snd", true), false);
  CHECK_EQ(raw.getUChar("volBgm", 0), (uint8_t)30);
  CHECK(!raw.isKey("volCry"));   // lo que no existia no aparece
  CHECK(!raw.isKey("dexn"));
  CHECK(!raw.isKey("init"));
  Pet q;
  q.begin();
  CHECK(q.awaitingStarter());    // vuelve a elegir inicial
  CHECK_EQ(q.registeredCount(), (uint16_t)0);
  CHECK_EQ(q.wildWins, (uint16_t)0);
  Box b2; b2.begin();
  CHECK_EQ(b2.count(), (uint8_t)0);
  DexLog d2; d2.begin();
  CHECK(!d2.wasSeen(16));
}

// ---------------------------------------------------------------- ko8: cheonjiin
#include "../cji.h"

static std::string cjiType(const std::vector<uint8_t> &keys, bool final = false) {
  Cji c;
  for (uint8_t k : keys) cjiPress(c, k);
  char buf[64];
  cjiCompose(c, buf, sizeof(buf), final);
  return buf;
}

TEST(cji, silabas_basicas) {
  using namespace std;
  enum { I = CJI_K_I, D = CJI_K_DOT, E = CJI_K_EU, G = CJI_K_G, N = CJI_K_N, T = CJI_K_D,
         B = CJI_K_B, S = CJI_K_S, J = CJI_K_J, SP = CJI_K_SPACE, O = CJI_K_O, DEL = CJI_K_DEL };
  CHECK_EQ(cjiType({ G, I, D }), string("가"));
  CHECK_EQ(cjiType({ G, I, D, D }), string("갸"));
  CHECK_EQ(cjiType({ G }), string("ㄱ"));
  CHECK_EQ(cjiType({ G, G }), string("ㅋ"));
  CHECK_EQ(cjiType({ G, G, G }), string("ㄲ"));
  CHECK_EQ(cjiType({ G, G, G, G }), string("ㄱ"));
  CHECK_EQ(cjiType({ O, O }), string("ㅁ"));
  CHECK_EQ(cjiType({ O, O, O }), string("ㅇ"));
  // 한글
  CHECK_EQ(cjiType({ S, S, I, D, N, G, E, N, N }), string("한글"));
  // 파이리 / 꼬부기
  CHECK_EQ(cjiType({ B, B, I, D, O, I, N, N, I }), string("파이리"));
  CHECK_EQ(cjiType({ G, G, G, D, E, B, E, D, G, I }), string("꼬부기"));
  // 각각: 띄움 separa la misma tecla
  CHECK_EQ(cjiType({ G, I, D, G, SP, G, I, D, G }), string("각각"));
  // sin 띄움, la segunda pulsacion cambia la consonante: 갘
  CHECK_EQ(cjiType({ G, I, D, G, G }), string("갘"));
  // final doble y paso de la final a la silaba siguiente
  CHECK_EQ(cjiType({ T, I, D, N, N, G }), string("닭"));
  CHECK_EQ(cjiType({ T, I, D, N, N, G, I, D }), string("달가"));
  // vocales compuestas
  CHECK_EQ(cjiType({ O, D, E, I, D }), string("와"));
  CHECK_EQ(cjiType({ O, E, D, D, I }), string("워"));
  CHECK_EQ(cjiType({ O, E, I }), string("의"));
  CHECK_EQ(cjiType({ O, I, D, I }), string("애"));
  CHECK_EQ(cjiType({ O, D, I, I }), string("에"));
  // ㄸ no puede ser final: empieza silaba nueva
  CHECK_EQ(cjiType({ G, I, D, T, T, T }), string("가ㄸ"));
  // espacio
  CHECK_EQ(cjiType({ G, I, D, SP, SP, N, I, D }), string("가 나"));
}

TEST(cji, trazos_a_medias_y_borrar) {
  using namespace std;
  enum { I = CJI_K_I, D = CJI_K_DOT, E = CJI_K_EU, G = CJI_K_G, DEL = CJI_K_DEL };
  CHECK_EQ(cjiType({ G, D }), string("ㄱㆍ"));
  CHECK_EQ(cjiType({ G, D }, true), string("ㄱ"));     // al guardar no queda el punto
  CHECK_EQ(cjiType({ G, D, D }), string("ㄱㆍㆍ"));
  CHECK_EQ(cjiType({ G, I, D, D, DEL }), string("가"));  // borra un trazo
  CHECK_EQ(cjiType({ G, I, D, DEL }), string("기"));
  CHECK_EQ(cjiType({ G, I, DEL }), string("ㄱ"));
  CHECK_EQ(cjiType({ G, I, DEL, DEL }), string(""));
  CHECK_EQ(cjiType({ DEL, DEL }), string(""));
}

TEST(cji, tope_de_6_silabas) {
  enum { I = CJI_K_I, D = CJI_K_DOT, G = CJI_K_G };
  Cji c;
  for (int k = 0; k < 6; k++) {
    CHECK(cjiPress(c, G));
    CHECK(cjiPress(c, I));
    CHECK(cjiPress(c, D));
  }
  char buf[64];
  CHECK_EQ(cjiCompose(c, buf, sizeof(buf), true), 18);
  CHECK(cjiPress(c, G));   // cabe como final de la sexta: ...각
  CHECK(!cjiPress(c, I));  // pero una septima silaba (..가기) no
  CHECK_EQ(cjiCompose(c, buf, sizeof(buf), true), 18);
  CHECK_EQ(std::string(buf), std::string("가가가가가각"));
}

TEST(trade, apodo_en_hangul_viaja_y_se_filtra) {
  Pet a;
  mockNvsReset();
  a.begin();
  if (a.awaitingStarter()) a.chooseStarter(4);
  a.eggTap(); a.eggTap(); a.eggTap();
  TradePet t;
  a.exportTrade(t);
  t.dex = 25;
  strcpy(t.nick, "피카 츄");
  CHECK(a.importTrade(t, 10));
  CHECK_EQ(std::string(a.nick), std::string("피카 츄"));
  // UTF-8 roto o fuera del hangul: se descarta
  Pet b;
  mockNvsReset();
  b.begin();
  if (b.awaitingStarter()) b.chooseStarter(4);
  b.eggTap(); b.eggTap(); b.eggTap();
  memset(t.nick, 0, sizeof(t.nick));
  const char bad[] = "\xE3\x81\x82" "AB" "\xEA\xB0" "\xEA\xB0\x80";  // あ AB (roto) 가
  memcpy(t.nick, bad, sizeof(bad) - 1);
  CHECK(b.importTrade(t, 10));
  CHECK_EQ(std::string(b.nick), std::string("AB가"));
}

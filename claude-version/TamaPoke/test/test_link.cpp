// Tests del tongsin (link_core.cpp): dos placas simuladas con una radio falsa
// que puede perder paquetes.
#include "framework.h"
#include "shim/Arduino.h"
#include "../link_core.h"
#include "../battle.h"
#include <vector>

struct Air;
struct Node {
  LinkCore core;
  uint8_t mac[6];
  Air *air;
};
struct Air {
  std::vector<Node *> nodes;
  uint32_t lossPct = 0;  // % de paquetes perdidos por receptor
  uint32_t rng = 1;
  uint32_t now = 1000;
  bool chance() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) % 100 >= lossPct; }
};
static void airSend(void *ctx, const LinkMsg &m) {
  Node *from = (Node *)ctx;
  for (Node *n : from->air->nodes)
    if (n != from && from->air->chance()) n->core.receive(from->mac, m, from->air->now);
}
static LinkPet petOf(int16_t dex, uint16_t lvl) {
  LinkPet p;
  memset(&p, 0, sizeof(p));
  p.t.dex = dex; p.t.ageMinutes = lvl * 60u; p.t.geneAtk = p.t.geneDef = p.t.geneSpe = 100;
  p.lvl = lvl; p.atk = 60 + lvl; p.def = 50 + lvl; p.spe = 55 + lvl;
  return p;
}
static void startNode(Node &n, Air &a, uint8_t last, LinkMode m, const LinkPet &p, uint32_t nonce) {
  uint8_t mac[6] = { 0x24, 0x6F, 0x28, 0, 0, last };
  memcpy(n.mac, mac, 6);
  n.air = &a;
  n.core.start(m, p, n.mac, nonce, a.now, airSend, &n);
}
static void run(Air &a, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 20) {
    a.now += 20;
    for (Node *n : a.nodes) n->core.poll(a.now);
  }
}

TEST(link, se_emparejan_y_acuerdan_la_batalla) {
  for (uint32_t loss : { 0u, 30u, 60u }) {
    Air a; a.lossPct = loss; a.rng = 7 + loss;
    Node x, y;
    a.nodes = { &x, &y };
    startNode(x, a, 1, LINK_BATTLE, petOf(7, 20), 111);
    startNode(y, a, 2, LINK_BATTLE, petOf(4, 22), 222);
    run(a, 6000);
    CHECK_EQ(x.core.state(), (LinkState)LS_READY);
    CHECK_EQ(y.core.state(), (LinkState)LS_READY);
    CHECK(x.core.iAmA() != y.core.iAmA());
    CHECK_EQ(x.core.seed(), y.core.seed());
    CHECK_EQ(x.core.partner().t.dex, (int16_t)4);
    CHECK_EQ(y.core.partner().t.dex, (int16_t)7);
    // cada placa simula por su cuenta: mismo ganador
    auto sim = [](Node &n) {
      Battler me = makeBattler(n.core.mine().t.dex, n.core.mine().lvl, n.core.mine().atk,
                               n.core.mine().def, n.core.mine().spe);
      const LinkPet &o = n.core.partner();
      Battler fo = makeBattler(o.t.dex, o.lvl, o.atk, o.def, o.spe);
      uint8_t w = n.core.iAmA() ? battleAuto(me, fo, n.core.seed(), nullptr, 0, nullptr)
                                : battleAuto(fo, me, n.core.seed(), nullptr, 0, nullptr);
      return (w == 0) == n.core.iAmA();  // gane yo
    };
    CHECK(sim(x) != sim(y));  // exactamente uno de los dos gana
  }
}

TEST(link, modos_distintos_no_se_emparejan) {
  Air a;
  Node x, y;
  a.nodes = { &x, &y };
  startNode(x, a, 1, LINK_BATTLE, petOf(7, 20), 1);
  startNode(y, a, 2, LINK_TRADE, petOf(4, 20), 2);
  run(a, 5000);
  CHECK_EQ(x.core.state(), (LinkState)LS_SEARCH);
  CHECK_EQ(y.core.state(), (LinkState)LS_SEARCH);
  run(a, LINK_SEARCH_MS);
  CHECK_EQ(x.core.state(), (LinkState)LS_LOST);  // se rinde
}

TEST(link, un_tercero_no_se_cuela) {
  Air a;
  Node x, y, z;
  a.nodes = { &x, &y, &z };
  startNode(x, a, 1, LINK_TRADE, petOf(7, 20), 1);
  startNode(y, a, 2, LINK_TRADE, petOf(4, 20), 2);
  startNode(z, a, 3, LINK_TRADE, petOf(1, 20), 3);
  run(a, 6000);
  int ready = 0;
  for (Node *n : a.nodes) if (n->core.state() == LS_READY) ready++;
  CHECK_EQ(ready, 2);  // una pareja; el tercero sigue buscando
}

TEST(link, intercambio_completo_en_los_dos_lados) {
  for (uint32_t loss : { 0u, 40u }) {
    Air a; a.lossPct = loss; a.rng = 99 + loss;
    Node x, y;
    a.nodes = { &x, &y };
    startNode(x, a, 1, LINK_TRADE, petOf(64, 30), 5);  // Kadabra
    startNode(y, a, 2, LINK_TRADE, petOf(25, 18), 6);  // Pikachu
    run(a, 5000);
    CHECK_EQ(x.core.state(), (LinkState)LS_READY);
    x.core.accept();
    run(a, 1500);
    TradePet t;
    CHECK(!x.core.takeTrade(t));  // el otro aun no ha aceptado
    CHECK(x.core.partnerAccepted() == false);
    y.core.accept();
    run(a, 4000);
    TradePet tx, ty;
    CHECK(x.core.takeTrade(tx));
    CHECK(y.core.takeTrade(ty));
    CHECK_EQ(tx.dex, (int16_t)25);
    CHECK_EQ(ty.dex, (int16_t)64);
    CHECK(!x.core.takeTrade(tx));  // solo una vez
  }
}

TEST(link, rechazar_cancela_en_los_dos) {
  Air a;
  Node x, y;
  a.nodes = { &x, &y };
  startNode(x, a, 1, LINK_TRADE, petOf(7, 20), 5);
  startNode(y, a, 2, LINK_TRADE, petOf(4, 20), 6);
  run(a, 3000);
  x.core.accept();
  run(a, 500);
  y.core.decline();
  run(a, 2000);
  TradePet t;
  CHECK_EQ(y.core.state(), (LinkState)LS_DECLINED);
  CHECK_EQ(x.core.state(), (LinkState)LS_DECLINED);
  CHECK(!x.core.takeTrade(t));
  CHECK(!y.core.takeTrade(t));
}

TEST(link, silencio_del_otro_se_detecta) {
  Air a;
  Node x, y;
  a.nodes = { &x, &y };
  startNode(x, a, 1, LINK_BATTLE, petOf(7, 20), 5);
  startNode(y, a, 2, LINK_BATTLE, petOf(4, 20), 6);
  run(a, 3000);
  CHECK_EQ(x.core.state(), (LinkState)LS_READY);
  y.core.stop();  // el otro apaga
  run(a, LINK_SILENCE_MS + 1000);
  CHECK_EQ(x.core.state(), (LinkState)LS_LOST);
}

TEST(link, paquetes_basura_se_ignoran) {
  Air a;
  Node x;
  a.nodes = { &x };
  startNode(x, a, 1, LINK_BATTLE, petOf(7, 20), 5);
  LinkMsg m;
  memset(&m, 0xAB, sizeof(m));
  uint8_t src[6] = { 9, 9, 9, 9, 9, 9 };
  x.core.receive(src, m, a.now);
  m.magic = LINK_MAGIC; m.ver = 99;
  x.core.receive(src, m, a.now);
  CHECK_EQ(x.core.state(), (LinkState)LS_SEARCH);
}

// ko6.1: en la placa el loop toma `now` ANTES de procesar el toque que llama a
// start() con millis() (unos ms despues). El primer poll llega con un now algo
// menor que t0: la resta sin signo daba ~49 dias y la busqueda se daba por
// perdida al instante ("conexion perdida" nada mas abrir tongsin).
TEST(link, poll_con_now_anterior_al_inicio_no_pierde_la_busqueda) {
  Air a;
  Node n;
  a.nodes = { &n };
  startNode(n, a, 1, LINK_BATTLE, petOf(4, 10), 7);
  n.core.poll(a.now - 5);  // el now del loop, tomado antes del toque
  CHECK_EQ(n.core.state(), LS_SEARCH);
  run(a, 1000);
  CHECK_EQ(n.core.state(), LS_SEARCH);  // sigue buscando (nadie mas en el aire)
}

// ---------------------------------------------------------------- ko10.4: hora por tongsin
TEST(link, la_hora_de_fiar_pasa_al_que_la_perdio) {
  // dos nucleos conectados sin perdidas
  struct Wire { LinkCore *to; uint8_t from[6]; uint32_t now; };
  static LinkCore a, b;
  static uint8_t macA[6] = { 1, 1, 1, 1, 1, 1 }, macB[6] = { 2, 2, 2, 2, 2, 2 };
  static uint32_t tNow = 0;
  auto toB = [](void *, const LinkMsg &m) { b.receive(macA, m, tNow); };
  auto toA = [](void *, const LinkMsg &m) { a.receive(macB, m, tNow); };
  LinkPet pa, pb;
  memset(&pa, 0, sizeof(pa)); memset(&pb, 0, sizeof(pb));
  pa.t.dex = 4; pb.t.dex = 7;
  a.setClock(1790343900u, true, 0);   // A: hora buena
  b.setClock(1767225600u, false, 0);  // B: sembrada tras perder la pila
  a.start(LINK_BATTLE, pa, macA, 11, 0, toB, nullptr);
  b.start(LINK_BATTLE, pb, macB, 22, 0, toA, nullptr);
  for (tNow = 0; tNow < 5000; tNow += 100) { a.poll(tNow); b.poll(tNow); }
  uint32_t e = 0;
  CHECK(b.partnerClock(tNow, &e));
  CHECK_RANGE((int)(e - 1790343900u), 4, 6);   // la de A, avanzada ~5 s
  CHECK(!a.partnerClock(tNow, &e));             // la de B no es de fiar: A no la toma
  a.stop(); b.stop();
}

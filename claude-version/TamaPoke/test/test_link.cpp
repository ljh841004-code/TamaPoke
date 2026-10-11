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
static bool (*airFilter)(const Node *from, const Node *to) = nullptr;  // ko12.9.12: quien oye a quien
static void airSend(void *ctx, const LinkMsg &m) {
  Node *from = (Node *)ctx;
  for (Node *n : from->air->nodes)
    if (n != from && (!airFilter || airFilter(from, n)) && from->air->chance()) n->core.receive(from->mac, m, from->air->now);
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

// ko12.9.12: 3 placas en circulo (cada una elige a otra distinta: A->B, B->C, C->A).
// Antes las tres se quedaban esperando y caian a los 8 s; ahora sueltan y se emparejan dos
static Node *gCyc[3];
static bool cycleOnly(const Node *from, const Node *to) {
  for (int i = 0; i < 3; i++)
    if (to == gCyc[i]) return from == gCyc[(i + 1) % 3];  // cada uno solo oye al siguiente
  return true;
}
TEST(link, tres_en_circulo_se_deshace) {
  Air a;
  Node x, y, z;
  a.nodes = { &x, &y, &z };
  gCyc[0] = &x; gCyc[1] = &y; gCyc[2] = &z;
  startNode(x, a, 1, LINK_BATTLE, petOf(7, 20), 1);
  startNode(y, a, 2, LINK_BATTLE, petOf(4, 20), 2);
  startNode(z, a, 3, LINK_BATTLE, petOf(1, 20), 3);
  airFilter = cycleOnly;
  run(a, 400);          // primer HELLO: x elige a y, y a z, z a x
  airFilter = nullptr;  // ya se oyen todos
  run(a, 6000);
  int ready = 0, lost = 0;
  for (Node *n : a.nodes) {
    if (n->core.state() == LS_READY) ready++;
    if (n->core.state() == LS_LOST) lost++;
  }
  CHECK_EQ(ready, 2);
  CHECK_EQ(lost, 0);    // el tercero sigue buscando (no "conexion perdida")
  run(a, 10000);
  ready = 0; lost = 0;
  for (Node *n : a.nodes) {
    if (n->core.state() == LS_READY) ready++;
    if (n->core.state() == LS_LOST) lost++;
  }
  CHECK_EQ(ready, 2);   // la pareja sigue bien
  CHECK_EQ(lost, 0);    // antes: el que eligio al que ya tenia pareja caia a los 8 s
}

// el que sobra (eligio a uno que ya esta con otro) no cae a los 8 s: sigue buscando
TEST(link, el_que_sobra_sigue_buscando) {
  Air a;
  Node x, y, z;
  a.nodes = { &x, &y, &z };
  startNode(x, a, 1, LINK_TRADE, petOf(7, 20), 1);
  startNode(y, a, 2, LINK_TRADE, petOf(4, 20), 2);
  startNode(z, a, 3, LINK_TRADE, petOf(1, 20), 3);
  run(a, 12000);
  int ready = 0, search = 0;
  for (Node *n : a.nodes) {
    if (n->core.state() == LS_READY) ready++;
    if (n->core.state() == LS_SEARCH) search++;
  }
  CHECK_EQ(ready, 2);
  CHECK_EQ(search, 1);
}

// ---------------------------------------------------------------- ko12.9.16: combate por turnos
static LinkPet teamOf(int16_t d0, int16_t d1, int16_t d2, uint16_t lvl) {
  LinkPet p = petOf(d0, lvl);
  const int16_t ds[3] = { d0, d1, d2 };
  for (int i = 0; i < 3; i++) {
    Battler b = makeBattler(ds[i], lvl, 60 + lvl, 50 + lvl, 55 + lvl);
    LinkMon &m = p.team[i];
    m.dex = ds[i]; m.lvl = lvl; m.atk = b.atk; m.def = b.def; m.spe = b.spe;
    memcpy(m.mv, b.mv, 4);
  }
  p.teamN = 3;
  return p;
}
static void buildSide(LinkSide &s, const LinkPet &lp) {
  s.n = 0; s.cur = 0;
  for (uint8_t i = 0; i < lp.teamN; i++) {
    const LinkMon &m = lp.team[i];
    Battler b = makeBattler(m.dex, m.lvl, m.atk, m.def, m.spe);
    memcpy(b.mv, m.mv, 4);
    movesFillPP(b);
    s.mon[s.n++] = b;
  }
}

TEST(link, el_turno_viaja_y_el_anterior_sigue_llegando) {
  for (uint32_t loss : { 0u, 40u }) {
    Air a; a.lossPct = loss; a.rng = 3 + loss;
    Node x, y;
    a.nodes = { &x, &y };
    startNode(x, a, 1, LINK_BATTLE, teamOf(7, 25, 1, 20), 11);
    startNode(y, a, 2, LINK_BATTLE, teamOf(4, 16, 19, 22), 22);
    run(a, 3000);
    CHECK_EQ(x.core.state(), (LinkState)LS_READY);
    CHECK_EQ(y.core.partner().teamN, (uint8_t)3);
    CHECK_EQ(y.core.partner().team[1].dex, (int16_t)25);
    uint8_t act = 0;
    CHECK(!x.core.partnerAct(0, &act));  // aun no ha elegido
    x.core.setTurn(0, BA_M0 + 1);
    run(a, 1500);
    CHECK(y.core.partnerAct(0, &act));
    CHECK_EQ(act, (uint8_t)(BA_M0 + 1));
    CHECK(!x.core.partnerAct(0, &act));
    y.core.setTurn(0, LA_SWAP + 2);
    run(a, 1500);
    CHECK(x.core.partnerAct(0, &act));
    CHECK_EQ(act, (uint8_t)(LA_SWAP + 2));
    // x ya resolvio y pasa al turno 1: y aun puede leer lo que x eligio en el 0
    x.core.setTurn(1, LA_WAIT);
    run(a, 1500);
    CHECK(y.core.partnerAct(0, &act));
    CHECK_EQ(act, (uint8_t)(BA_M0 + 1));
    CHECK(!y.core.partnerAct(1, &act));
    x.core.stop(); y.core.stop();
  }
}

// las dos placas resuelven los mismos turnos (cada una con su punto de vista) y acaban igual
TEST(link, tres_contra_tres_las_dos_placas_ven_lo_mismo) {
  LinkPet pa = teamOf(6, 9, 3, 40), pb = teamOf(65, 68, 94, 41);
  for (uint32_t seed : { 1u, 77u, 4242u }) {
    LinkSide A1, B1, A2, B2;
    buildSide(A1, pa); buildSide(B1, pb);
    buildSide(A2, pa); buildSide(B2, pb);
    BRng r1(seed), r2(seed), pick(seed * 3 + 1);
    BEvent ev1[BATTLE_MAX_EVENTS], ev2[BATTLE_MAX_EVENTS];
    int turns = 0;
    while (linkSideAlive(A1) && linkSideAlive(B1) && turns < 200) {
      // eleccion de cada lado (igual para las dos placas: se intercambia)
      uint8_t aAct, bAct;
      if (!A1.act().hp || !B1.act().hp) {  // tras un desmayo: quien sale
        aAct = A1.act().hp ? LA_NONE : (uint8_t)(LA_SWAP + pick.below(3));
        bAct = B1.act().hp ? LA_NONE : (uint8_t)(LA_SWAP + pick.below(3));
      } else {
        uint32_t k = pick.below(10);
        aAct = k == 0 ? (uint8_t)(LA_SWAP + pick.below(3)) : k == 1 ? (uint8_t)BA_GUARD : (uint8_t)(BA_M0 + pick.below(4));
        k = pick.below(10);
        bAct = k == 0 ? (uint8_t)(LA_SWAP + pick.below(3)) : k == 1 ? (uint8_t)BA_GUARD : (uint8_t)(BA_M0 + pick.below(4));
      }
      linkApplySwap(A1, aAct); linkApplySwap(B1, bAct);
      linkApplySwap(A2, aAct); linkApplySwap(B2, bAct);
      int n1 = linkAttacks(A1, B1, aAct, bAct, r1, ev1, BATTLE_MAX_EVENTS);
      int n2 = linkAttacks(A2, B2, aAct, bAct, r2, ev2, BATTLE_MAX_EVENTS);
      CHECK_EQ(n1, n2);
      for (int i = 0; i < n1 && i < n2; i++) {
        CHECK_EQ(ev1[i].kind, ev2[i].kind);
        CHECK_EQ(ev1[i].hpA, ev2[i].hpA);
        CHECK_EQ(ev1[i].hpB, ev2[i].hpB);
      }
      if (n1) {  // las barras del ultimo evento cuadran con el estado (lado 0 = A)
        CHECK_EQ(ev1[n1 - 1].hpA, A1.act().hp);
        CHECK_EQ(ev1[n1 - 1].hpB, B1.act().hp);
      }
      CHECK(!A1.act().hp || A1.act().hp <= A1.act().maxHp);
      turns++;
    }
    CHECK(turns < 200);  // se acaba
    CHECK(linkSideAlive(A1) != linkSideAlive(B1));
    CHECK_EQ(linkSideAlive(A1), linkSideAlive(A2));
    for (int i = 0; i < 3; i++) CHECK_EQ(A1.mon[i].hp, A2.mon[i].hp);
  }
}

TEST(link, cambiar_y_solo_ataca_el_otro) {
  LinkPet pa = teamOf(6, 9, 3, 30), pb = teamOf(65, 68, 94, 30);
  LinkSide A, B;
  buildSide(A, pa); buildSide(B, pb);
  BRng r(5);
  BEvent ev[BATTLE_MAX_EVENTS];
  // A cambia al 2; B ataca: le da al que entra
  CHECK(linkApplySwap(A, LA_SWAP + 2));
  CHECK_EQ(A.cur, (uint8_t)2);
  uint16_t hp0 = A.act().hp;
  int n = linkAttacks(A, B, LA_SWAP + 2, BA_TACKLE, r, ev, BATTLE_MAX_EVENTS);
  CHECK(n > 0);
  CHECK_EQ(ev[0].side, (uint8_t)1);
  CHECK(A.act().hp <= hp0);
  CHECK_EQ(ev[n - 1].hpA, A.act().hp);
  // solo ataca A (B cambia): los eventos quedan del lado 0
  linkApplySwap(B, LA_SWAP + 1);
  uint16_t hpb = B.act().hp;
  n = linkAttacks(A, B, BA_TACKLE, LA_SWAP + 1, r, ev, BATTLE_MAX_EVENTS);
  CHECK(n > 0);
  CHECK_EQ(ev[0].side, (uint8_t)0);
  CHECK(B.act().hp <= hpb);
  CHECK_EQ(ev[n - 1].hpB, B.act().hp);
  // protegerse mientras el otro cambia: no pasa nada
  CHECK_EQ(linkAttacks(A, B, BA_GUARD, LA_SWAP + 0, r, ev, BATTLE_MAX_EVENTS), 0);
  // cambiar a uno caido no vale; si el que lucha esta caido, sale el primero en pie
  A.mon[0].hp = 0;
  CHECK(!linkApplySwap(A, LA_SWAP + 0));
  A.act().hp = 0;
  CHECK(linkApplySwap(A, LA_NONE));
  CHECK(A.act().hp > 0);
}

TEST(link, pocion_en_tongsin) {
  LinkPet pa = teamOf(6, 9, 3, 30), pb = teamOf(65, 68, 94, 30);
  LinkSide A, B;
  buildSide(A, pa); buildSide(B, pb);
  BRng r(9);
  BEvent ev[BATTLE_MAX_EVENTS];
  A.act().hp = A.act().maxHp / 4;
  uint16_t h0 = A.act().hp;
  // A se cura y B ataca: la pocion va antes
  int n = linkAttacks(A, B, BA_POTION, BA_TACKLE, r, ev, BATTLE_MAX_EVENTS);
  CHECK(n > 0);
  CHECK_EQ(ev[0].kind, (uint8_t)EV_HEAL);
  CHECK_EQ(ev[0].side, (uint8_t)0);
  // A se cura mientras B cambia: solo la cura, del lado de A
  A.act().hp = h0;
  linkApplySwap(B, LA_SWAP + 1);
  n = linkAttacks(A, B, BA_POTION, LA_SWAP + 1, r, ev, BATTLE_MAX_EVENTS);
  CHECK_EQ(n, 1);
  CHECK_EQ(ev[0].kind, (uint8_t)EV_HEAL);
  CHECK_EQ(ev[0].side, (uint8_t)0);
  CHECK(A.act().hp > h0);
  CHECK_EQ(ev[0].hpA, A.act().hp);
  // huir o la ball no valen: placaje
  n = linkAttacks(A, B, BA_RUN, BA_BALL, r, ev, BATTLE_MAX_EVENTS);
  for (int i = 0; i < n; i++) CHECK(ev[i].kind != EV_RUN_OK && ev[i].kind != EV_CATCH && ev[i].kind != EV_BREAK);
}

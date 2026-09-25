// Maquina de estados del tongsin (ver link_core.h y link.h).
#include "link_core.h"

// ms transcurridos desde t, CON signo. ko6.1: el loop de la placa toma `now`
// antes de procesar el toque que llama a start() con millis(), asi que el
// primer poll puede llegar con now < t0; la resta sin signo daba ~49 dias y
// la busqueda se daba por perdida al instante.
static inline int32_t since(uint32_t now, uint32_t t) { return (int32_t)(now - t); }

static bool sameMac(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }
static bool zeroMac(const uint8_t *a) {
  for (int i = 0; i < 6; i++)
    if (a[i]) return false;
  return true;
}

void LinkCore::start(LinkMode mode, const LinkPet &mine, const uint8_t mac[6], uint32_t nonce,
                     uint32_t now, SendFn fn, void *ctx) {
  md = mode;
  me = mine;
  memcpy(myMac, mac, 6);
  memset(peer, 0, 6);
  memset(&theirs, 0, sizeof(theirs));
  havePeer = false;
  myAccept = theirAccept = commitPending = false;
  declineLeft = 0;
  myNonce = nonce;
  theirNonce = 0;
  sendFn = fn;
  sendCtx = ctx;
  st = LS_SEARCH;
  t0 = lastRx = now;
  lastTx = now - LINK_HELLO_MS;  // el primer HELLO sale en el primer poll
}

void LinkCore::stop() {
  st = LS_OFF;
  md = LINK_NONE;
  havePeer = false;
}

uint32_t LinkCore::seed() const {
  uint32_t a = iAmA() ? myNonce : theirNonce;
  uint32_t b = iAmA() ? theirNonce : myNonce;
  uint32_t s = a ^ (b * 2654435761UL);
  return s ? s : 1;
}

void LinkCore::send(uint8_t type, uint32_t now) {
  LinkMsg m;
  memset(&m, 0, sizeof(m));
  m.magic = LINK_MAGIC;
  m.ver = LINK_PROTO_VER;
  m.type = type;
  m.mode = md;
  if (havePeer) memcpy(m.peerMac, peer, 6);
  m.nonce = myNonce;
  m.pet = me;
  if (sendFn) sendFn(sendCtx, m);
  lastTx = now;
}

void LinkCore::accept() {
  if (st != LS_READY || md != LINK_TRADE) return;
  myAccept = true;
  st = LS_TRADE_WAIT;
  lastTx = 0;  // que salga ya en el proximo poll
}

void LinkCore::decline() {
  if (md != LINK_TRADE) return;
  if (st == LS_TRADE_DONE) return;  // ya esta hecho: no se puede deshacer
  declineLeft = 5;  // se repite un poco para que llegue
  st = LS_DECLINED;
}

bool LinkCore::takeTrade(TradePet &out) {
  if (!commitPending) return false;
  commitPending = false;
  out = theirs.t;
  return true;
}

void LinkCore::receive(const uint8_t src[6], const LinkMsg &m, uint32_t now) {
  if (md == LINK_NONE) return;
  if (m.magic != LINK_MAGIC || m.ver != LINK_PROTO_VER) return;
  if (m.mode != md || sameMac(src, myMac)) return;
  // si el otro ya tiene pareja y no soy yo, no es para mi
  if (!zeroMac(m.peerMac) && !sameMac(m.peerMac, myMac)) return;
  if (!havePeer) {
    if (st != LS_SEARCH || m.type != MSG_HELLO) return;
    memcpy(peer, src, 6);
    havePeer = true;
    theirs = m.pet;
    theirNonce = m.nonce;
  }
  if (!sameMac(src, peer)) return;
  lastRx = now;
  if (m.type == MSG_HELLO) {
    if (st == LS_SEARCH && sameMac(m.peerMac, myMac)) st = LS_READY;  // nos vemos los dos
  } else if (m.type == MSG_ACCEPT) {
    if (st == LS_SEARCH) st = LS_READY;  // su HELLO se perdio, pero esta ahi
    theirAccept = true;
  } else if (m.type == MSG_DECLINE) {
    if (st != LS_TRADE_DONE) st = LS_DECLINED;
  }
}

void LinkCore::poll(uint32_t now) {
  if (md == LINK_NONE) return;

  // completar el intercambio cuando los dos han aceptado
  if (st == LS_TRADE_WAIT && myAccept && theirAccept) {
    st = LS_TRADE_DONE;
    doneAt = now;
    commitPending = true;
  }

  // emision periodica
  if (declineLeft) {
    if (since(now, lastTx) >= 100) {
      send(MSG_DECLINE, now);
      declineLeft--;
    }
  } else if (st == LS_TRADE_WAIT || (st == LS_TRADE_DONE && since(now, doneAt) < (int32_t)LINK_AFTER_DONE_MS)) {
    if (since(now, lastTx) >= (int32_t)LINK_HELLO_MS) send(MSG_ACCEPT, now);
  } else if (st == LS_SEARCH || st == LS_READY) {
    if (since(now, lastTx) >= (int32_t)LINK_HELLO_MS) send(MSG_HELLO, now);
  }

  // tiempos muertos
  if (st == LS_SEARCH && !havePeer && since(now, t0) > (int32_t)LINK_SEARCH_MS) st = LS_LOST;
  if ((st == LS_SEARCH || st == LS_READY || st == LS_TRADE_WAIT) && havePeer &&
      since(now, lastRx) > (int32_t)LINK_SILENCE_MS)
    st = LS_LOST;
}

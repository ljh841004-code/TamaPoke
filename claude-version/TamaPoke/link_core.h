#pragma once
// Maquina de estados del tongsin, sin radio: la usa link.cpp (ESP-NOW) y los
// tests del PC (dos instancias conectadas por una "radio" falsa con perdidas).
#include <stdint.h>
#include <string.h>
#include "link.h"

#define LINK_MAGIC 0x314B5054UL   // "TPK1"
#define LINK_HELLO_MS 300UL
#define LINK_SEARCH_MS 90000UL    // sin pareja en 90 s: se rinde
#define LINK_SILENCE_MS 8000UL    // sin noticias del otro en 8 s: enlace perdido
#define LINK_AFTER_DONE_MS 3000UL // sigue confirmando tras completar el intercambio

enum : uint8_t { MSG_HELLO = 1, MSG_ACCEPT, MSG_DECLINE };

struct __attribute__((packed)) LinkMsg {
  uint32_t magic;
  uint8_t ver;
  uint8_t type;
  uint8_t mode;
  uint8_t peerMac[6];  // con quien me he emparejado (ceros = con nadie aun)
  uint32_t nonce;
  LinkPet pet;
  // ko10.4 (proto 4): la hora local del que envia, y si es de fiar
  uint32_t clock;
  uint8_t clockOk;
};

class LinkCore {
public:
  typedef void (*SendFn)(void *ctx, const LinkMsg &m);

  void start(LinkMode mode, const LinkPet &mine, const uint8_t myMac[6], uint32_t nonce,
             uint32_t now, SendFn fn, void *ctx);
  void stop();
  void receive(const uint8_t srcMac[6], const LinkMsg &m, uint32_t now);
  void poll(uint32_t now);
  void accept();
  void decline();
  bool takeTrade(TradePet &out);
  // ko10.4: mi hora (se manda en cada mensaje) y la del otro, si es de fiar
  void setClock(uint32_t epoch, bool ok, uint32_t now) { myClock = epoch; myClockOk = ok; myClockAt = now; }
  bool partnerClock(uint32_t now, uint32_t *epoch) const;

  LinkState state() const { return st; }
  LinkMode mode() const { return md; }
  const LinkPet &partner() const { return theirs; }
  const LinkPet &mine() const { return me; }
  bool partnerAccepted() const { return theirAccept; }
  bool iAmA() const { return memcmp(myMac, peer, 6) < 0; }
  uint32_t seed() const;

private:
  void send(uint8_t type, uint32_t now);
  LinkState st = LS_OFF;
  LinkMode md = LINK_NONE;
  uint8_t myMac[6] = { 0 }, peer[6] = { 0 };
  bool havePeer = false;
  LinkPet me, theirs;
  uint32_t myNonce = 0, theirNonce = 0;
  uint32_t lastTx = 0, lastRx = 0, t0 = 0, doneAt = 0;
  bool myAccept = false, theirAccept = false, commitPending = false;
  uint8_t declineLeft = 0;
  uint32_t myClock = 0, myClockAt = 0, theirClock = 0, theirClockAt = 0;
  bool myClockOk = false, theirClockOk = false;
  SendFn sendFn = nullptr;
  void *sendCtx = nullptr;
};

// Tongsin por ESP-NOW: transporte de radio alrededor de LinkCore (link_core.cpp).
#include "link.h"
#include "link_core.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_random.h>

#define LINK_CHANNEL 1

static LinkCore gCore;
static bool gRadio = false;
static bool gError = false;

// cola de recepcion: el callback corre en la tarea de WiFi, el loop la vacia
struct RxItem { uint8_t mac[6]; LinkMsg msg; };
#define RXQ 8
static RxItem gRxQ[RXQ];
static volatile uint8_t gRxHead = 0, gRxTail = 0;
static portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
static const uint8_t BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != (int)sizeof(LinkMsg) || !info) return;
  portENTER_CRITICAL(&gMux);
  uint8_t next = (gRxHead + 1) % RXQ;
  if (next != gRxTail) {  // cola llena: se descarta (el otro repite cada 300 ms)
    memcpy(gRxQ[gRxHead].mac, info->src_addr, 6);
    memcpy(&gRxQ[gRxHead].msg, data, sizeof(LinkMsg));
    gRxHead = next;
  }
  portEXIT_CRITICAL(&gMux);
}

static bool popRx(RxItem &out) {
  bool ok = false;
  portENTER_CRITICAL(&gMux);
  if (gRxTail != gRxHead) {
    out = gRxQ[gRxTail];
    gRxTail = (gRxTail + 1) % RXQ;
    ok = true;
  }
  portEXIT_CRITICAL(&gMux);
  return ok;
}

// broadcast siempre: no hace falta registrar al otro como par, y si hay mas
// TamaPoke cerca cada uno filtra por peerMac
static void radioSend(void *, const LinkMsg &m) {
  esp_now_send(BCAST, (const uint8_t *)&m, sizeof(m));
}

void linkStart(LinkMode mode, const LinkPet &mine) {
  linkStop();
  gError = false;
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);
  gRadio = true;
  if (esp_now_init() != ESP_OK) {
    gError = true;
  } else {
    esp_now_register_recv_cb(onRecv);
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = LINK_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  gRxHead = 0;
  gRxTail = 0;
  gCore.start(mode, mine, mac, esp_random(), millis(), radioSend, nullptr);
  Serial.printf("LINK start modo=%u\n", mode);
}

void linkStop() {
  gCore.stop();
  if (!gRadio) return;
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  gRadio = false;
}

bool linkActive() { return gCore.mode() != LINK_NONE; }
LinkState linkState() { return gError ? LS_ERROR : gCore.state(); }
LinkMode linkMode() { return gCore.mode(); }
const LinkPet &linkPartner() { return gCore.partner(); }
void linkSetClock(uint32_t epoch, bool ok) { gCore.setClock(epoch, ok, millis()); }
bool linkPartnerClock(uint32_t *epoch) { return gCore.partnerClock(millis(), epoch); }
const LinkPet &linkMine() { return gCore.mine(); }
bool linkPartnerAccepted() { return gCore.partnerAccepted(); }
bool linkIAmA() { return gCore.iAmA(); }
uint32_t linkSeed() { return gCore.seed(); }
void linkTradeAccept() { gCore.accept(); }
void linkTradeDecline() { gCore.decline(); }
bool linkTakeTrade(TradePet &out) { return gCore.takeTrade(out); }

void linkPoll(uint32_t now) {
  if (!linkActive() || gError) return;
  RxItem it;
  while (popRx(it)) gCore.receive(it.mac, it.msg, now);
  gCore.poll(now);
}

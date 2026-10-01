// Stubs de hardware para test/render: reloj, red, tongsin, audio, PMU, USB.
#include <string.h>
#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include "../shim/Preferences.h"
#include "../../net.h"
#include "../../link.h"
#include "../../audio.h"
#include "../../rtcbat.h"
#include <map>
#include <vector>

uint32_t gMockMillis = 1000;
uint32_t gMockEpoch = 1790343900;  // 2026-09-25 13:45 (hora local del reloj)
uint32_t millis() { return gMockMillis; }
uint32_t micros() { return gMockMillis * 1000; }
void delay(uint32_t ms) { gMockMillis += ms; }
void yield() {}
static uint32_t gSeed = 12345;
static uint32_t nextRand() { gSeed ^= gSeed << 13; gSeed ^= gSeed >> 17; gSeed ^= gSeed << 5; return gSeed; }
static long gForcedRand = -1;  // ko11.1: forzar random() en capturas
void mockForceRandom(long v) { gForcedRand = v; }
void mockClearForcedRandom() { gForcedRand = -1; }
long random(long n) {
  if (gForcedRand >= 0) return n > 0 ? gForcedRand % n : 0;
  return n > 0 ? (long)(nextRand() % (uint32_t)n) : 0;
}
long random(long lo, long hi) { return hi > lo ? lo + random(hi - lo) : lo; }
void randomSeed(unsigned long s) { gSeed = s ? s : 1; }
uint32_t esp_random() { return nextRand(); }
void *ps_malloc(size_t n) { return malloc(n); }
void pinMode(int, int) {}
void digitalWrite(int, int) {}
int digitalRead(int) { return 0; }
int digitalPinToInterrupt(int p) { return p; }
void attachInterrupt(int, void (*)(), int) {}
MockSerial Serial;
MockEsp ESP;
TwoWire Wire;
SDMMCFS SD_MMC;
const char *gSdRoot = ".";

// ---- red / tongsin ----
void netBegin() {}
void netSafeMode() {}
bool netConfigured() { return true; }
const char *netSsid() { return "MyHome_2.4G"; }
int16_t netTzMin() { return 540; }
void netSetTzMin(int16_t) {}
bool netAuto() { return true; }
void netSetAuto(bool) {}
uint32_t netLastSync() { return gMockEpoch - 3600; }
bool netSetCreds(const char *, const char *) { return true; }
void netClearCreds() {}
void netSyncNow() {}
bool netBusy() { return false; }
NetState netState() { return NET_IDLE; }
void netStartPortal() {}
void netStopPortal() {}
bool gMockPortal = false;
bool netPortalOn() { return gMockPortal; }
uint8_t netSavedCount() { return 3; }
const char *netSavedSsid(uint8_t) { return "MyHome_2.4G"; }
void netForgetSaved(uint8_t) {}
bool netOpenAllowed() { return true; }
void netSetOpenAllowed(bool) {}

// ---- QR: en el PC lo codifica segno (python) para comprobar el dibujo; en la
// placa lo hace esp_qrcode. Mismo formato de "handle": [lado][modulos...]
#include <qrcode.h>
static uint8_t gQrBuf[1 + 177 * 177];
esp_err_t esp_qrcode_generate(esp_qrcode_config_t *cfg, const char *text) {
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
           "python3 -c \"import segno,sys;q=segno.make(sys.argv[1],error='m',boost_error=False,micro=False);"
           "print(chr(10).join(''.join('1' if c else '0' for c in r) for r in q.matrix))\" '%s'", text);
  FILE *f = popen(cmd, "r");
  if (!f) return ESP_FAIL;
  char line[200];
  int n = 0;
  while (fgets(line, sizeof(line), f)) {
    int w = (int)strcspn(line, "\r\n");
    if (!w) continue;
    gQrBuf[0] = (uint8_t)w;
    for (int x = 0; x < w; x++) gQrBuf[1 + n * w + x] = line[x] == '1';
    n++;
  }
  pclose(f);
  if (!n) return ESP_FAIL;
  cfg->display_func(gQrBuf);
  return ESP_OK;
}
int esp_qrcode_get_size(esp_qrcode_handle_t q) { return q[0]; }
bool esp_qrcode_get_module(esp_qrcode_handle_t q, int x, int y) { return q[1 + y * q[0] + x]; }
const char *netApName() { return "TamaPoke-3F2A"; }
uint32_t netPoll(uint32_t) { return 0; }
bool netSerialCommand(const String &) { return false; }
static LinkPet gLp;
void linkStart(LinkMode, const LinkPet &) {}
void linkStop() {}
bool linkActive() { return false; }
LinkState linkState() { return LS_OFF; }
LinkMode linkMode() { return LINK_NONE; }
const LinkPet &linkPartner() { return gLp; }
void linkSetClock(uint32_t, bool) {}
bool linkPartnerClock(uint32_t *) { return false; }
const LinkPet &linkMine() { return gLp; }
bool linkPartnerAccepted() { return false; }
bool linkIAmA() { return true; }
uint32_t linkSeed() { return 1; }
void linkTradeAccept() {}
void linkTradeDecline() {}
void linkPoll(uint32_t) {}
bool linkTakeTrade(TradePet &) { return false; }

// ---- audio ----
static bool gOn = true;
static uint8_t gVol[3] = { 30, 70, 100 };
void audioBegin() {}
void sfxPlay(uint8_t) {}
void audioSetEnabled(bool on) { gOn = on; }
bool audioEnabled() { return gOn; }
uint32_t audioBgmSeconds() { return 170; }
void audioSetSleeping(bool) {}
void audioSetVolume(uint8_t c, uint8_t p) { if (c < 3) gVol[c] = p; }
uint8_t audioVolume(uint8_t c) { return c < 3 ? gVol[c] : 0; }
void audioLoadMusic() {}
void audioSetBattleMusic(bool, bool) {}
void audioSetMusicTrack(uint8_t) {}
bool audioPauseForUpload() { return true; }
void audioResumeAfterUpload() {}
void audioCry(uint16_t) {}
void audioSetMusicPaused(bool) {}
// ko11.8: dos fondos (bgm.wav sin titulo, bgm2.wav con titulo)
static uint8_t gBgmMask = 0xFF;
void audioBgmPath(uint8_t i, char *out, size_t n) {
  if (i == 0) snprintf(out, n, "/mons/bgm.wav"); else snprintf(out, n, "/mons/bgm%u.wav", (unsigned)(i + 1));
}
void audioScanBgm() {}
uint8_t audioBgmAvail() { return 0x03; }
uint8_t audioBgmMask() { return gBgmMask; }
void audioSetBgmMask(uint8_t m) { if (m) gBgmMask = m; }
uint16_t audioBgmSecondsOf(uint8_t i) { return i == 0 ? 170 : i == 1 ? 188 : 0; }
const char *audioBgmTitle(uint8_t i) { return i == 1 ? "Pallet Town" : ""; }
int8_t audioBgmNow() { return 1; }
void audioBgmPlay(uint8_t) {}
#include "../../sdupdate.h"
UpdCheck sdUpdateCheck(uint32_t *size) { if (size) *size = 1873367; return UPD_OK; }
bool sdUpdateRun(void (*)(uint32_t, uint32_t)) { return false; }
bool sdUpdateFileVersion(char *out, size_t n) { snprintf(out, n, "1.17-ko6.3"); return true; }

// ---- reloj / PMU ----
bool rtcBegin() { return true; }
uint32_t rtcEpoch() { return gMockEpoch; }
void rtcSetEpoch(uint32_t e) { gMockEpoch = e; }
bool batBegin() { return true; }
void batSetChargeLimit(bool) {}
void pmuEnablePanel() {}
int batPercent() { return 78; }
bool batCharging() { return false; }
int batMillivolts() { return 3900; }
bool usbPresent() { return false; }
void pwrSetup() {}
bool pwrShortPressed() { return false; }
uint8_t pwrPoll() { return 0; }

// ---- Preferences en memoria (la misma de los tests) ----
#include "build/prefs_impl.inc"

// ---- ko11.6: copia en la SD (en el PC no hay SD: dos copias de ejemplo) ----
#include "../../savebak.h"
static bool gBakDemo = true;
void bakInfo(BakSlot out[2]) {
  memset(out, 0, sizeof(BakSlot) * 2);
  if (!gBakDemo) return;
  out[0].ok = true; memcpy(out[0].h.magic, "TPBK", 4); out[0].h.seq = 4; out[0].h.epoch = 1790505000u; out[0].h.dex = 25; out[0].h.lvl = 23;
  out[1].ok = true; memcpy(out[1].h.magic, "TPBK", 4); out[1].h.seq = 5; out[1].h.epoch = 1790591400u; out[1].h.dex = 26; out[1].h.lvl = 31; out[1].h.flags = BAKF_MANUAL;
}
int bakNewest(const BakSlot s[2]) {
  if (s[0].ok && s[1].ok) return s[1].h.seq > s[0].h.seq ? 1 : 0;
  return s[0].ok ? 0 : s[1].ok ? 1 : -1;
}
bool bakBackupNow(int16_t, uint16_t, uint32_t, bool) { return true; }
bool bakRestore(uint8_t) { return false; }
bool bakCrashLog(const char *) { return false; }
bool panicTake(uint32_t *, uint8_t *n, uint32_t *) { *n = 0; return false; }
void panicRecBegin() {}

#include <string>
// Android (claude-version/android): lo mismo que test/render/stubs.cpp pero con el reloj de verdad,
// la hora del movil, la bateria del movil y la "NVS" guardada en un fichero. Sin red, sin tongsin.
#include <time.h>
#include <unistd.h>
#include <atomic>
#include <string.h>
#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include "../shim/Preferences.h"
#include "../../net.h"
#include "../../link.h"
#include "../../TamaPoke/audio.h"
#include "../../rtcbat.h"
#include <map>
#include <vector>

uint32_t gMockMillis = 0;  // (no se usa: lo pide la cabecera)
// reloj del juego = monotono menos el tiempo en segundo plano (la app parada no "vive" en millis;
// al volver, el tiempo de fuera se aplica con pet.syncClock como si la placa estuviera apagada)
static int64_t monoUs() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000; }
std::atomic<int64_t> gHostPausedUs{0};
static int64_t gHostT0 = monoUs();
static int64_t gameUs() { return monoUs() - gHostT0 - gHostPausedUs.load(); }
uint32_t millis() { return (uint32_t)(gameUs() / 1000); }
uint32_t micros() { return (uint32_t)gameUs(); }
void delay(uint32_t ms) { usleep(ms * 1000); }
void yield() {}
static uint32_t gSeed = (uint32_t)time(nullptr) | 1;
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
bool ledcAttach(uint8_t, uint32_t, uint8_t) { return true; }
bool ledcWrite(uint8_t, uint32_t) { return true; }
int digitalRead(int) { return 0; }
int digitalPinToInterrupt(int p) { return p; }
void attachInterrupt(int, void (*)(), int) {}
MockSerial Serial;
MockEsp ESP;
TwoWire Wire;
SDMMCFS SD_MMC;
const char *gSdRoot = ".";  // host.cpp: <filesDir>/sd

// ---- red / tongsin ----
void netBegin() {}
void netSafeMode() {}
bool netConfigured() { return false; }
const char *netSsid() { return ""; }
int16_t netTzMin() { return 540; }
void netSetTzMin(int16_t) {}
bool netAuto() { return false; }
void netSetAuto(bool) {}
uint32_t netLastSync() { return 0; }
bool netSetCreds(const char *, const char *) { return true; }
void netClearCreds() {}
void netSyncNow() {}
bool netBusy() { return false; }
NetState netState() { return NET_IDLE; }
void netStartPortal() {}
void netSetFwHooks(const NetFwHooks &) {}  // ko12.9.2
void netStopPortal() {}
bool gMockPortal = false;
bool netPortalOn() { return gMockPortal; }
bool gMockManual = false;  // ko12.9.25: manual.pdf en la SD
bool netManualOnSd() { return gMockManual; }
// ko12.9.4: /fw en la WiFi de casa (las pruebas cambian el estado)
NetLan gMockLan = LAN_OFF;
void netStartLanFw() { gMockLan = LAN_FAIL_NOSAVED; }  // el movil no hace de servidor
void netStopLanFw() { gMockLan = LAN_OFF; }
NetLan netLanState() { return gMockLan; }
const char *netLanIp() { return gMockLan == LAN_ON ? "192.168.0.123" : ""; }
uint32_t netLanLeftS(uint32_t) { return gMockLan == LAN_ON ? 540 : 0; }
uint8_t netSavedCount() { return 0; }
void netForgetSaved(uint8_t) {}
bool netOpenAllowed() { return false; }
int8_t gMockRealWx = -1;  // ko12.9.24
int8_t netRealWx(uint32_t) { return gMockRealWx; }
bool gMockWxOn = false;
bool netWxOn() { return gMockWxOn; }
void netSetWxOn(bool on) { gMockWxOn = on; }
void netSetOpenAllowed(bool) {}

// ---- QR: en el PC lo codifica segno (python) para comprobar el dibujo; en la
// placa lo hace esp_qrcode. Mismo formato de "handle": [lado][modulos...]
#include <qrcode.h>
static uint8_t gQrBuf[1 + 177 * 177];
esp_err_t esp_qrcode_generate(esp_qrcode_config_t *, const char *) { (void)gQrBuf; return ESP_FAIL; }  // sin QR en el movil
int esp_qrcode_get_size(esp_qrcode_handle_t q) { return q[0]; }
bool esp_qrcode_get_module(esp_qrcode_handle_t q, int x, int y) { return q[1 + y * q[0] + x]; }
const char *netApName() { return "TamaPoke-3F2A"; }
uint32_t netPoll(uint32_t) { return 0; }
bool netSerialCommand(const String &) { return false; }
LinkPet gLp;
LinkState gStubLinkState = LS_OFF;  // las pruebas lo cambian para pintar las pantallas del enlace
LinkMode gStubLinkMode = LINK_NONE;
void linkStart(LinkMode, const LinkPet &) {}
void linkStop() {}
bool linkActive() { return false; }
LinkState linkState() { return gStubLinkState; }
LinkMode linkMode() { return gStubLinkMode; }
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
uint8_t gStubPartnerAct = LA_WAIT;  // ko12.9.16: lo que "elige" el otro en el tongsin por turnos
void linkSetTurn(uint16_t, uint8_t) {}
bool linkPartnerAct(uint16_t, uint8_t *a) {
  if (gStubPartnerAct == LA_WAIT) return false;
  if (a) *a = gStubPartnerAct;
  return true;
}

#ifndef HOST_REAL_AUDIO  // Android: audio.cpp de verdad (audio_host.cpp); la prueba del PC, sin sonido
// ---- audio ----
static bool gOn = true;
static uint8_t gVol[3] = { 30, 70, 100 };
void audioBegin() {}
void sfxPlay(uint8_t) {}
void audioSetEnabled(bool on) { gOn = on; }
bool audioEnabled() { return gOn; }
bool audioSdFree() { return true; }
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
#endif
#include "../../sdupdate.h"
UpdCheck sdUpdateCheck(uint32_t *size) { if (size) *size = 0; return UPD_NONE; }
bool sdUpdateRun(void (*)(uint32_t, uint32_t)) { return false; }
bool sdUpdateFileVersion(char *out, size_t n) { if (n) out[0] = 0; return false; }

// ---- reloj / PMU ----
bool rtcBegin() { return true; }
volatile bool rtcJustSet = false;
// hora LOCAL guardada como si fuera UTC (como el firmware) + el ajuste que hagas en el juego
int32_t gHostClockAdj = 0;
static uint32_t phoneLocalEpoch() {
  time_t t = time(nullptr);
  struct tm lt;
#ifdef _WIN32  // PC (Windows): la hora local como si fuera UTC
  localtime_s(&lt, &t);
  return (uint32_t)_mkgmtime(&lt);
#else
  localtime_r(&t, &lt);
  return (uint32_t)(t + lt.tm_gmtoff);
#endif
}
uint32_t rtcEpoch() { return phoneLocalEpoch() + gHostClockAdj; }
void hostSaveClockAdj();
void rtcSetEpoch(uint32_t e) { gHostClockAdj = (int32_t)(e - phoneLocalEpoch()); rtcJustSet = true; hostSaveClockAdj(); }
bool batBegin() { return true; }
void batSetChargeLimit(bool) {}
void pmuEnablePanel() {}
static int readSysInt(const char *p, int def) {
  FILE *f = fopen(p, "r");
  if (!f) return def;
  int v = def;
  if (fscanf(f, "%d", &v) != 1) v = def;
  fclose(f);
  return v;
}
int gHostBatPct = -1;      // lo pone MainActivity (ACTION_BATTERY_CHANGED); -1 = no se sabe
bool gHostCharging = false;
int batPercent() { return gHostBatPct >= 0 ? gHostBatPct : readSysInt("/sys/class/power_supply/battery/capacity", 100); }
bool batCharging() { return gHostCharging; }
int batMillivolts() { return 3900; }
bool usbPresent() { return false; }
void pwrSetup() {}
uint8_t hostPwrTake();
uint8_t pwrPoll() { return hostPwrTake(); }  // menu de la app: [화면 끄기/켜기]

// ---- Preferences en memoria (la misma de los tests) ----
#include "prefs_impl.inc"
// host.cpp guarda / lee todo el almacen en un fichero: u32 nns, [u16 ns, ns, u32 nk, [u16 k, k, u32 nv, v]]
static void putU32(std::vector<uint8_t> &o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back((uint8_t)(v >> (8 * i))); }
static void putStr(std::vector<uint8_t> &o, const std::string &s) { o.push_back((uint8_t)s.size()); o.push_back((uint8_t)(s.size() >> 8)); o.insert(o.end(), s.begin(), s.end()); }
void hostPrefsDump(std::vector<uint8_t> &o) {
  o.clear();
  o.insert(o.end(), { 'T', 'P', 'N', 'V' });
  uint32_t nns = 0;
  for (auto &ns : gStore) nns += !ns.second.empty();  // los espacios vacios (solo abiertos) no se guardan
  putU32(o, nns);
  for (auto &ns : gStore) {
    if (ns.second.empty()) continue;
    putStr(o, ns.first);
    putU32(o, (uint32_t)ns.second.size());
    for (auto &kv : ns.second) {
      putStr(o, kv.first);
      putU32(o, (uint32_t)kv.second.size());
      o.insert(o.end(), kv.second.begin(), kv.second.end());
    }
  }
}
void hostPrefsLoad(const uint8_t *d, size_t n) {
  size_t p = 0;
  auto u32 = [&](uint32_t &v) { if (p + 4 > n) return false; v = d[p] | d[p + 1] << 8 | d[p + 2] << 16 | (uint32_t)d[p + 3] << 24; p += 4; return true; };
  auto str = [&](std::string &s) { if (p + 2 > n) return false; size_t l = d[p] | d[p + 1] << 8; p += 2; if (p + l > n) return false; s.assign((const char *)d + p, l); p += l; return true; };
  if (n < 8 || memcmp(d, "TPNV", 4) != 0) return;
  p = 4;
  uint32_t nns;
  if (!u32(nns)) return;
  for (uint32_t i = 0; i < nns; i++) {
    std::string ns;
    uint32_t nk;
    if (!str(ns) || !u32(nk)) return;
    for (uint32_t k = 0; k < nk; k++) {
      std::string key;
      uint32_t nv;
      if (!str(key) || !u32(nv) || p + nv > n) return;
      gStore[ns][key].assign(d + p, d + p + nv);
      p += nv;
    }
  }
}

// ---- ko11.6: copia en la SD (en el PC no hay SD: dos copias de ejemplo) ----
#include "../../savebak.h"
static bool gBakDemo = false;
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

// ko12.8: mons.pak en el PC: sin paquete, se lee la carpeta de siempre
#include "../../pak.h"
bool pakLoad() { return false; }
void pakUnload() {}
bool pakActive() { return false; }
uint32_t pakCount() { return 0; }
int8_t pakState() { return 0; }
void pakDiag(char *out, size_t n) { snprintf(out, n, "Android"); }
#ifndef HOST_REAL_AUDIO
uint32_t gBgmScanMs = 0;  // ko12.8.5 (audio.cpp)
#endif
File monsOpen(const char *path) { return SD_MMC.open(path, FILE_READ); }
bool monsExists(const char *path) { return SD_MMC.exists(path); }
bool monsIsDir(const char *path) { File d = SD_MMC.open(path); bool r = d && d.isDirectory(); if (d) d.close(); return r; }
void pakForEach(void (*)(const char *, uint32_t, void *), void *) {}
#include <dirent.h>
#include <sys/stat.h>
bool monsForEachName(void (*cb)(const char *, void *), void *ctx) {
  for (const char *sub : { "", "fx/" }) {
    std::string dir = std::string(gSdRoot) + "/mons/" + sub;
    DIR *d = opendir(dir.c_str());
    if (!d) continue;
    while (struct dirent *e = readdir(d)) {
      if (e->d_name[0] == '.') continue;
      struct stat st;
      if (stat((dir + e->d_name).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) continue;
      cb((std::string(sub) + e->d_name).c_str(), ctx);
    }
    closedir(d);
  }
  return true;
}
void pakSetPass(const char *) {}
const char *pakPassLabel() { return ""; }
bool pakScan(PakBuildInfo &out) { out.files = 1007; out.bytes = 268ull << 20; return true; }
uint8_t pakBuild(void (*progress)(uint64_t, uint64_t, uint32_t, uint32_t)) { if (progress) progress(1, 2, 500, 1007); return 0; }

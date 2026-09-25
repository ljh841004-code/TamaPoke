// WiFi + NTP + portal de configuracion. Ver net.h.
#include "net.h"
#include "link.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <esp_mac.h>
#include <time.h>

#define NET_CONNECT_MS 20000UL
#define NET_NTP_MS 15000UL
#define NET_AUTO_EVERY_MS (24UL * 3600UL * 1000UL)
#define NET_BOOT_DELAY_MS 6000UL
#define NET_PORTAL_MS (10UL * 60UL * 1000UL)  // el portal se cierra solo a los 10 min
#define NET_AP_PASS "tamapoke"

static char gSsid[33] = "";
static char gPass[65] = "";
static int16_t gTz = 540;
static bool gAuto = true;
static uint32_t gLastSync = 0;

static NetState gState = NET_IDLE;
static uint32_t gT0 = 0;            // inicio de la fase actual
static uint32_t gLastAutoTry = 0;   // millis del ultimo intento automatico
static bool gBootTried = false;
static volatile bool gSntpDone = false;
static bool gPortal = false;
static uint32_t gPortalT0 = 0;
static bool gSyncAfterPortal = false;
static char gApName[24] = "TamaPoke";

static WebServer *gWeb = nullptr;
static DNSServer *gDns = nullptr;

static void loadCfg() {
  Preferences p;
  p.begin("tpnet", true);
  p.getString("ssid", gSsid, sizeof(gSsid));
  p.getString("pass", gPass, sizeof(gPass));
  gTz = p.getShort("tz", 540);
  gAuto = p.getBool("auto", true);
  gLastSync = p.getUInt("lsync", 0);
  p.end();
  if (gTz < -720 || gTz > 840) gTz = 540;
}

static void saveCfg() {
  Preferences p;
  p.begin("tpnet", false);
  p.putString("ssid", gSsid);
  p.putString("pass", gPass);
  p.putShort("tz", gTz);
  p.putBool("auto", gAuto);
  p.putUInt("lsync", gLastSync);
  p.end();
}

void netBegin() {
  loadCfg();
  uint8_t mac[6] = { 0 };
  esp_read_mac(mac, ESP_MAC_WIFI_STA);  // sin arrancar la radio
  snprintf(gApName, sizeof(gApName), "TamaPoke-%02X%02X", mac[4], mac[5]);
}

bool netConfigured() { return gSsid[0] != 0; }
const char *netSsid() { return gSsid; }
int16_t netTzMin() { return gTz; }
void netSetTzMin(int16_t m) {
  if (m < -720) m = -720;
  if (m > 840) m = 840;
  gTz = m;
  saveCfg();
}
bool netAuto() { return gAuto; }
void netSetAuto(bool on) { gAuto = on; saveCfg(); }
uint32_t netLastSync() { return gLastSync; }
bool netPortalOn() { return gPortal; }
const char *netApName() { return gApName; }
NetState netState() { return gState; }
bool netBusy() { return gPortal || gState == NET_CONNECTING || gState == NET_NTP; }

static void radioOff() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

static void onSntp(struct timeval *) { gSntpDone = true; }

void netSyncNow() {
  if (!netConfigured() || gPortal || linkActive()) return;
  if (gState == NET_CONNECTING || gState == NET_NTP) return;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin(gSsid, gPass);
  gState = NET_CONNECTING;
  gT0 = millis();
  Serial.printf("NET conectando a '%s'\n", gSsid);
}

bool netSetCreds(const char *ssid, const char *pass) {
  if (!ssid || !ssid[0] || strlen(ssid) >= sizeof(gSsid)) return false;
  if (pass && strlen(pass) >= sizeof(gPass)) return false;
  strncpy(gSsid, ssid, sizeof(gSsid) - 1);
  gSsid[sizeof(gSsid) - 1] = 0;
  strncpy(gPass, pass ? pass : "", sizeof(gPass) - 1);
  gPass[sizeof(gPass) - 1] = 0;
  saveCfg();
  return true;
}

void netClearCreds() {
  gSsid[0] = gPass[0] = 0;
  saveCfg();
}

// ---------------------------------------------------------------- portal web

static String htmlEsc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

static void pageRoot() {
  String opts;
  int n = WiFi.scanComplete();
  if (n > 0) {
    for (int i = 0; i < n && i < 20; i++) {
      String s = WiFi.SSID(i);
      if (s.length()) opts += "<option value=\"" + htmlEsc(s) + "\">";
    }
  }
  String tz;
  for (int m = -720; m <= 840; m += 30) {
    char lab[16];
    snprintf(lab, sizeof(lab), "UTC%c%d:%02d", m < 0 ? '-' : '+', abs(m) / 60, abs(m) % 60);
    tz += "<option value=\"" + String(m) + "\"" + (m == gTz ? " selected" : "") + ">" + lab + "</option>";
  }
  String h;
  h.reserve(4096);
  h += F("<!doctype html><html><head><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>TamaPoke WiFi</title><style>"
         "body{font-family:sans-serif;background:#fff6e0;margin:0;padding:20px;color:#222}"
         ".c{max-width:420px;margin:auto;background:#fff;border-radius:16px;padding:20px;"
         "box-shadow:0 2px 8px #0002}h1{color:#e7352c;font-size:22px;margin-top:0}"
         "label{display:block;margin-top:14px;font-weight:bold}"
         "input,select{width:100%;box-sizing:border-box;font-size:17px;padding:10px;"
         "border:2px solid #ccc;border-radius:10px;margin-top:6px}"
         "button{margin-top:20px;width:100%;font-size:18px;padding:12px;border:0;"
         "border-radius:12px;background:#e7352c;color:#fff}small{color:#777}"
         "</style></head><body><div class=c><h1>TamaPoke WiFi</h1>"
         "<p>시계를 인터넷 시간(NTP)에 맞추기 위한 WiFi를 설정해요.<br>"
         "<small>Set the WiFi used to sync the clock (NTP).</small></p>"
         "<form method=post action=/save>"
         "<label>WiFi 이름 (SSID)</label><input name=s list=nets required maxlength=32 value=\"");
  h += htmlEsc(gSsid);
  h += F("\"><datalist id=nets>");
  h += opts;
  h += F("</datalist><label>비밀번호 (Password)</label>"
         "<input name=p type=password maxlength=64 placeholder='(없으면 비워두기)'>"
         "<label>시간대 (Time zone)</label><select name=z>");
  h += tz;
  h += F("</select><button>저장하고 시간 맞추기 / Save</button></form>"
         "<p><small>2.4GHz WiFi만 됩니다. 5GHz는 안 돼요.</small></p></div></body></html>");
  gWeb->send(200, "text/html; charset=utf-8", h);
}

static void pageSave() {
  String s = gWeb->arg("s"), p = gWeb->arg("p"), z = gWeb->arg("z");
  s.trim();
  bool ok = netSetCreds(s.c_str(), p.c_str());
  if (z.length()) netSetTzMin((int16_t)z.toInt());
  String h = F("<!doctype html><html><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'></head>"
               "<body style='font-family:sans-serif;text-align:center;padding:40px'>");
  h += ok ? F("<h2>저장했어요! Saved!</h2><p>TamaPoke가 이제 시간을 맞춰요.<br>"
              "이 WiFi 연결은 곧 끊겨요.</p>")
          : F("<h2>WiFi 이름을 확인해 주세요</h2><p><a href=/>돌아가기</a></p>");
  h += F("</body></html>");
  gWeb->send(200, "text/html; charset=utf-8", h);
  if (ok) {
    gSyncAfterPortal = true;
    gPortalT0 = millis() - NET_PORTAL_MS + 2500;  // cerrar en 2,5 s (que llegue la respuesta)
  }
}

static void pageRedirect() {
  gWeb->sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  gWeb->send(302, "text/plain", "");
}

void netStartPortal() {
  if (gPortal || linkActive()) return;
  if (gState == NET_CONNECTING || gState == NET_NTP) radioOff();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(gApName, NET_AP_PASS);
  delay(100);
  WiFi.scanNetworks(true);  // asincrono: la lista llega a la web cuando termine
  gDns = new DNSServer();
  gDns->start(53, "*", WiFi.softAPIP());  // portal cautivo: todo lleva a la web
  gWeb = new WebServer(80);
  gWeb->on("/", HTTP_GET, pageRoot);
  gWeb->on("/save", HTTP_POST, pageSave);
  gWeb->onNotFound(pageRedirect);
  gWeb->begin();
  gPortal = true;
  gPortalT0 = millis();
  gSyncAfterPortal = false;
  gState = NET_PORTAL;
  Serial.printf("NET portal %s (%s) en %s\n", gApName, NET_AP_PASS,
                WiFi.softAPIP().toString().c_str());
}

void netStopPortal() {
  if (!gPortal) return;
  if (gWeb) { gWeb->stop(); delete gWeb; gWeb = nullptr; }
  if (gDns) { gDns->stop(); delete gDns; gDns = nullptr; }
  WiFi.scanDelete();
  WiFi.softAPdisconnect(true);
  radioOff();
  gPortal = false;
  gState = NET_IDLE;
  if (gSyncAfterPortal) {
    gSyncAfterPortal = false;
    netSyncNow();
  }
}

// ---------------------------------------------------------------- sondeo

// ms transcurridos desde t, CON signo. ko6.1: el portal se abria y se cerraba en
// el mismo loop: `now` se toma antes del toque que arranca el portal con
// millis(), la resta sin signo daba ~49 dias y saltaba el tiempo maximo.
static inline int32_t since(uint32_t now, uint32_t t) { return (int32_t)(now - t); }

uint32_t netPoll(uint32_t now) {
  if (gPortal) {
    gDns->processNextRequest();
    gWeb->handleClient();
    if (since(now, gPortalT0) > (int32_t)NET_PORTAL_MS) netStopPortal();
    return 0;
  }

  // sincronizacion automatica: poco despues de arrancar y luego cada 24 h
  if (gAuto && netConfigured() && !linkActive() &&
      (gState == NET_IDLE || gState == NET_OK || gState == NET_FAIL_WIFI || gState == NET_FAIL_NTP)) {
    bool due = !gBootTried ? (now > NET_BOOT_DELAY_MS)
                           : (since(now, gLastAutoTry) > (int32_t)NET_AUTO_EVERY_MS);
    if (due) {
      gBootTried = true;
      gLastAutoTry = now;
      netSyncNow();
    }
  }

  if (gState == NET_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      gSntpDone = false;
      sntp_set_time_sync_notification_cb(onSntp);
      configTime(0, 0, "pool.ntp.org", "time.google.com", "kr.pool.ntp.org");
      gState = NET_NTP;
      gT0 = now;
    } else if (since(now, gT0) > (int32_t)NET_CONNECT_MS) {
      Serial.println("NET fallo: WiFi");
      radioOff();
      gState = NET_FAIL_WIFI;
    }
  } else if (gState == NET_NTP) {
    time_t utc = time(nullptr);
    if (gSntpDone && utc > 1700000000) {
      uint32_t local = (uint32_t)((int64_t)utc + (int64_t)gTz * 60);
      esp_sntp_stop();
      radioOff();
      gLastSync = local;
      saveCfg();
      gState = NET_OK;
      Serial.printf("NET hora ok: utc=%lu local=%lu\n", (unsigned long)utc, (unsigned long)local);
      return local;
    }
    if (since(now, gT0) > (int32_t)NET_NTP_MS) {
      Serial.println("NET fallo: NTP");
      esp_sntp_stop();
      radioOff();
      gState = NET_FAIL_NTP;
    }
  }
  return 0;
}

// ---------------------------------------------------------------- consola serie

bool netSerialCommand(const String &line) {
  if (line.startsWith("WIFI ")) {
    String a = line.substring(5);
    int bar = a.indexOf('|');
    String s = bar >= 0 ? a.substring(0, bar) : a;
    String p = bar >= 0 ? a.substring(bar + 1) : "";
    bool ok = netSetCreds(s.c_str(), p.c_str());
    Serial.printf("wifi=%s %s\n", gSsid, ok ? "ok" : "ERROR");
    if (ok) netSyncNow();
    Serial.println("DONE");
    return true;
  }
  if (line == "WIFIOFF") {
    netClearCreds();
    Serial.println("DONE");
    return true;
  }
  if (line == "NTP") {
    netSyncNow();
    Serial.println("DONE");
    return true;
  }
  if (line.startsWith("TZ ")) {
    netSetTzMin((int16_t)line.substring(3).toInt());
    Serial.printf("tz=%d min\n", gTz);
    Serial.println("DONE");
    return true;
  }
  if (line == "NET") {
    Serial.printf("ssid=%s tz=%d auto=%d state=%u last=%lu portal=%d\n", gSsid, gTz, gAuto,
                  gState, (unsigned long)gLastSync, gPortal);
    Serial.println("DONE");
    return true;
  }
  return false;
}

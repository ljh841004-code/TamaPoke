// WiFi + NTP + portal de configuracion. Ver net.h.
#include "net.h"
#include "link.h"
#include "net_pick.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <esp_mac.h>
#include <time.h>

#define NET_CONNECT_MS 12000UL   // ko8: por red (hay varias que probar)
#define NET_NTP_MS 12000UL
#define NET_SCAN_MS 9000UL
#define NET_AUTO_EVERY_MS (24UL * 3600UL * 1000UL)
#define NET_BOOT_DELAY_MS 6000UL
#define NET_PORTAL_MS (10UL * 60UL * 1000UL)  // el portal se cierra solo a los 10 min
#define NET_AP_PASS "tamapoke"

// fork KO (ko8): hasta 5 WiFi guardadas (la 0 es la mas reciente) y, si se
// permite, las abiertas de alrededor. gSsid = la que se esta probando / la
// ultima que funciono (para la pantalla).
static char gSaved[NET_MAX_SAVED][33];
static char gSavedPass[NET_MAX_SAVED][65];
static uint8_t gNSaved = 0;
static bool gOpenOk = true;
static char gSsid[33] = "";
static NetCand gCand[NET_MAX_CAND];
static char gCandSsid[NET_MAX_CAND][33];
static int gNCand = 0, gCandI = 0;
static bool gAnyConnected = false;
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
  memset(gSaved, 0, sizeof(gSaved));
  memset(gSavedPass, 0, sizeof(gSavedPass));
  gNSaved = 0;
  if (p.isKey("n")) {
    uint8_t n = p.getUChar("n", 0);
    for (uint8_t i = 0; i < n && i < NET_MAX_SAVED; i++) {
      char k[4] = { 's', (char)('0' + i), 0 };
      p.getString(k, gSaved[gNSaved], 33);
      k[0] = 'p';
      p.getString(k, gSavedPass[gNSaved], 65);
      if (gSaved[gNSaved][0]) gNSaved++;
    }
  } else if (p.isKey("ssid")) {  // ko7 y antes: una sola red
    p.getString("ssid", gSaved[0], 33);
    p.getString("pass", gSavedPass[0], 65);
    if (gSaved[0][0]) gNSaved = 1;
  }
  gOpenOk = p.getBool("open", true);
  strcpy(gSsid, gSaved[0]);
  gTz = p.getShort("tz", 540);
  gAuto = p.getBool("auto", true);
  gLastSync = p.getUInt("lsync", 0);
  p.end();
  if (gTz < -720 || gTz > 840) gTz = 540;
}

static void saveCfg() {
  Preferences p;
  p.begin("tpnet", false);
  p.putUChar("n", gNSaved);
  for (uint8_t i = 0; i < NET_MAX_SAVED; i++) {
    char k[4] = { 's', (char)('0' + i), 0 };
    if (i < gNSaved) {
      p.putString(k, gSaved[i]);
      k[0] = 'p';
      p.putString(k, gSavedPass[i]);
    } else {
      p.remove(k);
      k[0] = 'p';
      p.remove(k);
    }
  }
  p.remove("ssid");  // formato de ko7: ya migrado
  p.remove("pass");
  p.putBool("open", gOpenOk);
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

bool netConfigured() { return gNSaved > 0; }
const char *netSsid() { return gSsid[0] ? gSsid : gSaved[0]; }
uint8_t netSavedCount() { return gNSaved; }
const char *netSavedSsid(uint8_t i) { return i < gNSaved ? gSaved[i] : ""; }
bool netOpenAllowed() { return gOpenOk; }
void netSetOpenAllowed(bool on) { gOpenOk = on; saveCfg(); }
static bool netCanSync() { return gNSaved > 0 || gOpenOk; }
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
bool netBusy() { return gPortal || gState == NET_SCAN || gState == NET_CONNECTING || gState == NET_NTP; }

static void radioOff() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

static void onSntp(struct timeval *) { gSntpDone = true; }

// ko8: primero se escanea y luego se prueban las candidatas en orden (net_pick.h)
void netSyncNow() {
  if (!netCanSync() || gPortal || linkActive()) return;
  if (netBusy()) return;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  gState = NET_SCAN;
  gT0 = millis();
  gAnyConnected = false;
  Serial.println("NET buscando WiFi...");
}

static void tryCand(int i, uint32_t now) {
  gCandI = i;
  const NetCand &c = gCand[i];
  strcpy(gSsid, gCandSsid[i]);
  WiFi.disconnect(false, false);
  if (c.saved >= 0 && gSavedPass[c.saved][0]) WiFi.begin(gSsid, gSavedPass[c.saved]);
  else WiFi.begin(gSsid);
  gState = NET_CONNECTING;
  gT0 = now;
  Serial.printf("NET conectando a '%s'%s\n", gSsid, c.saved < 0 ? " (abierta)" : "");
}

// la candidata actual fallo: la siguiente, o se acabo
static void nextCand(uint32_t now) {
  if (gCandI + 1 < gNCand) { tryCand(gCandI + 1, now); return; }
  radioOff();
  gState = gAnyConnected ? NET_FAIL_NTP : NET_FAIL_WIFI;
  strcpy(gSsid, gSaved[0]);
  Serial.println(gAnyConnected ? "NET fallo: NTP" : "NET fallo: WiFi");
}

static void scanDone(uint32_t now) {
  int n = WiFi.scanComplete();
  if (n < 0) n = 0;
  if (n > 40) n = 40;
  static String names[40];
  NetSeen seen[40];
  for (int i = 0; i < n; i++) {
    names[i] = WiFi.SSID(i);
    seen[i].ssid = names[i].c_str();
    seen[i].rssi = (int16_t)WiFi.RSSI(i);
    seen[i].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  gNCand = netPickCandidates(gSaved, gNSaved, seen, n, gOpenOk, gCand, NET_MAX_CAND);
  for (int i = 0; i < gNCand; i++) {
    const char *src = gCand[i].seen >= 0 ? seen[gCand[i].seen].ssid : gSaved[gCand[i].saved];
    strncpy(gCandSsid[i], src, 32);
    gCandSsid[i][32] = 0;
  }
  for (int i = 0; i < n; i++) names[i] = String();
  WiFi.scanDelete();
  Serial.printf("NET %d redes vistas, %d candidatas\n", n, gNCand);
  if (!gNCand) {
    radioOff();
    gState = NET_FAIL_WIFI;
    return;
  }
  tryCand(0, now);
}

bool netSetCreds(const char *ssid, const char *pass) {
  if (!ssid || !ssid[0] || strlen(ssid) > 32) return false;
  if (pass && strlen(pass) > 64) return false;
  netRememberFront(gSaved, gSavedPass, gNSaved, ssid, pass);
  strcpy(gSsid, gSaved[0]);
  saveCfg();
  return true;
}

void netForgetSaved(uint8_t i) {
  netForget(gSaved, gSavedPass, gNSaved, i);
  strcpy(gSsid, gSaved[0]);
  saveCfg();
}

void netClearCreds() {
  while (gNSaved) netForget(gSaved, gSavedPass, gNSaved, 0);
  gSsid[0] = 0;
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
  // fork KO (ko7): lista de redes para ELEGIR (el datalist solo sugeria al
  // escribir y en muchos moviles no se veia). Sin repetidos, la mas fuerte primero
  // (scanNetworks ya las da ordenadas por senal).
  String opts;
  int n = WiFi.scanComplete();
  int shown = 0;
  for (int i = 0; i < n && shown < 20; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (int j = 0; j < i && !dup; j++) dup = WiFi.SSID(j) == s;
    if (dup) continue;
    int r = WiFi.RSSI(i);
    const char *bars = r > -60 ? "\u2582\u2584\u2586" : r > -75 ? "\u2582\u2584" : "\u2582";
    String e = htmlEsc(s);
    opts += "<option value=\"" + e + "\"" + ">" + e + "  " + bars +
            (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "" : " \U0001F512") + "</option>";
    shown++;
  }
  bool scanning = n == WIFI_SCAN_RUNNING;
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
         "h2{font-size:17px;margin:22px 0 6px}.ck{font-weight:normal}.ck input{width:auto;margin-right:8px}"
         ".sv{display:flex;justify-content:space-between;padding:10px;border:1px solid #ddd;"
         "border-radius:10px;margin-top:6px;font-size:16px}.sv a{color:#e7352c}"
         "</style></head><body><div class=c><h1>TamaPoke WiFi</h1>"
         "<p>시계를 인터넷 시간(NTP)에 맞추기 위한 WiFi를 설정해요.<br>"
         "<small>Set the WiFi used to sync the clock (NTP).</small></p>"
         "<form method=post action=/save>"
         "<label>WiFi 선택 (Choose)</label><select id=pick "
         "onchange=\"if(this.value)document.getElementById('s').value=this.value\">"
         "<option value=''>");
  h += scanning ? F("검색 중... (Searching)") : shown ? F("-- 목록에서 고르기 --") : F("찾은 WiFi 없음 (None)");
  h += F("</option>");
  h += opts;
  h += F("</select><small><a href=/rescan>다시 검색 (Rescan)</a></small>"
         "<label>WiFi 이름 (SSID) <small>직접 입력도 돼요</small></label>"
         "<input id=s name=s maxlength=32 placeholder='(새 WiFi 추가)'>"
         "<label>비밀번호 (Password)</label>"
         "<input name=p type=password maxlength=64 placeholder='(없으면 비워두기)'>"
         "<label>시간대 (Time zone)</label><select name=z>");
  h += tz;
  // ko8: WiFi abiertas como ultimo recurso
  h += F("</select><label class=ck><input type=checkbox name=o value=1");
  if (gOpenOk) h += F(" checked");
  h += F("> 비밀번호 없는 WiFi도 자동 연결 (시간만 받고 바로 끊어요)</label>"
         "<button>저장하고 시간 맞추기 / Save</button></form>");
  // ko8: WiFi guardadas (hasta 5): se prueba la de mejor senal
  h += F("<h2>저장된 WiFi (최대 5개)</h2>");
  if (!gNSaved) h += F("<p><small>아직 없어요</small></p>");
  for (uint8_t i = 0; i < gNSaved; i++) {
    h += "<div class=sv><span>" + htmlEsc(gSaved[i]) + "</span><a href=/del?i=" + String(i) +
         " onclick=\"return confirm('삭제할까요?')\">삭제</a></div>";
  }
  h += F("<p><small>켤 때와 하루 한 번, 주변에서 신호가 가장 센 저장된 WiFi로 시간을 맞춰요."
         " 저장된 WiFi가 없으면 비밀번호 없는 WiFi를 써요.</small></p>"
         "<p><small>2.4GHz WiFi만 됩니다. 5GHz는 안 돼요.</small></p></div>");
  // la busqueda aun no acabo: recargar sola en 3 s para que aparezca la lista
  if (scanning) h += F("<script>setTimeout(function(){location.reload()},3000)</script>");
  h += F("</body></html>");
  gWeb->send(200, "text/html; charset=utf-8", h);
}

static void pageSave() {
  String s = gWeb->arg("s"), p = gWeb->arg("p"), z = gWeb->arg("z");
  s.trim();
  // ko8: sin nombre solo se guardan los ajustes (zona horaria, WiFi abiertas)
  bool ok = s.length() ? netSetCreds(s.c_str(), p.c_str()) : true;
  gOpenOk = gWeb->hasArg("o");
  if (z.length()) netSetTzMin((int16_t)z.toInt());
  else saveCfg();
  ok = ok && (gNSaved > 0 || gOpenOk);
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

static void pageDel() {
  int i = gWeb->arg("i").toInt();
  if (gWeb->hasArg("i") && i >= 0 && i < gNSaved) netForgetSaved((uint8_t)i);
  gPortalT0 = millis();
  gWeb->sendHeader("Location", "/", true);
  gWeb->send(302, "text/plain", "");
}

static void pageRescan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  gPortalT0 = millis();  // sigue abierto: el usuario esta en ello
  gWeb->sendHeader("Location", "/", true);
  gWeb->send(302, "text/plain", "");
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
  gWeb->on("/rescan", HTTP_GET, pageRescan);
  gWeb->on("/del", HTTP_GET, pageDel);
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
  if (gAuto && netCanSync() && !linkActive() &&
      (gState == NET_IDLE || gState == NET_OK || gState == NET_FAIL_WIFI || gState == NET_FAIL_NTP)) {
    bool due = !gBootTried ? (now > NET_BOOT_DELAY_MS)
                           : (since(now, gLastAutoTry) > (int32_t)NET_AUTO_EVERY_MS);
    if (due) {
      gBootTried = true;
      gLastAutoTry = now;
      netSyncNow();
    }
  }

  if (gState == NET_SCAN) {
    if (WiFi.scanComplete() >= 0 || since(now, gT0) > (int32_t)NET_SCAN_MS) scanDone(now);
  } else if (gState == NET_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      gAnyConnected = true;
      gSntpDone = false;
      sntp_set_time_sync_notification_cb(onSntp);
      configTime(0, 0, "pool.ntp.org", "time.google.com", "kr.pool.ntp.org");
      gState = NET_NTP;
      gT0 = now;
    } else if (since(now, gT0) > (int32_t)NET_CONNECT_MS) {
      Serial.printf("NET '%s' no conecta\n", gSsid);
      nextCand(now);
    }
  } else if (gState == NET_NTP) {
    time_t utc = time(nullptr);
    if (gSntpDone && utc > 1700000000) {
      uint32_t local = (uint32_t)((int64_t)utc + (int64_t)gTz * 60);
      esp_sntp_stop();
      radioOff();
      gLastSync = local;
      // una guardada que funciona pasa a ser la mas reciente (se prueba primero)
      const NetCand &c = gCand[gCandI];
      if (c.saved > 0) {  // copia: netRememberFront desplaza la propia lista
        char ss[33], pp[65];
        memcpy(ss, gSaved[c.saved], sizeof(ss));
        memcpy(pp, gSavedPass[c.saved], sizeof(pp));
        netRememberFront(gSaved, gSavedPass, gNSaved, ss, pp);
      }
      saveCfg();
      gState = NET_OK;
      Serial.printf("NET hora ok: utc=%lu local=%lu\n", (unsigned long)utc, (unsigned long)local);
      return local;
    }
    if (since(now, gT0) > (int32_t)NET_NTP_MS) {
      // p. ej. WiFi abierta con pagina de registro: no deja salir a internet
      Serial.printf("NET '%s' sin hora (NTP)\n", gSsid);
      esp_sntp_stop();
      nextCand(now);
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
    Serial.printf("ssid=%s tz=%d auto=%d open=%d state=%u last=%lu portal=%d\n", gSsid, gTz, gAuto,
                  gOpenOk, gState, (unsigned long)gLastSync, gPortal);
    for (uint8_t i = 0; i < gNSaved; i++) Serial.printf("  guardada %u: %s\n", i, gSaved[i]);
    Serial.println("DONE");
    return true;
  }
  return false;
}

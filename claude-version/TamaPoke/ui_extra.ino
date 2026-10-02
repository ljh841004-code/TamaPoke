// ui_extra.ino - pantallas nuevas del fork KO (se concatena tras TamaPoke.ino)
//
//   - Red: WiFi + hora por NTP (desde la pantalla de hora, pildora "WiFi")
//   - Yasaeng: batalla por turnos contra un Pokemon salvaje (ficha > Batalla,
//     o el aviso "! yasaeng !" que aparece de vez en cuando)
//   - Tongsin: batalla automatica o intercambio entre dos TamaPoke (ESP-NOW)
//
// Se engancha al sketch principal solo por funciones (extraRender, extraTap,
// extraSwipe, extraLoop...), para tocar lo minimo el fichero original.

enum : uint8_t { XS_NONE = 0, XS_NET, XS_WILD, XS_LINKMENU, XS_LINK, XS_BOX, XS_VOL, XS_UPD, XS_RESET,
                 XS_REGION,    // ko10.1: elegir region antes del salvaje
                 XS_GYM, XS_DAILY,  // ko10.4: gimnasios y reto del dia
                 XS_NEXTPICK,       // ko10.5: elegir el siguiente tras un ciclo
                 XS_CANDY,          // ko10.11: bolsa de caramelos
                 XS_FAME, XS_BAK,
                 XS_BGM, XS_BRIGHT, XS_PARTY,
                 XS_STORY, XS_STORYCH, XS_SCENE, XS_ROGUE,
                 XS_SET,     // ko11.26: menu de ajustes
                 XS_STRAIN };  // ko11.28: combates de entrenamiento de la historia  // ko11.21: historia  // ko11.18: brillo  // ko11.8: elegir los fondos normales  // ko11.6: copia en la SD         // ko11.1: salon de la fama (campeones de la liga)
uint8_t xScreen = XS_NONE;

// ko11.17: [<] vuelve a la pantalla DESDE LA QUE se abrio el menu (la ficha, el
// menu de entrenamiento o la hora), no siempre a la principal. retMark() se
// llama al abrir, ANTES de cerrar la pantalla de origen; entre pantallas extra
// (red -> copias, gimnasios -> salon) no cambia: cada una sabe a cual volver
enum : uint8_t { RET_MAIN = 0, RET_CARD, RET_TRAIN, RET_CLOCK, RET_SET };
uint8_t gRet = RET_MAIN, gRetPage = 0;
void retMark() {
  if (xScreen == XS_SET) { gRet = RET_SET; gRetPage = 0; return; }  // ko11.26: [<] vuelve a ajustes
  if (xScreen != XS_NONE) return;
  if (cardOpen) { gRet = RET_CARD; gRetPage = cardPage; }
  else if (trainMenuOpen) { gRet = RET_TRAIN; gRetPage = trainMenuPage; }
  else if (clockOpen) { gRet = RET_CLOCK; gRetPage = 0; }
  else { gRet = RET_MAIN; gRetPage = 0; }
}
void reopenTrainMenu(uint8_t page);  // train.ino
void goBack() {
  xScreen = XS_NONE;
  uint8_t r = gRet;
  gRet = RET_MAIN;
  if (r == RET_CARD && !pet.isEgg()) { cardOpen = true; cardPage = gRetPage; }
  else if (r == RET_TRAIN && !pet.isEgg()) reopenTrainMenu(gRetPage);
  else if (r == RET_CLOCK) clockOpen = true;
  else if (r == RET_SET) xScreen = XS_SET;
  navGuardUntil = millis() + 300;  // el dedo que toco [<] no pulsa lo de debajo
}

// aviso breve en la pantalla principal
char toastBuf[96] = "";
uint32_t toastUntil = 0;

void showToast(const char *s) {
  strncpy(toastBuf, s, sizeof(toastBuf) - 1);
  toastBuf[sizeof(toastBuf) - 1] = 0;
  toastUntil = millis() + 2600;
}

// ko11.16: "<tipo> 공격구슬" / "<tipo> 방어구슬"
void orbName(uint16_t o, char *out, size_t n) {
  snprintf(out, n, XT(orbDef(o) ? X_ORB_DEF_FMT : X_ORB_ATK_FMT), typeName(orbType(o)));
}

// ko11.16: un orbe del tipo dado (ataque o defensa y % al azar) a la bolsa, con aviso
uint8_t orbDrop(uint8_t type) {
  uint16_t o = orbMake(type, random(2) != 0, (uint8_t)(ORB_MIN_PCT + random(ORB_MAX_PCT - ORB_MIN_PCT + 1)));
  uint8_t r = pet.gainOrb(o);
  if (!r) return 0;
  char nm[40], t[96];
  orbName(o, nm, sizeof(nm));
  snprintf(t, sizeof(t), XT(r == 1 ? X_ORB_GOT_FMT : r == 2 ? X_ORB_UP_FMT : X_ORB_CANDY_FMT), nm, orbPct(o));
  showToast(t);
  return r;
}

bool extraOpen() { return xScreen != XS_NONE; }
// ko11.8: combate. ko11.8.1: tambien lo de alrededor (elegir region, gimnasios, reto,
// tongsin): el minuto que caia ahi hacia caca y al volver "habia cagado en la batalla"
bool inBattleScreen() {
  return xScreen == XS_WILD || xScreen == XS_LINK || xScreen == XS_REGION || xScreen == XS_GYM ||
         xScreen == XS_DAILY || xScreen == XS_LINKMENU;
}

// texto centrado que se encoge a tamano 1 si no cabe en maxW
void drawFit(const char *s, int y, int maxW, uint16_t col, uint8_t size) {
  gfx->setTextColor(col);
  setSize(size);
  if (textW(s, size) > maxW && size > 1) { size = (size > 2) ? 2 : 1; setSize(size); }
  if (textW(s, size) > maxW && size > 1) { size = 1; setSize(1); }
  setCur(centerX(s, size), y);
  printT(s);
}

// boton redondeado con texto centrado (auto-encoge)
void drawBtn(int x, int y, int w, int h, uint16_t bg, uint16_t fg, const char *s) {
  uiButton(x, y, w, h, 12, bg, UI_INK);
  uint8_t sz = 2;
  setSize(sz);
  if (textW(s, sz) > w - 10) { sz = 1; setSize(1); }
  gfx->setTextColor(fg);
  int tw = textW(s, sz);
  int th = textH(sz);
  setCur(x + (w - tw) / 2, y + (h - 3 - th) / 2);  // ko11.12: centrado en la cara
  printT(s);
}

bool inRect(int16_t x, int16_t y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// ko11.6.1: antes negro + circulo de fondo. Si el volcado DMA del frame anterior
// aun leia el framebuffer, se colaba el negro a medio pintar: la pantalla de red
// "parpadeaba" sin parar. La pantalla es redonda: las esquinas no se ven, asi
// que se pinta todo del color de fondo (la escena principal ya lo hacia asi)
void screenBase() {
  uiScreenBg();
}

// ======================================================================
// Red: WiFi + NTP
// ======================================================================

#define NET_BTN_X 83
#define NET_BTN_W 300
#define NET_BTN_H 44
#define NET_SYNC_Y 214
#define NET_SETUP_Y 266
#define NET_AUTO_Y 318
#define NET_TZ_Y 150

void openNet() { retMark(); clockOpen = false; xScreen = XS_NET; }

void closeNet() {
  if (netPortalOn()) netStopPortal();
  goBack();  // ko11.17: a la hora (desde donde se abre)
}

// la hora local llega del NTP: al RTC y al juego
void applyNetTime(uint32_t e) {
  rtcSetEpoch(e);
  gClockTrusted = true;
  // si el RTC habia perdido la hora al arrancar, ahora SI se sabe cuanto tiempo
  // estuvo apagado: se aplica como progresion offline (igual que con pila)
  if (gRtcWasLost) {
    gRtcWasLost = false;
    pet.syncClock(e);
  } else {
    pet.setClock(e);
  }
}

// ---------------------------------------------------------------- ko8: QR
// QR del WiFi del portal ("WIFI:T:WPA;S:<red>;P:<clave>;;"): los moviles lo
// reconocen con la camara y se conectan solos. Se codifica una vez (la red no
// cambia) con el codificador que trae el core (esp_qrcode / qrcodegen).
#define NET_AP_PASS_UI "tamapoke"   // igual que NET_AP_PASS en net.cpp
#define QR_MAX 41                   // hasta la version 6 (sobra: el texto es de ~40 bytes)
static uint8_t gQrSize = 0;
static uint8_t gQrBits[QR_MAX][(QR_MAX + 7) / 8];

static void qrKeep(esp_qrcode_handle_t q) {
  int n = esp_qrcode_get_size(q);
  if (n <= 0 || n > QR_MAX) return;
  gQrSize = (uint8_t)n;
  memset(gQrBits, 0, sizeof(gQrBits));
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (esp_qrcode_get_module(q, x, y)) gQrBits[y][x >> 3] |= 1 << (x & 7);
}

static void makeWifiQr() {
  if (gQrSize) return;
  char txt[80];
  snprintf(txt, sizeof(txt), "WIFI:T:WPA;S:%s;P:%s;;", netApName(), NET_AP_PASS_UI);
  esp_qrcode_config_t cfg = { qrKeep, 6, ESP_QRCODE_ECC_MED };
  esp_qrcode_generate(&cfg, txt);
}

// cuadro blanco de lado ~box centrado en (cx, cy), con margen de 3 modulos
void drawWifiQr(int cx, int cy, int box) {
  makeWifiQr();
  if (!gQrSize) return;
  int cells = gQrSize + 6;
  int m = box / cells;             // pixeles por modulo (entero: bordes nitidos)
  int side = m * cells;
  int x0 = cx - side / 2, y0 = cy - side / 2;
  uiButton(x0 - 4, y0 - 4, side + 8, side + 8, 10, UI_WHITE, UI_INK);
  int ox = x0 + 3 * m, oy = y0 + 3 * m;
  for (int y = 0; y < gQrSize; y++)
    for (int x = 0; x < gQrSize; x++)
      if (gQrBits[y][x >> 3] & (1 << (x & 7))) gfx->fillRect(ox + x * m, oy + y * m, m, m, RGB565_BLACK);
}

// fila "etiqueta   VALOR" en una caja blanca (valor grande)
void portalRow(int y, const char *label, const char *value, uint16_t col) {
  uiButton(78, y, 310, 34, 10, UI_WHITE, UI_TRACK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(92, y + 9);
  printT(label);
  uint8_t sz = 3;
  int lw = textW(label, 2) + 24;
  if (textW(value, sz) > 296 - lw) sz = 2;
  setSize(sz);
  gfx->setTextColor(col);
  int vh = textH(sz);
  setCur(376 - textW(value, sz), y + (34 - vh) / 2);
  printT(value);
}

void renderNet() {
  screenBase();
  gfx->setTextColor(UI_INK);
  if (netPortalOn()) {
    // ko8: QR grande (la camara del movil se une al WiFi sin teclear) y los
    // datos en letra grande por si el movil no lee QR
    drawFit(XT(X_QR_HINT), 28, 230, UI_INK, 2);
    drawWifiQr(CX, 164, 212);
    portalRow(284, XT(X_QR_WIFI), netApName(), UI_BAR_BAD);
    portalRow(324, XT(X_QR_PASS), NET_AP_PASS_UI, UI_INK);
    portalRow(364, XT(X_QR_ADDR), "192.168.4.1", UI_INK);
    drawBtn(158, 408, 150, 34, UI_TRACK, UI_INK, T(S_BACK));
    uiFlush();
    return;
  }

  drawFit(XT(X_NET_TITLE), 36, 300, UI_INK, 3);
  char l[64];
  if (netConfigured()) {
    int k = snprintf(l, sizeof(l), "WiFi: %s", netSsid());
    if (netSavedCount() > 1 && k > 0 && k < (int)sizeof(l))  // ko8: varias guardadas
      snprintf(l + k, sizeof(l) - k, XT(X_SAVED_MORE_FMT), (unsigned)(netSavedCount() - 1));
  } else {
    strncpy(l, XT(netOpenAllowed() ? X_OPEN_ON : X_NOT_SET), sizeof(l));
  }
  l[sizeof(l) - 1] = 0;
  drawFit(l, 80, 340, UI_INK, 2);

  // estado o ultima sincronizacion
  const char *st = nullptr;
  uint16_t sc = UI_INK;
  switch (netState()) {
    case NET_SCAN:       st = XT(X_ST_SCAN);       sc = UI_BAR_WARN; break;
    case NET_CONNECTING: st = XT(X_ST_CONNECTING); sc = UI_BAR_WARN; break;
    case NET_NTP:        st = XT(X_ST_NTP);        sc = UI_BAR_WARN; break;
    case NET_OK:         st = XT(X_ST_OK);         sc = UI_BAR_OK;   break;
    case NET_FAIL_WIFI:  st = XT(X_ST_FAIL_WIFI);  sc = UI_BAR_BAD;  break;
    case NET_FAIL_NTP:   st = XT(X_ST_FAIL_NTP);   sc = UI_BAR_BAD;  break;
    default: break;
  }
  char ls[40];
  if (!st) {
    uint32_t e = netLastSync();
    if (e) {
      // dia/mes a partir de dias desde 1970 (civil_from_days, sin libc)
      int32_t z = (int32_t)(e / 86400) + 719468;
      int32_t era = z / 146097, doe = z - era * 146097;
      int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
      int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
      unsigned d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
      snprintf(ls, sizeof(ls), XT(X_LAST_SYNC_FMT), m, d, (unsigned)((e / 3600) % 24),
               (unsigned)((e / 60) % 60));
      st = ls;
    } else {
      st = XT(X_NEVER);
      sc = UI_TRACK;
    }
  }
  drawFit(st, 108, 340, sc, 2);

  // zona horaria
  int tz = netTzMin();
  char tzs[32];
  snprintf(tzs, sizeof(tzs), "%s UTC%c%d:%02d", XT(X_TIMEZONE), tz < 0 ? '-' : '+', abs(tz) / 60,
           abs(tz) % 60);
  drawBtn(83, NET_TZ_Y, 48, 44, UI_WHITE, UI_INK, "-");
  drawBtn(335, NET_TZ_Y, 48, 44, UI_WHITE, UI_INK, "+");
  drawFit(tzs, NET_TZ_Y + 14, 190, UI_INK, 2);

  bool busy = netBusy() || linkActive();
  bool canSync = netConfigured() || netOpenAllowed();
  drawBtn(NET_BTN_X, NET_SYNC_Y, NET_BTN_W, NET_BTN_H, (canSync && !busy) ? UI_BAR_OK : UI_TRACK,
          UI_WHITE, XT(X_SYNC_NOW));
  drawBtn(NET_BTN_X, NET_SETUP_Y, NET_BTN_W, NET_BTN_H, 0x4C98, UI_WHITE, XT(X_SETUP_WIFI));
  // ko8: la fila se parte en dos: sincronizacion automatica | WiFi abiertas
  int hw = (NET_BTN_W - 8) / 2;
  drawBtn(NET_BTN_X, NET_AUTO_Y, hw, NET_BTN_H, netAuto() ? UI_WHITE : UI_TRACK, UI_INK,
          XT(netAuto() ? X_AUTO_S_ON : X_AUTO_S_OFF));
  drawBtn(NET_BTN_X + hw + 8, NET_AUTO_Y, hw, NET_BTN_H, netOpenAllowed() ? UI_WHITE : UI_TRACK, UI_INK,
          XT(netOpenAllowed() ? X_OPEN_ON : X_OPEN_OFF));
  // ko11.26: [SD update] y [copia] pasaron al menu de ajustes
  drawBackArrow();  // ko11.6.1: flecha izquierda = volver (antes "tocar arriba", con el aviso abajo junto a [SD])
  uiFlush();
}

void netTap(int16_t x, int16_t y) {
  if (netPortalOn()) {
    if (y >= 400) netStopPortal();  // ko8: [volver] bajo el QR
    return;
  }
  if (navHit(NAV_L, x, y)) { closeNet(); sfxPlay(SFX_TAP); return; }  // ko11.6.1: flecha = volver
  if (y >= NET_TZ_Y && y < NET_TZ_Y + 44) {
    if (x < 140) netSetTzMin(netTzMin() - 30);
    else if (x > 326) netSetTzMin(netTzMin() + 30);
    sfxPlay(SFX_TAP);
    return;
  }
  if (!inRect(x, y, NET_BTN_X, NET_SYNC_Y, NET_BTN_W, NET_AUTO_Y + NET_BTN_H - NET_SYNC_Y)) return;
  if (y < NET_SYNC_Y + NET_BTN_H) {
    if ((netConfigured() || netOpenAllowed()) && !netBusy() && !linkActive()) { netSyncNow(); sfxPlay(SFX_TAP); }
    else sfxPlay(SFX_DENY);
  } else if (y >= NET_SETUP_Y && y < NET_SETUP_Y + NET_BTN_H) {
    if (linkActive()) { sfxPlay(SFX_DENY); return; }
    netStartPortal();
    sfxPlay(SFX_TAP);
  } else if (y >= NET_AUTO_Y) {
    if (x < NET_BTN_X + NET_BTN_W / 2) netSetAuto(!netAuto());
    else netSetOpenAllowed(!netOpenAllowed());
    sfxPlay(SFX_TAP);
  }
}

// ======================================================================
// Batalla: vista comun (yasaeng y tongsin)
// ======================================================================

#define BQ_MAX 160
BEvent bq[BQ_MAX];     // cola de eventos a reproducir
int bqN = 0, bqI = 0;
uint32_t bqT = 0;      // inicio del evento en curso
bool bqAisMe = true;   // el lado "a" de los eventos es mi Pokemon

// lo que se ve en pantalla
int16_t bvMeDex, bvFoeDex;
uint8_t bvMeType, bvFoeType;
uint8_t bvMeTier = 0, bvFoeTier = 0;  // ko10.4: fase del ataque de tipo (moveTier)
uint16_t bvMeLvl, bvFoeLvl, bvMeMax, bvFoeMax;
float bvMeHp, bvFoeHp;        // animado
uint16_t bvMeTgt, bvFoeTgt;   // objetivo
char bvMeName[32], bvFoeName[40];
bool bvFoeShiny = false;
char bvL1[80] = "", bvL2[64] = "";
bool bvMeFainted = false, bvFoeFainted = false;  // ya se reprodujo su desmayo
bool bvFoeCaught = false;  // fork KO (ko4): el rival ya esta dentro de la pokeball
bool bCaught = false;      // la batalla acabo en captura
int16_t bBoxMsg = -1;      // XId del aviso de la caja en el resultado (-1 = nada)
#define BOX_JOIN_PCT 20    // ko5: % de salvajes vencidos que se unen a la caja
PmdMon foePmd;
// ko11.16: el orbe equipado sube ataque o defensa (solo si es de su tipo)
void applyOrb(Battler &b) {
  uint8_t a = pet.orbAtkPct(), d = pet.orbDefPct();
  if (a) b.atk = (uint16_t)((uint32_t)b.atk * (100 + a) / 100);
  if (d) b.def = (uint16_t)((uint32_t)b.def * (100 + d) / 100);
}

// ko11.16: sprites de combate al estilo de los juegos (rNNN.bin de la SD): el rival de
// frente y el tuyo de espaldas. Si no estan, se usan los PMD de siempre
PmdMon prgFoe, prgMe;
// ko11.16: estilo de los sprites de combate (se guarda): 0 = PMD (los de siempre),
// 1 = PokeRogue (rNNN.bin de la SD; si falta el de un Pokemon, ese sale en PMD).
// Se cambia con la pildora de arriba del combate. Fuera del combate, siempre PMD
uint8_t gBattleArt = 255;
static int16_t prgFoeDex = 0;
static bool prgFoeShiny = false;
uint8_t battleArt() {
  if (gBattleArt == 255) {
    Preferences p;
    p.begin("tamapoke", true);
    gBattleArt = p.getUChar("bart", 0) ? 1 : 0;
    p.end();
  }
  return gBattleArt;
}
static void prgLoadFor(int16_t meDex, int16_t foeDex, bool foeShiny) {
  prgFoeDex = foeDex;
  prgFoeShiny = foeShiny;
  if (!battleArt()) { prgFoe.unload(); prgMe.unload(); return; }  // PMD: ni se leen (menos SD y RAM)
  prgFoe.load((uint8_t)foeDex, foeShiny, 'r');  // ~100 KB de la SD, una vez por combate
  if (!prgMe.loaded || bvMeDex != meDex) prgMe.load((uint8_t)meDex, pet.shiny && meDex == pet.speciesId, 'r');
}
#define BART_X 163
#define BART_Y 16
#define BART_W 140
#define BART_H 30
static void drawBattleArtChip() {
  bool prg = battleArt();
  drawBtn(BART_X, BART_Y, BART_W, BART_H, prg ? C565(0x6a, 0x4c, 0xf0) : UI_WHITE, prg ? UI_WHITE : UI_INK,
          XT(prg ? X_BART_PRG : X_BART_PMD));
}
static void battleArtToggle() {
  gBattleArt = battleArt() ? 0 : 1;
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("bart", gBattleArt);
  p.end();
  sfxPlay(SFX_TAP);
}
static bool battleArtTap(int16_t x, int16_t y) {
  // ko11.19: zona de toque mucho mas grande (arriba del todo el tactil es menos preciso
  // y la pildora es pequena): toda la franja de arriba, 40 px mas a cada lado
  if (!inRect(x, y, BART_X - 40, 0, BART_W + 80, BART_Y + BART_H + 22)) return false;
  battleArtToggle();
  prgLoadFor(bvMeDex, prgFoeDex, prgFoeShiny);  // se ve al momento
  return true;
}
int16_t foePmdDex = 0;       // que especie tiene cargada foePmd
bool foePmdShiny = false;

void loadFoe(int16_t dex, bool shiny) {
  if (foePmd.loaded && foePmdDex == dex && foePmdShiny == shiny) return;
  foePmd.unload();
  foePmdDex = dex;
  foePmdShiny = shiny;
  if (dex >= 1 && dex <= DEX_COUNT) foePmd.load(dex, shiny);
}

// fase de la batalla
// BP_NEXT (ko9.2, solo salvajes): tras el resultado, "seguir buscando o salir"
// BP_DUP (ko10.4): repetido capturado -> quedarselo (caja) o cambiarlo por caramelos
enum : uint8_t { BP_INTRO = 0, BP_MENU, BP_PLAY, BP_RESULT, BP_NEXT, BP_DUP,
                 BP_JOIN,   // ko11.8: el vencido quiere venir -> preguntar
                 BP_SWAP };  // ko11.8: el vencido quiere venir -> preguntar
#define BD_MS 20000UL  // sin elegir en 20 s: se lo queda (no se pierde nada)
#define BDUP_KEEP_X 70  // botones de BP_DUP
#define BDUP_CANDY_X 240
#define BDUP_W 156
bool bDupPending = false, bDupCaught = false;
bool bJoinPending = false;  // ko11.8
uint32_t bDupEpoch = 0;
char bNote[80] = "";  // ko10.11: 48 -> 80 (textos de revancha y liga)   // linea extra bajo "seguir?" (caramelos ganados)
#define BN_Y 330      // botones de BP_NEXT
#define BN_H 50
#define BN_MS 15000UL // sin elegir en 15 s, vuelve a la pantalla principal
uint8_t bPhase = BP_INTRO;
int bvShakeX = 0, bvShakeY = 0;  // fork KO (ko7): temblor de la escena (critico)
uint32_t bPhaseT = 0;
bool bWon = false, bFled = false, bLink = false, bRewarded = false;
uint8_t bItems = 0;  // ko11.1: objetos ganados (1=bola 2=pocion)
// ko10.1: region del salvaje (= escenario 0..15). La de mi Pokemon por defecto
uint8_t bRegion = 0;
uint8_t bGroup = WG_COMMON;  // de que grupo salio el rival (WG_RARE: brillo al aparecer)
// ko10.4: combates contra entrenador (gimnasio / reto del dia): varios rivales seguidos
enum : uint8_t { BK_WILD = 0, BK_GYM, BK_DAILY, BK_CHAMP, BK_STORY, BK_ROGUE };  // ko11.21: historia y expedicion  // ko10.11: liga
uint8_t bKind = BK_WILD;
// ko11.22: en la historia, como en Rojo/Verde, a nivel bajo el rival aun no sabe su ataque de tipo
// (Placaje/Aranazo hasta ~nivel 8). Sin esto, el 1er combate (rival con ventaja de tipo, x2) era casi imposible
#define STORY_TYPED_LV 8
extern Battler bFoe;
static BAct foeMoveRule(BAct a) {
  return (a == BA_TYPE && bKind == BK_STORY && bFoe.lvl < STORY_TYPED_LV) ? BA_TACKLE : a;
}
uint8_t bvOwned = 0;      // ko10.11: de esta especie en la caja
int16_t bExpDex = 0;      // ko11: la EXP del salvaje sale de su especie y nivel ANTES de ajustarlo
uint16_t bExpLvl = 0;
uint32_t bvOwnedT = 0;
uint8_t bGym = 0;
Battler bTeam[CHAMP_TEAM];  // ko10.11: la liga lleva 6 (antes 3)
uint8_t bTeamN = 0, bTeamI = 0;
extern uint8_t stBattleWho;  // ko11.21: ui_story.ino
// ko11.20: mi equipo contra entrenadores: [0] el que crias + hasta 2 ayudantes de la caja
Battler pMon[PARTY_MAX];
int8_t pBox[PARTY_MAX] = { -1, -1, -1 };  // indice en la caja (-1 = el que crias)
uint8_t pN = 1, pCur = 0, pUsed = 1;      // pUsed: bit i = ya salio a luchar
uint8_t pShiny = 0;                       // bit i = variocolor
uint32_t bQuitArm = 0;                    // ko11.23.1: [◀] armado (salir de un combate de la historia)
void storyBattleQuit(uint8_t kind);
bool pSlot0Pet = true;                    // ko11.21: el primero es el que crias (no en historia/expedicion)
Battler storyPartnerBattler();
void storyAddParty();
uint8_t bSwapMode = 0;  // BP_SWAP: 0 obligado (cayo el mio), 1 rival nuevo (puede quedarse), 2 manual (gasta turno)
PmdMon helperPmd;
int16_t helperPmdDex = 0;
bool helperPmdShiny = false;
// eleccion antes del combate (pantalla XS_PARTY, ui_more.ino)
uint8_t ppKind = 0, ppRegion = 0, ppN = 0, ppFrom = 0, ppPage = 0;
Battler ppTeam[CHAMP_TEAM];
int8_t ppPick[PARTY_HELPERS] = { -1, -1 };        // indices en la caja (-1 = nadie)
int16_t ppPickDex[PARTY_HELPERS] = { 0, 0 };      // para reconocerlos la proxima vez
uint32_t ppPickEpoch[PARTY_HELPERS] = { 0, 0 };
bool ppArmed = false;                             // startTrainer usa ppPick
char bPartyNote[40] = "";                         // "도우미 Lv+1" en el resultado

// ko11.20: cada ayudante, HELPER_USES_PER_DAY combates al dia (espacio "tpparty":
// el dia y {especie, llegada a la caja, veces}; se reconoce por especie + fecha)
struct __attribute__((packed)) HelperUse { int16_t dex; uint32_t epoch; uint8_t n; };
static HelperUse gHU[16];
static uint8_t gHUN = 0;
static uint16_t gHUDay = 0xFFFF;
static bool gHULoaded = false;
static void huLoad() {
  uint16_t today = (uint16_t)(pet.lastSeenEpoch / 86400u);
  if (!gHULoaded) {
    Preferences p;
    p.begin("tpparty", true);
    gHUDay = p.getUShort("day", 0xFFFF);
    gHUN = p.isKey("use") ? (uint8_t)(p.getBytes("use", gHU, sizeof(gHU)) / sizeof(HelperUse)) : 0;
    p.end();
    gHULoaded = true;
  }
  if (gHUDay != today) { gHUDay = today; gHUN = 0; }
}
static void huSave() {
  Preferences p;
  p.begin("tpparty", false);
  p.putUShort("day", gHUDay);
  if (gHUN) p.putBytes("use", gHU, gHUN * sizeof(HelperUse));
  else p.remove("use");
  p.end();
}
uint8_t helperUsesLeft(const BoxMon &m) {
  huLoad();
  for (uint8_t i = 0; i < gHUN; i++)
    if (gHU[i].dex == m.dex && gHU[i].epoch == m.epoch)
      return gHU[i].n >= HELPER_USES_PER_DAY ? 0 : (uint8_t)(HELPER_USES_PER_DAY - gHU[i].n);
  return HELPER_USES_PER_DAY;
}
static void helperUse(const BoxMon &m) {
  huLoad();
  uint8_t i = 0;
  while (i < gHUN && !(gHU[i].dex == m.dex && gHU[i].epoch == m.epoch)) i++;
  if (i == gHUN) {
    if (gHUN == 16) { memmove(gHU, gHU + 1, sizeof(HelperUse) * 15); gHUN--; i = gHUN; }
    gHU[i].dex = m.dex; gHU[i].epoch = m.epoch; gHU[i].n = 0;
    gHUN++;
  }
  if (gHU[i].n < 255) gHU[i].n++;
  huSave();
}

// ko11.20: salon de la liga, una ficha por Pokemon (espacio "tpteam" en la NVS
// grande, clave "r"): victorias en solitario / en equipo y los ultimos ayudantes.
// Corona de oro = alguna victoria en solitario; de plata = solo en equipo (BOXF_TEAM).
#define FAME_REC_MAX 60
static FameRec gFR[FAME_REC_MAX];
static uint8_t gFRN = 0;
static bool gFRLoaded = false;
static bool sameFameKey(const FameRec &r, const BoxMon &m) {
  return r.epoch == m.epoch && r.fam == DEX_FAM[m.dex] && r.gA == m.geneAtk && r.gD == m.geneDef && r.gS == m.geneSpe;
}
static void frSave() {
  Preferences p;
  p.begin("tpteam", false, bigPart());
  if (gFRN) p.putBytes("r", gFR, gFRN * sizeof(FameRec));
  else p.remove("r");
  p.putUChar("mg", 1);
  p.end();
}
static int frFind(const BoxMon &m) {
  for (int i = gFRN - 1; i >= 0; i--) if (sameFameKey(gFR[i], m)) return i;
  return -1;
}
// las fichas de antes (una por victoria): 1 en solitario, o 1 en equipo si tiene la marca
static FameRec frDefault(const BoxMon &m) {
  FameRec r = {};
  r.epoch = m.epoch; r.fam = DEX_FAM[m.dex]; r.gA = m.geneAtk; r.gD = m.geneDef; r.gS = m.geneSpe;
  bool team = m.flags & BOXF_TEAM;
  r.solo = team ? 0 : 1;
  r.team = team ? 1 : 0;
  return r;
}
static FameRec *frGetOrAdd(const BoxMon &m) {
  int i = frFind(m);
  if (i >= 0) return &gFR[i];
  if (gFRN == FAME_REC_MAX) { memmove(gFR, gFR + 1, sizeof(FameRec) * (FAME_REC_MAX - 1)); gFRN--; }
  gFR[gFRN] = frDefault(m);
  return &gFR[gFRN++];
}
static bool sameIndividual(const BoxMon &a, const BoxMon &b) {
  return a.geneAtk && DEX_FAM[a.dex] == DEX_FAM[b.dex] && a.geneAtk == b.geneAtk && a.geneDef == b.geneDef &&
         a.geneSpe == b.geneSpe;
}
// una vez: las fichas repetidas del mismo Pokemon (antes, una por victoria) se juntan
static void fameMergeOld() {
  bool changed = false;
  for (int i = fame.count() - 1; i > 0; i--) {
    const BoxMon mi = fame.at((uint8_t)i);
    for (int j = 0; j < i; j++) {
      BoxMon mj = fame.at((uint8_t)j);
      if (!sameIndividual(mi, mj)) continue;
      FameRec ri = frFind(mi) >= 0 ? gFR[frFind(mi)] : frDefault(mi);
      FameRec *rj = frGetOrAdd(mj);
      rj->solo += ri.solo;
      rj->team += ri.team;
      if (ri.team && ri.help[0]) { rj->help[0] = ri.help[0]; rj->help[1] = ri.help[1]; rj->shiny = ri.shiny; }
      mj.dex = mi.dex;  // su forma y nivel mas recientes
      mj.lvl = mi.lvl;
      mj.flags = (uint8_t)((mj.flags | (mi.flags & BOXF_SHINY)) & ~BOXF_TEAM);
      if (!rj->solo) mj.flags |= BOXF_TEAM;
      fame.set((uint8_t)j, mj);
      if (i < (int)sizeof(pet.fameStreak)) {
        if (pet.fameStreak[i] > pet.fameStreak[j]) pet.fameStreak[j] = pet.fameStreak[i];
        memmove(pet.fameStreak + i, pet.fameStreak + i + 1, sizeof(pet.fameStreak) - i - 1);
        pet.fameStreak[sizeof(pet.fameStreak) - 1] = 0;
      }
      int k = frFind(mi);
      if (k >= 0) { memmove(gFR + k, gFR + k + 1, sizeof(FameRec) * (gFRN - k - 1)); gFRN--; }
      fame.release((uint8_t)i);
      changed = true;
      break;
    }
  }
  frSave();
  if (changed) pet.saveNow();
}
static void frLoad() {
  if (gFRLoaded) return;
  gFRLoaded = true;
  Preferences p;
  p.begin("tpteam", true, bigPart());
  gFRN = p.isKey("r") ? (uint8_t)(p.getBytes("r", gFR, sizeof(gFR)) / sizeof(FameRec)) : 0;
  bool merged = p.getUChar("mg", 0);
  p.end();
  if (!merged) fameMergeOld();
}
FameRec fameRecOf(const BoxMon &m) {
  frLoad();
  int i = frFind(m);
  return i >= 0 ? gFR[i] : frDefault(m);
}
int fameCardOfPet() {
  frLoad();
  BoxMon me = {};
  me.dex = pet.speciesId; me.geneAtk = pet.geneAtk; me.geneDef = pet.geneDef; me.geneSpe = pet.geneSpe;
  for (int i = fame.count() - 1; i >= 0; i--) if (sameIndividual(fame.at((uint8_t)i), me)) return i;
  return -1;
}

// salvaje
Battler bMe, bFoe;
BRng bRng(1);

uint32_t evDur(const BEvent &e) {
  switch (e.kind) {
    case EV_HIT: return (e.eff != 2 || e.crit) ? 1900 : 1400;
    case EV_COUNTER: return 1500;  // ko11.8
    case EV_FAINT: return 1500;
    default: return 1200;
  }
}

bool evIsMe(uint8_t side) { return (side == 0) == bqAisMe; }

void evMessages(const BEvent &e) {
  bool me = evIsMe(e.side);
  const char *who = me ? bvMeName : bvFoeName;
  bvL2[0] = 0;
  switch (e.kind) {
    case EV_HIT:
    case EV_MISS:
      txFmt(bvL1, sizeof(bvL1), X_USED, who, moveName(e.move, me ? bvMeType : bvFoeType, me ? bvMeTier : bvFoeTier));
      if (e.kind == EV_MISS) strncpy(bvL2, XT(X_MISSED), sizeof(bvL2) - 1);
      else if (e.eff == 0) strncpy(bvL2, XT(X_NOEFFECT), sizeof(bvL2) - 1);
      else if (e.eff == 4) strncpy(bvL2, XT(X_SUPER), sizeof(bvL2) - 1);
      else if (e.eff == 1) strncpy(bvL2, XT(X_NOTVERY), sizeof(bvL2) - 1);
      else if (e.crit) strncpy(bvL2, XT(X_CRIT), sizeof(bvL2) - 1);
      break;
    case EV_GUARD: txFmt(bvL1, sizeof(bvL1), X_GUARDS, who); break;
    case EV_COUNTER:  // ko11.8: se protegio y devuelve el golpe
      txFmt(bvL1, sizeof(bvL1), X_COUNTER, who);
      if (e.eff == 4) strncpy(bvL2, XT(X_SUPER), sizeof(bvL2) - 1);
      else if (e.eff == 1) strncpy(bvL2, XT(X_NOTVERY), sizeof(bvL2) - 1);
      break;
    case EV_RUN_OK: strncpy(bvL1, XT(X_FLED), sizeof(bvL1) - 1); break;
    case EV_RUN_FAIL: strncpy(bvL1, XT(X_CANT_RUN), sizeof(bvL1) - 1); break;
    case EV_FAINT: txFmt(bvL1, sizeof(bvL1), X_FAINTED, who); break;
    case EV_HEAL: txFmt(bvL1, sizeof(bvL1), X_HEALED, who); break;
    case EV_CATCH:
      strncpy(bvL1, XT(X_THREW), sizeof(bvL1) - 1);
      txFmt(bvL2, sizeof(bvL2), X_CAUGHT, bvFoeName);
      break;
    case EV_BREAK:
      strncpy(bvL1, XT(X_THREW), sizeof(bvL1) - 1);
      txFmt(bvL2, sizeof(bvL2), X_BROKE, bvFoeName);
      break;
  }
  bvL1[sizeof(bvL1) - 1] = 0;
  bvL2[sizeof(bvL2) - 1] = 0;
  if ((e.kind == EV_HIT || e.kind == EV_COUNTER) && e.dmg) {
    sfxPlay(SFX_PLAY);
    vibPulse((e.crit || e.eff > 2) ? 140 : 70, 1, 0);  // ko11.25: golpe (critico / muy eficaz, mas largo)
  }
  else if (e.kind == EV_FAINT) sfxPlay(SFX_DENY);
  else if (e.kind == EV_HEAL) sfxPlay(SFX_HEART);
  else if (e.kind == EV_CATCH) sfxPlay(SFX_MEDAL);
  else if (e.kind == EV_BREAK) sfxPlay(SFX_DENY);
}

void startEvent(int i) {
  if (i > 0 && i - 1 < bqN && bq[i - 1].kind == EV_FAINT) {
    if (evIsMe(bq[i - 1].side)) bvMeFainted = true;
    else bvFoeFainted = true;
  }
  if (i > 0 && i - 1 < bqN && bq[i - 1].kind == EV_CATCH) bvFoeCaught = true;
  bqI = i;
  bqT = millis();
  if (i < bqN) {
    const BEvent &e = bq[i];
    bvMeTgt = bqAisMe ? e.hpA : e.hpB;
    bvFoeTgt = bqAisMe ? e.hpB : e.hpA;
    evMessages(e);
  }
}

// cielo segun la hora y el tiempo + escenario + dos plataformas.
// ko10.1: en salvaje, el escenario es la region elegida; en tongsin, el de mi Pokemon
void drawBattleBg() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint8_t wx = sceneWeather();
  uint8_t bio = bLink ? petRegion() : bRegion;
  uint32_t now = millis();
  int hor = 150;
  drawSky(hor, hh, night, wx, now, false);
  drawBiome(bio, hor, 262, now, night, wx);
  uint16_t soil = nightDim(BIOME_SOIL[bio < BIOME_N ? bio : 0], night);
  uint16_t pad = lerp565(soil, C565(0x10, 0x18, 0x20), 4, 16);
  gfx->fillEllipse(316 + bvShakeX, 160 + bvShakeY, 78, 16, pad);   // plataforma del rival
  gfx->fillEllipse(140 + bvShakeX, 256 + bvShakeY, 92, 18, pad);   // la mia
  drawWeather(wx, 262, now, night);
  gfx->fillRect(0, 262, 466, 204, UI_BG_DAY);  // panel inferior (mensajes/menu)
}

// ko11.9.1: los de la 2a generacion (152-251) con el nombre en dorado oscuro
#define UI_GEN2_INK C565(0x98, 0x68, 0x00)
static uint16_t nameInkFor(int16_t dex) { return dex > 151 ? UI_GEN2_INK : UI_INK; }

void drawHpBox(int x, int y, int w, const char *name, uint16_t lvl, float hp, uint16_t maxHp, bool showNum,
               uint16_t nameInk) {
  uiPanel(x, y, w, showNum ? 58 : 46, 10, UI_WHITE, UI_INK);  // ko11.12
  char lv[12];
  snprintf(lv, sizeof(lv), "Lv%u", lvl);
  setSize(1);
  int lw = textW(lv, 1);
  gfx->setTextColor(UI_INK);
  setCur(x + w - lw - 8, y + 7);
  printT(lv);
  // nombre (encoge si hace falta)
  uint8_t sz = 2;
  setSize(sz);
  if (textW(name, sz) > w - lw - 22) { sz = 1; setSize(1); }
  gfx->setTextColor(nameInk);
  setCur(x + 8, y + 5);
  printT(name);
  gfx->setTextColor(UI_INK);
  int bx = x + 8, by = y + 28, bw = w - 16;
  float f = maxHp ? hp / maxHp : 0;
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  uint16_t col = f > 0.5f ? UI_BAR_OK : f > 0.2f ? UI_BAR_WARN : UI_BAR_BAD;
  uiGauge(bx, by, bw, 11, (int)(f * 1000), col, UI_TRACK);  // ko11.12
  if (showNum) {
    char hs[16];
    snprintf(hs, sizeof(hs), "%u/%u", (unsigned)(hp + 0.5f), maxHp);
    setSize(1);
    setCur(x + w - textW(hs, 1) - 8, y + 43);
    printT(hs);
  }
}

// ======================================================================
// fork KO (ko7): efectos de los ataques, uno por tipo. Solo formas simples
// (circulos, lineas, triangulos) sobre el fondo, sincronizados con la
// embestida: t 120..380 el proyectil viaja, desde t 350 el impacto en el
// rival (cuando parpadea). Critico = temblor; muy eficaz = onda de choque.
// ======================================================================

static uint32_t fxHash(uint32_t a) {
  a ^= a >> 16; a *= 0x7feb352dU; a ^= a >> 15; a *= 0x846ca68bU; a ^= a >> 16;
  return a;
}
static int fxRnd(uint32_t seed, int span) { return (int)(fxHash(seed) % (uint32_t)(2 * span + 1)) - span; }

// linea gruesa (w px) de a a b
static void fxLine(int x0, int y0, int x1, int y1, int w, uint16_t c) {
  int dx = x1 - x0, dy = y1 - y0;
  bool steep = abs(dy) > abs(dx);
  for (int k = -(w / 2); k <= w / 2; k++) {
    if (steep) gfx->drawLine(x0 + k, y0, x1 + k, y1, c);
    else gfx->drawLine(x0, y0 + k, x1, y1 + k, c);
  }
}

// estrella de n rayos (impactos, cristales, chispas)
static void fxStar(int x, int y, int r0, int r1, int rays, float rot, int w, uint16_t c) {
  for (int i = 0; i < rays; i++) {
    float a = rot + i * 6.2832f / rays;
    float ca = cosf(a), sa = sinf(a);
    fxLine(x + (int)(ca * r0), y + (int)(sa * r0), x + (int)(ca * r1), y + (int)(sa * r1), w, c);
  }
}

static float fxClamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// fx: PT_* del ataque (0xFF = placaje). (ax,ay) atacante, (tx,ty) objetivo.
// ko10.4: color de cada tipo para el aura y el estallido de las fases media y final
static const uint16_t FX_COL[PT_COUNT] = {
  C565(0xd8, 0xd0, 0xb0), C565(0xff, 0x7a, 0x1a), C565(0x40, 0x90, 0xf0), C565(0x3c, 0xb8, 0x48),
  C565(0xff, 0xd8, 0x20), C565(0x80, 0xe0, 0xf8), C565(0xe0, 0x60, 0x30), C565(0xa0, 0x48, 0xc0),
  C565(0xc0, 0x98, 0x50), C565(0xf0, 0x58, 0xa8), C565(0xa8, 0xc8, 0x30), C565(0xa8, 0x90, 0x60),
  C565(0x70, 0x58, 0xa8), C565(0x70, 0x60, 0xf0), C565(0x60, 0x48, 0x40), C565(0xb0, 0xb8, 0xc8),
};
// ataques finales que salen como rayo (Hiperrayo, Hidrobomba, Rayo Solar, Ventisca,
// Psiquico, Enfado...); el resto usa su efecto propio, mas grande
static const bool FX_BEAM[PT_COUNT] = {
  true, true, true, true, false, true, false, true, false, true, false, false, true, true, false, true,
};

void drawMoveFx(uint8_t fx, int ax, int ay, int tx, int ty, uint32_t t, bool hit, uint8_t eff, uint8_t tier) {
  const uint16_t WHITE = UI_WHITE, YEL = C565(0xff, 0xe0, 0x40), ORA = C565(0xff, 0x8c, 0x1a),
                 RED = C565(0xe8, 0x38, 0x20);
  float p = fxClamp01(((float)t - 120) / 260.0f);  // viaje del proyectil
  bool travel = t >= 120 && t < 380;
  int k = (int)t - 350;                              // tiempo de impacto
  bool impact = hit && k >= 0 && k < 600;
  float f = impact ? k / 600.0f : 0;
  int px = ax + (int)((tx - ax) * p), py = ay + (int)((ty - ay) * p);
  uint32_t sd = (uint32_t)bqI * 7919u + (uint32_t)bqT;  // variacion por golpe

  switch (fx) {
    case PT_FIRE:
      if (travel)
        for (int i = 0; i < 3; i++) {
          float q = fxClamp01(p - i * 0.14f);
          int x = ax + (int)((tx - ax) * q), y = ay + (int)((ty - ay) * q);
          gfx->fillCircle(x, y, 8 - i * 2, ORA);
          gfx->fillCircle(x, y, 4 - i, YEL);
        }
      if (impact)
        for (int i = 0; i < 8; i++) {
          float ph = ((k + i * 75) % 420) / 420.0f;
          int x = tx + fxRnd(sd + i, 32), y = ty + 26 - (int)(ph * 80);
          int r = 3 + (int)(11 * (1 - ph) * (1 - f * 0.6f));
          gfx->fillCircle(x, y, r, ph < 0.35f ? YEL : ph < 0.7f ? ORA : RED);
        }
      break;

    case PT_WATER: {
      const uint16_t BLU = C565(0x40, 0x90, 0xf0), LBL = C565(0xb0, 0xe0, 0xff);
      if (travel)
        for (int i = 0; i < 4; i++) {
          float q = fxClamp01(p - i * 0.1f);
          int x = ax + (int)((tx - ax) * q), y = ay + (int)((ty - ay) * q) - (int)(24 * sinf(q * 3.1416f));
          gfx->fillCircle(x, y, 6, BLU);
          gfx->fillCircle(x - 2, y - 2, 2, LBL);
        }
      if (impact) {
        gfx->drawCircle(tx, ty, 12 + k / 8, LBL);
        gfx->drawCircle(tx, ty, 13 + k / 8, LBL);
        for (int i = 0; i < 9; i++) {
          float a = i * 0.698f + 0.3f;
          int d = k / 6;
          int x = tx + (int)(cosf(a) * d), y = ty - 10 + (int)(sinf(a) * d * 0.7f) + d * d / 90;
          if (k < 480) gfx->fillCircle(x, y, 5 - k / 160, BLU);
        }
      }
      break;
    }

    case PT_GRASS: {
      const uint16_t GRN = C565(0x3c, 0xa8, 0x48), LGR = C565(0x9c, 0xe0, 0x6c);
      // latigo: la liana crece hasta el rival y vuelve
      if (t >= 100 && t < 620) {
        float L = t < 380 ? fxClamp01((t - 100) / 280.0f) : fxClamp01((620 - (float)t) / 240.0f);
        int lx = ax, ly = ay;
        for (int s2 = 1; s2 <= 12; s2++) {
          float q = L * s2 / 12.0f;
          int x = ax + (int)((tx - ax) * q), y = ay + (int)((ty - ay) * q) + (int)(10 * sinf(q * 12 + t * 0.02f));
          fxLine(lx, ly, x, y, 4, GRN);
          lx = x; ly = y;
        }
      }
      if (impact)
        for (int i = 0; i < 7; i++) {
          float a = i * 0.9f + f * 4;
          int d = 10 + k / 7;
          int x = tx + (int)(cosf(a) * d), y = ty + (int)(sinf(a) * d * 0.8f) + k / 20;
          if (k < 520) gfx->fillEllipse(x, y, 6, 3, (i & 1) ? GRN : LGR);
        }
      break;
    }

    case PT_ELECTRIC:
      if (impact && k < 520 && ((k / 45) % 3) != 2) {
        for (int b = 0; b < 3; b++) {
          int x = tx + (b - 1) * 26 + fxRnd(sd + b * 31 + k / 45, 8), y = ty - 100;
          for (int s2 = 0; s2 < 6; s2++) {
            int nx = tx + (b - 1) * 14 + fxRnd(sd + b * 97 + s2 * 13 + k / 45, 18), ny = y + 20;
            fxLine(x, y, nx, ny, 5, YEL);
            gfx->drawLine(x, y, nx, ny, WHITE);
            x = nx; y = ny;
          }
        }
        fxStar(tx, ty + 10, 6, 20, 8, k * 0.01f, 2, YEL);
      }
      if (travel) fxStar(ax, ay, 4, 14, 6, t * 0.03f, 2, YEL);  // carga
      break;

    case PT_ICE: {
      const uint16_t CYA = C565(0x80, 0xe0, 0xf8), ICE = C565(0xd8, 0xf6, 0xff);
      if (t >= 120 && t < 460) {
        int w = 5 + (int)(3 * sinf(t * 0.05f));
        fxLine(ax, ay, tx, ty, w + 4, CYA);
        fxLine(ax, ay, tx, ty, w, ICE);
      }
      if (impact)
        for (int i = 0; i < 6; i++) {
          int x = tx + fxRnd(sd + i * 5, 38), y = ty + fxRnd(sd + i * 11, 30);
          int g = (k - i * 40) / 8;
          if (g <= 0 || k > 540) continue;
          if (g > 14) g = 14;
          fxStar(x, y, 0, g, 6, 0.26f, 2, (i & 1) ? CYA : ICE);
        }
      break;
    }

    case PT_FIGHT:
    case PT_NORMAL:
    case 0xFF:
    default: {
      bool big = fx == PT_FIGHT || fx == PT_NORMAL;
      if (impact && k < 380) {
        float g = fxClamp01(k / 160.0f);
        int r = (int)((big ? 34 : 24) * g) + 6;
        uint16_t c1 = fx == PT_FIGHT ? ORA : YEL;
        fxStar(tx, ty, r / 3, r, big ? 10 : 8, 0.2f, 3, c1);
        if (k < 140) gfx->fillCircle(tx, ty, (140 - k) / 8 + 4, WHITE);
        if (big) {
          gfx->drawCircle(tx, ty, r + k / 6, c1);
          gfx->drawCircle(tx, ty, r + k / 6 + 1, c1);
        }
      }
      break;
    }

    case PT_POISON: {
      const uint16_t PUR = C565(0xa0, 0x48, 0xc0), LPU = C565(0xd8, 0xa0, 0xf0);
      if (travel) fxLine(px - (tx - ax) / 20, py - (ty - ay) / 20, px, py, 3, PUR);
      if (impact)
        for (int i = 0; i < 9; i++) {
          float ph = ((k + i * 60) % 480) / 480.0f;
          int x = tx + fxRnd(sd + i * 3, 34) + (int)(4 * sinf(ph * 9 + i)), y = ty + 28 - (int)(ph * 90);
          int r = 4 + (int)(fxHash(sd + i) % 5);
          gfx->fillCircle(x, y, r, LPU);
          gfx->drawCircle(x, y, r, PUR);
        }
      break;
    }

    case PT_GROUND: {
      const uint16_t MUD = C565(0x9c, 0x6c, 0x34), DUST = C565(0xd0, 0xb0, 0x80);
      if (travel)
        for (int i = 0; i < 4; i++) {
          float q = fxClamp01(p - i * 0.08f);
          int x = ax + (int)((tx - ax) * q) + fxRnd(sd + i, 8);
          int y = ay + (int)((ty - ay) * q) - (int)(40 * sinf(q * 3.1416f));
          gfx->fillCircle(x, y, 6 - i, MUD);
        }
      if (impact && k < 560) {
        int d = k / 5;
        for (int s2 = -1; s2 <= 1; s2 += 2) {
          gfx->fillEllipse(tx + s2 * (18 + d), ty + 34 - k / 30, 22 - k / 40, 10 - k / 80, DUST);
        }
        for (int i = 0; i < 6; i++) {
          int x = tx + fxRnd(sd + i * 7, 30), y = ty + 10 - (int)(60 * sinf(fxClamp01(k / 500.0f) * 3.1416f)) + fxRnd(sd + i, 12);
          gfx->fillCircle(x, y, 4, MUD);
        }
      }
      break;
    }

    case PT_PSYCHIC: {
      const uint16_t PNK = C565(0xf0, 0x58, 0xa8), LPK = C565(0xf8, 0xb0, 0xd8);
      if (t < 380) {  // el atacante se concentra
        int r = 46 - (int)(t / 12);
        if (r > 10) { gfx->drawCircle(ax, ay, r, LPK); gfx->drawCircle(ax, ay, r - 1, LPK); }
      }
      if (impact)
        for (int i = 0; i < 3; i++) {
          int r = ((k + i * 110) % 330) / 5 + 8;
          if (k + i * 110 >= 600) continue;
          gfx->drawEllipse(tx, ty, r + 6, r, i == 1 ? LPK : PNK);
          gfx->drawEllipse(tx, ty, r + 7, r + 1, i == 1 ? LPK : PNK);
        }
      break;
    }

    case PT_BUG: {
      const uint16_t LGR = C565(0xc8, 0xe8, 0x70), WHT = WHITE;
      int dx = tx - ax, dy = ty - ay;
      for (int i = 0; i < 4; i++) {
        float q = ((float)t - 120 - i * 70) / 200.0f;
        if (q < 0 || q > 1) continue;
        int x = ax + (int)(dx * q) + fxRnd(sd + i, 10), y = ay + (int)(dy * q) + fxRnd(sd + i * 3, 10);
        fxLine(x - dx / 10, y - dy / 10, x, y, 4, LGR);
        gfx->fillCircle(x, y, 3, WHT);
      }
      if (impact && k < 450)
        for (int i = 0; i < 4; i++) {
          int d = k - i * 70;
          if (d < 0 || d > 200) continue;
          fxStar(tx + fxRnd(sd + i * 9, 26), ty + fxRnd(sd + i * 4, 22), 3, 9 + d / 12, 4, 0.78f, 3, LGR);
        }
      break;
    }

    case PT_ROCK: {
      const uint16_t RCK = C565(0x98, 0x88, 0x70), RKD = C565(0x60, 0x54, 0x44);
      if (hit && t >= 250 && t < 950)
        for (int i = 0; i < 4; i++) {
          int s0 = 250 + i * 90, d = (int)t - s0;
          if (d < 0) continue;
          int x = tx + (i - 1) * 20 - 10 + fxRnd(sd + i, 6);
          int y = d < 220 ? ty - 150 + d * 150 / 220 : ty + (d - 220) / 12;
          if (d > 420) continue;
          int r = 9 + (i & 1) * 3;
          gfx->fillCircle(x, y, r, RKD);
          gfx->fillCircle(x - 2, y - 2, r - 3, RCK);
          if (d >= 220 && d < 300) fxStar(x, y + r, 2, 10, 5, 3.4f, 1, RCK);
        }
      break;
    }

    case PT_GHOST: {
      const uint16_t DPU = C565(0x48, 0x30, 0x70), PUR = C565(0x88, 0x60, 0xc0);
      if (impact && k < 560)
        for (int i = 0; i < 8; i++) {
          float a = i * 0.785f + k * 0.012f;
          int d = 44 - k / 16;
          int x = tx + (int)(cosf(a) * d), y = ty + (int)(sinf(a) * d * 0.7f);
          gfx->fillCircle(x, y, 7 - i / 3, (i & 1) ? DPU : PUR);
        }
      if (travel) gfx->fillEllipse(px, py, 10, 6, DPU);  // sombra que se desliza
      break;
    }

    case PT_DARK: {  // ko10: mordisco: dos filas de colmillos que se cierran
      const uint16_t DK = C565(0x38, 0x30, 0x40), WH = UI_WHITE;
      if (impact && k < 420) {
        int gap = k < 160 ? 40 - k / 4 : 0;
        // fauces oscuras detras de los colmillos: se leen sobre cualquier cielo
        gfx->fillRoundRect(tx - 44, ty - 34 - gap, 88, 20, 8, DK);
        gfx->fillRoundRect(tx - 44, ty + 14 + gap, 88, 20, 8, DK);
        for (int i = -2; i <= 2; i++) {
          int x = tx + i * 14;
          gfx->fillTriangle(x - 7, ty - 26 - gap, x + 7, ty - 26 - gap, x, ty - 8 - gap, WH);
          gfx->fillTriangle(x - 7, ty + 26 + gap, x + 7, ty + 26 + gap, x, ty + 8 + gap, WH);
        }
        gfx->drawRoundRect(tx - 42, ty - 32 - gap, 84, 64 + 2 * gap, 12, DK);
        if (k >= 160 && k < 260) fxStar(tx, ty, 6, 28, 8, 0.4f, 2, DK);
      }
      if (travel) gfx->fillCircle(px, py, 8, DK);
      break;
    }

    case PT_STEEL: {  // ko10: garra metalica: tres zarpazos plateados con brillo
      const uint16_t MT = C565(0x58, 0x62, 0x78), SH = UI_WHITE;
      if (impact && k < 480)
        for (int i = 0; i < 3; i++) {
          int d = k - i * 60;
          if (d < 0 || d > 260) continue;
          float f2 = fxClamp01(d / 120.0f);
          int x0 = tx - 34 + i * 22, y0 = ty - 34;
          int x1 = x0 + (int)(40 * f2), y1 = y0 + (int)(68 * f2);
          fxLine(x0, y0, x1, y1, 7, MT);
          fxLine(x0 + 1, y0, x1 + 1, y1, 2, SH);
        }
      if (impact && k >= 200 && k < 320) fxStar(tx + 10, ty - 10, 2, 16, 4, 0.78f, 2, SH);
      break;
    }

    case PT_DRAGON: {
      const uint16_t DBL = C565(0x50, 0x48, 0xe0), DPU = C565(0x98, 0x50, 0xe8);
      if (travel) {
        gfx->fillCircle(px, py, 11, DPU);
        gfx->fillCircle(px, py, 6, DBL);
      }
      if (impact && k < 580)
        for (int i = 0; i < 10; i++) {
          float a = i * 0.628f + k * 0.01f;
          float ph = ((k + i * 40) % 400) / 400.0f;
          int d = 30 + (int)(10 * ph);
          int x = tx + (int)(cosf(a) * d), y = ty + (int)(sinf(a) * d * 0.6f) - (int)(ph * 30);
          gfx->fillCircle(x, y, 3 + (int)(7 * (1 - ph)), i % 3 == 0 ? RED : (i & 1) ? DBL : DPU);
        }
      break;
    }
  }

  // ko10.4: fase media (aura al cargar + estallido) y final (ademas rayo y estrella grande)
  if (tier >= 1 && fx < PT_COUNT) {
    uint16_t c = FX_COL[fx], cl = lerp565(c, WHITE, 8, 16);
    if (t < 380) {  // aura de carga alrededor del atacante
      int r = 30 + (int)(6 * sinf(t * 0.04f)) + tier * 6;
      gfx->drawCircle(ax, ay, r, c);
      gfx->drawCircle(ax, ay, r + 1, cl);
      if (tier == 2) { gfx->drawCircle(ax, ay, r + 7, c); gfx->drawCircle(ax, ay, r + 8, c); }
    }
    if (tier == 2 && FX_BEAM[fx] && t >= 120 && t < 470) {  // rayo del definitivo
      float L = fxClamp01((t - 120) / 150.0f);
      int ex = ax + (int)((tx - ax) * L), ey = ay + (int)((ty - ay) * L);
      int w = 14 + (int)(4 * sinf(t * 0.08f));
      fxLine(ax, ay, ex, ey, w + 8, c);
      fxLine(ax, ay, ex, ey, w, cl);
      fxLine(ax, ay, ex, ey, w / 3 + 1, WHITE);
    }
    if (impact && k < 440) {  // estallido extra en el objetivo
      float g = fxClamp01(k / 320.0f);
      int R = (int)((tier == 2 ? 72 : 44) * g) + 10;
      gfx->drawCircle(tx, ty, R, c);
      gfx->drawCircle(tx, ty, R + 1, c);
      gfx->drawCircle(tx, ty, R + 3, cl);
      if (tier == 1) fxStar(tx, ty, R / 4, R * 2 / 3, 8, k * 0.004f, 2, c);
      if (tier == 2) {
        fxStar(tx, ty, R / 3, R, 12, k * 0.004f, 3, cl);
        if (k < 120) gfx->fillCircle(tx, ty, 26 - k / 6, WHITE);
      }
    }
  }

  // muy eficaz: onda de choque blanca que se abre desde el objetivo
  if (impact && eff >= 4 && k < 260) {
    int r = 24 + k * 2 / 5;
    for (int w = 0; w < 3; w++) gfx->drawCircle(tx, ty, r + w, WHITE);
    if (k > 60) gfx->drawCircle(tx, ty, r - 22, WHITE);
  }
}

// temblor de la escena durante el impacto de un critico
void updateShake(uint32_t now) {
  bvShakeX = bvShakeY = 0;
  if (bPhase != BP_PLAY || bqI >= bqN) return;
  const BEvent &e = bq[bqI];
  // ko10.4: tambien tiembla con los ataques definitivos (fase final)
  bool fin = e.move == BA_TYPE && (evIsMe(e.side) ? bvMeTier : bvFoeTier) == 2;
  if (e.kind != EV_HIT || !(e.crit || fin) || !e.eff) return;
  int k = (int)(now - bqT) - 350;
  if (k < 0 || k >= 360) return;
  int amp = (e.crit ? 7 : 5) * (360 - k) / 360 + 1;
  bvShakeX = ((k / 30) & 1) ? amp : -amp;
  bvShakeY = ((k / 45) & 1) ? amp / 2 : -amp / 2;
}

// ko11.15.1: en combate se miran: el tuyo de espaldas hacia arriba-derecha y el
// rival hacia abajo-izquierda (si el sprite de la SD trae esas filas)
static uint8_t battleFacing(const PmdMon &m, uint8_t act, bool mine) {
  uint8_t d = act == PMD_IDLE ? PMD_IDLE_UR : act == PMD_ATTACK ? PMD_ATTACK_UR : act == PMD_HURT ? PMD_HURT_UR : 0xFF;
  if (d == 0xFF) return act;
  if (!mine) d += PMD_IDLE_DL - PMD_IDLE_UR;
  return m.has(d) ? d : act;
}

// ko11.16: sprite de combate estilo juego: escala fija en cuartos (s4) para que se
// note el tamano de cada especie, con tope de alto (maxH px); anclado por los pies
static void drawPrgBattler(PmdMon &m, uint8_t act, int cx, int groundY, uint32_t t, bool sil, int s4, int maxH) {
  const PmdAct &a = m.acts[act];
  if (!a.frames) return;
  // ko11.16.1: escala ENTERA (x3, x2, x1) y sin suavizado EPX: el EPX trata el
  // transparente como un color y se comia el contorno negro de 1 px en curvas y
  // diagonales (los de PokeRogue salian palidos y sin borde); con x2,25 ademas los
  // pixeles salian de tamanos distintos. Asi se ven nitidos, como en el juego
  s4 = s4 / 4 * 4;
  while (s4 > 4 && a.h * s4 > maxH * 4) s4 -= 4;
  uint8_t fi = pmdFrameAt(a, t, true);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  int x0 = cx - a.w * s4 / 8, y0 = groundY - (a.base ? a.base : a.h) * s4 / 4;
  bool sm = gSmoothGfx;
  gSmoothGfx = false;
  smoothBlitQ(fr, a.w, a.h, m.pal, x0, y0, s4, sil);
  gSmoothGfx = sm;
}

// dibuja los dos Pokemon; anima al que actua segun el evento en curso
void drawBattlers() {
  uint32_t now = millis();
  int meX = 140 + bvShakeX, meG = 250 + bvShakeY, foeX = 316 + bvShakeX, foeG = 156 + bvShakeY;
  uint8_t meAct = PMD_IDLE, foeAct = PMD_IDLE;
  bool meHide = false, foeHide = false, meSil = false, foeSil = false;
  uint32_t t = now - bqT;
  if (bPhase == BP_PLAY && bqI < bqN) {
    const BEvent &e = bq[bqI];
    bool me = evIsMe(e.side);
    if (e.kind == EV_HIT || e.kind == EV_MISS || e.kind == EV_COUNTER) {
      int lunge = (t < 350) ? (int)(t / 12) : (t < 550 ? (int)((550 - t) / 7) : 0);
      if (me) { meAct = PMD_ATTACK; meX += lunge; meG -= lunge / 2; }
      else    { foeAct = PMD_ATTACK; foeX -= lunge; foeG += lunge / 2; }
      if (e.kind == EV_COUNTER && t < 700) {  // ko11.8: el escudo aun se ve al devolver el golpe
        int r = 34 + (int)((t / 10) % 20);
        if (me) gfx->drawCircle(meX, meG - 50, r, 0x4C98);
        else gfx->drawCircle(foeX, foeG - 40, r, 0x4C98);
      }
      if ((e.kind == EV_HIT || e.kind == EV_COUNTER) && e.eff && t > 350 && t < 1000) {
        bool blink = ((t / 80) % 2) == 0;
        if (me) { foeAct = PMD_HURT; foeHide = blink; }
        else    { meAct = PMD_HURT; meHide = blink; }
      }
    } else if (e.kind == EV_GUARD) {
      int r = 30 + (int)((t / 10) % 30);
      if (me) gfx->drawCircle(meX, meG - 50, r, 0x4C98);
      else gfx->drawCircle(foeX, foeG - 40, r, 0x4C98);
    } else if (e.kind == EV_FAINT) {
      if (me) { meSil = true; meG += (int)(t / 8); }
      else    { foeSil = true; foeG += (int)(t / 8); }
    } else if (e.kind == EV_RUN_OK && me) {
      meAct = PMD_WALKL;
      meX -= (int)(t / 4);
    } else if (e.kind == EV_HEAL) {  // fork KO: destellos verdes al curarse
      for (int k = 0; k < 6; k++) {
        int py = meG - 20 - (int)((t / 6 + k * 23) % 110);
        gfx->fillRect(meX - 40 + k * 16, py, 5, 5, UI_BAR_OK);
      }
    } else if (e.kind == EV_CATCH || e.kind == EV_BREAK) {  // la pokeball vuela y se agita
      float f = t < 500 ? t / 500.0f : 1.0f;
      int bx = meX + (int)((foeX - meX) * f), by = meG - 60 + (int)((foeG - 40 - (meG - 60)) * f) - (int)(60 * sinf(f * 3.14159f));
      if (t >= 500) {
        foeHide = !(e.kind == EV_BREAK && t > 900);   // dentro de la bola (sale si falla)
        bx += (t < 900) ? (int)(6 * sinf(t * 0.05f)) : 0;
      }
      if (!(e.kind == EV_BREAK && t > 900)) drawMap(SPR_ICON_PLAY, 16, bx - 16, by - 16, 2, false);
    }
  }
  // debilitados que ya no se ven (tras reproducir su desmayo)
  bool meGone = bvMeFainted, foeGone = bvFoeFainted || bvFoeCaught;
  if (bvFoeCaught) drawMap(SPR_ICON_PLAY, 16, foeX - 16, foeG - 36, 2, false);  // atrapado

  // ko10.1: un raro aparece con destellos dorados
  if (xScreen == XS_WILD && bGroup == WG_RARE && bPhase == BP_INTRO && !foeGone) {
    uint16_t gold = C565(0xff, 0xd8, 0x40);
    for (int k = 0; k < 8; k++) {
      float a = now / 400.0f + k * 0.785f;
      int r = 58 + (int)(8 * sinf(now / 150.0f + k));
      int sx = foeX + (int)(r * cosf(a)), sy = foeG - 44 + (int)(r * 0.7f * sinf(a));
      int l = 3 + ((now / 120 + k) % 3) * 2;
      gfx->fillRect(sx - l, sy - 1, 2 * l + 1, 3, gold);
      gfx->fillRect(sx - 1, sy - l, 3, 2 * l + 1, gold);
    }
  }
  if (!foeHide && !foeGone && prgFoe.loaded && prgFoe.has(PMD_IDLE)) {  // ko11.16: estilo juego, de frente
    drawPrgBattler(prgFoe, PMD_IDLE, foeX + 14, foeG, now, foeSil, 12, 124);  // x3 (x2 si es grande)
  } else if (!foeHide && !foeGone) {
    if (foePmd.loaded) {
      if (!foePmd.has(foeAct)) foeAct = PMD_IDLE;
      foeAct = battleFacing(foePmd, foeAct, false);
      drawPmdActM(foePmd, foeAct, foeX, foeG, now, true, foeSil, 4, 170);  // ko11.10.1: x4 (antes x3)
    } else {
      const uint8_t *th = thumbs.get(bvFoeDex);
      if (th) drawThumb(th, foeX - GAL_CELL / 2, foeG - GAL_CELL, 2, foeSil);
    }
  }
  if (!meHide && !meGone && pCur == 0 && pSlot0Pet && orbValid(pet.orb) && bvMeDex == pet.speciesId && xScreen != XS_LINK)
    drawOrbSlot(meX + 64, meG - 12, 11, pet.orb, now);  // ko11.16: su orbe, abajo a la derecha (detras del sprite)
  if (!meHide && !meGone && prgMe.loaded && prgMe.has(PMD_IDLE_UR)) {  // ko11.16: de espaldas
    drawPrgBattler(prgMe, PMD_IDLE_UR, meX, meG, now, meSil, 12, 176);
  } else if (!meHide && !meGone) {
    PmdMon &mm = (pCur || !pSlot0Pet) ? helperPmd : pmd;  // ko11.20: el ayudante que lucha ahora
    if (mm.loaded) {
      if (!mm.has(meAct)) meAct = PMD_IDLE;
      meAct = battleFacing(mm, meAct, true);
      drawPmdActM(mm, meAct, meX, meG, now, true, meSil, 4, 170);
    } else {
      const uint8_t *th = thumbs.get(bvMeDex);
      if (th) drawThumb(th, meX - GAL_CELL / 2, meG - GAL_CELL, 3, meSil);
    }
  }
  // fork KO (ko7): efecto del ataque encima de los dos
  if (bPhase == BP_PLAY && bqI < bqN) {
    const BEvent &e = bq[bqI];
    if (e.kind == EV_HIT || e.kind == EV_MISS || e.kind == EV_COUNTER) {
      bool me = evIsMe(e.side);
      uint8_t fx = e.move == BA_TYPE ? (me ? bvMeType : bvFoeType) : 0xFF;
      int ax = me ? 140 : 316, ay = me ? 200 : 116, tx = me ? 316 : 140, ty = me ? 116 : 200;
      drawMoveFx(fx, ax + bvShakeX, ay + bvShakeY, tx + bvShakeX, ty + bvShakeY, t,
                 (e.kind == EV_HIT || e.kind == EV_COUNTER) && e.eff, e.eff, e.move == BA_TYPE ? (me ? bvMeTier : bvFoeTier) : 0);
    }
  }
}

void drawBattleMsg() {
  uiButton(40, 266, 386, 56, 12, UI_WHITE, UI_INK);
  if (bvL2[0]) {
    drawFit(bvL1, 273, 370, UI_INK, 2);
    drawFit(bvL2, 296, 370, UI_BAR_BAD, 2);
  } else {
    drawFit(bvL1, 286, 370, UI_INK, 2);
  }
}

// menu 3x2 de la batalla salvaje (fork KO, ko4: + pocion y pokeball)
//   placaje | tecnica | proteger
//   pocion  | pokeball | huir
#define BM_Y1 326
#define BM_Y2 374
#define BM_H 42
#define BM_X 92
#define BM_W 90
#define BM_GAP 6
static const uint8_t BM_ACT[2][3] = { { BA_TACKLE, BA_TYPE, BA_GUARD }, { BA_POTION, BA_BALL, BA_RUN } };

// ko11.19: combate automatico contra entrenadores (gimnasio, liga, reto del dia).
// autoCount = cuantos rivales seguidos (1..los que quedan); autoLeft = en marcha
uint8_t autoCount = 1, autoLeft = 0, autoPotions = 0;
static uint32_t autoMenuT = 0;
static void battleDoAction(int a);
static bool autoAllowed() { return bKind != BK_WILD && !bLink; }
static uint8_t autoRemaining() { return bTeamN > bTeamI ? (uint8_t)(bTeamN - bTeamI) : 1; }
static bool autoHardFoe() {
  return bKind == BK_CHAMP || bFoe.lvl > bMe.lvl + 2 || typeEff(bFoe.type, bMe.type) > 2 ||
         bFoe.maxHp > bMe.maxHp + bMe.maxHp / 4;
}
// que haria un buen jugador: pocion si hace falta (mas pronto si el rival es fuerte),
// a veces defenderse de un golpe muy eficaz, y si no el ataque que mas hace
static uint8_t autoPick() {
  bool hard = autoHardFoe();
  uint32_t hpPct = (uint32_t)bMe.hp * 100 / (bMe.maxHp ? bMe.maxHp : 1);
  uint32_t foePct = (uint32_t)bFoe.hp * 100 / (bFoe.maxHp ? bFoe.maxHp : 1);
  bool foeAlmostDown = foePct <= 15 && hpPct >= 15;  // rematar antes que curarse
  if (pet.potions && !foeAlmostDown && hpPct < (hard ? AUTO_POTION_HP_HARD : AUTO_POTION_HP) &&
      (hard || autoPotions < AUTO_POTION_MAX))
    return BA_POTION;
  if (typeEff(bFoe.type, bMe.type) >= 4 && hpPct < 70 && bRng.below(100) < 25) return BA_GUARD;
  return battleAi(bMe, bFoe, bRng, 0);
}

void drawBattleMenu() {
  const DexEntry &me = DEX_TBL[bvMeDex];
  char pot[16], ball[16];
  snprintf(pot, sizeof(pot), XT(X_POTION_FMT), pet.potions);
  snprintf(ball, sizeof(ball), XT(X_BALL_FMT), pet.balls);
  int x0 = BM_X, x1 = BM_X + BM_W + BM_GAP, x2 = BM_X + 2 * (BM_W + BM_GAP);
  drawBtn(x0, BM_Y1, BM_W, BM_H, UI_WHITE, UI_INK, moveName(BA_TACKLE, bvMeType));
  drawBtn(x1, BM_Y1, BM_W, BM_H, me.accent, UI_WHITE, moveName(BA_TYPE, bvMeType, bvMeTier));
  drawBtn(x2, BM_Y1, BM_W, BM_H, 0x4C98, UI_WHITE, XT(X_GUARD));
  drawBtn(x0, BM_Y2, BM_W, BM_H, pet.potions ? UI_BAR_OK : UI_TRACK, pet.potions ? UI_WHITE : UI_INK, pot);
  if (autoAllowed()) {  // ko11.19: entrenadores: [자동] [N마리] en vez de ball / huir
    char cnt[16];
    snprintf(cnt, sizeof(cnt), XT(X_AUTO_CNT_FMT), (unsigned)autoCount);
    drawBtn(x1, BM_Y2, BM_W, BM_H, C565(0x6a, 0x4c, 0xf0), UI_WHITE, XT(X_AUTO_BTN));
    drawBtn(x2, BM_Y2, BM_W, BM_H, UI_WHITE, UI_INK, cnt);
    return;
  }
  drawBtn(x1, BM_Y2, BM_W, BM_H, pet.balls ? UI_BAR_BAD : UI_TRACK, pet.balls ? UI_WHITE : UI_INK, ball);
  drawBtn(x2, BM_Y2, BM_W, BM_H, UI_TRACK, UI_INK, XT(X_RUN));
}

int battleMenuHit(int16_t x, int16_t y) {
  int row = (y >= BM_Y1 && y < BM_Y1 + BM_H) ? 0 : (y >= BM_Y2 && y < BM_Y2 + BM_H) ? 1 : -1;
  if (row < 0 || x < BM_X) return -1;
  int col = (x - BM_X) / (BM_W + BM_GAP);
  if (col > 2 || (x - BM_X) % (BM_W + BM_GAP) >= BM_W) return -1;
  return BM_ACT[row][col];
}

// ======================================================================
// ko11.20: equipo (el que crias + 2 ayudantes de la caja) contra entrenadores.
// Como en PokeRogue: el rival se ve cuando sale; al caer uno suyo se pregunta
// "¿cambiar?", al caer el mio se elige quien sale, y tocando mi caja de vida
// se cambia a mano (el rival ataca ese turno).
// ======================================================================
#define SW_Y 330
#define SW_H 46
static bool partyOtherAlive() {
  for (uint8_t j = 0; j < pN; j++) if (j != pCur && pMon[j].hp > 0) return true;
  return false;
}
static void partyMeName() {
  const char *mn = pCur == 0 && pSlot0Pet ? (pet.nick[0] ? pet.nick : dexName(pet.speciesId)) : dexName(bMe.dex);
  snprintf(bvMeName, sizeof(bvMeName), "%s%s", (pShiny >> pCur) & 1 ? "*" : "", mn);
}
static void partyLoadMe() {
  if (!pCur && pSlot0Pet) return;
  bool sh = (pShiny >> pCur) & 1;
  if (helperPmd.loaded && helperPmdDex == bMe.dex && helperPmdShiny == sh) return;
  helperPmd.unload();
  helperPmdDex = bMe.dex;
  helperPmdShiny = sh;
  helperPmd.load((uint8_t)bMe.dex, sh);
}
// el mejor para salir contra el rival de ahora (-1 = mejor quedarse, si se puede)
static int8_t partyBest(bool forced) {
  int8_t best = -1;
  int bs = -10000;
  for (uint8_t j = 0; j < pN; j++) {
    if (j == pCur || pMon[j].hp == 0) continue;
    int sc = typeMatch(pMon[j].type, bFoe.type) * 100 + (int)((uint32_t)pMon[j].hp * 50 / pMon[j].maxHp);
    if (sc > bs) { bs = sc; best = (int8_t)j; }
  }
  if (!forced && best >= 0) {
    int cur = typeMatch(bMe.type, bFoe.type) * 100 + (int)((uint32_t)bMe.hp * 50 / bMe.maxHp);
    if (bs < cur + 60) return -1;  // solo si se gana mucho
  }
  return best;
}
static void partyAskSwap(uint8_t mode) {
  pMon[pCur] = bMe;
  bSwapMode = mode;
  bPhase = BP_SWAP;
  autoMenuT = millis();
}
static void partySwitchTo(uint8_t j) {
  pMon[pCur] = bMe;
  pCur = j;
  pUsed |= (uint8_t)(1 << j);
  bMe = pMon[j];
  bvMeDex = bMe.dex;
  bvMeType = bMe.type;
  bvMeTier = moveTier(bMe.dex);
  bvMeLvl = bMe.lvl;
  bvMeMax = bMe.maxHp;
  bvMeHp = bvMeTgt = bMe.hp;
  bvMeFainted = false;
  partyMeName();
  partyLoadMe();
  prgLoadFor(bMe.dex, bFoe.dex, bvFoeShiny);
  txFmt(bvL1, sizeof(bvL1), X_PT_SENDOUT, bvMeName);
  bvL2[0] = 0;
  audioCry(bMe.dex);
}
// botones del cambio: [ayudante][ayudante][quedarse/cancelar]; who = indice o -1
static int swapLayout(int16_t *xs, int16_t *ws, int8_t *who) {
  int m = 0;
  for (uint8_t j = 0; j < pN; j++) if (j != pCur && pMon[j].hp > 0) who[m++] = (int8_t)j;
  bool extra = bSwapMode != 0;
  int total = 320, gap = 6, ew = extra ? 80 : 0;
  int bw = (total - ew - gap * (m - 1 + (extra ? 1 : 0))) / (m ? m : 1);
  int x = 73;
  for (int i = 0; i < m; i++) { xs[i] = (int16_t)x; ws[i] = (int16_t)bw; x += bw + gap; }
  if (extra) { xs[m] = (int16_t)x; ws[m] = (int16_t)ew; who[m] = -1; m++; }
  return m;
}
static void drawMiniBall(int cx, int cy, bool alive, bool cur) {
  if (cur) gfx->fillCircle(cx, cy, 8, UI_BAR_WARN);
  gfx->fillCircle(cx, cy, 6, alive ? UI_WHITE : C565(0x9a, 0x9a, 0x9a));
  if (alive) gfx->fillRect(cx - 6, cy - 6, 13, 6, UI_BAR_BAD);
  gfx->drawCircle(cx, cy, 6, UI_INK);
  gfx->drawFastHLine(cx - 6, cy, 13, UI_INK);
  gfx->fillCircle(cx, cy, 2, UI_WHITE);
  gfx->drawCircle(cx, cy, 2, UI_INK);
}
// bolas del equipo: las del rival bajo su caja, las mias bajo la mia
static void drawPartyBalls() {
  if (bKind == BK_WILD || bLink) return;
  for (uint8_t i = 0; i < bTeamN; i++)
    drawMiniBall(96 + i * 18, 118, i > bTeamI || (i == bTeamI && bFoe.hp > 0), i == bTeamI);
  if (pN > 1)
    for (uint8_t i = 0; i < pN; i++) {
      uint16_t hp = i == pCur ? bMe.hp : pMon[i].hp;
      drawMiniBall(400 - (pN - 1 - i) * 18, 244, hp > 0, i == pCur);
    }
}
static void drawSwapPanel() {
  uiButton(40, 266, 386, 56, 12, UI_WHITE, UI_INK);
  char t[64];
  if (bSwapMode == 1) txFmt(t, sizeof(t), X_PT_NEXT_FMT, dexName(bFoe.dex));
  else strncpy(t, XT(bSwapMode == 2 ? X_PT_MANUAL : X_PT_WHO), sizeof(t) - 1), t[sizeof(t) - 1] = 0;
  drawFit(t, 285, 370, UI_INK, 2);
  int16_t xs[PARTY_MAX + 1], ws[PARTY_MAX + 1];
  int8_t who[PARTY_MAX + 1];
  int m = swapLayout(xs, ws, who);
  for (int i = 0; i < m; i++) {
    if (who[i] < 0) {
      drawBtn(xs[i], SW_Y, ws[i], SW_H, UI_TRACK, UI_INK, XT(bSwapMode == 2 ? X_BAK_CANCEL : X_PT_KEEP));
      continue;
    }
    const Battler &b = pMon[who[i]];
    int8_t tm = typeMatch(b.type, bFoe.type);
    uint16_t bg = tm > 0 ? C565(0xd6, 0xf5, 0xcc) : tm < 0 ? C565(0xff, 0xdc, 0xdc) : UI_WHITE;
    uiButton(xs[i], SW_Y, ws[i], SW_H, 10, bg, UI_INK);
    drawThumbAt(b.dex, xs[i] + 22, SW_Y + SW_H / 2, 1, false);
    gfx->setTextColor(UI_INK);
    setSize(1);
    setCur(xs[i] + 42, SW_Y + 4);
    printT(dexName(b.dex));
    char l[32];
    snprintf(l, sizeof(l), "Lv%u %s", b.lvl, tm > 0 ? XT(X_PT_GOOD) : tm < 0 ? XT(X_PT_BAD) : "");
    gfx->setTextColor(tm > 0 ? C565(0x1a, 0x8a, 0x3a) : tm < 0 ? UI_BAR_BAD : UI_INK);
    setCur(xs[i] + 42, SW_Y + 22);
    printT(l);
    int bw = ws[i] - 50;
    gfx->fillRect(xs[i] + 42, SW_Y + SW_H - 7, bw, 4, UI_TRACK);
    gfx->fillRect(xs[i] + 42, SW_Y + SW_H - 7, (int)((uint32_t)bw * b.hp / b.maxHp), 4,
                  b.hp * 4 < b.maxHp ? UI_BAR_BAD : UI_BAR_OK);
  }
}
// toque en BP_SWAP
static void swapTap(int16_t x, int16_t y) {
  if (y < SW_Y || y >= SW_Y + SW_H) return;
  int16_t xs[PARTY_MAX + 1], ws[PARTY_MAX + 1];
  int8_t who[PARTY_MAX + 1];
  int m = swapLayout(xs, ws, who);
  for (int i = 0; i < m; i++) {
    if (x < xs[i] || x >= xs[i] + ws[i]) continue;
    sfxPlay(SFX_TAP);
    uint8_t mode = bSwapMode;
    if (who[i] < 0) {  // quedarse / cancelar
      bPhase = BP_MENU;
      autoMenuT = millis();
      txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
      bvL2[0] = 0;
      return;
    }
    partySwitchTo((uint8_t)who[i]);
    if (mode == 2) {  // a mano: el rival aprovecha el turno
      BAct foeAct = foeMoveRule(battleAi(bFoe, bMe, bRng, 35));
      bqN = battleFoeOnly(bMe, bFoe, foeAct, bRng, bq, BATTLE_MAX_EVENTS);
      bqAisMe = true;
      if (bqN > 0) { bPhase = BP_PLAY; startEvent(0); return; }
    }
    bPhase = BP_MENU;
    autoMenuT = millis();
    return;
  }
}

// ko11.19: aviso del combate automatico (abajo, donde van los botones)
static void drawAutoBanner() {
  char t[64];
  snprintf(t, sizeof(t), XT(X_AUTO_ON_FMT), (unsigned)autoLeft);
  uint16_t pc = C565(0x6a, 0x4c, 0xf0);
  gfx->fillRoundRect(BM_X, BM_Y1, 3 * BM_W + 2 * BM_GAP, BM_H, 12, pc);
  drawFit(t, BM_Y1 + BM_H / 2 - 10, 3 * BM_W, UI_WHITE, 2);
  drawFit(XT(X_AUTO_TAP), BM_Y2 + 8, 3 * BM_W, 0x6B4D, 1);
}

void renderBattleView() {
  uint32_t now = millis();
  lastInteract = now;  // una batalla no deja que la pantalla se atenue
  // barras de vida que bajan poco a poco
  bvMeHp += (bvMeTgt - bvMeHp) * 0.25f;
  bvFoeHp += (bvFoeTgt - bvFoeHp) * 0.25f;
  if (fabsf(bvMeTgt - bvMeHp) < 0.5f) bvMeHp = bvMeTgt;
  if (fabsf(bvFoeTgt - bvFoeHp) < 0.5f) bvFoeHp = bvFoeTgt;

  updateShake(now);
  drawBattleBg();
  drawBattlers();
  drawHpBox(84, 50, 176, bvFoeName, bvFoeLvl, bvFoeHp, bvFoeMax, true, nameInkFor(bvFoeDex));  // ko9: rival con numeros
  drawHpBox(236, 176, 176, bvMeName, bvMeLvl, bvMeHp, bvMeMax, true, nameInkFor(bvMeDex));
  drawPartyBalls();  // ko11.20
  // ko10.11: ya lo tengo: "en la caja: N" bajo la caja del rival (5 s al aparecer)
  if (bKind == BK_WILD && !bLink && bvOwned && now - bvOwnedT < 5000) {
    char ob[32];
    snprintf(ob, sizeof(ob), XT(X_OWNED_FMT), bvOwned);
    int w = textW(ob, 1) + 20;
    gfx->fillRoundRect(84, 110, w, 24, 10, C565(0xf0, 0x5a, 0x9a));
    gfx->setTextColor(UI_WHITE);
    setSize(1);
    setCur(94, gCjkFont ? 112 : 118);
    printT(ob);
  }

  if (bPhase == BP_JOIN) {  // ko11.8: quiere venir
    uiButton(40, 266, 386, 56, 12, UI_WHITE, UI_INK);
    char q[64];
    txFmt(q, sizeof(q), X_JOIN_Q, dexName(bFoe.dex));
    drawFit(q, 272, 370, UI_INK, 2);
    drawFit(XT(X_JOIN_SUB), 298, 370, C565(0xc8, 0x3c, 0x78), 1);
    bool full = box.full() && !ownsSpecies(bFoe.dex);
    drawBtn(BDUP_KEEP_X, BN_Y, BDUP_W, BN_H, full ? UI_TRACK : UI_BAR_OK, full ? 0x8410 : UI_WHITE, XT(X_JOIN_TAKE));
    drawBtn(BDUP_CANDY_X, BN_Y, BDUP_W, BN_H, UI_TRACK, UI_INK, XT(X_JOIN_LEAVE));
    if (full) drawFit(XT(X_BOX_FULL), BN_Y + BN_H + 8, 300, UI_BAR_BAD, 1);
  } else if (bPhase == BP_DUP) {  // ko10.4: repetido
    uiButton(40, 266, 386, 56, 12, UI_WHITE, UI_INK);
    char q[64];
    txFmt(q, sizeof(q), X_DUP_Q, dexName(bFoe.dex));
    drawFit(q, 272, 370, UI_INK, 2);
    char have[40];
    char nb[8];
    snprintf(nb, sizeof(nb), "%u", pet.candyOf(bFoe.dex));
    txFmt(have, sizeof(have), X_CANDY_HAVE, dexName(DEX_FAM[bFoe.dex]), nb);
    drawFit(have, 298, 370, UI_INK, 1);
    char kb[32], cb[32];
    snprintf(kb, sizeof(kb), XT(X_DUP_KEEP), (unsigned)CANDY_KEEP);
    snprintf(cb, sizeof(cb), XT(X_DUP_CANDY), (unsigned)Pet::dupCandy(bvFoeShiny, bFoe.lvl));
    bool full = box.full();
    drawBtn(BDUP_KEEP_X, BN_Y, BDUP_W, BN_H, full ? UI_TRACK : 0x4C98, full ? 0x8410 : UI_WHITE, kb);
    drawBtn(BDUP_CANDY_X, BN_Y, BDUP_W, BN_H, C565(0xf0, 0x7a, 0xa8), UI_WHITE, cb);
    if (full) drawFit(XT(X_BOX_FULL), BN_Y + BN_H + 8, 300, UI_BAR_BAD, 1);
  } else if (bPhase == BP_NEXT) {
    uiButton(40, 266, 386, 56, 12, UI_WHITE, UI_INK);
    if (bNote[0]) {  // ko10.4: caramelos recien ganados
      drawFit(XT(X_NEXT_Q), 270, 370, UI_INK, 2);
      drawFit(bNote, 298, 370, C565(0xc8, 0x3c, 0x78), 1);
    } else {
      drawFit(XT(X_NEXT_Q), 284, 370, UI_INK, 2);
    }
    drawBtn(83, BN_Y, 146, BN_H, UI_BAR_OK, UI_WHITE, XT(X_NEXT_GO));
    drawBtn(237, BN_Y, 146, BN_H, UI_TRACK, UI_INK, XT(X_NEXT_EXIT));
  } else if (bPhase == BP_RESULT) {
    bool good = bWon || bCaught;
    uiPanel(60, 270, 346, 128, 16, good ? UI_BAR_WARN : UI_WHITE, UI_INK);
    const char *big = bFled ? XT(X_FLED) : bCaught ? XT(X_GOTCHA) : bWon ? XT(X_WIN) : XT(X_LOSE);
    drawFit(big, 280, 320, UI_INK, bFled ? 2 : 4);
    int ly = 322;
    if (good && pet.lastExpGain) {  // fork KO (ko7): EXP ganada (y nivel nuevo)
      char ex[64];
      int k = snprintf(ex, sizeof(ex), XT(X_EXP_GAIN_FMT), (unsigned long)pet.lastExpGain);
      if (pet.lastLvlUp && k > 0 && k < (int)sizeof(ex) - 2) {
        strcpy(ex + k, "  ");
        snprintf(ex + k + 2, sizeof(ex) - k - 2, XT(X_LVUP_FMT), pet.level());
      }
      drawFit(ex, ly, 330, pet.lastLvlUp ? UI_EXP : UI_INK, 2);
      ly += 26;
    }
    if (good && !bLink) {  // fork KO (ko4): objetos y caja
      if (ly < 374) {
        char it[48];
        it[0] = 0;
        if (bItems & 1) strncat(it, XT(X_GOT_BALL), sizeof(it) - 1);
        if (bItems & 2) {
          if (it[0]) strncat(it, "  ", sizeof(it) - strlen(it) - 1);
          strncat(it, XT(X_GOT_POTION), sizeof(it) - strlen(it) - 1);
        }
        drawFit(it[0] ? it : XT(X_REWARD), ly, 320, UI_INK, 2);
        ly += 26;
      }
      if (bBoxMsg >= 0 && ly <= 374) {
        drawFit(XT((XId)bBoxMsg), ly, 320, bBoxMsg == X_BOX_FULL ? UI_BAR_BAD : UI_INK, 2);
        ly += 26;
      }
      if (bNote[0] && bPartyNote[0] && ly > 350) {  // ko11.20: sin sitio: los dos en una linea
        char both[112];
        snprintf(both, sizeof(both), "%s  %s", bNote, bPartyNote);
        if (ly <= 374) drawFit(both, ly, 330, C565(0xa0, 0x30, 0x60), 2);
      } else {
        if (bNote[0] && ly <= 374) { drawFit(bNote, ly, 330, C565(0xa0, 0x30, 0x60), 2); ly += 24; }  // ko10.4: medalla / reto
        if (bPartyNote[0] && ly <= 374) drawFit(bPartyNote, ly, 330, C565(0x1a, 0x8a, 0x3a), 2);  // ko11.20
      }
    } else if (good && ly < 374) {
      drawFit(XT(X_REWARD), ly, 320, UI_INK, 2);
    }
  } else {
    if (bPhase == BP_SWAP) drawSwapPanel();  // ko11.20
    else drawBattleMsg();
    if (bPhase == BP_MENU && !autoLeft) { drawBattleMenu(); if (xScreen == XS_WILD) drawBattleArtChip(); }  // ko11.16
    if (autoLeft && bPhase != BP_RESULT && bPhase != BP_SWAP) drawAutoBanner();  // ko11.19
    if (bPhase == BP_MENU && !autoLeft && xScreen == XS_WILD && (bKind == BK_STORY || bKind == BK_ROGUE)) {  // ko11.23.1: salir
      drawNav(NAV_L, UI_INK);
      if (bQuitArm && millis() - bQuitArm < 3000) {
        const char *q = STX[SX_QUIT_SURE];
        int w = textW(q, 2) + 28;
        uiButton(CX - w / 2, 128, w, 36, 18, UI_BAR_BAD, UI_INK);
        drawFit(q, 134, w - 12, UI_WHITE, 2);
      }
    }
  }
  uiFlush();
}

void bvSetup(const Battler &me, const Battler &foe, const char *foeNick, bool foeShiny) {
  // ko11.16: sprites de combate (si la SD los tiene)
  prgLoadFor(me.dex, foe.dex, foeShiny);
  bvMeDex = me.dex; bvFoeDex = foe.dex;
  bvMeType = me.type; bvFoeType = foe.type;
  bvMeTier = moveTier(me.dex); bvFoeTier = moveTier(foe.dex);
  bvMeLvl = me.lvl; bvFoeLvl = foe.lvl;
  bvMeMax = me.maxHp; bvFoeMax = foe.maxHp;
  bvMeHp = bvMeTgt = me.hp;
  bvFoeHp = bvFoeTgt = foe.hp;
  const char *mn = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  strncpy(bvMeName, mn, sizeof(bvMeName) - 1);
  bvMeName[sizeof(bvMeName) - 1] = 0;
  const char *fn = (foeNick && foeNick[0]) ? foeNick : dexName(foe.dex);
  snprintf(bvFoeName, sizeof(bvFoeName), "%s%s", foeShiny ? "*" : "", fn);
  bvFoeShiny = foeShiny;
  loadFoe(foe.dex, foeShiny);
  bvL1[0] = bvL2[0] = 0;
  bqN = bqI = 0;
  bWon = bFled = bRewarded = false;
  bItems = 0;
  bDupPending = false;  // ko10.4
  bJoinPending = false;  // ko11.8
  bNote[0] = 0;
  bPartyNote[0] = 0;  // ko11.20
  bvMeFainted = bvFoeFainted = false;
  bvFoeCaught = bCaught = false;
  bBoxMsg = -1;
  dexLog.seen(foe.dex, clockEpoch());  // fork KO (ko4): la pokedex lo registra como visto
}

// ======================================================================
// Yasaeng: batalla contra un salvaje
// ======================================================================

uint32_t wildAlertUntil = 0;   // aviso "! yasaeng !" en la pantalla principal
// ko9.1: el aviso se va a los 30 s si nadie lo toca (antes 5 min)
#define WILD_ALERT_MS 30000UL
uint32_t wildNextRoll = 0;

void vibPulse(uint16_t ms, uint8_t n, uint16_t gap);
void triggerWildAlert() { wildAlertUntil = millis() + WILD_ALERT_MS; vibPulse(200, 3, 150); }  // ko11.25

bool battleAllowed(bool toast) {
  if (!pet.canBattle()) { if (toast) showToast(XT(X_CANT_NOW)); sfxPlay(SFX_DENY); return false; }
  if (pet.tooTiredToBattle()) { if (toast) showToast(XT(X_TOO_TIRED)); sfxPlay(SFX_DENY); return false; }
  return true;
}

uint8_t petRegion() { return pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome; }

// ko10.4: el tiempo de ahora cuenta en la batalla; se avisa en la 2a linea
void battleWeatherIntro() {
  uint8_t wx = sceneWeather();
  battleSetWeather(wx);
  const char *m = wx == WX_RAIN ? XT(X_WX_RAIN) : wx == WX_SUNNY && sceneHour() >= 6 && sceneHour() < 20 ? XT(X_WX_SUNNY)
                : wx == WX_SNOW ? XT(X_WX_SNOW) : "";
  if (wx == WX_SUNNY && !m[0]) battleSetWeather(WX_CLEAR);  // de noche no hay sol
  strncpy(bvL2, m, sizeof(bvL2) - 1);
  bvL2[sizeof(bvL2) - 1] = 0;
}

// ko10.4: combate contra entrenador: team[0..n-1] uno tras otro, sin huir ni pokeball
static void startTrainer(uint8_t kind, uint8_t region, const Battler *team, uint8_t n) {
  if (!battleAllowed(true)) return;
  wildAlertUntil = 0;
  cardOpen = false;
  bKind = kind;
  bRegion = region < REGION_COUNT ? region : 0;
  bTeamN = n;
  autoLeft = 0; autoPotions = 0; autoCount = n;  // ko11.19: por defecto, todo el equipo
  bTeamI = 0;
  for (uint8_t i = 0; i < n; i++) bTeam[i] = team[i];
  bRng = BRng(esp_random());
  bMe = makeBattler(pet.speciesId, pet.level(), pet.atkStat(), pet.defStat(), pet.speStat());
  pSlot0Pet = true;
  // ko11.21: en la historia y la expedicion lucha su companero; los premios, para el que crias
  if (kind == BK_STORY || kind == BK_ROGUE) { bMe = storyPartnerBattler(); pSlot0Pet = false; }
  else applyOrb(bMe);  // ko11.16
  // ko11.20: el equipo: el que crias primero y los ayudantes elegidos
  pN = 1; pCur = 0; pUsed = 1; pShiny = 0;
  pMon[0] = bMe; pBox[0] = -1;
  for (uint8_t k = 0; ppArmed && k < PARTY_HELPERS; k++) {
    int8_t bi = ppPick[k];
    if (bi < 0 || bi >= box.count()) continue;
    const BoxMon &m = box.at((uint8_t)bi);
    pMon[pN] = makeBoxBattler(m.dex, m.lvl, pSlot0Pet ? pet.level() : bMe.lvl, m.geneAtk, m.geneDef, m.geneSpe);
    pBox[pN] = bi;
    if (m.flags & BOXF_SHINY) pShiny |= (uint8_t)(1 << pN);
    pN++;
  }
  ppArmed = false;
  if (kind == BK_STORY) storyAddParty();  // ko11.22: los de la historia (no la caja)
  gStoryFlyAt = kind == BK_STORY ? &bMe : nullptr;  // ko11.23.3
  if (kind == BK_ROGUE) rogueApplyParty();  // ko11.21: la expedicion trae la vida de la oleada anterior
  bFoe = bTeam[0];
  bvSetup(bMe, bFoe, nullptr, false);
  if (pCur || !pSlot0Pet) { partyMeName(); partyLoadMe(); }
  bGroup = WG_COMMON;
  bqAisMe = true;
  bLink = false;
  if (kind == BK_GYM) txFmt(bvL1, sizeof(bvL1), X_GYM_INTRO, XT((XId)(X_LEADER_0 + bGym)));
  else if (kind == BK_CHAMP) strncpy(bvL1, XT(X_CHAMP_INTRO), sizeof(bvL1) - 1);
  else if (kind == BK_STORY || kind == BK_ROGUE) {  // ko11.21
    const char *fn = storyFoeName();
    if (kind == BK_STORY && stBattleWho != W_NONE) txFmtRaw(bvL1, sizeof(bvL1), STX[SX_INTRO_FMT], fn, nullptr);
    else snprintf(bvL1, sizeof(bvL1), "%s", fn);
  }
  else txFmt(bvL1, sizeof(bvL1), X_DAILY_INTRO, XT((XId)(X_REG_0 + bRegion)));
  battleWeatherIntro();
  bPhase = BP_INTRO;
  bPhaseT = millis();
  xScreen = XS_WILD;
  sfxPlay(SFX_MEDAL);
  audioSetBattleMusic(false, true);
  audioCry(bFoe.dex);
}

// el rival cayo: si le quedan Pokemon, sale el siguiente (EXP del vencido ya)
static bool nextTrainerMon() {
  if (bKind == BK_WILD || bTeamI + 1 >= bTeamN) return false;
  if (autoLeft) autoLeft--;  // ko11.19: uno menos en automatico
  if (autoCount > bTeamN - bTeamI - 1) autoCount = (uint8_t)(bTeamN - bTeamI - 1);
  pet.addExp(battleExp(bFoe.dex, bFoe.lvl));
  if (pCur == 0 && pSlot0Pet) bvMeLvl = pet.level();
  bTeamI++;
  if (bKind == BK_CHAMP) {  // ko11.20: el campeon elige segun el que tengo en el campo
    uint8_t j = pickNextFoe(bTeam, bTeamI, bTeamN, bMe.type);
    if (j != bTeamI) { Battler t = bTeam[bTeamI]; bTeam[bTeamI] = bTeam[j]; bTeam[j] = t; }
  }
  bFoe = bTeam[bTeamI];
  // ko11.18: reto del dia: antes del siguiente rival se recupera el 35 % de la vida
  // que queda (40 -> +14 = 54), sin pasar del maximo. Gimnasio y liga, como antes
  uint16_t healed = 0;
  if (bKind == BK_DAILY && bMe.hp > 0) {
    uint16_t add = (uint16_t)((uint32_t)bMe.hp * DAILY_HEAL_PCT / 100);
    if (bMe.hp + add > bMe.maxHp) add = bMe.maxHp - bMe.hp;
    bMe.hp += add;
    healed = add;
  }
  bvSetup(bMe, bFoe, nullptr, false);
  partyMeName();  // ko11.20: puede estar luchando un ayudante
  const char *who = bKind == BK_GYM ? XT((XId)(X_LEADER_0 + bGym)) : bKind == BK_CHAMP ? XT(X_CHAMP_NAME)
                    : (bKind == BK_STORY || bKind == BK_ROGUE) ? storyFoeName() : XT(X_DAILY_FOE);
  txFmt(bvL1, sizeof(bvL1), X_TRAINER_NEXT, who, dexName(bFoe.dex));
  bvL2[0] = 0;
  if (healed) snprintf(bvL2, sizeof(bvL2), XT(X_DAILY_HEAL_FMT), (unsigned)healed);
  bPhase = BP_INTRO;
  bPhaseT = millis();
  audioCry(bFoe.dex);
  return true;
}

void startWildIn(uint8_t region) {
  if (!battleAllowed(true)) return;
  autoLeft = 0;  // ko11.19
  partyEnd();    // ko11.20.1: siempre con el que crias
  wildAlertUntil = 0;
  cardOpen = false;
  bRegion = region < REGION_COUNT ? region : 0;
  bRng = BRng(esp_random());
  bMe = makeBattler(pet.speciesId, pet.level(), pet.atkStat(), pet.defStat(), pet.speStat());
  uint32_t ep = pet.lastSeenEpoch;
  // ko10.11: hasta 3 tiradas; una especie ya vista/capturada se queda solo al 45 %
  // (asi salen mas nuevas). Los raros se quedan siempre
  DayEvent ev = dayEvent(gClockTrusted ? clockEpoch() : 0);  // ko11.7: evento del dia
  for (int t = 0; t < 3; t++) {
    bFoe = makeWildIn(bRegion, pet.level(), (uint8_t)sceneHour(), sceneWeather(), wxSeason(wxMonth(ep)),
                      bRng, &bGroup, &ev);
    bool fresh = !dexDiscovered(bFoe.dex) || (dexLog.caughtCount(bFoe.dex) == 0 && !ownsSpecies(bFoe.dex));
    if (bGroup == WG_RARE || fresh || bRng.below(100) < 45) break;
  }
  bExpDex = bFoe.dex;  // ko11: la EXP no se infla por subirle el nivel
  bExpLvl = bFoe.lvl;
  wildMatchPower(bFoe, bMe, bRng);  // ko10.11: de tu talla (90-105 % de tu fuerza)
  applyOrb(bMe);  // ko11.16: DESPUES de igualar: el orbe si se nota contra el salvaje
  bool shiny = bRng.below(ev.kind == DEV_SHINY ? 32 : 64) == 0;  // ko11.7: fin de semana x2
  bvSetup(bMe, bFoe, nullptr, shiny);
  // ko10.11: cuantos de esta especie hay ya en la caja (se ensena unos segundos)
  bvOwned = 0;
  for (uint8_t i = 0; i < box.count(); i++)
    if (box.at(i).dex == bFoe.dex) bvOwned++;
  bvOwnedT = millis();
  bqAisMe = true;
  bLink = false;
  txFmt(bvL1, sizeof(bvL1), bGroup == WG_RARE ? X_WILD_RARE : X_WILD_AT,
        XT((XId)(X_REG_0 + bRegion)), dexName(bFoe.dex));
  bKind = BK_WILD;
  battleWeatherIntro();
  bPhase = BP_INTRO;
  bPhaseT = millis();
  xScreen = XS_WILD;
  sfxPlay(bGroup == WG_RARE ? SFX_EVOLVE : SFX_MEDAL);
  audioSetBattleMusic(false, true);  // batalla nueva: la cancion empieza de cero
  audioCry(bFoe.dex);
}

// aviso de salvaje en la pantalla principal: en la region de mi Pokemon
void startWild() { startWildIn(petRegion()); }

// ---- ko10.1: pantalla "a donde vamos?": 2 paginas de 8 regiones (2x4, botones
// grandes para no fallar el toque). Se pasa de pagina deslizando o con las flechas
#define RG_PER_PAGE 8
#define RG_X 78
#define RG_Y 104
#define RG_W 150
#define RG_H 52
#define RG_GAPX 10
#define RG_GAPY 10
#define RG_ARROW_Y 222
#define RG_DOTS_Y 356
#define RG_BACK_Y 372
#define RG_ART_X 92     // ko11.16.1: [그림: PMD/포케로그] [뒤로]
#define RG_BACK_X 238
#define RG_ART_W 136
uint8_t regionPage = 0;
uint32_t regionMsgUntil = 0;
// ko10.4: abierta si hay medallas suficientes; la region de mi Pokemon, siempre
bool regionOpen(uint8_t r) { return r == petRegion() || regionUnlocked(r, pet.badges); }

void openRegionPick() {
  if (!battleAllowed(true)) return;
  retMark();
  trainMenuOpen = false;
  xScreen = XS_REGION;
  cardOpen = false;
  regionPage = petRegion() / RG_PER_PAGE;  // empieza en la pagina de mi region
  sfxPlay(SFX_TAP);
}

static void drawRegionArrow(int x, bool left) {
  uint16_t c = UI_INK;
  if (left) gfx->fillTriangle(x + 8, RG_ARROW_Y - 14, x + 8, RG_ARROW_Y + 14, x - 8, RG_ARROW_Y, c);
  else      gfx->fillTriangle(x - 8, RG_ARROW_Y - 14, x - 8, RG_ARROW_Y + 14, x + 8, RG_ARROW_Y, c);
}

void renderRegionPick() {
  screenBase();
  drawFit(XT(X_REGION_Q), 44, 300, UI_INK, 3);
  // ko11.7: el evento de hoy, bajo el titulo
  DayEvent ev = dayEvent(gClockTrusted ? clockEpoch() : 0);
  if (ev.kind != DEV_NONE) {
    char el[80];
    if (ev.moonNight) snprintf(el, sizeof(el), "%s", XT(X_EVENT_MOON));
    else if (ev.kind == DEV_TYPE) snprintf(el, sizeof(el), XT(X_EVENT_TYPE_FMT), typeName(ev.ptype));
    else snprintf(el, sizeof(el), "%s", XT(X_EVENT_SHINY));
    drawFit(el, 80, 330, C565(0xc0, 0x40, 0x90), 1);
  }
  uint8_t mine = petRegion();
  for (int k = 0; k < RG_PER_PAGE; k++) {
    int i = regionPage * RG_PER_PAGE + k;
    int x = RG_X + (k % 2) * (RG_W + RG_GAPX), y = RG_Y + (k / 2) * (RG_H + RG_GAPY);
    uint16_t bg = BIOME_SOIL[i];
    int lum = ((bg >> 11) & 31) * 2 + ((bg >> 5) & 63) * 2 + (bg & 31);  // aprox. 0..250
    uint16_t fg = lum > 150 ? UI_INK : UI_WHITE;
    if (!regionOpen((uint8_t)i)) {  // ko10.4: faltan medallas
      char l[40];
      snprintf(l, sizeof(l), XT(X_REGION_LOCK_FMT), XT((XId)(X_REG_0 + i)), regionBadgesNeeded((uint8_t)i));
      drawBtn(x, y, RG_W, RG_H, UI_TRACK, 0x8410, l);
    } else {
      drawBtn(x, y, RG_W, RG_H, bg, fg, XT((XId)(X_REG_0 + i)));
    }
    if (i == mine) {  // la region de mi Pokemon: marco naranja
      uint16_t o = C565(0xff, 0x8a, 0x1a);
      gfx->drawRoundRect(x - 2, y - 2, RG_W + 4, RG_H + 4, 13, o);
      gfx->drawRoundRect(x - 3, y - 3, RG_W + 6, RG_H + 6, 14, o);
      gfx->fillCircle(x + RG_W - 9, y + 9, 5, o);
    }
  }
  if (regionPage > 0) drawRegionArrow(40, true);
  else drawNav(NAV_L, UI_INK);  // ko11.17: en la 1a pagina = volver
  if (regionPage < 1) drawRegionArrow(426, false);
  if (timeLeft(regionMsgUntil)) {  // ko10.4: "faltan medallas"
    drawFit(XT(X_REGION_LOCKED), RG_DOTS_Y - 8, 300, UI_BAR_BAD, 1);
  } else {
    for (int p = 0; p < 2; p++) {
      int x = CX - 13 + p * 26;
      if (p == regionPage) gfx->fillCircle(x, RG_DOTS_Y, 5, UI_INK);
      else gfx->drawCircle(x, RG_DOTS_Y, 4, UI_INK);
    }
  }
  // ko11.16.1: el estilo de los sprites de combate tambien aqui (antes de empezar)
  bool prg = battleArt();
  drawBtn(RG_ART_X, RG_BACK_Y, RG_ART_W, 44, prg ? C565(0x6a, 0x4c, 0xf0) : UI_WHITE, prg ? UI_WHITE : UI_INK,
          XT(prg ? X_BART_PRG : X_BART_PMD));
  drawBtn(RG_BACK_X, RG_BACK_Y, RG_ART_W, 44, UI_TRACK, UI_INK, T(S_BACK));
  uiFlush();
}

static void regionTurn(int to) {
  if (to < 0 || to > 1 || to == regionPage) return;
  regionPage = (uint8_t)to;
  sfxPlay(SFX_TAP);
}

// deslizar a los lados: pagina (izquierda avanza, como la ficha)
bool regionSwipe(int dir) {
  if (xScreen != XS_REGION) return false;
  regionTurn((int)regionPage + (dir > 0 ? -1 : 1));
  return true;
}

void regionTap(int16_t x, int16_t y) {
  if (inRect(x, y, RG_BACK_X, RG_BACK_Y, RG_ART_W, 44) || (regionPage == 0 && navHit(NAV_L, x, y))) {
    sfxPlay(SFX_TAP);
    goBack();  // ko11.17: a donde estaba (ficha / menu de entrenamiento / principal)
    return;
  }
  if (inRect(x, y, RG_ART_X, RG_BACK_Y, RG_ART_W, 44)) { battleArtToggle(); return; }  // ko11.16.1
  if (y >= RG_ARROW_Y - 40 && y < RG_ARROW_Y + 40) {  // flechas (zona amplia)
    if (x < RG_X - 4) { regionTurn(regionPage - 1); return; }
    if (x >= RG_X + 2 * RG_W + RG_GAPX + 4) { regionTurn(regionPage + 1); return; }
  }
  if (x < RG_X || y < RG_Y) return;
  int cx = (x - RG_X) / (RG_W + RG_GAPX), cy = (y - RG_Y) / (RG_H + RG_GAPY);
  if (cx > 1 || cy > 3) return;
  if ((x - RG_X) % (RG_W + RG_GAPX) >= RG_W || (y - RG_Y) % (RG_H + RG_GAPY) >= RG_H) return;  // hueco
  uint8_t r = (uint8_t)(regionPage * RG_PER_PAGE + cy * 2 + cx);
  if (!regionOpen(r)) { sfxPlay(SFX_DENY); regionMsgUntil = millis() + 1800; return; }
  xScreen = XS_NONE;
  startWildIn(r);
}

// ======================================================================
// ko10.4: gimnasios (8 medallas) y reto del dia
// ======================================================================
static const uint16_t BADGE_COL[GYM_COUNT] = {
  C565(0x9c, 0xa4, 0xb0), C565(0x3a, 0x8c, 0xe0), C565(0xf0, 0x90, 0x30), C565(0x6c, 0xc0, 0x5c),
  C565(0xf0, 0x7a, 0xa8), C565(0xe0, 0xb8, 0x30), C565(0xd8, 0x40, 0x40), C565(0x40, 0xa0, 0x60),
};
#define GY_PER_PAGE 4
#define GY_X 58
#define GY_Y 124
#define GY_W 350
#define GY_H 50
#define GY_GAP 8
#define GY_BACK_Y 378
uint8_t gymPage = 0;
uint32_t gymMsgUntil = 0;
const char *gymMsg = nullptr;

#define GY_PAGES 3  // ko10.11: 2 de gimnasios + la liga
static void renderLeague();
static void leagueTap(int16_t x, int16_t y);
static void gymDots();
static void gymFooter();
bool gymRewardToday(uint8_t i);
void openGyms() {
  retMark();
  trainMenuOpen = false;
  cardOpen = false;
  xScreen = XS_GYM;
  uint8_t nb = badgeCount(pet.badges);
  // la pagina del proximo gimnasio. ko11.27: con las 8 medallas, la 1a (Brock): antes saltaba a
  // la liga y para las revanchas habia que volver atras (la liga sigue a dos paginas)
  gymPage = (nb >= GY_PER_PAGE && nb < GYM_COUNT) ? 1 : 0;
  sfxPlay(SFX_TAP);
}

static void drawBadge(int x, int y, int r, uint8_t i, bool got) {
  if (got) {
    // ko11.20.1: borde oscuro: el gris (roca) ya no parece un hueco vacio
    gfx->fillCircle(x, y, r + 1, UI_INK);
    gfx->fillCircle(x, y, r - 1, BADGE_COL[i]);
    gfx->fillCircle(x - r / 3, y - r / 3, r / 3, UI_WHITE);  // brillo
  } else {
    gfx->drawCircle(x, y, r, UI_TRACK);
  }
}

void renderGyms() {
  screenBase();
  char t[40];
  snprintf(t, sizeof(t), XT(X_GYM_TITLE_FMT), badgeCount(pet.badges));
  drawFit(t, 40, 320, UI_INK, 3);
  for (int i = 0; i < GYM_COUNT; i++) drawBadge(CX - 7 * 17 + i * 34, 96, 11, (uint8_t)i, pet.badges & (1 << i));
  uint8_t next = badgeCount(pet.badges);
  if (gymPage == 2) { renderLeague(); return; }  // ko10.11
  for (int k = 0; k < GY_PER_PAGE; k++) {
    uint8_t i = (uint8_t)(gymPage * GY_PER_PAGE + k);
    int y = GY_Y + k * (GY_H + GY_GAP);
    bool got = pet.badges & (1 << i), open = got || i <= next;
    uint16_t bg = got ? UI_WHITE : open ? C565(0xff, 0xf0, 0xc8) : UI_TRACK;
    uiButton(GY_X, y, GY_W, GY_H, 12, bg, UI_INK);
    drawBadge(GY_X + 26, y + GY_H / 2, 13, i, got);
    const GymDef &g = GYMS[i];
    char l1[48];
    if (got)  // ko10.11: la revancha va a tu nivel
      snprintf(l1, sizeof(l1), XT(X_REMATCH_LV_FMT), XT((XId)(X_LEADER_0 + i)), XT((XId)(X_REG_0 + g.region)),
               (unsigned)(pet.level() + 2 > LEVEL_MAX ? LEVEL_MAX : pet.level() + 2));
    else
      snprintf(l1, sizeof(l1), "%s  %s  Lv%u", XT((XId)(X_LEADER_0 + i)), XT((XId)(X_REG_0 + g.region)), g.lv[g.n - 1]);
    gfx->setTextColor(open ? UI_INK : 0x8410);
    setSize(2);
    setCur(GY_X + 50, y + 6);
    printT(l1);
    char l2[40];
    if (got) snprintf(l2, sizeof(l2), XT(gymRewardToday(i) ? X_REMATCH_FMT : X_REMATCH_DONE_FMT), pet.gymWins[i]);
    else if (open) snprintf(l2, sizeof(l2), "%s", XT(X_GYM_GO));
    else snprintf(l2, sizeof(l2), XT(X_GYM_LOCK_FMT), i);
    gfx->setTextColor(got ? 0x8410 : open ? UI_BAR_BAD : 0x8410);
    setSize(1);
    setCur(GY_X + 50, y + 30);
    printT(l2);
  }
  if (gymMsg && timeLeft(gymMsgUntil)) drawFit(gymMsg, 360, 300, UI_BAR_BAD, 1);
  else
    gymDots();
  gymFooter();
}

bool gymSwipe(int dir) {
  if (xScreen != XS_GYM) return false;
  int p = (int)gymPage + (dir > 0 ? -1 : 1);
  if (p >= 0 && p < GY_PAGES && p != gymPage) { gymPage = (uint8_t)p; gymMsgUntil = 0; sfxPlay(SFX_TAP); }
  return true;
}

// ko11.20: antes de un entrenador, elegir ayudantes de la caja (si hay alguno)
static void partyOpen(uint8_t kind, uint8_t region, const Battler *team, uint8_t n, uint8_t from) {
  if (!box.count()) { ppArmed = false; xScreen = XS_NONE; startTrainer(kind, region, team, n); return; }
  ppKind = kind;
  ppRegion = region;
  ppN = n > CHAMP_TEAM ? CHAMP_TEAM : n;
  for (uint8_t i = 0; i < ppN; i++) ppTeam[i] = team[i];
  ppFrom = from;
  for (uint8_t k = 0; k < PARTY_HELPERS; k++) {  // los de la ultima vez, si siguen en la caja
    int8_t f = -1;
    for (uint8_t i = 0; ppPickDex[k] && i < box.count(); i++)
      if (box.at(i).dex == ppPickDex[k] && box.at(i).epoch == ppPickEpoch[k] && (k == 0 || i != ppPick[0])) { f = (int8_t)i; break; }
    if (f >= 0 && !helperUsesLeft(box.at((uint8_t)f))) f = -1;  // hoy ya no puede
    ppPick[k] = f;
  }
  if (ppPick[0] < 0 && ppPick[1] >= 0) { ppPick[0] = ppPick[1]; ppPick[1] = -1; }
  ppPage = 0;
  xScreen = XS_PARTY;
  sfxPlay(SFX_TAP);
}
void partyStart(bool solo) {
  ppArmed = !solo;
  for (uint8_t k = 0; ppArmed && k < PARTY_HELPERS; k++)  // un uso del dia para cada ayudante
    if (ppPick[k] >= 0 && ppPick[k] < box.count()) helperUse(box.at((uint8_t)ppPick[k]));
  for (uint8_t k = 0; k < PARTY_HELPERS; k++) {
    bool ok = ppPick[k] >= 0 && ppPick[k] < box.count();
    ppPickDex[k] = ok ? box.at((uint8_t)ppPick[k]).dex : 0;
    ppPickEpoch[k] = ok ? box.at((uint8_t)ppPick[k]).epoch : 0;
  }
  xScreen = XS_NONE;
  startTrainer(ppKind, ppRegion, ppTeam, ppN);
  if (xScreen != XS_WILD) xScreen = ppFrom;  // no se pudo (cansado...)
}
void partyCancel() { xScreen = ppFrom; sfxPlay(SFX_TAP); }

void gymTap(int16_t x, int16_t y) {
  if (inRect(x, y, CX - 80, GY_BACK_Y, 160, 40)) { sfxPlay(SFX_TAP); goBack(); return; }  // ko11.17
  // ko10.11: flechas de pagina y la pagina de la liga (ko11.17: en la 1a, [<] = volver)
  if (navHit(NAV_L, x, y)) { if (gymPage > 0) gymSwipe(1); else { sfxPlay(SFX_TAP); goBack(); } return; }
  if (navHit(NAV_R, x, y)) { if (gymPage < GY_PAGES - 1) gymSwipe(-1); return; }
  if (gymPage == 2) { leagueTap(x, y); return; }
  if (x < GY_X || x >= GY_X + GY_W || y < GY_Y) return;
  int k = (y - GY_Y) / (GY_H + GY_GAP);
  if (k >= GY_PER_PAGE || (y - GY_Y) % (GY_H + GY_GAP) >= GY_H) return;
  uint8_t i = (uint8_t)(gymPage * GY_PER_PAGE + k);
  bool got = pet.badges & (1 << i);
  if (!got && i > badgeCount(pet.badges)) {
    sfxPlay(SFX_DENY);
    gymMsg = XT(X_REGION_LOCKED);
    gymMsgUntil = millis() + 1800;
    return;
  }
  if (!pet.canBattle() || pet.tooTiredToBattle()) {
    sfxPlay(SFX_DENY);
    gymMsg = !pet.canBattle() ? XT(X_CANT_NOW) : XT(X_TOO_TIRED);
    gymMsgUntil = millis() + 1800;
    return;
  }
  const GymDef &g = GYMS[i];
  bGym = i;
  xScreen = XS_NONE;
  if (got) {  // ko10.11: revancha: equipo del tipo, al azar y a tu nivel
    Battler team[REMATCH_MAX];
    uint8_t n = gymRematchTeam(i, pet.level(), esp_random(), team);
    partyOpen(BK_GYM, g.region, team, n, XS_GYM);  // ko11.20
    return;
  }
  Battler team[GYM_MAX_TEAM];
  for (uint8_t j = 0; j < g.n; j++) team[j] = makeTrainerMon(g.dex[j], g.lv[j]);
  partyOpen(BK_GYM, g.region, team, g.n, XS_GYM);
}

// ---- ko10.11: revanchas (premio 1 vez al dia por gimnasio) y liga
static uint16_t todayDay16() { return (uint16_t)(pet.lastSeenEpoch / 86400u); }
bool gymRewardToday(uint8_t i) { return pet.lastSeenEpoch && pet.gymDay[i] != todayDay16(); }

static void gymDots() {
  for (int p = 0; p < GY_PAGES; p++) {
    int x = CX - 26 + p * 26;
    if (p == gymPage) gfx->fillCircle(x, 364, 5, UI_INK);
    else gfx->drawCircle(x, 364, 4, UI_INK);
  }
}

static void gymFooter() {
  drawBtn(CX - 80, GY_BACK_Y, 160, 40, UI_TRACK, UI_INK, T(S_BACK));
  drawNav(NAV_L, UI_INK);  // ko11.17: pagina anterior, o volver en la 1a
  if (gymPage < GY_PAGES - 1) drawNav(NAV_R, UI_INK);
  uiFlush();
}

#define LG_BTN_Y 118
#define LG_BTN_H 52
#define LG_ROW_Y 232
#define LG_ROW_H 30
#define LG_ROWS 4
#define LG_FAME_Y 206
#define LG_FAME_H 48
static void renderLeague() {
  bool open = badgeCount(pet.badges) >= GYM_COUNT;
  if (open) drawBtn(GY_X + 20, LG_BTN_Y, GY_W - 40, LG_BTN_H, C565(0xd8, 0xa8, 0x20), UI_WHITE, XT(X_CHAMP_BTN));
  else drawBtn(GY_X + 20, LG_BTN_Y, GY_W - 40, LG_BTN_H, UI_TRACK, 0x8410, XT(X_CHAMP_LOCK));
  char w[48];
  snprintf(w, sizeof(w), XT(X_CHAMP_WINS_FMT), pet.champWins);
  if (pet.champWins) {  // ko11.6.1: + racha
    size_t k = strlen(w);
    snprintf(w + k, sizeof(w) - k, "  ");
    k = strlen(w);
    snprintf(w + k, sizeof(w) - k, XT(X_STREAK_FMT), (unsigned)pet.champStreak, (unsigned)pet.champBest);
  }
  drawFit(w, 178, 330, UI_INK, 2);
  // ko11.1: el salon de la fama es una pantalla aparte (antes lista pequena que se pisaba)
  uint8_t n = fame.count();
  snprintf(w, sizeof(w), XT(X_FAME_BTN_FMT), n);
  drawBtn(GY_X + 20, LG_FAME_Y, GY_W - 40, LG_FAME_H, n ? C565(0x7a, 0x4a, 0xa8) : UI_TRACK, n ? UI_WHITE : 0x8410, w);
  if (!n) drawFit(XT(X_FAME_EMPTY), LG_FAME_Y + LG_FAME_H + 16, 300, 0x8410, 1);
  else {  // el ultimo campeon, en una linea
    const BoxMon &m = fame.at((uint8_t)(n - 1));
    char l[64], when[12];
    when[0] = 0;
    if (m.epoch) {
      uint8_t mo, dd;
      wxDate(m.epoch, nullptr, &mo, &dd, nullptr);
      snprintf(when, sizeof(when), "%u/%u", mo, dd);
    }
    char nm[40];
    snprintf(nm, sizeof(nm), "%s%s Lv%u %s", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex), m.lvl, when);
    snprintf(l, sizeof(l), XT(X_FAME_LAST_FMT), nm);
    drawFit(l, LG_FAME_Y + LG_FAME_H + 14, 320, UI_INK, 2);
  }
  if (gymMsg && timeLeft(gymMsgUntil)) drawFit(gymMsg, 340, 300, UI_BAR_BAD, 1);
  gymDots();
  gymFooter();
}

static void leagueTap(int16_t x, int16_t y) {
  if (inRect(x, y, GY_X + 20, LG_FAME_Y, GY_W - 40, LG_FAME_H)) {  // ko11.1: salon de la fama
    if (fame.count()) openFame();
    else sfxPlay(SFX_DENY);
    return;
  }
  if (!inRect(x, y, GY_X + 20, LG_BTN_Y, GY_W - 40, LG_BTN_H)) return;
  if (badgeCount(pet.badges) < GYM_COUNT) {
    sfxPlay(SFX_DENY); gymMsg = XT(X_CHAMP_LOCK); gymMsgUntil = millis() + 1800; return;
  }
  if (!pet.canBattle() || pet.tooTiredToBattle()) {
    sfxPlay(SFX_DENY);
    gymMsg = !pet.canBattle() ? XT(X_CANT_NOW) : XT(X_TOO_TIRED);
    gymMsgUntil = millis() + 1800;
    return;
  }
  Battler team[CHAMP_TEAM];
  championTeam(pet.level(), esp_random(), team);
  xScreen = XS_NONE;
  partyOpen(BK_CHAMP, CHAMP_REGION, team, CHAMP_TEAM, XS_GYM);
}

// ---- reto del dia
void openDaily() {
  retMark();
  trainMenuOpen = false;
  cardOpen = false;
  xScreen = XS_DAILY;
  sfxPlay(SFX_TAP);
}

static uint32_t todayNum() { return pet.lastSeenEpoch / 86400u; }

void renderDaily() {
  screenBase();
  drawFit(XT(X_DAILY_BTN), 40, 320, UI_INK, 3);
  uint32_t day = todayNum();
  if (!day) {
    drawFit(XT(X_DAILY_NOCLOCK), 200, 320, UI_BAR_BAD, 2);
  } else {
    uint8_t reg = dailyRegion(day);
    char l[48];
    txFmt(l, sizeof(l), X_DAILY_INFO, XT((XId)(X_REG_0 + reg)), nullptr);
    drawFit(l, 90, 320, UI_INK, 2);
    // los tres rivales de hoy (en miniatura)
    Battler team[DAILY_TEAM];
    dailyTeam(day, pet.level(), team);
    gfx->fillRoundRect(73, 124, 320, 104, 14, BIOME_SOIL[reg]);
    for (int i = 0; i < DAILY_TEAM; i++) {
      int cx = 126 + i * 107;
      const uint8_t *th = thumbs.get(team[i].dex);
      if (th) drawThumb(th, cx - 40, 132, 2, false);  // 40 px x2
      char lv[8];
      snprintf(lv, sizeof(lv), "Lv%u", team[i].lvl);
      gfx->setTextColor(UI_INK);
      setSize(1);
      setCur(cx - textW(lv, 1) / 2, 206);
      printT(lv);
    }
    bool done = pet.dailyDoneDay == day;
    drawFit(done ? XT(X_DAILY_DONE_TODAY) : XT(X_DAILY_REWARD), 242, 330, done ? UI_BAR_OK : UI_INK, 2);
    char c[32];
    snprintf(c, sizeof(c), XT(X_DAILY_COUNT_FMT), pet.dailyClears);
    drawFit(c, 272, 300, 0x8410, 1);
    drawBtn(CX - 90, 300, 180, 50, UI_BAR_BAD, UI_WHITE, XT(X_GYM_GO));
  }
  if (gymMsg && timeLeft(gymMsgUntil)) drawFit(gymMsg, 356, 300, UI_BAR_BAD, 1);
  drawBtn(CX - 80, GY_BACK_Y, 160, 40, UI_TRACK, UI_INK, T(S_BACK));
  drawNav(NAV_L, UI_INK);  // ko11.17: volver
  uiFlush();
}

void dailyTap(int16_t x, int16_t y) {
  if (inRect(x, y, CX - 80, GY_BACK_Y, 160, 40) || navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); goBack(); return; }  // ko11.17
  uint32_t day = todayNum();
  if (!day || !inRect(x, y, CX - 90, 300, 180, 50)) return;
  if (!pet.canBattle() || pet.tooTiredToBattle()) {
    sfxPlay(SFX_DENY);
    gymMsg = !pet.canBattle() ? XT(X_CANT_NOW) : XT(X_TOO_TIRED);
    gymMsgUntil = millis() + 1800;
    return;
  }
  Battler team[DAILY_TEAM];
  dailyTeam(day, pet.level(), team);
  xScreen = XS_NONE;
  partyOpen(BK_DAILY, dailyRegion(day), team, DAILY_TEAM, XS_DAILY);
}

void endBattleScreen() {
  foePmd.unload();
  partyEnd();  // ko11.20
  xScreen = XS_NONE;
}

void wildTap(int16_t x, int16_t y) {
  if (autoLeft) {  // ko11.19: cualquier toque para el automatico
    autoLeft = 0;
    strncpy(bvL1, XT(X_AUTO_STOP), sizeof(bvL1) - 1); bvL2[0] = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  if (bPhase == BP_INTRO) { bPhaseT = 0; return; }  // saltar la intro
  // ko9.2: tocar el resultado pasa ya a la pregunta
  if (bPhase == BP_RESULT && millis() - bPhaseT > 800) { afterResult(); return; }
  if (bPhase == BP_DUP) { wildDupTap(x, y); return; }
  if (bPhase == BP_JOIN) { wildJoinTap(x, y); return; }  // ko11.8
  if (bPhase == BP_NEXT) { wildNextTap(x, y); return; }
  if (bPhase == BP_SWAP) { swapTap(x, y); return; }  // ko11.20
  if (bPhase != BP_MENU) return;
  // ko11.23.1: historia / expedicion: [◀] dos veces = salir del combate (sin premio ni derrota)
  if ((bKind == BK_STORY || bKind == BK_ROGUE) && navHit(NAV_L, x, y)) {
    if (bQuitArm && millis() - bQuitArm < 3000) {
      bQuitArm = 0;
      uint8_t k = bKind;
      endBattleScreen();
      storyBattleQuit(k);
    } else {
      bQuitArm = millis() ? millis() : 1;
      sfxPlay(SFX_DENY);
    }
    return;
  }
  bQuitArm = 0;
  if (battleArtTap(x, y)) return;  // ko11.16: PMD <-> PokeRogue
  if (pN > 1 && bKind != BK_WILD && !bLink && inRect(x, y, 236, 176, 176, 58) && partyOtherAlive()) {  // ko11.20
    sfxPlay(SFX_TAP);
    partyAskSwap(2);
    return;
  }
  int a = battleMenuHit(x, y);
  if (a < 0) return;
  if (autoAllowed() && a == BA_BALL) {  // ko11.19: [자동]
    if (autoCount > autoRemaining() || autoCount < 1) autoCount = autoRemaining();
    autoLeft = autoCount;
    autoMenuT = millis();
    sfxPlay(SFX_MEDAL);
    return;
  }
  if (autoAllowed() && a == BA_RUN) {  // ko11.19: [N마리] 1 -> 2 -> ... -> los que quedan
    autoCount = autoCount >= autoRemaining() ? 1 : autoCount + 1;
    sfxPlay(SFX_TAP);
    return;
  }
  battleDoAction(a);
}

// ko11.19: un turno con la accion a (toque o combate automatico)
static void battleDoAction(int a) {
  // fork KO (ko4): los objetos se gastan al elegirlos; sin existencias no hay turno
  if (bKind != BK_WILD && (a == BA_BALL || a == BA_RUN)) {  // ko10.4: entrenador
    strncpy(bvL1, XT(X_TRAINER_NO), sizeof(bvL1) - 1); bvL2[0] = 0; sfxPlay(SFX_DENY); return;
  }
  if (a == BA_POTION && !pet.usePotion()) {
    strncpy(bvL1, XT(X_NO_POTION), sizeof(bvL1) - 1); bvL2[0] = 0; sfxPlay(SFX_DENY); return;
  }
  if (a == BA_BALL && !pet.useBall()) {
    strncpy(bvL1, XT(X_NO_BALL), sizeof(bvL1) - 1); bvL2[0] = 0; sfxPlay(SFX_DENY); return;
  }
  sfxPlay(SFX_TAP);
  BAct foeAct = foeMoveRule(battleAi(bFoe, bMe, bRng, 35));  // el salvaje es algo torpe
  if (a == BA_POTION && autoLeft) autoPotions++;
  bqN = battleTurn(bMe, bFoe, (BAct)a, foeAct, bRng, bq, BATTLE_MAX_EVENTS, true);
  bqAisMe = true;
  bPhase = BP_PLAY;
  startEvent(0);
}

// ko10.4: ya tengo esa especie (en la caja o criandola)?
bool ownsSpecies(int16_t dex) {
  if (!pet.isEgg() && pet.speciesId == dex) return true;
  for (uint8_t i = 0; i < box.count(); i++)
    if (box.at(i).dex == dex) return true;
  return false;
}

// tras el resultado: el repetido (si lo hay) y luego "seguir?"
// ko11.20.1: al acabar un combate de equipo vuelve a luchar el que crias (antes, al
// volver a la lista de gimnasios no se reiniciaba y en el siguiente salvaje salia
// el ultimo ayudante con el nombre de tu Pokemon)
static void partyEnd() {
  helperPmd.unload();
  helperPmdDex = 0;
  pN = 1; pCur = 0; pUsed = 1; pShiny = 0;
  pSlot0Pet = true;
  gStoryFlyAt = nullptr;  // ko11.23.3
}

static void afterResult() {
  // ko10.11: tras un gimnasio se vuelve a la lista de gimnasios (en la pagina de
  // ese gimnasio, con la medalla nueva a la vista) y tras el reto del dia, a su
  // pantalla. Antes volvia a la principal y habia que entrar otra vez
  if (bKind == BK_ROGUE) rogueSaveParty();  // ko11.21: antes de deshacer el equipo
  if (bKind != BK_WILD) partyEnd();
  if (bKind == BK_GYM) {
    foePmd.unload();
    openGyms();
    gymPage = bGym / GY_PER_PAGE;
    return;
  }
  if (bKind == BK_DAILY) { foePmd.unload(); openDaily(); return; }
  if (bKind == BK_STORY) { foePmd.unload(); storyAfterBattle(bWon); return; }  // ko11.21
  if (bKind == BK_ROGUE) { foePmd.unload(); rogueAfterBattle(bWon); return; }
  if (bKind == BK_CHAMP) { foePmd.unload(); openGyms(); gymPage = 2; return; }  // ko10.11
  bPhase = bJoinPending ? BP_JOIN : bDupPending ? BP_DUP : BP_NEXT;  // ko11.8
  bPhaseT = millis();
}

static void dupDecide(bool keep) {
  if (!bDupPending) return;
  char fam[40];
  snprintf(fam, sizeof(fam), "%s", dexName(DEX_FAM[bFoe.dex]));
  uint16_t got;
  if (keep) {
    if (box.full()) { sfxPlay(SFX_DENY); return; }  // caja llena: solo caramelos
    box.add(bFoe.dex, bFoe.lvl, bvFoeShiny, bDupCaught, bDupEpoch);
    got = CANDY_KEEP;
  } else {
    got = Pet::dupCandy(bvFoeShiny, bFoe.lvl);
  }
  pet.addCandy(bFoe.dex, got);
  pet.saveNow();
  char n[8];
  snprintf(n, sizeof(n), "%u", got);
  char tot[8];
  snprintf(tot, sizeof(tot), "%u", pet.candyOf(bFoe.dex));
  txFmt(bNote, sizeof(bNote), X_CANDY_GOT, fam, n);
  size_t l = strlen(bNote);
  if (l + 2 < sizeof(bNote)) snprintf(bNote + l, sizeof(bNote) - l, " (%s)", tot);
  bDupPending = false;
  sfxPlay(keep ? SFX_TAP : SFX_MEDAL);
  bPhase = BP_NEXT;
  bPhaseT = millis();
}

static void wildDupTap(int16_t x, int16_t y) {
  if (y < BN_Y || y >= BN_Y + BN_H) return;
  if (x >= BDUP_KEEP_X && x < BDUP_KEEP_X + BDUP_W) dupDecide(true);
  else if (x >= BDUP_CANDY_X && x < BDUP_CANDY_X + BDUP_W) dupDecide(false);
}

// ko11.8: el vencido quiere venir: [데려가기] / [보내주기]
static void joinDecide(bool take) {
  if (!bJoinPending) return;
  bJoinPending = false;
  if (!take) {
    txFmt(bNote, sizeof(bNote), X_JOIN_BYE, dexName(bFoe.dex));
    sfxPlay(SFX_TAP);
    bPhase = BP_NEXT;
    bPhaseT = millis();
    return;
  }
  if (ownsSpecies(bFoe.dex)) {  // repetido: como siempre, caja o caramelos
    bDupPending = true;
    bDupCaught = false;
    bPhase = BP_DUP;
    bPhaseT = millis();
    sfxPlay(SFX_TAP);
    return;
  }
  if (box.add(bFoe.dex, bFoe.lvl, bvFoeShiny, false, bDupEpoch)) {
    txFmt(bNote, sizeof(bNote), X_JOIN_OK, dexName(bFoe.dex));
    sfxPlay(SFX_MEDAL);
  } else {
    strncpy(bNote, XT(X_BOX_FULL), sizeof(bNote) - 1);
    bNote[sizeof(bNote) - 1] = 0;
    sfxPlay(SFX_DENY);
  }
  bPhase = BP_NEXT;
  bPhaseT = millis();
}

static void wildJoinTap(int16_t x, int16_t y) {
  if (y < BN_Y || y >= BN_Y + BN_H) return;
  if (x >= BDUP_KEEP_X && x < BDUP_KEEP_X + BDUP_W) joinDecide(true);
  else if (x >= BDUP_CANDY_X && x < BDUP_CANDY_X + BDUP_W) joinDecide(false);
}

void finishBattle(bool won, bool fled, bool caught) {
  autoLeft = 0;  // ko11.19
  bWon = won;
  bFled = fled;
  bCaught = caught;
  bPhase = BP_RESULT;
  bPhaseT = millis();
  if (!bRewarded && won && bKind != BK_WILD && !bLink && pN > 1) {  // ko11.20: los ayudantes que lucharon +1 nivel
    uint8_t up = 0;
    for (uint8_t j = 1; j < pN; j++)
      if ((pUsed >> j) & 1 && pBox[j] >= 0 && box.bumpLevel((uint8_t)pBox[j], LEVEL_MAX)) up++;
    if (up) snprintf(bPartyNote, sizeof(bPartyNote), XT(X_PT_LVUP_FMT), up);
  }
  if (!bRewarded) {
    bRewarded = true;
    bool wildExp = bKind == BK_WILD && !bLink && bExpDex;  // ko11
    pet.battleResult(bLink ? BATTLE_LINK : BATTLE_WILD, won, fled, caught, wildExp ? bExpDex : bFoe.dex,
                     wildExp ? bExpLvl : bFoe.lvl);
    if (won && bKind == BK_WILD && !bLink) bItems = pet.wildWinItems();  // ko11.1: por probabilidad
    if (pCur == 0 && pSlot0Pet) bvMeLvl = pet.level();  // fork KO (ko7): la caja de vida ensena el nivel nuevo (ko11.20: si lucha el que crias)
    sfxPlay(pet.lastLvlUp ? SFX_LEVEL : won || caught ? SFX_MEDAL : SFX_BYE);  // ko7: subida de nivel
    // fork KO: el capturado va siempre a la caja; el vencido, solo a veces
    // (ko5: 1 de cada 5 "quiere unirse"; si no, la pokeball no servia de nada)
    bool joins = won && bKind == BK_WILD && (uint32_t)random(100) < BOX_JOIN_PCT;
    bNote[0] = 0;
    // ko10.11: a veces, un caramelo universal (salvaje ganado o capturado)
    if (bKind == BK_WILD && !bLink && (won || caught) && (uint32_t)random(100) < RARE_CANDY_PCT) {
      if (pet.rareCandy < 999) pet.rareCandy++;
      strncpy(bNote, XT(X_RARE_CANDY_GOT), sizeof(bNote) - 1);
      bNote[sizeof(bNote) - 1] = 0;
      pet.saveNow();
    }
    // ko11.16: a veces un orbe del tipo del rival (con aviso)
    if (bKind == BK_WILD && !bLink && (won || caught) && (uint32_t)random(100) < ORB_WILD_PCT)
      orbDrop(DEX_TBL[bFoe.dex].ptype);
    if (won && bKind == BK_GYM && !(pet.badges & (1 << bGym))) {  // ko10.4: medalla nueva
      pet.badges |= (uint8_t)(1 << bGym);
      char nb[4];
      snprintf(nb, sizeof(nb), "%u", badgeCount(pet.badges));
      txFmt(bNote, sizeof(bNote), X_BADGE_GOT, XT((XId)(X_BADGE_0 + bGym)), nb);
      pet.saveNow();
      sfxPlay(SFX_EVOLVE);
    } else if (won && bKind == BK_GYM) {  // ko10.11: revancha ganada
      if (pet.gymWins[bGym] < 255) pet.gymWins[bGym]++;
      if (gymRewardToday(bGym)) {
        pet.gymDay[bGym] = todayDay16();
        pet.giveItems(1, 1);
        pet.addCandy(pet.speciesId, 2);
        strncpy(bNote, XT(X_REMATCH_REWARD), sizeof(bNote) - 1);
        if ((uint32_t)random(100) < ORB_GYM_PCT) orbDrop(DEX_TBL[bFoe.dex].ptype);  // ko11.16
      } else {
        snprintf(bNote, sizeof(bNote), XT(X_REMATCH_NOREWARD_FMT), pet.gymWins[bGym]);
      }
      bNote[sizeof(bNote) - 1] = 0;
      pet.saveNow();
    } else if (won && bKind == BK_CHAMP) {  // ko10.11: campeon: al salon de la fama de la liga
      if (pet.champWins < 65535) pet.champWins++;
      if (pet.champStreak < 65535) pet.champStreak++;  // ko11.6.1: racha
      if (pet.champStreak > pet.champBest) pet.champBest = pet.champStreak;
      pet.giveItems(3, 3);  // ko11.1: antes +5/+5
      pet.addCandy(pet.speciesId, 10);
      // ko11.20: una ficha por Pokemon; con ayudantes que llegaron a luchar = en equipo
      bool teamWin = pUsed & (uint8_t)~1u;
      int ci = fameCardOfPet();
      bool promoted = false;
      if (ci < 0) {
        if (fame.full()) {  // lleno: se va el mas antiguo (y su racha con el)
          fame.release(0);
          memmove(pet.fameStreak, pet.fameStreak + 1, sizeof(pet.fameStreak) - 1);
          pet.fameStreak[sizeof(pet.fameStreak) - 1] = 0;
        }
        // ko11.1: con sus genes (la ficha del salon los ensena)
        fame.addRaised(pet.speciesId, pet.level(), pet.shiny, pet.geneAtk, pet.geneDef, pet.geneSpe, clockEpoch());
        ci = fame.count() - 1;
        FameRec *r = frGetOrAdd(fame.at((uint8_t)ci));
        r->solo = r->team = 0;
      }
      BoxMon card = fame.at((uint8_t)ci);
      FameRec *r = frGetOrAdd(card);
      bool wasSilver = r->solo == 0 && r->team > 0;
      if (teamWin) {
        if (r->team < 65535) r->team++;
        uint8_t k = 0;
        r->help[0] = r->help[1] = 0;
        r->shiny = 0;
        for (uint8_t j = 1; j < pN && k < PARTY_HELPERS; j++)
          if ((pUsed >> j) & 1) { if ((pShiny >> j) & 1) r->shiny |= (uint8_t)(1 << k); r->help[k++] = pMon[j].dex; }
      } else {
        if (r->solo < 65535) r->solo++;
        promoted = wasSilver;
      }
      card.dex = pet.speciesId;  // forma y nivel de ahora
      card.lvl = pet.level();
      if (pet.shiny) card.flags |= BOXF_SHINY;
      card.flags = (uint8_t)(r->solo ? (card.flags & ~BOXF_TEAM) : (card.flags | BOXF_TEAM));
      fame.set((uint8_t)ci, card);
      frSave();
      if (ci < (int)sizeof(pet.fameStreak) && pet.champStreak > pet.fameStreak[ci])  // su mejor racha
        pet.fameStreak[ci] = (uint8_t)(pet.champStreak > 255 ? 255 : pet.champStreak);
      snprintf(bNote, sizeof(bNote), XT(X_CHAMP_WIN), (unsigned)pet.champStreak);
      if (promoted) snprintf(bPartyNote, sizeof(bPartyNote), "%s", XT(X_FAME_PROMOTE));  // ko11.20: plata -> oro
      bakRequest();  // ko11.6: copia en la SD
      bNote[sizeof(bNote) - 1] = 0;
      pet.saveNow();
      sfxPlay(SFX_EVOLVE);
    } else if (!won && bKind == BK_CHAMP) {  // ko11.6.1: perder en la liga corta la racha (medallas intactas)
      if (pet.champStreak) {
        snprintf(bNote, sizeof(bNote), XT(X_STREAK_LOST_FMT), (unsigned)pet.champStreak);
        pet.champStreak = 0;
        pet.saveNow();
      }
    } else if (won && bKind == BK_DAILY) {
      uint32_t day = pet.lastSeenEpoch / 86400u;
      if (day && pet.dailyDoneDay != day) {  // el premio, una vez al dia
        pet.dailyDoneDay = day;
        pet.dailyClears++;
        pet.giveItems(2, 2);  // ko11.1: antes +3/+3
        pet.addCandy(pet.speciesId, 3);
        strncpy(bNote, XT(X_DAILY_WIN), sizeof(bNote) - 1);
        bNote[sizeof(bNote) - 1] = 0;
        pet.saveNow();
      }
    }
    if (!bLink && joins && !caught) {  // ko11.8: ya no entra solo: se pregunta tras el resultado
      bJoinPending = true;
      bDupEpoch = clockEpoch();
    }
    if (!bLink && caught) {
      uint32_t e = clockEpoch();
      dexLog.caught(bFoe.dex, e);
      if (ownsSpecies(bFoe.dex)) {  // ko10.4: repetido -> se pregunta tras el resultado
        bDupPending = true;
        bDupCaught = caught;
        bDupEpoch = e;
      } else {
        bBoxMsg = !box.add(bFoe.dex, bFoe.lvl, bvFoeShiny, caught, e) ? X_BOX_FULL
                  : caught ? X_TO_BOX : X_JOINED;
      }
    }
  }
}

// avanza la reproduccion de eventos; devuelve true cuando se acabaron
bool stepEvents() {
  if (bqI >= bqN) return true;
  if (millis() - bqT >= evDur(bq[bqI])) {
    startEvent(bqI + 1);  // al pasar del ultimo tambien marca su desmayo
    if (bqI >= bqN) return true;
  }
  return false;
}

void updateWild() {
  uint32_t now = millis();
  if (autoLeft && bPhase == BP_MENU && now - autoMenuT > AUTO_STEP_MS) {  // ko11.19
    battleDoAction(autoPick());
    return;
  }
  if (autoLeft && bPhase == BP_SWAP && now - autoMenuT > AUTO_STEP_MS) {  // ko11.20: elige solo
    int8_t j = partyBest(bSwapMode == 0);
    if (j >= 0) partySwitchTo((uint8_t)j);
    else txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName), bvL2[0] = 0;
    bPhase = BP_MENU;
    autoMenuT = now;
    return;
  }
  if (bPhase == BP_INTRO) {
    if (bPhaseT == 0 || now - bPhaseT > 2200) {
      bPhase = BP_MENU;
      autoMenuT = now;
      txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
      bvL2[0] = 0;
      // ko11.20: sale otro del rival: como en PokeRogue, "¿cambiar?"
      if (bKind != BK_WILD && !bLink && bTeamI > 0 && partyOtherAlive()) partyAskSwap(1);
    }
  } else if (bPhase == BP_PLAY) {
    if (stepEvents()) {
      bool fled = false, caught = false;
      for (int i = 0; i < bqN; i++) {
        if (bq[i].kind == EV_RUN_OK) fled = true;
        if (bq[i].kind == EV_CATCH) caught = true;
      }
      if (caught) finishBattle(false, false, true);
      else if (fled) finishBattle(false, true, false);
      else if (bMe.hp == 0) {  // ko11.20: si queda alguien del equipo, sale otro
        if (bKind != BK_WILD && !bLink && partyOtherAlive()) { pMon[pCur] = bMe; partyAskSwap(0); }
        else finishBattle(false, false, false);
      }
      else if (bFoe.hp == 0) { if (!nextTrainerMon()) finishBattle(true, false, false); }
      else {
        bPhase = BP_MENU;
        autoMenuT = now;
        txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
        bvL2[0] = 0;
      }
    }
  } else if (bPhase == BP_RESULT) {
    if (now - bPhaseT > 3800) afterResult();  // ko9.2: preguntar (ko10.4: antes el repetido)
  } else if (bPhase == BP_DUP) {
    if (now - bPhaseT > BD_MS) { if (!box.full()) dupDecide(true); else dupDecide(false); }
  } else if (bPhase == BP_JOIN) {  // ko11.8: sin elegir en 20 s, viene (como antes)
    if (now - bPhaseT > BD_MS) joinDecide(true);
  } else if (bPhase == BP_NEXT) {
    if (now - bPhaseT > BN_MS) endBattleScreen();
  }
}

// ko9.2: tras un salvaje, seguir con otro al azar o volver
static void wildNextTap(int16_t x, int16_t y) {
  if (y < BN_Y || y >= BN_Y + BN_H) return;
  if (x >= 83 && x < 229) {            // seguir
    if (!pet.canBattle() || pet.tooTiredToBattle()) {
      endBattleScreen();
      battleAllowed(true);             // aviso en la pantalla principal (cansado...)
      return;
    }
    foePmd.unload();
    startWildIn(bRegion);              // ko10.1: sigue en la misma region
  } else if (x >= 237 && x < 383) {    // salir
    sfxPlay(SFX_TAP);
    endBattleScreen();
  }
}

// aviso de encuentro en la pantalla principal (mismo sitio que los otros CTA)
bool wildAlertActive() { return timeLeft(wildAlertUntil) > 0 && pet.canBattle(); }

void drawWildAlert() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.008f));
  int x = FAR_BTN_X + 40 - p, y = FAR_BTN_Y - p, w = FAR_BTN_W - 80 + 2 * p, h = FAR_BTN_H + 2 * p;
  uiButton(x, y, w, h, 16, C565(0x2e, 0x7d, 0x32), UI_WHITE);
  drawFit(XT(X_WILD_ALERT), y + h / 2 - 8, w - 16, UI_WHITE, 2);
}

bool wildAlertTap(int16_t x, int16_t y) {
  if (!wildAlertActive()) return false;
  if (x >= FAR_BTN_X + 40 && x <= FAR_BTN_X + FAR_BTN_W - 40 && y >= FAR_BTN_Y &&
      y <= FAR_BTN_Y + FAR_BTN_H) {
    startWild();
    return true;
  }
  return false;
}

// de vez en cuando aparece un salvaje (solo despierto, con energia y sin otro aviso)
void rollWildEncounter(uint32_t now) {
  if (now < wildNextRoll) return;
  wildNextRoll = now + 60000;
  if (timeLeft(wildAlertUntil) || xScreen != XS_NONE) return;
  if (!pet.canBattle() || pet.energy < 30 || screenOff) return;
  if (pet.wantEvolveButton() || pet.canRunawayNow() || pet.wantFarewellButton()) return;
  if (random(100) < 3) {  // ~1 vez cada media hora-hora despierto
    wildAlertUntil = now + WILD_ALERT_MS;
    sfxPlay(SFX_HEART);
    vibPulse(200, 3, 150);  // ko11.25: un salvaje (tres toques)
  }
}

// ======================================================================
// Tongsin: menu, busqueda, batalla automatica, intercambio
// ======================================================================

uint32_t linkEndAt = 0;        // cierre programado (mensaje final)
const char *linkEndMsg = nullptr;
bool linkBattleStarted = false;
bool linkTradeApplied = false;

#define LM_BTN_X 103
#define LM_BTN_W 260
#define LM_BATTLE_Y 150
#define LM_TRADE_Y 222
#define LM_BTN_H 60

void openLinkMenu() {
  retMark();
  trainMenuOpen = false;
  cardOpen = false;
  xScreen = XS_LINKMENU;
}

void myLinkPet(LinkPet &lp) {
  memset(&lp, 0, sizeof(lp));
  pet.exportTrade(lp.t);
  lp.lvl = pet.level();
  lp.atk = pet.atkStat();
  lp.def = pet.defStat();
  lp.spe = pet.speStat();
}

void renderLinkMenu() {
  screenBase();
  drawFit(XT(X_LINK_TITLE), 44, 300, UI_INK, 3);
  bool ok = pet.canBattle() && !netBusy();
  drawBtn(LM_BTN_X, LM_BATTLE_Y, LM_BTN_W, LM_BTN_H, ok ? UI_BAR_BAD : UI_TRACK, UI_WHITE, XT(X_LINK_BATTLE));
  drawBtn(LM_BTN_X, LM_TRADE_Y, LM_BTN_W, LM_BTN_H, ok ? 0x4C98 : UI_TRACK, UI_WHITE, XT(X_LINK_TRADE));
  if (!pet.canBattle()) drawFit(XT(X_CANT_NOW), 104, 340, UI_BAR_BAD, 2);
  else if (netBusy()) drawFit(XT(X_ST_BUSY), 104, 340, UI_BAR_BAD, 2);
  else drawFit(XT(X_LINK_HINT), 104, 340, UI_INK, 2);
  char rec[48];
  snprintf(rec, sizeof(rec), XT(X_RECORD_FMT), pet.wildWins, pet.linkWins, pet.linkBattles, pet.trades);
  drawFit(rec, 306, 340, UI_INK, 2);
  drawFit(XT(X_TAP_CLOSE), 400, 300, UI_INK, 2);
  drawNav(NAV_L, UI_INK);  // ko11.17: volver
  uiFlush();
}

void linkMenuTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y) || y < 72 || y > 380) { sfxPlay(SFX_TAP); goBack(); return; }  // ko11.17
  LinkMode m = LINK_NONE;
  if (inRect(x, y, LM_BTN_X, LM_BATTLE_Y, LM_BTN_W, LM_BTN_H)) m = LINK_BATTLE;
  else if (inRect(x, y, LM_BTN_X, LM_TRADE_Y, LM_BTN_W, LM_BTN_H)) m = LINK_TRADE;
  if (m == LINK_NONE) return;
  if (!pet.canBattle() || netBusy()) { sfxPlay(SFX_DENY); return; }
  if (m == LINK_BATTLE && pet.tooTiredToBattle()) { showToast(XT(X_TOO_TIRED)); sfxPlay(SFX_DENY); return; }
  sfxPlay(SFX_TAP);
  LinkPet lp;
  myLinkPet(lp);
  linkSetClock(clockEpoch(), gClockTrusted);  // ko10.4: la hora viaja en cada mensaje
  linkStart(m, lp);
  linkEndAt = 0;
  linkEndMsg = nullptr;
  linkBattleStarted = false;
  linkTradeApplied = false;
  xScreen = XS_LINK;
}

void closeLink() {
  linkStop();
  foePmd.unload();
  xScreen = XS_NONE;
}

void linkFinish(const char *msg, uint32_t ms) {
  if (linkEndAt) return;
  linkEndMsg = msg;
  linkEndAt = millis() + ms;
}

const char *partnerName(char *buf, size_t n) {
  const TradePet &t = linkPartner().t;
  char nk[sizeof(t.nick) + 1];
  memcpy(nk, t.nick, sizeof(t.nick));
  nk[sizeof(t.nick)] = 0;
  const char *base = nk[0] ? nk : dexName(t.dex);
  snprintf(buf, n, "%s%s", t.shiny ? "*" : "", base);
  return buf;
}

void startLinkBattle() {
  partyEnd();  // ko11.20.1
  const LinkPet &th = linkPartner();
  if (th.t.dex < 1 || th.t.dex > DEX_COUNT) { linkFinish(XT(X_LINK_LOST), 2500); return; }
  // la instantanea que se envio, no las stats de ahora: si el nivel cambiara
  // entre el emparejamiento y aqui, las dos placas simularian batallas distintas
  const LinkPet &mine = linkMine();
  Battler me = makeBattler(mine.t.dex, mine.lvl, mine.atk, mine.def, mine.spe);
  Battler foe = makeBattler(th.t.dex, th.lvl, th.atk, th.def, th.spe);
  bool iA = linkIAmA();
  int n = 0;
  uint8_t winner = iA ? battleAuto(me, foe, linkSeed(), bq, BQ_MAX, &n)
                      : battleAuto(foe, me, linkSeed(), bq, BQ_MAX, &n);
  char pn[40];
  partnerName(pn, sizeof(pn));
  bvSetup(me, foe, pn[0] == '*' ? pn + 1 : pn, th.t.shiny);
  bqAisMe = iA;
  bqN = n;
  bLink = true;
  bWon = (winner == 0) == iA;
  snprintf(bvL1, sizeof(bvL1), "%s  VS  %s", bvMeName, bvFoeName);
  bPhase = BP_INTRO;
  bPhaseT = millis();
  linkBattleStarted = true;
  sfxPlay(SFX_MEDAL);
  audioSetBattleMusic(false, true);
  audioCry(th.t.dex);
}

void updateLinkBattle() {
  uint32_t now = millis();
  if (bPhase == BP_INTRO) {
    if (now - bPhaseT > 2500) { bPhase = BP_PLAY; startEvent(0); }
  } else if (bPhase == BP_PLAY) {
    if (stepEvents()) finishBattle(bWon, false, false);
  } else if (bPhase == BP_RESULT) {
    if (now - bPhaseT > 3800) closeLink();
  }
}

void renderLinkSearch(const char *title) {
  screenBase();
  drawFit(title, 44, 300, UI_INK, 3);
  if (pmd.loaded) drawPmdAct(PMD_IDLE, CX, 270, millis(), true, false, 4);
  int ph = (millis() / 250) % 4;
  for (int i = 0; i < 3; i++) gfx->fillCircle(CX - 24 + i * 24, 312, 6, i < ph ? UI_BAR_BAD : UI_TRACK);
  drawFit(XT(X_LINK_SEARCH), 336, 340, UI_INK, 2);
  drawFit(XT(X_LINK_HINT), 100, 340, UI_INK, 2);
  drawFit(XT(X_TAP_CLOSE), 400, 300, UI_INK, 2);
}

#define LT_YES_X 103
#define LT_NO_X 243
#define LT_BTN_Y 320
#define LT_BTN_W 120
#define LT_BTN_H 50

void renderLinkTrade() {
  LinkState st = linkState();
  screenBase();
  drawFit(XT(X_LINK_TRADE), 36, 300, UI_INK, 3);
  const LinkPet &th = linkPartner();
  char pn[40], l[80];
  partnerName(pn, sizeof(pn));
  snprintf(l, sizeof(l), XT(X_PARTNER_FMT), pn, th.lvl);
  drawFit(l, 76, 340, DEX_TBL[th.t.dex >= 1 && th.t.dex <= DEX_COUNT ? th.t.dex : 0].accent, 2);
  // sprite del Pokemon que llegaria
  loadFoe(th.t.dex, th.t.shiny);
  if (foePmd.loaded) drawPmdActM(foePmd, PMD_IDLE, CX, 236, millis(), true, false, 4, 170);
  if (st == LS_READY) {
    drawFit(XT(X_TRADE_Q), 250, 340, UI_INK, 2);
    drawFit(XT(X_TRADE_WARN), 276, 340, UI_BAR_BAD, 2);
    drawBtn(LT_YES_X, LT_BTN_Y, LT_BTN_W, LT_BTN_H, UI_BAR_OK, UI_WHITE, T(S_YES));
    drawBtn(LT_NO_X, LT_BTN_Y, LT_BTN_W, LT_BTN_H, UI_BAR_BAD, UI_WHITE, T(S_NO));
    if (linkPartnerAccepted()) drawFit(XT(X_PARTNER_OK), 384, 260, UI_BAR_OK, 2);
  } else if (st == LS_TRADE_WAIT) {
    int ph = (millis() / 250) % 4;
    for (int i = 0; i < 3; i++) gfx->fillCircle(CX - 24 + i * 24, 290, 6, i < ph ? 0x4C98 : UI_TRACK);
    drawFit(XT(X_TRADE_WAIT), 316, 340, UI_INK, 2);
  }
}

void renderLinkEnd() {
  screenBase();
  drawFit(XT(X_LINK_TITLE), 44, 300, UI_INK, 3);
  if (pmd.loaded && linkTradeApplied) drawPmdAct(PMD_IDLE, CX, 270, millis(), true, false, 4);
  drawFit(linkEndMsg ? linkEndMsg : "", 316, 360, linkTradeApplied ? UI_BAR_OK : UI_INK, 3);
}

void renderLink() {
  if (linkBattleStarted) { renderBattleView(); return; }
  if (linkEndAt) { renderLinkEnd(); uiFlush(); return; }
  if (linkState() == LS_SEARCH) renderLinkSearch(linkMode() == LINK_BATTLE ? XT(X_LINK_BATTLE) : XT(X_LINK_TRADE));
  else if (linkMode() == LINK_TRADE) renderLinkTrade();
  else renderLinkSearch(XT(X_LINK_BATTLE));
  uiFlush();
}

void linkTap(int16_t x, int16_t y) {
  if (linkBattleStarted) {
    if (bPhase == BP_RESULT) closeLink();
    return;
  }
  if (linkEndAt) return;
  LinkState st = linkState();
  if (st == LS_READY && linkMode() == LINK_TRADE) {
    if (inRect(x, y, LT_YES_X, LT_BTN_Y, LT_BTN_W, LT_BTN_H)) { linkTradeAccept(); sfxPlay(SFX_TAP); }
    else if (inRect(x, y, LT_NO_X, LT_BTN_Y, LT_BTN_W, LT_BTN_H)) { linkTradeDecline(); sfxPlay(SFX_TAP); }
    return;
  }
  // tras aceptar ya no se cancela a mano: el otro podria haberlo completado
  // (se resuelve solo al llegar su respuesta o por silencio a los 8 s)
  if (st == LS_TRADE_WAIT) return;
  if (y < 72 || y > 380) closeLink();
}

void updateLink() {
  uint32_t now = millis();
  lastInteract = now;
  if (linkBattleStarted) { updateLinkBattle(); return; }
  if (linkEndAt) {
    if ((int32_t)(now - linkEndAt) >= 0) closeLink();
    return;
  }
  // el Pokemon se durmio, evoluciono... mientras esperaba: se cancela
  if (!pet.canBattle() && !linkTradeApplied) {
    if (linkState() == LS_TRADE_WAIT || linkState() == LS_READY) linkTradeDecline();
    linkFinish(XT(X_CANT_NOW), 2500);
    return;
  }
  LinkState st = linkState();
  TradePet got;
  if (linkTakeTrade(got)) {
    if (pet.importTrade(got, linkPartner().lvl)) {
      linkTradeApplied = true;
      if (pet.evolving()) evoPmd.load(pet.prevSpeciesId, pet.shiny);
      sfxPlay(SFX_HATCH);
      foePmd.unload();
      linkFinish(XT(X_TRADE_DONE), 3500);  // el enlace sigue 3 s confirmando al otro
    } else {
      linkFinish(XT(X_LINK_LOST), 2500);
    }
    return;
  }
  if (st == LS_READY && linkMode() == LINK_BATTLE) startLinkBattle();
  else if (st == LS_DECLINED) linkFinish(XT(X_TRADE_NO), 2500);
  else if (st == LS_LOST || st == LS_ERROR) linkFinish(XT(X_LINK_LOST), 2500);
}

// ======================================================================
// ganchos para TamaPoke.ino
// ======================================================================

// /mons/battle_wild.wav suena durante el combate (salvaje y tongsin), no en el
// resultado: al acabar vuelve /mons/bgm.wav
// ko11: que pista toca ahora (ver audioSetMusicTrack)
uint8_t storyBattleTrack();  // ko11.24 (ui_story.ino)
uint8_t storySceneTrack();
uint8_t musicTrackNow() {
  if (battleMusicActive()) {
    if (bKind == BK_STORY || bKind == BK_ROGUE) return storyBattleTrack();  // ko11.24
    return bKind == BK_GYM ? MT_GYM : bKind == BK_CHAMP ? MT_CHAMP : MT_NORMAL;
  }
  if (xScreen == XS_SCENE || xScreen == XS_STORY || xScreen == XS_STORYCH) return storySceneTrack();  // ko11.24
  if ((xScreen == XS_GYM && gymPage == 2) || xScreen == XS_FAME) return MT_FAME;  // liga y salon
  return MT_NORMAL;
}

bool battleMusicActive() {
  bool fighting = xScreen == XS_WILD || (xScreen == XS_LINK && linkBattleStarted);
  return fighting && bPhase != BP_RESULT && bPhase != BP_NEXT && bPhase != BP_DUP && bPhase != BP_JOIN;
}

void extraLoop(uint32_t now) {
  now = millis();  // ko6.1: el toque de este loop ya se proceso (puede haber arrancado algo)
  uint32_t e = netPoll(now);
  if (e) applyNetTime(e);
  linkPoll(now);
  // ko10.4: si mi hora no es de fiar (pila agotada) y el amigo si la tiene, se toma
  uint32_t le;
  if (!gClockTrusted && linkActive() && linkPartnerClock(&le)) {
    applyNetTime(le);
    showToast(XT(X_TIME_FROM_LINK));
  }
  if (xScreen == XS_WILD) updateWild();
  else if (xScreen == XS_LINK) updateLink();
  if (xScreen == XS_NET) lastInteract = now;
  if (xScreen == XS_UPD || xScreen == XS_RESET) lastInteract = now;
  rollWildEncounter(now);
  nextPickPoll();  // ko10.5
}

bool extraRender() {
  switch (xScreen) {
    case XS_NET: renderNet(); return true;
    case XS_WILD: renderBattleView(); return true;
    case XS_LINKMENU: renderLinkMenu(); return true;
    case XS_LINK: renderLink(); return true;
    case XS_BOX: renderBox(); return true;    // ko4 (ui_more.ino)
    case XS_VOL: renderSound(); return true;  // ko4 (ui_more.ino)
    case XS_UPD: renderUpdate(); return true; // ko5 (ui_more.ino)
    case XS_RESET: renderReset(); return true; // ko8 (ui_more.ino)
    case XS_REGION: renderRegionPick(); return true;  // ko10.1
    case XS_GYM: renderGyms(); return true;           // ko10.4
    case XS_DAILY: renderDaily(); return true;
    case XS_NEXTPICK: renderNextPick(); return true;  // ko10.5
    case XS_CANDY: renderCandyBag(); return true;     // ko10.11
    case XS_FAME: renderFame(); return true;          // ko11.1
    case XS_BAK: renderBackup(); return true;         // ko11.6
    case XS_BGM: renderBgmPick(); return true;        // ko11.8
    case XS_BRIGHT: renderBright(); return true;      // ko11.18
    case XS_PARTY: renderPartyPick(); return true;    // ko11.20
    case XS_STORY: renderStoryMenu(); return true;    // ko11.21
    case XS_STORYCH: renderStoryChapters(); return true;
    case XS_SCENE: renderStoryScene(); return true;
    case XS_ROGUE: renderRogue(); return true;
    case XS_SET: renderSettings(); return true;  // ko11.26
    case XS_STRAIN: renderStoryTrain(); return true;  // ko11.28
    default: return false;
  }
}

bool extraTap(int16_t x, int16_t y) {
  switch (xScreen) {
    case XS_NET: netTap(x, y); return true;
    case XS_WILD: wildTap(x, y); return true;
    case XS_LINKMENU: linkMenuTap(x, y); return true;
    case XS_LINK: linkTap(x, y); return true;
    case XS_BOX: boxTap(x, y); return true;
    case XS_VOL: soundTap(x, y); return true;
    case XS_UPD: updateTap(x, y); return true;
    case XS_RESET: resetTap(x, y); return true;
    case XS_REGION: regionTap(x, y); return true;
    case XS_GYM: gymTap(x, y); return true;
    case XS_DAILY: dailyTap(x, y); return true;
    case XS_NEXTPICK: nextPickTap(x, y); return true;
    case XS_CANDY: candyBagTap(x, y); return true;
    case XS_FAME: fameTap(x, y); return true;
    case XS_BAK: backupTap(x, y); return true;
    case XS_BGM: bgmPickTap(x, y); return true;
    case XS_BRIGHT: brightTap(x, y); return true;  // ko11.18
    case XS_PARTY: partyPickTap(x, y); return true;  // ko11.20
    case XS_STORY: storyMenuTap(x, y); return true;  // ko11.21
    case XS_STORYCH: storyChaptersTap(x, y); return true;
    case XS_SCENE: storySceneTap(x, y); return true;
    case XS_ROGUE: rogueTap(x, y); return true;
    case XS_SET: settingsTap(x, y); return true;
    case XS_STRAIN: storyTrainTap(x, y); return true;
    default: return false;
  }
}

// los deslizamientos dentro de las pantallas nuevas: solo cerrar donde tiene sentido
bool extraSwipe() {
  if (xScreen == XS_NET) { closeNet(); return true; }
  if (xScreen == XS_LINKMENU) { goBack(); return true; }
  if (xScreen == XS_BOX) { boxSwipe(); return true; }
  if (xScreen == XS_VOL) { goBack(); return true; }
  if (xScreen == XS_BGM) { xScreen = XS_VOL; return true; }  // ko11.8
  if (xScreen == XS_BRIGHT || xScreen == XS_UPD) { xScreen = XS_SET; return true; }  // ko11.26
  if (xScreen == XS_SET) { goBack(); return true; }
  if (xScreen == XS_STRAIN) { xScreen = XS_STORY; return true; }  // ko11.28
  if (xScreen == XS_RESET) { goBack(); return true; }
  if (xScreen == XS_CANDY) { candyBagClose(); return true; }  // ko10.11: vertical = cerrar
  if (xScreen == XS_FAME) { fameClose(); return true; }       // ko11.1
  if (xScreen == XS_REGION || xScreen == XS_GYM || xScreen == XS_DAILY) { goBack(); return true; }  // ko11.17
  if (xScreen == XS_PARTY) { partyCancel(); return true; }  // ko11.20
  if (xScreen == XS_STORY) { goBack(); return true; }         // ko11.21
  if (xScreen == XS_STORYCH) { xScreen = XS_STORY; return true; }
  if (xScreen == XS_SCENE || xScreen == XS_ROGUE) return true;
  if (xScreen == XS_NEXTPICK) {
    xScreen = XS_NONE;  // ko10.5: en la eleccion, cerrar = quedarse el huevo
    return true;
  }
  return xScreen != XS_NONE;  // batalla / tongsin: se ignoran
}

void drawToast() {
  if (!timeLeft(toastUntil)) return;
  setSize(2);
  int w = textW(toastBuf, 2) + 28;
  if (w > 380) w = 380;
  // ko11.12: pildora oscura semitransparente con sombra y filo claro
  uiShade(CX - w / 2 - 1, 265, w + 2, 38, 18, 3);
  uiShade(CX - w / 2, 262, w, 36, 18, 12);
  gfx->drawRoundRect(CX - w / 2, 262, w, 36, 18, C565(0x90, 0x98, 0xa8));
  drawFit(toastBuf, 272, w - 12, UI_WHITE, 2);
}

// ui_extra.ino - pantallas nuevas del fork KO (se concatena tras TamaPoke.ino)
//
//   - Red: WiFi + hora por NTP (desde la pantalla de hora, pildora "WiFi")
//   - Yasaeng: batalla por turnos contra un Pokemon salvaje (ficha > Batalla,
//     o el aviso "! yasaeng !" que aparece de vez en cuando)
//   - Tongsin: batalla automatica o intercambio entre dos TamaPoke (ESP-NOW)
//
// Se engancha al sketch principal solo por funciones (extraRender, extraTap,
// extraSwipe, extraLoop...), para tocar lo minimo el fichero original.

enum : uint8_t { XS_NONE = 0, XS_NET, XS_WILD, XS_LINKMENU, XS_LINK, XS_USB, XS_BOX, XS_VOL };
uint8_t xScreen = XS_NONE;

// aviso breve en la pantalla principal
char toastBuf[64] = "";
uint32_t toastUntil = 0;

void showToast(const char *s) {
  strncpy(toastBuf, s, sizeof(toastBuf) - 1);
  toastBuf[sizeof(toastBuf) - 1] = 0;
  toastUntil = millis() + 2600;
}

bool extraOpen() { return xScreen != XS_NONE; }

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
  gfx->fillRoundRect(x, y, w, h, 12, bg);
  gfx->drawRoundRect(x, y, w, h, 12, UI_INK);
  uint8_t sz = 2;
  setSize(sz);
  if (textW(s, sz) > w - 10) { sz = 1; setSize(1); }
  gfx->setTextColor(fg);
  int tw = textW(s, sz);
  int th = gCjkFont ? 16 : 8 * sz;
  setCur(x + (w - tw) / 2, y + (h - th) / 2);
  printT(s);
}

bool inRect(int16_t x, int16_t y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void screenBase() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
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
#define NET_USB_X 113
#define NET_USB_W 240
#define NET_USB_Y 370
#define NET_USB_H 40

void openNet() { xScreen = XS_NET; }

void closeNet() {
  if (netPortalOn()) netStopPortal();
  xScreen = XS_NONE;
}

// la hora local llega del NTP: al RTC y al juego
void applyNetTime(uint32_t e) {
  rtcSetEpoch(e);
  // si el RTC habia perdido la hora al arrancar, ahora SI se sabe cuanto tiempo
  // estuvo apagado: se aplica como progresion offline (igual que con pila)
  if (gRtcWasLost) {
    gRtcWasLost = false;
    pet.syncClock(e);
  } else {
    pet.setClock(e);
  }
}

void renderNet() {
  screenBase();
  gfx->setTextColor(UI_INK);
  if (netPortalOn()) {
    drawFit(XT(X_SETUP_WIFI), 48, 300, UI_INK, 3);
    drawFit(XT(X_PORTAL_1), 104, 360, UI_INK, 2);
    gfx->fillRoundRect(73, 132, 320, 48, 12, UI_WHITE);
    gfx->drawRoundRect(73, 132, 320, 48, 12, UI_BAR_BAD);
    drawFit(netApName(), 146, 300, UI_BAR_BAD, 3);
    drawFit(XT(X_PORTAL_3), 196, 360, UI_INK, 2);
    drawFit(XT(X_PORTAL_2), 232, 360, UI_INK, 2);
    // icono de wifi animado
    int ph = (millis() / 300) % 4;
    for (int i = 0; i < 3; i++)
      if (i < ph) gfx->drawCircle(CX, 318, 12 + i * 12, UI_BAR_OK);
    gfx->fillCircle(CX, 318, 5, UI_BAR_OK);
    drawBtn(133, 356, 200, 44, UI_TRACK, UI_INK, T(S_BACK));
    gfx->flush();
    return;
  }

  drawFit(XT(X_NET_TITLE), 36, 300, UI_INK, 3);
  char l[64];
  if (netConfigured()) snprintf(l, sizeof(l), "WiFi: %s", netSsid());
  else strncpy(l, XT(X_NOT_SET), sizeof(l));
  l[sizeof(l) - 1] = 0;
  drawFit(l, 80, 340, UI_INK, 2);

  // estado o ultima sincronizacion
  const char *st = nullptr;
  uint16_t sc = UI_INK;
  switch (netState()) {
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
  drawBtn(NET_BTN_X, NET_SYNC_Y, NET_BTN_W, NET_BTN_H, (netConfigured() && !busy) ? UI_BAR_OK : UI_TRACK,
          UI_WHITE, XT(X_SYNC_NOW));
  drawBtn(NET_BTN_X, NET_SETUP_Y, NET_BTN_W, NET_BTN_H, 0x4C98, UI_WHITE, XT(X_SETUP_WIFI));
  drawBtn(NET_BTN_X, NET_AUTO_Y, NET_BTN_W, NET_BTN_H, netAuto() ? UI_WHITE : UI_TRACK, UI_INK,
          netAuto() ? XT(X_AUTO_ON) : XT(X_AUTO_OFF));
  drawBtn(NET_USB_X, NET_USB_Y, NET_USB_W, NET_USB_H, 0x8C1F, UI_WHITE, XT(X_USB_BTN));
  drawFit(XT(X_TAP_CLOSE), 420, 220, UI_INK, 2);
  gfx->flush();
}

void netTap(int16_t x, int16_t y) {
  if (netPortalOn()) {
    if (y >= 350) netStopPortal();
    return;
  }
  if (y < 72) { closeNet(); return; }
  if (inRect(x, y, NET_USB_X, NET_USB_Y, NET_USB_W, NET_USB_H)) { openUsb(); return; }
  if (y >= NET_TZ_Y && y < NET_TZ_Y + 44) {
    if (x < 140) netSetTzMin(netTzMin() - 30);
    else if (x > 326) netSetTzMin(netTzMin() + 30);
    sfxPlay(SFX_TAP);
    return;
  }
  if (!inRect(x, y, NET_BTN_X, NET_SYNC_Y, NET_BTN_W, NET_AUTO_Y + NET_BTN_H - NET_SYNC_Y)) return;
  if (y < NET_SYNC_Y + NET_BTN_H) {
    if (netConfigured() && !netBusy() && !linkActive()) { netSyncNow(); sfxPlay(SFX_TAP); }
    else sfxPlay(SFX_DENY);
  } else if (y >= NET_SETUP_Y && y < NET_SETUP_Y + NET_BTN_H) {
    if (linkActive()) { sfxPlay(SFX_DENY); return; }
    netStartPortal();
    sfxPlay(SFX_TAP);
  } else if (y >= NET_AUTO_Y) {
    netSetAuto(!netAuto());
    sfxPlay(SFX_TAP);
  }
}

// ======================================================================
// Unidad USB: la SD en el PC (desde la pantalla de red)
// ======================================================================

#define USB_BTN_Y 330
int8_t usbFail = -1;  // XId del error si no se pudo activar, -1 = activa

void openUsb() {
  if (netPortalOn()) netStopPortal();
  usbFail = !usbDiskSupported() ? X_USB_NA : (!sdReady || !usbDiskStart()) ? X_USB_FAIL : -1;
  sfxPlay(usbFail < 0 ? SFX_TAP : SFX_DENY);
  xScreen = XS_USB;
}

void closeUsb() {
  usbDiskStop();  // remonta la SD y recarga sprites/musica (nada si no estaba activa)
  xScreen = XS_NET;
}

void renderUsb() {
  screenBase();
  drawFit(XT(X_USB_TITLE), 48, 300, UI_INK, 3);
  if (usbFail >= 0) {
    drawFit(XT((XId)usbFail), 180, 360, UI_BAR_BAD, 2);
    drawBtn(133, USB_BTN_Y, 200, 50, UI_TRACK, UI_INK, T(S_BACK));
    gfx->flush();
    return;
  }
  drawFit(XT(X_USB_1), 118, 360, UI_INK, 2);
  drawFit(XT(X_USB_2), 158, 360, UI_INK, 2);
  drawFit(XT(X_USB_3), 198, 360, UI_INK, 2);
  bool seen = usbDiskHostSeen();
  drawFit(XT(seen ? X_USB_SEEN : X_USB_WAIT), 262, 340, seen ? UI_BAR_OK : UI_BAR_WARN, 2);
  drawBtn(133, USB_BTN_Y, 200, 50, UI_BAR_OK, UI_WHITE, XT(X_USB_DONE));
  gfx->flush();
}

void usbTap(int16_t x, int16_t y) {
  if (!inRect(x, y, 113, USB_BTN_Y - 10, 240, 70)) return;
  sfxPlay(SFX_TAP);
  closeUsb();
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
uint16_t bvMeLvl, bvFoeLvl, bvMeMax, bvFoeMax;
float bvMeHp, bvFoeHp;        // animado
uint16_t bvMeTgt, bvFoeTgt;   // objetivo
char bvMeName[32], bvFoeName[40];
bool bvFoeShiny = false;
char bvL1[80] = "", bvL2[64] = "";
bool bvMeFainted = false, bvFoeFainted = false;  // ya se reprodujo su desmayo
bool bvFoeCaught = false;  // fork KO (ko4): el rival ya esta dentro de la pokeball
bool bCaught = false;      // la batalla acabo en captura
int8_t bBoxMsg = -1;       // XId del aviso de la caja en el resultado (-1 = nada)
PmdMon foePmd;
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
enum : uint8_t { BP_INTRO = 0, BP_MENU, BP_PLAY, BP_RESULT };
uint8_t bPhase = BP_INTRO;
uint32_t bPhaseT = 0;
bool bWon = false, bFled = false, bLink = false, bRewarded = false;

// salvaje
Battler bMe, bFoe;
BRng bRng(1);

uint32_t evDur(const BEvent &e) {
  switch (e.kind) {
    case EV_HIT: return (e.eff != 2 || e.crit) ? 1900 : 1400;
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
      txFmt(bvL1, sizeof(bvL1), X_USED, who, moveName(e.move, me ? bvMeType : bvFoeType));
      if (e.kind == EV_MISS) strncpy(bvL2, XT(X_MISSED), sizeof(bvL2) - 1);
      else if (e.eff == 0) strncpy(bvL2, XT(X_NOEFFECT), sizeof(bvL2) - 1);
      else if (e.eff == 4) strncpy(bvL2, XT(X_SUPER), sizeof(bvL2) - 1);
      else if (e.eff == 1) strncpy(bvL2, XT(X_NOTVERY), sizeof(bvL2) - 1);
      else if (e.crit) strncpy(bvL2, XT(X_CRIT), sizeof(bvL2) - 1);
      break;
    case EV_GUARD: txFmt(bvL1, sizeof(bvL1), X_GUARDS, who); break;
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
  if (e.kind == EV_HIT && e.dmg) sfxPlay(SFX_PLAY);
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

// cielo segun la hora + suelo del bioma de mi Pokemon + dos plataformas
void drawBattleBg() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)        { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8)  { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18) { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else              { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = 150;
  for (int y = 0; y < hor; y += 8) gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, hor));
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < 6 ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, 466, 262 - hor, soil);
  uint16_t pad = lerp565(soil, C565(0x10, 0x18, 0x20), 4, 16);
  gfx->fillEllipse(316, 160, 78, 16, pad);   // plataforma del rival
  gfx->fillEllipse(140, 256, 92, 18, pad);   // la mia
  gfx->fillRect(0, 262, 466, 204, UI_BG_DAY);  // panel inferior (mensajes/menu)
}

void drawHpBox(int x, int y, int w, const char *name, uint16_t lvl, float hp, uint16_t maxHp, bool showNum) {
  gfx->fillRoundRect(x, y, w, showNum ? 58 : 46, 10, UI_WHITE);
  gfx->drawRoundRect(x, y, w, showNum ? 58 : 46, 10, UI_INK);
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
  setCur(x + 8, y + 5);
  printT(name);
  int bx = x + 8, by = y + 28, bw = w - 16;
  float f = maxHp ? hp / maxHp : 0;
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  uint16_t col = f > 0.5f ? UI_BAR_OK : f > 0.2f ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, by, bw, 10, 3, UI_TRACK);
  int fw = (int)((bw - 2) * f);
  if (fw > 1) gfx->fillRoundRect(bx + 1, by + 1, fw, 8, 3, col);
  if (showNum) {
    char hs[16];
    snprintf(hs, sizeof(hs), "%u/%u", (unsigned)(hp + 0.5f), maxHp);
    setSize(1);
    setCur(x + w - textW(hs, 1) - 8, y + 43);
    printT(hs);
  }
}

// dibuja los dos Pokemon; anima al que actua segun el evento en curso
void drawBattlers() {
  uint32_t now = millis();
  int meX = 140, meG = 250, foeX = 316, foeG = 156;
  uint8_t meAct = PMD_IDLE, foeAct = PMD_IDLE;
  bool meHide = false, foeHide = false, meSil = false, foeSil = false;
  uint32_t t = now - bqT;
  if (bPhase == BP_PLAY && bqI < bqN) {
    const BEvent &e = bq[bqI];
    bool me = evIsMe(e.side);
    if (e.kind == EV_HIT || e.kind == EV_MISS) {
      int lunge = (t < 350) ? (int)(t / 12) : (t < 550 ? (int)((550 - t) / 7) : 0);
      if (me) { meAct = PMD_ATTACK; meX += lunge; meG -= lunge / 2; }
      else    { foeAct = PMD_ATTACK; foeX -= lunge; foeG += lunge / 2; }
      if (e.kind == EV_HIT && e.eff && t > 350 && t < 1000) {
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

  if (!foeHide && !foeGone) {
    if (foePmd.loaded) {
      if (!foePmd.has(foeAct)) foeAct = PMD_IDLE;
      drawPmdActM(foePmd, foeAct, foeX, foeG, now, true, foeSil, 3);
    } else {
      const uint8_t *th = thumbs.get(bvFoeDex);
      if (th) drawThumb(th, foeX - GAL_CELL / 2, foeG - GAL_CELL, 2, foeSil);
    }
  }
  if (!meHide && !meGone) {
    if (pmd.loaded) {
      if (!pmd.has(meAct)) meAct = PMD_IDLE;
      drawPmdAct(meAct, meX, meG, now, true, meSil, 3);
    } else {
      const uint8_t *th = thumbs.get(bvMeDex);
      if (th) drawThumb(th, meX - GAL_CELL / 2, meG - GAL_CELL, 3, meSil);
    }
  }
}

void drawBattleMsg() {
  gfx->fillRoundRect(40, 266, 386, 56, 12, UI_WHITE);
  gfx->drawRoundRect(40, 266, 386, 56, 12, UI_INK);
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

void drawBattleMenu() {
  const DexEntry &me = DEX_TBL[bvMeDex];
  char pot[16], ball[16];
  snprintf(pot, sizeof(pot), XT(X_POTION_FMT), pet.potions);
  snprintf(ball, sizeof(ball), XT(X_BALL_FMT), pet.balls);
  int x0 = BM_X, x1 = BM_X + BM_W + BM_GAP, x2 = BM_X + 2 * (BM_W + BM_GAP);
  drawBtn(x0, BM_Y1, BM_W, BM_H, UI_WHITE, UI_INK, moveName(BA_TACKLE, bvMeType));
  drawBtn(x1, BM_Y1, BM_W, BM_H, me.accent, UI_WHITE, moveName(BA_TYPE, bvMeType));
  drawBtn(x2, BM_Y1, BM_W, BM_H, 0x4C98, UI_WHITE, XT(X_GUARD));
  drawBtn(x0, BM_Y2, BM_W, BM_H, pet.potions ? UI_BAR_OK : UI_TRACK, pet.potions ? UI_WHITE : UI_INK, pot);
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

void renderBattleView() {
  uint32_t now = millis();
  lastInteract = now;  // una batalla no deja que la pantalla se atenue
  // barras de vida que bajan poco a poco
  bvMeHp += (bvMeTgt - bvMeHp) * 0.25f;
  bvFoeHp += (bvFoeTgt - bvFoeHp) * 0.25f;
  if (fabsf(bvMeTgt - bvMeHp) < 0.5f) bvMeHp = bvMeTgt;
  if (fabsf(bvFoeTgt - bvFoeHp) < 0.5f) bvFoeHp = bvFoeTgt;

  drawBattleBg();
  drawBattlers();
  drawHpBox(84, 50, 176, bvFoeName, bvFoeLvl, bvFoeHp, bvFoeMax, false);
  drawHpBox(236, 176, 176, bvMeName, bvMeLvl, bvMeHp, bvMeMax, true);

  if (bPhase == BP_RESULT) {
    bool good = bWon || bCaught;
    gfx->fillRoundRect(60, 270, 346, 128, 16, good ? UI_BAR_WARN : UI_WHITE);
    gfx->drawRoundRect(60, 270, 346, 128, 16, UI_INK);
    const char *big = bFled ? XT(X_FLED) : bCaught ? XT(X_GOTCHA) : bWon ? XT(X_WIN) : XT(X_LOSE);
    drawFit(big, 284, 320, UI_INK, bFled ? 2 : 4);
    if (good && !bLink) {  // fork KO (ko4): objetos y caja
      drawFit(bWon ? XT(X_REWARD_ITEMS) : XT(X_REWARD), 330, 320, UI_INK, 2);
      if (bBoxMsg >= 0) drawFit(XT((XId)bBoxMsg), 358, 320, bBoxMsg == X_BOX_FULL ? UI_BAR_BAD : UI_INK, 2);
    } else if (good) {
      drawFit(XT(X_REWARD), 340, 320, UI_INK, 2);
    }
  } else {
    drawBattleMsg();
    if (bPhase == BP_MENU) drawBattleMenu();
  }
  gfx->flush();
}

void bvSetup(const Battler &me, const Battler &foe, const char *foeNick, bool foeShiny) {
  bvMeDex = me.dex; bvFoeDex = foe.dex;
  bvMeType = me.type; bvFoeType = foe.type;
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
  bvMeFainted = bvFoeFainted = false;
  bvFoeCaught = bCaught = false;
  bBoxMsg = -1;
  dexLog.seen(foe.dex, clockEpoch());  // fork KO (ko4): la pokedex lo registra como visto
}

// ======================================================================
// Yasaeng: batalla contra un salvaje
// ======================================================================

uint32_t wildAlertUntil = 0;   // aviso "! yasaeng !" en la pantalla principal
uint32_t wildNextRoll = 0;

void triggerWildAlert() { wildAlertUntil = millis() + 5UL * 60UL * 1000UL; }

bool battleAllowed(bool toast) {
  if (usbDiskActive()) { sfxPlay(SFX_DENY); return false; }  // la SD es del PC ahora
  if (!pet.canBattle()) { if (toast) showToast(XT(X_CANT_NOW)); sfxPlay(SFX_DENY); return false; }
  if (pet.tooTiredToBattle()) { if (toast) showToast(XT(X_TOO_TIRED)); sfxPlay(SFX_DENY); return false; }
  return true;
}

void startWild() {
  if (!battleAllowed(true)) return;
  wildAlertUntil = 0;
  cardOpen = false;
  bRng = BRng(esp_random());
  bMe = makeBattler(pet.speciesId, pet.level(), pet.atkStat(), pet.defStat(), pet.speStat());
  bFoe = makeWild(pet.level(), bRng);
  bool shiny = bRng.below(64) == 0;
  bvSetup(bMe, bFoe, nullptr, shiny);
  bqAisMe = true;
  bLink = false;
  txFmt(bvL1, sizeof(bvL1), X_WILD_APPEARS, dexName(bFoe.dex));
  bPhase = BP_INTRO;
  bPhaseT = millis();
  xScreen = XS_WILD;
  sfxPlay(SFX_MEDAL);
  audioSetBattleMusic(false, true);  // batalla nueva: la cancion empieza de cero
  audioCry(bFoe.dex);
}

void endBattleScreen() {
  foePmd.unload();
  xScreen = XS_NONE;
}

void wildTap(int16_t x, int16_t y) {
  if (bPhase == BP_INTRO) { bPhaseT = 0; return; }  // saltar la intro
  if (bPhase != BP_MENU) return;
  int a = battleMenuHit(x, y);
  if (a < 0) return;
  // fork KO (ko4): los objetos se gastan al elegirlos; sin existencias no hay turno
  if (a == BA_POTION && !pet.usePotion()) {
    strncpy(bvL1, XT(X_NO_POTION), sizeof(bvL1) - 1); bvL2[0] = 0; sfxPlay(SFX_DENY); return;
  }
  if (a == BA_BALL && !pet.useBall()) {
    strncpy(bvL1, XT(X_NO_BALL), sizeof(bvL1) - 1); bvL2[0] = 0; sfxPlay(SFX_DENY); return;
  }
  sfxPlay(SFX_TAP);
  BAct foeAct = battleAi(bFoe, bMe, bRng, 35);  // el salvaje es algo torpe
  bqN = battleTurn(bMe, bFoe, (BAct)a, foeAct, bRng, bq, BATTLE_MAX_EVENTS, true);
  bqAisMe = true;
  bPhase = BP_PLAY;
  startEvent(0);
}

void finishBattle(bool won, bool fled, bool caught) {
  bWon = won;
  bFled = fled;
  bCaught = caught;
  bPhase = BP_RESULT;
  bPhaseT = millis();
  if (!bRewarded) {
    bRewarded = true;
    pet.battleResult(bLink ? BATTLE_LINK : BATTLE_WILD, won, fled, caught);
    sfxPlay(won || caught ? SFX_MEDAL : SFX_BYE);
    // fork KO (ko4): el salvaje vencido o capturado va a la caja
    if (!bLink && (won || caught)) {
      uint32_t e = clockEpoch();
      if (caught) dexLog.caught(bFoe.dex, e);
      bBoxMsg = box.add(bFoe.dex, bFoe.lvl, bvFoeShiny, caught, e) ? X_TO_BOX : X_BOX_FULL;
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
  if (bPhase == BP_INTRO) {
    if (bPhaseT == 0 || now - bPhaseT > 2200) {
      bPhase = BP_MENU;
      txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
      bvL2[0] = 0;
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
      else if (bMe.hp == 0) finishBattle(false, false, false);
      else if (bFoe.hp == 0) finishBattle(true, false, false);
      else {
        bPhase = BP_MENU;
        txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
        bvL2[0] = 0;
      }
    }
  } else if (bPhase == BP_RESULT) {
    if (now - bPhaseT > 3800) endBattleScreen();
  }
}

// aviso de encuentro en la pantalla principal (mismo sitio que los otros CTA)
bool wildAlertActive() { return timeLeft(wildAlertUntil) > 0 && pet.canBattle(); }

void drawWildAlert() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.008f));
  int x = FAR_BTN_X + 40 - p, y = FAR_BTN_Y - p, w = FAR_BTN_W - 80 + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, C565(0x2e, 0x7d, 0x32));
  gfx->drawRoundRect(x, y, w, h, 16, UI_WHITE);
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
    wildAlertUntil = now + 5UL * 60UL * 1000UL;
    sfxPlay(SFX_HEART);
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
  gfx->flush();
}

void linkMenuTap(int16_t x, int16_t y) {
  if (y < 72 || y > 380) { xScreen = XS_NONE; return; }
  LinkMode m = LINK_NONE;
  if (inRect(x, y, LM_BTN_X, LM_BATTLE_Y, LM_BTN_W, LM_BTN_H)) m = LINK_BATTLE;
  else if (inRect(x, y, LM_BTN_X, LM_TRADE_Y, LM_BTN_W, LM_BTN_H)) m = LINK_TRADE;
  if (m == LINK_NONE) return;
  if (!pet.canBattle() || netBusy()) { sfxPlay(SFX_DENY); return; }
  if (m == LINK_BATTLE && pet.tooTiredToBattle()) { showToast(XT(X_TOO_TIRED)); sfxPlay(SFX_DENY); return; }
  sfxPlay(SFX_TAP);
  LinkPet lp;
  myLinkPet(lp);
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
  if (foePmd.loaded) drawPmdActM(foePmd, PMD_IDLE, CX, 236, millis(), true, false, 4);
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
  if (linkEndAt) { renderLinkEnd(); gfx->flush(); return; }
  if (linkState() == LS_SEARCH) renderLinkSearch(linkMode() == LINK_BATTLE ? XT(X_LINK_BATTLE) : XT(X_LINK_TRADE));
  else if (linkMode() == LINK_TRADE) renderLinkTrade();
  else renderLinkSearch(XT(X_LINK_BATTLE));
  gfx->flush();
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
    if (pet.importTrade(got)) {
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
bool battleMusicActive() {
  bool fighting = xScreen == XS_WILD || (xScreen == XS_LINK && linkBattleStarted);
  return fighting && bPhase != BP_RESULT;
}

void extraLoop(uint32_t now) {
  uint32_t e = netPoll(now);
  if (e) applyNetTime(e);
  linkPoll(now);
  if (xScreen == XS_WILD) updateWild();
  else if (xScreen == XS_LINK) updateLink();
  if (xScreen == XS_NET) lastInteract = now;
  if (xScreen == XS_USB) {
    lastInteract = now;  // la pantalla se queda encendida mientras el PC trabaja
    if (usbDiskEjected()) { closeUsb(); sfxPlay(SFX_MEDAL); }
  }
  rollWildEncounter(now);
}

bool extraRender() {
  switch (xScreen) {
    case XS_NET: renderNet(); return true;
    case XS_WILD: renderBattleView(); return true;
    case XS_LINKMENU: renderLinkMenu(); return true;
    case XS_LINK: renderLink(); return true;
    case XS_USB: renderUsb(); return true;
    case XS_BOX: renderBox(); return true;    // ko4 (ui_more.ino)
    case XS_VOL: renderSound(); return true;  // ko4 (ui_more.ino)
    default: return false;
  }
}

bool extraTap(int16_t x, int16_t y) {
  switch (xScreen) {
    case XS_NET: netTap(x, y); return true;
    case XS_WILD: wildTap(x, y); return true;
    case XS_LINKMENU: linkMenuTap(x, y); return true;
    case XS_LINK: linkTap(x, y); return true;
    case XS_USB: usbTap(x, y); return true;
    case XS_BOX: boxTap(x, y); return true;
    case XS_VOL: soundTap(x, y); return true;
    default: return false;
  }
}

// los deslizamientos dentro de las pantallas nuevas: solo cerrar donde tiene sentido
bool extraSwipe() {
  if (xScreen == XS_NET) { closeNet(); return true; }
  if (xScreen == XS_LINKMENU) { xScreen = XS_NONE; return true; }
  if (xScreen == XS_USB && usbFail >= 0) { xScreen = XS_NET; return true; }  // activa: solo el boton
  if (xScreen == XS_BOX) { boxSwipe(); return true; }
  if (xScreen == XS_VOL) { xScreen = XS_NONE; clockOpen = true; return true; }
  return xScreen != XS_NONE;  // batalla / tongsin: se ignoran
}

void drawToast() {
  if (!timeLeft(toastUntil)) return;
  setSize(2);
  int w = textW(toastBuf, 2) + 28;
  if (w > 380) w = 380;
  gfx->fillRoundRect(CX - w / 2, 262, w, 36, 12, UI_INK);
  drawFit(toastBuf, 272, w - 12, UI_WHITE, 2);
}

// ======================================================================
// ko12.4: casa y paseo (funciones de tamagotchi)
//   - Habitacion: el fondo de la pantalla principal puede ser una habitacion
//     (pared, ventana con el cielo de la hora, suelo de madera) en vez del paisaje
//   - Decorar: 3 sitios en el suelo con objetos que se consiguen cuidando
//   - Paseo: podometro con el sensor de movimiento de la placa (QMI8658), si lo hay.
//     Agitar la placa: si se durmio sola la despierta; si no, se alegra
// ======================================================================

static void homeTextAt(const char *s, int cx, int y, uint16_t c) {  // texto centrado en cx (talla 1)
  gfx->setTextColor(c);
  setSize(1);
  setCur(cx - textW(s, 1) / 2, y);
  printT(s);
}

// ---------- objetos ----------
static void decoCushion(int cx, int cy, int s, bool night) {
  uint16_t pk = nightDim(C565(0xf2, 0x9a, 0xb8), night), dk = uiLerp(pk, UI_INK, 7, 16);
  gfx->fillRoundRect(cx - 13 * s, cy - 9 * s / 2, 26 * s, 12 * s, 5 * s, pk);
  gfx->drawRoundRect(cx - 13 * s, cy - 9 * s / 2, 26 * s, 12 * s, 5 * s, dk);
  gfx->fillRoundRect(cx - 11 * s, cy - 15 * s / 2, 9 * s, 6 * s, 3 * s, nightDim(UI_WHITE, night));
  gfx->drawRoundRect(cx - 11 * s, cy - 15 * s / 2, 9 * s, 6 * s, 3 * s, dk);
  for (int i = 0; i < 3; i++) gfx->fillCircle(cx + (i - 1) * 7 * s, cy + s, s, dk);
}
static void decoPlant(int cx, int cy, int s, bool night) {
  uint16_t pot = nightDim(C565(0xc0, 0x6a, 0x3a), night), lf = nightDim(C565(0x4c, 0xb0, 0x50), night),
           lf2 = nightDim(C565(0x2e, 0x86, 0x3a), night);
  gfx->fillEllipse(cx - 6 * s, cy - 18 * s, 5 * s, 9 * s, lf2);
  gfx->fillEllipse(cx + 6 * s, cy - 18 * s, 5 * s, 9 * s, lf2);
  gfx->fillEllipse(cx, cy - 22 * s, 5 * s, 11 * s, lf);
  gfx->fillTriangle(cx - 9 * s, cy - 8 * s, cx + 9 * s, cy - 8 * s, cx + 6 * s, cy + 4 * s, pot);
  gfx->fillTriangle(cx - 9 * s, cy - 8 * s, cx - 6 * s, cy + 4 * s, cx + 6 * s, cy + 4 * s, pot);
  gfx->fillRect(cx - 10 * s, cy - 10 * s, 20 * s, 3 * s, uiLerp(pot, UI_INK, 5, 16));
}
static void decoBall(int cx, int cy, int s, bool night) {
  gfx->fillCircle(cx, cy, 8 * s, nightDim(C565(0xff, 0x5a, 0x4a), night));
  gfx->fillRect(cx - 8 * s, cy - 2 * s, 16 * s, 4 * s, nightDim(C565(0xff, 0xd8, 0x40), night));
  gfx->fillRect(cx - 2 * s, cy - 8 * s, 4 * s, 16 * s, nightDim(C565(0x3a, 0x8c, 0xff), night));
  gfx->drawCircle(cx, cy, 8 * s, UI_INK);
  gfx->fillCircle(cx - 3 * s, cy - 3 * s, 2 * s, nightDim(UI_WHITE, night));
}
static void decoLamp(int cx, int cy, int s, bool night) {
  uint16_t y = C565(0xff, 0xe0, 0x70);  // de noche brilla mas
  gfx->fillCircle(cx, cy - 16 * s, (night ? 16 : 10) * s, night ? uiLerp(y, C565(0x20, 0x24, 0x40), 9, 16) : uiLerp(y, UI_BG_DAY, 11, 16));
  gfx->fillRect(cx - s, cy - 8 * s, 2 * s, 12 * s, C565(0x6a, 0x50, 0x3a));
  gfx->fillRoundRect(cx - 6 * s, cy - 22 * s, 12 * s, 12 * s, 3 * s, y);
  gfx->drawRoundRect(cx - 6 * s, cy - 22 * s, 12 * s, 12 * s, 3 * s, UI_INK);
  gfx->fillRect(cx - 7 * s, cy + 3 * s, 14 * s, 2 * s, C565(0x6a, 0x50, 0x3a));
}
static void decoTrophy(int cx, int cy, int s, bool night) {
  uint16_t g = nightDim(C565(0xf0, 0xc0, 0x30), night), dk = uiLerp(g, UI_INK, 7, 16);
  gfx->fillRoundRect(cx - 8 * s, cy - 22 * s, 16 * s, 12 * s, 5 * s, g);
  gfx->drawCircle(cx - 9 * s, cy - 17 * s, 3 * s, g);
  gfx->drawCircle(cx + 9 * s, cy - 17 * s, 3 * s, g);
  gfx->fillRect(cx - 2 * s, cy - 10 * s, 4 * s, 7 * s, g);
  gfx->fillRect(cx - 7 * s, cy - 3 * s, 14 * s, 5 * s, nightDim(C565(0x8a, 0x5a, 0x3a), night));
  gfx->drawRoundRect(cx - 8 * s, cy - 22 * s, 16 * s, 12 * s, 5 * s, dk);
  gfx->fillCircle(cx - 3 * s, cy - 18 * s, s, nightDim(UI_WHITE, night));
}
static void decoDoll(int cx, int cy, int s, bool night) {  // muneco de peluche (osito)
  uint16_t b = nightDim(C565(0xc8, 0x8a, 0x58), night), dk = uiLerp(b, UI_INK, 8, 16);
  gfx->fillCircle(cx - 6 * s, cy - 24 * s, 3 * s, b);
  gfx->fillCircle(cx + 6 * s, cy - 24 * s, 3 * s, b);
  gfx->fillCircle(cx, cy - 18 * s, 7 * s, b);
  gfx->fillEllipse(cx, cy - 4 * s, 8 * s, 8 * s, b);
  gfx->fillCircle(cx, cy - 15 * s, 3 * s, nightDim(C565(0xf0, 0xd0, 0xa8), night));
  gfx->fillCircle(cx - 3 * s, cy - 19 * s, s, UI_INK);
  gfx->fillCircle(cx + 3 * s, cy - 19 * s, s, UI_INK);
  gfx->fillCircle(cx, cy - 16 * s, s, dk);
  gfx->fillEllipse(cx, cy - 2 * s, 4 * s, 4 * s, uiLerp(b, UI_WHITE, 6, 16));
}
void drawDecoItem(uint8_t item, int cx, int cy, int s, bool night) {
  switch (item) {
    case DECO_CUSHION: decoCushion(cx, cy, s, night); break;
    case DECO_PLANT: decoPlant(cx, cy, s, night); break;
    case DECO_BALL: decoBall(cx, cy - 4 * s, s, night); break;
    case DECO_LAMP: decoLamp(cx, cy, s, night); break;
    case DECO_TROPHY: decoTrophy(cx, cy, s, night); break;
    case DECO_DOLL: decoDoll(cx, cy, s, night); break;
  }
}
// los 3 sitios del suelo en la pantalla principal (pies en y)
static const int16_t DECO_POS[DECO_SLOTS][3] = { { 78, 300, 3 }, { 336, 304, 2 }, { 396, 300, 2 } };  // x, y, escala
void drawDecor(bool night) {
  for (uint8_t i = 0; i < DECO_SLOTS; i++) {
    uint8_t it = pet.deco[i];
    if (!it || it > DECO_COUNT) continue;
    int s = DECO_POS[i][2];
    if (it - 1 == DECO_CUSHION && i) s = 2;  // el cojin solo es grande a la izquierda
    drawDecoItem((uint8_t)(it - 1), DECO_POS[i][0], DECO_POS[i][1] - 12, s, night);
  }
}

// ---------- la habitacion (fondo) ----------
void drawRoom(uint32_t now, bool night) {  // night = luz apagada (dormido)
  int h = sceneHour();
  uint8_t wx = sceneWeather();
  const int floorY = 236;
  // pared con papel de rayas suaves
  uint16_t wall = nightDim(C565(0xf6, 0xe6, 0xd2), night), stripe = nightDim(C565(0xee, 0xd8, 0xc0), night);
  gfx->fillRect(0, 0, LCD_WIDTH, floorY, wall);
  for (int x = 10; x < LCD_WIDTH; x += 36) gfx->fillRect(x, 0, 12, floorY, stripe);
  // ventana (izquierda): el cielo de la hora y el tiempo
  const int wx0 = 40, wy0 = 96, ww = 92, wh = 104;
  uint16_t sky = (h >= 6 && h < 17) ? C565(0x8c, 0xc8, 0xf0) : (h >= 17 && h < 20) ? C565(0xf4, 0xa0, 0x70) : C565(0x1c, 0x24, 0x4c);
  if (wx == WX_RAIN || wx == WX_SNOW) sky = uiLerp(sky, C565(0x80, 0x88, 0x98), 9, 16);
  gfx->fillRect(wx0, wy0, ww, wh, sky);
  bool dark = !(h >= 6 && h < 20);
  if (dark) {
    gfx->fillCircle(wx0 + 64, wy0 + 26, 10, C565(0xf4, 0xf0, 0xd0));
    gfx->fillCircle(wx0 + 69, wy0 + 22, 9, sky);
    for (int k = 0; k < 6; k++) gfx->fillCircle(wx0 + 10 + (k * 29) % 80, wy0 + 12 + (k * 37) % 80, 1, UI_WHITE);
  } else if (wx != WX_RAIN && wx != WX_SNOW) {
    gfx->fillCircle(wx0 + 66, wy0 + 26, 11, C565(0xff, 0xe0, 0x60));
  }
  if (wx == WX_RAIN)
    for (int k = 0; k < 10; k++) {
      int x = wx0 + 6 + (k * 23 + (int)(now / 30)) % (ww - 12), y = wy0 + 6 + (k * 41 + (int)(now / 12)) % (wh - 18);
      gfx->drawLine(x, y, x - 3, y + 9, C565(0xd0, 0xe0, 0xf0));
    }
  if (wx == WX_SNOW)
    for (int k = 0; k < 10; k++) {
      int x = wx0 + 6 + (k * 23) % (ww - 12), y = wy0 + 6 + (k * 41 + (int)(now / 40)) % (wh - 12);
      gfx->fillCircle(x, y, 2, UI_WHITE);
    }
  uint16_t frame = nightDim(C565(0xb0, 0x80, 0x58), night);
  gfx->drawRect(wx0 - 1, wy0 - 1, ww + 2, wh + 2, frame);
  gfx->fillRect(wx0 - 5, wy0 - 5, ww + 10, 5, frame);
  gfx->fillRect(wx0 - 5, wy0 + wh, ww + 10, 7, frame);
  gfx->fillRect(wx0 + ww / 2 - 2, wy0, 4, wh, frame);
  gfx->fillRect(wx0, wy0 + wh / 2 - 2, ww, 4, frame);
  // cortinas
  uint16_t cur = nightDim(C565(0xe8, 0x86, 0x90), night);
  gfx->fillTriangle(wx0 - 12, wy0 - 6, wx0 + 16, wy0 - 6, wx0 - 12, wy0 + wh + 6, cur);
  gfx->fillTriangle(wx0 + ww + 12, wy0 - 6, wx0 + ww - 16, wy0 - 6, wx0 + ww + 12, wy0 + wh + 6, cur);
  gfx->fillRect(wx0 - 16, wy0 - 10, ww + 32, 5, nightDim(C565(0x8a, 0x5a, 0x3a), night));
  // cuadro (derecha)
  uint16_t pf = nightDim(C565(0x9a, 0x6a, 0x44), night);
  gfx->fillRect(352, 112, 64, 50, pf);
  gfx->fillRect(357, 117, 54, 40, nightDim(C565(0xbc, 0xe0, 0xf4), night));
  gfx->fillTriangle(360, 154, 382, 128, 404, 154, nightDim(C565(0x5a, 0x9a, 0x5a), night));
  gfx->fillCircle(398, 126, 5, nightDim(C565(0xff, 0xd8, 0x60), night));
  // zocalo y suelo de madera
  gfx->fillRect(0, floorY - 8, LCD_WIDTH, 8, nightDim(C565(0xd8, 0xb8, 0x94), night));
  uint16_t fl = nightDim(C565(0xc8, 0x94, 0x60), night), fl2 = nightDim(C565(0xb4, 0x80, 0x4e), night);
  gfx->fillRect(0, floorY, LCD_WIDTH, LCD_HEIGHT - floorY, fl);
  for (int y = floorY + 14, r = 0; y < LCD_HEIGHT; y += 16, r++) {
    gfx->drawFastHLine(0, y, LCD_WIDTH, fl2);
    for (int x = (r & 1) ? 40 : 0; x < LCD_WIDTH; x += 96) gfx->drawFastVLine(x, y - 15, 15, fl2);
  }
  // alfombra bajo el bicho
  gfx->fillEllipse(CX, 294, 120, 18, nightDim(C565(0x8c, 0xb4, 0xe0), night));
  gfx->drawEllipse(CX, 294, 112, 14, nightDim(C565(0xf0, 0xf4, 0xff), night));
  if (night) uiShade(0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 12);  // luz apagada
}

// ---------- pantalla "decorar" (ajustes) ----------
static int8_t roomSel = -1;  // objeto elegido para colocar
#define ROOM_TOG_Y 66
#define ROOM_PAN_Y 112
#define ROOM_GRID_Y 236
static const XId DECO_NAME[DECO_COUNT] = { X_DECO_CUSHION, X_DECO_PLANT, X_DECO_BALL, X_DECO_LAMP, X_DECO_TROPHY, X_DECO_DOLL };
static const XId DECO_HOW[DECO_COUNT] = { X_DECO_H_CUSHION, X_DECO_H_PLANT, X_DECO_H_BALL, X_DECO_H_LAMP, X_DECO_H_TROPHY, X_DECO_H_DOLL };
static const int16_t ROOM_SLOT_X[DECO_SLOTS] = { 118, 233, 348 };

void openRoom() { retMark(); roomSel = -1; xScreen = XS_ROOM; }

void renderRoomMenu() {
  uiScreenBg();
  drawFit(XT(X_ROOM_TITLE), 30, 300, UI_INK, 2);
  // fondo: paisaje / habitacion
  drawBtn(93, ROOM_TOG_Y, 136, 36, pet.roomOn ? UI_TRACK : UI_BAR_OK, pet.roomOn ? UI_INK : UI_WHITE, XT(X_ROOM_OUT));
  drawBtn(237, ROOM_TOG_Y, 136, 36, pet.roomOn ? UI_BAR_OK : UI_TRACK, pet.roomOn ? UI_WHITE : UI_INK, XT(X_ROOM_IN));
  // los 3 sitios
  uiPanel(48, ROOM_PAN_Y, 370, 112, 18, C565(0xfa, 0xf2, 0xe4), UI_INK);
  gfx->fillRect(60, ROOM_PAN_Y + 66, 346, 22, C565(0xd8, 0xc0, 0x98));
  static const XId SLOT_NM[DECO_SLOTS] = { X_ROOM_LEFT, X_ROOM_MID, X_ROOM_RIGHT };
  for (uint8_t i = 0; i < DECO_SLOTS; i++) {
    int cx = ROOM_SLOT_X[i];
    if (roomSel >= 0) gfx->drawRoundRect(cx - 52, ROOM_PAN_Y + 8, 104, 84, 12, UI_BAR_WARN);
    if (pet.deco[i]) drawDecoItem((uint8_t)(pet.deco[i] - 1), cx, ROOM_PAN_Y + 72, 2, false);
    homeTextAt(XT(SLOT_NM[i]), cx, ROOM_PAN_Y + 90, UI_INK);
  }
  // los objetos
  uint16_t dc = dexDiscoveredCount();
  for (int i = 0; i < DECO_COUNT; i++) {
    int x = 60 + (i % 2) * 176, y = ROOM_GRID_Y + (i / 2) * 52;
    bool got = pet.decoUnlocked((uint8_t)i, dc);
    bool sel = roomSel == i;
    uiPanel(x, y, 168, 46, 12, sel ? C565(0xff, 0xf0, 0xc0) : got ? UI_WHITE : C565(0xe4, 0xe0, 0xd8),
            sel ? UI_BAR_WARN : got ? UI_INK : 0x8410);
    int cx = x + 26, cy = y + 34;
    if (got) drawDecoItem((uint8_t)i, cx, cy, 1, false);
    else { gfx->drawCircle(cx, cy - 16, 6, 0x8410); gfx->fillRoundRect(cx - 9, cy - 14, 18, 14, 3, 0x8410); }
    drawFitIn(XT(DECO_NAME[i]), x + 50, y + 4, 114, got ? UI_INK : 0x8410, 1);
    drawFitIn(XT(DECO_HOW[i]), x + 50, y + 24, 114, 0x8410, 1);
  }
  drawFit(XT(roomSel >= 0 ? X_ROOM_HINT2 : X_ROOM_HINT), 396, 280, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void roomTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); goBack(); return; }
  if (y >= ROOM_TOG_Y && y < ROOM_TOG_Y + 36) {
    if (x >= 93 && x < 229) pet.roomOn = 0;
    else if (x >= 237 && x < 373) pet.roomOn = 1;
    else return;
    pet.saveNow();
    sfxPlay(SFX_TAP);
    return;
  }
  if (y >= ROOM_PAN_Y && y < ROOM_PAN_Y + 112) {  // un sitio: poner lo elegido / quitar
    for (uint8_t i = 0; i < DECO_SLOTS; i++)
      if (x >= ROOM_SLOT_X[i] - 56 && x < ROOM_SLOT_X[i] + 56) {
        if (roomSel >= 0) {
          for (uint8_t k = 0; k < DECO_SLOTS; k++) if (pet.deco[k] == roomSel + 1) pet.deco[k] = 0;  // cada objeto en un sitio
          pet.deco[i] = (uint8_t)(roomSel + 1);
          roomSel = -1;
          sfxPlay(SFX_MEDAL);
        } else if (pet.deco[i]) {
          pet.deco[i] = 0;
          sfxPlay(SFX_TAP);
        }
        pet.saveNow();
        return;
      }
    return;
  }
  if (y >= ROOM_GRID_Y && y < ROOM_GRID_Y + 3 * 52 && x >= 60 && x < 404) {
    int i = ((y - ROOM_GRID_Y) / 52) * 2 + (x >= 236 ? 1 : 0);
    if (i >= DECO_COUNT) return;
    if (!pet.decoUnlocked((uint8_t)i, dexDiscoveredCount())) { sfxPlay(SFX_DENY); return; }
    roomSel = roomSel == i ? -1 : i;
    sfxPlay(SFX_TAP);
  }
}

// ---------- sensor de movimiento (QMI8658) y podometro ----------
static uint8_t imuAddr = 0;
bool imuOk() { return imuAddr != 0; }
static bool imuW(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}
void imuBegin() {
  imuAddr = 0;
  for (uint8_t a : { (uint8_t)0x6B, (uint8_t)0x6A }) {
    Wire.beginTransmission(a);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) continue;
    if (Wire.requestFrom(a, (uint8_t)1) == 1 && Wire.read() == 0x05) { imuAddr = a; break; }
  }
  if (!imuAddr) return;
  // CTRL1: direcciones que avanzan solas; CTRL2: acelerometro +-4 g a 62,5 Hz; CTRL7: solo acelerometro
  bool ok = imuW(0x02, 0x40) && imuW(0x03, 0x17) && imuW(0x08, 0x01);
  if (!ok) imuAddr = 0;
  Serial.printf("IMU %s\n", imuAddr ? "ok" : "fallo");
}
// lectura del acelerometro en milesimas de g
static bool imuRead(int32_t &ax, int32_t &ay, int32_t &az) {
  Wire.beginTransmission(imuAddr);
  Wire.write(0x35);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(imuAddr, (uint8_t)6) != 6) return false;
  int16_t v[3];
  for (int i = 0; i < 3; i++) { uint8_t lo = Wire.read(), hi = Wire.read(); v[i] = (int16_t)(lo | (hi << 8)); }
  ax = v[0] * 1000 / 8192; ay = v[1] * 1000 / 8192; az = v[2] * 1000 / 8192;  // 4 g -> 8192 LSB/g
  return true;
}
static uint32_t walkDay() {
  uint32_t e = clockEpoch();
  return e ? e / 86400UL : 0;
}
// pasos: picos de la aceleracion (sin la gravedad) separados 0,3-2 s; se cuentan en rachas de 4+
// para no sumar golpes sueltos. Agitar fuerte = sacudida
static uint32_t imuT = 0, stepLastT = 0, shakeT = 0;
static int32_t imuBase = 1000;
static bool stepUp = false;
static uint8_t stepRun = 0, shakeHits = 0;
static uint32_t shakeWin = 0;
static uint8_t walkPend = 0;  // pasos de una racha aun sin confirmar
void walkReward(uint8_t got);
void imuPoll(uint32_t now) {
  if (!imuAddr || now - imuT < 35) return;
  imuT = now;
  int32_t ax, ay, az;
  if (!imuRead(ax, ay, az)) return;
  float m = sqrtf((float)(ax * ax + ay * ay + az * az));
  imuBase += ((int32_t)m - imuBase) / 16;  // ~0,5 s de media: la gravedad
  int32_t d = (int32_t)m - imuBase;
  // sacudida: 4 golpes de mas de 1,2 g en 1 s
  if (d > 1200 || d < -900) {
    if (now - shakeWin > 1000) { shakeWin = now; shakeHits = 0; }
    if (++shakeHits >= 4 && now - shakeT > 4000) {
      shakeT = now;
      shakeHits = 0;
      lastInteract = now;
      if (pet.wakeAuto()) { sfxPlay(SFX_TAP); showToast(XT(X_WALK_SHAKE_WAKE)); }
      else if (!pet.isEgg() && !pet.sleeping && !pet.ceremony) { pet.caress(); sfxPlay(SFX_PLAY); showToast(XT(X_WALK_SHAKE_JOY)); }
    }
    return;  // agitar no son pasos
  }
  if (!stepUp && d > 130) {
    stepUp = true;
    uint32_t dt = now - stepLastT;
    stepLastT = now;
    if (dt < 280) return;  // rebote del mismo paso
    if (dt > 2000) { stepRun = 0; walkPend = 0; }
    stepRun = stepRun < 255 ? stepRun + 1 : 255;
    if (stepRun < 4) { walkPend++; return; }
    uint16_t n = 1 + walkPend;
    walkPend = 0;
    uint32_t day = walkDay();
    if (!day) return;
    uint8_t got = pet.addSteps(n, day);
    if (got) walkReward(got);
  } else if (stepUp && d < 40) {
    stepUp = false;
  }
}
void walkReward(uint8_t got) {
  static const XId RW[3] = { X_WALK_RW1, X_WALK_RW2, X_WALK_RW3 };
  for (int k = 2; k >= 0; k--) if (got & (1 << k)) { showToast(XT(RW[k])); sfxPlay(SFX_MEDAL); break; }
}

// ---------- el contador en la pantalla principal (toque = pantalla del paseo) ----------
#define WALK_PILL_X 30
#define WALK_PILL_Y 110
#define WALK_PILL_W 92
#define WALK_PILL_H 26
static void drawFootIcon(int x, int y, uint16_t c) {
  gfx->fillEllipse(x, y + 3, 4, 6, c);
  for (int i = 0; i < 3; i++) gfx->fillCircle(x - 4 + i * 4, y - 6, 2, c);
}
void drawWalkPill() {
  if (!imuOk()) return;
  uiPanel(WALK_PILL_X, WALK_PILL_Y, WALK_PILL_W, WALK_PILL_H, 12, UI_WHITE, UI_INK);
  drawFootIcon(WALK_PILL_X + 16, WALK_PILL_Y + 13, C565(0x8a, 0x5a, 0x3a));
  char b[12];
  snprintf(b, sizeof(b), "%u", (unsigned)pet.stepsToday(walkDay()));
  drawFitIn(b, WALK_PILL_X + 28, WALK_PILL_Y + 5, WALK_PILL_W - 32, UI_INK, 1);
}
bool walkPillHit(int16_t x, int16_t y) {
  return imuOk() && x >= WALK_PILL_X - 6 && x < WALK_PILL_X + WALK_PILL_W + 6 && y >= WALK_PILL_Y - 8 && y < WALK_PILL_Y + WALK_PILL_H + 6;
}

// ---------- pantalla del paseo ----------
void openWalk() { retMark(); xScreen = XS_WALK; }
void renderWalk() {
  uiScreenBg();
  drawFit(XT(X_WALK_TITLE), 34, 300, UI_INK, 2);
  if (!imuOk()) {
    drawFit(XT(X_WALK_NOSENSOR), 200, 340, UI_INK, 2);
    drawFit(XT(X_WALK_NOSENSOR2), 236, 340, 0x8410, 1);
    drawNav(NAV_L, UI_INK);
    uiFlush();
    return;
  }
  uint32_t day = walkDay();
  uint16_t today = pet.stepsToday(day);
  char b[64];
  snprintf(b, sizeof(b), XT(X_WALK_STEPS_FMT), (unsigned)today);
  drawFootIcon(118, 92, C565(0x8a, 0x5a, 0x3a));
  drawFootIcon(132, 80, C565(0x8a, 0x5a, 0x3a));
  drawFit(b, 82, 260, UI_INK, 3);
  int bx = 83, bw = 300, by = 130;
  uint32_t goal = today < WALK_GOAL2 ? WALK_GOAL2 : WALK_GOAL3;
  gfx->fillRoundRect(bx, by, bw, 18, 9, UI_TRACK);
  int fw = (int)((uint32_t)bw * (today > goal ? goal : today) / goal);
  if (fw > 0) gfx->fillRoundRect(bx, by, fw < 18 ? 18 : fw, 18, 9, UI_BAR_OK);
  gfx->drawRoundRect(bx, by, bw, 18, 9, UI_INK);
  if (today < goal) snprintf(b, sizeof(b), XT(X_WALK_LEFT_FMT), (unsigned)goal, (unsigned)(goal - today));
  else snprintf(b, sizeof(b), "%s", XT(X_WALK_DONE));
  drawFit(b, 158, 320, 0x8410, 1);
  // la semana (hoy a la derecha)
  uint16_t mx = 2000;
  for (int i = 0; i < 7; i++) if (pet.walk.days[i] > mx) mx = pet.walk.days[i];
  for (int i = 0; i < 7; i++) {
    uint16_t v = pet.walk.days[6 - i];
    int x = 103 + i * 38, h = v ? 4 + (int)((uint32_t)v * 76 / mx) : 2, base = 270;
    uint16_t c = i == 6 ? C565(0xff, 0xb0, 0x40) : v >= WALK_GOAL2 ? UI_BAR_OK : C565(0xb8, 0xc8, 0xd8);
    gfx->fillRoundRect(x, base - h, 24, h, 5, c);
  }
  homeTextAt(XT(X_WALK_WEEK), CX, 276, 0x8410);
  // premios de hoy
  static const XId RL[3] = { X_WALK_RL1, X_WALK_RL2, X_WALK_RL3 };
  for (int i = 0; i < 3; i++) {
    int y = 300 + i * 24;
    bool ok = pet.walk.rw & (1 << i);
    gfx->fillCircle(112, y + 8, 7, ok ? UI_BAR_OK : UI_TRACK);
    drawFitIn(XT(RL[i]), 126, y, 240, ok ? UI_INK : 0x8410, 1);
  }
  snprintf(b, sizeof(b), XT(X_WALK_TOTAL_FMT), (unsigned long)pet.walk.total);
  drawFit(b, 376, 300, 0x8410, 1);
  drawFit(XT(X_WALK_SHAKE_HINT), 398, 280, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}
void walkTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y) || y < 60) { sfxPlay(SFX_TAP); goBack(); }
}

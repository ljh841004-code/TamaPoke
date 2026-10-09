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
  if (pet.endSeen) {  // ko12.7: trofeo de maestro: estrella encima
    uint16_t st = nightDim(C565(0xff, 0xe8, 0x80), night);
    gfx->fillTriangle(cx - 5 * s, cy - 24 * s, cx + 5 * s, cy - 24 * s, cx, cy - 31 * s, st);
    gfx->fillTriangle(cx - 5 * s, cy - 28 * s, cx + 5 * s, cy - 28 * s, cx, cy - 21 * s, st);
  }
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
  const int wx0 = 334, wy0 = 96, ww = 92, wh = 104;  // ko12.4.1: a la derecha (a la izquierda va el contador de pasos)
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
  // estante con un libro y un reloj (izquierda, bajo el contador de pasos)
  uint16_t sh = nightDim(C565(0x9a, 0x6a, 0x44), night);
  gfx->fillRect(40, 216, 100, 6, sh);
  gfx->fillRect(52, 198, 10, 18, nightDim(C565(0x5a, 0x8a, 0xd0), night));
  gfx->fillRect(64, 202, 8, 14, nightDim(C565(0xe0, 0x70, 0x60), night));
  gfx->fillCircle(116, 206, 9, nightDim(UI_WHITE, night));
  gfx->drawCircle(116, 206, 9, sh);
  gfx->drawLine(116, 206, 116, 200, UI_INK);
  gfx->drawLine(116, 206, 120, 208, UI_INK);
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
// pasos: picos de la aceleracion (sin la gravedad) separados 0,28-2 s. ko12.4.1: los pasos se guardan
// aparte y solo se suman cuando la racha termina tranquila (1,5 s sin pasos ni golpes) o ya es larga
// (16+ y sin golpes en 3 s): agitar la placa daba 4 pasos (prueba en el aparato). Un golpe fuerte
// (sacudida) borra lo pendiente. Las rachas de menos de 4 pasos no cuentan
static uint32_t imuT = 0, stepLastT = 0, shakeT = 0, bigT = 0;
static int32_t imuBase = 1000;
static bool stepUp = false;
static uint8_t stepRun = 0, shakeHits = 0;
static uint32_t shakeWin = 0;
static uint16_t walkPend = 0;  // pasos aun sin sumar
void walkReward(uint8_t got);
static void walkFlush() {
  uint16_t n = walkPend;
  walkPend = 0;
  uint32_t day = walkDay();
  if (!n || !day) return;
  // ko12.6: andar bajo la lluvia (rachas largas) puede resfriarlo
  if (n >= 16 && sceneWeather() == WX_RAIN && !pet.sleeping && (int)random(100) < SICK_RAIN_PCT && pet.catchCold()) pet.sickNote = 3;
  uint8_t got = pet.addSteps(n, day);
  if (got) walkReward(got);
}
void imuPoll(uint32_t now) {
  if (!imuAddr || now - imuT < 35) return;
  imuT = now;
  // la racha termino tranquila: se suma (si tuvo 4+ pasos)
  if (walkPend && now - stepLastT > 1500 && now - bigT > 1500) {
    if (stepRun >= 4) walkFlush(); else walkPend = 0;
    stepRun = 0;
  }
  int32_t ax, ay, az;
  if (!imuRead(ax, ay, az)) return;
  float m = sqrtf((float)(ax * ax + ay * ay + az * az));
  imuBase += ((int32_t)m - imuBase) / 16;  // ~0,5 s de media: la gravedad
  int32_t d = (int32_t)m - imuBase;
  if (d > 700 || d < -600) {  // golpe fuerte: no es andar (lo pendiente se tira)
    bigT = now;
    walkPend = 0;
    stepRun = 0;
    stepUp = false;
    if (d > 1200 || d < -900) {  // sacudida: 4 golpes de mas de 1,2 g en 1 s
      if (now - shakeWin > 1000) { shakeWin = now; shakeHits = 0; }
      if (++shakeHits >= 4 && now - shakeT > 4000) {
        shakeT = now;
        shakeHits = 0;
        lastInteract = now;
        if (pet.wakeAuto()) { sfxPlay(SFX_TAP); showToast(XT(X_WALK_SHAKE_WAKE)); }
        else if (!pet.isEgg() && !pet.sleeping && !pet.ceremony) { pet.caress(); sfxPlay(SFX_PLAY); showToast(XT(X_WALK_SHAKE_JOY)); }
      }
    }
    return;
  }
  if (now - bigT < 1500) return;  // justo despues de agitar: nada
  if (!stepUp && d > 130) {
    stepUp = true;
    uint32_t dt = now - stepLastT;
    if (dt < 280) return;  // rebote del mismo paso
    stepLastT = now;
    if (dt > 2000) { stepRun = 0; walkPend = 0; }
    if (stepRun < 255) stepRun++;
    if (walkPend < 60000) walkPend++;
    if (stepRun >= 16 && walkPend >= 16 && now - bigT > 3000) walkFlush();  // andando: se ve subir
  } else if (stepUp && d < 40) {
    stepUp = false;
  }
}
void walkReward(uint8_t got) {
  static const XId RW[3] = { X_WALK_RW1, X_WALK_RW2, X_WALK_RW3 };
  for (int k = 2; k >= 0; k--) if (got & (1 << k)) { showToast(XT(RW[k])); sfxPlay(SFX_MEDAL); break; }
}

// ---------- el contador en la pantalla principal (toque = pantalla del paseo) ----------
// ko12.4.1: anillo que se llena hacia la meta de hoy (verde; dorado al pasar 5.000; arcoiris a 10.000)
// con la huella dentro y el numero debajo. A la izquierda del reloj, dentro del circulo de la pantalla
// (antes una pastilla mas arriba: en la pantalla redonda se cortaba el borde y tapaba la ventana)
#define WALK_RING_X 92
#define WALK_RING_Y 150
#define WALK_RING_R 22
static void drawFootIcon(int x, int y, uint16_t c) {
  gfx->fillEllipse(x, y + 3, 4, 6, c);
  for (int i = 0; i < 3; i++) gfx->fillCircle(x - 4 + i * 4, y - 6, 2, c);
}
static void fmtSteps(char *b, size_t n, uint32_t v) {  // 12,345
  if (v >= 1000) snprintf(b, n, "%lu,%03lu", (unsigned long)(v / 1000), (unsigned long)(v % 1000));
  else snprintf(b, n, "%lu", (unsigned long)v);
}
void drawWalkRing(int cx, int cy, int r, uint16_t today, uint32_t now) {
  gfx->fillCircle(cx, cy, r + 3, UI_WHITE);
  gfx->drawCircle(cx, cy, r + 3, UI_INK);
  // la pista y lo andado (empieza arriba, sentido horario)
  uint32_t goal = today < WALK_GOAL2 ? WALK_GOAL2 : WALK_GOAL3;
  float fr = today >= WALK_GOAL3 ? 1.0f : (float)today / goal;
  const int N = 40;
  for (int k = 0; k < N; k++) {
    float a = -1.5708f + k * 6.2832f / N;
    int x = cx + (int)(cosf(a) * (r - 2)), y = cy + (int)(sinf(a) * (r - 2));
    uint16_t c = UI_TRACK;
    if (k < (int)(fr * N + 0.5f))
      c = today >= WALK_GOAL3 ? orbHue(k / (float)N + now * 0.0003f) : today >= WALK_GOAL2 ? C565(0xf0, 0xc0, 0x30) : UI_BAR_OK;
    gfx->fillCircle(x, y, 3, c);
  }
  drawFootIcon(cx - 4, cy + 2, C565(0x8a, 0x5a, 0x3a));
  drawFootIcon(cx + 5, cy - 4, C565(0x8a, 0x5a, 0x3a));
}
void drawWalkPill() {
  if (!imuOk()) return;
  uint16_t today = pet.stepsToday(walkDay());
  drawWalkRing(WALK_RING_X, WALK_RING_Y, WALK_RING_R, today, millis());
  char b[12];
  fmtSteps(b, sizeof(b), today);
  int w = textW(b, 1) + 12;
  uiPanel(WALK_RING_X - w / 2, WALK_RING_Y + WALK_RING_R + 4, w, 20, 9, UI_WHITE, UI_INK);
  homeTextAt(b, WALK_RING_X, WALK_RING_Y + WALK_RING_R + 6, UI_INK);
}
bool walkPillHit(int16_t x, int16_t y) {
  int dx = x - WALK_RING_X, dy = y - (WALK_RING_Y + 10);
  return imuOk() && dx * dx + dy * dy <= 40 * 40;
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
  { char nb[12]; fmtSteps(nb, sizeof(nb), today); snprintf(b, sizeof(b), XT(X_WALK_STEPS_FMT), nb); }
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

// ---------- ko12.4.1: elegir el fondo (una vez, al actualizar o al empezar) ----------
#define BGQ_Y 120
static bool bgAskShown() { return !pet.bgAsked && !pet.isEgg() && !pet.ceremony && !pet.awaitingStarter(); }
static void bgMini(int x, int y, bool room) {  // miniatura de cada fondo
  if (room) {
    gfx->fillRect(x, y, 110, 52, C565(0xf6, 0xe6, 0xd2));
    for (int i = 4; i < 110; i += 14) gfx->fillRect(x + i, y, 5, 52, C565(0xee, 0xd8, 0xc0));
    gfx->fillRect(x + 70, y + 8, 30, 30, C565(0x8c, 0xc8, 0xf0));
    gfx->drawRect(x + 70, y + 8, 30, 30, C565(0xb0, 0x80, 0x58));
    gfx->fillRect(x, y + 52, 110, 26, C565(0xc8, 0x94, 0x60));
    gfx->fillEllipse(x + 55, y + 62, 34, 6, C565(0x8c, 0xb4, 0xe0));
  } else {
    gfx->fillRect(x, y, 110, 52, C565(0x8c, 0xc8, 0xf0));
    gfx->fillCircle(x + 88, y + 16, 9, C565(0xff, 0xe0, 0x60));
    gfx->fillTriangle(x + 10, y + 52, x + 46, y + 22, x + 82, y + 52, C565(0x7a, 0xa8, 0x6a));
    gfx->fillRect(x, y + 52, 110, 26, C565(0x8c, 0xc0, 0x5c));
  }
  gfx->drawRect(x, y, 110, 78, UI_INK);
}
bool bgAskDraw() {
  if (!bgAskShown()) return false;
  uiShade(0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 9);
  uiPanel(58, BGQ_Y, 350, 230, 20, UI_WHITE, UI_INK);
  drawFit(XT(X_BGQ_TITLE), BGQ_Y + 16, 320, UI_INK, 2);
  bgMini(88, BGQ_Y + 54, false);
  bgMini(268, BGQ_Y + 54, true);
  drawBtn(88, BGQ_Y + 142, 110, 40, UI_BAR_OK, UI_WHITE, XT(X_ROOM_OUT));
  drawBtn(268, BGQ_Y + 142, 110, 40, C565(0xe8, 0x80, 0xa8), UI_WHITE, XT(X_ROOM_IN));
  drawFit(XT(X_BGQ_NOTE), BGQ_Y + 194, 300, 0x8410, 1);
  return true;
}
// true = el toque era del dialogo
bool bgAskTap(int16_t x, int16_t y) {
  if (!bgAskShown()) return false;
  if (y < BGQ_Y + 50 || y > BGQ_Y + 186) return true;  // fuera de las opciones: el dialogo sigue
  if (x >= 80 && x < 206) pet.roomOn = 0;
  else if (x >= 260 && x < 386) pet.roomOn = 1;
  else return true;
  pet.bgAsked = 1;
  pet.saveNow();
  sfxPlay(SFX_MEDAL);
  return true;
}

// ======================================================================
// ko12.5: educar (berrinche), rutina del dia y caracter
// ======================================================================
static bool tantrumDlg = false;
#define TT_Y 150
void tantrumMark(uint32_t now) {  // "!" rojo sobre el bicho mientras dura el berrinche
  if (!pet.tantrum || pet.sleeping) return;
  int x = CX + 64, y = 176 + (int)((now / 120) % 2) * 3;  // junto a la cabeza (debajo del reloj)
  gfx->fillCircle(x, y, 16, UI_WHITE);
  gfx->drawCircle(x, y, 16, UI_BAR_BAD);
  gfx->fillTriangle(x - 10, y + 10, x - 2, y + 14, x - 14, y + 20, UI_WHITE);
  gfx->fillRoundRect(x - 3, y - 10, 6, 13, 3, UI_BAR_BAD);
  gfx->fillCircle(x, y + 8, 3, UI_BAR_BAD);
}
bool tantrumDraw() {
  if (!tantrumDlg) return false;
  if (!pet.tantrum) { tantrumDlg = false; return false; }
  uiShade(0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 9);
  uiPanel(68, TT_Y, 330, 168, 20, UI_WHITE, UI_INK);
  drawFit(XT(X_TT_TITLE), TT_Y + 16, 300, UI_BAR_BAD, 2);
  drawFit(XT(X_TT_SUB), TT_Y + 48, 300, 0x8410, 1);
  drawBtn(90, TT_Y + 82, 136, 48, UI_BAR_BAD, UI_WHITE, XT(X_TT_SCOLD));
  drawBtn(240, TT_Y + 82, 136, 48, C565(0xe8, 0x80, 0xa8), UI_WHITE, XT(X_TT_SOOTHE));
  drawFit(XT(X_TT_HINT), TT_Y + 140, 300, 0x8410, 1);
  return true;
}
// true = el toque era suyo
bool tantrumTap(int16_t x, int16_t y) {
  if (tantrumDlg) {
    if (y >= TT_Y + 82 && y < TT_Y + 130) {
      if (x >= 90 && x < 226) { pet.scold(); sfxPlay(SFX_DENY); showToast(XT(X_TT_SCOLDED)); }
      else if (x >= 240 && x < 376) { pet.soothe(); sfxPlay(SFX_HEART); showToast(XT(X_TT_SOOTHED)); }
      else return true;
      tantrumDlg = false;
    } else if (y < TT_Y || y > TT_Y + 168) tantrumDlg = false;  // fuera: cerrar
    return true;
  }
  if (pet.tantrum && !pet.sleeping && inPetZone(x, y)) { tantrumDlg = true; sfxPlay(SFX_TAP); return true; }
  return false;
}
// avisos de la rutina (los pone Pet::routineDo)
static void tama126Loop();
void tamaLoop() {
  tama126Loop();  // ko12.6
  if (!pet.rtNote) return;
  static const XId RT_T[3] = { X_RT_DONE_MEAL, X_RT_DONE_PLAY, X_RT_DONE_BED };
  uint8_t n = pet.rtNote;
  pet.rtNote = 0;
  if (n == 9) { char b[64]; snprintf(b, sizeof(b), XT(X_RT_DONE_ALL_FMT), (unsigned)pet.rtStreak); showToast(b); sfxPlay(SFX_MEDAL); }
  else if (n >= 1 && n <= 3) showToast(XT(RT_T[n - 1]));
}

// pagina "생활" de la ficha: caracter, educacion y rutina de hoy
static const XId PERS_NM[PERS_COUNT] = { X_PERS_NONE, X_PERS_GLUTTON, X_PERS_PLAYFUL, X_PERS_CUDDLY, X_PERS_HARDWORK, X_PERS_TIDY, X_PERS_CALM };
static const XId PERS_DS[PERS_COUNT] = { X_PERSD_NONE, X_PERSD_GLUTTON, X_PERSD_PLAYFUL, X_PERSD_CUDDLY, X_PERSD_HARDWORK, X_PERSD_TIDY, X_PERSD_CALM };
const char *personalityName(uint8_t p) { return XT(PERS_NM[p < PERS_COUNT ? p : 0]); }
void renderCardLife() {
  drawFit(XT(X_LIFE_TITLE), 40, 300, UI_INK, 3);
  uint8_t ps = pet.personality();
  char b[80];
  snprintf(b, sizeof(b), XT(X_LIFE_PERS_FMT), personalityName(ps));
  drawFit(b, 84, 340, UI_INK, 2);
  drawFit(XT(PERS_DS[ps]), 112, 340, 0x8410, 1);
  // educacion
  drawFitIn(XT(X_LIFE_DISC), 76, 142, 120, UI_INK, 2);
  int bx = 190, bw = 190, by = 146;
  gfx->fillRoundRect(bx, by, bw, 16, 8, UI_TRACK);
  int fw = bw * pet.discipline / 100;
  if (fw > 0) gfx->fillRoundRect(bx, by, fw < 16 ? 16 : fw, 16, 8, C565(0x6a, 0x4c, 0xf0));
  gfx->drawRoundRect(bx, by, bw, 16, 8, UI_INK);
  if (pet.tantrum) drawFit(XT(X_LIFE_TANTRUM), 172, 320, UI_BAR_BAD, 1);
  // rutina de hoy
  uint8_t bits = pet.routineToday();
  drawFit(XT(X_LIFE_ROUTINE), 196, 300, UI_INK, 2);
  static const XId RT_L[3] = { X_RT_MEAL, X_RT_PLAY, X_RT_BED };
  for (int i = 0; i < 3; i++) {
    int y = 228 + i * 28;
    bool ok = bits & (1 << i);
    gfx->fillCircle(104, y + 9, 9, ok ? UI_BAR_OK : UI_TRACK);
    if (ok) { gfx->drawLine(99, y + 9, 103, y + 13, UI_WHITE); gfx->drawLine(103, y + 13, 110, y + 5, UI_WHITE); }
    drawFitIn(XT(RT_L[i]), 122, y, 250, ok ? UI_INK : 0x8410, 1);
  }
  snprintf(b, sizeof(b), XT(X_LIFE_STREAK_FMT), (unsigned)pet.rtStreak, (unsigned)pet.rtBest);
  drawFit(b, 318, 320, UI_INK, 1);
  // ko12.7: cuanto falta del viaje
  uint16_t tot = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) if (DEX_FAM[d] == d) tot++;
  if (pet.lap) snprintf(b, sizeof(b), XT(X_LIFE_SHINY_FMT), pet.famsShinyCount(), tot);
  else snprintf(b, sizeof(b), XT(X_LIFE_JOURNEY_FMT), pet.famsRaisedCount(), tot);
  drawFit(b, 344, 320, pet.lap ? C565(0xd0, 0x90, 0x10) : 0x8410, 1);
}

// ======================================================================
// ko12.6: resfriado (y medicina), visitas de la caja y cumpleanos
// ======================================================================
extern Box box;

// ---- resfriado: burbuja con un termometro junto a la cabeza (al otro lado que el berrinche) ----
static bool sickDlg = false;
#define SK_Y 140
void sickMark(uint32_t now) {
  if (!pet.sick || pet.sleeping || pet.isEgg()) return;
  int x = CX - 64, y = 176 + (int)((now / 400) % 2) * 3;
  gfx->fillCircle(x, y, 16, UI_WHITE);
  gfx->drawCircle(x, y, 16, C565(0x3a, 0x8c, 0xe0));
  gfx->fillTriangle(x + 10, y + 10, x + 2, y + 14, x + 14, y + 20, UI_WHITE);
  // termometro inclinado
  for (int k = -1; k <= 1; k++) gfx->drawLine(x - 7 + k, y + 7, x + 6 + k, y - 8, UI_INK);
  gfx->drawLine(x - 7, y + 7, x - 1, y, UI_BAR_BAD);
  gfx->fillCircle(x - 8, y + 8, 4, UI_BAR_BAD);
}
bool sickDraw() {
  if (!sickDlg) return false;
  if (!pet.sick) { sickDlg = false; return false; }
  uiShade(0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 9);
  uiPanel(68, SK_Y, 330, 188, 20, UI_WHITE, UI_INK);
  drawFit(XT(X_SICK_TITLE), SK_Y + 16, 300, C565(0x3a, 0x8c, 0xe0), 2);
  char b[64];
  if (pet.sickWait) snprintf(b, sizeof(b), XT(X_SICK_WAIT_FMT), (unsigned)pet.sickWait);
  else snprintf(b, sizeof(b), XT(X_SICK_SUB_FMT), (unsigned)pet.sickDoses);
  drawFit(b, SK_Y + 48, 300, 0x8410, 1);
  drawBtn(90, SK_Y + 82, 136, 48, pet.sickWait ? UI_TRACK : C565(0x3a, 0x8c, 0xe0), pet.sickWait ? 0x8410 : UI_WHITE, XT(X_SICK_GIVE));
  drawBtn(240, SK_Y + 82, 136, 48, UI_WHITE, UI_INK, XT(X_SICK_LATER));
  drawFit(XT(X_SICK_HINT), SK_Y + 146, 300, 0x8410, 1);
  return true;
}
bool sickTap(int16_t x, int16_t y) {
  if (sickDlg) {
    if (y >= SK_Y + 82 && y < SK_Y + 130 && x >= 90 && x < 226) {
      uint8_t r = pet.giveMedicine();
      if (r == 3) { sfxPlay(SFX_DENY); return true; }
      sfxPlay(r == 2 ? SFX_MEDAL : SFX_EAT);
      if (r) showToast(XT(r == 2 ? X_SICK_CURED : X_SICK_ONE_MORE));
    } else if (!(y >= SK_Y + 82 && y < SK_Y + 130 && x >= 240 && x < 376) && y >= SK_Y && y <= SK_Y + 188) {
      return true;  // dentro del panel pero fuera de los botones: nada
    }
    sickDlg = false;
    return true;
  }
  if (pet.sick && !pet.sleeping && !pet.isEgg() && inPetZone(x, y)) { sickDlg = true; sfxPlay(SFX_TAP); return true; }
  return false;
}

// ---- visitas: de vez en cuando uno de la caja viene a jugar un rato ----
#define VISIT_MS (15UL * 60 * 1000)  // se queda 15 min
#define VISIT_ODDS 240               // 1 de cada 240 minutos de dia (8-21 h: ~3 al dia como mucho)
#define VISIT_MAX_DAY 3
#define VISIT_X 344
#define VISIT_Y 246
int16_t visitDex = 0;
static uint32_t visitUntil = 0, visitByeAt = 0, visitCheckT = 0, visitDay = 0;
static uint8_t visitsToday = 0;
static bool visitPlayed = false;
static void visitName(char *out, size_t n) {
  snprintf(out, n, "%s", dexName(visitDex));
}
static void visitEnd() {
  char nm[32], b[96];
  visitName(nm, sizeof(nm));
  txFmt(b, sizeof(b), X_VISIT_BYE_FMT, nm);
  showToast(b);
  visitDex = 0;
  visitUntil = visitByeAt = 0;
}
void visitStart(uint8_t boxIdx) {  // tambien la usa la consola serie / las pruebas
  if (boxIdx >= box.count()) return;
  visitDex = box.at(boxIdx).dex;
  visitUntil = millis() + VISIT_MS;
  if (!visitUntil) visitUntil = 1;
  visitByeAt = 0;
  visitPlayed = false;
  char nm[32], b[96];
  visitName(nm, sizeof(nm));
  txFmt(b, sizeof(b), X_VISIT_FMT, nm);
  showToast(b);
  sfxPlay(SFX_HEART);
}
// ko12.9.1: antes solo se tiraba el dado con el bicho despierto, y se duerme solo a los 3 min sin
// tocarlo (ko12.2): casi nunca venia nadie. Ahora el dado corre de dia aunque duerma o la pantalla
// este apagada; si sale, el amigo "espera" y llega en cuanto se vuelve a ver la pantalla principal
static bool visitPending = false;
static void visitPoll(uint32_t now) {
  if (visitDex) {
    if (visitByeAt && (int32_t)(now - visitByeAt) >= 0) visitEnd();
    else if (!visitByeAt && !timeLeft(visitUntil)) visitEnd();
    else if (pet.sleeping || pet.isEgg() || pet.ceremony) {  // se durmio: vuelve luego si no jugaron
      if (!visitPlayed && !pet.isEgg() && !pet.ceremony) visitPending = true;
      visitDex = 0; visitUntil = visitByeAt = 0;
    }
    return;
  }
  if (visitPending && !screenOff && !pet.sleeping && !pet.isEgg() && !pet.ceremony && petHoldAllowed() &&
      mainNavAllowed() && box.count()) {
    visitPending = false;
    visitStart((uint8_t)random(box.count()));
    return;
  }
  if (now - visitCheckT < 60000UL) return;
  visitCheckT = now;
  uint32_t e = clockEpoch();
  if (!e || pet.isEgg() || pet.ceremony || !box.count()) return;
  uint32_t day = e / 86400UL;
  if (day != visitDay) { visitDay = day; visitsToday = 0; visitPending = false; }
  uint8_t hr = (uint8_t)((e / 3600UL) % 24);
  if (visitPending || hr < 8 || hr >= 21 || visitsToday >= VISIT_MAX_DAY) return;
  if (random(VISIT_ODDS)) return;
  visitsToday++;
  visitPending = true;
}
void drawVisitor(uint32_t now) {
  if (!visitDex || pet.sleeping || pet.isEgg()) return;
  int hop = visitByeAt ? (int)((now / 120) % 2) * 6 : (int)((now / 500) % 2) * 3;
  gfx->fillEllipse(VISIT_X, VISIT_Y + 40, 28, 7, lerp565(UI_INK, UI_BG_DAY, 10, 16));  // sombra
  drawThumbAt(visitDex, VISIT_X, VISIT_Y - hop, 3, false);
  if (visitByeAt || visitPlayed) drawMap(SPR_HEART, 32, VISIT_X - 16, VISIT_Y - 78 - hop, 1, false);
}
bool visitTap(int16_t x, int16_t y) {
  if (!visitDex || visitByeAt || pet.sleeping) return false;
  int dx = x - VISIT_X, dy = y - VISIT_Y;
  if (dx * dx + dy * dy > 44 * 44) return false;
  char nm[32], b[96];
  visitName(nm, sizeof(nm));
  if (!visitPlayed) {
    visitPlayed = true;
    pet.friendPlay();
    txFmt(b, sizeof(b), X_VISIT_PLAY_FMT, nm);
    showToast(b);
    sfxPlay(SFX_PLAY);
    audioCry(visitDex);
    behPetted();
  } else {
    sfxPlay(SFX_HEART);
  }
  visitByeAt = millis() + 4000;  // juega un poco y se va contento
  if (!visitByeAt) visitByeAt = 1;
  return true;
}

// ---- cumpleanos: ajuste (mes / dia) y fiesta con fuegos artificiales ----
static uint8_t bdM = 1, bdD = 1;
static const uint8_t MDAYS[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
void openBday() {
  retMark();
  if (pet.bdayM) { bdM = pet.bdayM; bdD = pet.bdayD; }
  else {
    uint8_t m = 1, d = 1;
    uint32_t e = clockEpoch();
    if (e) wxDate(e, nullptr, &m, &d, nullptr);
    bdM = m; bdD = d;
  }
  xScreen = XS_BDAY;
}
#define BD_ROW1 150
#define BD_ROW2 226
static void bdStepper(int y, const char *val, uint16_t accent) {
  drawBtn(96, y, 64, 56, UI_WHITE, UI_INK, "-");
  uiPanel(170, y, 126, 56, 14, lerp565(accent, UI_WHITE, 12, 16), accent);
  drawFitIn(val, 176, y + 14, 114, UI_INK, 2);
  drawBtn(306, y, 64, 56, UI_WHITE, UI_INK, "+");
}
void renderBday() {
  uiScreenBg();
  drawFit(XT(X_BDAY_TITLE), 40, 300, UI_INK, 3);
  char b[32];
  if (pet.bdayM) snprintf(b, sizeof(b), XT(X_BDAY_FMT), pet.bdayM, pet.bdayD);
  else snprintf(b, sizeof(b), "%s", XT(X_BDAY_NONE));
  drawFit(b, 92, 300, 0x8410, 2);
  char v[16];
  snprintf(v, sizeof(v), XT(X_BDAY_M_FMT), bdM);
  bdStepper(BD_ROW1, v, C565(0xf0, 0x60, 0x80));
  snprintf(v, sizeof(v), XT(X_BDAY_D_FMT), bdD);
  bdStepper(BD_ROW2, v, C565(0xf0, 0xa0, 0x30));
  drawBtn(96, 304, 132, 50, C565(0xf0, 0x60, 0x80), UI_WHITE, XT(X_BDAY_SAVE));
  drawBtn(238, 304, 132, 50, UI_WHITE, pet.bdayM ? UI_BAR_BAD : 0x8410, XT(X_BDAY_CLEAR));
  drawFit(XT(X_BDAY_HINT), 372, 320, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}
void bdayTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); goBack(); return; }
  auto step = [&](int row, int dir) {
    if (row == 0) { bdM = (uint8_t)((bdM + 11 + dir) % 12 + 1); if (bdD > MDAYS[bdM - 1]) bdD = MDAYS[bdM - 1]; }
    else { uint8_t n = MDAYS[bdM - 1]; bdD = (uint8_t)((bdD + n - 1 + dir) % n + 1); }
    sfxPlay(SFX_TAP);
  };
  for (int r = 0; r < 2; r++) {
    int ry = r ? BD_ROW2 : BD_ROW1;
    if (y < ry || y >= ry + 56) continue;
    if (x >= 96 && x < 160) step(r, -1);
    else if (x >= 306 && x < 370) step(r, +1);
    return;
  }
  if (y >= 304 && y < 354) {
    if (x >= 96 && x < 228) { pet.setBirthday(bdM, bdD); sfxPlay(SFX_MEDAL); showToast(XT(X_BDAY_SAVED)); goBack(); }
    else if (x >= 238 && x < 370 && pet.bdayM) { pet.setBirthday(0, 0); sfxPlay(SFX_TAP); }
  }
}

// fuegos artificiales: rafagas que nacen y se abren segun el tiempo (sin estado)
static void fireworks(uint32_t now, int n, int yTop, int yBot) {
  static const uint16_t FWC[6] = { C565(0xff, 0x50, 0x60), C565(0xff, 0xd0, 0x40), C565(0x50, 0xc0, 0xff),
                                   C565(0x70, 0xe0, 0x70), C565(0xd0, 0x70, 0xff), C565(0xff, 0x90, 0x30) };
  const uint32_t P = 1600;
  for (int k = 0; k < n; k++) {
    uint32_t t = now + (uint32_t)k * (P / n) * 7 / 5;
    uint32_t cyc = t / P, ph = t % P;
    uint32_t h = (cyc * 2654435761u) ^ (uint32_t)(k * 40503u);
    int cx = 90 + (int)(h % 286), cy = yTop + (int)((h >> 9) % (uint32_t)(yBot - yTop));
    uint16_t c = FWC[(h >> 17) % 6];
    if (ph < 350) {  // sube la estela
      int sy = cy + 120 - (int)(ph * 120 / 350);
      gfx->fillRect(cx - 1, sy, 3, 8, c);
      continue;
    }
    int r = (int)((ph - 350) * 54 / (P - 350));
    uint16_t cc = ph > P - 400 ? lerp565(c, UI_WHITE, 6, 16) : c;
    for (int a = 0; a < 12; a++) {
      static const int8_t CS[12][2] = { { 16, 0 }, { 14, 8 }, { 8, 14 }, { 0, 16 }, { -8, 14 }, { -14, 8 },
                                        { -16, 0 }, { -14, -8 }, { -8, -14 }, { 0, -16 }, { 8, -14 }, { 14, -8 } };
      int px = cx + CS[a][0] * r / 16, py = cy + CS[a][1] * r / 16;
      gfx->fillCircle(px, py, r > 40 ? 3 : 4, cc);
    }
  }
}
static bool bdayDlg = false;
static uint32_t bdayDlgT = 0, bdayShownDay = 0;
static bool bdayGot = false;
bool birthdayToday() {
  if (!pet.bdayM) return false;
  uint32_t e = clockEpoch();
  if (!e) return false;
  uint8_t m, d;
  wxDate(e, nullptr, &m, &d, nullptr);
  return pet.isBirthday(m, d);
}
// en la pantalla principal del dia: fuegos de fondo y, la primera vez, la fiesta
void bdayMain(uint32_t now) {
  if (!birthdayToday() || pet.ceremony) return;
  fireworks(now, 3, 70, 170);
  uint32_t day = clockEpoch() / 86400UL;
  if (bdayShownDay != day && !pet.isEgg()) {
    bdayShownDay = day;
    int y;
    wxDate(clockEpoch(), &y, nullptr, nullptr, nullptr);
    bdayGot = pet.birthdayGift((uint16_t)y);
    bdayDlg = true;
    bdayDlgT = now;
    sfxPlay(SFX_MEDAL);
    audioCry(pet.speciesId);
  }
}
bool bdayDraw() {
  if (!bdayDlg) return false;
  uint32_t now = millis();
  if (now - bdayDlgT > 20000UL) { bdayDlg = false; return false; }
  uiShade(0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 5);
  fireworks(now, 5, 70, 200);
  // abajo: el bicho se sigue viendo en medio
  uiPanel(80, 296, 306, 110, 20, UI_WHITE, C565(0xf0, 0x60, 0x80));
  drawFit(XT(X_BDAY_MSG), 306, 290, C565(0xf0, 0x60, 0x80), 3);
  char nm[44], b[96];
  snprintf(nm, sizeof(nm), "%s", pet.nick[0] ? pet.nick : dexName(pet.speciesId));
  txFmt(b, sizeof(b), X_BDAY_SING_FMT, nm);
  drawFit(b, 346, 290, UI_INK, 1);
  drawFit(bdayGot ? XT(X_BDAY_GIFT) : XT(X_BDAY_TAP), 372, 290, bdayGot ? UI_BAR_OK : 0x8410, 1);
  return true;
}
bool bdayTapDlg(int16_t x, int16_t y) {
  if (!bdayDlg) return false;
  (void)x; (void)y;
  if (millis() - bdayDlgT < 1200) return true;  // que no se cierre con el toque que despierta
  bdayDlg = false;
  sfxPlay(SFX_TAP);
  return true;
}
void bdayShowAgain() { bdayDlg = true; bdayDlgT = millis(); bdayGot = false; }  // consola serie

// avisos del resfriado + visitas (lo llama tamaLoop)
static void tama126Loop() {
  uint32_t now = millis();
  visitPoll(now);
  if (pet.sickNote) {
    uint8_t n = pet.sickNote;
    pet.sickNote = 0;
    if (n == 4) {  // ko12.6.1: ya toca la segunda toma (dormido: sin ruido)
      if (pet.sick && !pet.sleeping) {
        showToast(XT(X_SICK_READY));
        if (audioEnabled()) sfxPlay(SFX_ALERT); else vibPulse(250, 2, 180);
      }
      return;
    }
    showToast(XT(n == 3 ? X_SICK_RAIN : n == 2 ? X_SICK_MISS : X_SICK_GOT));
    sfxPlay(n == 2 ? SFX_DENY : SFX_ALERT);
  }
}

// ======================================================================
// ko12.7: final del viaje (todas las familias criadas) y viaje brillante
// ======================================================================
static uint8_t endWhich = 0, endPhase = 0;
static bool endReplay = false;
static uint32_t endT0 = 0;
static uint8_t endOrder[HALL_MAX];
static uint16_t endN = 0;
#define END_TITLE_MS 4500UL
#define END_PARADE_GAP 96
#define END_PARADE_PXS 150      // pixeles por segundo
#define END_CREDIT_LINE 40
#define END_CREDIT_PXS 32
#define END_CR_MAX 14
static char endCr[END_CR_MAX][72];
static uint8_t endCrN = 0;

static uint32_t endParadeMs() { return (uint32_t)(endN * END_PARADE_GAP + LCD_WIDTH + 120) * 1000UL / END_PARADE_PXS; }
static uint32_t endCreditMs() { return (uint32_t)(endCrN * END_CREDIT_LINE + LCD_HEIGHT + 60) * 1000UL / END_CREDIT_PXS; }

static void endBuildCredits() {
  endCrN = 0;
  auto add = [&](const char *s) { if (endCrN < END_CR_MAX) snprintf(endCr[endCrN++], sizeof(endCr[0]), "%s", s); };
  char b[72];
  add(XT(X_END_CR_TITLE));
  add("");
  uint32_t now = clockEpoch();
  if (pet.journeyStart) {
    int y; uint8_t m, d;
    wxDate(pet.journeyStart, &y, &m, &d, nullptr);
    snprintf(b, sizeof(b), XT(X_END_CR_START_FMT), (unsigned)y, m, d); add(b);
    if (now > pet.journeyStart) { snprintf(b, sizeof(b), XT(X_END_CR_DAYS_FMT), (unsigned long)((now - pet.journeyStart) / 86400UL + 1)); add(b); }
  }
  snprintf(b, sizeof(b), XT(X_END_CR_HALL_FMT), hall.count()); add(b);
  snprintf(b, sizeof(b), XT(X_END_CR_DEX_FMT), dexDiscoveredCount()); add(b);
  uint16_t sh = 0;
  for (uint8_t i = 0; i < hall.count(); i++) if (hall.at(i).flags & BOXF_SHINY) sh++;
  snprintf(b, sizeof(b), XT(X_END_CR_SHINY_FMT), sh); add(b);
  char st[20];
  fmtSteps(st, sizeof(st), pet.walk.total);
  snprintf(b, sizeof(b), XT(X_END_CR_STEPS_FMT), st); add(b);
  snprintf(b, sizeof(b), XT(X_END_CR_WILD_FMT), pet.wildWins); add(b);
  snprintf(b, sizeof(b), XT(X_END_CR_CHAMP_FMT), pet.champWins); add(b);
  snprintf(b, sizeof(b), XT(X_END_CR_LINK_FMT), pet.linkWins, pet.trades); add(b);
  uint8_t bd = 0;
  for (int i = 0; i < 8; i++) if (pet.badges & (1 << i)) bd++;
  snprintf(b, sizeof(b), XT(X_END_CR_BADGE_FMT), bd); add(b);
  add("");
  add(XT(X_END_CR_THANKS));
}

void openEnding(uint8_t which, bool replay) {
  if (which < 1 || which > 2) return;
  endWhich = which;
  endReplay = replay;
  endPhase = 0;
  endT0 = millis();
  // el salon por orden de llegada (el primero = el primer companero que llego al final)
  endN = hall.count();
  for (uint16_t i = 0; i < endN; i++) endOrder[i] = (uint8_t)i;
  for (uint16_t i = 1; i < endN; i++) {
    uint8_t k = endOrder[i];
    uint32_t e = hall.at(k).epoch;
    int j = (int)i - 1;
    while (j >= 0 && hall.at(endOrder[j]).epoch > e) { endOrder[j + 1] = endOrder[j]; j--; }
    endOrder[j + 1] = k;
  }
  endBuildCredits();
  retMark();
  xScreen = XS_ENDING;
  sfxPlay(SFX_MEDAL);
}

// lo abre el bucle principal cuando la ultima familia se despide
void endingPoll() {
  if (!pet.pendingEnding || xScreen != XS_NONE || pet.ceremony || cardOpen || clockOpen) return;
  openEnding(pet.pendingEnding, false);
}

static void endFinish() {
  if (!endReplay) pet.endingDone(endWhich);
  xScreen = XS_NONE;
  if (endReplay) goBack();
}

static void endSky(uint32_t now) {
  gfx->fillScreen(C565(0x10, 0x16, 0x30));
  for (int i = 0; i < 60; i++) {  // estrellas que titilan
    int x = (i * 97 + 31) % LCD_WIDTH, y = (i * 53 + 17) % 300;
    bool on = ((now / 300) + i) % 5 != 0;
    if (on) gfx->fillRect(x, y, (i % 7) ? 2 : 3, (i % 7) ? 2 : 3, (i % 3) ? UI_WHITE : C565(0xff, 0xe0, 0x80));
  }
  gfx->fillRect(0, 300, LCD_WIDTH, LCD_HEIGHT - 300, C565(0x24, 0x3a, 0x2c));
}

static void endSparkle(int cx, int cy, uint32_t now, uint16_t c) {
  for (int a = 0; a < 4; a++) {
    int k = (int)((now / 120 + a * 3) % 8);
    int dx = (a & 1 ? 1 : -1) * (14 + k * 2), dy = (a & 2 ? 1 : -1) * (12 + k);
    gfx->drawFastHLine(cx + dx - 3, cy + dy, 7, c);
    gfx->drawFastVLine(cx + dx, cy + dy - 3, 7, c);
  }
}

void renderEnding() {
  uint32_t now = millis(), t = now - endT0;
  uint16_t gold = C565(0xff, 0xd0, 0x40);
  if (endPhase == 0 && t > END_TITLE_MS) { endPhase = 1; endT0 = now; t = 0; }
  if (endPhase == 1 && t > endParadeMs()) { endPhase = 2; endT0 = now; t = 0; }
  if (endPhase == 2 && t > endCreditMs()) { endPhase = 3; endT0 = now; t = 0; }
  endSky(now);
  if (endPhase == 0) {
    fireworks(now, 4, 70, 220);
    drawFit(XT(endWhich == 2 ? X_END_T2 : X_END_T1), 236, 400, gold, 2);
    char b[48];
    uint16_t tot = 0;  // todas las familias (al llegar aqui ya estan todas)
    for (int16_t d = 1; d <= DEX_COUNT; d++) if (DEX_FAM[d] == d) tot++;
    snprintf(b, sizeof(b), XT(X_END_SUB_FMT), (unsigned)tot);
    drawFit(b, 272, 360, UI_WHITE, 2);
  } else if (endPhase == 1) {
    drawFit(XT(X_END_PARADE), 120, 300, gold, 2);
    int off = (int)((uint64_t)t * END_PARADE_PXS / 1000);
    int center = -1, best = 9999;
    for (uint16_t i = 0; i < endN; i++) {
      int x = LCD_WIDTH + 60 + (int)i * END_PARADE_GAP - off;
      if (x < -60 || x > LCD_WIDTH + 60) continue;
      const BoxMon &m = hall.at(endOrder[i]);
      int hop = ((now / 180) + i) % 2 ? 4 : 0;
      drawThumbAt(m.dex, x, 250 - hop, 2, false);
      if ((m.flags & BOXF_SHINY) || endWhich == 2) endSparkle(x, 240, now + i * 70, gold);
      int dc = abs(x - CX);
      if (dc < best) { best = dc; center = i; }
    }
    if (center >= 0 && best < END_PARADE_GAP / 2) {
      const BoxMon &m = hall.at(endOrder[center]);
      char b[48];
      snprintf(b, sizeof(b), XT(X_END_NO_FMT), (unsigned)(center + 1));
      drawFit(b, 312, 200, C565(0xb0, 0xc0, 0xd0), 1);
      snprintf(b, sizeof(b), "%s%s", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex));
      drawFit(b, 336, 300, UI_WHITE, 2);
    }
  } else if (endPhase == 2) {
    int y0 = LCD_HEIGHT - (int)((uint64_t)t * END_CREDIT_PXS / 1000);
    for (uint8_t i = 0; i < endCrN; i++) {
      int y = y0 + i * END_CREDIT_LINE;
      if (y < 90 || y > 380 || !endCr[i][0]) continue;  // ko12.8: arriba y abajo el circulo se estrecha (y abajo esta [skip])
      drawFit(endCr[i], y, 380, i == 0 || i + 1 == endCrN ? gold : UI_WHITE, 2);
    }
  } else {
    fireworks(now, 3, 60, 160);
    int16_t first = endN ? dexFirstForm(hall.at(endOrder[0]).dex) : (pet.isEgg() ? 1 : pet.speciesId);
    int hop = (now / 300) % 2 ? 6 : 0;
    drawThumbAt(first, CX, 210 - hop, 4, false);
    if (endWhich == 2) endSparkle(CX, 200, now, gold);
    drawFit(XT(endWhich == 2 ? X_END_MSG2 : X_END_MSG1), 292, 380, UI_WHITE, 1);
    drawFit(XT(endWhich == 2 ? X_END_MSG2B : X_END_MSG1B), 316, 380, UI_WHITE, 1);
    drawFit(XT(endWhich == 2 ? X_END_RW2 : X_END_RW1), 352, 360, gold, 1);
    if (t > 1500) drawFit(XT(X_END_TAP), 392, 300, C565(0xb0, 0xc0, 0xd0), 1);
  }
  if (endPhase < 3) drawFit(XT(X_END_SKIP), 420, 200, C565(0x70, 0x80, 0x98), 1);
  uiFlush();
}

void endingTap(int16_t x, int16_t y) {
  (void)x; (void)y;
  uint32_t t = millis() - endT0;
  if (t < 600) return;  // que un toque suelto no se salte nada sin querer
  sfxPlay(SFX_TAP);
  if (endPhase < 3) { endPhase++; endT0 = millis(); return; }
  if (t > 1500) endFinish();
}

// ---- recompensas: corona junto al nombre ----
void drawMasterCrown(int cx, int cy) {
  if (!pet.endSeen) return;
  uint16_t g = (pet.endSeen & 2) ? C565(0xff, 0xe8, 0x80) : C565(0xf0, 0xc0, 0x30), dk = C565(0x8a, 0x60, 0x10);
  gfx->fillRect(cx - 11, cy, 22, 7, g);
  gfx->fillTriangle(cx - 11, cy, cx - 11, cy - 10, cx - 4, cy, g);
  gfx->fillTriangle(cx - 5, cy, cx, cy - 13, cx + 5, cy, g);
  gfx->fillTriangle(cx + 11, cy, cx + 11, cy - 10, cx + 4, cy, g);
  gfx->drawRect(cx - 11, cy, 22, 7, dk);
  gfx->fillCircle(cx, cy - 13, 2, (pet.endSeen & 2) ? C565(0x80, 0xd0, 0xff) : C565(0xe0, 0x40, 0x50));
}

// ---- 2a vuelta: elegir el huevo (con un vale) ----
#define EP_COLS 3
#define EP_ROWS 3
#define EP_X0 83
#define EP_Y0 92
#define EP_W 100
#define EP_H 74
#define EP_NAV_Y 330
static int16_t epList[DEX_COUNT];
static uint16_t epN = 0, epPage = 0;
static void epBuild() {  // una especie de huevo por familia aun sin hacer (la mas baja)
  epN = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (DEX_TBL[d].rarity == R_EVO || pet.famDone(d)) continue;
    bool dup = false;
    for (uint16_t k = 0; k < epN && !dup; k++) dup = DEX_FAM[epList[k]] == DEX_FAM[d];
    if (!dup) epList[epN++] = d;
  }
}
static uint16_t epPages() { return epN ? (epN + EP_COLS * EP_ROWS - 1) / (EP_COLS * EP_ROWS) : 1; }
void openEggPick() { retMark(); epBuild(); epPage = 0; xScreen = XS_EGGPICK; }
void renderEggPick() {
  uiScreenBg();
  drawFit(XT(X_PICK_TITLE), 36, 300, UI_INK, 2);
  drawFit(XT(X_PICK_HINT), 66, 330, 0x8410, 1);
  if (!epN) drawFit(XT(X_PICK_NONE), 200, 300, UI_INK, 2);
  for (int i = 0; i < EP_COLS * EP_ROWS; i++) {
    int k = epPage * EP_COLS * EP_ROWS + i;
    if (k >= epN) break;
    int x = EP_X0 + (i % EP_COLS) * EP_W, y = EP_Y0 + (i / EP_COLS) * EP_H;
    uiButton(x + 4, y + 4, EP_W - 8, EP_H - 8, 12, UI_WHITE, UI_INK);
    drawThumbAt(epList[k], x + EP_W / 2, y + 28, 1, false);
    drawFitIn(dexName(epList[k]), x + 8, y + 48, EP_W - 16, UI_INK, 1);
  }
  if (epPages() > 1) {
    drawBtn(113, EP_NAV_Y, 60, 34, epPage ? UI_WHITE : UI_TRACK, UI_INK, "<");
    drawBtn(293, EP_NAV_Y, 60, 34, epPage + 1 < epPages() ? UI_WHITE : UI_TRACK, UI_INK, ">");
    char pg[16];
    snprintf(pg, sizeof(pg), "%u/%u", epPage + 1, epPages());
    drawFit(pg, EP_NAV_Y + 8, 100, UI_INK, 2);
  }
  char b[40];
  snprintf(b, sizeof(b), XT(X_PICK_BTN_FMT), pet.pickTokens);
  drawFit(b, 380, 300, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}
void eggPickTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); goBack(); return; }
  if (epPages() > 1 && y >= EP_NAV_Y && y < EP_NAV_Y + 34) {
    if (x < CX && epPage > 0) epPage--;
    else if (x >= CX && epPage + 1 < epPages()) epPage++;
    sfxPlay(SFX_TAP);
    return;
  }
  if (x < EP_X0 || x >= EP_X0 + EP_COLS * EP_W || y < EP_Y0 || y >= EP_Y0 + EP_ROWS * EP_H) return;
  int k = epPage * EP_COLS * EP_ROWS + ((y - EP_Y0) / EP_H) * EP_COLS + (x - EP_X0) / EP_W;
  if (k >= epN) return;
  if (!pet.chooseEgg(epList[k])) { sfxPlay(SFX_DENY); return; }
  char b[80];
  txFmt(b, sizeof(b), X_PICK_DONE_FMT, dexName(epList[k]));
  showToast(b);
  sfxPlay(SFX_MEDAL);
  xScreen = XS_NONE;
}

// ======================================================================
// ko12.8: SD 파일 묶기: la carpeta /mons entera -> /mons.pak (un fichero cifrado)
// ======================================================================
static PakBuildInfo pakInfo;
static bool pakScanned = false;
static int8_t pakResult = -1;  // -1 nada, 0 ok, 1 sin espacio, 2 error, 3 vacio
void openPak() {  // se abre desde la pantalla de SD 업데이트 y [<] vuelve alli
  pakScanned = pakScan(pakInfo);
  pakResult = -1;
  xScreen = XS_PAK;
}
static void pakScreenBase() {
  gfx->fillScreen(UI_BG_DAY);
  drawFit(XT(X_PAK_TITLE), 48, 300, UI_INK, 3);
}
static void pakProgress(uint64_t done, uint64_t total, uint32_t nf, uint32_t tf) {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (nf < tf && now - last < 300) return;
  last = now;
  pakScreenBase();
  drawFit(XT(X_PAK_WORKING), 160, 360, UI_INK, 2);
  int w = 300, fw = (int)((uint64_t)(w - 4) * done / (total ? total : 1));
  gfx->fillRoundRect(CX - w / 2, 206, w, 24, 8, UI_TRACK);
  if (fw > 0) gfx->fillRoundRect(CX - w / 2 + 2, 208, fw, 20, 7, C565(0x6a, 0x4c, 0xf0));
  char b[48];
  snprintf(b, sizeof(b), "%u%%", (unsigned)(done * 100 / (total ? total : 1)));
  drawFit(b, 244, 200, UI_INK, 2);
  snprintf(b, sizeof(b), XT(X_PAK_PROG_FMT), (unsigned)nf, (unsigned)tf);
  drawFit(b, 280, 300, 0x8410, 1);
  gfx->flush();
}
void renderPak() {
  pakScreenBase();
  char b[64];
  int8_t st = pakState();
  if (st == 1) snprintf(b, sizeof(b), XT(X_PAK_NOW_FMT), (unsigned)pakCount());
  else snprintf(b, sizeof(b), "%s", XT(st == -1 ? X_PAK_BADPASS : st == -2 ? X_PAK_BROKEN : X_PAK_NONE));
  drawFit(b, 96, 340, st == 1 ? UI_BAR_OK : st < 0 ? UI_BAR_BAD : 0x8410, 1);
  if (pakScanned) {
    snprintf(b, sizeof(b), XT(X_PAK_SCAN_FMT), (unsigned)pakInfo.files, (unsigned)(pakInfo.bytes >> 20));
    drawFit(b, 136, 340, UI_INK, 2);
  }
  drawFit(XT(X_PAK_DESC), 180, 360, UI_INK, 1);
  drawFit(XT(X_PAK_DESC2), 204, 360, 0x8410, 1);
  if (pakResult >= 0) {
    static const XId R[4] = { X_PAK_DONE, X_PAK_ERR_SPACE, X_PAK_ERR_SD, X_PAK_ERR_EMPTY };
    drawFit(XT(R[pakResult < 4 ? pakResult : 2]), 244, 360, pakResult == 0 ? UI_BAR_OK : UI_BAR_BAD, 2);
  } else {  // ko12.8.4: diagnostico (solo ASCII): modo de lectura del .pak y pasos lentos del arranque
    void bootDiagLine(char *out, size_t n);
    char d[120];
    pakDiag(d, sizeof(d));
    char *bar = strchr(d, '|');
    if (bar) { *bar = 0; drawFit(d, 222, 340, 0x8410, 1); drawFit(bar + 2, 238, 340, 0x8410, 1); }
    bootDiagLine(d, sizeof(d));
    bar = strchr(d, '|');  // ko12.9: pasos lentos | tiempos de SD y del primer dibujo
    if (bar) { bar[-1] = 0; drawFit(bar + 2, 270, 340, 0x8410, 1); }
    drawFit(d, 254, 340, 0x8410, 1);
  }
  bool can = pakScanned && pakInfo.files > 0;
  drawBtn(113, 290, 240, 52, can ? C565(0x6a, 0x4c, 0xf0) : UI_TRACK, can ? UI_WHITE : 0x8410, XT(X_PAK_GO));
  drawBtn(133, 354, 200, 40, UI_WHITE, UI_INK, XT(X_SDC_BTN));  // ko12.8: SD 파일 점검
  drawNav(NAV_L, UI_INK);
  uiFlush();
}
void pakTap(int16_t x, int16_t y) {
  if (pakBusy) return;
  if (navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); xScreen = XS_UPD; return; }
  if (inRect(x, y, 133, 354, 200, 40)) { sfxPlay(SFX_TAP); openSdCheck(); return; }
  if (inRect(x, y, 113, 290, 240, 52) && pakScanned && pakInfo.files > 0) {
    sfxPlay(SFX_TAP);
    pet.saveNow();
    pakBusy = true;
    pakProgress(0, 1, 0, pakInfo.files);
    pakResult = (int8_t)pakBuild(pakProgress);
    pakBusy = false;
    sdDirty = true;  // recargar sprite / miniaturas (ahora del .pak)
    sfxPlay(pakResult == 0 ? SFX_MEDAL : SFX_DENY);
    lastInteract = millis();
  }
}

// ======================================================================
// ko12.8: SD 파일 점검: cuantos ficheros de cada clase hay en /mons (o en mons.pak)
// ======================================================================
static SdInv sdcInv;
static bool sdcOk = false;
#define SDC_ROW_Y0 90
#define SDC_ROW_DY 34
void openSdCheck() {
  sdcInv = SdInv();
  sdcOk = monsForEachName([](const char *rel, void *ctx) { ((SdInv *)ctx)->add(rel); }, &sdcInv);
  xScreen = XS_SDCHK;
}
void renderSdCheck() {
  uiScreenBg();
  drawFit(XT(X_SDC_TITLE), 40, 300, UI_INK, 2);
  if (!sdcOk) {
    drawFit(XT(X_SDC_NOSD), 200, 300, UI_BAR_BAD, 2);
    drawNav(NAV_L, UI_INK);
    uiFlush();
    return;
  }
  struct Row { XId label; uint8_t a, b; bool optional; };
  static const Row ROWS[7] = {
    { X_SDC_SPR, SDC_SPR, SDC_SPRS, false }, { X_SDC_BAT, SDC_BAT, SDC_BATS, false },
    { X_SDC_FX, SDC_FX, 0xFF, false },       { X_SDC_CRY, SDC_CRY, 0xFF, false },
    { X_SDC_THUMB, SDC_THUMB, 0xFF, false }, { X_SDC_STORY, SDC_STORY, 0xFF, false },
    { X_SDC_MUSIC, SDC_MUSIC, 0xFF, true },
  };
  char miss[24] = "";
  uint16_t missN = 0;
  for (uint8_t r = 0; r < 7; r++) {
    const Row &w = ROWS[r];
    uint16_t have = sdcInv.have(w.a), need = SdInv::need(w.a);
    if (w.b != 0xFF) { have += sdcInv.have(w.b); need += SdInv::need(w.b); }
    int y = SDC_ROW_Y0 + r * SDC_ROW_DY;
    bool full = have >= need;
    uint16_t c = full ? UI_BAR_OK : w.optional ? 0x8410 : UI_BAR_BAD;
    gfx->fillCircle(86, y + 10, 6, c);
    drawFitIn(XT(w.label), 100, y, 190, UI_INK, 1);
    char n[16];
    snprintf(n, sizeof(n), "%u/%u", (unsigned)have, (unsigned)need);
    drawFitIn(n, 296, y, 92, c, 1);
    if (!w.optional && !full) {
      missN += need - have;
      if (!miss[0] && !sdcInv.firstMissing(w.a, miss, sizeof(miss)) && w.b != 0xFF)
        sdcInv.firstMissing(w.b, miss, sizeof(miss));
    }
  }
  int yMsg = SDC_ROW_Y0 + 7 * SDC_ROW_DY + 8;
  if (!missN) drawFit(XT(X_SDC_OK), yMsg, 330, UI_BAR_OK, 1);
  else {
    char b[64];
    snprintf(b, sizeof(b), XT(X_SDC_MISS_FMT), miss, (unsigned)(missN - 1));
    drawFit(b, yMsg, 330, UI_BAR_BAD, 1);
  }
  if (pakActive()) {
    char b[48];
    snprintf(b, sizeof(b), XT(X_SDC_PAK_FMT), (unsigned)pakCount());
    drawFit(b, yMsg + 26, 300, 0x8410, 1);
  }
  drawNav(NAV_L, UI_INK);
  uiFlush();
}
void sdCheckTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); xScreen = XS_UPD; }
}

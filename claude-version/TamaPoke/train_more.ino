// ko12.9.3: dos entrenamientos nuevos
//   - Ataque: "타이밍 펀치". Una luz da vueltas por el borde redondo; tocar cuando pasa por la zona
//     amarilla = el bicho carga y dispara y rompe una roca (el centro naranja = "완벽!", dos rocas).
//     Cada acierto estrecha la zona y acelera la luz; 3 fallos (tocar fuera o dejarla pasar) = fin.
//     Sustituye al saco (el codigo del saco sigue en TamaPoke.ino, ya no sale en el menu).
//   - Juego: "기울여 열매 모으기". Se inclina la placa (QMI8658) y una pokeball rueda por el campo
//     recogiendo bayas (+1, doradas +3); los agujeros quitan 5 s. 30 s. La primera vez se aprende
//     la direccion: "inclina a la derecha" y "inclina hacia abajo" (el eje y el signo del sensor
//     cambian con como esta montado). Sin sensor: el juego de toques de siempre.

bool punchOpen = false, tiltOpen = false;

// ======================================================================
// ataque: timing punch
// ======================================================================
#define TP_R 212         // radio del anillo
#define TP_ROCK_Y 200
#define TP_PET_Y 404
#define TP_FX_MS 450
static uint32_t tpStart = 0, tpOverUntil = 0, tpFxT = 0, tpLastT = 0, tpJudgeT = 0;
static float tpAng = 0, tpSpeed = 150, tpTgt = 90, tpW = 70;
static uint8_t tpLives = 3, tpCombo = 0, tpBestCombo = 0, tpJudge = 0, tpGain = 0;
static uint16_t tpRocks = 0, tpPerfect = 0;
static bool tpNewHi = false;

static float tpDiff(float a, float b) {  // a - b en (-180, 180]
  float d = fmodf(a - b + 540.0f, 360.0f) - 180.0f;
  return d;
}
static float tpPerfW() { float p = tpW * 0.28f; return p < 8 ? 8 : p; }
static void tpNewTarget() { tpTgt = fmodf(tpAng + 130 + random(140), 360.0f); }

void startPunch() {
  perfReset();
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  punchOpen = true;
  tpStart = tpLastT = millis();
  tpOverUntil = tpFxT = tpJudgeT = 0;
  tpAng = 0; tpSpeed = 150; tpW = 70;
  tpLives = 3; tpCombo = tpBestCombo = tpJudge = 0;
  tpRocks = tpPerfect = 0;
  tpNewHi = false;
  tpNewTarget();
}

static void tpMiss(uint32_t now) {
  if (tpLives) tpLives--;
  tpCombo = 0;
  tpJudge = 3; tpJudgeT = now;
  sfxPlay(SFX_DENY);
  tpNewTarget();
}

static void stepPunch(uint32_t now) {
  float dt = (now - tpLastT) / 1000.0f;
  if (dt > 0.25f) dt = 0.25f;
  tpLastT = now;
  if (now - tpStart < 900) return;  // un respiro para leer
  tpAng = fmodf(tpAng + tpSpeed * dt, 360.0f);
  float d = tpDiff(tpAng, tpTgt);
  if (d > tpW / 2 + 4 && d < 90) tpMiss(now);  // la dejo pasar entera
}

void punchPress(int16_t x, int16_t y) {
  (void)x; (void)y;  // vale tocar en cualquier sitio
  uint32_t now = millis();
  if (tpOverUntil || !tpLives || now - tpStart < 900) return;
  float d = fabsf(tpDiff(tpAng, tpTgt));
  if (d <= tpPerfW() / 2) {
    tpRocks += 2; tpPerfect++; tpCombo++;
    tpJudge = 1;
    sfxPlay(SFX_MEDAL);
  } else if (d <= tpW / 2) {
    tpRocks += 1; tpCombo++;
    tpJudge = 2;
    sfxPlay(SFX_PLAY);
  } else {
    tpMiss(now);
    return;
  }
  if (tpCombo > tpBestCombo) tpBestCombo = tpCombo;
  tpJudgeT = tpFxT = now;
  tpSpeed = tpSpeed + 12 > 430 ? 430 : tpSpeed + 12;
  tpW = tpW - 3 < 26 ? 26 : tpW - 3;
  tpNewTarget();
}

static void tpDojo() {
  gfx->fillScreen(C565(0x2c, 0x26, 0x38));
  gfx->fillCircle(CX, CX, 196, C565(0xe8, 0xd8, 0xb8));
  for (int y = 70; y < 430; y += 36) {
    int w = (int)sqrtf((float)(196 * 196 - (y - CX) * (y - CX)));
    gfx->drawFastHLine(CX - w, y, 2 * w, C565(0xd4, 0xc0, 0x9c));
  }
}
static void tpRing(uint32_t now) {
  for (int a = 0; a < 360; a += 3) {  // pista
    float r = (a - 90) * 0.01745f;
    gfx->fillCircle(CX + (int)(TP_R * cosf(r)), CX + (int)(TP_R * sinf(r)), 4, C565(0x4a, 0x42, 0x5a));
  }
  float pw = tpPerfW();
  for (float a = tpTgt - tpW / 2; a <= tpTgt + tpW / 2; a += 1.5f) {  // zona buena / perfecta
    float r = (a - 90) * 0.01745f;
    bool perf = fabsf(tpDiff(a, tpTgt)) <= pw / 2;
    gfx->fillCircle(CX + (int)(TP_R * cosf(r)), CX + (int)(TP_R * sinf(r)), perf ? 9 : 7,
                    perf ? C565(0xff, 0x8a, 0x20) : C565(0xff, 0xd8, 0x40));
  }
  for (int k = 6; k >= 1; k--) {  // estela de la luz
    float r = (tpAng - k * 4 - 90) * 0.01745f;
    gfx->fillCircle(CX + (int)(TP_R * cosf(r)), CX + (int)(TP_R * sinf(r)), 9 - k,
                    lerp565(C565(0x4a, 0x42, 0x5a), UI_WHITE, 16 - k * 2, 16));
  }
  float r = (tpAng - 90) * 0.01745f;
  int dx = CX + (int)(TP_R * cosf(r)), dy = CX + (int)(TP_R * sinf(r));
  bool flash = tpFxT && now - tpFxT < 150;
  gfx->fillCircle(dx, dy, flash ? 16 : 12, flash ? C565(0xff, 0xf0, 0xa0) : UI_WHITE);
  gfx->drawCircle(dx, dy, flash ? 16 : 12, UI_INK);
}
static void tpRock(int cx, int cy, int r, int cracks) {
  gfx->fillCircle(cx, cy + 4, r, C565(0x5a, 0x52, 0x4a));
  gfx->fillCircle(cx, cy, r, C565(0x9a, 0x92, 0x88));
  gfx->fillCircle(cx - r / 3, cy - r / 3, r / 3, C565(0xb8, 0xb0, 0xa6));
  gfx->drawCircle(cx, cy, r, UI_INK);
  for (int i = 0; i < cracks; i++) {
    float a = i * 2.4f;
    int x0 = cx + (int)(r * 0.15f * cosf(a)), y0 = cy + (int)(r * 0.15f * sinf(a));
    int x1 = cx + (int)(r * 0.8f * cosf(a + 0.3f)), y1 = cy + (int)(r * 0.8f * sinf(a + 0.3f));
    gfx->drawLine(x0, y0, x1, y1, UI_INK);
    gfx->drawLine(x0 + 1, y0, x1 + 1, y1, UI_INK);
  }
}

void renderPunch() {
  uint32_t now = millis();
  if (tpOverUntil) {
    if (!timeLeft(tpOverUntil)) { punchOpen = false; backToTrainMenu(); return; }
    char s[32], g[20], sub[48];
    snprintf(s, sizeof(s), XT(X_TP_ROCKS_FMT), (unsigned)tpRocks);
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), tpGain);
    snprintf(sub, sizeof(sub), XT(X_TP_SUB_FMT), (unsigned)tpPerfect, (unsigned)tpBestCombo);
    drawTrainResult(s, g, UI_BAR_BAD, tpNewHi && tpRocks > 0, pet.strHi, sub);
    return;
  }
  if (!tpLives) {  // fin: el entrenamiento cuenta las rocas (como los sacos: rocas = record, 1 roca = 1 punto)
    tpNewHi = tpRocks > pet.strHi;
    tpGain = pet.trainStrength((uint16_t)(tpRocks * 4), tpRocks);
    sfxPlay(tpNewHi ? SFX_MEDAL : SFX_PLAY);
    tpOverUntil = now + 3500;
    return;
  }
  stepPunch(now);
  tpDojo();
  tpRing(now);
  // marcador: rocas, vidas, combo
  uiPanel(168, 46, 130, 38, 14, UI_WHITE, UI_INK);
  char b[32];
  snprintf(b, sizeof(b), XT(X_TP_ROCKS_FMT), (unsigned)tpRocks);
  drawFit(b, 54, 120, UI_INK, 2);
  for (int i = 0; i < 3; i++) {
    int x = 186 + i * 34;
    if (i < tpLives) drawMap(SPR_HEART, 32, x - 4, 92, 1, false);
    else gfx->drawCircle(x + 12, 106, 9, C565(0x9a, 0x92, 0x88));
  }
  if (tpCombo > 1) { snprintf(b, sizeof(b), XT(X_TP_COMBO_FMT), (unsigned)tpCombo); drawFit(b, 132, 160, C565(0xd0, 0x50, 0x20), 1); }
  // la roca: se rompe al acertar y sale otra
  uint32_t ft = tpFxT ? now - tpFxT : 9999;
  if (ft < TP_FX_MS) {
    for (int i = 0; i < 9; i++) {  // trozos
      float a = i * 0.7f;
      int dist = 30 + (int)(ft / 5);
      int x = CX + (int)(dist * cosf(a)), y = TP_ROCK_Y + (int)(dist * 0.85f * sinf(a)) + (int)(ft * ft / 9000);
      gfx->fillTriangle(x, y, x + 12, y + 4, x + 4, y + 14, C565(0x9a, 0x92, 0x88));
      gfx->drawTriangle(x, y, x + 12, y + 4, x + 4, y + 14, UI_INK);
    }
    for (int a = 0; a < 8; a++) {
      float r = a * 0.785f;
      gfx->drawLine(CX + (int)(20 * cosf(r)), TP_ROCK_Y + (int)(20 * sinf(r)), CX + (int)(52 * cosf(r)),
                    TP_ROCK_Y + (int)(52 * sinf(r)), C565(0xff, 0xb0, 0x30));
    }
    drawMoveFx(DEX_TBL[pet.speciesId].ptype, CX, 330, CX, TP_ROCK_Y, 120 + ft * 560 / TP_FX_MS, true, 2,
               moveTier(pet.speciesId), pet.moveVar());
  } else {
    int grow = ft < TP_FX_MS + 200 ? (int)(ft - TP_FX_MS) / 5 : 40;  // la nueva aparece creciendo
    tpRock(CX, TP_ROCK_Y, 6 + grow, tpCombo > 3 ? 3 : tpCombo);
  }
  // el bicho: carga esperando, dispara al acertar, se duele al fallar
  if (pmd.loaded) {
    uint8_t act = ft < TP_FX_MS ? (pmd.has(PMD_SHOOT) ? PMD_SHOOT : PMD_ATTACK)
                : (tpJudge == 3 && now - tpJudgeT < 500) ? PMD_HURT
                : pmd.has(PMD_CHARGE) ? PMD_CHARGE : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdActM(pmd, act, CX, TP_PET_Y, now, true, false, 4, 140);
  }
  // juicio
  if (tpJudge && now - tpJudgeT < 600) {
    uint32_t t = now - tpJudgeT;
    XId id = tpJudge == 1 ? X_TP_PERFECT : tpJudge == 2 ? X_TP_GOOD : X_TP_MISS;
    uint16_t c = tpJudge == 1 ? C565(0xe0, 0x50, 0x10) : tpJudge == 2 ? C565(0x1a, 0x86, 0x34) : UI_BAR_BAD;
    if (tpJudge == 3) drawFit(XT(id), 256, 220, c, 2);  // fallo: debajo de la roca (sigue entera)
    else {  // acierto: sobre los trozos de la roca
      drawFit(XT(id), 150 - (int)(t / 30), 220, c, 3);
      drawFit(tpJudge == 1 ? "+2" : "+1", 196 - (int)(t / 30), 80, c, 2);
    }
  }
  if (now - tpStart < 2500) drawFit(XT(X_TP_HINT), 280, 330, UI_INK, 2);
  uiFlush();
}

// ======================================================================
// juego: inclinar y recoger bayas
// ======================================================================
bool imuOk();
bool imuAccel(int32_t &ax, int32_t &ay, int32_t &az);
// pruebas (capturas): sin sensor de verdad, una inclinacion simulada
bool gTiltSim = false;
int32_t gTiltSimX = 0, gTiltSimY = 0;
static bool tiltRead(int32_t &x, int32_t &y) {
  if (gTiltSim) { x = gTiltSimX; y = gTiltSimY; return true; }
  int32_t z;
  return imuAccel(x, y, z);
}
bool tiltAvailable() { return gTiltSim || imuOk(); }

#define TL_MS 30000UL
#define TL_ARENA 200
#define TL_BALL 18
#define TL_ITEMS 5
#define TL_HOLES 3
#define TL_FALL_MS 700
enum : uint8_t { TL_HOW = 0, TL_CAL_R, TL_CAL_D, TL_PLAY };
static uint8_t tlPhase = TL_HOW;
static int8_t tlMap[2] = { -1, -1 };     // eje del sensor para x / y de la pantalla: 0 = x, 1 = y (+2 = signo -)
static bool tlMapLoaded = false;
static int32_t tlBase[2] = { 0, 0 };
static bool tlBaseOk = false;
static float tlBX, tlBY, tlVX, tlVY;
static struct { int16_t x, y; uint8_t k; } tlItem[TL_ITEMS];
static struct { int16_t x, y; } tlHole[TL_HOLES];
static uint16_t tlScore = 0;
static uint32_t tlEnd = 0, tlLastT = 0, tlFallT = 0, tlOverUntil = 0, tlCalT = 0, tlPopT = 0;
static int16_t tlPopX = 0, tlPopY = 0;
static uint8_t tlPopV = 0;
static bool tlNewHi = false;

static void tlLoadMap() {
  if (tlMapLoaded) return;
  tlMapLoaded = true;
  Preferences p;
  p.begin("tptilt", true);
  tlMap[0] = (int8_t)p.getChar("mx", -1);
  tlMap[1] = (int8_t)p.getChar("my", -1);
  p.end();
}
static void tlSaveMap() {
  Preferences p;
  p.begin("tptilt", false);
  p.putChar("mx", tlMap[0]);
  p.putChar("my", tlMap[1]);
  p.end();
}
static bool tlMapped() { return tlMap[0] >= 0 && tlMap[1] >= 0 && (tlMap[0] & 1) != (tlMap[1] & 1); }
// inclinacion de la pantalla (milesimas de g) segun lo aprendido
static float tlAxis(const int32_t *raw, int8_t m) {
  float v = (float)(raw[m & 1] - tlBase[m & 1]);
  return (m & 2) ? -v : v;
}

static bool tlFree(int x, int y, int skipItem) {
  float bx = tlBX - x, by = tlBY - y;
  if (bx * bx + by * by < 70 * 70) return false;
  for (auto &h : tlHole) { int dx = h.x - x, dy = h.y - y; if (dx * dx + dy * dy < 50 * 50) return false; }
  for (int i = 0; i < TL_ITEMS; i++) {
    if (i == skipItem) continue;
    int dx = tlItem[i].x - x, dy = tlItem[i].y - y;
    if (dx * dx + dy * dy < 44 * 44) return false;
  }
  return true;
}
static void tlRandPos(int16_t &x, int16_t &y, int skipItem) {
  for (int t = 0; t < 40; t++) {
    float a = random(628) / 100.0f, r = 30 + random(TL_ARENA - 50);
    int px = CX + (int)(r * cosf(a)), py = CX + (int)(r * sinf(a));
    if (tlFree(px, py, skipItem) || t == 39) { x = (int16_t)px; y = (int16_t)py; return; }
  }
}
static void tlSpawnItem(int i) {
  tlItem[i].x = tlItem[i].y = -999;
  tlRandPos(tlItem[i].x, tlItem[i].y, i);
  tlItem[i].k = random(100) < 12 ? 3 : (uint8_t)random(3);
}

static void tlStartPlay() {
  int32_t x, y;
  tlBaseOk = tiltRead(x, y);
  tlBase[0] = tlBaseOk ? x : 0;
  tlBase[1] = tlBaseOk ? y : 0;
  tlBX = tlBY = CX; tlVX = tlVY = 0;
  for (auto &h : tlHole) h.x = h.y = -999;
  for (auto &it : tlItem) it.x = it.y = -999;
  for (auto &h : tlHole) tlRandPos(h.x, h.y, -1);
  for (int i = 0; i < TL_ITEMS; i++) tlSpawnItem(i);
  tlScore = 0;
  tlLastT = millis();
  tlEnd = tlLastT + TL_MS;
  tlFallT = tlPopT = 0;
  tlPhase = TL_PLAY;
  sfxPlay(SFX_TAP);
}

void startTilt() {
  perfReset();
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  if (!tiltAvailable()) { startGame(); return; }  // sin sensor: el de toques
  tlLoadMap();
  tiltOpen = true;
  tlPhase = TL_HOW;
  tlOverUntil = 0;
  tlNewHi = false;
}

static void tlStartCal() {
  int32_t x, y;
  tlBaseOk = tiltRead(x, y);
  tlBase[0] = x; tlBase[1] = y;
  tlMap[0] = tlMap[1] = -1;
  tlCalT = millis();
  tlPhase = TL_CAL_R;
  sfxPlay(SFX_TAP);
}

// aprender un eje: el que mas cambio (mas de 0,3 g) respecto al de partida
static void tlCalStep(uint32_t now) {
  int32_t r[2];
  if (!tiltRead(r[0], r[1])) {
    if (now - tlCalT > 6000) { tiltOpen = false; startGame(); }  // el sensor no contesta
    return;
  }
  int32_t d0 = r[0] - tlBase[0], d1 = r[1] - tlBase[1];
  int ax = abs(d0) > abs(d1) ? 0 : 1;
  int32_t d = ax ? d1 : d0;
  if (abs(d) < 300) return;
  if (tlPhase == TL_CAL_R) {
    tlMap[0] = (int8_t)(ax | (d < 0 ? 2 : 0));
    tlPhase = TL_CAL_D;
    tlCalT = now;
    sfxPlay(SFX_PLAY);
  } else if (tlPhase == TL_CAL_D && ax != (tlMap[0] & 1)) {
    tlMap[1] = (int8_t)(ax | (d < 0 ? 2 : 0));
    tlSaveMap();
    showToast(XT(X_TL_CAL_OK));
    sfxPlay(SFX_MEDAL);
    tlStartPlay();
  }
}

static void tlStep(uint32_t now) {
  float dt = (now - tlLastT) / 1000.0f;
  if (dt > 0.2f) dt = 0.2f;
  tlLastT = now;
  if (tlFallT) {  // cayendo en un agujero
    if (now - tlFallT >= TL_FALL_MS) { tlFallT = 0; tlBX = tlBY = CX; tlVX = tlVY = 0; }
    return;
  }
  int32_t raw[2];
  if (tiltRead(raw[0], raw[1])) {
    float gx = tlAxis(raw, tlMap[0]) / 1000.0f, gy = tlAxis(raw, tlMap[1]) / 1000.0f;
    if (gx > 0.6f) gx = 0.6f;
    if (gx < -0.6f) gx = -0.6f;
    if (gy > 0.6f) gy = 0.6f;
    if (gy < -0.6f) gy = -0.6f;
    tlVX += gx * 900.0f * dt;
    tlVY += gy * 900.0f * dt;
  }
  float fr = powf(0.35f, dt);  // rozamiento
  tlVX *= fr; tlVY *= fr;
  tlBX += tlVX * dt; tlBY += tlVY * dt;
  float dx = tlBX - CX, dy = tlBY - CX, d = sqrtf(dx * dx + dy * dy), lim = TL_ARENA - TL_BALL;
  if (d > lim) {  // borde: rebota
    float nx = dx / d, ny = dy / d, dot = tlVX * nx + tlVY * ny;
    if (dot > 0) { tlVX -= 1.5f * dot * nx; tlVY -= 1.5f * dot * ny; }
    tlBX = CX + nx * lim; tlBY = CX + ny * lim;
  }
  for (auto &h : tlHole) {  // agujeros: -5 s y vuelve al centro
    float hx = tlBX - h.x, hy = tlBY - h.y;
    if (hx * hx + hy * hy < 13 * 13) {
      tlFallT = now;
      tlEnd -= 5000;
      tlBX = h.x; tlBY = h.y;
      tlPopX = h.x; tlPopY = h.y; tlPopV = 0; tlPopT = now;
      sfxPlay(SFX_DENY);
      tlRandPos(h.x, h.y, -1);
      return;
    }
  }
  for (int i = 0; i < TL_ITEMS; i++) {  // bayas
    float ix = tlBX - tlItem[i].x, iy = tlBY - tlItem[i].y;
    if (ix * ix + iy * iy < 30 * 30) {
      uint8_t v = tlItem[i].k == 3 ? 3 : 1;
      tlScore += v;
      tlPopX = tlItem[i].x; tlPopY = tlItem[i].y; tlPopV = v; tlPopT = now;
      sfxPlay(v == 3 ? SFX_MEDAL : SFX_PLAY);
      tlSpawnItem(i);
    }
  }
}

static void tlPokeball(int cx, int cy, int r) {
  gfx->fillCircle(cx, cy, r, UI_WHITE);
  for (int y = -r; y <= 0; y++) {
    int w = (int)sqrtf((float)(r * r - y * y));
    gfx->drawFastHLine(cx - w, cy + y, 2 * w + 1, C565(0xe8, 0x3a, 0x3a));
  }
  gfx->fillRect(cx - r, cy - 2, 2 * r + 1, 4, UI_INK);
  gfx->drawCircle(cx, cy, r, UI_INK);
  gfx->fillCircle(cx, cy, r / 3, UI_WHITE);
  gfx->drawCircle(cx, cy, r / 3, UI_INK);
  gfx->fillCircle(cx - r / 2, cy - r / 2, r / 6 + 1, UI_WHITE);
}
static void tlBerry(int cx, int cy, uint8_t k) {
  if (k == 3) {  // dorada
    gfx->fillCircle(cx, cy + 2, 12, C565(0xff, 0xc8, 0x30));
    gfx->drawCircle(cx, cy + 2, 12, C565(0xa0, 0x70, 0x10));
    gfx->fillCircle(cx - 4, cy - 2, 3, UI_WHITE);
    gfx->fillRect(cx - 1, cy - 14, 3, 6, C565(0x4c, 0x8a, 0x3a));
    return;
  }
  static const char *const *const B[3] = { SPR_ICON_FOOD, SPR_ICON_BERRY_B, SPR_ICON_BERRY_G };
  drawMap(B[k % 3], 16, cx - 16, cy - 16, 2, false);
}
static void tlHoleDraw(int cx, int cy) {
  gfx->fillCircle(cx, cy, 17, C565(0x8a, 0x6a, 0x48));
  gfx->fillCircle(cx, cy, 14, C565(0x2a, 0x22, 0x1c));
}
static void tlArena() {
  uint16_t grass = C565(0x8c, 0xc8, 0x6a);
  gfx->fillScreen(C565(0x5a, 0x9a, 0x4a));
  gfx->fillCircle(CX, CX, TL_ARENA, grass);
  for (int i = 0; i < 40; i++) {
    int a = (i * 97) % 360, d = 30 + (i * 53) % 160;
    int x = CX + (int)(d * cosf(a * 0.01745f)), y = CX + (int)(d * sinf(a * 0.01745f));
    gfx->drawLine(x, y, x - 3, y - 6, lerp565(grass, UI_INK, 4, 16));
    gfx->drawLine(x, y, x + 3, y - 6, lerp565(grass, UI_INK, 4, 16));
  }
  gfx->drawCircle(CX, CX, TL_ARENA, C565(0x6a, 0x4a, 0x2a));
  gfx->drawCircle(CX, CX, TL_ARENA + 1, C565(0x6a, 0x4a, 0x2a));
}
static void tlTimerRing(float frac) {
  for (int a = 0; a < 360; a += 3) {
    float rad = (a - 90) * 0.01745f;
    bool on = a < frac * 360;
    uint16_t c = on ? (frac < 0.25f ? UI_BAR_BAD : UI_BAR_OK) : C565(0x40, 0x60, 0x38);
    gfx->fillCircle(CX + (int)(218 * cosf(rad)), CX + (int)(218 * sinf(rad)), 4, c);
  }
}

#define TL_START_X 153
#define TL_START_Y 342
#define TL_CAL_Y 404
static void tlHowTo() {
  uiScreenBg();
  drawFit(XT(X_TL_TITLE), 40, 320, UI_INK, 3);
  tlPokeball(CX, 132, 18);
  gfx->fillTriangle(CX - 40, 132, CX - 26, 122, CX - 26, 142, UI_INK);
  gfx->fillTriangle(CX + 40, 132, CX + 26, 122, CX + 26, 142, UI_INK);
  drawFit(XT(X_TL_HOW1), 172, 340, UI_INK, 1);
  tlBerry(126, 226, 0); drawFitIn(XT(X_TL_BERRY), 146, 216, 110, UI_INK, 1);
  tlBerry(262, 226, 3); drawFitIn(XT(X_TL_GOLD), 282, 216, 110, C565(0xd0, 0x90, 0x10), 1);
  tlHoleDraw(136, 272); drawFitIn(XT(X_TL_HOLE), 160, 262, 200, UI_BAR_BAD, 1);
  drawFit(XT(X_TL_HOW2), 306, 340, 0x8410, 1);
  drawBtn(TL_START_X, TL_START_Y, 160, 50, UI_BAR_OK, UI_WHITE, XT(X_TL_START));
  if (tlMapped()) drawFit(XT(X_TL_CAL_BTN), TL_CAL_Y, 200, C565(0x4c, 0x6a, 0xa0), 1);
}
static void tlCalScreen(uint32_t now) {
  uiScreenBg();
  drawFit(XT(X_TL_TITLE), 40, 320, UI_INK, 3);
  bool right = tlPhase == TL_CAL_R;
  drawFit(XT(right ? X_TL_CAL_R : X_TL_CAL_D), 120, 340, UI_INK, 2);
  // una placa que se inclina hacia donde toca
  float a = 0.35f * sinf(now * 0.006f);
  int cx = CX, cy = 250;
  if (right) {
    int dy = (int)(60 * a);
    gfx->fillTriangle(cx - 70, cy - dy, cx + 70, cy + dy, cx + 70, cy + dy + 10, C565(0xb8, 0xd8, 0xf0));
    gfx->fillTriangle(cx - 70, cy - dy, cx - 70, cy - dy + 10, cx + 70, cy + dy + 10, C565(0xb8, 0xd8, 0xf0));
    gfx->fillTriangle(cx + 92, cy + 20, cx + 78, cy + 10, cx + 78, cy + 30, UI_INK);
  } else {  // la parte de arriba se aleja (trapecio) y la de abajo baja hacia ti
    int k = (int)(26 * fabsf(a));
    uint16_t bc = C565(0xb8, 0xd8, 0xf0);
    gfx->fillTriangle(cx - 70 + k, cy - 34 + k / 2, cx + 70 - k, cy - 34 + k / 2, cx + 70, cy + 34, bc);
    gfx->fillTriangle(cx - 70 + k, cy - 34 + k / 2, cx - 70, cy + 34, cx + 70, cy + 34, bc);
    gfx->fillTriangle(cx, cy + 72, cx - 12, cy + 52, cx + 12, cy + 52, UI_INK);
  }
  tlPokeball(cx + (right ? (int)(40 * a) : 0), cy + (right ? 0 : (int)(20 * fabsf(a))) - 4, 12);
  drawFit(XT(X_TL_HOLD), 340, 340, 0x8410, 1);
}

void tiltPress(int16_t x, int16_t y) {
  if (tlOverUntil) return;
  if (tlPhase == TL_HOW) {
    if (inRect(x, y, TL_START_X - 10, TL_START_Y - 8, 180, 66)) {
      if (tlMapped()) tlStartPlay(); else tlStartCal();
    } else if (tlMapped() && y >= TL_CAL_Y - 14 && y < TL_CAL_Y + 30 && x > 120 && x < 346) {
      tlStartCal();
    }
  }
}

void renderTilt() {
  uint32_t now = millis();
  if (tlOverUntil) {
    if (!timeLeft(tlOverUntil)) { tiltOpen = false; backToTrainMenu(); return; }
    char s[32];
    snprintf(s, sizeof(s), XT(X_TL_RES_FMT), (unsigned)tlScore);
    const char *msg = tlNewHi ? XT(X_GAME_REWARD) : tlScore ? XT(X_GAME_SMALL) : XT(X_GAME_NO_REWARD);
    drawTrainResult(s, msg, tlNewHi ? UI_BAR_OK : UI_INK, tlNewHi, pet.gameHi, nullptr);
    return;
  }
  if (tlPhase == TL_HOW) { tlHowTo(); uiFlush(); return; }
  if (tlPhase == TL_CAL_R || tlPhase == TL_CAL_D) { tlCalStep(now); if (tiltOpen && tlPhase != TL_PLAY) tlCalScreen(now); uiFlush(); return; }
  if ((int32_t)(now - tlEnd) >= 0) {  // fin: como el juego de toques (record = animo + energia)
    pet.lastTrainExp = 0;
    pet.lastTrainCandy = 0;
    tlNewHi = pet.playResult((uint8_t)(tlScore > 255 ? 255 : tlScore));
    sfxPlay(tlNewHi ? SFX_MEDAL : SFX_LEVEL);
    tlOverUntil = now + 4000;
    return;
  }
  tlStep(now);
  tlArena();
  uint32_t left = (int32_t)(tlEnd - now) > 0 ? tlEnd - now : 0;
  tlTimerRing((float)left / TL_MS);
  for (auto &h : tlHole) tlHoleDraw(h.x, h.y);
  for (auto &it : tlItem) tlBerry(it.x, it.y, it.k);
  if (tlFallT) {  // se hunde
    int r = TL_BALL - (int)((now - tlFallT) * TL_BALL / TL_FALL_MS);
    if (r > 2) tlPokeball((int)tlBX, (int)tlBY, r);
  } else {
    float sp = sqrtf(tlVX * tlVX + tlVY * tlVY);
    if (sp > 60) {  // estela
      for (int k = 3; k >= 1; k--)
        gfx->fillCircle((int)(tlBX - tlVX * 0.03f * k), (int)(tlBY - tlVY * 0.03f * k), TL_BALL - 3 * k,
                        lerp565(C565(0x8c, 0xc8, 0x6a), UI_WHITE, 6 - k, 16));
    }
    tlPokeball((int)tlBX, (int)tlBY, TL_BALL);
  }
  if (tlPopT && now - tlPopT < 600) {  // +1 / +3 / -5초
    uint32_t t = now - tlPopT;
    char p[16];
    if (tlPopV) snprintf(p, sizeof(p), "+%u", tlPopV);
    else snprintf(p, sizeof(p), "%s", XT(X_TL_FALL));
    setSize(2);
    gfx->setTextColor(tlPopV == 3 ? C565(0xd0, 0x90, 0x10) : tlPopV ? UI_INK : UI_BAR_BAD);
    setCur(tlPopX - textW(p, 2) / 2, tlPopY - 30 - (int)(t / 25));
    printT(p);
  }
  uiPanel(173, 44, 120, 40, 16, UI_WHITE, UI_INK);
  char b[16];
  snprintf(b, sizeof(b), XT(X_TL_CNT_FMT), (unsigned)tlScore);
  drawFit(b, 52, 110, UI_INK, 2);
  snprintf(b, sizeof(b), XT(X_TL_SEC_FMT), (unsigned)((left + 999) / 1000));
  drawFit(b, 396, 100, UI_WHITE, 2);
  uiFlush();
}

// ---- para las pruebas (test/render) ----
float tpDiffProbe() { return tpDiff(tpAng, tpTgt); }
uint16_t tpRocksProbe() { return tpRocks; }
uint8_t tpLivesProbe() { return tpLives; }
uint8_t tiltPhaseProbe() { return tlPhase; }
float tiltBallXProbe() { return tlBX; }
bool tiltOverProbe() { return tlOverUntil != 0; }
void tiltForceScoreProbe(uint16_t s) { tlScore = s; }
void tiltResetMapProbe() { tlMapLoaded = false; tlMap[0] = tlMap[1] = -1; }

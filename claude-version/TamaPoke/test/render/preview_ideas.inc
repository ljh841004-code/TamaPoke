// ko12.6: MAQUETA (solo para ensenar la idea; no es codigo del firmware):
//   juego de inclinar: la placa se inclina y una pokeball rueda por el campo redondo
//   recogiendo bayas (sustituiria al juego de la pelota de tocar rapido)
// Se pinta encima de las pantallas de verdad para ver como quedaria.

static void pvPokeball(int cx, int cy, int r) {
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
static void pvHole(int cx, int cy, int r) {
  gfx->fillCircle(cx, cy, r + 3, C565(0x8a, 0x6a, 0x48));
  gfx->fillCircle(cx, cy, r, C565(0x2a, 0x22, 0x1c));
}
static void pvBerry(int cx, int cy, int kind) {
  if (kind == 3) {  // baya dorada (+3)
    gfx->fillCircle(cx, cy + 2, 12, C565(0xff, 0xc8, 0x30));
    gfx->drawCircle(cx, cy + 2, 12, C565(0xa0, 0x70, 0x10));
    gfx->fillCircle(cx - 4, cy - 2, 3, UI_WHITE);
    gfx->fillRect(cx - 1, cy - 14, 3, 6, C565(0x4c, 0x8a, 0x3a));
    for (int a = 0; a < 4; a++) gfx->drawLine(cx + 16, cy - 12 + a * 8, cx + 20, cy - 12 + a * 8, C565(0xff, 0xd0, 0x40));
    return;
  }
  static const char *const *const B[3] = { SPR_ICON_FOOD, SPR_ICON_BERRY_B, SPR_ICON_BERRY_G };
  drawMap(B[kind % 3], 16, cx - 16, cy - 16, 2, false);
}
static void pvArena(uint16_t grass) {
  gfx->fillScreen(C565(0x5a, 0x9a, 0x4a));
  gfx->fillCircle(CX, CX, 200, grass);
  for (int i = 0; i < 40; i++) {  // matas de hierba
    int a = (i * 97) % 360, d = 30 + (i * 53) % 160;
    int x = CX + (int)(d * cosf(a * 0.01745f)), y = CX + (int)(d * sinf(a * 0.01745f));
    gfx->drawLine(x, y, x - 3, y - 6, lerp565(grass, UI_INK, 4, 16));
    gfx->drawLine(x, y, x + 3, y - 6, lerp565(grass, UI_INK, 4, 16));
  }
  gfx->drawCircle(CX, CX, 200, C565(0x6a, 0x4a, 0x2a));
  gfx->drawCircle(CX, CX, 201, C565(0x6a, 0x4a, 0x2a));
}
static void pvTimerRing(float frac) {  // tiempo restante por el borde
  for (int a = 0; a < 360; a += 2) {
    float rad = (a - 90) * 0.01745f;
    bool on = a < frac * 360;
    uint16_t c = on ? (frac < 0.25f ? UI_BAR_BAD : UI_BAR_OK) : C565(0x40, 0x60, 0x38);
    gfx->fillCircle(CX + (int)(220 * cosf(rad)), CX + (int)(220 * sinf(rad)), 4, c);
  }
}
static void pvTiltPhone(int cx, int cy, int deg, uint16_t c) {  // dibujito de la placa inclinada
  float r = deg * 0.01745f;
  int pts[4][2] = { { -34, -22 }, { 34, -22 }, { 34, 22 }, { -34, 22 } };
  int q[4][2];
  for (int i = 0; i < 4; i++) {
    q[i][0] = cx + (int)(pts[i][0] * cosf(r) - pts[i][1] * sinf(r));
    q[i][1] = cy + (int)(pts[i][0] * sinf(r) + pts[i][1] * cosf(r));
  }
  gfx->fillTriangle(q[0][0], q[0][1], q[1][0], q[1][1], q[2][0], q[2][1], c);
  gfx->fillTriangle(q[0][0], q[0][1], q[2][0], q[2][1], q[3][0], q[3][1], c);
  for (int i = 0; i < 4; i++) gfx->drawLine(q[i][0], q[i][1], q[(i + 1) % 4][0], q[(i + 1) % 4][1], UI_INK);
  gfx->fillCircle(cx, cy, 14, UI_WHITE);
  gfx->drawCircle(cx, cy, 14, UI_INK);
}

static void previewShots() {
  // ---------------- 1) como se juega ----------------
  closeAll();
  uiScreenBg();
  drawFit("기울여 열매 모으기", 40, 320, UI_INK, 3);
  pvTiltPhone(150, 150, -18, C565(0xb8, 0xd8, 0xf0));
  pvTiltPhone(316, 150, 18, C565(0xb8, 0xd8, 0xf0));
  gfx->fillTriangle(196, 150, 214, 140, 214, 160, UI_INK);
  gfx->fillTriangle(270, 150, 252, 140, 252, 160, UI_INK);
  pvPokeball(233, 150, 14);
  drawFit("기기를 기울이면 몬스터볼이 굴러가요", 196, 340, UI_INK, 1);
  pvBerry(120, 250, 0); drawFitIn("열매 +1", 146, 240, 120, UI_INK, 1);
  pvBerry(260, 250, 3); drawFitIn("금색 +3", 286, 240, 120, C565(0xd0, 0x90, 0x10), 1);
  pvHole(132, 296, 12); drawFitIn("구멍: 시간 -5초", 152, 286, 200, UI_BAR_BAD, 1);
  drawFit("30초 동안 최대한 많이!  (기울이기 센서 없으면 예전 공놀이)", 330, 340, 0x8410, 1);
  drawBtn(153, 364, 160, 52, UI_BAR_OK, UI_WHITE, "시작");
  shot("p12a_tilt_howto");

  // ---------------- 2) jugando ----------------
  pvArena(C565(0x8c, 0xc8, 0x6a));
  pvTimerRing(0.62f);
  pvHole(170, 300, 14);
  pvHole(318, 180, 14);
  pvBerry(140, 170, 0); pvBerry(300, 290, 2); pvBerry(250, 120, 3); pvBerry(360, 250, 0); pvBerry(200, 360, 2);
  // estela de la pelota (va hacia abajo a la derecha)
  for (int k = 4; k >= 1; k--) gfx->fillCircle(233 - k * 10, 232 - k * 7, 16 - k * 2, lerp565(C565(0x8c, 0xc8, 0x6a), UI_WHITE, 6 - k, 16));
  pvPokeball(233, 232, 18);
  // marcador arriba, dentro del circulo
  uiPanel(173, 44, 120, 40, 16, UI_WHITE, UI_INK);
  drawFit("12개", 52, 110, UI_INK, 2);
  drawFit("19초", 396, 100, UI_WHITE, 2);
  shot("p12b_tilt_play");

  // ---------------- 2b) atrapando una dorada (efecto) ----------------
  pvArena(C565(0x8c, 0xc8, 0x6a));
  pvTimerRing(0.30f);
  pvHole(150, 220, 14); pvHole(300, 320, 14); pvHole(330, 150, 14);
  pvBerry(120, 300, 0); pvBerry(260, 370, 2);
  for (int a = 0; a < 8; a++) {  // destellos
    float r = a * 0.785f;
    gfx->fillCircle(250 + (int)(34 * cosf(r)), 200 + (int)(34 * sinf(r)), 4, C565(0xff, 0xd0, 0x40));
  }
  pvPokeball(250, 200, 18);
  drawFit("+3", 140, 60, C565(0xd0, 0x90, 0x10), 3);
  uiPanel(173, 44, 120, 40, 16, UI_WHITE, UI_INK);
  drawFit("21개", 52, 110, UI_INK, 2);
  drawFit("9초", 396, 100, UI_WHITE, 2);
  shot("p12c_tilt_golden");

  // ---------------- 3) resultado ----------------
  uiScreenBg();
  drawFit("끝!", 50, 300, UI_INK, 3);
  pvBerry(200, 140, 0); pvBerry(233, 128, 3); pvBerry(266, 140, 2);
  drawFit("열매 24개", 176, 300, UI_INK, 3);
  drawFit("최고 기록! (전 기록 19개)", 222, 320, UI_BAR_OK, 2);
  drawFit("기분 +20   기력 +10   경험치 +12", 262, 340, UI_INK, 1);
  drawFit("하루 일과: 낮 놀이 완료", 290, 340, 0x8410, 1);
  drawBtn(90, 330, 136, 52, UI_BAR_OK, UI_WHITE, "한 번 더");
  drawBtn(240, 330, 136, 52, UI_WHITE, UI_INK, "그만");
  shot("p12d_tilt_result");
  closeAll();
}

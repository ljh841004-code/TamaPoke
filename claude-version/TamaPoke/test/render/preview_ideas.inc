// ko12.4: MAQUETAS (solo para ensenar ideas; no es codigo del firmware):
//   - #5 decorar la habitacion
//   - #8 paseo (podometro con el sensor de movimiento de la placa)
// Se pintan encima de las pantallas de verdad para ver como quedarian.

static void pvCushion(int cx, int cy, int s) {  // cojin / camita
  uint16_t pk = C565(0xf2, 0x9a, 0xb8), dk = uiLerp(pk, UI_INK, 7, 16);
  gfx->fillRoundRect(cx - 26 * s / 2, cy - 9 * s / 2, 26 * s, 12 * s, 5 * s, pk);
  gfx->drawRoundRect(cx - 26 * s / 2, cy - 9 * s / 2, 26 * s, 12 * s, 5 * s, dk);
  gfx->fillRoundRect(cx - 22 * s / 2, cy - 15 * s / 2, 9 * s, 6 * s, 3 * s, UI_WHITE);
  gfx->drawRoundRect(cx - 22 * s / 2, cy - 15 * s / 2, 9 * s, 6 * s, 3 * s, dk);
  for (int i = 0; i < 3; i++) gfx->fillCircle(cx + (i - 1) * 7 * s, cy + s, s, dk);
}
static void pvPlant(int cx, int cy, int s) {  // maceta
  uint16_t pot = C565(0xc0, 0x6a, 0x3a), lf = C565(0x4c, 0xb0, 0x50), lf2 = C565(0x2e, 0x86, 0x3a);
  gfx->fillEllipse(cx - 6 * s, cy - 18 * s, 5 * s, 9 * s, lf2);
  gfx->fillEllipse(cx + 6 * s, cy - 18 * s, 5 * s, 9 * s, lf2);
  gfx->fillEllipse(cx, cy - 22 * s, 5 * s, 11 * s, lf);
  gfx->fillTriangle(cx - 9 * s, cy - 8 * s, cx + 9 * s, cy - 8 * s, cx + 6 * s, cy + 4 * s, pot);
  gfx->fillTriangle(cx - 9 * s, cy - 8 * s, cx - 6 * s, cy + 4 * s, cx + 6 * s, cy + 4 * s, pot);
  gfx->fillRect(cx - 10 * s, cy - 10 * s, 20 * s, 3 * s, uiLerp(pot, UI_INK, 5, 16));
}
static void pvBall(int cx, int cy, int s) {  // pelota de playa
  gfx->fillCircle(cx, cy, 8 * s, UI_WHITE);
  gfx->fillCircle(cx, cy, 8 * s, C565(0xff, 0x5a, 0x4a));
  gfx->fillRect(cx - 8 * s, cy - 2 * s, 16 * s, 4 * s, C565(0xff, 0xd8, 0x40));
  gfx->fillRect(cx - 2 * s, cy - 8 * s, 4 * s, 16 * s, C565(0x3a, 0x8c, 0xff));
  gfx->drawCircle(cx, cy, 8 * s, UI_INK);
  gfx->fillCircle(cx - 3 * s, cy - 3 * s, 2 * s, UI_WHITE);
}
static void pvLamp(int cx, int cy, int s) {  // farolillo
  uint16_t y = C565(0xff, 0xe0, 0x70);
  gfx->fillCircle(cx, cy - 16 * s, 10 * s, uiLerp(y, UI_BG_DAY, 11, 16));
  gfx->fillRect(cx - 1 * s, cy - 8 * s, 2 * s, 12 * s, C565(0x6a, 0x50, 0x3a));
  gfx->fillRoundRect(cx - 6 * s, cy - 22 * s, 12 * s, 12 * s, 3 * s, y);
  gfx->drawRoundRect(cx - 6 * s, cy - 22 * s, 12 * s, 12 * s, 3 * s, UI_INK);
  gfx->fillRect(cx - 7 * s, cy + 3 * s, 14 * s, 2 * s, C565(0x6a, 0x50, 0x3a));
}
static void pvFoot(int x, int y, uint16_t c) {  // huella
  gfx->fillEllipse(x, y + 3, 4, 6, c);
  for (int i = 0; i < 3; i++) gfx->fillCircle(x - 4 + i * 4, y - 6, 2, c);
}

static void previewShots() {
  // ---------------- #5 decorar la habitacion ----------------
  closeAll();
  render();
  pvCushion(80, 282, 3);
  pvLamp(150, 290, 2);
  pvBall(330, 296, 2);
  pvPlant(396, 284, 2);
  shot("p5a_room_main");

  uiScreenBg();
  drawFit("방 꾸미기", 34, 300, UI_INK, 2);
  // los 3 sitios del suelo con lo puesto
  uiPanel(48, 62, 370, 132, 18, C565(0xfa, 0xf2, 0xe4), UI_INK);
  gfx->fillRect(60, 140, 346, 24, C565(0xd8, 0xc0, 0x98));
  pvCushion(118, 140, 2); pvBall(233, 140, 2); pvPlant(348, 146, 2);
  textAtX("왼쪽", 118, 168, UI_INK, 1); textAtX("가운데", 233, 168, UI_INK, 1); textAtX("오른쪽", 348, 168, UI_INK, 1);
  // lo que tienes y como se consigue
  struct It { const char *nm, *how; bool got; int kind; };
  const It IT[6] = {
    { "쿠션", "처음부터", true, 0 }, { "화분", "100일 돌보기", true, 1 }, { "비치볼", "공놀이 20점", true, 2 },
    { "등불", "산책 1만 걸음", false, 3 }, { "트로피", "챔피언 1번", false, 2 }, { "인형", "도감 100종", false, 0 },
  };
  for (int i = 0; i < 6; i++) {
    int x = 60 + (i % 2) * 176, y = 206 + (i / 2) * 58;
    uiPanel(x, y, 168, 52, 14, IT[i].got ? UI_WHITE : C565(0xe4, 0xe0, 0xd8), IT[i].got ? UI_INK : 0x8410);
    int cx = x + 28, cy = y + 28;
    if (IT[i].got) {
      if (IT[i].kind == 0) pvCushion(cx, cy, 1); else if (IT[i].kind == 1) pvPlant(cx, cy + 12, 1);
      else if (IT[i].kind == 2) pvBall(cx, cy, 1); else pvLamp(cx, cy + 12, 1);
    } else {  // candado
      gfx->drawCircle(cx, cy - 6, 7, 0x8410); gfx->fillRoundRect(cx - 10, cy - 4, 20, 16, 3, 0x8410);
    }
    drawFitIn(IT[i].nm, x + 54, y + 6, 110, IT[i].got ? UI_INK : 0x8410, 1);
    drawFitIn(IT[i].how, x + 54, y + 26, 110, 0x8410, 1);
  }
  drawFit("물건을 누르고 놓을 자리를 누르면 끝", 384, 330, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
  shot("p5b_room_menu");

  // ---------------- #8 paseo ----------------
  closeAll();
  render();
  uiPanel(18, 54, 104, 30, 14, UI_WHITE, UI_INK);
  pvFoot(36, 69, C565(0x8a, 0x5a, 0x3a));
  drawFitIn("3,214", 48, 61, 70, UI_INK, 1);
  shot("p8a_walk_main");

  uiScreenBg();
  drawFit("오늘의 산책", 34, 300, UI_INK, 2);
  pvFoot(150, 92, C565(0x8a, 0x5a, 0x3a)); pvFoot(166, 80, C565(0x8a, 0x5a, 0x3a));
  drawFit("3,214 걸음", 84, 260, UI_INK, 3);
  // barra hacia la meta de hoy
  int bx = 83, bw = 300, by = 130;
  gfx->fillRoundRect(bx, by, bw, 18, 9, UI_TRACK);
  gfx->fillRoundRect(bx, by, bw * 3214 / 5000, 18, 9, UI_BAR_OK);
  gfx->drawRoundRect(bx, by, bw, 18, 9, UI_INK);
  for (int k : { 2000, 5000 }) gfx->fillCircle(bx + bw * k / 5000, by + 9, 4, UI_INK);
  drawFit("목표 5,000 걸음까지 1,786", 158, 320, 0x8410, 1);
  // la semana
  static const int WK[7] = { 4210, 6120, 1830, 5005, 2890, 7430, 3214 };
  static const char *const DN[7] = { "토", "일", "월", "화", "수", "목", "금" };
  for (int i = 0; i < 7; i++) {
    int x = 103 + i * 38, h = WK[i] * 80 / 8000, base = 270;
    uint16_t c = WK[i] >= 5000 ? UI_BAR_OK : C565(0xb8, 0xc8, 0xd8);
    gfx->fillRoundRect(x, base - h, 24, h, 5, i == 6 ? C565(0xff, 0xb0, 0x40) : c);
    textAtX(DN[i], x + 12, base + 4, UI_INK, 1);
  }
  // premios del dia
  struct Rw { const char *s; bool ok; };
  const Rw RW[3] = { { "2,000  기분 +20, 친밀도 +2", true }, { "5,000  사탕 1개", false }, { "10,000  만능 사탕 조각 2", false } };
  for (int i = 0; i < 3; i++) {
    int y = 304 + i * 24;
    gfx->fillCircle(112, y + 8, 7, RW[i].ok ? UI_BAR_OK : UI_TRACK);
    drawFitIn(RW[i].s, 126, y, 230, RW[i].ok ? UI_INK : 0x8410, 1);
  }
  drawFit("흔들면: 자는 중엔 깨우기, 깨어 있으면 신나서 뛰어요", 380, 340, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
  shot("p8b_walk_page");
  closeAll();
}

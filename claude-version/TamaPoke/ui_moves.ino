// ======================================================================
// ko12.10 (미리보기): 원작(크리스탈) 기술 체계 화면
//   - 전투 기술 창 v2: 큰 2x2 버튼(속성 그림 · 이름 · 물리/특수/변화 · PP 막대 · 효과 표시), 아래 [◀ 뒤로]
//   - 기술 설명 창: 길게 누르기(전투) / 누르기(기술 쪽) -> 위력 · 명중 · PP · 설명
//   - 상태 카드 "기술" 쪽: 장착 4칸 + 배운 기술 목록(분류 탭, 쪽 넘기기)
//   - 도감 "기술표": 레벨업 / 기술머신
//   - 기술머신 가방
// 데이터: moves2_data.h (tools/gen_moves2.py, PokeAPI 크리스탈)
// ======================================================================
#include "moves2_data.h"

enum : uint8_t { MVK2_0 = 0, MVK2_SUPER, MVK2_WEAK, MVK2_NONE, MVK2_REST };

uint16_t mv2Col(uint8_t t) { return t == MV2_FLYING ? C565(0x78, 0x98, 0xe8) : typeColor(t); }
const char *mv2TypeName(uint8_t t) { return t == MV2_FLYING ? "비행" : typeName(t); }
const char *mv2ClassName(uint8_t c) { return c == MV2_PHYS ? "물리" : c == MV2_SPEC ? "특수" : "변화"; }
uint8_t mv2Find(const char *ko) {
  for (uint8_t i = 1; i < MV2_N; i++) if (!strcmp(MV2_KO[i], ko)) return i;
  return 0;
}

// 물리 = 주황 폭발, 특수 = 파란 동심원, 변화 = 회색 반원 두 개 (원작 아이콘 느낌)
void mv2ClassIcon(int cx, int cy, uint8_t cls, int r) {
  if (cls == MV2_PHYS) {
    uint16_t c = C565(0xf0, 0x70, 0x30);
    gfx->fillCircle(cx, cy, r, c);
    for (int k = 0; k < 8; k++) {
      float a = k * 0.785f;
      gfx->fillTriangle(cx + (int)(cosf(a) * (r + 3)), cy + (int)(sinf(a) * (r + 3)),
                        cx + (int)(cosf(a + 0.35f) * (r - 1)), cy + (int)(sinf(a + 0.35f) * (r - 1)),
                        cx + (int)(cosf(a - 0.35f) * (r - 1)), cy + (int)(sinf(a - 0.35f) * (r - 1)), c);
    }
    gfx->fillCircle(cx, cy, r / 2, C565(0xff, 0xe0, 0x60));
  } else if (cls == MV2_SPEC) {
    uint16_t c = C565(0x50, 0x70, 0xe0);
    gfx->fillCircle(cx, cy, r + 1, c);
    gfx->drawCircle(cx, cy, r - 3, UI_WHITE);
    gfx->fillCircle(cx, cy, r / 3, UI_WHITE);
  } else {
    uint16_t c = C565(0x9a, 0x98, 0xa4);
    gfx->fillCircle(cx, cy, r + 1, c);
    gfx->fillCircle(cx - r / 3, cy, r / 3 + 1, UI_WHITE);
    gfx->fillCircle(cx + r / 3, cy, r / 3 + 1, C565(0x50, 0x50, 0x58));
  }
}

// 비행 그림 (날개 둘): 나머지 속성은 drawTypeGlyph
static void mv2Glyph(int x, int y, uint8_t t, uint8_t cls, uint16_t bg) {
  if (t != MV2_FLYING) { drawTypeGlyph(x, y, t, cls == MV2_STAT, bg); return; }
  for (int s = 0; s < 2; s++) {
    uint16_t c = s ? UI_WHITE : UI_INK;
    int o = s ? 0 : 1;
    gfx->fillTriangle(x - 13 + o, y + 6 + o, x + o, y - 2 + o, x - 4 + o, y + 9 + o, c);
    gfx->fillTriangle(x - 13 + o, y + 6 + o, x - 10 + o, y - 9 + o, x + o, y - 2 + o, c);
    gfx->fillTriangle(x + 13 + o, y + 6 + o, x + o, y - 2 + o, x + 4 + o, y + 9 + o, c);
    gfx->fillTriangle(x + 13 + o, y + 6 + o, x + 10 + o, y - 9 + o, x + o, y - 2 + o, c);
  }
}

// 글자 줄바꿈 (띄어쓰기 기준)
static int mvWrap(const char *s, int w, uint8_t size, char out[][120], int maxL) {
  int n = 0;
  char line[120] = "";
  char word[64];
  const char *p = s;
  while (*p && n < maxL) {
    int k = 0;
    while (*p == ' ') p++;
    while (*p && *p != ' ' && k < 63) word[k++] = *p++;
    word[k] = 0;
    if (!k) break;
    char test[120];
    snprintf(test, sizeof(test), line[0] ? "%s %s" : "%s%s", line, word);
    if (textW(test, size) > w && line[0]) {
      strcpy(out[n++], line);
      strcpy(line, word);
    } else {
      strcpy(line, test);
    }
  }
  if (line[0] && n < maxL) strcpy(out[n++], line);
  return n;
}

// ---------------------------------------------------------------- 전투 기술 창 v2
#define FV_X0 69
#define FV_X1 237
#define FV_W 160
#define FV_H 64
#define FV_Y0 266
#define FV_Y1 336
#define FV_BX 166
#define FV_BY 408
#define FV_BW 134
#define FV_BH 36
void drawFightBtnV2(int x, int y, uint8_t id, uint8_t pp, uint8_t mark) {
  if (!id) {
    uiButton(x, y, FV_W, FV_H, 16, UI_TRACK, C565(0xb8, 0xb4, 0xac));
    drawFitIn("-", x, y + 20, FV_W, 0x8410, 2);
    return;
  }
  const Mv2Def &d = MV2[id];
  bool off = pp == 0 || mark == MVK2_REST;
  uint16_t bg = off ? UI_TRACK : mv2Col(d.type);
  uiButton(x, y, FV_W, FV_H, 16, bg, UI_INK);
  if (!off) gfx->fillRoundRect(x + 4, y + 3, FV_W - 8, 10, 5, lerp565(bg, UI_WHITE, 5, 16));  // 윗쪽 빛
  mv2Glyph(x + 22, y + 24, d.type, d.cls, bg);
  uint16_t ink = off ? 0x8410 : UI_WHITE;
  drawFitIn(MV2_KO[id], x + 40, y + 8, FV_W - 46, ink, 2);
  // 아랫줄: 물리/특수/변화 아이콘 + PP, 오른쪽 효과
  mv2ClassIcon(x + 50, y + 46, d.cls, 6);
  char pps[16];
  if (mark == MVK2_REST) snprintf(pps, sizeof(pps), "쉬는 중");
  else snprintf(pps, sizeof(pps), "%u/%u", pp, d.pp);
  bool low = !off && pp * 4 <= d.pp;
  gfx->setTextColor(low ? C565(0xff, 0xe0, 0x50) : ink);
  setSize(1);
  setCur(x + 61, y + 36);
  printT(pps);
  // PP 막대
  int bw = FV_W - 50, bx = x + 40, by = y + FV_H - 7;
  gfx->fillRoundRect(bx, by, bw, 4, 2, lerp565(bg, UI_INK, 6, 16));
  if (d.pp) gfx->fillRoundRect(bx, by, bw * pp / d.pp, 4, 2, low ? C565(0xff, 0xd0, 0x40) : UI_WHITE);
  if (mark == MVK2_SUPER || mark == MVK2_WEAK || mark == MVK2_NONE) {
    const char *tx = mark == MVK2_SUPER ? "굉장" : mark == MVK2_WEAK ? "별로" : "없음";
    uint16_t pc = mark == MVK2_SUPER ? C565(0xe8, 0x30, 0x20) : mark == MVK2_WEAK ? C565(0x50, 0x5a, 0x70) : UI_INK;
    int pw = textW(tx, 1) + 14;  // 오른쪽 위 모서리에 걸친 딱지 (이름·PP를 가리지 않게)
    uiButton(x + FV_W - pw + 6, y - 11, pw, 22, 11, pc, UI_WHITE);
    drawFitIn(tx, x + FV_W - pw + 6, y - 10, pw, UI_WHITE, 1);
  }
}

void drawFightPanelV2(const uint8_t mv[4], const uint8_t pp[4], const uint8_t mark[4]) {
  for (uint8_t i = 0; i < 4; i++) drawFightBtnV2(i & 1 ? FV_X1 : FV_X0, i & 2 ? FV_Y1 : FV_Y0, mv[i], pp[i], mark[i]);
  uiButton(FV_BX, FV_BY, FV_BW, FV_BH, 18, UI_WHITE, UI_INK);
  drawFitIn("< 뒤로", FV_BX, FV_BY + 6, FV_BW, UI_INK, 2);
}

// ---------------------------------------------------------------- 기술 설명 창
// buttons: 0 = [닫기]만, 1 = [장착하기][닫기], 2 = [가르치기][닫기]
void drawMoveInfoV2(uint8_t id, uint8_t buttons) {
  const Mv2Def &d = MV2[id];
  const int px = 52, py = 92, pw = 362, ph = 300;
  uint16_t c = mv2Col(d.type);
  gfx->fillRoundRect(px + 4, py + 6, pw, ph, 26, lerp565(UI_INK, UI_BG_DAY, 10, 16));  // 그림자
  uiPanel(px, py, pw, ph, 26, UI_WHITE, UI_INK);
  gfx->fillRoundRect(px + 2, py + 2, pw - 4, 64, 24, c);
  gfx->fillRect(px + 2, py + 40, pw - 4, 26, c);
  mv2Glyph(px + 34, py + 34, d.type, d.cls, c);
  drawFitIn(MV2_KO[id], px + 60, py + 14, pw - 76, UI_WHITE, 3);
  // 속성 · 분류 알약
  int y = py + 78;
  uiButton(px + 22, y, 92, 28, 14, c, UI_INK);
  drawFitIn(mv2TypeName(d.type), px + 22, y + 4, 92, UI_WHITE, 1);
  uiButton(px + 122, y, 104, 28, 14, UI_WHITE, UI_INK);
  mv2ClassIcon(px + 140, y + 14, d.cls, 7);
  drawFitIn(mv2ClassName(d.cls), px + 150, y + 4, 70, UI_INK, 1);
  // 위력 · 명중 · PP
  y += 40;
  const char *lab[3] = { "위력", "명중", "PP" };
  char val[3][8];
  if (d.pow) snprintf(val[0], 8, "%u", d.pow); else strcpy(val[0], "-");
  if (d.acc) snprintf(val[1], 8, "%u", d.acc); else strcpy(val[1], "-");
  snprintf(val[2], 8, "%u", d.pp);
  for (int k = 0; k < 3; k++) {
    int bx = px + 22 + k * 108;
    gfx->fillRoundRect(bx, y, 100, 52, 12, C565(0xf4, 0xef, 0xe6));
    drawFitIn(lab[k], bx, y + 4, 100, 0x8410, 1);
    drawFitIn(val[k], bx, y + 22, 100, UI_INK, 2);
  }
  // 설명
  y += 62;
  char L[3][120];
  int n = mvWrap(MV2_DESC[id], pw - 50, 1, L, 3);
  gfx->setTextColor(UI_INK);
  setSize(1);
  for (int i = 0; i < n; i++) { setCur(px + 25, y + i * 22); printT(L[i]); }
  // 버튼
  int by = py + ph - 52;
  if (buttons) {
    drawBtn(px + 34, by, 140, 40, buttons == 1 ? UI_BAR_OK : C565(0x6a, 0x4c, 0xf0), UI_WHITE, buttons == 1 ? "장착하기" : "가르치기");
    drawBtn(px + pw - 174, by, 140, 40, UI_TRACK, UI_INK, "닫기");
  } else {
    drawBtn(px + pw / 2 - 75, by, 150, 40, UI_TRACK, UI_INK, "닫기");
  }
}

// ---------------------------------------------------------------- 상태 카드 "기술" 쪽
// eq: 장착 4칸, lr: 배운 기술 목록, filt: 0 전체 1 물리 2 특수 3 변화, page
#define MC_ROWS 4
void renderMovesCardV2(const uint8_t eq[4], const uint8_t eqpp[4], const uint8_t *lr, uint8_t nlr, uint8_t filt,
                       uint8_t page) {
  uiScreenBg();
  char t[40];
  uint8_t list[64], n = 0;
  for (uint8_t i = 0; i < nlr && n < 64; i++)
    if (!filt || MV2[lr[i]].cls == filt - 1) list[n++] = lr[i];
  drawFit("기술", 30, 200, UI_INK, 2);
  snprintf(t, sizeof(t), "배운 기술 %u개", nlr);
  drawFit(t, 58, 220, 0x8410, 1);
  // 장착 4칸 (작은 버튼)
  for (uint8_t i = 0; i < 4; i++) {
    int x = i & 1 ? 237 : 83, y = i & 2 ? 132 : 84;
    uint8_t id = eq[i];
    if (!id) { uiButton(x, y, 146, 42, 12, UI_TRACK, UI_INK); continue; }
    uint16_t c = mv2Col(MV2[id].type);
    uiButton(x, y, 146, 42, 12, c, UI_INK);
    drawFitIn(MV2_KO[id], x + 6, y + 3, 112, UI_WHITE, 1);
    char p[12];
    snprintf(p, sizeof(p), "%u/%u", eqpp[i], MV2[id].pp);
    gfx->setTextColor(UI_WHITE);
    setSize(1);
    setCur(x + 8, y + 21);
    printT(mv2TypeName(MV2[id].type));
    setCur(x + 138 - textW(p, 1), y + 21);
    printT(p);
    mv2ClassIcon(x + 133, y + 12, MV2[id].cls, 5);
  }
  // 분류 탭
  static const char *TAB[4] = { "전체", "물리", "특수", "변화" };
  for (uint8_t k = 0; k < 4; k++) {
    int x = 75 + k * 80;
    bool on = k == filt;
    uiButton(x, 184, 74, 32, 16, on ? UI_INK : UI_WHITE, UI_INK);
    drawFitIn(TAB[k], x, 189, 74, on ? UI_WHITE : UI_INK, 1);
  }
  // 목록
  uint8_t pages = n ? (n + MC_ROWS - 1) / MC_ROWS : 1;
  if (page >= pages) page = pages - 1;
  for (uint8_t r = 0; r < MC_ROWS; r++) {
    uint8_t k = page * MC_ROWS + r;
    if (k >= n) break;
    uint8_t id = list[k];
    int y = 226 + r * 40;
    bool eqd = id == eq[0] || id == eq[1] || id == eq[2] || id == eq[3];
    uiButton(66, y, 334, 36, 10, eqd ? C565(0xff, 0xf4, 0xd6) : UI_WHITE, UI_INK);
    gfx->fillRoundRect(68, y + 2, 10, 32, 5, mv2Col(MV2[id].type));
    mv2ClassIcon(94, y + 18, MV2[id].cls, 7);
    drawFitIn(MV2_KO[id], 108, y + 6, 150, UI_INK, 2);
    char s[20];
    if (MV2[id].pow) snprintf(s, sizeof(s), "위력 %u", MV2[id].pow); else strcpy(s, "변화");
    gfx->setTextColor(0x8410);
    setSize(1);
    setCur(eqd ? 296 : 330, y + 9);
    printT(s);
    if (eqd) { uiButton(344, y + 7, 50, 22, 11, UI_BAR_OK, UI_INK); drawFitIn("장착", 344, y + 8, 50, UI_WHITE, 1); }
  }
  // 쪽
  snprintf(t, sizeof(t), "%u/%u", page + 1, pages);
  drawBtn(118, 392, 64, 38, page ? UI_WHITE : UI_TRACK, UI_INK, "<");
  drawBtn(284, 392, 64, 38, page + 1 < pages ? UI_WHITE : UI_TRACK, UI_INK, ">");
  drawFit(t, 402, 90, UI_INK, 2);
  drawFit("누르면 설명 / 장착", 440, 220, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
}

// ---------------------------------------------------------------- 도감 "기술표"
uint8_t mv2Learnset(int16_t dex, uint8_t lv[], uint8_t mv[], uint8_t max) {
  if (dex < 1 || dex > 251) return 0;
  uint8_t n = 0;
  for (uint16_t i = MV2_LS_OFF[dex - 1]; i < MV2_LS_OFF[dex] && n < max; i++, n++) { lv[n] = MV2_LS[i][0]; mv[n] = MV2_LS[i][1]; }
  return n;
}
bool mv2TmCan(int16_t dex, uint8_t tm) { return dex >= 1 && dex <= 251 && tm < TM_N && (TM_BITS[dex][tm / 8] >> (tm % 8)) & 1; }

#define DL_ROWS 6
void renderDexMovesV2(int16_t dex, uint8_t tab, uint8_t page, const uint8_t *known, uint8_t nk, uint8_t petLv) {
  uiScreenBg();
  char t[48];
  snprintf(t, sizeof(t), "No.%03d %s", dex, dexName(dex));
  drawFit(t, 30, 300, UI_INK, 2);
  uint8_t lv[24], mv[24], n = mv2Learnset(dex, lv, mv, 24);
  uint8_t tms[TM_N], nt = 0;
  for (uint8_t i = 0; i < TM_N; i++) if (mv2TmCan(dex, i)) tms[nt++] = i;
  char a[24], b[24];
  snprintf(a, sizeof(a), "레벨업 %u", n);
  snprintf(b, sizeof(b), "기술머신 %u", nt);
  uiButton(88, 64, 140, 34, 17, tab == 0 ? UI_INK : UI_WHITE, UI_INK);
  drawFitIn(a, 88, 70, 140, tab == 0 ? UI_WHITE : UI_INK, 1);
  uiButton(238, 64, 140, 34, 17, tab == 1 ? UI_INK : UI_WHITE, UI_INK);
  drawFitIn(b, 238, 70, 140, tab == 1 ? UI_WHITE : UI_INK, 1);
  uint8_t total = tab ? nt : n;
  uint8_t pages = total ? (total + DL_ROWS - 1) / DL_ROWS : 1;
  for (uint8_t r = 0; r < DL_ROWS; r++) {
    uint8_t k = page * DL_ROWS + r;
    if (k >= total) break;
    uint8_t id = tab ? TM_MOVE[tms[k]] : mv[k];
    int y = 110 + r * 44;
    bool has = false;
    for (uint8_t q = 0; q < nk; q++) if (known[q] == id) has = true;
    bool future = !tab && lv[k] > petLv;
    uiButton(56, y, 354, 40, 10, has ? C565(0xe6, 0xf6, 0xe0) : UI_WHITE, UI_INK);
    char l[24];
    snprintf(l, sizeof(l), lv[k] <= 1 ? "처음" : "Lv.%u", lv[k]);
    gfx->setTextColor(future ? 0x8410 : UI_INK);
    setSize(1);
    if (tab) {  // "기술머신01": 위에 "기술머신", 아래 번호 (한글 4자 = 12바이트)
      char head[16];
      memcpy(head, TM_NAME[tms[k]], 12);
      head[12] = 0;
      setCur(66, y + 3);
      printT(head);
      setCur(66, y + 20);
      printT(TM_NAME[tms[k]] + 12);
    } else {
      setCur(66, y + 11);
      printT(l);
    }
    gfx->fillRoundRect(140, y + 6, 8, 28, 4, mv2Col(MV2[id].type));
    drawFitIn(MV2_KO[id], 156, y + 7, 150, future ? 0x8410 : UI_INK, 2);
    mv2ClassIcon(330, y + 20, MV2[id].cls, 7);
    char s[12];
    if (MV2[id].pow) snprintf(s, sizeof(s), "%u", MV2[id].pow); else strcpy(s, "-");
    setCur(346, y + 11);
    gfx->setTextColor(0x8410);
    printT(s);
    if (has) { gfx->drawCircle(392, y + 20, 9, C565(0xe0, 0x40, 0x38)); gfx->drawCircle(392, y + 20, 8, C565(0xe0, 0x40, 0x38)); }
  }
  snprintf(t, sizeof(t), "%u/%u", page + 1, pages);
  drawBtn(118, 382, 64, 38, page ? UI_WHITE : UI_TRACK, UI_INK, "<");
  drawBtn(284, 382, 64, 38, page + 1 < pages ? UI_WHITE : UI_TRACK, UI_INK, ">");
  drawFit(t, 392, 90, UI_INK, 2);
  drawFit("○ 배운 기술, 회색 = 레벨 부족", 432, 300, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
}

// ---------------------------------------------------------------- 기술머신 가방
#define TB_ROWS 5
void renderTmBagV2(const uint8_t *have, uint8_t page, int16_t petDex) {
  uiScreenBg();
  uint8_t list[TM_N], n = 0;
  for (uint8_t i = 0; i < TM_N; i++) if (have[i]) list[n++] = i;
  char t[40];
  drawFit("기술머신", 34, 220, UI_INK, 2);
  snprintf(t, sizeof(t), "%u종류, 밝은 것 = 배울 수 있음", n);
  drawFit(t, 64, 330, 0x8410, 1);
  uint8_t pages = n ? (n + TB_ROWS - 1) / TB_ROWS : 1;
  for (uint8_t r = 0; r < TB_ROWS; r++) {
    uint8_t k = page * TB_ROWS + r;
    if (k >= n) break;
    uint8_t tm = list[k], id = TM_MOVE[tm];
    bool can = mv2TmCan(petDex, tm);
    int y = 96 + r * 54;
    uint16_t c = mv2Col(MV2[id].type);
    uiButton(60, y, 346, 48, 14, can ? UI_WHITE : C565(0xe4, 0xe0, 0xd8), UI_INK);
    // 디스크 그림
    gfx->fillCircle(92, y + 24, 17, can ? c : C565(0xb0, 0xac, 0xa4));
    gfx->drawCircle(92, y + 24, 17, UI_INK);
    gfx->fillCircle(92, y + 24, 5, UI_WHITE);
    gfx->drawCircle(92, y + 24, 5, UI_INK);
    gfx->setTextColor(can ? 0x8410 : C565(0xa0, 0x9c, 0x94));
    setSize(1);
    setCur(118, y + 4);
    printT(TM_NAME[tm]);
    drawFitIn(MV2_KO[id], 118, y + 20, 170, can ? UI_INK : C565(0xa0, 0x9c, 0x94), 2);
    char q[8];
    snprintf(q, sizeof(q), "x%u", have[tm]);
    setCur(380 - textW(q, 1), y + 6);
    printT(q);
    if (can) mv2ClassIcon(372, y + 32, MV2[id].cls, 7);
  }
  snprintf(t, sizeof(t), "%u/%u", page + 1, pages);
  drawBtn(118, 372, 64, 38, page ? UI_WHITE : UI_TRACK, UI_INK, "<");
  drawBtn(284, 372, 64, 38, page + 1 < pages ? UI_WHITE : UI_TRACK, UI_INK, ">");
  drawFit(t, 382, 90, UI_INK, 2);
  drawFit("누르면 설명 / 가르치기", 424, 260, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
}

// ---------------------------------------------------------------- ko12.10: 전설 / 희귀 표시 (모든 화면 공용)
// rk 2 = 전설 (금색), 1 = 희귀 (보라색)
uint16_t rarityInk(uint8_t rk) { return rk == 2 ? C565(0xd8, 0x98, 0x10) : C565(0x8a, 0x5c, 0xe8); }
void rarityTag(int cx, int cy, uint8_t rk) {
  if (!rk) return;
  const char *t = rk == 2 ? "전설" : "희귀";
  int w = textW(t, 1) + 30;
  uiButton(cx - w / 2, cy - 11, w, 22, 11, rk == 2 ? C565(0xff, 0xd8, 0x50) : C565(0xc8, 0xa8, 0xff), rarityInk(rk));
  int sx = cx - w / 2 + 11, sy = cy;
  uint16_t c = rk == 2 ? C565(0xb0, 0x60, 0x00) : C565(0x5a, 0x30, 0xb0);
  gfx->fillTriangle(sx, sy - 6, sx - 2, sy, sx + 2, sy, c);
  gfx->fillTriangle(sx, sy + 6, sx - 2, sy, sx + 2, sy, c);
  gfx->fillTriangle(sx - 6, sy, sx, sy - 2, sx, sy + 2, c);
  gfx->fillTriangle(sx + 6, sy, sx, sy - 2, sx, sy + 2, c);
  drawFitIn(t, cx - w / 2 + 18, cy - 10, w - 22, c, 1);
}
// 뒤쪽 빛: 천천히 도는 빛살 + 숨 쉬는 원
void rarityBackGlow(int cx, int cy, uint8_t rk, int r) {
  if (!rk) return;
  uint32_t t = millis();
  uint16_t c = rk == 2 ? C565(0xf4, 0xc4, 0x30) : C565(0xb4, 0x90, 0xf8);
  float pl = 0.5f + 0.5f * sinf(t * 0.004f);
  int nr = rk == 2 ? 12 : 8;
  for (int k = 0; k < nr; k++) {
    float a = t * 0.0006f + k * 6.2832f / nr;
    gfx->drawLine(cx + (int)(cosf(a) * r * 0.7f), cy + (int)(sinf(a) * r * 0.7f), cx + (int)(cosf(a) * (r + 6 * pl)),
                  cy + (int)(sinf(a) * (r + 6 * pl)), uiLerp(c, UI_BG_DAY, 7, 16));
  }
  gfx->fillCircle(cx, cy, r * 7 / 10 + (int)(pl * 3), uiLerp(c, UI_BG_DAY, 12, 16));
}
// 목록 줄 테두리: 금색/보라색 두 줄 + 오른쪽 위 반짝
void rarityRowFrame(int x, int y, int w, int h, uint8_t rk) {
  if (!rk) return;
  uint16_t c = rarityInk(rk), cl = rk == 2 ? C565(0xff, 0xe0, 0x70) : C565(0xd8, 0xc4, 0xff);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 9, c);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 8, cl);
  uint32_t t = millis();
  int l = 2 + (int)((t / 120) % 4);
  gfx->drawFastHLine(x + w - 14 - l, y + 8, 2 * l + 1, c);
  gfx->drawFastVLine(x + w - 14, y + 8 - l, 2 * l + 1, c);
}

// ---------------------------------------------------------------- ko12.10: 새 기술 이펙트 (종류별)
// 원작 기술 중 속성 이펙트가 어울리지 않는 것: 펀치 · 킥 · 할퀴기 · 물기 · 몸통 · 소리 · 가루 ·
// 능력 올리기 / 내리기 · 회복 · 방어막 · 흡수. t = 0..FX2_MS, (x0,y0) 쓰는 쪽, (x1,y1) 맞는 쪽, c = 속성 색
enum : uint8_t { FX2_PUNCH = 0, FX2_KICK, FX2_SLASH, FX2_BITE, FX2_BODY, FX2_SOUND, FX2_POWDER, FX2_STATUP,
                 FX2_STATDOWN, FX2_HEAL, FX2_BARRIER, FX2_DRAIN, FX2_N };
#define FX2_MS 900
static void fx2Star(int x, int y, int r, uint16_t c, uint16_t in) {  // 충격 별
  for (int k = 0; k < 8; k++) {
    float a = k * 0.785f + 0.2f;
    int ro = (k & 1) ? r * 6 / 10 : r;
    gfx->fillTriangle(x, y, x + (int)(cosf(a - 0.3f) * r / 3), y + (int)(sinf(a - 0.3f) * r / 3),
                      x + (int)(cosf(a) * ro), y + (int)(sinf(a) * ro), c);
    gfx->fillTriangle(x, y, x + (int)(cosf(a + 0.3f) * r / 3), y + (int)(sinf(a + 0.3f) * r / 3),
                      x + (int)(cosf(a) * ro), y + (int)(sinf(a) * ro), c);
  }
  gfx->fillCircle(x, y, r / 3, in);
}
static void fx2Fist(int x, int y, int s, uint16_t c) {
  gfx->fillRoundRect(x - 6 * s, y - 5 * s, 12 * s, 10 * s, 3 * s, c);
  gfx->drawRoundRect(x - 6 * s, y - 5 * s, 12 * s, 10 * s, 3 * s, UI_INK);
  for (int k = 1; k < 4; k++) gfx->drawFastVLine(x - 6 * s + k * 3 * s, y - 5 * s, 4 * s, UI_INK);
  gfx->fillRoundRect(x - 8 * s, y - 2 * s, 4 * s, 6 * s, s, c);
  gfx->drawRoundRect(x - 8 * s, y - 2 * s, 4 * s, 6 * s, s, UI_INK);
}
void drawMoveFx2(uint8_t fam, int x0, int y0, int x1, int y1, uint32_t t, uint16_t c) {
  float p = t >= FX2_MS ? 1.0f : (float)t / FX2_MS;
  uint16_t w = UI_WHITE, light = lerp565(c, UI_WHITE, 9, 16);
  switch (fam) {
    case FX2_PUNCH: {  // 주먹이 커지며 날아가 쾅
      if (p < 0.55f) {
        float q = p / 0.55f;
        int x = x0 + (int)((x1 - x0) * q), y = y0 + (int)((y1 - y0) * q);
        for (int k = 1; k <= 3; k++) gfx->drawLine(x - k * 10, y + k * 6, x - k * 10 - 12, y + k * 6 + 7, light);
        fx2Fist(x, y, 2 + (int)(q * 2), c);
      } else {
        float q = (p - 0.55f) / 0.45f;
        fx2Star(x1, y1, 22 + (int)(q * 26), q < 0.5f ? w : light, c);
        fx2Fist(x1 - 6, y1 + 4, 4, c);
      }
      break;
    }
    case FX2_KICK: {  // 발이 호를 그리며 차고 별
      float a = -1.6f + p * 2.2f;
      int fx = x1 - 30 + (int)(cosf(a) * 46), fy = y1 + 30 + (int)(sinf(a) * 46);
      for (int k = 1; k < 5; k++) {
        float b = a - k * 0.18f;
        gfx->fillCircle(x1 - 30 + (int)(cosf(b) * 46), y1 + 30 + (int)(sinf(b) * 46), 6 - k, lerp565(c, UI_BG_DAY, k * 3, 16));
      }
      gfx->fillRoundRect(fx - 10, fy - 7, 22, 12, 6, c);
      gfx->drawRoundRect(fx - 10, fy - 7, 22, 12, 6, UI_INK);
      if (p > 0.6f) fx2Star(x1 + 8, y1 - 4, 18 + (int)((p - 0.6f) * 50), w, c);
      break;
    }
    case FX2_SLASH: {  // 세 줄 할퀴기 (위에서 아래로 그어짐)
      for (int k = 0; k < 3; k++) {
        float q = (p - k * 0.12f) / 0.5f;
        if (q <= 0) continue;
        if (q > 1) q = 1;
        int sx = x1 - 34 + k * 22, sy = y1 - 40;
        int ex = sx + (int)(36 * q), ey = sy + (int)(80 * q);
        for (int d = -2; d <= 2; d++) gfx->drawLine(sx + d, sy, ex + d, ey, d ? light : w);
      }
      if (p > 0.6f) for (int k = 0; k < 6; k++) {
        float a = k * 1.05f;
        int r = (int)((p - 0.6f) * 90);
        gfx->fillCircle(x1 + (int)(cosf(a) * r), y1 + (int)(sinf(a) * r), 2, c);
      }
      break;
    }
    case FX2_BITE: {  // 위아래 이빨이 닫힘
      float q = p < 0.6f ? p / 0.6f : 1.0f;
      int gap = (int)(40 * (1 - q)) + 2;
      for (int k = 0; k < 5; k++) {
        int tx = x1 - 40 + k * 20;
        gfx->fillTriangle(tx, y1 - gap - 22, tx + 18, y1 - gap - 22, tx + 9, y1 - gap, w);
        gfx->drawTriangle(tx, y1 - gap - 22, tx + 18, y1 - gap - 22, tx + 9, y1 - gap, UI_INK);
        gfx->fillTriangle(tx, y1 + gap + 22, tx + 18, y1 + gap + 22, tx + 9, y1 + gap, w);
        gfx->drawTriangle(tx, y1 + gap + 22, tx + 18, y1 + gap + 22, tx + 9, y1 + gap, UI_INK);
      }
      if (p > 0.6f) fx2Star(x1, y1, 14 + (int)((p - 0.6f) * 40), c, w);
      break;
    }
    case FX2_BODY: {  // 몸통 돌진: 속도선 + 큰 충격 원
      for (int k = 0; k < 7; k++) {
        int ly = y0 - 40 + k * 12, l = 30 + (k * 37) % 40;
        int lx = x0 + (int)((x1 - x0) * p) - l - 20;
        gfx->drawFastHLine(lx, ly + (int)((y1 - y0) * p), l, light);
      }
      if (p > 0.5f) {
        float q = (p - 0.5f) / 0.5f;
        gfx->drawCircle(x1, y1, 20 + (int)(q * 40), w);
        gfx->drawCircle(x1, y1, 21 + (int)(q * 40), c);
        fx2Star(x1, y1, 26, w, c);
      }
      break;
    }
    case FX2_SOUND: {  // 쓰는 쪽에서 퍼지는 소리 고리 + 음표
      for (int k = 0; k < 4; k++) {
        float q = p * 1.4f - k * 0.18f;
        if (q <= 0 || q > 1) continue;
        int x = x0 + (int)((x1 - x0) * q), y = y0 + (int)((y1 - y0) * q), r = 10 + (int)(q * 30);
        gfx->drawCircle(x, y, r, c);
        gfx->drawCircle(x, y, r - 1, light);
      }
      for (int k = 0; k < 3; k++) {
        float q = p - k * 0.15f;
        if (q <= 0) continue;
        int nx = x0 + 20 + (int)((x1 - x0) * q * 0.8f) + k * 14, ny = y0 - 30 - (int)(q * 60) + (int)(6 * sinf(q * 12));
        gfx->fillCircle(nx, ny, 5, c);
        gfx->fillRect(nx + 3, ny - 16, 3, 16, c);
      }
      break;
    }
    case FX2_POWDER: {  // 맞는 쪽 위에서 가루가 내려앉음
      for (int k = 0; k < 26; k++) {
        uint32_t h = k * 2654435761u;
        int sx = x1 - 50 + (int)(h % 100), sy0 = y1 - 70 - (int)((h >> 8) % 30);
        int sy = sy0 + (int)(p * 90) + (int)(4 * sinf(p * 10 + k));
        if (sy > y1 + 30) continue;
        gfx->fillCircle(sx, sy, 2 + (k & 1), (k % 3) ? c : light);
      }
      break;
    }
    case FX2_STATUP:
    case FX2_STATDOWN: {  // 올리기 = 쓰는 쪽에 위로 화살표 + 빛, 내리기 = 맞는 쪽에 아래로
      bool up = fam == FX2_STATUP;
      int cx = up ? x0 : x1, cy = up ? y0 : y1;
      uint16_t ac = up ? C565(0xf0, 0x60, 0x40) : C565(0x40, 0x80, 0xf0);
      for (int k = 0; k < 5; k++) {
        float q = fmodf(p * 2 + k * 0.2f, 1.0f);
        int ax = cx - 40 + k * 20, ay = up ? cy + 40 - (int)(q * 90) : cy - 50 + (int)(q * 90);
        int d = up ? -1 : 1;
        gfx->fillTriangle(ax - 8, ay, ax + 8, ay, ax, ay + d * 10, ac);
        gfx->fillRect(ax - 3, ay - d * 10 - (d < 0 ? 0 : 0), 6, 10, ac);
      }
      gfx->drawEllipse(cx, cy + 40, 44 + (int)(p * 8), 10, ac);
      break;
    }
    case FX2_HEAL: {  // 초록 빛 기둥 + 위로 오르는 십자
      uint16_t g = C565(0x60, 0xd8, 0x80);
      int r = 30 + (int)(6 * sinf(p * 12));
      gfx->drawEllipse(x0, y0 + 40, r + 14, 10, g);
      for (int k = 0; k < 6; k++) {
        float q = fmodf(p * 1.5f + k * 0.17f, 1.0f);
        int sx = x0 - 40 + (k * 31) % 80, sy = y0 + 30 - (int)(q * 90);
        gfx->fillRect(sx - 6, sy - 2, 13, 5, g);
        gfx->fillRect(sx - 2, sy - 6, 5, 13, g);
      }
      break;
    }
    case FX2_BARRIER: {  // 쓰는 쪽 앞에 육각 방어막
      int cx = x0 + 34, cy = y0 - 10, R = 30 + (int)(p * 14);
      for (int k = 0; k < 6; k++) {
        float a = k * 1.047f, b = a + 1.047f;
        gfx->drawLine(cx + (int)(cosf(a) * R), cy + (int)(sinf(a) * R * 1.3f), cx + (int)(cosf(b) * R),
                      cy + (int)(sinf(b) * R * 1.3f), c);
        gfx->drawLine(cx + (int)(cosf(a) * (R - 3)), cy + (int)(sinf(a) * (R - 3) * 1.3f), cx + (int)(cosf(b) * (R - 3)),
                      cy + (int)(sinf(b) * (R - 3) * 1.3f), light);
      }
      for (int k = 0; k < 3; k++) {
        int sy = cy - R + (int)((p * 3 + k) * R * 2 / 3) % (R * 2);
        gfx->drawFastHLine(cx - R / 2, sy, R, light);
      }
      break;
    }
    case FX2_DRAIN: {  // 맞는 쪽에서 빛 구슬이 쓰는 쪽으로 빨려 옴
      uint16_t g = C565(0x80, 0xe0, 0x60);
      for (int k = 0; k < 7; k++) {
        float q = fmodf(p * 1.3f + k * 0.14f, 1.0f);
        float bend = sinf(q * 3.14f) * (k % 2 ? 30 : -30);
        int x = x1 + (int)((x0 - x1) * q), y = y1 + (int)((y0 - y1) * q + bend);
        gfx->fillCircle(x, y, 6 - (int)(q * 3), g);
        gfx->drawCircle(x, y, 7 - (int)(q * 3), w);
      }
      break;
    }
  }
}

// ui_more.ino - fork KO (ko4): bogwanham (caja) y ajustes de sonido
// (se concatena tras ui_extra.ino, que tiene el despachador de xScreen)

// ======================================================================
// Caja: Pokemon ganados o capturados en batalla
// ======================================================================

#define BOX_ROWS 5
#define BOX_ROW_Y 92
#define BOX_ROW_H 46
#define BOX_ROW_GAP 4
#define BOX_NAV_Y 352
// ko11.8: pestanas un poco mas abajo y estrechas (antes y=34, 150 de ancho: en la
// pantalla redonda se cortaban las esquinas)
// ko11.16: tres pestanas [caja] [corona] [orbe], algo mas abajo (y=48: la pantalla
// redonda deja 92..374 de ancho arriba)
#define BOX_TAB_Y 48      // ko10.5: pestanas [caja] [corona]
#define BOX_TAB_H 38
#define BOX_TAB_W 104
#define BOX_TAB_X1 80
#define BOX_TAB_X2 188
#define BOX_TAB_X3 296    // ko11.16: bolsa de orbes (solo el icono)
#define BOX_TAB_W3 42
#define BOX_TAB_X4 342    // ko11.26: bolsa de caramelos (abre su pantalla)
uint8_t boxPage = 0;
int16_t boxSel = -1;          // indice en la caja con la ficha abierta, -1 = lista
bool boxHall = false;         // ko10.5: pestana del salon de la fama (corona)
bool boxOrb = false;          // ko11.16: pestana de la bolsa de orbes
int8_t orbSel = -1;           // orbe con la ventana abierta (indice de la vista), -1 = ninguno
uint8_t orbPage = 0;
static const char *orbMsg = nullptr;
static uint32_t orbMsgUntil = 0;
// ko11.19: fusion (3 orbes de la bolsa -> 1 del tipo del Pokemon que crias)
#define SYN_BTN_Y 364
#define SYN_ANIM_MS 1500
static bool synMode = false;
static uint8_t synPick[3], synN = 0;   // posiciones en pet.orbBag
static uint16_t synMat[3], synOut = 0;
static uint8_t synRes = 0;
static uint32_t synAnimT = 0;          // animacion en curso (0 = no)
static bool synShow = false;           // ventana del resultado
static bool orbSel_open() { return orbSel >= 0; }
static bool synPicked(uint8_t bi) {
  for (uint8_t i = 0; i < synN; i++) if (synPick[i] == bi) return true;
  return false;
}
static void synReset() { synMode = false; synN = 0; synAnimT = 0; synShow = false; }

static Box &cb() { return boxHall ? hall : box; }

// ko10.4: la lista se ve ordenada por numero de pokedex (los repetidos quedan
// juntos; entre iguales, el mas antiguo primero). Solo cambia el orden de la
// vista: la caja guardada no se toca. boxView(k) = indice real del k-esimo
static uint8_t boxOrd[BOX_CAP_MAX];
static void boxSortView(Box &b) {
  uint8_t n = b.count();
  for (uint8_t i = 0; i < n; i++) boxOrd[i] = i;
  for (uint8_t i = 1; i < n; i++) {  // insercion (estable)
    uint8_t v = boxOrd[i];
    int j = i - 1;
    while (j >= 0 && b.at(boxOrd[j]).dex > b.at(v).dex) { boxOrd[j + 1] = boxOrd[j]; j--; }
    boxOrd[j + 1] = v;
  }
}
static uint8_t boxView(int k) { return boxOrd[k]; }
uint32_t boxConfirmUntil = 0; // segundo toque en "soltar" para confirmar

// ---- ko11.7: expediciones (ver expeditionReward en battle.cpp) ----
static const uint8_t EXP_HOURS[3] = { 2, 4, 8 };
#define EXP_STRIP_Y 392
bool expPick = false;          // eligiendo cuantas horas
bool expResOpen = false;       // ventana con lo que trajo
static ExpReward expRes;
static int16_t expResDex = 0, expNewDex = 0;
static uint8_t expGotBalls = 0, expGotPotions = 0;

static void expSend(uint8_t idx, uint8_t hours) {
  BoxMon m;
  if (pet.exped.on || !box.take(idx, m)) return;
  uint32_t now = clockEpoch();
  pet.exped.on = 1;
  pet.exped.hours = hours;
  pet.exped.dex = m.dex;
  pet.exped.lvl = m.lvl;
  pet.exped.flags = m.flags;
  pet.exped.gA = m.geneAtk;
  pet.exped.gD = m.geneDef;
  pet.exped.gS = m.geneSpe;
  pet.exped.epoch = m.epoch;
  pet.exped.start = now;
  pet.exped.end = now + (uint32_t)hours * 3600u;
  pet.saveNow();
  expPick = false;
  boxSel = -1;
  char t[64];
  snprintf(t, sizeof(t), XT(X_EXP_LEFT_FMT), dexName(m.dex));
  showToast(t);
  sfxPlay(SFX_PLAY);
}

static void expCollect() {
  if (!pet.exped.on) return;
  if (box.full()) { showToast(XT(X_EXP_FULL)); sfxPlay(SFX_DENY); return; }
  BoxMon m;
  memset(&m, 0, sizeof(m));
  m.dex = pet.exped.dex;
  m.lvl = pet.exped.lvl;
  m.flags = pet.exped.flags;
  m.geneAtk = pet.exped.gA;
  m.geneDef = pet.exped.gD;
  m.geneSpe = pet.exped.gS;
  m.epoch = pet.exped.epoch;
  box.put(m);
  BRng rng((uint32_t)random(0x7fffffff) ^ clockEpoch());
  expRes = expeditionReward(pet.exped.hours, pet.exped.lvl, rng);
  expResDex = m.dex;
  pet.addCandy(m.dex, expRes.candy);
  uint8_t b0 = pet.balls, p0 = pet.potions;
  pet.giveItems(expRes.balls, expRes.potions);
  expGotBalls = pet.balls - b0;
  expGotPotions = pet.potions - p0;
  if (expRes.rare && pet.rareCandy < 999) pet.rareCandy++;
  // ko11.16: a veces trae un orbe de su tipo (2 h 10 %, 4 h 20 %, 8 h 35 %)
  {
    uint8_t h = pet.exped.hours, pc = h >= 8 ? 35 : h >= 4 ? 20 : 10;
    if ((uint32_t)random(100) < pc) orbDrop(DEX_TBL[m.dex].ptype);
  }
  expNewDex = 0;
  if (expRes.newMon && !box.full()) {  // un Pokemon de su region se viene con el
    Battler w = makeWildIn(DEX_TBL[m.dex].biome, m.lvl, (uint8_t)sceneHour(), 0, 0, rng, nullptr);
    if (box.add(w.dex, w.lvl, false, true, clockEpoch(), w.mv)) {
      expNewDex = w.dex;
      dexLog.caught(w.dex, clockEpoch());
      dexRecordMoves(w.dex, w.mv, true);  // ko11.31.3
    }
  }
  memset(&pet.exped, 0, sizeof(pet.exped));
  pet.saveNow();
  expResOpen = true;
  sfxPlay(SFX_MEDAL);
}

static void drawExpResult() {
  uiPanel(48, 110, 370, 240, 22, UI_WHITE, UI_INK);
  char l[64];
  snprintf(l, sizeof(l), XT(X_EXP_RESULT_FMT), dexName(expResDex));
  drawFit(l, 132, 330, UI_INK, 2);
  int y = 172;
  snprintf(l, sizeof(l), XT(X_EXP_CANDY_FMT), dexName(expResDex), expRes.candy);
  drawFit(l, y, 330, UI_INK, 2); y += 32;
  if (expGotBalls || expGotPotions) {
    snprintf(l, sizeof(l), XT(X_EXP_ITEMS_FMT), expGotBalls, expGotPotions);
    drawFit(l, y, 330, UI_INK, 2); y += 32;
  }
  if (expRes.rare) { drawFit(XT(X_EXP_RARE), y, 330, UI_BAR_OK, 2); y += 32; }
  if (expNewDex) {
    snprintf(l, sizeof(l), XT(X_EXP_NEW_FMT), dexName(expNewDex));
    drawFit(l, y, 330, C565(0xc0, 0x40, 0x90), 2); y += 32;
  }
  drawFit(XT(X_EXP_TAP_CLOSE), 322, 200, 0x8410, 1);
}

// en loop(): aviso en la pantalla principal cuando vuelve (una vez)
void expLoop() {
  // ko11.19: apagada toda la noche: al volver, "잘 잤어요!" (una vez, en la principal)
  if (pet.sleptOffline && !extraOpen() && !screenOff && !pet.isEgg() && !cardOpen && !clockOpen) {
    char t[64];
    txFmt(t, sizeof(t), X_SLEPT_WELL, pet.nick[0] ? pet.nick : dexName(pet.speciesId), nullptr);
    showToast(t);
    sfxPlay(SFX_HEART);
    pet.sleptOffline = 0;
  }
  // ko11.16: tras la animacion de evolucion, el aviso del orbe que se volvio caramelos
  if (pet.orbEvoNote && !pet.evolving() && !extraOpen() && !screenOff) {
    showToast(XT(pet.orbEvoNote == 4 ? X_ORB_EVO_DUAL_RARE : pet.orbEvoNote == 3 ? X_ORB_EVO_DUAL : pet.orbEvoNote == 2 ? X_ORB_EVO_RARE : X_ORB_EVO_CANDY));
    pet.orbEvoNote = 0;
  }
  static int16_t told = 0;
  if (!pet.exped.on) { told = 0; return; }
  if (told == pet.exped.dex) return;
  if (clockEpoch() >= pet.exped.end && !extraOpen() && !galleryOpen && !cardOpen && !clockOpen && !screenOff) {
    char t[64];
    snprintf(t, sizeof(t), XT(X_EXP_TOAST_FMT), dexName(pet.exped.dex));
    showToast(t);
    sfxPlay(SFX_MEDAL);
    told = pet.exped.dex;
  }
}

// ko11.17: los criados hasta el final llevan una ESCARAPELA (lazo de premio), no
// corona: la corona queda para los campeones de la liga (salon de la fama).
// (cx, cy) = centro del medallon, r = su radio
static void drawRibbon(int cx, int cy, int r, bool light = false) {
  uint16_t blue = C565(0x3a, 0x7a, 0xe0), blueD = C565(0x22, 0x4c, 0x9a), gold = C565(0xf0, 0xc0, 0x30);
  if (light) { blue = UI_WHITE; blueD = C565(0xd0, 0xd8, 0xe8); }
  int tl = r + r / 2, tw = r * 2 / 3;
  // dos cintas que cuelgan (con el corte en V)
  gfx->fillTriangle(cx - tw, cy, cx - tw / 3, cy, cx - tw - r / 3, cy + tl, blueD);
  gfx->fillTriangle(cx - tw / 3, cy, cx - tw - r / 3, cy + tl, cx - r / 4, cy + tl - r / 3, blueD);
  gfx->fillTriangle(cx + tw, cy, cx + tw / 3, cy, cx + tw + r / 3, cy + tl, blue);
  gfx->fillTriangle(cx + tw / 3, cy, cx + tw + r / 3, cy + tl, cx + r / 4, cy + tl - r / 3, blue);
  // roseta: petalos alrededor + medallon dorado
  for (int k = 0; k < 10; k++) {
    float a = k * 0.6283f;
    gfx->fillCircle(cx + (int)(cosf(a) * r * 0.8f), cy + (int)(sinf(a) * r * 0.8f), r / 3 + 1, blue);
  }
  gfx->fillCircle(cx, cy, r * 3 / 4, gold);
  gfx->drawCircle(cx, cy, r * 3 / 4, C565(0xa0, 0x70, 0x10));
  gfx->fillCircle(cx - r / 5, cy - r / 5, r / 5 + 1, C565(0xff, 0xec, 0xa0));
}

static uint8_t boxPages() { return cb().count() ? (cb().count() + BOX_ROWS - 1) / BOX_ROWS : 1; }

void openBox() {
  retMark();
  cardOpen = false;
  boxHall = false;
  boxOrb = false;
  orbSel = -1;
  orbPage = 0;
  boxPage = 0;
  boxSel = -1;
  boxConfirmUntil = 0;
  xScreen = XS_BOX;
}

// miniatura centrada en (cx, cy), a escala s
void drawThumbAt(int16_t dex, int cx, int cy, int s, bool sil) {
  const uint8_t *b = thumbs.get(dex);
  if (!b) {  // sin thumbs.bin: un circulo con el color de la especie
    gfx->fillCircle(cx, cy, 8 * s, sil ? INK_K : DEX_TBL[dex].accent);
    return;
  }
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = cx - w * s / 2, oy = cy - h * s / 2;
  for (int r = 0; r < h; r++)
    for (int c = 0; c < w; c++) {
      uint8_t idx = d[r * w + c];
      if (idx == 0xFF) continue;
      uint16_t col = sil ? INK_K : (uint16_t)(pal[idx * 2] | (pal[idx * 2 + 1] << 8));
      gfx->fillRect(ox + c * s, oy + r * s, s, s, col);
    }
}

static void boxDate(uint32_t e, char *out, size_t n) {
  if (!e) { out[0] = 0; return; }
  int32_t z = (int32_t)(e / 86400) + 719468;  // civil_from_days
  int32_t era = z / 146097, doe = z - era * 146097;
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  unsigned d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
  snprintf(out, n, "%02u/%02u", m, d);
}

// ko12.4: recuerdos de un criado (ficha de la cinta): 2 paginas, tocar = la otra
int16_t memSel = -1;  // ficha de la cinta con los recuerdos abiertos (-1 = ninguna)
uint8_t memPage = 0;
extern MemStore memStore;
static void memDate(uint32_t e, char *out, size_t n) {
  if (!e || e < 86400UL * 365) { snprintf(out, n, "-"); return; }
  int32_t z = (int32_t)(e / 86400) + 719468;  // civil_from_days
  int32_t era = z / 146097, doe = z - era * 146097;
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  unsigned d = doy - (153 * mp + 2) / 5 + 1, mo = mp < 10 ? mp + 3 : mp - 9;
  unsigned y = (unsigned)(yoe + era * 400 + (mo <= 2 ? 1 : 0));
  snprintf(out, n, "%04u.%02u.%02u", y, mo, d);
}
static void renderMemory(const BoxMon &bm) {
  uiScreenBg();
  MemRec r;
  bool have = memStore.get(bm.epoch, bm.dex, r);
  char l[96], d1[16], d2[16];
  const char *nm = have && r.nick[0] ? r.nick : dexName(bm.dex);
  snprintf(l, sizeof(l), XT(X_MEM_TITLE_FMT), nm);
  drawFit(l, 40, 300, DEX_TBL[bm.dex].accent, 2);
  drawRibbon(CX + 128, 50, 10);
  if (!have) {
    drawThumbAt(bm.dex, CX, 150, 3, false);
    drawFit(XT(X_MEM_NONE), 230, 340, UI_INK, 2);
    drawFit(XT(X_MEM_NONE2), 260, 340, 0x8410, 1);
    memDate(bm.epoch, d1, sizeof(d1));
    snprintf(l, sizeof(l), XT(X_MEM_END_FMT), (unsigned)bm.lvl, d1);
    drawFit(l, 292, 340, UI_INK, 1);
    drawNav(NAV_L, UI_INK);
    uiFlush();
    return;
  }
  int y = 74;
  const int LH = 26;
  if (memPage == 0) {  // la historia
    // las formas por las que paso, de pequena a grande
    int16_t forms[3] = { r.life.firstDex, r.life.evoDex[0], r.life.evoDex[1] };
    int nf = 0;
    for (int i = 0; i < 3; i++) if (forms[i] >= 1 && forms[i] <= DEX_COUNT) forms[nf++] = forms[i];
    if (!nf) { forms[0] = bm.dex; nf = 1; }
    for (int i = 0; i < nf; i++) {
      int cx = CX + (i - (nf - 1) / 2.0f) * 96;
      drawThumbAt(forms[i], cx, y + 30, 2, false);
      if (i + 1 < nf) { gfx->fillTriangle(cx + 40, y + 24, cx + 40, y + 36, cx + 50, y + 30, 0x8410); }
    }
    y += 72;
    memDate(r.life.start, d1, sizeof(d1));
    memDate(r.end, d2, sizeof(d2));
    snprintf(l, sizeof(l), XT(X_MEM_DAYS_FMT), (unsigned)r.days);
    drawFit(l, y, 340, UI_INK, 2); y += LH + 2;
    snprintf(l, sizeof(l), "%s ~ %s", d1, d2);
    drawFit(l, y, 340, 0x8410, 1); y += LH - 4;
    static const XId FROM[4] = { X_MEM_FROM_EGG, X_MEM_FROM_BOX, X_MEM_FROM_TRADE, X_MEM_FROM_UPD };
    drawFit(XT(FROM[r.life.from < 4 ? r.life.from : 0]), y, 340, UI_INK, 1); y += LH - 4;
    for (int i = 0; i < 2; i++) {
      int16_t e = r.life.evoDex[i];
      if (e < 1 || e > DEX_COUNT) continue;
      memDate(r.life.evoT[i], d1, sizeof(d1));
      snprintf(l, sizeof(l), XT(X_MEM_EVO_FMT), dexName(e), d1);
      drawFit(l, y, 340, UI_INK, 1); y += LH - 4;
    }
    if (r.life.firstWin) {
      memDate(r.life.firstWin, d1, sizeof(d1));
      snprintf(l, sizeof(l), XT(X_MEM_WIN_FMT), d1, (unsigned)r.wins);
    } else snprintf(l, sizeof(l), "%s", XT(X_MEM_NOWIN));
    drawFit(l, y, 340, UI_INK, 1); y += LH - 4;
    memDate(r.end, d1, sizeof(d1));
    snprintf(l, sizeof(l), XT(X_MEM_END_FMT), (unsigned)r.lvl, d1);
    drawFit(l, y, 340, UI_INK, 1); y += LH - 4;
    if (r.nick[0]) { snprintf(l, sizeof(l), XT(X_MEM_NICK_FMT), r.nick); drawFit(l, y, 340, 0x8410, 1); }
  } else {  // los numeros
    drawFit(XT(X_MEM_P2), y, 300, UI_INK, 2); y += LH + 8;
    snprintf(l, sizeof(l), XT(X_MEM_CARE_FMT), r.life.meals, r.life.snacks, r.life.cleans);
    drawFit(l, y, 360, UI_INK, 1); y += LH;
    snprintf(l, sizeof(l), XT(X_MEM_CARE2_FMT), r.life.pets, r.life.plays, r.life.trains);
    drawFit(l, y, 360, UI_INK, 1); y += LH + 6;
    snprintf(l, sizeof(l), XT(X_MEM_WIN_FMT), "", (unsigned)r.wins);
    {  // solo "야생 승리 N번" (sin la fecha): lo que va despues de los espacios
      const char *p = strstr(l, "   ");
      drawFit(p ? p + 3 : l, y, 360, UI_INK, 1); y += LH;
    }
    snprintf(l, sizeof(l), XT(X_MEM_BATTLE_FMT), r.link, r.daily);
    drawFit(l, y, 360, UI_INK, 1); y += LH;
    snprintf(l, sizeof(l), XT(X_MEM_LEAGUE_FMT), r.badges, r.champ);
    drawFit(l, y, 360, UI_INK, 1); y += LH + 6;
    snprintf(l, sizeof(l), XT(X_MEM_BOND_FMT), r.bond, r.mistakes, r.medals);
    drawFit(l, y, 360, UI_INK, 1); y += LH + 6;
    snprintf(l, sizeof(l), XT(X_MEM_REC_FMT), r.gameHi, r.strHi, r.defHi);
    drawFit(l, y, 360, 0x8410, 1); y += LH;
    snprintf(l, sizeof(l), XT(X_MEM_REC2_FMT), r.speHi, r.vbBest);
    drawFit(l, y, 360, 0x8410, 1);
  }
  snprintf(l, sizeof(l), "%u/2", memPage + 1u);
  drawFit(l, 392, 80, UI_INK, 1);
  drawFit(XT(X_MEM_TAP), 412, 220, 0x8410, 1);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void renderBoxDetail() {
  const BoxMon &m = cb().at(boxSel);
  if (boxHall && memSel == boxSel) { renderMemory(m); return; }  // ko12.4
  uiScreenBg();  // ko11.6.1: sin pasar por negro (parpadeo)
  char head[48];
  snprintf(head, sizeof(head), "%s%s", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex));
  drawFit(head, 44, 300, DEX_TBL[m.dex].accent, 3);
  bool perfect = m.flags & BOXF_PERFECT;
  if (perfect) {  // ko11.21: crianza perfecta: halo dorado que late, rayos que giran y destellos
    uint32_t t = millis();
    uint16_t gold = C565(0xf0, 0xc0, 0x30);
    float pl = 0.5f + 0.5f * sinf(t * 0.004f);
    for (int k = 0; k < 12; k++) {
      float a = t * 0.0008f + k * 0.5236f;
      int r0 = 50, r1 = 74 + (int)(pl * 8);
      gfx->drawLine(CX + (int)(cosf(a) * r0), 136 + (int)(sinf(a) * r0), CX + (int)(cosf(a) * r1),
                    136 + (int)(sinf(a) * r1), uiLerp(gold, UI_BG_DAY, 7, 16));
    }
    gfx->fillCircle(CX, 136, 58 + (int)(pl * 4), uiLerp(gold, UI_BG_DAY, 12, 16));
    gfx->fillCircle(CX, 136, 48, uiLerp(gold, UI_BG_DAY, 10, 16));
    int ph = (int)((t / 12) % 60);
    gfx->drawCircle(CX, 136, 48 + ph / 2, uiLerp(gold, UI_BG_DAY, 8 + ph / 8, 16));
  }
  drawThumbAt(m.dex, CX, 136, 3, false);
  if (perfect) {
    uint32_t t = millis();
    for (int k = 0; k < 6; k++) {
      float a = t * 0.0015f + k * 1.047f;
      int sx = CX + (int)(cosf(a) * 70), sy = 136 + (int)(sinf(a) * 54);
      int ln = 2 + (int)((t / 90 + k * 3) % 4);
      gfx->drawFastHLine(sx - ln, sy, 2 * ln + 1, UI_WHITE);
      gfx->drawFastVLine(sx, sy - ln, 2 * ln + 1, UI_WHITE);
    }
  }
  if (m.flags & BOXF_RAISED) drawRibbon(CX + 78, 104, 14);  // ko11.17: lo criaste hasta el final
  char l[48];
  snprintf(l, sizeof(l), "No.%03d  Lv.%u  %s", m.dex, m.lvl, typeName(DEX_TBL[m.dex].ptype));
  drawFit(l, 204, 340, UI_INK, 2);
  char date[8];
  boxDate(m.epoch, date, sizeof(date));
  snprintf(l, sizeof(l), "%s  %s", XT((m.flags & BOXF_RAISED) ? X_RAISED_TAG
                                      : (m.flags & BOXF_CAUGHT) ? X_CAUGHT_TAG : X_WON_TAG), date);
  drawFit(l, 232, 340, UI_INK, 2);
  if (perfect) {  // ko11.21
    const char *pt = XT(X_PERFECT_L);
    int w = textW(pt, 1) + 30;
    drawBtn(CX - w / 2, 252, w, 28, C565(0xe0, 0xa8, 0x20), UI_WHITE, pt);
  } else {
    drawFit(XT(boxHall ? X_HALL_NOTE : X_BOX_NEXT), 262, 360, UI_INK, 1);
  }
  bool conf = timeLeft(boxConfirmUntil) > 0;
  if (boxHall) {  // ko10.5: los de corona son recuerdos: no se sueltan. ko12.4: [추억] [닫기]
    drawBtn(93, 300, 130, 48, C565(0xe8, 0x80, 0xa8), UI_WHITE, XT(X_MEM_BTN));
    drawBtn(243, 300, 130, 48, UI_TRACK, UI_INK, XT(X_CLOSE));
  } else {  // ko11.7: [soltar] [explorar] [cerrar]
    drawBtn(73, 300, 100, 48, UI_BAR_BAD, UI_WHITE, XT(conf ? X_RELEASE_Q : X_RELEASE));
    bool away = pet.exped.on;
    drawBtn(183, 300, 100, 48, away ? UI_TRACK : C565(0x3a, 0x9a, 0x5a), away ? 0x8410 : UI_WHITE,
            XT(away ? X_EXP_BUSY : X_EXP_BTN));
    drawBtn(293, 300, 100, 48, UI_TRACK, UI_INK, XT(X_CLOSE));
    if (expPick) {  // elegir horas
      uiPanel(58, 120, 350, 200, 20, UI_WHITE, UI_INK);
      drawFit(XT(X_EXP_Q), 146, 320, UI_INK, 2);
      char hm[40];
      snprintf(hm, sizeof(hm), XT(X_EXP_HOME_FMT), XT((XId)(X_REG_0 + DEX_TBL[m.dex].biome)));
      drawFit(hm, 176, 320, 0x8410, 1);
      for (int i = 0; i < 3; i++) {
        char hb[12];
        snprintf(hb, sizeof(hb), XT(X_EXP_H_FMT), (unsigned)EXP_HOURS[i]);
        drawBtn(78 + i * 106, 204, 98, 48, C565(0x3a, 0x9a, 0x5a), UI_WHITE, hb);
      }
      drawBtn(153, 264, 160, 42, UI_TRACK, UI_INK, XT(X_BAK_CANCEL));
    }
  }
  if (!expPick) drawNav(NAV_L, UI_INK);  // ko11.8: <- volver a la lista
  uiFlush();
}

void renderOrbBag();
// ko11.26: caramelo envuelto (icono de la pestana de la bolsa de caramelos)
static void drawCandyIcon(int cx, int cy) {
  uint16_t pk = C565(0xf0, 0x7a, 0xa8), dk = lerp565(pk, UI_INK, 6, 16);
  gfx->fillTriangle(cx - 8, cy, cx - 16, cy - 7, cx - 16, cy + 7, pk);
  gfx->fillTriangle(cx + 8, cy, cx + 16, cy - 7, cx + 16, cy + 7, pk);
  gfx->drawTriangle(cx - 8, cy, cx - 16, cy - 7, cx - 16, cy + 7, dk);
  gfx->drawTriangle(cx + 8, cy, cx + 16, cy - 7, cx + 16, cy + 7, dk);
  gfx->fillCircle(cx, cy, 9, pk);
  gfx->drawCircle(cx, cy, 9, dk);
  gfx->drawLine(cx - 5, cy - 6, cx + 3, cy + 7, UI_WHITE);
}

static void drawBoxTabs(const char *t1, const char *t2) {
  bool box1 = !boxHall && !boxOrb;
  drawBtn(BOX_TAB_X1, BOX_TAB_Y, BOX_TAB_W, BOX_TAB_H, box1 ? UI_BAR_WARN : UI_TRACK, UI_INK, t1);
  drawBtn(BOX_TAB_X2, BOX_TAB_Y, BOX_TAB_W, BOX_TAB_H, boxHall ? C565(0xe8, 0xb0, 0x20) : UI_TRACK, UI_INK, t2);
  drawRibbon(BOX_TAB_X2 + 20, BOX_TAB_Y + 15, 8, boxHall);  // ko11.17: escarapela
  uiButton(BOX_TAB_X3, BOX_TAB_Y, BOX_TAB_W3, BOX_TAB_H, 12, boxOrb ? C565(0x6a, 0x4c, 0xf0) : UI_TRACK, UI_INK);
  drawOrb(BOX_TAB_X3 + BOX_TAB_W3 / 2, BOX_TAB_Y + BOX_TAB_H / 2 - 1, 9,
          orbValid(pet.orb) ? pet.orb : orbMake(PT_PSYCHIC, true, 10), millis());
  uiButton(BOX_TAB_X4, BOX_TAB_Y, BOX_TAB_W3, BOX_TAB_H, 12, UI_TRACK, UI_INK);
  drawCandyIcon(BOX_TAB_X4 + BOX_TAB_W3 / 2, BOX_TAB_Y + BOX_TAB_H / 2 - 1);
}

void renderBox() {
  if (boxOrb) { renderOrbBag(); return; }
  if (boxSel >= 0 && boxSel < cb().count()) { renderBoxDetail(); return; }
  boxSel = -1;
  uiScreenBg();  // ko11.6.1: sin pasar por negro (parpadeo)
  // ko10.5: dos pestanas: la caja y el salon de la fama (corona)
  char t1[24], t2[24];
  snprintf(t1, sizeof(t1), XT(X_BOX_TITLE_FMT), box.count(), BOX_MAX);
  snprintf(t2, sizeof(t2), XT(X_HALL_TAB_FMT), hall.count());
  drawBoxTabs(t1, t2);
  if (boxHall && !hall.count()) {
    drawFit(XT(X_HALL_EMPTY), 190, 330, UI_INK, 2);
  } else if (!boxHall && !box.count()) {
    drawFit(XT(X_BOX_EMPTY), 170, 300, UI_INK, 3);
    drawFit(XT(X_BOX_HINT), 220, 360, UI_INK, 2);
    drawFit(XT(X_BOX_NEXT), 248, 360, UI_INK, 2);
  }
  if (boxPage >= boxPages()) boxPage = boxPages() - 1;
  Box &b = cb();
  boxSortView(b);
  for (int r = 0; r < BOX_ROWS; r++) {
    int k = boxPage * BOX_ROWS + r;
    if (k >= b.count()) break;
    const BoxMon &m = b.at(boxView(k));
    int y = BOX_ROW_Y + r * (BOX_ROW_H + BOX_ROW_GAP);
    // ko10.4: repetidos: fondo de color por especie y "xN" (cuantos hay en la caja)
    uint8_t same = 0;
    for (uint8_t j = 0; j < b.count(); j++) if (b.at(j).dex == m.dex) same++;
    static const uint16_t DUP_BG[6] = {
      C565(0xff, 0xe4, 0xec), C565(0xe0, 0xf0, 0xff), C565(0xe6, 0xf8, 0xdc),
      C565(0xff, 0xf2, 0xd0), C565(0xee, 0xe4, 0xff), C565(0xdc, 0xf6, 0xf2),
    };
    uint16_t rowBg = same > 1 ? DUP_BG[m.dex % 6] : UI_WHITE;
    bool perfect = m.flags & BOXF_PERFECT;  // ko11.21
    if (perfect) rowBg = C565(0xff, 0xf2, 0xc4);
    uiButton(73, y, 320, BOX_ROW_H, 10, rowBg, UI_INK);
    drawThumbAt(m.dex, 104, y + BOX_ROW_H / 2, 1, false);
    if (same > 1) {
      char xn[8];
      snprintf(xn, sizeof(xn), XT(X_DUP_COUNT_FMT), same);
      drawBtn(300, y + 6, 44, 22, C565(0xf0, 0x7a, 0xa8), UI_WHITE, xn);
    }
    char l[48];
    snprintf(l, sizeof(l), "%s%s", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex));
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(134, y + 6);
    printT(l);
    snprintf(l, sizeof(l), "Lv.%u  %s", m.lvl, XT((m.flags & BOXF_RAISED) ? X_RAISED_TAG
                                                 : (m.flags & BOXF_CAUGHT) ? X_CAUGHT_TAG : X_WON_TAG));
    setSize(1);
    setCur(134, y + 28);
    printT(l);
    if (perfect) {  // ko11.21: "완벽 육성" en oro y destellos junto a la cinta
      gfx->setTextColor(C565(0xc0, 0x86, 0x10));
      setCur(134 + textW(l, 1) + 10, y + 28);
      printT(XT(X_PERFECT));
      uint32_t t = millis();
      for (int q = 0; q < 3; q++) {
        int ph = (int)((t / 90 + q * 5) % 14);
        int sx = 363 + (q - 1) * 14, sy = y + 6 + (q % 2) * 22, ln = ph < 7 ? ph / 2 + 1 : (14 - ph) / 2 + 1;
        gfx->drawFastHLine(sx - ln, sy, 2 * ln + 1, C565(0xf0, 0xb8, 0x20));
        gfx->drawFastVLine(sx, sy - ln, 2 * ln + 1, C565(0xf0, 0xb8, 0x20));
      }
    }
    if (m.flags & BOXF_RAISED) drawRibbon(363, y + 17, 9);  // ko10.5: criado (ko11.17: escarapela)
    else if (m.flags & BOXF_CAUGHT) drawMap(SPR_ICON_PLAY, 16, 352, y + 7, 2, false);
  }
  // paginas
  if (boxPages() > 1) {
    drawBtn(113, BOX_NAV_Y, 60, 36, boxPage ? UI_WHITE : UI_TRACK, UI_INK, "<");
    drawBtn(293, BOX_NAV_Y, 60, 36, boxPage + 1 < boxPages() ? UI_WHITE : UI_TRACK, UI_INK, ">");
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", boxPage + 1, boxPages());
    drawFit(pg, BOX_NAV_Y + 10, 100, UI_INK, 2);
  }
  if (pet.exped.on) {  // ko11.7: la expedicion en curso (o de vuelta)
    char el[64];
    uint32_t now = clockEpoch();
    bool back = now >= pet.exped.end;
    if (back) snprintf(el, sizeof(el), XT(X_EXP_BACK_FMT), dexName(pet.exped.dex));
    else {
      uint32_t left = pet.exped.end - now;
      snprintf(el, sizeof(el), XT(X_EXP_AWAY_FMT), dexName(pet.exped.dex), (unsigned)(left / 3600),
               (unsigned)(left / 60 % 60));
    }
    drawBtn(88, EXP_STRIP_Y, 290, 30, back ? UI_BAR_OK : C565(0xd8, 0xea, 0xff), back ? UI_WHITE : UI_INK, el);
  }
  drawNav(NAV_L, UI_INK);  // ko11.8: salir con la flecha (antes solo tocando abajo)
  if (expResOpen) drawExpResult();
  uiFlush();
}

void boxSwipe() {  // deslizar: cierra la ficha, o la caja si estaba en la lista
  if (boxOrb && orbSel >= 0) { orbSel = -1; return; }
  expPick = false;
  expResOpen = false;
  if (boxSel >= 0) boxSel = -1;
  else goBack();  // ko11.17
}

static void boxTabTap(int16_t x) {
  if (x >= BOX_TAB_X1 && x < BOX_TAB_X1 + BOX_TAB_W && (boxHall || boxOrb)) { boxHall = false; boxOrb = false; }
  else if (x >= BOX_TAB_X2 && x < BOX_TAB_X2 + BOX_TAB_W && !boxHall) { boxHall = true; boxOrb = false; }
  else if (x >= BOX_TAB_X3 && x < BOX_TAB_X3 + BOX_TAB_W3 && !boxOrb) { boxOrb = true; boxHall = false; orbSel = -1; orbPage = 0; synReset(); }
  else if (x >= BOX_TAB_X4 && x < BOX_TAB_X4 + BOX_TAB_W3) { openCandyBag(); return; }  // ko11.26
  else return;
  boxPage = 0;
  boxSel = -1;
  sfxPlay(SFX_TAP);
}

void orbBagTap(int16_t x, int16_t y);
void boxTap(int16_t x, int16_t y) {
  if (boxOrb) { orbBagTap(x, y); return; }
  if (boxSel >= 0) {  // ficha: soltar (dos toques) o cerrar
    if (boxHall && memSel == boxSel) {  // ko12.4: recuerdos: <- vuelve a la ficha, tocar = la otra pagina
      if (navHit(NAV_L, x, y)) memSel = -1;
      else memPage ^= 1;
      sfxPlay(SFX_TAP);
      return;
    }
    if (boxHall && inRect(x, y, 93, 300, 130, 48)) { memSel = boxSel; memPage = 0; sfxPlay(SFX_TAP); return; }
    if (!expPick && navHit(NAV_L, x, y)) { boxSel = -1; boxConfirmUntil = 0; sfxPlay(SFX_TAP); return; }  // ko11.8
    // ko9.1: tocar al Pokemon repite su grito
    if (y >= 80 && y < 200 && x >= 120 && x < 346) {
      audioCry(cb().at((uint8_t)boxSel).dex);
      return;
    }
    if (expPick) {  // ko11.7: horas de la expedicion
      for (int i = 0; i < 3; i++)
        if (inRect(x, y, 78 + i * 106, 204, 98, 48)) { expSend((uint8_t)boxSel, EXP_HOURS[i]); return; }
      if (inRect(x, y, 153, 264, 160, 42)) { expPick = false; sfxPlay(SFX_TAP); }
      return;
    }
    if (!boxHall && y >= 300 && y < 348 && x >= 183 && x < 283) {  // ko11.7: explorar
      if (pet.exped.on) { sfxPlay(SFX_DENY); return; }
      expPick = true;
      sfxPlay(SFX_TAP);
      return;
    }
    if (!boxHall && y >= 300 && y < 348 && x >= 73 && x < 173) {  // el salon no suelta
      if (timeLeft(boxConfirmUntil)) {
        // ko10.4: soltar daba 1 caramelo de su familia. ko11.14: solo los que CAPTURASTE
        // (los que te siguieron tras ganar no dan nada): 1 caramelo de su familia y a
        // veces un caramelo raro (30 % raro o legendario, 10 % los demas), con toast
        int16_t rd = box.at((uint8_t)boxSel).dex;
        if (box.at((uint8_t)boxSel).flags & BOXF_CAUGHT) {
          pet.addCandy(rd, 1);
          uint8_t rar = DEX_TBL[rd].rarity;
          bool gotRare = (int)random(100) < ((rar == R_RARO || rar == R_LEGENDARIO) ? 30 : 10);
          if (gotRare && pet.rareCandy < 999) pet.rareCandy++;
          char t[72];
          snprintf(t, sizeof(t), XT(X_EXP_CANDY_FMT), dexName(rd), 1u);
          if (gotRare) { strncat(t, "  ", sizeof(t) - strlen(t) - 1); strncat(t, XT(X_EXP_RARE), sizeof(t) - strlen(t) - 1); }
          showToast(t);
        } else {
          // ko11.19: el que solo te siguio tambien deja un regalo pequeno
          uint16_t go = 0;
          uint8_t g = pet.releaseGift(rd, &go);
          char t[96], it[48];
          txFmt(t, sizeof(t), X_RG_THANKS, dexName(rd), nullptr);
          if (g == RG_ORB) { orbName(go, it, sizeof(it)); size_t l = strlen(it); snprintf(it + l, sizeof(it) - l, " +%u%%", orbPct(go)); }
          else if (g == RG_EXP) { char nb[8]; snprintf(nb, sizeof(nb), "%u", (unsigned)(20 + pet.level() * 2)); txFmt(it, sizeof(it), X_RG_EXP, "", nb); }
          else snprintf(it, sizeof(it), "%s", XT(g == RG_BALL ? X_RG_BALL : g == RG_POTION ? X_RG_POTION : X_RG_SHARD));
          strncat(t, " ", sizeof(t) - strlen(t) - 1);
          strncat(t, it, sizeof(t) - strlen(t) - 1);
          showToast(t);
        }
        pet.saveNow();
        box.release((uint8_t)boxSel);
        boxSel = -1;
        boxConfirmUntil = 0;
        sfxPlay(SFX_BYE);
      } else {
        boxConfirmUntil = millis() + 3000;
        sfxPlay(SFX_TAP);
      }
    } else {
      boxSel = -1;
      boxConfirmUntil = 0;
    }
    return;
  }
  if (expResOpen) { expResOpen = false; sfxPlay(SFX_TAP); return; }  // ko11.7: cerrar el resultado
  if (pet.exped.on && inRect(x, y, 88, EXP_STRIP_Y, 290, 30)) {  // ko11.7: recoger
    if (clockEpoch() >= pet.exped.end) expCollect();
    else sfxPlay(SFX_TAP);
    return;
  }
  if (navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); goBack(); return; }  // ko11.8: <- salir (ko11.17: a donde estaba)
  // ko10.5: pestanas caja / salon de la fama
  if (y >= BOX_TAB_Y && y < BOX_TAB_Y + BOX_TAB_H) { boxTabTap(x); return; }
  if (y < BOX_TAB_Y - 10 || y >= 392) { goBack(); return; }  // arriba / abajo: salir (como antes)
  if (y >= BOX_NAV_Y && y < BOX_NAV_Y + 36) {
    if (x < CX && boxPage > 0) boxPage--;
    else if (x >= CX && boxPage + 1 < boxPages()) boxPage++;
    sfxPlay(SFX_TAP);
    return;
  }
  if (x < 73 || x >= 393 || y < BOX_ROW_Y) return;
  int r = (y - BOX_ROW_Y) / (BOX_ROW_H + BOX_ROW_GAP);
  if (r >= BOX_ROWS || (y - BOX_ROW_Y) % (BOX_ROW_H + BOX_ROW_GAP) >= BOX_ROW_H) return;
  int k = boxPage * BOX_ROWS + r;
  Box &bx = cb();
  boxSortView(bx);
  if (k < bx.count()) { boxSel = boxView(k); expPick = false; memSel = -1; audioCry(bx.at((uint8_t)boxSel).dex); }  // ko9.1: su grito
}

// ======================================================================
// ko11.16: bolsa de orbes (tercera pestana de la caja). Rejilla de orbes grandes y
// animados (el equipado, el primero, con aro dorado); al tocar uno: ventana con el
// orbe en grande, su nombre y % y [장착]/[빼기] [닫기]
// ======================================================================
#define ORB_COLS 4
#define ORB_ROWS 3
#define ORB_PER_PAGE (ORB_COLS * ORB_ROWS)
#define ORB_CELL_W 78
#define ORB_CELL_H 94
#define ORB_GRID_Y 130   // centro de la primera fila
#define ORB_R 19

static void textAtX(const char *s, int cx, int y, uint16_t c, uint8_t sz) {
  setSize(sz);
  gfx->setTextColor(c);
  setCur(cx - textW(s, sz) / 2, y);
  printT(s);
}
static uint8_t orbViewN() { return (orbValid(pet.orb) ? 1 : 0) + pet.orbN; }
// k-esimo de la vista: el equipado primero (worn = true), luego la bolsa
static uint16_t orbViewAt(int k, bool &worn) {
  worn = false;
  if (orbValid(pet.orb)) {
    if (k == 0) { worn = true; return pet.orb; }
    k--;
  }
  return (k >= 0 && k < pet.orbN) ? pet.orbBag[k] : 0;
}
static uint8_t orbPages() { return orbViewN() ? (uint8_t)((orbViewN() + ORB_PER_PAGE - 1) / ORB_PER_PAGE) : 1; }
static void orbCell(int i, int &cx, int &cy) {
  cx = CX + (int)((i % ORB_COLS) * 2 - (ORB_COLS - 1)) * ORB_CELL_W / 2;
  cy = ORB_GRID_Y + (i / ORB_COLS) * ORB_CELL_H;
}
static void orbPopupRect(int &x, int &y, int &w, int &h) { x = 58; y = 88; w = 350; h = 294; }
static void orbSay(const char *m, bool good) {
  orbMsg = m;
  orbMsgUntil = millis() + 1800;
  sfxPlay(good ? SFX_HEART : SFX_DENY);
}

void renderOrbBag() {
  uiScreenBg();
  char t1[24], t2[24], b[64];
  snprintf(t1, sizeof(t1), XT(X_BOX_TITLE_FMT), box.count(), BOX_MAX);
  snprintf(t2, sizeof(t2), XT(X_HALL_TAB_FMT), hall.count());
  drawBoxTabs(t1, t2);
  uint32_t now = millis();
  uint8_t n = orbViewN();
  if (orbPage >= orbPages()) orbPage = orbPages() - 1;
  if (!n) {
    drawOrb(CX, 190, 34, 0, now);
    drawFit(XT(X_ORB_EMPTY), 250, 300, UI_INK, 2);
    drawFit(XT(X_ORB_HINT), 282, 330, 0x8410, 1);
  }
  for (int i = 0; i < ORB_PER_PAGE; i++) {
    int k = orbPage * ORB_PER_PAGE + i;
    if (k >= n) break;
    bool worn;
    uint16_t o = orbViewAt(k, worn);
    int cx, cy;
    orbCell(i, cx, cy);
    bool fits = pet.orbFits(o);
    // pedestal: sombra ovalada bajo el orbe; el equipado, con aro dorado
    gfx->fillEllipse(cx, cy + ORB_R + 6, ORB_R, 5, uiLerp(UI_BG_DAY, UI_INK, 3, 16));
    uint8_t bi = (uint8_t)(k - (orbValid(pet.orb) ? 1 : 0));
    bool picked = synMode && !worn && synPicked(bi);
    if (worn && !synMode) {
      gfx->drawCircle(cx, cy, ORB_R + 5, C565(0xe8, 0xb0, 0x20));
      gfx->drawCircle(cx, cy, ORB_R + 6, C565(0xe8, 0xb0, 0x20));
    }
    if (picked) {  // ko11.19: material elegido: aro verde que late
      int pr = ORB_R + 6 + (int)((now / 90) % 3);
      gfx->drawCircle(cx, cy, pr, UI_BAR_OK);
      gfx->drawCircle(cx, cy, pr + 1, UI_BAR_OK);
    }
    drawOrb(cx, cy, ORB_R, o, now + k * 137);  // cada uno a su ritmo
    if (synMode && worn) {  // el equipado no se puede fundir
      textAtX(XT(X_SYN_WORN), cx, cy + ORB_R + 12, 0x8410, 1);
      continue;
    }
    if (picked) drawCheckMark(cx + ORB_R - 2, cy - ORB_R + 2, true);
    snprintf(b, sizeof(b), "+%u%%", orbPct(o));
    textAtX(b, cx, cy + ORB_R + 12, worn ? C565(0xb0, 0x80, 0x10) : fits ? C565(0x2e, 0x7d, 0x32) : 0x8410, 1);
  }
  // abajo: [구슬 합성] (o, eligiendo: [취소] [합성하기 N/3]); el aviso encima
  if (orbMsg && timeLeft(orbMsgUntil) && orbSel < 0) drawFit(orbMsg, SYN_BTN_Y + 8, 320, UI_BAR_OK, 2);
  else if (synMode) {
    drawBtn(93, SYN_BTN_Y, 110, 36, UI_TRACK, UI_INK, XT(X_SYN_CANCEL));
    snprintf(b, sizeof(b), XT(X_SYN_GO_FMT), synN);
    bool ready = synN == 3;
    drawBtn(213, SYN_BTN_Y, 160, 36, ready ? C565(0x6a, 0x4c, 0xf0) : UI_TRACK, ready ? UI_WHITE : 0x8410, b);
  } else if (n && !synShow && !synAnimT) {
    drawBtn(CX - 80, SYN_BTN_Y, 160, 36, C565(0x6a, 0x4c, 0xf0), UI_WHITE, XT(X_SYN_BTN));
  }
  if (orbPages() > 1) {
    snprintf(b, sizeof(b), XT(X_ORB_PAGE_FMT), orbPage + 1, orbPages());
    drawFit(b, SYN_BTN_Y + 42, 100, UI_INK, 1);
  }
  drawNav(NAV_L, UI_INK);  // pagina anterior, o salir en la primera
  if (orbPage + 1 < orbPages()) drawNav(NAV_R, UI_INK);
  // ventana del orbe elegido
  if (orbSel >= 0 && orbSel < n) {
    bool worn;
    uint16_t o = orbViewAt(orbSel, worn);
    int x, y, w, h;
    orbPopupRect(x, y, w, h);
    uiPanel(x, y, w, h, 20, UI_WHITE, UI_INK);
    // fondo del orbe: un halo tenue de su color
    gfx->fillCircle(CX, y + 90, 60, uiLerp(orbColor(orbType(o)), UI_WHITE, 13, 16));
    drawOrb(CX, y + 96, 40, o, now);
    orbName(o, b, sizeof(b));
    drawFit(b, y + 146, w - 30, uiLerp(orbColor(orbType(o)), UI_INK, 8, 16), 2);
    orbPctText(o, b, sizeof(b));
    drawFit(b, y + 174, w - 30, UI_INK, 2);
    int by = y + h - 58;
    if (worn) {
      drawBtn(x + 20, by, (w - 50) / 2, 42, C565(0xe8, 0xb0, 0x20), UI_WHITE, XT(X_ORB_UNEQUIP));
    } else if (pet.orbFits(o)) {
      drawBtn(x + 20, by, (w - 50) / 2, 42, C565(0x6a, 0x4c, 0xf0), UI_WHITE, XT(X_ORB_EQUIP));
    } else {
      snprintf(b, sizeof(b), XT(X_ORB_NOFIT_FMT), typeName(orbType(o)));
      drawFit(b, y + 208, w - 30, 0x8410, 1);
      drawBtn(x + 20, by, (w - 50) / 2, 42, UI_TRACK, 0x8410, XT(X_ORB_EQUIP));
    }
    if (worn) drawFit(XT(X_ORB_WORN), y + 208, w - 30, C565(0xb0, 0x80, 0x10), 1);
    drawBtn(x + 30 + (w - 50) / 2, by, (w - 50) / 2, 42, UI_TRACK, UI_INK, XT(X_CLOSE));
    if (orbMsg && timeLeft(orbMsgUntil)) drawFit(orbMsg, y + h + 8, 320, UI_BAR_OK, 1);
  }
  // ko11.19: animacion de la fusion: los 3 giran hacia el centro y destellan
  if (synAnimT) {
    uint32_t t = now - synAnimT;
    uiShade(0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 9);
    float p = t >= SYN_ANIM_MS ? 1.0f : (float)t / SYN_ANIM_MS;
    if (p < 0.8f) {
      float q = p / 0.8f;
      int rad = (int)(96 * (1.0f - q * q));
      for (int i = 0; i < 3; i++) {
        float a = q * 9.0f + i * 2.094f;
        int ox = CX + (int)(cosf(a) * rad), oy = 220 + (int)(sinf(a) * rad);
        drawOrb(ox, oy, 22 - (int)(q * 8), synMat[i], now + i * 200);
      }
      gfx->fillCircle(CX, 220, 4 + (int)(q * 10), UI_WHITE);
    } else {  // destello
      int fr = 20 + (int)((p - 0.8f) / 0.2f * 140);
      gfx->fillCircle(CX, 220, fr, uiLerp(UI_WHITE, C565(0xf8, 0xe8, 0x90), 6, 16));
      gfx->fillCircle(CX, 220, fr * 2 / 3, UI_WHITE);
    }
    if (t >= SYN_ANIM_MS) { synAnimT = 0; synShow = true; sfxPlay(synRes ? SFX_MEDAL : SFX_DENY); }
  }
  // ventana del resultado
  if (synShow) {
    int x = 58, y = 88, w = 350, h = 294;
    uiPanel(x, y, w, h, 20, UI_WHITE, UI_INK);
    if (synRes) {
      uint16_t o = synOut;
      if (synRes >= 2)  // exito grande: rayos dorados (ko12.2.1: arcoiris: de colores)
        for (int k = 0; k < (synRes == 3 ? 18 : 12); k++) {
          float a = now * 0.001f + k * (synRes == 3 ? 0.349f : 0.5236f);
          uint16_t rc = synRes == 3 ? orbHue(k / 18.0f + now * 0.0003f) : C565(0xf0, 0xc0, 0x30);
          int r1 = synRes == 3 ? 82 + (int)(6 * sinf(now * 0.01f + k)) : 78;
          gfx->drawLine(CX + (int)(cosf(a) * 50), y + 96 + (int)(sinf(a) * 50), CX + (int)(cosf(a) * r1), y + 96 + (int)(sinf(a) * r1), rc);
          if (synRes == 3) gfx->drawLine(CX + (int)(cosf(a) * 50) + 1, y + 96 + (int)(sinf(a) * 50), CX + (int)(cosf(a) * r1) + 1, y + 96 + (int)(sinf(a) * r1), rc);
        }
      gfx->fillCircle(CX, y + 90, 58, uiLerp(orbColor(orbType(o)), UI_WHITE, 13, 16));
      drawOrb(CX, y + 96, 40, o, now);
      const char *ttl = XT(synRes == 3 ? X_SYN_DUAL : synRes == 2 ? X_SYN_GREAT : X_SYN_OK);
      int tw = textW(ttl, 2);
      if (tw > w - 40) tw = w - 40;
      gfx->fillRoundRect(CX - tw / 2 - 10, y + 8, tw + 20, textH(2) + 14, 10, UI_WHITE);  // ko12.2.1: legible sobre las llamas
      drawFit(ttl, y + 14, w - 40,
              synRes == 3 ? uiLerp(orbHue(now * 0.0005f), UI_INK, 6, 16) : synRes == 2 ? C565(0xc0, 0x80, 0x10) : UI_BAR_OK, 2);
      orbName(o, b, sizeof(b));
      drawFit(b, y + 150, w - 30, uiLerp(orbColor(orbType(o)), UI_INK, 8, 16), 2);
      orbPctText(o, b, sizeof(b));
      drawFit(b, y + 178, w - 30, UI_INK, 2);
    } else {
      // fallo: esfera gris rajada y humo
      uint16_t g = C565(0x90, 0x90, 0x98);
      gfx->fillCircle(CX, y + 96, 36, g);
      gfx->drawLine(CX - 10, y + 62, CX + 4, y + 90, UI_INK);
      gfx->drawLine(CX + 4, y + 90, CX - 6, y + 108, UI_INK);
      gfx->drawLine(CX - 6, y + 108, CX + 8, y + 130, UI_INK);
      for (int k = 0; k < 3; k++)
        gfx->fillCircle(CX - 30 + k * 30, y + 52 - (int)((now / 40 + k * 13) % 20), 7 + k, C565(0xc8, 0xc8, 0xcc));
      drawFit(XT(X_SYN_FAIL), y + 150, w - 30, UI_BAR_BAD, 2);
      drawFit(XT(X_SYN_FAIL_SUB), y + 178, w - 30, UI_INK, 2);
    }
    drawFit(XT(X_SYN_TAP), y + h - 40, w - 40, 0x8410, 1);
  }
  uiFlush();
}

void orbBagTap(int16_t x, int16_t y) {
  uint8_t n = orbViewN();
  if (synAnimT) return;                                      // ko11.19: animacion
  if (synShow) { synShow = false; sfxPlay(SFX_TAP); return; }  // cerrar el resultado
  if (synMode || (n && !orbSel_open())) {
    if (y >= SYN_BTN_Y - 4 && y < SYN_BTN_Y + 40) {  // botones de abajo
      if (!synMode) {
        if (x < CX - 80 || x >= CX + 80) return;
        if (pet.isEgg()) { orbSay(XT(X_SYN_EGG), false); return; }
        if (pet.orbN < 3) { orbSay(XT(X_SYN_NEED), false); return; }
        synMode = true;
        synN = 0;
        sfxPlay(SFX_TAP);
        return;
      }
      if (x >= 93 && x < 203) { synReset(); sfxPlay(SFX_TAP); return; }  // cancelar
      if (x >= 213 && x < 373 && synN == 3) {                            // fusionar
        for (int i = 0; i < 3; i++) synMat[i] = pet.orbBag[synPick[i]];
        synRes = pet.synthOrbs(synPick, synOut);
        synMode = false;
        synN = 0;
        synAnimT = millis();
        sfxPlay(SFX_EVOLVE);
      }
      return;
    }
  }
  if (synMode) {  // elegir materiales (el equipado no)
    if (navHit(NAV_L, x, y)) { if (orbPage > 0) orbPage--; else synReset(); sfxPlay(SFX_TAP); return; }
    if (navHit(NAV_R, x, y)) { if (orbPage + 1 < orbPages()) orbPage++; sfxPlay(SFX_TAP); return; }
    for (int i = 0; i < ORB_PER_PAGE; i++) {
      int k = orbPage * ORB_PER_PAGE + i;
      if (k >= n) break;
      int cx, cy;
      orbCell(i, cx, cy);
      if (abs(x - cx) >= ORB_CELL_W / 2 || abs(y - cy) >= ORB_CELL_H / 2) continue;
      bool worn;
      orbViewAt(k, worn);
      if (worn) { sfxPlay(SFX_DENY); return; }
      uint8_t bi = (uint8_t)(k - (orbValid(pet.orb) ? 1 : 0));
      for (uint8_t j = 0; j < synN; j++)
        if (synPick[j] == bi) {  // quitar
          for (uint8_t m = j; m + 1 < synN; m++) synPick[m] = synPick[m + 1];
          synN--;
          sfxPlay(SFX_TAP);
          return;
        }
      if (synN < 3) { synPick[synN++] = bi; sfxPlay(SFX_TAP); }
      else sfxPlay(SFX_DENY);
      return;
    }
    return;
  }
  if (orbSel >= 0) {  // ventana abierta
    int px, py, pw, ph;
    orbPopupRect(px, py, pw, ph);
    int by = py + ph - 58, bw = (pw - 50) / 2;
    if (!inRect(x, y, px, py, pw, ph) || inRect(x, y, px + 30 + bw, by, bw, 42)) { orbSel = -1; sfxPlay(SFX_TAP); return; }
    if (!inRect(x, y, px + 20, by, bw, 42) || orbSel >= n) return;
    bool worn;
    uint16_t o = orbViewAt(orbSel, worn);
    if (worn) { pet.unequipOrb(); orbSel = -1; orbSay(XT(X_ORB_UNEQUIP), true); return; }
    if (!pet.orbFits(o)) { sfxPlay(SFX_DENY); return; }
    uint8_t bagIdx = (uint8_t)(orbSel - (orbValid(pet.orb) ? 1 : 0));
    if (pet.equipOrb(bagIdx)) { orbSel = -1; orbPage = 0; orbSay(XT(X_ORB_WORN), true); }
    else sfxPlay(SFX_DENY);
    return;
  }
  if (navHit(NAV_L, x, y)) {
    if (orbPage > 0) orbPage--;
    else goBack();
    sfxPlay(SFX_TAP);
    return;
  }
  if (navHit(NAV_R, x, y)) { if (orbPage + 1 < orbPages()) orbPage++; sfxPlay(SFX_TAP); return; }
  if (y >= BOX_TAB_Y && y < BOX_TAB_Y + BOX_TAB_H) { boxTabTap(x); return; }
  if (y < BOX_TAB_Y - 10 || y >= 392) { goBack(); return; }
  for (int i = 0; i < ORB_PER_PAGE; i++) {
    int k = orbPage * ORB_PER_PAGE + i;
    if (k >= n) break;
    int cx, cy;
    orbCell(i, cx, cy);
    if (abs(x - cx) < ORB_CELL_W / 2 && abs(y - cy) < ORB_CELL_H / 2) { orbSel = (int8_t)k; sfxPlay(SFX_TAP); return; }
  }
}

// ======================================================================
// ko10.5: fin de un ciclo -> el que se va queda en la caja (corona) y se elige
// el siguiente: huevo nuevo (familia sin criar, al azar) o uno de la caja
// (familia sin criar), que vuelve a su primera forma a nivel 1
// ======================================================================
bool gNextPickPending = false;
uint8_t nextPage = 0;
#define NP_EGG_Y 72
#define NP_ROW_Y 132
#define NP_ROW_H 46
#define NP_ROW_GAP 6
#define NP_ROWS 4
#define NP_NAV_Y 346

void bakRequest();  // ko11.6
void onPetEnd(Pet &p, uint8_t how) {
  bakRequest();  // ko11.6: la despedida tambien va a la copia de la SD
  if (how == CER_RUNAWAY || p.isEgg()) return;  // escapada: huevo y ya
  if (how != CER_FAREWELL) {  // ko11.9.2: la corona (salon) solo para quien llego al final
    gNextPickPending = true;   // soltarlo lo decidimos nosotros: sin corona
    return;
  }
  uint32_t endE = clockEpoch();
  bool added = hall.addRaised(p.speciesId, p.level(), p.shiny, p.geneAtk, p.geneDef, p.geneSpe, endE, p.mv);  // salon
  if (added) {  // ko12.4: su diario, para verlo en la ficha de la cinta
    MemRec mr;
    p.lifeMemory(mr, endE);
    memStore.put(endE, p.speciesId, mr);
  }
  // ko11.21: con las 8 medallas = crianza perfecta (brilla en la cinta)
  const uint16_t all = (uint16_t)((1u << MED_COUNT) - 1);
  if (added && (p.medals & all) == all) hall.markFlag((uint8_t)(hall.count() - 1), BOXF_PERFECT);
  gNextPickPending = true;
}

// se puede elegir si su familia no se ha criado (o si ya se criaron todas)
static bool nextPickable(const BoxMon &m) { return pet.allFamsRaised() || !pet.isFamRaised(m.dex); }


static uint8_t nextPages() { return box.count() ? (box.count() + NP_ROWS - 1) / NP_ROWS : 1; }

void renderNextPick() {
  uiScreenBg();  // ko11.6.1: sin pasar por negro (parpadeo)
  drawFit(XT(X_NEXT_TITLE), 34, 320, UI_INK, 2);
  drawBtn(93, NP_EGG_Y, 280, 48, UI_BAR_WARN, UI_INK, XT(X_NEXT_EGG));
  boxSortView(box);
  if (nextPage >= nextPages()) nextPage = nextPages() - 1;
  for (int r = 0; r < NP_ROWS; r++) {
    int k = nextPage * NP_ROWS + r;
    if (k >= box.count()) break;
    const BoxMon &m = box.at(boxView(k));
    bool ok = nextPickable(m);
    int y = NP_ROW_Y + r * (NP_ROW_H + NP_ROW_GAP);
    uiButton(73, y, 320, NP_ROW_H, 10, ok ? UI_WHITE : UI_TRACK, UI_INK);
    drawThumbAt(m.dex, 102, y + NP_ROW_H / 2, 1, !ok);
    char l[48];
    snprintf(l, sizeof(l), "%s%s Lv.%u", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex), m.lvl);
    gfx->setTextColor(ok ? UI_INK : 0x8410);
    setSize(2);
    setCur(130, y + 4);
    printT(l);
    char l2[48];
    if (ok) txFmt(l2, sizeof(l2), X_NEXT_FROM, dexName(dexFirstForm(m.dex)), nullptr);
    else snprintf(l2, sizeof(l2), "%s", XT(X_NEXT_RAISED));
    setSize(1);
    setCur(130, y + 27);
    printT(l2);
    if (m.flags & BOXF_RAISED) drawRibbon(369, y + 17, 9);
  }
  if (nextPages() > 1) {
    drawBtn(113, NP_NAV_Y, 60, 34, nextPage ? UI_WHITE : UI_TRACK, UI_INK, "<");
    drawBtn(293, NP_NAV_Y, 60, 34, nextPage + 1 < nextPages() ? UI_WHITE : UI_TRACK, UI_INK, ">");
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", nextPage + 1, nextPages());
    drawFit(pg, NP_NAV_Y + 8, 100, UI_INK, 2);
  }
  drawFit(XT(X_NEXT_HINT), 392, 300, 0x8410, 1);
  uiFlush();
}

void nextPickTap(int16_t x, int16_t y) {
  if (inRect(x, y, 93, NP_EGG_Y, 280, 48)) {  // el huevo que ya esta puesto
    sfxPlay(SFX_TAP);
    xScreen = XS_NONE;
    return;
  }
  if (nextPages() > 1 && y >= NP_NAV_Y && y < NP_NAV_Y + 34) {
    if (x < CX && nextPage > 0) nextPage--;
    else if (x >= CX && nextPage + 1 < nextPages()) nextPage++;
    sfxPlay(SFX_TAP);
    return;
  }
  if (x < 73 || x >= 393 || y < NP_ROW_Y) return;
  int r = (y - NP_ROW_Y) / (NP_ROW_H + NP_ROW_GAP);
  if (r >= NP_ROWS || (y - NP_ROW_Y) % (NP_ROW_H + NP_ROW_GAP) >= NP_ROW_H) return;
  int k = nextPage * NP_ROWS + r;
  boxSortView(box);
  if (k >= box.count()) return;
  uint8_t idx = boxView(k);
  if (!nextPickable(box.at(idx))) { sfxPlay(SFX_DENY); return; }
  BoxMon m;
  if (!box.take(idx, m)) return;
  // vuelve a su primera forma, a nivel 1; conserva shiny y genes
  pet.adoptMon(dexFirstForm(m.dex), 1, m.flags & BOXF_SHINY, m.geneAtk, m.geneDef, m.geneSpe);
  char msg[64];
  txFmt(msg, sizeof(msg), X_FROM_BOX, dexName(pet.speciesId));
  showToast(msg);
  toastUntil = millis() + 6000;
  xScreen = XS_NONE;
}

// se abre sola al volver a la pantalla principal tras la ceremonia
void nextPickPoll() {
  if (!gNextPickPending || xScreen != XS_NONE) return;
  gNextPickPending = false;
  if (!pet.isEgg() || !box.count()) return;  // sin caja: huevo y ya
  nextPage = 0;
  xScreen = XS_NEXTPICK;
}

// ======================================================================
// Sonido: volumen de musica, voces y sistema (desde la pantalla de hora)
// ======================================================================

#define VOL_ROW_Y 128  // ko11.8: un poco mas juntas (cabe [배경음 고르기])
#define VOL_ROW_H 66
#define VOL_BGM_Y 326
#define VOL_DONE_Y 366
#define VOL_BAR_X 138
#define VOL_BAR_W 190
#define SND_BTN_X0 80   // ko11.25
#define SND_BTN_X1 236
bool vibEnabled();
void vibSetEnabled(bool on);
void vibPulse(uint16_t ms, uint8_t n, uint16_t gap);

void openSound() {
  retMark();
  clockOpen = false;
  xScreen = XS_VOL;
}

void renderSound() {
  uiScreenBg();  // ko11.6.1: sin pasar por negro (parpadeo)
  drawFit(XT(X_SOUND_TITLE), 40, 300, UI_INK, 3);
  bool on = audioEnabled();
  // ko11.25: [소리 켬/끔] [진동 켬/끔] lado a lado
  drawBtn(SND_BTN_X0, 78, 150, 40, on ? UI_BAR_OK : UI_TRACK, on ? UI_WHITE : UI_INK,
          XT(on ? X_SOUND_ON : X_SOUND_OFF));
  // ko11.26: [진동 꺼짐 / 약 / 중 / 강]: cada toque pasa al siguiente
  bool vb = vibEnabled();
  static const XId VLBL[3] = { X_VIB_WEAK, X_VIB_MID, X_VIB_ON };
  drawBtn(SND_BTN_X1, 78, 150, 40, vb ? UI_BAR_OK : UI_TRACK, vb ? UI_WHITE : UI_INK,
          XT(vb ? VLBL[vibLevel()] : X_VIB_OFF));
  static const XId LBL[3] = { X_VOL_BGM, X_VOL_CRY, X_VOL_SYS };
  for (int i = 0; i < 3; i++) {
    int y = VOL_ROW_Y + i * VOL_ROW_H;
    uint8_t v = audioVolume(i);
    gfx->setTextColor(on ? UI_INK : 0x4208);
    setSize(2);
    setCur(96, y);
    printT(XT(LBL[i]));
    char pc[8];
    snprintf(pc, sizeof(pc), "%u%%", v);
    setCur(370 - textW(pc, 2), y);
    printT(pc);
    drawBtn(84, y + 24, 44, 36, UI_WHITE, UI_INK, "-");
    drawBtn(338, y + 24, 44, 36, UI_WHITE, UI_INK, "+");
    uiGauge(VOL_BAR_X, y + 34, VOL_BAR_W, 16, v * 10, on ? UI_BAR_OK : 0x8410, UI_TRACK);  // ko11.12
  }
  // ko11.8: elegir que fondos suenan (la duracion de cada uno esta en esa pantalla)
  // ko11.26: el brillo paso al menu de ajustes
  drawBtn(133, VOL_BGM_Y, 200, 34, UI_WHITE, UI_INK, XT(X_BGM_PICK_BTN));
  drawBtn(143, VOL_DONE_Y, 180, 40, UI_BAR_OK, UI_WHITE, XT(X_VOL_DONE));
  drawNav(NAV_L, UI_INK);  // ko11.17: volver a la hora
  uiFlush();
}

static void soundPreview(int ch) {
  if (ch == 1) audioCry(pet.isEgg() ? 25 : pet.speciesId);
  else if (ch == 2) sfxPlay(SFX_TAP);
}

void soundTap(int16_t x, int16_t y) {
  if (y >= 78 && y < 118 && x >= SND_BTN_X0 && x < SND_BTN_X0 + 150) {
    audioSetEnabled(!audioEnabled());
    if (audioEnabled()) sfxPlay(SFX_TAP);
    return;
  }
  if (y >= 78 && y < 118 && x >= SND_BTN_X1 && x < SND_BTN_X1 + 150) {  // ko11.25: vibracion
    // ko11.26: apagada -> debil -> media -> fuerte -> apagada
    if (!vibEnabled()) { vibSetLevel(0); vibSetEnabled(true); }
    else if (vibLevel() < 2) vibSetLevel(vibLevel() + 1);
    else vibSetEnabled(false);
    if (vibEnabled()) vibPulse(150, 1, 0);  // asi se nota la fuerza elegida
    sfxPlay(SFX_TAP);
    return;
  }
  if (y >= VOL_BGM_Y && y < VOL_BGM_Y + 36 && x >= 133 && x < 333) {  // ko11.8
    openBgmPick();
    sfxPlay(SFX_TAP);
    return;
  }
  if ((y >= VOL_DONE_Y - 4 && x >= 133 && x < 333) || y < 72 || navHit(NAV_L, x, y)) {  // completar -> hora
    sfxPlay(SFX_TAP);
    goBack();
    return;
  }
  for (int i = 0; i < 3; i++) {
    int y0 = VOL_ROW_Y + i * VOL_ROW_H + 20;
    if (y < y0 || y >= y0 + 46) continue;
    int v = audioVolume(i);
    if (x < VOL_BAR_X - 4) v -= 10;
    else if (x >= VOL_BAR_X + VOL_BAR_W + 4) v += 10;
    else v = (x - VOL_BAR_X) * 100 / VOL_BAR_W;  // tocar la barra = poner ahi
    v = (v + 5) / 10 * 10;                      // pasos de 10
    audioSetVolume(i, (uint8_t)constrain(v, 0, 100));
    soundPreview(i);
    return;
  }
}

// ======================================================================
// ko11.18: brillo de la pantalla (1..10), igual con USB y con bateria. Se ve al
// momento; [<] / [완료] vuelve a sonido
// ======================================================================
#define BR_SEG_Y 214
#define BR_CHG_Y 324   // ko11.23.3
#define BR_CHG_X0 78
#define BR_CHG_X1 210
bool chargeCap90();
void setChargeCap90(bool on);
void renderBright() {
  uiScreenBg();
  drawFit(XT(X_BRIGHT_TITLE), 40, 300, UI_INK, 3);
  uint8_t lv = brightLevel();
  // sol que crece con el brillo
  int r = 14 + lv * 2;
  uint16_t sun = C565(0xf8, 0xc8, 0x30);
  for (int k = 0; k < 12; k++) {
    float a = k * 0.5236f;
    int x1 = CX + (int)(cosf(a) * (r + 6)), y1 = 128 + (int)(sinf(a) * (r + 6));
    int x2 = CX + (int)(cosf(a) * (r + 12 + lv)), y2 = 128 + (int)(sinf(a) * (r + 12 + lv));
    gfx->drawLine(x1, y1, x2, y2, sun);
    gfx->drawLine(x1 + 1, y1, x2 + 1, y2, sun);
  }
  gfx->fillCircle(CX, 128, r, sun);
  gfx->fillCircle(CX - r / 3, 128 - r / 3, r / 3, C565(0xff, 0xec, 0xa0));
  // 10 segmentos + [-] [+]
  for (int i = 0; i < 10; i++) {
    int x = 133 + i * 20;
    uint16_t c = i < lv ? uiLerp(C565(0xf0, 0xa0, 0x20), C565(0xff, 0xe0, 0x60), i * 16 / 9, 16) : UI_TRACK;
    uiButton(x, BR_SEG_Y, 16, 36, 4, c, UI_INK);
  }
  drawBtn(78, BR_SEG_Y, 48, 36, UI_WHITE, UI_INK, "-");
  drawBtn(340, BR_SEG_Y, 48, 36, UI_WHITE, UI_INK, "+");
  char b[12];
  snprintf(b, sizeof(b), "%u%%", (unsigned)(lv * 10));
  drawFit(b, 262, 200, UI_INK, 3);
  // ko11.23.3: limite de carga [100%] [~90%] (el aviso del brillo deja sitio)
  bool cap = chargeCap90();
  gfx->drawFastHLine(118, BR_CHG_Y - 30, 230, UI_TRACK);
  drawFit(XT(X_CHG_LABEL), BR_CHG_Y - 24, 300, C565(0x60, 0x68, 0x70), 1);
  drawBtn(BR_CHG_X0, BR_CHG_Y, 120, 34, cap ? UI_WHITE : UI_BAR_OK, cap ? UI_INK : UI_WHITE, XT(X_CHG_FULL));
  drawBtn(BR_CHG_X1, BR_CHG_Y, 170, 34, cap ? UI_BAR_OK : UI_WHITE, cap ? UI_WHITE : UI_INK, XT(X_CHG_SAFE));
  drawBtn(143, VOL_DONE_Y, 180, 40, UI_BAR_OK, UI_WHITE, XT(X_VOL_DONE));
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void brightTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y) || (y >= VOL_DONE_Y - 4 && x >= 133 && x < 333) || y < 60) {
    xScreen = XS_SET;  // ko11.26: el brillo se abre desde ajustes
    sfxPlay(SFX_TAP);
    return;
  }
  if (y >= BR_CHG_Y - 6 && y < BR_CHG_Y + 40) {  // ko11.23.3: limite de carga
    if (x >= BR_CHG_X0 && x < BR_CHG_X0 + 120) setChargeCap90(false);
    else if (x >= BR_CHG_X1 && x < BR_CHG_X1 + 170) setChargeCap90(true);
    else return;
    sfxPlay(SFX_TAP);
    return;
  }
  if (y < BR_SEG_Y - 10 || y >= BR_SEG_Y + 46) return;
  uint8_t lv = brightLevel();
  if (x < 130) { if (lv > 1) lv--; }
  else if (x >= 336) { if (lv < 10) lv++; }
  else if (x >= 133 && x < 333) lv = (uint8_t)((x - 133) / 20 + 1);  // tocar un segmento
  setBrightLevel(lv);
  updateBrightness(millis());  // se ve ya
  sfxPlay(SFX_TAP);
}

// ======================================================================
// ko11.8: elegir los fondos normales (bgm.wav, bgm2.wav ... bgm8.wav de la SD).
// Suenan al azar solo los activados; tocar el nombre o [>] = escucharlo ya.
// ======================================================================

#define BGMP_ROWS 4
#define BGMP_Y0 104
#define BGMP_H 50
static uint8_t bgmPage = 0;
static uint32_t bgmWarnUntil = 0;

static uint8_t bgmList(uint8_t *out) {  // indices de los ficheros que hay
  uint8_t n = 0, av = audioBgmAvail();
  for (uint8_t i = 0; i < BGM_MAX; i++)
    if (av & (1u << i)) out[n++] = i;
  return n;
}

void openBgmPick() {
  audioScanBgm();  // por si se anadieron canciones con el instalador web
  bgmPage = 0;
  bgmWarnUntil = 0;
  xScreen = XS_BGM;
}

void renderBgmPick() {
  uiScreenBg();
  drawFit(XT(X_BGM_PICK_TITLE), 38, 300, UI_INK, 3);
  drawFit(XT(X_BGM_PICK_HINT), 76, 320, 0x8410, 1);
  uint8_t list[BGM_MAX];
  uint8_t n = bgmList(list), mask = audioBgmMask();
  uint8_t pages = (n + BGMP_ROWS - 1) / BGMP_ROWS;
  if (bgmPage >= pages) bgmPage = 0;
  int8_t now = audioBgmNow();
  for (uint8_t r = 0; r < BGMP_ROWS; r++) {
    uint8_t k = bgmPage * BGMP_ROWS + r;
    if (k >= n) break;
    uint8_t i = list[k];
    int y = BGMP_Y0 + r * BGMP_H;
    bool on = mask & (1u << i);
    if (now == (int8_t)i) gfx->fillRoundRect(58, y - 2, 350, BGMP_H - 4, 10, C565(0xd8, 0xf0, 0xd8));
    drawBtn(64, y + 4, 62, 36, on ? UI_BAR_OK : UI_TRACK, on ? UI_WHITE : UI_INK, XT(on ? X_BGM_ON : X_BGM_OFF));
    char name[40];
    const char *t = audioBgmTitle(i);
    if (t[0]) snprintf(name, sizeof(name), "%s", t);
    else snprintf(name, sizeof(name), XT(X_BGM_ROW_FMT), (unsigned)(i + 1));
    gfx->setTextColor(on ? UI_INK : 0x8410);
    setSize(textW(name, 2) > 204 ? 1 : 2);
    setCur(136, y + 4);
    printT(name);
    char sub[48], file[48];
    audioBgmPath(i, file, sizeof(file));
    const char *base = strrchr(file, '/');  // ko11.8.1: el nombre real (puede venir cambiado)
    base = base ? base + 1 : file;
    uint16_t sec = audioBgmSecondsOf(i);
    snprintf(sub, sizeof(sub), "%.24s  %u:%02u", base, (unsigned)(sec / 60), (unsigned)(sec % 60));
    gfx->setTextColor(0x8410);
    setSize(1);
    setCur(136, y + 30);
    printT(sub);
    // [>] escuchar
    int px = 352, py = y + 4;
    uiButton(px, py, 44, 36, 10, now == (int8_t)i ? UI_BAR_OK : UI_WHITE, UI_INK);
    uint16_t tc = now == (int8_t)i ? UI_WHITE : UI_INK;
    gfx->fillTriangle(px + 16, py + 9, px + 16, py + 27, px + 31, py + 18, tc);
  }
  drawNav(NAV_L, UI_INK);  // ko11.17: pagina anterior, o volver al sonido en la 1a
  if (pages > 1) {
    drawNav(NAV_R, UI_INK);
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", (unsigned)(bgmPage + 1), (unsigned)pages);
    drawFit(pg, BGMP_Y0 + BGMP_ROWS * BGMP_H + 2, 100, UI_INK, 1);
  }
  int my = BGMP_Y0 + BGMP_ROWS * BGMP_H + 20;
  if (timeLeft(bgmWarnUntil)) drawFit(XT(X_BGM_LAST), my, 320, UI_BAR_BAD, 1);
  else if (n < BGM_MAX) {
    uint8_t nx = 1;  // primer numero libre (bgm.wav = 1)
    while (nx < BGM_MAX && (audioBgmAvail() & (1u << nx))) nx++;
    char hint[64];
    snprintf(hint, sizeof(hint), XT(X_BGM_ADD_HINT), (unsigned)(nx + 1));
    drawFit(hint, my, 320, 0x8410, 1);
  }
  drawBtn(143, VOL_DONE_Y, 180, 40, UI_BAR_OK, UI_WHITE, XT(X_VOL_DONE));
  uiFlush();
}

void bgmPickTap(int16_t x, int16_t y) {
  uint8_t list[BGM_MAX];
  uint8_t n = bgmList(list);
  uint8_t pages = (n + BGMP_ROWS - 1) / BGMP_ROWS;
  if ((y >= VOL_DONE_Y - 4 && x >= 133 && x < 333) || y < 60) {  // hecho -> sonido
    xScreen = XS_VOL;
    sfxPlay(SFX_TAP);
    return;
  }
  if (navHit(NAV_L, x, y)) {  // ko11.17
    if (bgmPage > 0) bgmPage--;
    else xScreen = XS_VOL;
    sfxPlay(SFX_TAP);
    return;
  }
  if (pages > 1 && navHit(NAV_R, x, y)) { bgmPage = (bgmPage + 1) % pages; sfxPlay(SFX_TAP); return; }
  if (y < BGMP_Y0 - 4) return;
  uint8_t r = (y - (BGMP_Y0 - 4)) / BGMP_H;
  uint8_t k = bgmPage * BGMP_ROWS + r;
  if (r >= BGMP_ROWS || k >= n) return;
  uint8_t i = list[k];
  if (x >= 58 && x < 130) {  // [켬]/[끔]
    uint8_t m = audioBgmMask() ^ (uint8_t)(1u << i);
    if (!(m & audioBgmAvail())) {  // no dejar todas apagadas
      bgmWarnUntil = millis() + 2500;
      sfxPlay(SFX_DENY);
      return;
    }
    audioSetBgmMask(m);
    sfxPlay(SFX_TAP);
    return;
  }
  if (x >= 130 && x < 410) audioBgmPlay(i);  // nombre o [>]: escucharla ya
}

// ======================================================================
// ko5: actualizar el firmware desde /update.bin de la SD (pantalla de red)
// ======================================================================

UpdCheck updState = UPD_NONE;
uint32_t updSize = 0;
char updVer[24] = "";  // ko6.2: version dentro de update.bin ("" si no lleva marca)
int8_t updResult = 0;  // 0 nada, 1 hecho (reinicia), -1 fallo

void openUpdate() {
  if (netPortalOn()) netStopPortal();
  updResult = 0;
  updState = sdUpdateCheck(&updSize);
  updVer[0] = 0;
  if (updState == UPD_OK) sdUpdateFileVersion(updVer, sizeof(updVer));
  xScreen = XS_UPD;
  sfxPlay(SFX_TAP);
}

static void updScreenBase() {
  gfx->fillScreen(UI_BG_DAY);  // ko11.13: liso, que no frene la escritura  // ko11.6.1: sin pasar por negro (parpadeo)
  drawFit(XT(X_UPD_TITLE), 48, 300, UI_INK, 3);
}

static void updProgress(uint32_t done, uint32_t total) {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (done < total && now - last < 250) return;  // no frenar la escritura
  last = now;
  updScreenBase();
  drawFit(XT(X_UPD_WRITING), 170, 360, UI_INK, 2);
  int w = 300, fw = (int)((uint64_t)(w - 4) * done / (total ? total : 1));
  gfx->fillRoundRect(CX - w / 2, 214, w, 24, 8, UI_TRACK);
  if (fw > 0) gfx->fillRoundRect(CX - w / 2 + 2, 216, fw, 20, 7, UI_BAR_OK);
  char pc[8];
  snprintf(pc, sizeof(pc), "%u%%", (unsigned)((uint64_t)done * 100 / (total ? total : 1)));
  drawFit(pc, 254, 200, UI_INK, 2);
  gfx->flush();  // ko11.13: sin fundido mientras escribe
}

void renderUpdate() {
  updScreenBase();
  char ver[40];
  snprintf(ver, sizeof(ver), XT(X_UPD_CUR_FMT), FW_VERSION);
  drawFit(ver, 92, 300, UI_INK, 2);
  if (updResult > 0) {
    drawFit(XT(X_UPD_DONE), 200, 360, UI_BAR_OK, 3);
  } else if (updResult < 0) {
    drawFit(XT(X_UPD_FAIL), 180, 360, UI_BAR_BAD, 3);
    drawBtn(133, 330, 200, 48, UI_TRACK, UI_INK, T(S_BACK));
  } else if (updState == UPD_OK) {
    char l[40];
    // ko6.2: la version que trae el fichero (si no se sabe, solo el tamano)
    if (updVer[0]) {
      snprintf(l, sizeof(l), XT(X_UPD_FILE_FMT), updVer);
      bool same = !strcmp(updVer, FW_VERSION);
      drawFit(l, 140, 340, same ? UI_BAR_WARN : UI_BAR_OK, 2);
      if (same) drawFit(XT(X_UPD_SAME), 200, 340, UI_BAR_WARN, 2);
    }
    snprintf(l, sizeof(l), XT(X_UPD_SIZE_FMT), (unsigned)(updSize / 1024));
    drawFit(l, 172, 340, UI_INK, 1);
    drawBtn(88, 250, 140, 52, UI_BAR_OK, UI_WHITE, XT(X_UPD_GO));
    drawBtn(238, 250, 140, 52, UI_TRACK, UI_INK, XT(X_UPD_CANCEL));
  } else {
    drawFit(XT(updState == UPD_FULLIMG ? X_UPD_FULLIMG : X_UPD_NOFILE), 170, 360, UI_BAR_BAD, 2);
    drawFit(XT(X_UPD_HINT), 206, 380, UI_INK, 1);
    drawBtn(133, 330, 200, 48, UI_TRACK, UI_INK, T(S_BACK));
  }
  if (updResult == 0) drawNav(NAV_L, UI_INK);  // ko11.17: volver a la red
  uiFlush();
}

void updateTap(int16_t x, int16_t y) {
  if (updResult > 0) return;  // ya reiniciando
  if (updResult == 0 && updState == UPD_OK) {
    if (y >= 250 && y < 302 && x >= 88 && x < 228) {  // actualizar
      pet.saveNow();  // el estado queda guardado antes de reiniciar
      updProgress(0, 1);
      bool ok = sdUpdateRun(updProgress);
      updResult = ok ? 1 : -1;
      sfxPlay(ok ? SFX_MEDAL : SFX_DENY);
      renderUpdate();
      if (ok) { delay(1500); ESP.restart(); }
      return;
    }
    if ((y >= 250 && y < 302 && x >= 238 && x < 378) || navHit(NAV_L, x, y)) { xScreen = XS_SET; return; }
    return;
  }
  if (y >= 320 || y < 72 || navHit(NAV_L, x, y)) xScreen = XS_SET;  // ko11.26: [<] = a ajustes
}

// ======================================================================
// ko8: [nuevo comienzo]. Borra la partida entera (mascota, pokedex, caja,
// medallas, records) y conserva WiFi, sonido e idioma. Para que no se haga
// sin querer: pantalla propia y un boton que hay que mantener 3 s.
// ======================================================================
#define RST_BTN_Y 286
#define RST_BTN_R 66
#define RST_HOLD_MS 3000UL
bool rstHint = false;
bool rstDone = false;

void openReset() {
  retMark();
  clockOpen = false;
  rstHint = false;
  rstDone = false;
  xScreen = XS_RESET;
  sfxPlay(SFX_TAP);
}

static bool rstInButton(int x, int y) {
  int dx = x - CX, dy = y - RST_BTN_Y;
  return dx * dx + dy * dy <= (RST_BTN_R + 10) * (RST_BTN_R + 10);
}

void doResetGame() {
  rstDone = true;
  renderReset();  // "empezando de nuevo..." antes de reiniciar
  pet.wipeGameKeepSettings();
  box.wipe();
  hall.wipe();  // ko10.5
  memStore.wipe();  // ko12.4
  fame.wipe();  // ko10.11
  dexLog.wipe();
  // ko11.22: tambien la historia/expedicion, los usos de ayudantes y las fichas del salon de la liga
  static const char *const NS_SMALL[] = { "tpstory", "tpparty" };
  for (const char *ns : NS_SMALL) { Preferences p; if (p.begin(ns, false)) p.clear(); p.end(); }
  { Preferences p; if (p.begin("tpteam", false, bigPart())) p.clear(); p.end(); }
  sfxPlay(SFX_BYE);
  delay(1200);
  ESP.restart();
}

void renderReset() {
  uiScreenBg();  // ko11.6.1: sin pasar por negro (parpadeo)
  drawFit(XT(X_RESET_TITLE), 40, 300, UI_INK, 3);
  if (rstDone) {
    drawFit(XT(X_RESET_DONE), 220, 320, UI_INK, 3);
    uiFlush();
    return;
  }
  drawFit(XT(X_RESET_L1), 96, 340, UI_INK, 2);
  drawFit(XT(X_RESET_L2), 124, 340, UI_BAR_BAD, 2);
  drawFit(XT(X_RESET_L3), 160, 340, UI_INK, 2);
  // progreso de la pulsacion (dedo apoyado dentro del boton)
  float p = 0;
  if (wasPressed && rstInButton(tX0, tY0) && rstInButton(tXl, tYl)) {
    p = (float)(millis() - tStart) / RST_HOLD_MS;
    if (p >= 1.0f) { doResetGame(); return; }
  }
  gfx->fillCircle(CX, RST_BTN_Y, RST_BTN_R + 12, UI_TRACK);
  // anillo que se llena en el sentido de las agujas del reloj
  int segs = (int)(p * 60);
  for (int i = 0; i < segs; i++) {
    float a = -1.5708f + i * 6.2832f / 60;
    gfx->fillCircle(CX + (int)(cosf(a) * (RST_BTN_R + 6)), RST_BTN_Y + (int)(sinf(a) * (RST_BTN_R + 6)), 6,
                    UI_BAR_BAD);
  }
  gfx->fillCircle(CX, RST_BTN_Y, RST_BTN_R, p > 0 ? C565(0xb8, 0x28, 0x20) : UI_BAR_BAD);
  drawFit(XT(X_RESET_HOLD), RST_BTN_Y - 12, 2 * RST_BTN_R - 10, UI_WHITE, 2);
  if (rstHint) drawFit(XT(X_RESET_HINT), 196, 340, UI_INK, 2);
  drawBtn(163, 372, 140, 40, UI_WHITE, UI_INK, XT(X_UPD_CANCEL));
  drawNav(NAV_L, UI_INK);  // ko11.17: volver a la hora
  uiFlush();
}

void resetTap(int16_t x, int16_t y) {
  if (rstInButton(x, y)) { rstHint = true; sfxPlay(SFX_DENY); return; }  // toque corto: no basta
  if (y >= 364 || y < 72 || navHit(NAV_L, x, y)) {  // cancelar -> vuelve a la hora
    sfxPlay(SFX_TAP);
    goBack();
  }
}

// ko10.11: "que pantalla hay" (para el antirrebote de navegacion en handleTouch)
uint16_t screenSig() {
  return (uint16_t)xScreen | (cardOpen ? 1u << 8 : 0) | (galleryOpen ? 1u << 9 : 0) |
         (trainMenuOpen ? 1u << 10 : 0) | (clockOpen ? 1u << 11 : 0) | (kbOpen ? 1u << 12 : 0) |
         (galleryDetail ? 1u << 13 : 0) | (gameOpen || sackOpen || trainingFast() ? 1u << 14 : 0);
}

// ======================================================================
// ko10.11: bolsa de caramelos. Todas las familias con caramelos, el universal,
// y al tocar: cambiar 3 de otra familia por 1 de la que crias, o usar
// ======================================================================
#define BAG_ROW_X 70
#define BAG_ROW_W 326
#define BAG_ROW_Y 132
#define BAG_ROW_H 40
#define BAG_GAP 6
#define BAG_ROWS 5
#define BAG_RARE_Y 80
#define BAG_SHARD_BTN_Y 364  // ko11.15.1
static uint8_t bagFam[PET_DEX_MAX];
static uint8_t bagN = 0, bagPage = 0;
static int16_t bagSel = -1;        // -1 nada, 0 = universal, si no familia (dex base)
static const char *bagMsg = nullptr;
static uint32_t bagMsgUntil = 0;

static void bagBuild() {
  bagN = 0;
  uint8_t mine = pet.isEgg() ? 0 : DEX_FAM[pet.speciesId];
  if (mine && pet.candy[mine]) bagFam[bagN++] = mine;  // la tuya, la primera
  for (int16_t f = 1; f <= DEX_COUNT; f++)
    if (f != mine && DEX_FAM[f] == f && pet.candy[f]) bagFam[bagN++] = (uint8_t)f;
}
static uint8_t bagPages() { return bagN ? (uint8_t)((bagN + BAG_ROWS - 1) / BAG_ROWS) : 1; }

static bool bagFromBox = false;  // ko11.26: abierta desde la pestana de la caja
void openCandyBag() {
  bagFromBox = xScreen == XS_BOX;
  retMark();
  xScreen = XS_CANDY;
  cardOpen = false;
  bagBuild();
  bagPage = 0;
  bagSel = -1;
  bagMsgUntil = 0;
  sfxPlay(SFX_TAP);
}

void candyBagClose() {
  if (bagFromBox) { bagFromBox = false; xScreen = XS_BOX; navGuardUntil = millis() + 300; }
  else goBack();
}

bool candyBagSwipe(int dir) {
  if (xScreen != XS_CANDY) return false;
  if (bagSel >= 0) return true;
  int p = (int)bagPage + (dir > 0 ? -1 : 1);
  if (p >= 0 && p < bagPages()) { bagPage = (uint8_t)p; sfxPlay(SFX_TAP); }
  return true;
}

static void bagPopupRect(int &x, int &y, int &w, int &h) { x = 58; y = 150; w = 350; h = 196; }

void renderCandyBag() {
  screenBase();
  drawFit(XT(X_BAG_TITLE), 36, 300, UI_INK, 3);
  char b[48];
  // ko11.15.1: caramelo raro a la izquierda y los trozos (barra) a la derecha
  {
    uint16_t rc = pet.rareCandy ? C565(0xd8, 0xa8, 0x20) : UI_TRACK;
    uint16_t tc = pet.rareCandy ? UI_WHITE : 0x8410;
    uiButton(BAG_ROW_X, BAG_RARE_Y, BAG_ROW_W, BAG_ROW_H, 12, rc, UI_INK);
    snprintf(b, sizeof(b), XT(X_BAG_RARE_FMT), pet.rareCandy);
    setSize(2);
    gfx->setTextColor(tc);
    setCur(BAG_ROW_X + 14, BAG_RARE_Y + 9);
    printT(b);
    snprintf(b, sizeof(b), XT(X_BAG_SHARD_FMT), pet.rareShards, SHARDS_PER_RARE);
    setSize(1);
    setCur(BAG_ROW_X + BAG_ROW_W - 116, BAG_RARE_Y + 4);
    printT(b);
    uiGauge(BAG_ROW_X + BAG_ROW_W - 116, BAG_RARE_Y + 22, 104, 10, pet.rareShards * 1000 / SHARDS_PER_RARE,
            C565(0xf8, 0xd8, 0x50), lerp565(rc, UI_WHITE, 8, 16));
  }
  uint8_t mine = pet.isEgg() ? 0 : DEX_FAM[pet.speciesId];
  bool leftovers = false;  // hay caramelos de otras familias
  for (int k = 0; k < bagN; k++) if (bagFam[k] != mine) { leftovers = true; break; }
  if (!bagN) drawFit(XT(X_BAG_EMPTY), BAG_ROW_Y + 60, 300, 0x8410, 2);
  for (int r = 0; r < BAG_ROWS; r++) {
    int k = bagPage * BAG_ROWS + r;
    if (k >= bagN) break;
    uint8_t f = bagFam[k];
    int y = BAG_ROW_Y + r * (BAG_ROW_H + BAG_GAP);
    bool isMine = f == mine;
    uiButton(BAG_ROW_X, y, BAG_ROW_W, BAG_ROW_H, 12, isMine ? C565(0xf0, 0x7a, 0xa8) : UI_WHITE, UI_INK);
    const uint8_t *th = thumbs.get(f);
    if (th) drawThumb(th, BAG_ROW_X + 8, y - 14, 1, false);
    snprintf(b, sizeof(b), XT(X_BAG_ROW_FMT), dexName(f), pet.candy[f]);
    gfx->setTextColor(isMine ? UI_WHITE : UI_INK);
    setSize(2);
    setCur(BAG_ROW_X + 54, y + 9);
    printT(b);
    if (isMine) {
      setSize(1);
      setCur(BAG_ROW_X + BAG_ROW_W - 44, y + 13);
      printT(XT(X_BAG_MINE));
    }
  }
  // ko11.15.1: boton "sobrantes -> trozos"
  if (leftovers && !pet.isEgg())
    drawBtn(CX - 125, BAG_SHARD_BTN_Y, 250, 34, C565(0xd8, 0xa8, 0x20), UI_WHITE, XT(X_BAG_TO_SHARDS));
  if (bagMsg && timeLeft(bagMsgUntil)) drawFit(bagMsg, 408, 300, UI_BAR_OK, 2);
  else if (bagPages() > 1) {
    snprintf(b, sizeof(b), XT(X_BAG_PAGE_FMT), bagPage + 1, bagPages());
    drawFit(b, 408, 200, UI_INK, 1);
  }
  drawNav(NAV_L, UI_INK);  // ko11.17: pagina anterior, o volver en la 1a
  if (bagPage + 1 < bagPages()) drawNav(NAV_R, UI_INK);
  drawNav(NAV_DOWN, UI_INK);
  // ventana de la seleccion
  if (bagSel >= 0) {
    int x, y, w, h;
    bagPopupRect(x, y, w, h);
    uiButton(x, y, w, h, 16, UI_WHITE, UI_INK);
    gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 15, UI_INK);
    if (bagSel == 0) {
      snprintf(b, sizeof(b), XT(X_BAG_RARE_FMT), pet.rareCandy);
      drawFit(b, y + 14, w - 20, C565(0xb0, 0x80, 0x10), 2);
      drawFit(XT(X_BAG_RARE_INFO), y + 50, w - 20, UI_INK, 2);
      bool ok = pet.rareCandy && !pet.isEgg();
      drawBtn(x + 20, y + 96, w - 40, 40, ok ? C565(0xd8, 0xa8, 0x20) : UI_TRACK, ok ? UI_WHITE : 0x8410, XT(X_BAG_USE));
    } else {
      uint8_t f = (uint8_t)bagSel;
      snprintf(b, sizeof(b), XT(X_BAG_ROW_FMT), dexName(f), pet.candy[f]);
      drawFit(b, y + 14, w - 20, UI_INK, 2);
      if (f == mine) {
        drawFit(XT(X_BAG_MINE_INFO), y + 50, w - 20, UI_INK, 2);
        drawBtn(x + 20, y + 96, w - 40, 40, C565(0xf0, 0x7a, 0xa8), UI_WHITE, XT(X_BAG_USE_GO));
      } else {
        drawFit(pet.isEgg() ? XT(X_BAG_NOEGG) : XT(X_BAG_TRADE_INFO), y + 50, w - 20, UI_INK, 2);
        uint16_t all = pet.candy[f] / CANDY_TRADE_RATE;
        bool ok = all && !pet.isEgg();
        char a[24];
        snprintf(a, sizeof(a), XT(X_BAG_TRADEALL_FMT), all);
        drawBtn(x + 20, y + 96, (w - 50) / 2, 40, ok ? C565(0xf0, 0x7a, 0xa8) : UI_TRACK, ok ? UI_WHITE : 0x8410,
                XT(X_BAG_TRADE1));
        drawBtn(x + 30 + (w - 50) / 2, y + 96, (w - 50) / 2, 40, ok ? C565(0xc8, 0x3c, 0x78) : UI_TRACK,
                ok ? UI_WHITE : 0x8410, a);
      }
    }
    drawBtn(x + 20, y + 144, w - 40, 36, UI_TRACK, UI_INK, XT(X_BAG_CLOSE));
    if (bagMsg && timeLeft(bagMsgUntil)) drawFit(bagMsg, y - 30, 300, UI_BAR_OK, 2);
  }
  uiFlush();
}

static void bagSay(const char *m, bool good) {
  bagMsg = m;
  bagMsgUntil = millis() + 1800;
  sfxPlay(good ? SFX_HEART : SFX_DENY);
}

void candyBagTap(int16_t x, int16_t y) {
  if (bagSel >= 0) {  // ventana abierta
    int px, py, pw, ph;
    bagPopupRect(px, py, pw, ph);
    if (inRect(x, y, px + 20, py + 144, pw - 40, 36) || !inRect(x, y, px, py, pw, ph)) { bagSel = -1; sfxPlay(SFX_TAP); return; }
    if (y < py + 96 || y >= py + 136) return;
    uint8_t mine = pet.isEgg() ? 0 : DEX_FAM[pet.speciesId];
    if (bagSel == 0) {
      if (pet.useRareCandy()) { bagBuild(); bagSay(XT(X_BAG_DONE), true); }
      else bagSay(pet.isEgg() ? XT(X_BAG_NOEGG) : XT(X_CANDY_NO), false);
      return;
    }
    uint8_t f = (uint8_t)bagSel;
    if (f == mine) {  // usar: a la pagina de caramelos de la ficha
      xScreen = XS_NONE;
      cardOpen = true;
      cardPage = 4;
      sfxPlay(SFX_TAP);
      return;
    }
    bool left = x < px + pw / 2;
    uint16_t times = left ? 1 : pet.candy[f] / CANDY_TRADE_RATE;
    if (pet.candyTrade(f, times)) {
      bagBuild();
      if (!pet.candy[f]) bagSel = -1;
      bagSay(XT(X_BAG_DONE), true);
    } else {
      bagSay(pet.isEgg() ? XT(X_BAG_NOEGG) : XT(X_CANDY_NO), false);
    }
    return;
  }
  if (navHit(NAV_DOWN, x, y)) { sfxPlay(SFX_TAP); candyBagClose(); return; }
  if (navHit(NAV_L, x, y)) { if (bagPage > 0) candyBagSwipe(1); else { sfxPlay(SFX_TAP); candyBagClose(); } return; }  // ko11.17
  if (navHit(NAV_R, x, y)) { candyBagSwipe(-1); return; }
  if (inRect(x, y, BAG_ROW_X, BAG_RARE_Y, BAG_ROW_W, BAG_ROW_H)) { bagSel = 0; sfxPlay(SFX_TAP); return; }
  if (inRect(x, y, CX - 125, BAG_SHARD_BTN_Y, 250, 34)) {  // ko11.15.1: sobrantes -> trozos
    static char msg[40];
    uint16_t n = pet.candyToShards();
    if (n) {
      snprintf(msg, sizeof(msg), XT(X_BAG_SHARDED_FMT), n);
      bagBuild();
      bagPage = 0;
      bagSay(msg, true);
    } else bagSay(XT(X_BAG_NO_LEFT), false);
    return;
  }
  if (x < BAG_ROW_X || x >= BAG_ROW_X + BAG_ROW_W || y < BAG_ROW_Y) return;
  int r = (y - BAG_ROW_Y) / (BAG_ROW_H + BAG_GAP);
  if (r >= BAG_ROWS || (y - BAG_ROW_Y) % (BAG_ROW_H + BAG_GAP) >= BAG_ROW_H) return;
  int k = bagPage * BAG_ROWS + r;
  if (k >= bagN) return;
  bagSel = bagFam[k];
  sfxPlay(SFX_TAP);
}

// ======================================================================
// ko11.1: salon de la fama de la liga. Rejilla (el mas reciente primero) con
// corona; al tocar uno, su ficha: sprite grande con corona, fecha y genes
// ======================================================================
#define FM_COLS 3
#define FM_ROWS 3
#define FM_CELL 96
#define FM_X (CX - FM_COLS * FM_CELL / 2)
#define FM_Y 92
static uint8_t famePage = 0;
static int16_t fameSel = -1;  // indice en fame (0 = el mas antiguo), -1 = rejilla

static uint8_t famePages() {
  uint8_t n = fame.count();
  return n ? (uint8_t)((n + FM_COLS * FM_ROWS - 1) / (FM_COLS * FM_ROWS)) : 1;
}

// corona dorada grande (centrada en cx, con la base en y)
// ko11.20: silver = solo gano en equipo (corona de plata)
static void drawCrownBig(int cx, int y, int w, bool silver = false) {
  uint16_t gold = silver ? C565(0xd4, 0xda, 0xe4) : C565(0xf0, 0xc0, 0x30);
  uint16_t dark = silver ? C565(0x78, 0x80, 0x90) : C565(0xa0, 0x70, 0x10);
  uint16_t red = silver ? C565(0x40, 0x80, 0xe0) : C565(0xe0, 0x30, 0x40);
  int h = w / 2, x = cx - w / 2;
  gfx->fillRect(x, y - h / 3, w, h / 3, gold);
  gfx->fillTriangle(x, y - h / 3, x + w / 6, y - h, x + w / 3, y - h / 3, gold);
  gfx->fillTriangle(x + w / 3, y - h / 3, cx, y - h - h / 4, x + 2 * w / 3, y - h / 3, gold);
  gfx->fillTriangle(x + 2 * w / 3, y - h / 3, x + 5 * w / 6, y - h, x + w, y - h / 3, gold);
  gfx->drawRect(x, y - h / 3, w, h / 3, dark);
  gfx->fillCircle(cx, y - h / 6, h / 8 + 1, red);
  gfx->fillCircle(x + w / 6, y - h, h / 10 + 1, red);
  gfx->fillCircle(x + 5 * w / 6, y - h, h / 10 + 1, red);
  gfx->fillCircle(cx, y - h - h / 4, h / 10 + 1, red);
}

void openFame() {
  xScreen = XS_FAME;
  famePage = 0;
  fameSel = -1;
  sfxPlay(SFX_TAP);
}

PmdMon fameHelpPmd[PARTY_HELPERS];  // ko11.20: los ayudantes de una victoria en equipo
static void fameHelpUnload() { for (auto &h : fameHelpPmd) h.unload(); }
void fameClose() {
  fameHelpUnload();
  if (fameSel >= 0) { fameSel = -1; galleryPmd.unload(); return; }
  galleryPmd.unload();
  xScreen = XS_GYM;  // vuelve a la pagina de la liga
  gymPage = 2;
}

bool fameSwipe(int dir) {
  if (xScreen != XS_FAME) return false;
  if (fameSel >= 0) { fameClose(); return true; }
  int p = (int)famePage + (dir > 0 ? -1 : 1);
  if (p >= 0 && p < famePages()) { famePage = (uint8_t)p; sfxPlay(SFX_TAP); }
  return true;
}

static void fameDetail() {
  const BoxMon &m = fame.at((uint8_t)fameSel);
  drawFit(XT(X_FAME_TITLE), 30, 300, C565(0xb0, 0x80, 0x10), 2);
  const int ground = 250;
  int top = ground - 120;
  // ko11.17: el campeon luce su tipo: halo del color del tipo que late, ondas que
  // salen y su tecnica estallando detras (cada 2 s), con destellos delante
  uint32_t now = millis();
  uint8_t pt = DEX_TBL[m.dex].ptype;
  uint16_t tc = orbColor(pt);
  const int acx = CX, acy = ground - 72;
  float pl = 0.5f + 0.5f * sinf(now * 0.004f);
  gfx->fillCircle(acx, acy, 84 + (int)(pl * 6), uiLerp(tc, UI_BG_DAY, 12, 16));
  gfx->fillCircle(acx, acy, 64, uiLerp(tc, UI_BG_DAY, 9, 16));
  for (int k = 0; k < 3; k++) {  // ondas
    int ph = (int)((now / 12 + k * 40) % 120);
    uint16_t c = uiLerp(tc, UI_BG_DAY, 6 + ph / 12, 16);
    gfx->drawCircle(acx, acy, 60 + ph / 2, c);
    gfx->drawCircle(acx, acy, 61 + ph / 2, c);
  }
  uint32_t cyc = now % 2000;
  if (cyc < 1000) drawMoveFx(pt, acx, acy, acx, acy, 350 + cyc * 650 / 1000, true, 2, moveTier(m.dex), moveVarFor(m.dex, m.lvl));
  FameRec fr = fameRecOf(m);
  if (fr.team && fr.help[0]) {  // ko11.20: los ayudantes (de la ultima en equipo) a los lados, algo mas pequenos, con su luz
    const FameRec *t = &fr;
    static const int HX[PARTY_HELPERS] = { CX - 142, CX + 142 };
    for (uint8_t k = 0; t && k < PARTY_HELPERS; k++) {
      int16_t hd = t->help[k];
      if (hd < 1 || hd > DEX_COUNT) continue;
      uint16_t hc = orbColor(DEX_TBL[hd].ptype);
      int hy = ground - 50;
      float hp = 0.5f + 0.5f * sinf(now * 0.004f + 1.5f + k * 1.5f);
      gfx->fillCircle(HX[k], hy, 46 + (int)(hp * 4), uiLerp(hc, UI_BG_DAY, 12, 16));
      gfx->fillCircle(HX[k], hy, 34, uiLerp(hc, UI_BG_DAY, 9, 16));
      int ph = (int)((now / 14 + k * 50) % 80);
      gfx->drawCircle(HX[k], hy, 34 + ph / 2, uiLerp(hc, UI_BG_DAY, 6 + ph / 8, 16));
      if (fameHelpPmd[k].loaded && fameHelpPmd[k].acts[PMD_IDLE].frames)
        drawPmdActM(fameHelpPmd[k], PMD_IDLE, HX[k], ground - 10, 0, true, false, 4, 110);
      else drawThumbAt(hd, HX[k], hy, 2, false);
      for (int q = 0; q < 3; q++) {  // destellos pequenos
        float a = now * 0.002f + q * 2.09f + k;
        int sx = HX[k] + (int)(cosf(a) * 44), sy = hy + (int)(sinf(a) * 36);
        gfx->drawFastHLine(sx - 3, sy, 7, UI_WHITE);
        gfx->drawFastVLine(sx, sy - 3, 7, UI_WHITE);
      }
    }
  }
  if (galleryPmd.loaded && galleryPmd.acts[PMD_IDLE].frames) {
    // ko11.1: retrato grande y quieto (frame 0) para que la corona quede justo en la cabeza
    const PmdAct &ia = galleryPmd.acts[PMD_IDLE];
    uint8_t sc = ia.h ? 210 / ia.h : 4;
    sc = sc < 2 ? 2 : sc > 6 ? 6 : sc;
    drawPmdActM(galleryPmd, PMD_IDLE, CX, ground, 0, true, false, 6, 210);
    int r0 = 0;
    for (; r0 < ia.h; r0++) {
      const uint8_t *row = ia.data + r0 * ia.w;
      bool hit = false;
      for (int c = 0; c < ia.w && !hit; c++) hit = row[c] != 0xFF;
      if (hit) break;
    }
    top = ground - ((ia.base ? ia.base : ia.h) - r0) * sc;
  } else {
    const uint8_t *th = thumbs.get(m.dex);
    if (th) drawThumb(th, CX - 60, ground - 120, 3, false);
  }
  if (top < 96) top = 96;
  drawCrownBig(CX, top + 4, 64, m.flags & BOXF_TEAM);
  {  // ko11.20: victorias en solitario y en equipo
    FameRec r = fameRecOf(m);
    char tl[40];
    snprintf(tl, sizeof(tl), XT(X_FAME_COUNT_FMT), (unsigned)r.solo, (unsigned)r.team);
    int w = textW(tl, 1) + 28;
    drawBtn(CX - w / 2, 54, w, 26, r.solo ? C565(0xd0, 0x98, 0x20) : 0x4C98, UI_WHITE, tl);
  }
  for (int k = 0; k < 6; k++) {  // destellos alrededor
    float a = now * 0.0015f + k * 1.047f;
    int sx = acx + (int)(cosf(a) * 100), sy = acy + (int)(sinf(a) * 70);
    int l = 3 + (int)((now / 90 + k * 3) % 4);
    gfx->drawFastHLine(sx - l, sy, 2 * l + 1, UI_WHITE);
    gfx->drawFastVLine(sx, sy - l, 2 * l + 1, UI_WHITE);
    gfx->drawFastHLine(sx - l / 2, sy, l + 1, uiLerp(tc, UI_WHITE, 8, 16));
  }
  char b[64];
  snprintf(b, sizeof(b), "%s%s  Lv%u", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex), m.lvl);
  drawFit(b, 262, 340, UI_INK, 3);
  char when[32];
  when[0] = 0;
  if (m.epoch) {
    int yy;
    uint8_t mo, dd;
    wxDate(m.epoch, &yy, &mo, &dd, nullptr);
    snprintf(when, sizeof(when), XT(X_FAME_DATE_FMT), (unsigned)yy, mo, dd);
  }
  snprintf(b, sizeof(b), XT(X_FAME_NTH_FMT), (unsigned)(fameSel + 1));
  uint8_t stk = fameSel < (int)sizeof(pet.fameStreak) ? pet.fameStreak[fameSel] : 0;
  if (stk) {  // ko11.6.1: con cuantas seguidas llego aqui
    size_t l = strlen(b);
    snprintf(b + l, sizeof(b) - l, "  ");
    l = strlen(b);
    snprintf(b + l, sizeof(b) - l, XT(X_FAME_STREAK_FMT), (unsigned)stk);
  }
  drawFit(b, 302, 340, C565(0xb0, 0x80, 0x10), 2);
  if (when[0]) drawFit(when, 330, 300, C565(0xb0, 0x80, 0x10), 2);
  if (m.geneAtk) {
    snprintf(b, sizeof(b), XT(X_FAME_GENES_FMT), m.geneAtk, m.geneDef, m.geneSpe);
    drawFit(b, 358, 320, 0x8410, 2);
  }
  drawFit(XT(X_FAME_TAP), 392, 240, UI_INK, 1);
  drawNav(NAV_DOWN, UI_INK);
}

void renderFame() {
  screenBase();
  if (fameSel >= 0 && fameSel < fame.count()) { fameDetail(); uiFlush(); return; }
  char t[40];
  snprintf(t, sizeof(t), XT(X_FAME_BTN_FMT), fame.count());
  drawFit(t, 36, 320, C565(0xb0, 0x80, 0x10), 3);
  uint8_t n = fame.count();
  for (int k = 0; k < FM_COLS * FM_ROWS; k++) {
    int idx = famePage * FM_COLS * FM_ROWS + k;
    if (idx >= n) break;
    const BoxMon &m = fame.at((uint8_t)(n - 1 - idx));  // el mas reciente primero
    int x = FM_X + (k % FM_COLS) * FM_CELL, y = FM_Y + (k / FM_COLS) * FM_CELL;
    uiButton(x + 4, y + 4, FM_CELL - 8, FM_CELL - 8, 14, (m.flags & BOXF_SHINY) ? C565(0xff, 0xf0, 0xc0) : UI_WHITE, C565(0xb0, 0x80, 0x10));
    const uint8_t *th = thumbs.get(m.dex);
    if (th) drawThumb(th, x + 8, y + 14, 2, false);
    drawCrownBig(x + FM_CELL / 2, y + 22, 26, m.flags & BOXF_TEAM);
    FameRec fr = fameRecOf(m);  // ko11.20: solo / equipo / los dos
    const char *bt = XT(fr.solo && fr.team ? X_FAME_BOTH : fr.solo ? X_FAME_SOLO : X_FAME_TEAM);
    int bw = textW(bt, 1) + 14;
    drawBtn(x + FM_CELL - 6 - bw, y + FM_CELL - 30, bw, 22, fr.solo ? C565(0xd0, 0x98, 0x20) : 0x4C98, UI_WHITE, bt);
  }
  if (famePages() > 1) {
    snprintf(t, sizeof(t), XT(X_BAG_PAGE_FMT), famePage + 1, famePages());
    drawFit(t, 392, 200, UI_INK, 1);
  }
  drawNav(NAV_L, UI_INK);  // ko11.17: pagina anterior, o volver a la liga en la 1a
  if (famePage + 1 < famePages()) drawNav(NAV_R, UI_INK);
  drawNav(NAV_DOWN, UI_INK);
  uiFlush();
}

void fameTap(int16_t x, int16_t y) {
  if (fameSel >= 0) { fameClose(); sfxPlay(SFX_TAP); return; }  // la ficha: cualquier toque vuelve
  if (navHit(NAV_DOWN, x, y)) { fameClose(); sfxPlay(SFX_TAP); return; }
  if (navHit(NAV_L, x, y)) { if (famePage > 0) fameSwipe(1); else { fameClose(); sfxPlay(SFX_TAP); } return; }
  if (navHit(NAV_R, x, y)) { fameSwipe(-1); return; }
  if (x < FM_X || y < FM_Y) return;
  int c = (x - FM_X) / FM_CELL, r = (y - FM_Y) / FM_CELL;
  if (c >= FM_COLS || r >= FM_ROWS) return;
  int idx = famePage * FM_COLS * FM_ROWS + r * FM_COLS + c;
  uint8_t n = fame.count();
  if (idx >= n) return;
  fameSel = (int16_t)(n - 1 - idx);
  const BoxMon &m = fame.at((uint8_t)fameSel);
  galleryPmd.load((uint8_t)m.dex, m.flags & BOXF_SHINY);
  fameHelpUnload();
  FameRec fr = fameRecOf(m);
  if (fr.team && fr.help[0]) {  // ko11.20
    const FameRec *t = &fr;
    for (uint8_t k = 0; t && k < PARTY_HELPERS; k++)
      if (t->help[k] >= 1) fameHelpPmd[k].load((uint8_t)t->help[k], (t->shiny >> k) & 1);
  }
  audioCry(m.dex);
}

// ======================================================================
// ko11.6: copia de la partida en la SD (savebak.h)
// Automatica: una al dia y en los momentos importantes (evolucion / nuevo
// companero, campeon, despedida). Manual: red > [copia]. Al arrancar con una
// partida nueva y una copia en la SD, se pregunta si restaurarla.
// ======================================================================

BakSlot bakSlots[2];
int8_t bakSel = -1;
bool bakCrashView = false;  // ko11.9.2: ventana del registro de reinicios          // ranura elegida para restaurar (confirmacion)
int16_t bakMsg = -1;         // XId del ultimo aviso (-1 nada). ko11.19.1: int16 (con int8 X_BAK_DONE=311 se leia como X_CANT_NOW)
uint32_t bakMsgUntil = 0;
bool bakAsk = false;         // pregunta al arrancar
int8_t bakAskSlot = -1;
static bool bakPending = false;
static uint32_t bakLastT = 0;
static int32_t bakKnownDay = -2;  // dia de la copia AUTOMATICA mas nueva (-1 ninguna, -2 sin mirar)

void bakRequest() { bakPending = true; }

static void bakSetMsg(XId m) { bakMsg = (int16_t)m; bakMsgUntil = millis() + 3000; }

static bool bakDoBackup(bool manual = false) {
  if (!sdReady) { bakSetMsg(X_BAK_NOSD); return false; }
  pet.saveNow();  // lo ultimo tambien
  uint32_t e = clockEpoch();
  bool ok = bakBackupNow(pet.speciesId, pet.level(), gClockTrusted ? e : 0, manual);
  bakLastT = millis() ? millis() : 1;
  bakPending = false;
  if (ok && gClockTrusted && !manual) bakKnownDay = (int32_t)(e / 86400);  // ko11.21.1: la manual no cuenta
  return ok;
}

// en loop(): la copia automatica, solo con la placa tranquila
void bakAutoLoop(uint32_t now) {
  static int16_t lastSp = -32000;
  if (lastSp == -32000) lastSp = pet.speciesId;
  if (pet.speciesId != lastSp) {  // nacio / evoluciono / nuevo companero
    if (pet.speciesId >= 1) bakPending = true;
    lastSp = pet.speciesId;
  }
  static uint32_t lastCheck = 0;
  if (now < 60000) return;  // el primer minuto, a lo suyo
  if (!bakPending && lastCheck && now - lastCheck < 60000) return;
  if (!sdReady || safeMode || bakAsk || pet.awaitingStarter() || fastGameNow() || screenOff) return;
  if (xScreen == XS_WILD || xScreen == XS_LINK || xScreen == XS_UPD) return;  // en plena batalla / actualizacion
  if (bakLastT && now - bakLastT < 60000) return;
  lastCheck = now;
  bool due = bakPending;
  if (!due && gClockTrusted) {  // una al dia
    if (bakKnownDay == -2) {
      BakSlot s[2];
      bakInfo(s);
      int n = bakNewestAuto(s);  // ko11.21.1: solo las automaticas
      bakKnownDay = n < 0 ? -1 : (int32_t)(s[n].h.epoch / 86400);
    }
    due = (int32_t)(clockEpoch() / 86400) != bakKnownDay;
  }
  if (due) bakDoBackup();
}

// al final de setup(): partida nueva + copia en la SD -> preguntar
void bakBootCheck() {
  if (!sdReady || safeMode || !pet.awaitingStarter()) return;
  bakInfo(bakSlots);
  int n = bakNewest(bakSlots);
  if (n < 0 || bakSlots[n].h.dex < 1) return;
  bakAsk = true;
  bakAskSlot = (int8_t)n;
}

// "2026.09.27 12:30" + "PIKACHU Lv23"
static void bakSlotLines(const BakHdr &h, char *l1, size_t n1, char *l2, size_t n2) {
  if (h.epoch) {
    int y;
    uint8_t mo, d;
    wxDate(h.epoch, &y, &mo, &d, nullptr);
    snprintf(l1, n1, "%d.%02u.%02u %02u:%02u", y, mo, d, (unsigned)(h.epoch / 3600 % 24), (unsigned)(h.epoch / 60 % 60));
  } else {
    snprintf(l1, n1, "#%u", (unsigned)h.seq);
  }
  if (h.dex >= 1 && h.dex <= DEX_COUNT) snprintf(l2, n2, "%s  Lv%u", dexName(h.dex), h.lvl);
  else snprintf(l2, n2, "%s", T(S_EGG_HDR));
}

#define BAK_NOW_Y 104
#define BAK_ROW_Y 170
#define BAK_ROW_H 70
#define BAK_ROW_GAP 12

void openBackup() {
  if (netPortalOn()) netStopPortal();
  bakInfo(bakSlots);
  bakSel = -1;
  bakMsg = -1;
  bakCrashView = false;
  xScreen = XS_BAK;
  sfxPlay(SFX_TAP);
}

// ko11.9.2: registro del ultimo reinicio inesperado (ventana aparte: no cabe en una linea)
#define BAK_CR_X 118
#define BAK_CR_Y 384
#define BAK_CR_W 230
#define BAK_CR_H 34
static void drawCrashView() {
  uiPanel(40, 84, 386, 300, 22, UI_WHITE, UI_INK);
  drawFit(XT(X_CRASH_TITLE), 100, 300, UI_BAR_BAD, 3);
  char l[64], when[24] = "-";
  if (crashEpoch > 1000000000UL) {
    int y;
    uint8_t mo, d;
    wxDate(crashEpoch, &y, &mo, &d, nullptr);
    snprintf(when, sizeof(when), "%d.%02u.%02u %02u:%02u", y, mo, d, (unsigned)(crashEpoch / 3600 % 24),
             (unsigned)(crashEpoch / 60 % 60));
  }
  snprintf(l, sizeof(l), XT(X_CRASH_LAST_FMT), when);
  drawFit(l, 146, 340, UI_INK, 2);
  snprintf(l, sizeof(l), XT(X_CRASH_WHY_FMT), crashReasonName());
  drawFit(l, 176, 340, UI_INK, 2);
  XId why = crashReason == 9 ? X_CRASH_POWER : crashReason == 4 ? X_CRASH_PANIC : X_CRASH_WDT;
  drawFit(XT(why), 206, 340, 0x8410, 1);
  snprintf(l, sizeof(l), XT(X_CRASH_AT_FMT), crashWhereName(), (unsigned)(crashWhere >> 8),
           (unsigned)(crashWhere & 0xFF));
  drawFit(l, 230, 340, UI_INK, 2);
  snprintf(l, sizeof(l), XT(X_CRASH_CNT_FMT), (unsigned)crashCount);
  drawFit(l, 260, 340, UI_INK, 2);
  if (crashPcN) {  // ko11.9.3: donde fallo el codigo (para buscarlo con el .elf)
    char pcl[48] = "PC";
    for (uint8_t i = 0; i < crashPcN && i < 3; i++) {
      size_t k = strlen(pcl);
      snprintf(pcl + k, sizeof(pcl) - k, " %08x", (unsigned)crashPc[i]);
    }
    drawFit(pcl, 286, 340, UI_INK, 1);
  }
  drawFit(XT(X_CRASH_SD), 304, 340, 0x8410, 1);
  drawBtn(78, 328, 150, 44, UI_TRACK, UI_INK, XT(X_CRASH_CLEAR));
  drawBtn(238, 328, 150, 44, UI_BAR_OK, UI_WHITE, XT(X_CRASH_CLOSE));
}

static void drawBakConfirm(const char *title, int8_t slot, XId yes, XId no, bool warn = true) {
  uiPanel(48, 120, 370, 226, 22, UI_WHITE, UI_INK);
  drawFit(title, 150, 330, UI_INK, 2);
  if (slot >= 0 && bakSlots[slot].ok) {
    char l1[32], l2[48];
    bakSlotLines(bakSlots[slot].h, l1, sizeof(l1), l2, sizeof(l2));
    drawFit(l2, 192, 330, UI_INK, 3);
    drawFit(l1, 234, 330, 0x8410, 2);
  }
  if (warn) drawFit(XT(X_BAK_WARN), 264, 330, UI_BAR_BAD, 1);  // al arrancar no hay nada que perder
  drawBtn(78, 288, 150, 44, UI_BAR_OK, UI_WHITE, XT(yes));
  drawBtn(238, 288, 150, 44, UI_TRACK, UI_INK, XT(no));
}

void renderBackup() {
  screenBase();
  drawFit(XT(X_BAK_TITLE), 50, 300, UI_INK, 3);
  drawBtn(113, BAK_NOW_Y, 240, 44, UI_BAR_OK, UI_WHITE, XT(X_BAK_NOW));
  int nw = bakNewest(bakSlots);
  for (int i = 0; i < 2; i++) {
    int y = BAK_ROW_Y + i * (BAK_ROW_H + BAK_ROW_GAP);
    bool ok = bakSlots[i].ok;
    // ko11.6.1: manual = amarillo claro, automatica = azul claro, con su etiqueta
    bool manual = ok && (bakSlots[i].h.flags & BAKF_MANUAL);
    uint16_t bg = !ok ? UI_TRACK : manual ? C565(0xff, 0xf0, 0xc0) : C565(0xd8, 0xea, 0xff);
    uiButton(73, y, 320, BAK_ROW_H, 14, bg, i == nw ? UI_BAR_OK : UI_INK);
    if (ok) {  // etiqueta a la izquierda
      uint16_t tc = manual ? C565(0xc0, 0x80, 0x10) : C565(0x30, 0x70, 0xc0);
      gfx->fillRoundRect(84, y + 8, 50, 24, 8, tc);
      gfx->setTextColor(UI_WHITE);
      setSize(1);
      const char *tg = XT(manual ? X_BAK_MANUAL : X_BAK_AUTO_TAG);
      setCur(84 + (50 - textW(tg, 1)) / 2, y + 12);
      printT(tg);
    }
    if (!ok) {
      drawFit(XT(X_BAK_EMPTY), y + 24, 280, 0x8410, 2);
      continue;
    }
    char l1[48], l2[48];
    bakSlotLines(bakSlots[i].h, l1, sizeof(l1), l2, sizeof(l2));
    if (i == nw) {  // la mas nueva: borde verde y "(최신)"
      size_t k = strlen(l1);
      snprintf(l1 + k, sizeof(l1) - k, "  (%s)", XT(X_BAK_NEWEST));
    }
    drawFit(l2, y + 12, 290, UI_INK, 2);
    drawFit(l1, y + 42, 290, i == nw ? UI_BAR_OK : 0x8410, 1);
  }
  drawFit(XT(X_BAK_TAP_SLOT), 338, 300, UI_INK, 1);
  drawFit(XT(X_BAK_AUTO), 362, 320, 0x8410, 1);
  if (bakMsg >= 0 && timeLeft(bakMsgUntil))
    drawFit(XT((XId)bakMsg), 390, 300, bakMsg == X_BAK_DONE ? UI_BAR_OK : UI_BAR_BAD, 2);
  if (crashCount && !(bakMsg >= 0 && timeLeft(bakMsgUntil))) {  // ko11.9.2: boton del registro de reinicios
    char l[40];
    snprintf(l, sizeof(l), XT(X_CRASH_BTN_FMT), (unsigned)crashCount);
    drawBtn(BAK_CR_X, BAK_CR_Y, BAK_CR_W, BAK_CR_H, C565(0xfc, 0xe4, 0xe4), UI_BAR_BAD, l);
  }
  drawBackArrow();  // ko11.6.1
  if (bakSel >= 0) drawBakConfirm(XT(X_BAK_CONFIRM), bakSel, X_BAK_RESTORE, X_BAK_CANCEL);
  if (bakCrashView) drawCrashView();
  uiFlush();
}

static void bakRestoreAndRestart(int8_t slot) {
  screenBase();
  drawFit(XT(X_BAK_RESTORING), 220, 360, UI_INK, 2);
  uiFlush();
  if (bakRestore((uint8_t)slot)) {
    delay(800);
    ESP.restart();
  }
  bakSetMsg(X_BAK_FAIL);
}

void backupTap(int16_t x, int16_t y) {
  if (bakCrashView) {  // ko11.9.2
    if (inRect(x, y, 78, 328, 150, 44)) { crashClear(); bakCrashView = false; sfxPlay(SFX_TAP); }
    else if (inRect(x, y, 238, 328, 150, 44)) { bakCrashView = false; sfxPlay(SFX_TAP); }
    return;
  }
  if (bakSel >= 0) {  // confirmacion
    if (inRect(x, y, 78, 288, 150, 44)) { int8_t s = bakSel; bakSel = -1; bakRestoreAndRestart(s); }
    else if (inRect(x, y, 238, 288, 150, 44)) { bakSel = -1; sfxPlay(SFX_TAP); }
    return;
  }
  if (navHit(NAV_L, x, y)) { xScreen = XS_SET; sfxPlay(SFX_TAP); return; }  // ko11.26: vuelve a ajustes
  if (crashCount && inRect(x, y, BAK_CR_X, BAK_CR_Y, BAK_CR_W, BAK_CR_H)) {  // ko11.9.2
    bakCrashView = true;
    sfxPlay(SFX_TAP);
    return;
  }
  if (inRect(x, y, 113, BAK_NOW_Y, 240, 44)) {
    bool ok = bakDoBackup(true);  // ko11.6.1: marcada como manual
    if (sdReady) bakSetMsg(ok ? X_BAK_DONE : X_BAK_FAIL);
    bakInfo(bakSlots);
    sfxPlay(ok ? SFX_MEDAL : SFX_DENY);
    return;
  }
  for (int i = 0; i < 2; i++) {
    int ry = BAK_ROW_Y + i * (BAK_ROW_H + BAK_ROW_GAP);
    if (inRect(x, y, 73, ry, 320, BAK_ROW_H) && bakSlots[i].ok) { bakSel = (int8_t)i; sfxPlay(SFX_TAP); return; }
  }
}

// pregunta al arrancar (tiene prioridad sobre elegir inicial)
void renderBakAsk() {
  screenBase();
  drawBakConfirm(XT(X_BAK_ASK_TITLE), bakAskSlot, X_BAK_RESTORE, X_RESET_BTN, false);
  uiFlush();
}

void bakAskTap(int16_t x, int16_t y) {
  if (inRect(x, y, 78, 288, 150, 44)) { bakAsk = false; bakRestoreAndRestart(bakAskSlot); return; }
  if (inRect(x, y, 238, 288, 150, 44)) { bakAsk = false; sfxPlay(SFX_TAP); }  // empezar de cero
}

// ======================================================================
// ko11.7: premios de la pokedex (especies capturadas o criadas)
// ======================================================================
static const uint8_t DEXRW_AT[7] = { 10, 30, 50, 100, 151, 200, 251 };

uint16_t dexCaughtCount() {
  uint16_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (dexLog.caughtCount(d) > 0 || pet.isRegistered(d)) n++;
  return n;
}

// el siguiente objetivo (0 = todos dados)
uint8_t dexNextReward() {
  for (int i = 0; i < 7; i++) if (!(pet.dexRewards & (1 << i))) return DEXRW_AT[i];
  return 0;
}

// en loop(): da como mucho un premio cada vez (con su aviso)
void dexRewardLoop(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 3000 || extraOpen() || fastGameNow() || pet.awaitingStarter() || galleryOpen || cardOpen ||
      clockOpen || screenOff) return;  // el aviso se ve en la pantalla principal
  last = now;
  uint16_t n = dexCaughtCount();
  for (int i = 0; i < 7; i++) {
    if ((pet.dexRewards & (1 << i)) || n < DEXRW_AT[i]) continue;
    switch (i) {
      case 0: pet.giveItems(5, 0); break;
      case 1: pet.giveItems(0, 5); break;
      case 2: pet.rareCandy += 1; break;
      case 3: pet.rareCandy += 3; break;
      case 4: pet.rareCandy += 5; pet.shinyCharm = true; break;
      case 5: pet.rareCandy += 5; break;
      default: pet.rareCandy += 10; pet.shinyCharm = true; break;
    }
    if (pet.rareCandy > 999) pet.rareCandy = 999;
    pet.dexRewards |= (uint8_t)(1 << i);
    pet.saveNow();
    char t[80];
    snprintf(t, sizeof(t), XT(X_DEXRW_FMT), DEXRW_AT[i], XT((XId)(X_DEXRW_0 + i)));
    showToast(t);
    sfxPlay(SFX_MEDAL);
    return;
  }
}

// ======================================================================
// ko11.20: doumi (ayudantes) antes de gimnasio / liga / reto del dia
// ko12.2: tantos como el rival (contando al que crias), hasta 5 de la caja; [혼자] sigue. En el gimnasio se sabe el tipo del lider: "유리" / "불리".
// En la liga y el reto, como en PokeRogue, el rival se ve al salir.
// ======================================================================
#define PP_ROWS 4
#define PP_ROW_Y 96
#define PP_ROW_H 44
#define PP_ROW_GAP 4
#define PP_NAV_Y 292
#define PP_BTN_Y 334
static uint8_t ppView[BOX_MAX];
static uint8_t ppViewN = 0;
static int ppFoeType() { return ppKind == BK_GYM && bGym < GYM_COUNT ? GYM_TYPE[bGym] : -1; }
uint8_t ppMaxHelpers();
static uint8_t ppPickN() { uint8_t n = 0; for (uint8_t k = 0; k < TEAM_HELPERS; k++) n += ppPick[k] >= 0; return n; }
static bool ppPicked(int8_t bi) { for (uint8_t k = 0; k < TEAM_HELPERS; k++) if (ppPick[k] == bi) return true; return false; }
// orden: en el gimnasio primero los que tienen ventaja; luego por nivel
static void ppBuildView() {
  ppViewN = 0;
  for (uint8_t i = 0; i < box.count() && ppViewN < BOX_MAX; i++) ppView[ppViewN++] = i;
  int ft = ppFoeType();
  for (uint8_t a = 0; a < ppViewN; a++)
    for (uint8_t b = a + 1; b < ppViewN; b++) {
      const BoxMon &x = box.at(ppView[a]), &y = box.at(ppView[b]);
      int sx = (ft >= 0 ? typeMatch(DEX_TBL[x.dex].ptype, (uint8_t)ft) * 1000 : 0) + x.lvl;
      int sy = (ft >= 0 ? typeMatch(DEX_TBL[y.dex].ptype, (uint8_t)ft) * 1000 : 0) + y.lvl;
      if (sy > sx) { uint8_t t = ppView[a]; ppView[a] = ppView[b]; ppView[b] = t; }
    }
}
static uint8_t ppPages() { return ppViewN ? (uint8_t)((ppViewN + PP_ROWS - 1) / PP_ROWS) : 1; }

void renderPartyPick() {
  uiScreenBg();
  ppBuildView();
  if (ppPage >= ppPages()) ppPage = ppPages() - 1;
  char t[48];
  snprintf(t, sizeof(t), XT(X_PT_TITLE_FMT), ppPickN(), ppMaxHelpers());
  drawFit(t, 30, 300, UI_INK, 2);
  int ft = ppFoeType();
  if (ft >= 0) {
    snprintf(t, sizeof(t), XT(X_PT_VS_TYPE_FMT), typeName((uint8_t)ft));
    int w = textW(t, 1) + 28;
    drawBtn(CX - w / 2, 58, w, 28, orbColor((uint8_t)ft), UI_WHITE, t);
  } else {
    snprintf(t, sizeof(t), XT(X_PT_VS_N_FMT), ppN);
    drawFit(t, 64, 330, UI_INK, 1);
  }
  uint16_t pl = pet.level();
  for (int r = 0; r < PP_ROWS; r++) {
    int k = ppPage * PP_ROWS + r;
    if (k >= ppViewN) break;
    uint8_t bi = ppView[k];
    const BoxMon &m = box.at(bi);
    int y = PP_ROW_Y + r * (PP_ROW_H + PP_ROW_GAP);
    bool sel = ppPicked((int8_t)bi);
    uint8_t left = helperUsesLeft(m);  // ko11.20: hoy le quedan
    uiButton(73, y, 320, PP_ROW_H, 10, sel ? C565(0xe6, 0xf8, 0xdc) : left ? UI_WHITE : UI_TRACK, sel ? UI_BAR_OK : UI_INK);
    drawThumbAt(m.dex, 100, y + PP_ROW_H / 2, 1, false);
    char l[48];
    snprintf(l, sizeof(l), "%s%s", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex));
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(126, y + 3);
    printT(l);
    uint8_t ty = DEX_TBL[m.dex].ptype;
    snprintf(l, sizeof(l), "Lv%u  %s", m.lvl < pl ? m.lvl : pl, typeName(ty));
    setSize(1);
    gfx->setTextColor(orbColor(ty));
    setCur(126, y + 26);
    printT(l);
    snprintf(l, sizeof(l), XT(X_PT_USES_FMT), left, (unsigned)HELPER_USES_PER_DAY);
    gfx->setTextColor(left ? 0x8410 : UI_BAR_BAD);
    setCur(126 + textW("Lv100  ", 1) + textW(typeName(ty), 1) + 6, y + 26);
    printT(l);
    if (ft >= 0) {
      int8_t tm = typeMatch(ty, (uint8_t)ft);
      if (tm) drawBtn(286, y + 9, 50, 26, tm > 0 ? UI_BAR_OK : UI_BAR_BAD, UI_WHITE, XT(tm > 0 ? X_PT_GOOD : X_PT_BAD));
    }
    if (sel) drawCheckMark(364, y + PP_ROW_H / 2, true);
    else gfx->drawCircle(364, y + PP_ROW_H / 2, 10, C565(0xb0, 0xb0, 0xb0));
  }
  if (ppPages() > 1) {
    drawBtn(113, PP_NAV_Y, 60, 32, ppPage ? UI_WHITE : UI_TRACK, UI_INK, "<");
    drawBtn(293, PP_NAV_Y, 60, 32, ppPage + 1 < ppPages() ? UI_WHITE : UI_TRACK, UI_INK, ">");
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", ppPage + 1, ppPages());
    drawFit(pg, PP_NAV_Y + 8, 100, UI_INK, 2);
  }
  uint8_t n = ppPickN();
  drawBtn(83, PP_BTN_Y, 110, 44, UI_TRACK, UI_INK, XT(X_PT_ALONE));
  snprintf(t, sizeof(t), XT(X_PT_GO_FMT), n, ppMaxHelpers());
  drawBtn(203, PP_BTN_Y, 180, 44, n ? UI_BAR_OK : C565(0x9a, 0xc8, 0x9a), UI_WHITE, t);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void partyPickTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { partyCancel(); return; }
  if (inRect(x, y, 83, PP_BTN_Y, 110, 44)) { sfxPlay(SFX_TAP); partyStart(true); return; }
  if (inRect(x, y, 203, PP_BTN_Y, 180, 44)) { sfxPlay(SFX_TAP); partyStart(ppPickN() == 0); return; }
  if (ppPages() > 1 && y >= PP_NAV_Y && y < PP_NAV_Y + 32) {
    if (x >= 113 && x < 173 && ppPage) { ppPage--; sfxPlay(SFX_TAP); }
    else if (x >= 293 && x < 353 && ppPage + 1 < ppPages()) { ppPage++; sfxPlay(SFX_TAP); }
    return;
  }
  if (x < 73 || x >= 393 || y < PP_ROW_Y) return;
  int r = (y - PP_ROW_Y) / (PP_ROW_H + PP_ROW_GAP);
  if (r >= PP_ROWS || (y - PP_ROW_Y) % (PP_ROW_H + PP_ROW_GAP) >= PP_ROW_H) return;
  ppBuildView();
  int k = ppPage * PP_ROWS + r;
  if (k >= ppViewN) return;
  int8_t bi = (int8_t)ppView[k];
  if (!ppPicked(bi) && !helperUsesLeft(box.at((uint8_t)bi))) { sfxPlay(SFX_DENY); return; }  // hoy ya no
  // ko12.2: hasta ppMaxHelpers() (los mismos que el rival). Quitar = los de detras avanzan; lleno = el mas viejo sale
  uint8_t mx = ppMaxHelpers(), n = ppPickN();
  int at = -1;
  for (uint8_t k = 0; k < TEAM_HELPERS; k++) if (ppPick[k] == bi) at = k;
  if (at >= 0) {
    for (uint8_t k = (uint8_t)at; k + 1 < TEAM_HELPERS; k++) ppPick[k] = ppPick[k + 1];
    ppPick[TEAM_HELPERS - 1] = -1;
  } else if (mx == 0) {
    sfxPlay(SFX_DENY); return;
  } else if (n < mx) {
    ppPick[n] = bi;
  } else {
    for (uint8_t k = 0; k + 1 < mx; k++) ppPick[k] = ppPick[k + 1];
    ppPick[mx - 1] = bi;
  }
  sfxPlay(SFX_TAP);
}

// ======================================================================
// ko11.26: menu de ajustes (flecha de arriba). Antes todo colgaba de la hora:
// sonido -> brillo, WiFi -> SD update / copias, idioma y nuevo comienzo.
// Ahora cada cosa del aparato es un boton de esta rejilla 2x4
// ======================================================================
#define SET_X1 66
#define SET_X2 238
#define SET_W 162
#define SET_H 56
#define SET_Y0 98
#define SET_DY 68
static bool gClockFromSet = false;

void openSettings() {
  retMark();
  clockOpen = false;
  cardOpen = false;
  xScreen = XS_SET;
}

// cerrar la hora (OK o [<]): si se abrio desde ajustes, se vuelve ahi
void clockClose() {
  clockOpen = false;
  if (gClockFromSet) { gClockFromSet = false; xScreen = XS_SET; }
}

void renderSettings() {
  uiScreenBg();
  drawFit(XT(X_SET_TITLE), 36, 300, UI_INK, 3);
  char lang[24];
  snprintf(lang, sizeof(lang), XT(X_SET_LANG_FMT), LANG_CODES[gLang]);
  struct { const char *t; uint16_t bg, fg; } b[8] = {
    { XT(X_SET_TIME), UI_WHITE, UI_INK },
    { XT(X_SET_SOUND), (uint16_t)(audioEnabled() ? UI_BAR_OK : UI_WHITE), (uint16_t)(audioEnabled() ? UI_WHITE : UI_INK) },
    { XT(X_SET_SCREEN), UI_WHITE, UI_INK },
    { "WiFi", (uint16_t)(netConfigured() ? 0x4C98 : UI_WHITE), (uint16_t)(netConfigured() ? UI_WHITE : UI_INK) },
    { XT(X_UPD_BTN), 0xFB20, UI_WHITE },
    { XT(X_BAK_BTN), 0x6B4D, UI_WHITE },
    { lang, UI_WHITE, UI_INK },
    { XT(X_RESET_BTN), UI_WHITE, UI_BAR_BAD },
  };
  for (int i = 0; i < 8; i++)
    drawBtn(i % 2 ? SET_X2 : SET_X1, SET_Y0 + (i / 2) * SET_DY, SET_W, SET_H, b[i].bg, b[i].fg, b[i].t);
  // version del firmware (antes en la pantalla de la hora)
  char ver[40];
  snprintf(ver, sizeof(ver), "TamaPoke v%s", FW_VERSION);
  int vy = SET_Y0 + 4 * SET_DY + 6;
  gfx->setTextColor(UI_INK);
  setSize(1);
  setCur(centerX(ver, 1), vy);
  printT(ver);
  if (bigPart()) gfx->fillCircle(centerX(ver, 1) + textW(ver, 1) + 10, vy + 9, 5, UI_BAR_OK);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void settingsTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y) || y < 60) { sfxPlay(SFX_TAP); goBack(); return; }
  if (y < SET_Y0 || x < SET_X1 || x >= SET_X2 + SET_W) return;
  int row = (y - SET_Y0) / SET_DY;
  if (row > 3 || (y - SET_Y0) % SET_DY >= SET_H) return;
  int i = row * 2 + (x >= SET_X2 - 5 ? 1 : 0);
  sfxPlay(SFX_TAP);
  switch (i) {
    case 0: xScreen = XS_NONE; gClockFromSet = true; openClock(); break;
    case 1: openSound(); break;
    case 2: xScreen = XS_BRIGHT; break;
    case 3: openNet(); break;
    case 4: openUpdate(); break;
    case 5: openBackup(); break;
    case 6: setLang((Lang)((gLang + 1) % LANG_COUNT)); applyLangFont(); break;
    case 7: openReset(); break;
  }
}

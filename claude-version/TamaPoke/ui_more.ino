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
#define BOX_TAB_Y 34      // ko10.5: pestanas [caja] [corona]
#define BOX_TAB_H 40
#define BOX_TAB_W 150
#define BOX_TAB_X1 80
#define BOX_TAB_X2 236
uint8_t boxPage = 0;
int16_t boxSel = -1;          // indice en la caja con la ficha abierta, -1 = lista
bool boxHall = false;         // ko10.5: pestana del salon de la fama (corona)
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

static void drawCrown(int x, int y, uint16_t c) {
  gfx->fillRect(x, y + 8, 18, 6, c);
  gfx->fillTriangle(x, y + 8, x + 3, y, x + 6, y + 8, c);
  gfx->fillTriangle(x + 6, y + 8, x + 9, y - 2, x + 12, y + 8, c);
  gfx->fillTriangle(x + 12, y + 8, x + 15, y, x + 18, y + 8, c);
}

static uint8_t boxPages() { return cb().count() ? (cb().count() + BOX_ROWS - 1) / BOX_ROWS : 1; }

void openBox() {
  cardOpen = false;
  boxHall = false;
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

void renderBoxDetail() {
  const BoxMon &m = cb().at(boxSel);
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[48];
  snprintf(head, sizeof(head), "%s%s", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex));
  drawFit(head, 44, 300, DEX_TBL[m.dex].accent, 3);
  drawThumbAt(m.dex, CX, 136, 3, false);
  char l[48];
  snprintf(l, sizeof(l), "No.%03d  Lv.%u  %s", m.dex, m.lvl, typeName(DEX_TBL[m.dex].ptype));
  drawFit(l, 204, 340, UI_INK, 2);
  char date[8];
  boxDate(m.epoch, date, sizeof(date));
  snprintf(l, sizeof(l), "%s  %s", XT((m.flags & BOXF_RAISED) ? X_RAISED_TAG
                                      : (m.flags & BOXF_CAUGHT) ? X_CAUGHT_TAG : X_WON_TAG), date);
  drawFit(l, 232, 340, UI_INK, 2);
  drawFit(XT(boxHall ? X_HALL_NOTE : X_BOX_NEXT), 262, 360, UI_INK, 1);
  bool conf = timeLeft(boxConfirmUntil) > 0;
  if (boxHall) {  // ko10.5: los de corona son recuerdos: no se sueltan
    drawBtn(165, 300, 136, 48, UI_TRACK, UI_INK, XT(X_CLOSE));
  } else {
    drawBtn(93, 300, 136, 48, UI_BAR_BAD, UI_WHITE, XT(conf ? X_RELEASE_Q : X_RELEASE));
    drawBtn(237, 300, 136, 48, UI_TRACK, UI_INK, XT(X_CLOSE));
  }
  gfx->flush();
}

void renderBox() {
  if (boxSel >= 0 && boxSel < cb().count()) { renderBoxDetail(); return; }
  boxSel = -1;
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  // ko10.5: dos pestanas: la caja y el salon de la fama (corona)
  char t1[24], t2[24];
  snprintf(t1, sizeof(t1), XT(X_BOX_TITLE_FMT), box.count(), BOX_MAX);
  snprintf(t2, sizeof(t2), XT(X_HALL_TAB_FMT), hall.count());
  drawBtn(BOX_TAB_X1, BOX_TAB_Y, BOX_TAB_W, BOX_TAB_H, boxHall ? UI_TRACK : UI_BAR_WARN, UI_INK, t1);
  drawBtn(BOX_TAB_X2, BOX_TAB_Y, BOX_TAB_W, BOX_TAB_H, boxHall ? C565(0xe8, 0xb0, 0x20) : UI_TRACK, UI_INK, t2);
  drawCrown(BOX_TAB_X2 + 12, BOX_TAB_Y + 12, boxHall ? UI_WHITE : C565(0xe8, 0xb0, 0x20));
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
    gfx->fillRoundRect(73, y, 320, BOX_ROW_H, 10, rowBg);
    gfx->drawRoundRect(73, y, 320, BOX_ROW_H, 10, UI_INK);
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
    if (m.flags & BOXF_RAISED) drawCrown(354, y + 14, C565(0xe8, 0xb0, 0x20));  // ko10.5: criado
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
  drawFit(T(S_BACK), 404, 200, UI_INK, 2);
  gfx->flush();
}

void boxSwipe() {  // deslizar: cierra la ficha, o la caja si estaba en la lista
  if (boxSel >= 0) boxSel = -1;
  else xScreen = XS_NONE;
}

void boxTap(int16_t x, int16_t y) {
  if (boxSel >= 0) {  // ficha: soltar (dos toques) o cerrar
    // ko9.1: tocar al Pokemon repite su grito
    if (y >= 80 && y < 200 && x >= 120 && x < 346) {
      audioCry(cb().at((uint8_t)boxSel).dex);
      return;
    }
    if (!boxHall && y >= 300 && y < 348 && x >= 93 && x < 229) {  // el salon no suelta
      if (timeLeft(boxConfirmUntil)) {
        pet.addCandy(box.at((uint8_t)boxSel).dex, 1);  // ko10.4: soltar da 1 caramelo
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
  // ko10.5: pestanas caja / salon de la fama
  if (y >= BOX_TAB_Y && y < BOX_TAB_Y + BOX_TAB_H) {
    if (x >= BOX_TAB_X1 && x < BOX_TAB_X1 + BOX_TAB_W && boxHall) { boxHall = false; boxPage = 0; sfxPlay(SFX_TAP); }
    else if (x >= BOX_TAB_X2 && x < BOX_TAB_X2 + BOX_TAB_W && !boxHall) { boxHall = true; boxPage = 0; sfxPlay(SFX_TAP); }
    return;
  }
  if (y < BOX_TAB_Y || y >= 392) { xScreen = XS_NONE; return; }  // arriba / "atras"
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
  if (k < bx.count()) { boxSel = boxView(k); audioCry(bx.at((uint8_t)boxSel).dex); }  // ko9.1: su grito
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
  hall.addRaised(p.speciesId, p.level(), p.shiny, p.geneAtk, p.geneDef, p.geneSpe, clockEpoch());  // salon
  gNextPickPending = true;
}

// se puede elegir si su familia no se ha criado (o si ya se criaron todas)
static bool nextPickable(const BoxMon &m) { return pet.allFamsRaised() || !pet.isFamRaised(m.dex); }


static uint8_t nextPages() { return box.count() ? (box.count() + NP_ROWS - 1) / NP_ROWS : 1; }

void renderNextPick() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
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
    gfx->fillRoundRect(73, y, 320, NP_ROW_H, 10, ok ? UI_WHITE : UI_TRACK);
    gfx->drawRoundRect(73, y, 320, NP_ROW_H, 10, UI_INK);
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
    if (m.flags & BOXF_RAISED) drawCrown(360, y + 14, C565(0xe8, 0xb0, 0x20));
  }
  if (nextPages() > 1) {
    drawBtn(113, NP_NAV_Y, 60, 34, nextPage ? UI_WHITE : UI_TRACK, UI_INK, "<");
    drawBtn(293, NP_NAV_Y, 60, 34, nextPage + 1 < nextPages() ? UI_WHITE : UI_TRACK, UI_INK, ">");
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", nextPage + 1, nextPages());
    drawFit(pg, NP_NAV_Y + 8, 100, UI_INK, 2);
  }
  drawFit(XT(X_NEXT_HINT), 392, 300, 0x8410, 1);
  gfx->flush();
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

#define VOL_ROW_Y 138
#define VOL_ROW_H 72
#define VOL_BAR_X 138
#define VOL_BAR_W 190

void openSound() {
  clockOpen = false;
  xScreen = XS_VOL;
}

void renderSound() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  drawFit(XT(X_SOUND_TITLE), 40, 300, UI_INK, 3);
  bool on = audioEnabled();
  drawBtn(133, 78, 200, 40, on ? UI_BAR_OK : UI_TRACK, on ? UI_WHITE : UI_INK,
          XT(on ? X_SOUND_ON : X_SOUND_OFF));
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
    gfx->fillRoundRect(VOL_BAR_X, y + 34, VOL_BAR_W, 16, 6, UI_TRACK);
    int fw = (VOL_BAR_W - 4) * v / 100;
    if (fw > 0) gfx->fillRoundRect(VOL_BAR_X + 2, y + 36, fw, 12, 5, on ? UI_BAR_OK : 0x8410);
  }
  // ko10.4: duracion del fondo cargado (si sale 0:30 y la cancion era mas larga,
  // el bgm.wav de la SD esta recortado: tools/prep_music.py)
  uint32_t bs = audioBgmSeconds();
  char bl[40];
  if (bs) snprintf(bl, sizeof(bl), XT(X_BGM_LEN_FMT), (unsigned)(bs / 60), (unsigned)(bs % 60));
  else snprintf(bl, sizeof(bl), "%s", XT(X_BGM_NONE));
  drawFit(bl, 340, 300, 0x8410, 1);
  drawBtn(143, 364, 180, 44, UI_BAR_OK, UI_WHITE, XT(X_VOL_DONE));
  gfx->flush();
}

static void soundPreview(int ch) {
  if (ch == 1) audioCry(pet.isEgg() ? 25 : pet.speciesId);
  else if (ch == 2) sfxPlay(SFX_TAP);
}

void soundTap(int16_t x, int16_t y) {
  if (y >= 78 && y < 118 && x >= 133 && x < 333) {
    audioSetEnabled(!audioEnabled());
    if (audioEnabled()) sfxPlay(SFX_TAP);
    return;
  }
  if ((y >= 356 && x >= 133 && x < 333) || y < 72) {  // completar -> hora
    xScreen = XS_NONE;
    clockOpen = true;
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
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
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
  gfx->flush();
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
  gfx->flush();
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
    if (y >= 250 && y < 302 && x >= 238 && x < 378) { xScreen = XS_NET; return; }
    return;
  }
  if (y >= 320 || y < 72) xScreen = XS_NET;
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
  fame.wipe();  // ko10.11
  dexLog.wipe();
  sfxPlay(SFX_BYE);
  delay(1200);
  ESP.restart();
}

void renderReset() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  drawFit(XT(X_RESET_TITLE), 40, 300, UI_INK, 3);
  if (rstDone) {
    drawFit(XT(X_RESET_DONE), 220, 320, UI_INK, 3);
    gfx->flush();
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
  gfx->flush();
}

void resetTap(int16_t x, int16_t y) {
  if (rstInButton(x, y)) { rstHint = true; sfxPlay(SFX_DENY); return; }  // toque corto: no basta
  if (y >= 364 || y < 72) {  // cancelar -> vuelve a la hora
    xScreen = XS_NONE;
    clockOpen = true;
    sfxPlay(SFX_TAP);
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

void openCandyBag() {
  xScreen = XS_CANDY;
  cardOpen = false;
  bagBuild();
  bagPage = 0;
  bagSel = -1;
  bagMsgUntil = 0;
  sfxPlay(SFX_TAP);
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
  snprintf(b, sizeof(b), XT(X_BAG_RARE_FMT), pet.rareCandy);
  drawBtn(BAG_ROW_X, BAG_RARE_Y, BAG_ROW_W, BAG_ROW_H, pet.rareCandy ? C565(0xd8, 0xa8, 0x20) : UI_TRACK,
          pet.rareCandy ? UI_WHITE : 0x8410, b);
  uint8_t mine = pet.isEgg() ? 0 : DEX_FAM[pet.speciesId];
  if (!bagN) drawFit(XT(X_BAG_EMPTY), BAG_ROW_Y + 60, 300, 0x8410, 2);
  for (int r = 0; r < BAG_ROWS; r++) {
    int k = bagPage * BAG_ROWS + r;
    if (k >= bagN) break;
    uint8_t f = bagFam[k];
    int y = BAG_ROW_Y + r * (BAG_ROW_H + BAG_GAP);
    bool isMine = f == mine;
    gfx->fillRoundRect(BAG_ROW_X, y, BAG_ROW_W, BAG_ROW_H, 12, isMine ? C565(0xf0, 0x7a, 0xa8) : UI_WHITE);
    gfx->drawRoundRect(BAG_ROW_X, y, BAG_ROW_W, BAG_ROW_H, 12, UI_INK);
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
  if (bagMsg && timeLeft(bagMsgUntil)) drawFit(bagMsg, 372, 300, UI_BAR_OK, 2);
  else if (bagPages() > 1) {
    snprintf(b, sizeof(b), XT(X_BAG_PAGE_FMT), bagPage + 1, bagPages());
    drawFit(b, 372, 200, UI_INK, 1);
  }
  if (bagPage > 0) drawNav(NAV_L, UI_INK);
  if (bagPage + 1 < bagPages()) drawNav(NAV_R, UI_INK);
  drawNav(NAV_DOWN, UI_INK);
  // ventana de la seleccion
  if (bagSel >= 0) {
    int x, y, w, h;
    bagPopupRect(x, y, w, h);
    gfx->fillRoundRect(x, y, w, h, 16, UI_WHITE);
    gfx->drawRoundRect(x, y, w, h, 16, UI_INK);
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
  gfx->flush();
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
  if (navHit(NAV_DOWN, x, y)) { xScreen = XS_NONE; sfxPlay(SFX_TAP); return; }
  if (navHit(NAV_L, x, y)) { candyBagSwipe(1); return; }
  if (navHit(NAV_R, x, y)) { candyBagSwipe(-1); return; }
  if (inRect(x, y, BAG_ROW_X, BAG_RARE_Y, BAG_ROW_W, BAG_ROW_H)) { bagSel = 0; sfxPlay(SFX_TAP); return; }
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
static void drawCrownBig(int cx, int y, int w) {
  uint16_t gold = C565(0xf0, 0xc0, 0x30), dark = C565(0xa0, 0x70, 0x10), red = C565(0xe0, 0x30, 0x40);
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

void fameClose() {
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
  drawCrownBig(CX, top + 4, 64);
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
  if (when[0]) { size_t l = strlen(b); snprintf(b + l, sizeof(b) - l, "  %s", when); }
  drawFit(b, 306, 340, C565(0xb0, 0x80, 0x10), 2);
  if (m.geneAtk) {
    snprintf(b, sizeof(b), XT(X_FAME_GENES_FMT), m.geneAtk, m.geneDef, m.geneSpe);
    drawFit(b, 336, 320, 0x8410, 2);
  }
  drawFit(XT(X_FAME_TAP), 392, 240, UI_INK, 1);
  drawNav(NAV_DOWN, UI_INK);
}

void renderFame() {
  screenBase();
  if (fameSel >= 0 && fameSel < fame.count()) { fameDetail(); gfx->flush(); return; }
  char t[40];
  snprintf(t, sizeof(t), XT(X_FAME_BTN_FMT), fame.count());
  drawFit(t, 36, 320, C565(0xb0, 0x80, 0x10), 3);
  uint8_t n = fame.count();
  for (int k = 0; k < FM_COLS * FM_ROWS; k++) {
    int idx = famePage * FM_COLS * FM_ROWS + k;
    if (idx >= n) break;
    const BoxMon &m = fame.at((uint8_t)(n - 1 - idx));  // el mas reciente primero
    int x = FM_X + (k % FM_COLS) * FM_CELL, y = FM_Y + (k / FM_COLS) * FM_CELL;
    gfx->fillRoundRect(x + 4, y + 4, FM_CELL - 8, FM_CELL - 8, 14, (m.flags & BOXF_SHINY) ? C565(0xff, 0xf0, 0xc0) : UI_WHITE);
    gfx->drawRoundRect(x + 4, y + 4, FM_CELL - 8, FM_CELL - 8, 14, C565(0xb0, 0x80, 0x10));
    const uint8_t *th = thumbs.get(m.dex);
    if (th) drawThumb(th, x + 8, y + 14, 2, false);
    drawCrownBig(x + FM_CELL / 2, y + 22, 26);
  }
  if (famePages() > 1) {
    snprintf(t, sizeof(t), XT(X_BAG_PAGE_FMT), famePage + 1, famePages());
    drawFit(t, 392, 200, UI_INK, 1);
  }
  if (famePage > 0) drawNav(NAV_L, UI_INK);
  if (famePage + 1 < famePages()) drawNav(NAV_R, UI_INK);
  drawNav(NAV_DOWN, UI_INK);
  gfx->flush();
}

void fameTap(int16_t x, int16_t y) {
  if (fameSel >= 0) { fameClose(); sfxPlay(SFX_TAP); return; }  // la ficha: cualquier toque vuelve
  if (navHit(NAV_DOWN, x, y)) { fameClose(); sfxPlay(SFX_TAP); return; }
  if (navHit(NAV_L, x, y)) { fameSwipe(1); return; }
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
  audioCry(m.dex);
}

// ======================================================================
// ko11.6: copia de la partida en la SD (savebak.h)
// Automatica: una al dia y en los momentos importantes (evolucion / nuevo
// companero, campeon, despedida). Manual: red > [copia]. Al arrancar con una
// partida nueva y una copia en la SD, se pregunta si restaurarla.
// ======================================================================

BakSlot bakSlots[2];
int8_t bakSel = -1;          // ranura elegida para restaurar (confirmacion)
int8_t bakMsg = -1;          // XId del ultimo aviso (-1 nada)
uint32_t bakMsgUntil = 0;
bool bakAsk = false;         // pregunta al arrancar
int8_t bakAskSlot = -1;
static bool bakPending = false;
static uint32_t bakLastT = 0;
static int32_t bakKnownDay = -2;  // dia de la copia mas nueva (-1 ninguna, -2 sin mirar)

void bakRequest() { bakPending = true; }

static void bakSetMsg(XId m) { bakMsg = (int8_t)m; bakMsgUntil = millis() + 3000; }

static bool bakDoBackup() {
  if (!sdReady) { bakSetMsg(X_BAK_NOSD); return false; }
  pet.saveNow();  // lo ultimo tambien
  uint32_t e = clockEpoch();
  bool ok = bakBackupNow(pet.speciesId, pet.level(), gClockTrusted ? e : 0);
  bakLastT = millis() ? millis() : 1;
  bakPending = false;
  if (ok && gClockTrusted) bakKnownDay = (int32_t)(e / 86400);
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
      int n = bakNewest(s);
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
  xScreen = XS_BAK;
  sfxPlay(SFX_TAP);
}

static void drawBakConfirm(const char *title, int8_t slot, XId yes, XId no, bool warn = true) {
  gfx->fillRoundRect(48, 120, 370, 226, 22, UI_WHITE);
  gfx->drawRoundRect(48, 120, 370, 226, 22, UI_INK);
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
    gfx->fillRoundRect(73, y, 320, BAK_ROW_H, 14, ok ? UI_WHITE : UI_TRACK);
    gfx->drawRoundRect(73, y, 320, BAK_ROW_H, 14, i == nw ? UI_BAR_OK : UI_INK);
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
  drawTopExitHint(424);
  if (bakSel >= 0) drawBakConfirm(XT(X_BAK_CONFIRM), bakSel, X_BAK_RESTORE, X_BAK_CANCEL);
  gfx->flush();
}

static void bakRestoreAndRestart(int8_t slot) {
  screenBase();
  drawFit(XT(X_BAK_RESTORING), 220, 360, UI_INK, 2);
  gfx->flush();
  if (bakRestore((uint8_t)slot)) {
    delay(800);
    ESP.restart();
  }
  bakSetMsg(X_BAK_FAIL);
}

void backupTap(int16_t x, int16_t y) {
  if (bakSel >= 0) {  // confirmacion
    if (inRect(x, y, 78, 288, 150, 44)) { int8_t s = bakSel; bakSel = -1; bakRestoreAndRestart(s); }
    else if (inRect(x, y, 238, 288, 150, 44)) { bakSel = -1; sfxPlay(SFX_TAP); }
    return;
  }
  if (topDoubleTap(y)) { xScreen = XS_NET; return; }  // vuelve a la red
  if (inRect(x, y, 113, BAK_NOW_Y, 240, 44)) {
    bool ok = bakDoBackup();
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
  gfx->flush();
}

void bakAskTap(int16_t x, int16_t y) {
  if (inRect(x, y, 78, 288, 150, 44)) { bakAsk = false; bakRestoreAndRestart(bakAskSlot); return; }
  if (inRect(x, y, 238, 288, 150, 44)) { bakAsk = false; sfxPlay(SFX_TAP); }  // empezar de cero
}

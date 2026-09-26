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
uint8_t boxPage = 0;
int16_t boxSel = -1;          // indice en la caja con la ficha abierta, -1 = lista

// ko10.4: la lista se ve ordenada por numero de pokedex (los repetidos quedan
// juntos; entre iguales, el mas antiguo primero). Solo cambia el orden de la
// vista: la caja guardada no se toca. boxView(k) = indice real del k-esimo
static uint8_t boxOrd[BOX_MAX];
static void boxSortView() {
  uint8_t n = box.count();
  for (uint8_t i = 0; i < n; i++) boxOrd[i] = i;
  for (uint8_t i = 1; i < n; i++) {  // insercion (estable), n <= 60
    uint8_t v = boxOrd[i];
    int j = i - 1;
    while (j >= 0 && box.at(boxOrd[j]).dex > box.at(v).dex) { boxOrd[j + 1] = boxOrd[j]; j--; }
    boxOrd[j + 1] = v;
  }
}
static uint8_t boxView(int k) { return boxOrd[k]; }
uint32_t boxConfirmUntil = 0; // segundo toque en "soltar" para confirmar

static uint8_t boxPages() { return box.count() ? (box.count() + BOX_ROWS - 1) / BOX_ROWS : 1; }

void openBox() {
  cardOpen = false;
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
  const BoxMon &m = box.at(boxSel);
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
  drawFit(XT(X_BOX_NEXT), 262, 360, UI_INK, 1);
  bool conf = timeLeft(boxConfirmUntil) > 0;
  drawBtn(93, 300, 136, 48, UI_BAR_BAD, UI_WHITE, XT(conf ? X_RELEASE_Q : X_RELEASE));
  drawBtn(237, 300, 136, 48, UI_TRACK, UI_INK, XT(X_CLOSE));
  gfx->flush();
}

void renderBox() {
  if (boxSel >= 0 && boxSel < box.count()) { renderBoxDetail(); return; }
  boxSel = -1;
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[24];
  snprintf(head, sizeof(head), XT(X_BOX_TITLE_FMT), box.count(), BOX_MAX);
  drawFit(head, 40, 300, UI_INK, 3);
  if (!box.count()) {
    drawFit(XT(X_BOX_EMPTY), 170, 300, UI_INK, 3);
    drawFit(XT(X_BOX_HINT), 220, 360, UI_INK, 2);
    drawFit(XT(X_BOX_NEXT), 248, 360, UI_INK, 2);
  }
  if (boxPage >= boxPages()) boxPage = boxPages() - 1;
  boxSortView();
  for (int r = 0; r < BOX_ROWS; r++) {
    int k = boxPage * BOX_ROWS + r;
    if (k >= box.count()) break;
    const BoxMon &m = box.at(boxView(k));
    int y = BOX_ROW_Y + r * (BOX_ROW_H + BOX_ROW_GAP);
    // ko10.4: repetidos: fondo de color por especie y "xN" (cuantos hay en la caja)
    uint8_t same = 0;
    for (uint8_t j = 0; j < box.count(); j++) if (box.at(j).dex == m.dex) same++;
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
      audioCry(box.at((uint8_t)boxSel).dex);
      return;
    }
    if (y >= 300 && y < 348 && x >= 93 && x < 229) {
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
  if (y < 72 || y >= 392) { xScreen = XS_NONE; return; }  // arriba / "atras"
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
  boxSortView();
  if (k < box.count()) { boxSel = boxView(k); audioCry(box.at((uint8_t)boxSel).dex); }  // ko9.1: su grito
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

void onPetEnd(Pet &p, uint8_t how) {
  if (how == CER_RUNAWAY || p.isEgg()) return;  // escapada: huevo y ya
  box.addRaised(p.speciesId, p.level(), p.shiny, p.geneAtk, p.geneDef, p.geneSpe, clockEpoch());
  gNextPickPending = true;
}

// se puede elegir si su familia no se ha criado (o si ya se criaron todas)
static bool nextPickable(const BoxMon &m) { return pet.allFamsRaised() || !pet.isFamRaised(m.dex); }

static void drawCrown(int x, int y, uint16_t c) {
  gfx->fillRect(x, y + 8, 18, 6, c);
  gfx->fillTriangle(x, y + 8, x + 3, y, x + 6, y + 8, c);
  gfx->fillTriangle(x + 6, y + 8, x + 9, y - 2, x + 12, y + 8, c);
  gfx->fillTriangle(x + 12, y + 8, x + 15, y, x + 18, y + 8, c);
}

static uint8_t nextPages() { return box.count() ? (box.count() + NP_ROWS - 1) / NP_ROWS : 1; }

void renderNextPick() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  drawFit(XT(X_NEXT_TITLE), 34, 320, UI_INK, 2);
  drawBtn(93, NP_EGG_Y, 280, 48, UI_BAR_WARN, UI_INK, XT(X_NEXT_EGG));
  boxSortView();
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
  boxSortView();
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

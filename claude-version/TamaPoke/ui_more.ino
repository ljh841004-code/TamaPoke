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
  snprintf(l, sizeof(l), "%s  %s", XT((m.flags & BOXF_CAUGHT) ? X_CAUGHT_TAG : X_WON_TAG), date);
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
  for (int r = 0; r < BOX_ROWS; r++) {
    int i = boxPage * BOX_ROWS + r;
    if (i >= box.count()) break;
    const BoxMon &m = box.at(i);
    int y = BOX_ROW_Y + r * (BOX_ROW_H + BOX_ROW_GAP);
    gfx->fillRoundRect(73, y, 320, BOX_ROW_H, 10, UI_WHITE);
    gfx->drawRoundRect(73, y, 320, BOX_ROW_H, 10, UI_INK);
    drawThumbAt(m.dex, 104, y + BOX_ROW_H / 2, 1, false);
    char l[48];
    snprintf(l, sizeof(l), "%s%s", (m.flags & BOXF_SHINY) ? "*" : "", dexName(m.dex));
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(134, y + 6);
    printT(l);
    snprintf(l, sizeof(l), "Lv.%u  %s", m.lvl, XT((m.flags & BOXF_CAUGHT) ? X_CAUGHT_TAG : X_WON_TAG));
    setSize(1);
    setCur(134, y + 28);
    printT(l);
    if (m.flags & BOXF_CAUGHT) drawMap(SPR_ICON_PLAY, 16, 352, y + 7, 2, false);
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
    if (y >= 300 && y < 348 && x >= 93 && x < 229) {
      if (timeLeft(boxConfirmUntil)) {
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
  int i = boxPage * BOX_ROWS + r;
  if (i < box.count()) { boxSel = i; sfxPlay(SFX_TAP); }
}

// tras la despedida (forma final) el siguiente compañero sale de la caja
bool nextFromBox(Pet &p) {
  int i = box.pickRandom();
  BoxMon m;
  if (i < 0 || !box.take((uint8_t)i, m)) return false;
  p.adoptMon(m.dex, m.lvl, m.flags & BOXF_SHINY, m.geneAtk, m.geneDef, m.geneSpe);
  char msg[64];
  txFmt(msg, sizeof(msg), X_FROM_BOX, dexName(m.dex));
  showToast(msg);
  toastUntil = millis() + 6000;
  return true;
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
  drawBtn(143, 360, 180, 44, UI_BAR_OK, UI_WHITE, XT(X_VOL_DONE));
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
int8_t updResult = 0;  // 0 nada, 1 hecho (reinicia), -1 fallo

void openUpdate() {
  if (netPortalOn()) netStopPortal();
  updResult = 0;
  updState = sdUpdateCheck(&updSize);
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
  char ver[24];
  snprintf(ver, sizeof(ver), "v%s", FW_VERSION);
  drawFit(ver, 92, 200, UI_INK, 2);
  if (updResult > 0) {
    drawFit(XT(X_UPD_DONE), 200, 360, UI_BAR_OK, 3);
  } else if (updResult < 0) {
    drawFit(XT(X_UPD_FAIL), 180, 360, UI_BAR_BAD, 3);
    drawBtn(133, 330, 200, 48, UI_TRACK, UI_INK, T(S_BACK));
  } else if (updState == UPD_OK) {
    char l[40];
    snprintf(l, sizeof(l), XT(X_UPD_SIZE_FMT), (unsigned)(updSize / 1024));
    drawFit(l, 170, 340, UI_INK, 2);
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

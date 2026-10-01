// ui_story.ino - ko11.21: modo historia (menu, capitulos, escenas con dialogo)
// (se concatena tras ui_more.ino: usa startTrainer / partyOpen de ui_extra.ino)
//
// Estilos: 0 "관동 여행기" (juego), 1 "지우와 함께" (anime), 2 "원정" (PokeRogue, sin guion).
// Cada capitulo se juega cuando se quiera; un combate necesita al Pokemon con fuerzas
// (si esta cansado se guarda el punto y se vuelve luego). Retratos: /mons/story.bin

#define ST_BOX_X 48
#define ST_BOX_Y 272
#define ST_BOX_W 370
#define ST_BOX_H 112
#define ST_LINE_W 336
#define ST_LINES 3
#define ST_TYPE_MS 28          // un caracter cada 28 ms (efecto maquina de escribir)
#define ST_CARD_Y 84
#define ST_CARD_H 80
#define ST_CARD_GAP 8
#define ST_ROW_Y 86
#define ST_ROW_H 48
#define ST_ROW_GAP 6
#define ST_RESTART_Y 364
#define STORY_FOE_EASE 15   // ko11.23.2: rivales de la historia -15 % (vida, ataque, defensa, velocidad)
#define ST_PG_LX 92
#define ST_PG_RX 330
#define ST_PG_W 44

uint8_t stStyle = 0, stCh = 0;
uint16_t stStep = 0;
uint32_t stDone[STORY_STYLES] = { 0, 0 };  // bit i = capitulo i superado (ko11.23: 32 bits)
uint8_t stResStyle = 0xFF, stResCh = 0;   // punto guardado (0xFF = ninguno)
uint16_t stResStep = 0;
uint16_t rgBest = 0;                      // expedicion: mejor oleada
// ko11.21: el companero de cada estilo (juego: el elegido con Oak, anime: Pikachu) y su exp
int16_t stPDex[STORY_STYLES] = { 0, 0 };
uint32_t stPExp[STORY_STYLES] = { 0, 0 };
PmdMon storyPmd;                          // su sprite en las escenas
// ko11.22: los que se unen en la historia (0 = nadie; | JOIN_KEEP = no evoluciona)
int16_t stJ[STORY_STYLES][STORY_JOIN_MAX] = {};
uint8_t stPick[STORY_STYLES] = { 0, 0 };  // ko11.23: bit i = stJ[i] va al combate (si hay mas de 2); 0x80 = ya elegido
uint8_t stChPage = 0;                     // pagina de la lista de capitulos
bool stPicking = false;                   // eligiendo quien va al combate
static int16_t storyPmdDex = 0;
static bool stLoaded = false;
static uint32_t stRestartArm = 0, stRestartMsg = 0;  // ko11.22: [처음부터]
static uint8_t stBg = 0, stWho = W_NONE;
static int16_t stMon = 0;
static char stText[320];
static uint8_t stPage = 0;
static uint32_t stTypeT = 0;
static bool stChoice = false, stBattleWait = false, stEnd = false, stTired = false;
static char stOpt[3][64];
static uint8_t stOptLabel[3];
static uint8_t stOptN = 2;  // 2 o 3 opciones

static void stLoad() {
  if (stLoaded) return;
  stLoaded = true;
  Preferences p;
  p.begin("tpstory", true);
  // ko11.23: 32 bits ("g2"/"a2"); las partidas de ko11.21-22 tenian 8 bits en "g"/"a"
  stDone[0] = p.isKey("g2") ? p.getUInt("g2", 0) : p.getUChar("g", 0);
  stDone[1] = p.isKey("a2") ? p.getUInt("a2", 0) : p.getUChar("a", 0);
  stResStyle = p.getUChar("rs", 0xFF);
  stResCh = p.getUChar("rc", 0);
  stResStep = p.getUShort("rp", 0);
  rgBest = p.getUShort("rb", 0);
  stPDex[0] = (int16_t)p.getShort("pd0", 0);
  stPDex[1] = (int16_t)p.getShort("pd1", 0);
  stPExp[0] = p.getUInt("pe0", 0);
  stPExp[1] = p.getUInt("pe1", 0);
  if (p.isKey("j0")) {  // ko11.23: la lista entera
    p.getBytes("j0", stJ[0], sizeof(stJ[0]));
    p.getBytes("j1", stJ[1], sizeof(stJ[1]));
  } else {  // ko11.22: dos claves por estilo
    stJ[0][0] = p.getShort("j00", 0); stJ[0][1] = p.getShort("j01", 0);
    stJ[1][0] = p.getShort("j10", 0); stJ[1][1] = p.getShort("j11", 0);
  }
  stPick[0] = p.getUChar("k0", 0);
  stPick[1] = p.getUChar("k1", 0);
  p.end();
}
static void stSave() {
  Preferences p;
  p.begin("tpstory", false);
  p.putUInt("g2", stDone[0]);
  p.putUInt("a2", stDone[1]);
  p.putUChar("rs", stResStyle);
  p.putUChar("rc", stResCh);
  p.putUShort("rp", stResStep);
  p.putUShort("rb", rgBest);
  p.putShort("pd0", stPDex[0]);
  p.putShort("pd1", stPDex[1]);
  p.putUInt("pe0", stPExp[0]);
  p.putUInt("pe1", stPExp[1]);
  p.putBytes("j0", stJ[0], sizeof(stJ[0]));
  p.putBytes("j1", stJ[1], sizeof(stJ[1]));
  p.putUChar("k0", stPick[0]);
  p.putUChar("k1", stPick[1]);
  p.end();
}
static uint8_t stDoneCount(uint8_t s) {
  uint8_t n = 0;
  for (int i = 0; i < STORY_NCH[s]; i++) n += (stDone[s] >> i) & 1;
  return n;
}
static const char *stPetName() { return pet.nick[0] ? pet.nick : dexName(pet.speciesId); }
static int stWrap(const char *s, char lines[][96], int maxLines, int w, uint8_t size);
// boton con el texto centrado en el boton (tamano 2, o 1 si no cabe)
static void stBtn(int x, int y, int w, int h, uint16_t bg, uint16_t fg, const char *t) {
  uiButton(x, y, w, h, 12, bg, UI_INK);
  uint8_t sz = textW(t, 2) <= w - 14 ? 2 : 1;
  gfx->setTextColor(fg);
  setSize(sz);
  int tw = textW(t, sz);
  setCur(x + (w - tw) / 2, y + h / 2 - (sz == 2 ? 12 : 9));
  printT(t);
}
// texto centrado en varias lineas
static void stCentered(const char *t, int y, int w, uint8_t size, uint16_t col, int maxLines) {
  char L[4][96];
  int n = stWrap(t, L, maxLines < 4 ? maxLines : 4, w, size);
  for (int i = 0; i < n; i++) drawFit(L[i], y + i * (size == 1 ? 20 : 28), w, col, size);
}

// ---------------- menu de estilos ----------------
void openStory() {
  stLoad();
  retMark();
  trainMenuOpen = false;
  cardOpen = false;
  xScreen = XS_STORY;
  sfxPlay(SFX_TAP);
}

static const uint16_t ST_STYLE_COL[3] = { C565(0xd8, 0x30, 0x30), C565(0xf0, 0xa8, 0x20), C565(0x5a, 0x4c, 0xd0) };

void renderStoryMenu() {
  uiScreenBg();
  drawFit(STX[SX_TITLE], 34, 300, UI_INK, 3);
  for (int i = 0; i < 3; i++) {
    int y = ST_CARD_Y + i * (ST_CARD_H + ST_CARD_GAP);
    uiButton(73, y, 320, ST_CARD_H, 14, UI_WHITE, UI_INK);
    gfx->fillRoundRect(81, y + 10, 10, ST_CARD_H - 20, 5, ST_STYLE_COL[i]);
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(102, y + 10);
    printT(STORY_STYLE_NAME[i]);
    setSize(1);
    gfx->setTextColor(C565(0x60, 0x68, 0x70));
    setCur(102, y + 42);
    printT(STORY_STYLE_SUB[i]);
    char pr[32];
    if (i < STORY_STYLES && stDoneCount(i) >= STORY_NCH[i]) snprintf(pr, sizeof(pr), "%s", STX[SX_COMPLETE]);  // ko11.23: entera
    else if (i < STORY_STYLES) snprintf(pr, sizeof(pr), STX[SX_PROG_FMT], stDoneCount(i), (unsigned)STORY_NCH[i]);
    else snprintf(pr, sizeof(pr), STX[SX_BEST_FMT], (unsigned)rgBest);
    int w = textW(pr, 1) + 16;
    drawBtn(383 - w, y + 42, w, 26, ST_STYLE_COL[i], UI_WHITE, pr);
  }
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void rogueOpen();
void storyMenuTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { sfxPlay(SFX_TAP); goBack(); return; }
  if (x < 73 || x >= 393 || y < ST_CARD_Y) return;
  int i = (y - ST_CARD_Y) / (ST_CARD_H + ST_CARD_GAP);
  if (i > 2 || (y - ST_CARD_Y) % (ST_CARD_H + ST_CARD_GAP) >= ST_CARD_H) return;
  sfxPlay(SFX_TAP);
  if (i == 2) { rogueOpen(); return; }
  stStyle = (uint8_t)i;
  // ko11.23: la pagina del primer capitulo sin superar
  uint8_t c = 0;
  while (c + 1 < STORY_NCH[stStyle] && ((stDone[stStyle] >> c) & 1)) c++;
  stChPage = c / STORY_PAGE;
  xScreen = XS_STORYCH;
}

// ---------------- lista de capitulos ----------------
static bool stUnlocked(uint8_t s, uint8_t c) { return c == 0 || ((stDone[s] >> (c - 1)) & 1); }

void renderStoryChapters() {
  uiScreenBg();
  drawFit(STORY_STYLE_NAME[stStyle], 34, 300, ST_STYLE_COL[stStyle], 3);
  uint8_t pages = (STORY_NCH[stStyle] + STORY_PAGE - 1) / STORY_PAGE;
  if (stChPage >= pages) stChPage = 0;
  for (int r = 0; r < STORY_PAGE; r++) {
    int i = stChPage * STORY_PAGE + r;
    if (i >= STORY_NCH[stStyle]) break;
    int y = ST_ROW_Y + r * (ST_ROW_H + ST_ROW_GAP);
    bool open = stUnlocked(stStyle, i), done = (stDone[stStyle] >> i) & 1;
    bool res = stResStyle == stStyle && stResCh == i && stResStep > 0;
    uiButton(73, y, 320, ST_ROW_H, 12, open ? UI_WHITE : UI_TRACK, UI_INK);
    gfx->setTextColor(open ? UI_INK : C565(0x90, 0x90, 0x90));
    setSize(2);
    setCur(88, y + 10);
    printT(STORY[stStyle][i].title);
    if (done) drawCheckMark(368, y + ST_ROW_H / 2, true);
    else if (res) drawBtn(300, y + 11, 82, 26, ST_STYLE_COL[stStyle], UI_WHITE, STX[SX_RESUME]);
    else if (!open) stBtn(318, y + 11, 64, 26, UI_TRACK, C565(0x90, 0x90, 0x90), STX[SX_LOCKED]);
  }
  // ko11.22: [처음부터] (dos toques: el 1o arma, 3 s para confirmar)
  bool armed = stRestartArm && millis() - stRestartArm < 3000;
  if (!armed) stRestartArm = 0;
  if (stRestartMsg && millis() - stRestartMsg < 2500) drawFit(STX[SX_RESTART_DONE], ST_RESTART_Y + 8, 300, UI_BAR_OK, 1);
  else stBtn(CX - 80, ST_RESTART_Y, 160, 36, armed ? UI_BAR_BAD : UI_TRACK, armed ? UI_WHITE : UI_INK, STX[armed ? SX_RESTART_SURE : SX_RESTART]);
  // ko11.23: paginas (< 1/3 >)
  if (pages > 1) {
    char pg[16];
    snprintf(pg, sizeof(pg), STX[SX_PAGE_FMT], (unsigned)(stChPage + 1), (unsigned)pages);
    drawFit(pg, 62, 120, C565(0x60, 0x68, 0x70), 1);
    uint16_t cl = stChPage > 0 ? ST_STYLE_COL[stStyle] : UI_TRACK, cr = stChPage + 1 < pages ? ST_STYLE_COL[stStyle] : UI_TRACK;
    uiButton(ST_PG_LX, ST_RESTART_Y, ST_PG_W, 36, 12, cl, UI_INK);
    gfx->fillTriangle(ST_PG_LX + 14, ST_RESTART_Y + 18, ST_PG_LX + 28, ST_RESTART_Y + 9, ST_PG_LX + 28, ST_RESTART_Y + 27, UI_WHITE);
    uiButton(ST_PG_RX, ST_RESTART_Y, ST_PG_W, 36, 12, cr, UI_INK);
    gfx->fillTriangle(ST_PG_RX + 30, ST_RESTART_Y + 18, ST_PG_RX + 16, ST_RESTART_Y + 9, ST_PG_RX + 16, ST_RESTART_Y + 27, UI_WHITE);
  }
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

static void stStart(uint8_t s, uint8_t c);
// ko11.22: un estilo desde cero (los premios ya recibidos se quedan con el que crias)
static void stRestartStyle(uint8_t s) {
  stDone[s] = 0;
  if (stResStyle == s) stResStyle = 0xFF;
  stPDex[s] = 0;
  stPExp[s] = 0;
  for (int i = 0; i < STORY_JOIN_MAX; i++) stJ[s][i] = 0;
  stPick[s] = 0;
  stChPage = 0;
  stSave();
  storyPmd.unload();
  storyPmdDex = 0;
}
void storyChaptersTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { stRestartArm = 0; xScreen = XS_STORY; sfxPlay(SFX_TAP); return; }
  if (inRect(x, y, CX - 80, ST_RESTART_Y, 160, 36)) {
    if (stRestartArm && millis() - stRestartArm < 3000) {
      stRestartStyle(stStyle);
      stRestartArm = 0;
      stRestartMsg = millis() ? millis() : 1;
      sfxPlay(SFX_BYE);
    } else {
      stRestartArm = millis() ? millis() : 1;
      sfxPlay(SFX_DENY);
    }
    return;
  }
  stRestartArm = 0;
  uint8_t pages = (STORY_NCH[stStyle] + STORY_PAGE - 1) / STORY_PAGE;
  if (pages > 1 && y >= ST_RESTART_Y && y < ST_RESTART_Y + 36) {  // ko11.23: paginas
    if (x >= ST_PG_LX && x < ST_PG_LX + ST_PG_W && stChPage > 0) { stChPage--; sfxPlay(SFX_TAP); }
    else if (x >= ST_PG_RX && x < ST_PG_RX + ST_PG_W && stChPage + 1 < pages) { stChPage++; sfxPlay(SFX_TAP); }
    return;
  }
  if (x < 73 || x >= 393 || y < ST_ROW_Y) return;
  int r = (y - ST_ROW_Y) / (ST_ROW_H + ST_ROW_GAP);
  if (r >= STORY_PAGE || (y - ST_ROW_Y) % (ST_ROW_H + ST_ROW_GAP) >= ST_ROW_H) return;
  int i = stChPage * STORY_PAGE + r;
  if (i >= STORY_NCH[stStyle]) return;
  if (!stUnlocked(stStyle, i)) { sfxPlay(SFX_DENY); return; }
  sfxPlay(SFX_TAP);
  stStart(stStyle, (uint8_t)i);
}

// ---------------- motor ----------------
static const SStep &stCur() { return STORY[stStyle][stCh].steps[stStep]; }

static const char *stPartnerName();
static void stShow(const char *tpl, uint8_t who, const char *a2 = nullptr) {
  txFmtRaw(stText, sizeof(stText), tpl, stPartnerName(), a2);
  stWho = who;
  stPage = 0;
  stTypeT = millis();
}

static int16_t stEvolveFor(int16_t d, uint16_t lv) {
  for (int g = 0; g < 3; g++) {
    const DexEntry &e = DEX_TBL[d];
    if (!e.evolvesTo || !e.evolveLevel || lv < e.evolveLevel) break;
    d = e.evolvesTo;
  }
  return d;
}
// el companero del estilo: su forma base, su nivel (con el minimo del capitulo) y su forma de ahora
static int16_t stPartnerBase(uint8_t s) { return stPDex[s] > 0 ? stPDex[s] : STORY_PARTNER0[s]; }
static uint16_t stPartnerLv(uint8_t s) {
  uint16_t lv = levelForExp(stPExp[s]);
  uint8_t fl = STORY_FLOOR[s][stCh < STORY_NCH[s] ? stCh : 0];
  return lv < fl ? fl : lv;
}
// ko11.23: el Pikachu del anime no evoluciona nunca (se nego a la piedra trueno)
static int16_t stPartnerDex(uint8_t s) { return s == 1 ? stPartnerBase(s) : stEvolveFor(stPartnerBase(s), stPartnerLv(s)); }
static const char *stPartnerName() { return dexName(stPartnerDex(stStyle)); }
static void stLoadPartnerPmd() {
  int16_t d = stPartnerDex(stStyle);
  if (storyPmd.loaded && storyPmdDex == d) return;
  storyPmd.unload();
  storyPmdDex = d;
  storyPmd.load((uint8_t)d, false);
}
// el inicial del rival: el que tiene ventaja sobre tu companero (como en Rojo/Azul)
static int16_t stRivalStarter() {
  int16_t b = stPartnerBase(stStyle);
  return b == 1 ? 4 : b == 4 ? 7 : b == 7 ? 1 : 133;
}

// ko11.23: los que se unieron: nivel (2 por debajo del companero) y forma de ahora
static uint16_t stJoinLv() {
  uint16_t lv = stPartnerLv(stStyle);
  return lv > 4 ? lv - 2 : 3;
}
static int16_t stJoinDex(int16_t v) {
  int16_t d = v & ~JOIN_KEEP;
  return (v & JOIN_KEEP) ? d : stEvolveFor(d, stJoinLv());
}
static uint8_t stJoinCount() {
  uint8_t n = 0;
  for (int i = 0; i < STORY_JOIN_MAX; i++) n += stJ[stStyle][i] > 0;
  return n;
}
// quienes van al combate: todos si son 2 o menos; si no, los elegidos (como mucho 2)
static uint8_t stTeamMask() {
  uint8_t n = stJoinCount();
  if (n <= STORY_PICK) return (uint8_t)((1 << n) - 1);
  if (!(stPick[stStyle] & 0x80)) return (uint8_t)((1 << STORY_PICK) - 1);  // sin elegir aun: los 2 primeros
  uint8_t m = stPick[stStyle] & (uint8_t)((1 << n) - 1), c = 0;
  for (int i = 0; i < n; i++) if ((m >> i) & 1) { if (++c > STORY_PICK) m &= (uint8_t)~(1 << i); }
  return m;
}

static void stSetResume() {
  stResStyle = stStyle; stResCh = stCh; stResStep = stStep;
  stSave();
}

// ejecuta pasos hasta uno que se ve (dialogo, eleccion, combate, premio o final)
static void stRun() {
  stChoice = stBattleWait = stEnd = stTired = false;
  const SChapter &ch = STORY[stStyle][stCh];
  for (int guard = 0; guard < 400 && stStep < ch.n; guard++) {
    const SStep &s = ch.steps[stStep];
    switch (s.op) {
      case ST_BG: stBg = s.a < REGION_COUNT ? s.a : 0; stStep++; continue;
      case ST_MON: stMon = s.c; stStep++; continue;
      case ST_LABEL: stStep++; continue;
      case ST_STARTER:  // el companero de este estilo (empieza en el nivel del capitulo)
        // (si se repite el capitulo con el mismo, conserva lo que ya crecio)
        if (stPDex[stStyle] != s.c) stPExp[stStyle] = expForLevel(STORY_FLOOR[stStyle][stCh]);
        stPDex[stStyle] = s.c;
        stSave();
        stLoadPartnerPmd();
        stStep++;
        continue;
      case ST_GOTO: {
        uint16_t k = 0;
        while (k < ch.n && !(ch.steps[k].op == ST_LABEL && ch.steps[k].a == s.a)) k++;
        stStep = k < ch.n ? k : stStep + 1;
        continue;
      }
      case ST_JOIN: {  // ko11.22: se une al equipo de la historia (si ya estaba, nada)
        int8_t k = -1;
        bool have = false;
        for (int8_t i = 0; i < STORY_JOIN_MAX; i++) {
          if (stJ[stStyle][i] > 0 && (stJ[stStyle][i] & ~JOIN_KEEP) == s.c) have = true;
          if (stJ[stStyle][i] <= 0 && k < 0) k = i;
        }
        if (!have && k >= 0) { stJ[stStyle][k] = (int16_t)(s.c | (s.a ? JOIN_KEEP : 0)); stSave(); }
        txFmtRaw(stText, sizeof(stText), STX[SX_JOIN_FMT], dexName(s.c), nullptr);
        stWho = W_NONE; stPage = 0; stTypeT = millis();
        sfxPlay(SFX_MEDAL);
        return;
      }
      case ST_LEAVE: {  // ko11.23: se despide (con la forma que tenga ahora)
        int16_t now = s.c;
        for (int8_t i = 0; i < STORY_JOIN_MAX; i++) {
          if (stJ[stStyle][i] > 0 && (stJ[stStyle][i] & ~JOIN_KEEP) == s.c) {
            now = stJoinDex(stJ[stStyle][i]);
            for (int8_t j = i; j + 1 < STORY_JOIN_MAX; j++) stJ[stStyle][j] = stJ[stStyle][j + 1];
            stJ[stStyle][STORY_JOIN_MAX - 1] = 0;
            stPick[stStyle] = 0;  // la lista cambio: se vuelve a elegir
            stSave();
            break;
          }
        }
        txFmtRaw(stText, sizeof(stText), STX[SX_LEAVE_FMT], dexName(now), nullptr);
        stWho = W_NONE; stPage = 0; stTypeT = millis();
        sfxPlay(SFX_BYE);
        return;
      }
      case ST_SAY: stShow(s.t, s.who); return;
      case ST_NARR: stShow(s.t, W_NONE); return;
      case ST_CHOICE: {
        char buf[200];
        txFmtRaw(buf, sizeof(buf), s.t, stPartnerName(), nullptr);
        char *p1 = strchr(buf, '|'), *p2 = p1 ? strchr(p1 + 1, '|') : nullptr, *p3 = p2 ? strchr(p2 + 1, '|') : nullptr;
        if (p1) *p1 = 0;
        if (p2) *p2 = 0;
        if (p3) *p3 = 0;
        snprintf(stText, sizeof(stText), "%s", buf);
        snprintf(stOpt[0], sizeof(stOpt[0]), "%s", p1 ? p1 + 1 : "");
        snprintf(stOpt[1], sizeof(stOpt[1]), "%s", p2 ? p2 + 1 : "");
        snprintf(stOpt[2], sizeof(stOpt[2]), "%s", p3 ? p3 + 1 : "");
        stOptLabel[0] = s.a; stOptLabel[1] = s.b; stOptLabel[2] = (uint8_t)s.c;
        stOptN = p3 ? 3 : 2;
        stWho = W_NONE; stPage = 0; stTypeT = millis();
        stChoice = true;
        return;
      }
      case ST_BATTLE: {
        if (s.who == W_NONE) stShow(STX[SX_WILD_GROUP], W_NONE);
        else stShow(STX[SX_VS_FMT], s.who), txFmtRaw(stText, sizeof(stText), STX[SX_VS_FMT], STORY_WHO_NAME[s.who], nullptr);
        stBattleWait = true;
        stSetResume();
        return;
      }
      case ST_GIVE: {
        char t[96];
        if (s.a == SG_BALL) { pet.giveItems((uint8_t)s.b, 0); snprintf(t, sizeof(t), STX[SX_GOT_BALL], (unsigned)s.b); }
        else if (s.a == SG_POTION) { pet.giveItems(0, (uint8_t)s.b); snprintf(t, sizeof(t), STX[SX_GOT_POTION], (unsigned)s.b); }
        else if (s.a == SG_CANDY) {
          int16_t d = s.c ? s.c : pet.speciesId;
          pet.addCandy(d, s.b);
          char f[48];
          snprintf(f, sizeof(f), "%s", dexName(DEX_FAM[d]));
          txFmtRaw(t, sizeof(t), STX[SX_GOT_CANDY], f, nullptr);
          char t2[96];
          snprintf(t2, sizeof(t2), t, (unsigned)s.b);
          snprintf(t, sizeof(t), "%s", t2);
        } else if (s.a == SG_RARE) { if (pet.rareCandy < 999) pet.rareCandy++; snprintf(t, sizeof(t), "%s", STX[SX_GOT_RARE]); }
        else { pet.addExp(s.b); snprintf(t, sizeof(t), STX[SX_GOT_EXP], (unsigned)s.b); }
        pet.saveNow();
        snprintf(stText, sizeof(stText), "%s", t);
        stWho = W_NONE; stPage = 0; stTypeT = millis();
        sfxPlay(SFX_MEDAL);
        return;
      }
      case ST_END: {
        stDone[stStyle] |= (uint32_t)1 << stCh;
        if (stResStyle == stStyle && stResCh == stCh) stResStyle = 0xFF;
        stSave();
        stMon = 0;
        txFmtRaw(stText, sizeof(stText), STX[SX_CLEAR_FMT], STORY[stStyle][stCh].title, nullptr);
        stWho = W_NONE; stPage = 0; stTypeT = millis();
        stEnd = true;
        sfxPlay(SFX_EVOLVE);
        return;
      }
      default: stStep++; continue;
    }
  }
  stEnd = true;
}

static void stStart(uint8_t s, uint8_t c) {
  stStyle = s; stCh = c;
  stStep = (stResStyle == s && stResCh == c) ? stResStep : 0;
  stBg = 0; stMon = 0;
  // el fondo y el Pokemon en escena que tocaban en el punto guardado
  const SChapter &ch = STORY[s][c];
  for (uint16_t k = 0; k < stStep && k < ch.n; k++) {
    if (ch.steps[k].op == ST_BG) stBg = ch.steps[k].a;
    if (ch.steps[k].op == ST_MON) stMon = ch.steps[k].c;
  }
  xScreen = XS_SCENE;
  stPicking = false;
  stLoadPartnerPmd();
  stRun();
}

// ---------------- texto: lineas que caben (palabras; si una no cabe, por letras) ----------------
static int stWrap(const char *s, char lines[][96], int maxLines, int w, uint8_t size) {
  int n = 0;
  char cur[96] = "";
  const char *p = s;
  while (*p && n < maxLines) {
    const char *q = p;
    while (*q && *q != ' ') q++;
    char word[96];
    size_t wl = (size_t)(q - p) < sizeof(word) - 1 ? (size_t)(q - p) : sizeof(word) - 1;
    memcpy(word, p, wl); word[wl] = 0;
    char trial[192];
    snprintf(trial, sizeof(trial), "%s%s%s", cur, cur[0] ? " " : "", word);
    if (textW(trial, size) <= w) {
      snprintf(cur, sizeof(cur), "%s", trial);
    } else if (cur[0]) {
      snprintf(lines[n++], 96, "%s", cur);
      snprintf(cur, sizeof(cur), "%s", word);
    } else {  // una palabra sola demasiado larga: se corta por letras
      size_t k = 0;
      while (word[k]) {
        size_t cl = ((uint8_t)word[k] < 0x80) ? 1 : ((uint8_t)word[k] >> 5) == 6 ? 2 : ((uint8_t)word[k] >> 4) == 14 ? 3 : 4;
        char t2[96];
        memcpy(t2, word, k + cl); t2[k + cl] = 0;
        if (textW(t2, size) > w && k) break;
        k += cl;
      }
      memcpy(lines[n], word, k); lines[n][k] = 0; n++;
      snprintf(cur, sizeof(cur), "%s", word + k);
    }
    p = *q ? q + 1 : q;
  }
  if (cur[0] && n < maxLines) snprintf(lines[n++], 96, "%s", cur);
  return n;
}

static int utf8Count(const char *s) {
  int n = 0;
  for (; *s; s++) if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}
static void utf8Cut(char *s, int chars) {
  int n = 0;
  for (char *p = s; *p; p++) {
    if (((uint8_t)*p & 0xC0) != 0x80) { if (n == chars) { *p = 0; return; } n++; }
  }
}

static int stLineCount() {
  char L[12][96];
  return stWrap(stText, L, 12, ST_LINE_W, 2);
}
static bool stTyping() {
  char L[12][96];
  int n = stWrap(stText, L, 12, ST_LINE_W, 2);
  int total = 0;
  for (int i = stPage * ST_LINES; i < n && i < stPage * ST_LINES + ST_LINES; i++) total += utf8Count(L[i]);
  return (int)((millis() - stTypeT) / ST_TYPE_MS) < total;
}

// ---------------- escena ----------------
// ko11.23: antes de un combate, si hay mas de 2 que se unieron: quienes van (como mucho 2)
#define SP_CW 104
#define SP_CH 92
#define SP_X0 69
#define SP_Y0 60
static int stPickCard(int16_t x, int16_t y) {
  for (int i = 0; i < stJoinCount(); i++) {
    int cx = SP_X0 + (i % 3) * (SP_CW + 8), cy = SP_Y0 + (i / 3) * (SP_CH + 8);
    if (inRect(x, y, cx, cy, SP_CW, SP_CH)) return i;
  }
  return -1;
}
static void stRenderPick() {
  uint8_t m = stTeamMask();
  uint16_t lv = stJoinLv();
  for (int i = 0; i < stJoinCount(); i++) {
    int cx = SP_X0 + (i % 3) * (SP_CW + 8), cy = SP_Y0 + (i / 3) * (SP_CH + 8);
    bool on = (m >> i) & 1;
    uiButton(cx, cy, SP_CW, SP_CH, 12, on ? C565(0xff, 0xf0, 0xb0) : UI_WHITE, on ? ST_STYLE_COL[stStyle] : UI_INK);
    int16_t d = stJoinDex(stJ[stStyle][i]);
    drawThumbAt(d, cx + SP_CW / 2, cy + 36, 2, false);
    char t[40];
    snprintf(t, sizeof(t), "%s Lv%u", dexName(d), (unsigned)lv);
    gfx->setTextColor(UI_INK);
    setSize(1);
    setCur(cx + (SP_CW - textW(t, 1)) / 2, cy + SP_CH - 24);
    printT(t);
    if (on) drawCheckMark(cx + SP_CW - 14, cy + 14, true);
  }
  uiPanel(ST_BOX_X, ST_BOX_Y, ST_BOX_W, ST_BOX_H, 14, UI_WHITE, UI_INK);
  drawFit(STX[SX_PICK], ST_BOX_Y + 14, ST_LINE_W, UI_INK, 2);
  stBtn(CX - 70, ST_BOX_Y + 58, 140, 40, ST_STYLE_COL[stStyle], UI_WHITE, STX[SX_GO]);
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void renderStoryScene() {
  uint32_t now = millis();
  lastInteract = now;
  bRegion = stBg; bLink = false;
  bvShakeX = bvShakeY = 0;
  drawBattleBg();
  // el companero de la historia a la izquierda (aun no, antes de recibirlo)
  if (stPDex[stStyle] <= 0) {
  } else if (storyPmd.loaded && storyPmd.has(PMD_IDLE)) drawPmdActM(storyPmd, PMD_IDLE, 132, 258, now, true, false, 4, 170);
  else drawThumbAt(stPartnerDex(stStyle), 132, 220, 3, false);
  // el Pokemon de la escena (si hay) y quien habla a la derecha
  // el Pokemon de la escena, salvo que sea el propio companero (ya esta a la izquierda; ko11.23.1)
  bool monIsPartner = stPDex[stStyle] > 0 && (stMon == stPartnerDex(stStyle) || stMon == stPartnerBase(stStyle));
  if (stMon > 0 && !monIsPartner) drawThumbAt(stMon, 250, 214, 2, false);
  if (stWho != W_NONE && stWho != W_PET) {
    const uint8_t *b = portraits.get(stWho);
    if (b) drawThumb(b, 330 - b[0], 262 - b[1] * 2, 2, false);
    else gfx->fillCircle(330, 200, 40, UI_TRACK);  // sin story.bin en la SD
  }
  if (stPicking) { stRenderPick(); return; }
  // cuadro de dialogo
  uiPanel(ST_BOX_X, ST_BOX_Y, ST_BOX_W, ST_BOX_H, 14, UI_WHITE, UI_INK);
  const char *name = stWho == W_PET ? stPartnerName() : stWho != W_NONE ? STORY_WHO_NAME[stWho] : nullptr;
  if (name && name[0]) {
    int w = textW(name, 2) + 24;
    drawBtn(ST_BOX_X + 12, ST_BOX_Y - 22, w, 32, stWho == W_PET ? UI_BAR_OK : ST_STYLE_COL[stStyle], UI_WHITE, name);
  }
  char L[12][96];
  int n = stWrap(stText, L, 12, ST_LINE_W, 2);
  int shown = (int)((now - stTypeT) / ST_TYPE_MS);
  gfx->setTextColor(stTired ? UI_BAR_BAD : UI_INK);
  setSize(2);
  int lines = stChoice ? 1 : ST_LINES;
  for (int i = 0; i < lines && stPage * ST_LINES + i < n; i++) {
    char t[96];
    snprintf(t, sizeof(t), "%s", L[stPage * ST_LINES + i]);
    int c = utf8Count(t);
    if (shown < c) utf8Cut(t, shown < 0 ? 0 : shown);
    shown -= c;
    setCur(ST_BOX_X + 18, ST_BOX_Y + 16 + i * 30);
    printT(t);
    if (shown <= 0) break;
  }
  if (stChoice && stOptN == 3) {  // tres opciones cortas en fila (los iniciales)
    static const uint16_t c3[3] = { C565(0x48, 0xa8, 0x58), C565(0xe0, 0x60, 0x30), C565(0x38, 0x80, 0xd8) };
    for (int k = 0; k < 3; k++) stBtn(ST_BOX_X + 12 + k * 118, ST_BOX_Y + 52, 110, 48, c3[k], UI_WHITE, stOpt[k]);
  } else if (stChoice) {
    stBtn(ST_BOX_X + 12, ST_BOX_Y + 42, ST_BOX_W - 24, 32, ST_STYLE_COL[stStyle], UI_WHITE, stOpt[0]);
    stBtn(ST_BOX_X + 12, ST_BOX_Y + 78, ST_BOX_W - 24, 32, 0x4C98, UI_WHITE, stOpt[1]);
  } else if (!stTyping()) {
    if (stBattleWait) drawFit(STX[SX_TAP_BATTLE], ST_BOX_Y + ST_BOX_H - 26, 300, ST_STYLE_COL[stStyle], 1);
    else {  // triangulo: toca para seguir
      int tx = ST_BOX_X + ST_BOX_W - 26, ty = ST_BOX_Y + ST_BOX_H - 22 + (int)((now / 300) % 2) * 3;
      gfx->fillTriangle(tx, ty, tx + 12, ty, tx + 6, ty + 8, UI_INK);
    }
  }
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

static void stStartBattle();
void storySceneTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) {  // salir: el punto se guarda
    stPicking = false;
    if (!stEnd) { stSetResume(); }
    xScreen = XS_STORYCH;
    sfxPlay(SFX_TAP);
    return;
  }
  if (stPicking) {  // ko11.23: elegir quien va
    int k = stPickCard(x, y);
    if (k >= 0) {
      uint8_t m = stTeamMask(), c = 0;
      for (int i = 0; i < STORY_JOIN_MAX; i++) c += (m >> i) & 1;
      if ((m >> k) & 1) m &= (uint8_t)~(1 << k);
      else if (c < STORY_PICK) m |= (uint8_t)(1 << k);
      else { sfxPlay(SFX_DENY); return; }
      stPick[stStyle] = m | 0x80;
      sfxPlay(SFX_TAP);
      return;
    }
    if (inRect(x, y, CX - 70, ST_BOX_Y + 58, 140, 40)) {
      stPicking = false;
      stPick[stStyle] = stTeamMask() | 0x80;
      stSave();
      sfxPlay(SFX_TAP);
      stStartBattle();
    }
    return;
  }
  if (stTyping()) { stTypeT = millis() - 600000UL; return; }  // mostrar todo de golpe
  if (stChoice) {
    if (y < ST_BOX_Y + 40 || y > ST_BOX_Y + 112) return;
    int k;
    if (stOptN == 3) {
      if (x < ST_BOX_X + 12 || x >= ST_BOX_X + 12 + 3 * 118) return;
      k = (x - ST_BOX_X - 12) / 118;
    } else k = y < ST_BOX_Y + 76 ? 0 : 1;
    sfxPlay(SFX_TAP);
    uint8_t lab = stOptLabel[k];
    const SChapter &ch = STORY[stStyle][stCh];
    uint16_t j = 0;
    while (j < ch.n && !(ch.steps[j].op == ST_LABEL && ch.steps[j].a == lab)) j++;
    stStep = j < ch.n ? j : stStep + 1;
    stRun();
    return;
  }
  if ((stPage + 1) * ST_LINES < stLineCount()) { stPage++; stTypeT = millis(); return; }  // siguiente trozo
  sfxPlay(SFX_TAP);
  if (stEnd) { xScreen = XS_STORYCH; return; }
  if (stBattleWait) {
    // ko11.23: mas de 2 companeros de viaje: primero se elige quien va
    if (stJoinCount() > STORY_PICK && stCur().a < STORY_TEAM_COUNT) { stPicking = true; return; }
    stStartBattle();
    return;
  }
  if (stTired) { xScreen = XS_STORYCH; return; }
  stStep++;
  stRun();
}

// ---------------- combates de la historia ----------------
uint8_t stBattleWho = W_NONE;
static void stStartBattle() {
  const SStep &s = stCur();
  if (s.a >= STORY_TEAM_COUNT) { stStep++; stRun(); return; }
  if (!pet.canBattle() || pet.tooTiredToBattle()) {  // cansado: se descansa y se vuelve
    stSetResume();
    txFmtRaw(stText, sizeof(stText), STX[SX_TIRED], stPetName(), nullptr);  // el que crias es el cansado
    stWho = W_NONE; stPage = 0; stTypeT = millis();
    stBattleWait = false;
    stTired = true;
    sfxPlay(SFX_DENY);
    return;
  }
  const STeam &tm = STORY_TEAMS[s.a];
  Battler team[STORY_TEAM_MAX];
  for (uint8_t i = 0; i < tm.n && i < STORY_TEAM_MAX; i++) {
    int lv = tm.lv[i];  // niveles fijos, como en el original
    if (lv < 3) lv = 3;
    if (lv > LEVEL_MAX) lv = LEVEL_MAX;
    // el inicial del rival evoluciona con su nivel; los demas, tal cual (ko11.23: el Bulbasaur del anime no)
    int16_t d = tm.dex[i] < 0 ? stEvolveFor(stRivalStarter(), (uint16_t)lv) : tm.dex[i];
    team[i] = makeTrainerMon(d, (uint16_t)lv);
    // ko11.23.2: la historia algo mas facil (el usuario perdia casi siempre al principio): -15 % en todo
    Battler &b = team[i];
    b.maxHp = b.hp = (uint16_t)(b.maxHp * (100 - STORY_FOE_EASE) / 100 > 0 ? b.maxHp * (100 - STORY_FOE_EASE) / 100 : 1);
    b.atk = (uint16_t)(b.atk * (100 - STORY_FOE_EASE) / 100 > 0 ? b.atk * (100 - STORY_FOE_EASE) / 100 : 1);
    b.def = (uint16_t)(b.def * (100 - STORY_FOE_EASE) / 100 > 0 ? b.def * (100 - STORY_FOE_EASE) / 100 : 1);
    b.spe = (uint16_t)(b.spe * (100 - STORY_FOE_EASE) / 100 > 0 ? b.spe * (100 - STORY_FOE_EASE) / 100 : 1);
  }
  stBattleWho = s.who;
  // ko11.22: sin ayudantes de la caja: luchan el companero y los que se unieron en la historia
  ppArmed = false;
  xScreen = XS_NONE;
  startTrainer(BK_STORY, tm.region, team, tm.n);
  if (xScreen != XS_WILD) xScreen = XS_SCENE;
}

// startTrainer: los que se unieron en la historia, detras del companero (un poco por debajo de su nivel)
void storyAddParty() {
  uint16_t lv = stJoinLv();
  uint8_t m = stTeamMask();
  for (uint8_t i = 0; i < STORY_JOIN_MAX && pN < PARTY_MAX; i++) {
    int16_t v = stJ[stStyle][i];
    if (v <= 0 || !((m >> i) & 1)) continue;
    pMon[pN] = makeBoxBattler(stJoinDex(v), lv, lv, 105, 105, 105);
    pBox[pN] = -1;
    pN++;
  }
}

// startTrainer: quien lucha en primer lugar (historia: el companero; expedicion: el inicial elegido)
static uint16_t rgPartnerLv();
static int16_t rgPartnerDex();
Battler storyPartnerBattler() {
  int16_t d;
  uint16_t lv;
  if (bKind == BK_ROGUE) { d = rgPartnerDex(); lv = rgPartnerLv(); }
  else { d = stPartnerDex(stStyle); lv = stPartnerLv(stStyle); }
  return makeBoxBattler(d, lv, lv, 110, 110, 110);
}

// vuelta del combate (afterResult): ganado -> sigue; perdido -> se puede repetir
void storyAfterBattle(bool won) {
  xScreen = XS_SCENE;
  if (won) {  // el companero sube un nivel (los premios del combate ya fueron al que crias)
    uint16_t lv = stPartnerLv(stStyle);
    if (lv < LEVEL_MAX) stPExp[stStyle] = expForLevel(lv + 1);
    stSave();
    stLoadPartnerPmd();
    stStep++;
    stRun();
    return;
  }
  if (stStep < STORY[stStyle][stCh].n && stCur().op == ST_BATTLE && stCur().b == 1) {  // ko11.23: sigue igual
    stStep++;
    stRun();
    return;
  }
  stShow(STX[SX_LOST], W_NONE);
  stBattleWait = true;
}

// la presentacion del combate (startTrainer / nextTrainerMon)
const char *rogueFoeLabel();
const char *storyFoeName() {
  if (bKind == BK_ROGUE) return rogueFoeLabel();
  return stBattleWho != W_NONE ? STORY_WHO_NAME[stBattleWho] : STX[SX_WILD_GROUP];
}

// ======================================================================
// Expedicion (estilo PokeRogue): oleadas seguidas por regiones que cambian cada 5,
// entrenador cada 5 y jefe cada 10. Entre oleadas se elige un premio. El equipo
// (tu Pokemon + ayudantes elegidos al empezar) conserva la vida de una oleada a otra.
// Si el Pokemon se cansa, la expedicion se guarda y se sigue luego.
// ======================================================================
static const uint8_t RG_ROUTE[8] = { 0, 2, 15, 1, 6, 8, 3, 13 };  // 초원 숲 광산 바닷가 발전소 늪 화산 용의 계곡
enum : uint8_t { RG_HUB = 0, RG_REWARD, RG_OVER, RG_PICK };
// ko11.21: la expedicion se hace con un inicial propio que crece una vez por oleada
static const int16_t RG_STARTERS[5] = { 1, 4, 7, 25, 133 };
int16_t rgStarter = 1;
uint8_t rgPhase = RG_HUB;
bool rgOn = false;
uint16_t rgWave = 1;
uint16_t rgHp[PARTY_MAX];     // vida guardada de cada miembro (0 = debilitado)
uint16_t rgMax[PARTY_MAX];    // y su maximo
uint8_t rgHpOk = 0;           // bit i = rgHp[i] valido
static char rgLabel[64];
static char rgResult[96];
static bool rgNewBest = false;

static void rgSave() {
  Preferences p;
  p.begin("tpstory", false);
  p.putBool("ro", rgOn);
  p.putUShort("rw", rgWave);
  p.putBytes("rh", rgHp, sizeof(rgHp));
  p.putBytes("rm", rgMax, sizeof(rgMax));
  p.putUChar("rk", rgHpOk);
  p.putUShort("rb", rgBest);
  p.putShort("rd", rgStarter);
  p.end();
}
static void rgLoad() {
  stLoad();
  static bool done = false;
  if (done) return;
  done = true;
  Preferences p;
  p.begin("tpstory", true);
  rgOn = p.getBool("ro", false);
  rgWave = p.getUShort("rw", 1);
  if (p.isKey("rh")) p.getBytes("rh", rgHp, sizeof(rgHp));
  if (p.isKey("rm")) p.getBytes("rm", rgMax, sizeof(rgMax));
  rgHpOk = p.getUChar("rk", 0);
  rgStarter = (int16_t)p.getShort("rd", 1);
  p.end();
}
static uint8_t rgRegion(uint16_t w) { return RG_ROUTE[((w - 1) / 5) % 8]; }
static uint16_t rgPartnerLv() { uint16_t lv = 4 + rgWave; return lv > LEVEL_MAX ? LEVEL_MAX : lv; }
static int16_t rgPartnerDex() { return stEvolveFor(rgStarter > 0 ? rgStarter : 1, rgPartnerLv()); }

void rogueOpen() {
  rgLoad();
  rgPhase = RG_HUB;
  xScreen = XS_ROGUE;
}

// la oleada w: salvaje (1), entrenador cada 5 (2) o jefe cada 10 (3, mas fuerte)
static uint8_t rgMakeTeam(uint16_t w, Battler *team) {
  uint8_t reg = rgRegion(w);
  int lv = (int)rgPartnerLv() - 1;  // un poco por debajo del inicial
  bool boss = w % 10 == 0, trainer = !boss && w % 5 == 0;
  if (boss) lv += 3;
  else if (trainer) lv += 1;
  if (lv < 3) lv = 3;
  if (lv > LEVEL_MAX) lv = LEVEL_MAX;
  uint8_t n = boss ? 3 : trainer ? 2 : 1;
  BRng rng(esp_random() | 1);
  for (uint8_t i = 0; i < n; i++) {
    uint8_t g;
    Battler b = makeWildIn(reg, (uint16_t)lv, sceneHour(), WX_CLEAR, 0, rng, &g);
    team[i] = makeTrainerMon(b.dex, (uint16_t)(lv + (boss && i == n - 1 ? 2 : 0)));
  }
  if (boss) txFmtRaw(rgLabel, sizeof(rgLabel), RGX[RX_BOSS_FMT], XT((XId)(X_REG_0 + reg)), nullptr);
  else if (trainer) snprintf(rgLabel, sizeof(rgLabel), "%s", RGX[RX_TRAINER]);
  else txFmtRaw(rgLabel, sizeof(rgLabel), RGX[RX_WILD_FMT], dexName(team[0].dex), nullptr);
  return n;
}

const char *rogueFoeLabel() { return rgLabel; }

static bool rgCheckTired() {
  if (pet.canBattle() && !pet.tooTiredToBattle()) return false;
  txFmtRaw(rgResult, sizeof(rgResult), RGX[RX_TIRED], stPetName(), nullptr);
  sfxPlay(SFX_DENY);
  return true;
}

static void rgStartWave(bool first) {
  rgResult[0] = 0;
  if (rgCheckTired()) return;
  Battler team[3];
  uint8_t n = rgMakeTeam(rgWave, team);
  stBattleWho = W_NONE;
  if (first) { partyOpen(BK_ROGUE, rgRegion(rgWave), team, n, XS_ROGUE); return; }
  ppArmed = true;  // los mismos ayudantes de la expedicion
  xScreen = XS_NONE;
  startTrainer(BK_ROGUE, rgRegion(rgWave), team, n);
  if (xScreen != XS_WILD) xScreen = XS_ROGUE;
}

// startTrainer: al empezar una oleada, la vida que traia cada uno
void rogueApplyParty() {
  if (!rgOn) return;
  for (uint8_t i = 0; i < pN; i++)
    if ((rgHpOk >> i) & 1) pMon[i].hp = rgHp[i] > pMon[i].maxHp ? pMon[i].maxHp : rgHp[i];
  uint8_t first = 0;
  while (first < pN && pMon[first].hp == 0) first++;
  if (first >= pN) first = 0;
  pCur = first;
  pUsed = (uint8_t)(1 << first);
  bMe = pMon[first];
}
// antes de deshacer el equipo (afterResult): guardar la vida de todos
void rogueSaveParty() {
  pMon[pCur] = bMe;
  rgHpOk = 0;
  for (uint8_t i = 0; i < pN && i < PARTY_MAX; i++) { rgHp[i] = pMon[i].hp; rgMax[i] = pMon[i].maxHp; rgHpOk |= (uint8_t)(1 << i); }
}

void rogueAfterBattle(bool won) {
  xScreen = XS_ROGUE;
  if (won) {
    snprintf(rgResult, sizeof(rgResult), RGX[RX_WON_FMT], (unsigned)rgWave);
    if (rgWave > rgBest) { rgBest = rgWave; rgNewBest = true; }
    rgWave++;
    rgPhase = RG_REWARD;
    rgSave();
    return;
  }
  // fin: premio segun lo lejos que se llego
  uint16_t cleared = rgWave - 1;
  uint16_t exp = cleared * 8, candy = cleared / 2;
  if (exp) pet.addExp(exp);
  if (candy) pet.addCandy(pet.speciesId, candy);
  pet.saveNow();
  snprintf(rgResult, sizeof(rgResult), RGX[RX_RESULT_FMT], (unsigned)cleared, (unsigned)exp, (unsigned)candy);
  rgOn = false;
  rgWave = 1;
  rgHpOk = 0;
  rgPhase = RG_OVER;
  rgSave();
  sfxPlay(SFX_BYE);
}

static void rgTeamBar(int y) {  // la vida del equipo (de lo guardado)
  for (uint8_t i = 0; i < PARTY_MAX; i++) {
    if (!((rgHpOk >> i) & 1)) continue;
    int x = 103 + i * 90;
    gfx->drawRoundRect(x, y, 80, 10, 4, UI_INK);
    uint16_t w = rgMax[i] ? (uint16_t)((uint32_t)76 * rgHp[i] / rgMax[i]) : 0;
    gfx->fillRoundRect(x + 2, y + 2, w, 6, 3, rgHp[i] ? UI_BAR_OK : UI_BAR_BAD);
  }
}

#define RG_BTN_Y 300
#define RG_PICK_X 55
#define RG_PICK_STEP 74
void renderRogue() {
  uiScreenBg();
  drawFit(STORY_STYLE_NAME[2], 34, 300, ST_STYLE_COL[2], 3);
  char t[64];
  snprintf(t, sizeof(t), RGX[RX_BEST_FMT], (unsigned)rgBest);
  drawFit(t, 76, 300, UI_INK, 2);
  if (rgPhase == RG_PICK) {  // el inicial de esta expedicion
    drawFit(RGX[RX_STARTER], 130, 330, UI_INK, 2);
    for (int i = 0; i < 5; i++) {
      int x = RG_PICK_X + i * RG_PICK_STEP;
      uiButton(x, 170, RG_PICK_STEP - 6, 120, 12, rgStarter == RG_STARTERS[i] ? C565(0xe8, 0xe4, 0xff) : UI_WHITE, UI_INK);
      drawThumbAt(RG_STARTERS[i], x + 34, 218, 2, false);
      const char *nm = dexName(RG_STARTERS[i]);
      gfx->setTextColor(UI_INK);
      setSize(1);
      setCur(x + 34 - textW(nm, 1) / 2, 260);
      printT(nm);
    }
  } else if (rgPhase == RG_REWARD) {
    drawFit(rgResult, 114, 320, UI_BAR_OK, 3);
    if (rgNewBest) drawFit(RGX[RX_NEWBEST], 150, 300, UI_BAR_WARN, 2);
    drawFit(RGX[RX_PICK], 184, 320, UI_INK, 2);
    const char *opt[3] = { RGX[RX_HEAL], RGX[RX_BALL], RGX[RX_CANDY] };
    uint16_t col[3] = { UI_BAR_OK, UI_BAR_BAD, C565(0xf0, 0x7a, 0xa8) };
    for (int i = 0; i < 3; i++) stBtn(73 + i * 110, 222, 100, 60, col[i], UI_WHITE, opt[i]);
  } else if (rgPhase == RG_OVER) {
    drawFit(RGX[RX_OVER], 130, 320, UI_INK, 4);
    stCentered(rgResult, 186, 330, 2, UI_INK, 2);
    if (rgNewBest) drawFit(RGX[RX_NEWBEST], 222, 300, UI_BAR_WARN, 2);
    drawBtn(133, RG_BTN_Y, 200, 52, ST_STYLE_COL[2], UI_WHITE, RGX[RX_START]);
  } else {
    stCentered(RGX[RX_INFO], 108, 330, 1, C565(0x60, 0x68, 0x70), 2);
    if (rgOn) {
      snprintf(t, sizeof(t), RGX[RX_WAVE_FMT], (unsigned)rgWave);
      drawFit(t, 160, 300, UI_INK, 3);
      drawFit(XT((XId)(X_REG_0 + rgRegion(rgWave))), 204, 300, ST_STYLE_COL[2], 2);
      rgTeamBar(236);
      char pl[64];
      snprintf(pl, sizeof(pl), "%s Lv.%u", dexName(rgPartnerDex()), (unsigned)rgPartnerLv());
      drawFit(pl, 256, 300, UI_INK, 1);
      drawBtn(83, RG_BTN_Y, 150, 52, ST_STYLE_COL[2], UI_WHITE, RGX[RX_CONT]);
      drawBtn(243, RG_BTN_Y, 140, 52, UI_TRACK, UI_INK, RGX[RX_GIVEUP]);
    } else {
      drawBtn(133, RG_BTN_Y, 200, 52, ST_STYLE_COL[2], UI_WHITE, RGX[RX_START]);
    }
    if (rgResult[0]) stCentered(rgResult, 360, 320, 1, UI_BAR_BAD, 2);
  }
  drawNav(NAV_L, UI_INK);
  uiFlush();
}

void rogueTap(int16_t x, int16_t y) {
  if (navHit(NAV_L, x, y)) { if (rgPhase == RG_PICK) { rgPhase = RG_HUB; sfxPlay(SFX_TAP); return; } rgPhase = RG_HUB; xScreen = XS_STORY; sfxPlay(SFX_TAP); return; }
  if (rgPhase == RG_REWARD) {
    if (y < 222 || y > 282 || x < 73 || x >= 403) return;
    int k = (x - 73) / 110;
    if (k == 0) {  // +40 % de vida para todos (los debilitados vuelven)
      for (uint8_t i = 0; i < PARTY_MAX; i++)
        if ((rgHpOk >> i) & 1) {
          uint32_t h = rgHp[i] + (uint32_t)rgMax[i] * 40 / 100;
          rgHp[i] = (uint16_t)(h > rgMax[i] ? rgMax[i] : h);
        }
    } else if (k == 1) pet.giveItems(1, 0);
    else pet.addCandy(pet.speciesId, 2);
    pet.saveNow();
    rgNewBest = false;
    rgPhase = RG_HUB;
    rgSave();
    sfxPlay(SFX_MEDAL);
    rgStartWave(false);
    return;
  }
  if (rgPhase == RG_PICK) {
    if (y < 170 || y > 290 || x < RG_PICK_X || x >= RG_PICK_X + 5 * RG_PICK_STEP) return;
    rgStarter = RG_STARTERS[(x - RG_PICK_X) / RG_PICK_STEP];
    sfxPlay(SFX_TAP);
    rgPhase = RG_HUB;
    rgOn = true; rgWave = 1; rgHpOk = 0; rgNewBest = false;
    rgSave();
    rgStartWave(true);
    return;
  }
  if (rgPhase == RG_OVER) {
    if (inRect(x, y, 133, RG_BTN_Y, 200, 52)) { rgPhase = RG_HUB; rgNewBest = false; sfxPlay(SFX_TAP); }
    return;
  }
  if (rgOn) {
    if (inRect(x, y, 83, RG_BTN_Y, 150, 52)) { sfxPlay(SFX_TAP); rgStartWave(false); }
    else if (inRect(x, y, 243, RG_BTN_Y, 140, 52)) {  // rendirse: cuenta lo superado
      sfxPlay(SFX_TAP);
      rogueAfterBattle(false);
    }
    return;
  }
  if (inRect(x, y, 133, RG_BTN_Y, 200, 52)) {  // primero se elige el inicial
    sfxPlay(SFX_TAP);
    rgPhase = RG_PICK;
  }
}

// ko11.23.1: se salio del combate con [◀] (endBattleScreen ya deshizo el equipo): como antes de empezarlo
void storyBattleQuit(uint8_t kind) {
  sfxPlay(SFX_TAP);
  if (kind == BK_ROGUE) {  // la vida de la oleada anterior sigue guardada (rogueSaveParty no se llamo)
    rgPhase = RG_HUB;
    rgResult[0] = 0;
    xScreen = XS_ROGUE;
    return;
  }
  xScreen = XS_SCENE;
  stPicking = false;
  snprintf(stText, sizeof(stText), "%s", STX[SX_QUIT_DONE]);
  stWho = W_NONE; stPage = 0; stTypeT = millis();
  stBattleWait = true;
}

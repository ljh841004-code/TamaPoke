// train.ino - fork KO (ko4): menu de entrenamiento y los juegos de DEF y VEL
// (se concatena tras TamaPoke.ino)
//
//   - Ataque:  saco de golpes (ya existia): toques rapidos en 10 s
//   - Defensa: caen pokeballs del cielo; tocarlas antes de que lleguen al suelo.
//              ko10.6: sin tiempo, 3 fallos y se acaba (cada vez mas rapido):
//              antes 20 s daban como mucho 28 y el record no se podia batir
//   - Velocidad: aparece una pokeball un instante; tocarla a tiempo. 15 rondas,
//                cada vez mas rapido. ko10.6: puntos por reflejos (hasta 100 por
//                ronda segun lo rapido que toques): antes el tope era 15/15
//   - Pelota: el juego de siempre (ahora solo sube el animo)
//
// Se engancha a TamaPoke.ino por funciones: trainingRender / trainingTap /
// trainingFast / trainingPress / trainingSwipe (como ui_extra.ino).

bool trainMenuOpen = false;
uint8_t trainMenuPage = 0;   // ko9.1: 0 entrenamiento, 1 batallas
uint32_t trainMsgUntil = 0;  // aviso en el menu ("demasiado cansado")
const char *trainMsg = nullptr;

#define TRM_X 73
#define TRM_W 320
#define TRM_Y 90
#define TRM_H 54
#define TRM_GAP 8

// ---------- defensa: pokeballs que caen ----------
#define DEF_LIVES 3     // ko10.6: fallos permitidos (antes 20 s fijos)
#define DEF_BALLS 4
#define DEF_GROUND 360
bool defOpen = false;
uint32_t defStart = 0, defOverUntil = 0, defLastStep = 0, defNextSpawn = 0;
uint16_t defScore = 0, defMissN = 0;
uint8_t defGain = 0;
bool defNewHi = false;
struct { float x, y; bool on; } defBall[DEF_BALLS];
struct { int16_t x, y; uint32_t t; bool good; } defFx[DEF_BALLS];

// ---------- velocidad: reflejos izquierda/derecha ----------
#define SPD_ROUNDS 15
#define SPD_POS 8       // ko8: posiciones posibles de la pokeball
enum : uint8_t { SP_WAIT = 0, SP_SHOW, SP_FEED };
bool spdOpen = false;
uint8_t spdRound = 0, spdPhase = SP_WAIT, spdSide = 0, spdGain = 0;
uint16_t spdScore = 0;
uint32_t spdUntil = 0, spdOverUntil = 0, spdShowAt = 0;
uint8_t spdHits = 0;       // ko10.6: aciertos (entrenan la VEL); spdScore = puntos
uint32_t spdRtSum = 0;     // suma de reflejos (ms) de los aciertos
bool spdGood = false, spdNewHi = false;

bool trainingFast() { return defOpen || spdOpen; }  // toques al apoyar el dedo
bool trainingOpen() { return trainMenuOpen || trainingFast(); }

void openTrainMenu() {
  if (pet.isEgg() || pet.ceremony) return;
  cardOpen = false;
  trainMenuOpen = true;
  trainMenuPage = 0;  // ko9.1: siempre empieza en entrenamiento
  trainMsgUntil = 0;
}

// ---------- menu ----------
// ko9.1: dos paginas (deslizar a los lados): 0 entrenamiento, 1 batallas
// (yasaeng y tongsin, los mismos que en la ficha > Batalla, que siguen alli)
#define TRB_Y1 104   // botones de la pagina de batallas (ko10.4: rejilla 2x2)
#define TRB_Y2 180
#define TRB_H 64
#define TRB_W 154
#define TRB_X2 (TRM_X + TRB_W + 12)

static void drawTrainMenuDots() {
  for (int i = 0; i < 2; i++) {
    int x = CX - 13 + i * 26;
    if (i == trainMenuPage) gfx->fillCircle(x, 370, 5, UI_INK);
    else gfx->drawCircle(x, 370, 4, UI_INK);
  }
}

static void renderTrainPage() {
  drawFit(XT(X_TRAIN_TITLE), 44, 300, UI_INK, 3);
  static const XId LABEL[4] = { X_TR_ATK, X_TR_DEF, X_TR_SPE, X_TR_PLAY };
  const uint16_t COL[4] = { UI_BAR_BAD, 0x4C98, UI_BAR_WARN, UI_BAR_OK };
  uint16_t best[4] = { pet.strHi, pet.defHi, pet.speHi, pet.gameHi };
  for (int i = 0; i < 4; i++) {
    int y = TRM_Y + i * (TRM_H + TRM_GAP);
    gfx->fillRoundRect(TRM_X, y, TRM_W, TRM_H, 12, UI_WHITE);
    gfx->drawRoundRect(TRM_X, y, TRM_W, TRM_H, 12, UI_INK);
    gfx->fillRoundRect(TRM_X + 8, y + 10, 8, TRM_H - 20, 4, COL[i]);  // color del tipo de juego
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(TRM_X + 26, y + 8);
    printT(XT(LABEL[i]));
    char b[24];
    snprintf(b, sizeof(b), XT(X_BEST_FMT), best[i]);
    setSize(1);
    setCur(TRM_X + 26, y + 34);
    printT(b);
  }
  if (!(timeLeft(trainMsgUntil) && trainMsg))
    drawFit(XT(X_TRAIN_QUIT_HINT), 340, 260, C565(0x60, 0x68, 0x70), 1);
}

static void renderBattlePage() {
  drawFit(T(S_BATTLE), 44, 300, UI_INK, 3);
  drawBtn(TRM_X, TRB_Y1, TRB_W, TRB_H, UI_BAR_OK, UI_WHITE, XT(X_WILD_BTN));
  drawBtn(TRB_X2, TRB_Y1, TRB_W, TRB_H, 0x4C98, UI_WHITE, XT(X_LINK_BTN));
  // ko10.4: gimnasios (con las medallas) y reto del dia
  char gb[24];
  snprintf(gb, sizeof(gb), "%s %u/8", XT(X_GYM_BTN), badgeCount(pet.badges));
  drawBtn(TRM_X, TRB_Y2, TRB_W, TRB_H, C565(0xc0, 0x5a, 0x2a), UI_WHITE, gb);
  bool done = pet.lastSeenEpoch && pet.dailyDoneDay == pet.lastSeenEpoch / 86400u;
  drawBtn(TRB_X2, TRB_Y2, TRB_W, TRB_H, done ? UI_TRACK : C565(0x9a, 0x4c, 0xc0), done ? UI_INK : UI_WHITE,
          XT(X_DAILY_BTN));
  char rec[48];
  snprintf(rec, sizeof(rec), XT(X_RECORD_FMT), pet.wildWins, pet.linkWins, pet.linkBattles, pet.trades);
  drawFit(rec, 268, 320, UI_INK, 2);
}

void renderTrainMenu() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (trainMenuPage == 0) renderTrainPage();
  else renderBattlePage();
  if (timeLeft(trainMsgUntil) && trainMsg) drawFit(trainMsg, 336, 320, UI_BAR_BAD, 2);
  drawTrainMenuDots();
  drawFit(T(S_BACK), 390, 220, UI_INK, 2);
  gfx->flush();
}

// deslizar a los lados cambia de pagina; mas alla de los extremos, cierra
bool trainMenuSwipe(int dir) {
  if (!trainMenuOpen) return false;
  int p = (int)trainMenuPage + (dir > 0 ? -1 : 1);  // izquierda avanza (como la ficha)
  if (p < 0 || p > 1) trainMenuOpen = false;
  else { trainMenuPage = (uint8_t)p; trainMsgUntil = 0; sfxPlay(SFX_TAP); }
  return true;
}

static void battlePageTap(int16_t x, int16_t y) {
  if (x < TRM_X || x >= TRM_X + TRM_W) { trainMenuOpen = false; return; }
  int row = (y >= TRB_Y1 && y < TRB_Y1 + TRB_H) ? 0 : (y >= TRB_Y2 && y < TRB_Y2 + TRB_H) ? 1 : -1;
  if (row < 0) {
    if (y < TRB_Y1 || y > 380) trainMenuOpen = false;
    return;
  }
  bool right = x >= TRB_X2;
  if (!right && x >= TRM_X + TRB_W) return;  // hueco entre columnas
  if (row == 0 && right) {                   // tongsin
    trainMenuOpen = false;
    openLinkMenu();
    sfxPlay(SFX_TAP);
    return;
  }
  // salvaje, gimnasio y reto: mismos motivos que battleAllowed(), avisados aqui
  if (!pet.canBattle()) { trainMsg = XT(X_CANT_NOW); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return; }
  if (row == 0 && pet.tooTiredToBattle()) { trainMsg = XT(X_TOO_TIRED); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return; }
  trainMenuOpen = false;
  if (row == 0) openRegionPick();  // ko10.1: primero se elige a donde ir
  else if (!right) openGyms();     // ko10.4
  else openDaily();
}

void trainMenuTap(int16_t x, int16_t y) {
  if (trainMenuPage == 1) { battlePageTap(x, y); return; }
  if (x < TRM_X || x >= TRM_X + TRM_W || y < TRM_Y) { trainMenuOpen = false; return; }
  int i = (y - TRM_Y) / (TRM_H + TRM_GAP);
  if (i > 3) { trainMenuOpen = false; return; }
  if ((y - TRM_Y) % (TRM_H + TRM_GAP) >= TRM_H) return;  // entre dos filas
  if (pet.sleeping || pet.isEgg() || pet.ceremony) {
    trainMsg = XT(X_CANT_NOW); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return;
  }
  if (i < 3 && pet.energy < 10) {  // la pelota es juego: se puede aunque este cansado
    trainMsg = XT(X_TOO_TIRED); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return;
  }
  sfxPlay(SFX_TAP);
  trainMenuOpen = false;
  if (i == 0) startSack();
  else if (i == 1) startDefense();
  else if (i == 2) startSpeed();
  else startGame();
}

// ---------- pantalla de resultado comun ----------

void drawTrainResult(const char *score, const char *gain, uint16_t gainCol, bool newHi, uint16_t hi,
                     const char *sub) {
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  drawFit(score, 150, 360, ink, 4);
  drawFit(gain, 214, 320, gainCol, 3);
  if (newHi) {
    drawFit(T(S_NEW_RECORD), 262, 320, UI_BAR_WARN, 2);
  } else {
    char r[24];
    snprintf(r, sizeof(r), T(S_RECORD_FMT), hi);
    drawFit(r, 262, 320, ink, 2);
  }
  if (sub) drawFit(sub, 300, 340, ink, 2);  // ko10.6: aciertos y reflejo medio
  // ko10.7: premio de la sesion (EXP siempre; record = mas EXP + caramelo)
  char bn[40];
  bn[0] = 0;
  if (pet.lastTrainExp && pet.lastTrainCandy)
    snprintf(bn, sizeof(bn), XT(X_TRAIN_EXP_CANDY_FMT), (unsigned long)pet.lastTrainExp);
  else if (pet.lastTrainExp) snprintf(bn, sizeof(bn), XT(X_TRAIN_EXP_FMT), (unsigned long)pet.lastTrainExp);
  else if (pet.lastTrainCandy) snprintf(bn, sizeof(bn), "%s", XT(X_TRAIN_CANDY));
  if (bn[0]) drawFit(bn, 334, 340, UI_BAR_OK, 2);
  gfx->flush();
}

// el bicho en el suelo, mirando al juego
void drawTrainPet(int x, uint8_t act) {
  if (pmd.loaded) {
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, x, 394, millis(), true, false, 3);
  }
}

void drawTimeBar(uint32_t left, uint32_t total, int y) {
  int bw = 280, fw = (int)((uint64_t)bw * left / total);
  gfx->fillRoundRect(CX - bw / 2, y, bw, 14, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, y, fw, 14, 5, UI_BAR_OK);
}

// ---------- defensa ----------

void startDefense() {
  defOpen = true;
  defStart = defLastStep = millis();
  defNextSpawn = defStart + 600;
  defOverUntil = 0;
  defScore = defMissN = 0;
  defNewHi = false;
  for (auto &b : defBall) b.on = false;
  for (auto &f : defFx) f.t = 0;
}

static void defFxAdd(int x, int y, bool good) {
  int k = 0;
  for (int i = 1; i < DEF_BALLS; i++) if (defFx[i].t < defFx[k].t) k = i;  // el mas viejo
  defFx[k].x = x; defFx[k].y = y; defFx[k].t = millis(); defFx[k].good = good;
}

void stepDefense() {
  uint32_t now = millis();
  float el = (now - defStart) / 1000.0f;       // segundos transcurridos
  float dt = (now - defLastStep) / 1000.0f;
  if (dt > 0.3f) dt = 0.3f;
  defLastStep = now;
  float vy = 110 + el * 7;                     // px/s: cada vez caen mas deprisa
  for (auto &b : defBall) {
    if (!b.on) continue;
    b.y += vy * dt;
    if (b.y > DEF_GROUND) { b.on = false; defMissN++; defFxAdd((int)b.x, DEF_GROUND, false); }
  }
  if ((int32_t)(now - defNextSpawn) >= 0) {
    for (auto &b : defBall)
      if (!b.on) { b.on = true; b.x = 120 + random(227); b.y = 64; break; }
    int gap = 1000 - (int)(el * 26);
    defNextSpawn = now + (gap < 300 ? 300 : gap);  // ko10.6: sigue apretando
  }
}

void defensePress(int16_t x, int16_t y) {
  if (defOverUntil) return;
  int best = -1;
  float bd = 52 * 52;  // radio tactil generoso
  for (int i = 0; i < DEF_BALLS; i++) {
    if (!defBall[i].on) continue;
    float dx = defBall[i].x - x, dy = defBall[i].y - y, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  if (best < 0) return;
  defBall[best].on = false;
  defScore++;
  defFxAdd((int)defBall[best].x, (int)defBall[best].y, true);
  sfxPlay(SFX_PLAY);
}

void renderDefense() {
  uint32_t now = millis();
  if (defOverUntil) {
    if (!timeLeft(defOverUntil)) { defOpen = false; return; }
    char s[24], g[20];
    snprintf(s, sizeof(s), XT(X_BLOCKED_FMT), defScore);
    snprintf(g, sizeof(g), XT(X_DEF_GAIN_FMT), defGain);
    drawTrainResult(s, g, 0x4C98, defNewHi && defScore > 0, pet.defHi, nullptr);
    return;
  }
  if (defMissN >= DEF_LIVES) {  // ko10.6: 3 fallos y se acabo: aplicar entrenamiento
    defNewHi = defScore > pet.defHi;
    defGain = pet.trainDefense(defScore);
    sfxPlay(defNewHi ? SFX_MEDAL : SFX_PLAY);
    defOverUntil = now + 3500;
    return;
  }
  stepDefense();
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  char b[8];
  snprintf(b, sizeof(b), "%u", defScore);
  drawFit(b, 22, 200, ink, 4);
  for (int i = 0; i < DEF_LIVES; i++) {  // ko10.6: vidas (como el juego de pelota)
    if (i < DEF_LIVES - (int)defMissN) gfx->fillCircle(CX - 28 + i * 28, 70, 7, UI_BAR_BAD);
    else gfx->drawCircle(CX - 28 + i * 28, 70, 7, UI_TRACK);
  }
  drawTrainPet(CX, PMD_IDLE);
  for (auto &f : defFx) {  // anillo verde al atrapar, rojo al caer
    uint32_t t = now - f.t;
    if (!f.t || t > 300) continue;
    int r = 20 + (int)(t / 8);
    gfx->drawCircle(f.x, f.y, r, f.good ? UI_BAR_OK : UI_BAR_BAD);
    gfx->drawCircle(f.x, f.y, r - 2, f.good ? UI_BAR_OK : UI_BAR_BAD);
  }
  for (auto &ball : defBall)
    if (ball.on) drawMap(SPR_ICON_PLAY, 16, (int)ball.x - 24, (int)ball.y - 24, 3, false);
  if (now - defStart < 2500) drawFit(XT(X_TR_DEF_HINT), 180, 320, ink, 2);
  gfx->flush();
}

// ---------- velocidad ----------

static uint32_t spdWindow() {  // cuanto dura visible la pokeball en esta ronda
  int w = 1100 - spdRound * 40;
  return w < 520 ? 520 : w;
}

static void spdNextRound() {
  spdPhase = SP_WAIT;
  spdUntil = millis() + 500 + random(800);
  // ko8: 8 posiciones como las horas de un reloj (12, 1:30, 3...), al azar y
  // sin repetir la anterior: antes solo izquierda/derecha y se adivinaba
  uint8_t prev = spdSide;
  do spdSide = random(SPD_POS); while (spdSide == prev);
}

void startSpeed() {
  spdOpen = true;
  spdRound = 0;
  spdScore = 0;
  spdHits = 0;
  spdRtSum = 0;
  spdOverUntil = 0;
  spdNewHi = false;
  spdNextRound();
  spdUntil += 700;  // un respiro para leer la ayuda
}

// ko8: anillo de 8 posiciones alrededor del bicho (0 = arriba, sentido horario)
#define SPD_RING_X 233
#define SPD_RING_Y 240
#define SPD_RING_R 160
#define SPD_PET_G 300   // el bicho, en el centro del anillo
static const int8_t SPD_DIR[SPD_POS][2] = { { 0, -10 }, { 7, -7 }, { 10, 0 }, { 7, 7 },
                                            { 0, 10 }, { -7, 7 }, { -10, 0 }, { -7, -7 } };
static int spdBallX() { return SPD_RING_X + SPD_DIR[spdSide % SPD_POS][0] * SPD_RING_R / 10; }
static int spdBallY() { return SPD_RING_Y + SPD_DIR[spdSide % SPD_POS][1] * SPD_RING_R / 10; }

// ko10.6: puntos por reflejos: 100 - ms/10 (0,25 s = 75), minimo 10
uint16_t spdPoints(uint32_t rt) {
  return rt >= 900 ? 10 : (uint16_t)(100 - rt / 10);
}

static void spdResolve(bool good) {
  spdGood = good;
  if (good) {
    uint32_t rt = millis() - spdShowAt;
    spdHits++;
    spdRtSum += rt;
    spdScore += spdPoints(rt);
    sfxPlay(SFX_PLAY);
  }
  else sfxPlay(SFX_DENY);
  spdPhase = SP_FEED;
  spdUntil = millis() + 450;
}

void speedPress(int16_t x, int16_t y) {
  if (spdOverUntil || spdPhase == SP_FEED) return;
  if (spdPhase == SP_WAIT) { spdResolve(false); return; }  // se adelanto
  int dx = x - spdBallX(), dy = y - spdBallY();
  spdResolve(dx * dx + dy * dy <= 80 * 80);
}

void stepSpeed() {
  if (timeLeft(spdUntil)) return;
  if (spdPhase == SP_WAIT) {
    spdPhase = SP_SHOW;
    spdShowAt = millis();
    spdUntil = spdShowAt + spdWindow();
  } else if (spdPhase == SP_SHOW) {
    spdResolve(false);  // no llego a tiempo
  } else if (++spdRound >= SPD_ROUNDS) {
    spdNewHi = spdScore > pet.speHi;
    spdGain = pet.trainSpeed(spdHits, spdScore);
    sfxPlay(spdNewHi ? SFX_MEDAL : SFX_PLAY);
    spdOverUntil = millis() + 3500;
  } else {
    spdNextRound();
  }
}

void renderSpeed() {
  if (spdOverUntil) {
    if (!timeLeft(spdOverUntil)) { spdOpen = false; return; }
    char s[24], g[20];
    snprintf(s, sizeof(s), XT(X_SPE_PTS_FMT), spdScore);
    snprintf(g, sizeof(g), XT(X_SPE_GAIN_FMT), spdGain);
    char sub[48];
    uint32_t avg = spdHits ? spdRtSum / spdHits : 0;
    snprintf(sub, sizeof(sub), XT(X_SPE_AVG_FMT), (unsigned)spdHits, (unsigned)SPD_ROUNDS,
             (unsigned)(avg / 1000), (unsigned)(avg % 1000 / 10));
    drawTrainResult(s, g, UI_BAR_WARN, spdNewHi && spdScore > 0, pet.speHi, sub);
    return;
  }
  stepSpeed();
  if (spdOverUntil) return;
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  char b[16];
  // ko8: marcador en el centro del anillo (arriba sale una pokeball)
  snprintf(b, sizeof(b), "%u/%u", spdRound + 1, SPD_ROUNDS);
  drawFit(b, 128, 200, ink, 3);
  snprintf(b, sizeof(b), "%u", spdScore);
  drawFit(b, 160, 200, UI_BAR_OK, 2);
  // el bicho mira hacia donde salio la pokeball
  int bx = spdBallX(), by = spdBallY();
  uint8_t act = PMD_IDLE;
  if (spdPhase == SP_SHOW && bx != SPD_RING_X) act = bx > SPD_RING_X ? PMD_WALKR : PMD_WALKL;
  // plataforma flotante (como en las batallas) para el bicho del centro
  uint8_t bio = DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < 6 ? bio : 0];
  gfx->fillEllipse(CX, SPD_PET_G + 4, 70, 14, lerp565(soil, C565(0x10, 0x18, 0x20), 4, 16));
  gfx->fillEllipse(CX, SPD_PET_G, 70, 12, soil);
  if (pmd.loaded) {
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, CX, SPD_PET_G, millis(), true, false, 3);
  }
  // huecos del anillo: se ve donde PUEDE salir
  for (int i = 0; i < SPD_POS; i++)
    gfx->drawCircle(SPD_RING_X + SPD_DIR[i][0] * SPD_RING_R / 10, SPD_RING_Y + SPD_DIR[i][1] * SPD_RING_R / 10,
                    10, lerp565(ink, UI_BG_DAY, 10, 16));
  if (spdPhase == SP_SHOW) {
    gfx->fillCircle(bx, by, 44, lerp565(UI_WHITE, UI_BAR_WARN, 5, 16));  // halo
    drawMap(SPR_ICON_PLAY, 16, bx - 32, by - 32, 4, false);
    // la ventana que queda, como una barra que se vacia
    uint32_t left = timeLeft(spdUntil), win = spdWindow();
    int w = (int)(80 * left / win);
    int ty = spdSide == 4 ? by - 56 : by + 48;  // abajo del todo: la barra va encima
    gfx->fillRoundRect(bx - 40, ty, 80, 6, 3, UI_TRACK);
    if (w > 1) gfx->fillRoundRect(bx - 40, ty, w, 6, 3, UI_BAR_WARN);
  } else if (spdPhase == SP_FEED) {
    // el resultado sale donde estaba la pokeball
    const char *fb = XT(spdGood ? X_NICE : X_MISS);
    gfx->setTextColor(spdGood ? UI_BAR_OK : UI_BAR_BAD);
    setSize(2);
    int fx = bx - textW(fb, 2) / 2;
    if (fx < 40) fx = 40;
    if (fx + textW(fb, 2) > 426) fx = 426 - textW(fb, 2);
    setCur(fx, by - 8);
    printT(fb);
  } else if (spdRound == 0) {
    drawFit(XT(X_TR_SPE_HINT), 186, 220, ink, 2);
  }
  gfx->flush();
}

// ---------- ganchos para TamaPoke.ino ----------

bool trainingRender() {
  if (trainMenuOpen) { renderTrainMenu(); return true; }
  if (defOpen) { renderDefense(); return true; }
  if (spdOpen) { renderSpeed(); return true; }
  return false;
}

bool trainingTap(int16_t x, int16_t y) {
  if (!trainMenuOpen) return false;
  trainMenuTap(x, y);
  return true;
}

void trainingPress(int16_t x, int16_t y) {  // al apoyar el dedo (juegos rapidos)
  if (defOpen) defensePress(x, y);
  else if (spdOpen) speedPress(x, y);
}

// ko9.1: abandonar sin premio (mantener el dedo 2 s, ver handleTouch)
void trainingQuit() { defOpen = spdOpen = false; }

bool trainingSwipe() {
  if (trainMenuOpen) { trainMenuOpen = false; return true; }
  return trainingFast();
}

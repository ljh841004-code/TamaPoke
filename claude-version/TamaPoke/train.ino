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
// ko12.9.5: el menu queda asi: ataque = dianas numeradas (el juego "en orden" de velocidad: tocar
// 1, 2, 3... y el bicho dispara a cada una), velocidad = buscar la pokeball (train_more.ino),
// juego = imitar ("따라 해 봐!", train_more.ino). El saco y la timing punch ya no salen.
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
#define TRM_H 46  // ko11.9: 5 filas (antes 54/8 con 4)
#define TRM_GAP 6
#define TRM_N 5

// ---------- defensa: pokeballs que caen ----------
#define DEF_LIVES 3     // ko10.6: fallos permitidos (antes 20 s fijos)
// ko11.14: un solo balon que baja por un carril; hay que tocar cuando pasa por
// la franja: centro verde = PERFECTO (2), amarillo = BIEN (1), antes o despues = fallo
#define DEF_Y0 104        // arriba del carril (donde espera el balon)
#define DEF_ZONE_Y 262    // centro de la franja
#define DEF_PERFECT 10    // +-px del centro: perfecto
#define DEF_GOOD 28       // +-px: bien
#define DEF_RAIL_W 56
bool defOpen = false;
uint32_t defStart = 0, defOverUntil = 0, defLastStep = 0, defNextSpawn = 0;
uint16_t defScore = 0, defMissN = 0;
uint8_t defGain = 0;
bool defNewHi = false;
uint32_t defDropT = 0;        // 0 = el balon espera arriba (hasta defNextSpawn)
float defV = 170;             // px/s de la caida actual
uint16_t defHits = 0, defPerfectN = 0, defGoodN = 0, defCombo = 0;
uint8_t defJudge = 0;         // 1 perfecto, 2 bien, 3 pronto, 4 tarde
uint32_t defJudgeT = 0;
int16_t defJudgeY = 0;

// ---------- velocidad: reflejos izquierda/derecha ----------
#define SPD_ROUNDS 15
#define SPD_POS 8       // ko8: posiciones posibles de la pokeball
enum : uint8_t { SP_WAIT = 0, SP_SHOW, SP_FEED };
bool spdOpen = false;
uint8_t spdRound = 0, spdPhase = SP_WAIT, spdSide = 0, spdGain = 0;
uint16_t spdScore = 0;
uint32_t spdUntil = 0, spdOverUntil = 0, spdShowAt = 0;
uint8_t spdHits = 0;       // ko10.6: aciertos (entrenan la VEL); spdScore = puntos
uint16_t spdTargets = 0;   // ko12.9.5: dianas tocadas en orden (ataque: el record y lo que entrena)
uint32_t spdRtSum = 0;     // suma de reflejos (ms) de los aciertos (ko11.14: por balon)
#define SPD_MAXN 5      // ko11.14: balones por ronda (3 -> 5)
int16_t spdBx[SPD_MAXN], spdBy[SPD_MAXN];
uint8_t spdN = 3, spdNext = 0, spdFail = 0;  // spdFail: 1 orden equivocado, 2 tiempo
uint16_t spdBase = 0;  // ko11.17: los numeros siguen de ronda en ronda (1-3, 4-7, 8-12...)
#define SPD_RANDOM_FROM 7   // desde esta ronda (0 = la 1a) el primer numero es al azar
#define SPD_RANDOM_MAX 60   // ... entre 1 y 60
uint32_t spdTapT[SPD_MAXN];  // ko11.16: cuando se toco cada balon (estallido)
uint8_t spdRes[SPD_ROUNDS];  // ko11.16: 0 pendiente, 1 bien, 2 fallo (puntos de progreso)
bool spdGood = false, spdNewHi = false;

extern bool vbOpen;  // ko11.9 (volley.ino)
extern bool shellOpen, simonOpen;  // ko12.9.5 (train_more.ino)
bool trainingFast() { return defOpen || spdOpen || vbOpen || shellOpen || simonOpen; }  // toques al apoyar el dedo
bool trainingOpen() { return trainMenuOpen || trainingFast(); }

// ko11.17: el menu se abre desde la principal o desde la ficha: [<] vuelve ahi
static bool trainFromCard = false;
static uint8_t trainFromCardPage = 0;
void openTrainMenu() {
  if (pet.isEgg() || pet.ceremony) return;
  trainFromCard = cardOpen;
  trainFromCardPage = cardPage;
  cardOpen = false;
  trainMenuOpen = true;
  trainMenuPage = 0;  // ko9.1: siempre empieza en entrenamiento
  trainMsgUntil = 0;
}

// ko11.9.3: al acabar (o abandonar) un entrenamiento se vuelve al menu de
// entrenamiento, no a la pantalla principal. El dedo que cerro el resultado no
// debe pulsar nada del menu al levantarse.
// ko11.11: tras volver, 2 s en los que tocar fuera de las filas NO cierra el
// menu (en la pelota se sigue tocando sin mirar y se salia a la pantalla principal)
static uint32_t trainBackUntil = 0;
static bool trainOutsideClose() { return !timeLeft(trainBackUntil); }

void backToTrainMenu() {
  bool fc = trainFromCard;  // ko11.17: sigue sabiendo de donde vino
  openTrainMenu();
  trainFromCard = fc;
  swallowGesture = true;
  navGuardUntil = millis() + 600;
  trainBackUntil = millis() + 2000;
}

// ko11.17: volver a este menu desde un submenu (region, gimnasios, tongsin...)
void reopenTrainMenu(uint8_t page) {
  bool fc = trainFromCard;
  openTrainMenu();
  trainFromCard = fc;
  trainMenuPage = page > 1 ? 1 : page;
}
// [<] / cerrar: a la ficha si se abrio desde ella, si no a la principal
static void trainMenuClose() {
  trainMenuOpen = false;
  if (trainFromCard && !pet.isEgg()) { cardOpen = true; cardPage = trainFromCardPage; }
  trainFromCard = false;
}

// ---------- menu ----------
// ko9.1: dos paginas (deslizar a los lados): 0 entrenamiento, 1 batallas
// (yasaeng y tongsin, los mismos que en la ficha > Batalla, que siguen alli)
#define TRB_Y1 104   // botones de la pagina de batallas (ko10.4: rejilla 2x2)
#define TRB_Y2 180
#define TRB_H 64
#define TRB_W 154
#define TRB_X2 (TRM_X + TRB_W + 12)
#define TRB_Y3 256   // ko11.21: [스토리] a lo ancho
#define TRB_H3 52

static void drawTrainMenuDots() {
  for (int i = 0; i < 2; i++) {
    int x = CX - 13 + i * 26;
    if (i == trainMenuPage) gfx->fillCircle(x, 382, 5, UI_INK);
    else gfx->drawCircle(x, 382, 4, UI_INK);
  }
}

static void renderTrainPage() {
  drawFit(XT(X_TRAIN_TITLE), 44, 300, UI_INK, 3);
  static const XId LABEL[TRM_N] = { X_TR_ATK, X_TR_DEF, X_TR_SPE, X_TR_PLAY, X_TR_VOLLEY };
  const uint16_t COL[TRM_N] = { UI_BAR_BAD, 0x4C98, UI_BAR_WARN, UI_BAR_OK, C565(0xf0, 0xc0, 0x20) };
  uint16_t best[TRM_N] = { pet.strHi, pet.defHi, pet.speHi, pet.gameHi, pet.vbBest };
  // ko11.9.2: el record es de este bicho; al lado, el de siempre
  uint16_t all[TRM_N] = { pet.allStrHi, pet.allDefHi, pet.allSpeHi, pet.allGameHi, pet.allVbBest };
  for (int i = 0; i < TRM_N; i++) {
    int y = TRM_Y + i * (TRM_H + TRM_GAP);
    uiButton(TRM_X, y, TRM_W, TRM_H, 12, UI_WHITE, UI_INK);
    gfx->fillRoundRect(TRM_X + 8, y + 8, 8, TRM_H - 16, 4, COL[i]);  // color del tipo de juego
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(TRM_X + 26, y + 4);
    printT(XT(LABEL[i]));
    char b[48];
    uint16_t a = all[i] > best[i] ? all[i] : best[i];
    if (i == 4)  // ko11.9.4: voleibol: racha actual + record de este bicho + historico
      snprintf(b, sizeof(b), XT(X_VB_NOW_FMT), (unsigned)pet.vbStreak, (unsigned)best[i], (unsigned)a);
    else
      snprintf(b, sizeof(b), XT(X_BEST_ALL_FMT), best[i], a);
    setSize(1);
    setCur(TRM_X + 26, y + 28);
    printT(b);
  }
  if (!(timeLeft(trainMsgUntil) && trainMsg))
    drawFit(XT(X_TRAIN_QUIT_HINT), 352, 260, C565(0x60, 0x68, 0x70), 1);
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
  drawBtn(TRM_X, TRB_Y3, TRM_W, TRB_H3, C565(0xd8, 0x30, 0x30), UI_WHITE, STX[SX_TITLE]);  // ko11.21
  char rec[48];
  snprintf(rec, sizeof(rec), XT(X_RECORD_FMT), pet.wildWins, pet.linkWins, pet.linkBattles, pet.trades);
  drawFit(rec, 318, 320, UI_INK, 1);
}

void renderTrainMenu() {
  uiScreenBg();  // ko11.6.1: sin pasar por negro (parpadeo)
  if (trainMenuPage == 0) renderTrainPage();
  else renderBattlePage();
  if (timeLeft(trainMsgUntil) && trainMsg) drawFit(trainMsg, 350, 320, UI_BAR_BAD, 2);
  drawTrainMenuDots();
  drawFit(T(S_BACK), 396, 220, UI_INK, 2);
  drawNav(NAV_L, UI_INK);  // ko11.17: en la 1a pagina = volver (principal o ficha)
  if (trainMenuPage == 0) drawNav(NAV_R, UI_INK);
  drawNav(NAV_DOWN, UI_INK);
  uiFlush();
}

// deslizar a los lados cambia de pagina; mas alla de los extremos, cierra
bool trainMenuSwipe(int dir) {
  if (!trainMenuOpen) return false;
  int p = (int)trainMenuPage + (dir > 0 ? -1 : 1);  // izquierda avanza (como la ficha)
  if (p < 0 || p > 1) { if (trainOutsideClose()) trainMenuClose(); }
  else { trainMenuPage = (uint8_t)p; trainMsgUntil = 0; sfxPlay(SFX_TAP); }
  return true;
}

static void battlePageTap(int16_t x, int16_t y) {
  if (x < TRM_X || x >= TRM_X + TRM_W) { if (trainOutsideClose()) trainMenuClose(); return; }
  if (y >= TRB_Y3 && y < TRB_Y3 + TRB_H3) { openStory(); return; }  // ko11.21: historia
  int row = (y >= TRB_Y1 && y < TRB_Y1 + TRB_H) ? 0 : (y >= TRB_Y2 && y < TRB_Y2 + TRB_H) ? 1 : -1;
  if (row < 0) {
    if ((y < TRB_Y1 || y > 380) && trainOutsideClose()) trainMenuClose();
    return;
  }
  bool right = x >= TRB_X2;
  if (!right && x >= TRM_X + TRB_W) return;  // hueco entre columnas
  if (row == 0 && right) {                   // tongsin
    openLinkMenu();  // ko11.17: cierra el menu (y [<] vuelve a esta pagina)
    sfxPlay(SFX_TAP);
    return;
  }
  // salvaje, gimnasio y reto: mismos motivos que battleAllowed(), avisados aqui
  if (!pet.canBattle()) { trainMsg = XT(X_CANT_NOW); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return; }
  if (row == 0 && pet.tooTiredToBattle()) { trainMsg = XT(X_TOO_TIRED); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return; }
  if (row == 0) openRegionPick();  // ko10.1: primero se elige a donde ir (ko11.17: cierra el menu)
  else if (!right) openGyms();     // ko10.4
  else openDaily();
}

void trainMenuTap(int16_t x, int16_t y) {
  // ko10.8: flechas
  if (navHit(NAV_L, x, y) && trainMenuPage == 1) { trainMenuPage = 0; trainMsgUntil = 0; sfxPlay(SFX_TAP); return; }
  if (navHit(NAV_L, x, y)) { trainMenuClose(); sfxPlay(SFX_TAP); return; }  // ko11.17: [<] volver
  if (navHit(NAV_R, x, y) && trainMenuPage == 0) { trainMenuPage = 1; trainMsgUntil = 0; sfxPlay(SFX_TAP); return; }
  if (navHit(NAV_DOWN, x, y)) { if (trainOutsideClose()) { trainMenuClose(); sfxPlay(SFX_TAP); } return; }
  if (trainMenuPage == 1) { battlePageTap(x, y); return; }
  if (x < TRM_X || x >= TRM_X + TRM_W || y < TRM_Y) { if (trainOutsideClose()) trainMenuClose(); return; }
  int i = (y - TRM_Y) / (TRM_H + TRM_GAP);
  if (i >= TRM_N) { if (trainOutsideClose()) trainMenuClose(); return; }
  if ((y - TRM_Y) % (TRM_H + TRM_GAP) >= TRM_H) return;  // entre dos filas
  if (pet.sleeping || pet.isEgg() || pet.ceremony) {
    trainMsg = XT(X_CANT_NOW); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return;
  }
  if (i != 3 && pet.energy < 10) {  // la pelota es juego: se puede aunque este cansado
    trainMsg = XT(X_TOO_TIRED); trainMsgUntil = millis() + 2500; sfxPlay(SFX_DENY); return;
  }
  sfxPlay(SFX_TAP);
  trainMenuOpen = false;
  if (i == 0) startSpeed();  // ko12.9.5: dianas numeradas (antes timing punch / saco)
  else if (i == 1) startDefense();
  else if (i == 2) startShell();  // ko12.9.5: buscar la pokeball
  else if (i == 4) startVolley();  // ko11.9
  else startSimon();  // ko12.9.5: imitar (antes inclinar / pelota)
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
    drawFit(pet.lastAllTime ? XT(X_ALL_RECORD) : T(S_NEW_RECORD), 262, 320, UI_BAR_WARN, 2);
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
  drawPerfLine(362, ink);  // ko11.3
  uiFlush();
}

// ko11.3: medida del minijuego (para ver de donde vienen los tirones). ko12.8: ya no se pinta en el
// resultado (era texto de desarrollo y se salia por abajo): va a la consola serie, una vez por partida
extern uint16_t perfRenderMax, perfStallMax;
extern uint32_t perfFrames, perfRenderSum;
void drawPerfLine(int y, uint16_t ink) {
  (void)y; (void)ink;
  static uint32_t shownFrames = 0;
  if (!perfFrames || perfFrames == shownFrames) return;
  shownFrames = perfFrames;
  Serial.printf("PERF juego: media %u max %u ms, parada max %u ms\n", (unsigned)(perfRenderSum / perfFrames),
                (unsigned)perfRenderMax, (unsigned)perfStallMax);
}

void drawTimeBar(uint32_t left, uint32_t total, int y) {
  int bw = 280, fw = (int)((uint64_t)bw * left / total);
  uiGauge(CX - bw / 2, y, bw, 14, fw * 1000 / bw, UI_BAR_OK, UI_TRACK);  // ko11.12
}

// ---------- defensa ----------
// ko11.14: TIMING. Un balon baja por el carril hacia el Pokemon; se toca en
// cualquier sitio cuando pasa por la franja. Cada acierto lo hace un poco mas
// rapido (y cada caida varia +-12 % y espera distinto arriba: sin ritmo fijo)

static float defBallY(uint32_t now) {
  if (!defDropT) return DEF_Y0;
  return DEF_Y0 + defV * (float)(now - defDropT) / 1000.0f;
}

static void defNextBall(uint32_t now, uint32_t wait) {
  defDropT = 0;
  defNextSpawn = now + wait;
  float v = 170.0f + 16.0f * defHits;
  if (v > 640) v = 640;
  defV = v * (0.88f + random(25) / 100.0f);
}

void startDefense() {
  perfReset();  // ko11.3
  defOpen = true;
  defStart = defLastStep = millis();
  defOverUntil = 0;
  defScore = defMissN = 0;
  defHits = defPerfectN = defGoodN = defCombo = 0;
  defNewHi = false;
  defJudge = 0; defJudgeT = 0;
  defNextBall(defStart, 900);
}

static void defSetJudge(uint8_t j, int y) {
  defJudge = j; defJudgeT = millis(); defJudgeY = (int16_t)y;
}

void stepDefense() {
  uint32_t now = millis();
  defLastStep = now;
  if (!defDropT) {
    if ((int32_t)(now - defNextSpawn) >= 0) defDropT = now;
    return;
  }
  if (defBallY(now) > DEF_ZONE_Y + DEF_GOOD) {  // se paso: tarde
    defMissN++;
    defCombo = 0;
    defSetJudge(4, DEF_ZONE_Y + DEF_GOOD);
    sfxPlay(SFX_DENY);
    defNextBall(now, 700 + random(400));
  }
}

void defensePress(int16_t x, int16_t y) {
  (void)x; (void)y;  // ko11.14: vale tocar en cualquier sitio
  if (defOverUntil || !defDropT) return;
  uint32_t now = millis();
  float by = defBallY(now);
  float d = fabsf(by - DEF_ZONE_Y);
  if (d <= DEF_PERFECT) {
    defScore += 2; defPerfectN++; defHits++; defCombo++;
    defSetJudge(1, (int)by);
    sfxPlay(SFX_MEDAL);
  } else if (d <= DEF_GOOD) {
    defScore += 1; defGoodN++; defHits++; defCombo = 0;
    defSetJudge(2, (int)by);
    sfxPlay(SFX_PLAY);
  } else {  // antes de la franja: pronto
    defMissN++; defCombo = 0;
    defSetJudge(3, (int)by);
    sfxPlay(SFX_DENY);
  }
  defNextBall(now, 450 + random(500));
}

void renderDefense() {
  uint32_t now = millis();
  if (defOverUntil) {
    if (!timeLeft(defOverUntil)) { defOpen = false; backToTrainMenu(); return; }
    char s[24], g[20], sub[40];
    snprintf(s, sizeof(s), XT(X_BLOCKED_FMT), defScore);
    snprintf(g, sizeof(g), XT(X_DEF_GAIN_FMT), defGain);
    snprintf(sub, sizeof(sub), XT(X_DEF_SUB_FMT), defPerfectN, defGoodN);
    drawTrainResult(s, g, 0x4C98, defNewHi && defScore > 0, pet.defHi, sub);
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
  // carril (medidor): pista hundida, franja amarilla (bien) con centro verde (perfecto)
  const int rx = CX - DEF_RAIL_W / 2, rt = DEF_Y0 - 30, rb = DEF_ZONE_Y + DEF_GOOD + 20;
  uiShade(rx, rt, DEF_RAIL_W, rb - rt, DEF_RAIL_W / 2, 4);
  gfx->drawRoundRect(rx, rt, DEF_RAIL_W, rb - rt, DEF_RAIL_W / 2, C565(0xf0, 0xf0, 0xf0));
  uint16_t goodC = C565(0xf4, 0xc4, 0x3c), perfC = C565(0x4c, 0xc8, 0x5c);
  gfx->fillRoundRect(rx - 10, DEF_ZONE_Y - DEF_GOOD, DEF_RAIL_W + 20, 2 * DEF_GOOD, 8, goodC);
  gfx->fillRoundRect(rx - 10, DEF_ZONE_Y - DEF_PERFECT, DEF_RAIL_W + 20, 2 * DEF_PERFECT, 6, perfC);
  gfx->drawRoundRect(rx - 10, DEF_ZONE_Y - DEF_GOOD, DEF_RAIL_W + 20, 2 * DEF_GOOD, 8, ink);
  gfx->drawFastHLine(rx - 16, DEF_ZONE_Y, DEF_RAIL_W + 32, UI_WHITE);
  if (pmd.loaded) {  // x3,5 como en el voleibol
    uint8_t act = defJudge == 4 && now - defJudgeT < 500 ? PMD_HURT : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdActM(pmd, act, CX, 394, now, true, false, 4, 124);
  }
  // balon (esperando arriba: tiembla un poco)
  if (!defOverUntil) {
    int by = (int)defBallY(now);
    int bx = CX + (defDropT ? 0 : (int)(3 * sinf(now * 0.03f)));
    if (by <= rb) drawMap(SPR_ICON_PLAY, 16, bx - 24, by - 24, 3, false);
  }
  // juicio: aro + texto que sube
  if (defJudge && now - defJudgeT < 650) {
    uint32_t t = now - defJudgeT;
    uint16_t c = defJudge == 1 ? perfC : defJudge == 2 ? goodC : UI_BAR_BAD;
    int r = 26 + (int)(t / 10);
    gfx->drawCircle(CX, defJudgeY, r, c);
    gfx->drawCircle(CX, defJudgeY, r - 2, c);
    XId id = defJudge == 1 ? X_DEF_PERFECT : defJudge == 2 ? X_DEF_GOOD : defJudge == 3 ? X_DEF_EARLY : X_DEF_LATE;
    // a la derecha del carril, subiendo un poco
    uint8_t sz = defJudge == 1 ? 3 : 2;
    const char *jt = XT(id);
    setSize(sz);
    if (textW(jt, sz) > LCD_WIDTH - (rx + DEF_RAIL_W + 18) - 30) { sz = 2; setSize(sz); }
    gfx->setTextColor(defJudge == 1 ? C565(0x1a, 0x86, 0x34) : defJudge == 2 ? C565(0xb8, 0x7a, 0x00) : UI_BAR_BAD);  // texto mas oscuro que el aro
    setCur(rx + DEF_RAIL_W + 18, DEF_ZONE_Y - 60 - (int)(t / 25));
    printT(jt);
  }
  if (defCombo >= 2) {  // a la izquierda del carril
    char cb[20];
    snprintf(cb, sizeof(cb), XT(X_DEF_COMBO_FMT), defCombo);
    setSize(2);
    gfx->setTextColor(C565(0x1a, 0x86, 0x34));
    setCur(rx - 18 - textW(cb, 2), DEF_ZONE_Y - 12);
    printT(cb);
  }
  if (now - defStart < 2500) drawFit(XT(X_TR_DEF_HINT), 188, 320, ink, 2);
  uiFlush();
}

// ---------- ataque (ko12.9.5; antes velocidad): dianas numeradas ----------
// ko11.14: EN ORDEN. Salen 3-5 pokeballs numeradas a la vez; hay que tocarlas
// 1, 2, 3... lo antes posible. Orden equivocado o sin tiempo = ronda fallada.
// Puntos por la media de cada balon (100 - ms/10, minimo 10)

// ko11.16: 3 balones las rondas 1-3, 4 las 4-7 y 5 desde la 8 (antes 5 / 10)
static uint8_t spdCount() { return spdRound < 3 ? 3 : spdRound < 7 ? 4 : 5; }

static uint32_t spdLimit() {  // tiempo de la ronda
  int per = 1000 - spdRound * 25;
  if (per < 600) per = 600;
  return (uint32_t)per * spdN;
}

// ko11.16: antes al azar con 60 intentos: si no encontraba hueco se quedaba con el
// ultimo y dos balones podian montarse (tocabas el bueno y contaba el otro = fallo).
// Ahora cada balon va a una casilla distinta de una rejilla 3x3 (107 x 86 px; el balon mide 64) con
// un poco de temblor: nunca se tocan
static void spdPlace() {
  spdN = spdCount();
  uint8_t cell[9];
  for (int i = 0; i < 9; i++) cell[i] = (uint8_t)i;
  for (int i = 8; i > 0; i--) {
    int j = random(i + 1);
    uint8_t t = cell[i]; cell[i] = cell[j]; cell[j] = t;
  }
  for (int i = 0; i < spdN; i++) {
    int c = cell[i] % 3, r = cell[i] / 3;
    spdBx[i] = (int16_t)(CX + (c - 1) * 107 + (int)random(-10, 11));
    spdBy[i] = (int16_t)(136 + r * 86 + (int)random(-5, 6));
  }
}

static void spdNextRound() {
  spdPhase = SP_WAIT;
  spdUntil = millis() + 600 + random(500);
  spdNext = 0;
  spdFail = 0;
  spdPlace();
}

void startSpeed() {
  perfReset();  // ko11.3
  spdOpen = true;
  spdRound = 0;
  spdBase = 0;
  spdScore = 0;
  spdHits = 0;
  spdTargets = 0;
  spdRtSum = 0;
  spdOverUntil = 0;
  spdNewHi = false;
  for (auto &v : spdRes) v = 0;
  spdNextRound();
  spdUntil += 900;  // un respiro para leer la ayuda
}

// ko10.6: puntos por reflejos: 100 - ms/10 (0,25 s = 75), minimo 10
uint16_t spdPoints(uint32_t rt) {
  return rt >= 900 ? 10 : (uint16_t)(100 - rt / 10);
}

static void spdResolve(bool good) {
  spdGood = good;
  if (spdRound < SPD_ROUNDS) spdRes[spdRound] = good ? 1 : 2;
  if (good) {
    uint32_t per = (millis() - spdShowAt) / spdN;  // media por balon
    spdHits++;
    spdRtSum += per;
    spdScore += spdPoints(per);
    sfxPlay(SFX_MEDAL);
  }
  else sfxPlay(SFX_DENY);
  spdPhase = SP_FEED;
  spdUntil = millis() + 550;
}

void speedPress(int16_t x, int16_t y) {
  if (spdOverUntil || spdPhase != SP_SHOW) return;  // antes de salir no penaliza
  int best = -1, bd = 60 * 60;
  for (int i = spdNext; i < spdN; i++) {
    int dx = x - spdBx[i], dy = y - spdBy[i], d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  if (best < 0) return;  // toque en vacio: no cuenta
  if (best != spdNext) { spdFail = 1; spdResolve(false); return; }
  spdTapT[spdNext] = millis();
  spdNext++;
  spdTargets++;
  if (spdNext >= spdN) spdResolve(true);
  else sfxPlay(SFX_TAP);
}

void stepSpeed() {
  if (timeLeft(spdUntil)) return;
  if (spdPhase == SP_WAIT) {
    spdPhase = SP_SHOW;
    spdShowAt = millis();
    spdUntil = spdShowAt + spdLimit();
  } else if (spdPhase == SP_SHOW) {
    spdFail = 2;
    spdResolve(false);  // no llego a tiempo
  } else if (++spdRound >= SPD_ROUNDS) {
    // ko12.9.5: ahora entrena el ATAQUE: record = dianas (como las rocas / sacos de antes)
    spdNewHi = spdTargets > pet.strHi;
    spdGain = pet.trainStrength(spdTargets, spdTargets);
    sfxPlay(spdNewHi ? SFX_MEDAL : SFX_PLAY);
    spdOverUntil = millis() + 3500;
  } else {
    // ko11.17: hasta la mitad los numeros siguen contando (1-3, 4-7...); desde la
    // ronda 8 (la de 5 balones) empiezan en un numero al azar (p. ej. 2-6, luego 37-41)
    if (spdRound < SPD_RANDOM_FROM) spdBase += spdN;
    else {
      uint16_t prev = spdBase;
      do spdBase = (uint16_t)random(0, SPD_RANDOM_MAX); while (spdBase == prev);
    }
    spdNextRound();
  }
}

// ko12.9.5: diana roja y blanca (el numero va encima, en el circulo blanco del centro)
static void drawTarget(int x, int y, int r) {
  const uint16_t RED = C565(0xe0, 0x40, 0x30);
  gfx->fillCircle(x, y, r, RED);
  gfx->fillCircle(x, y, r * 3 / 4, UI_WHITE);
  gfx->fillCircle(x, y, r / 2 + 2, RED);
  gfx->drawCircle(x, y, r, UI_INK);
  gfx->drawCircle(x, y, r - 1, UI_INK);
}

void renderSpeed() {
  if (spdOverUntil) {
    if (!timeLeft(spdOverUntil)) { spdOpen = false; backToTrainMenu(); return; }
    char s[24], g[20];
    snprintf(s, sizeof(s), XT(X_TGT_FMT), (unsigned)spdTargets);
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), spdGain);
    char sub[48];
    uint32_t avg = spdHits ? spdRtSum / spdHits : 0;
    snprintf(sub, sizeof(sub), XT(X_SPE_AVG_FMT), (unsigned)spdHits, (unsigned)SPD_ROUNDS,
             (unsigned)(avg / 1000), (unsigned)(avg % 1000 / 10));
    drawTrainResult(s, g, UI_BAR_BAD, spdNewHi && spdTargets > 0, pet.strHi, sub);
    return;
  }
  stepSpeed();
  if (spdOverUntil) return;
  uint32_t now = millis();
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  const uint16_t okC = C565(0x4c, 0xc8, 0x5c);
  // ko11.16: el tiempo es un aro alrededor de la pantalla redonda (verde -> rojo)
  if (spdPhase == SP_SHOW) {
    uint32_t lim = spdLimit(), left = timeLeft(spdUntil);
    float f = lim ? (float)left / lim : 0;
    uint16_t rc = f > 0.5f ? okC : f > 0.25f ? UI_BAR_WARN : UI_BAR_BAD;
    gfx->fillArc(CX, CX, 232, 222, 0, 360, lerp565(UI_TRACK, UI_INK, 2, 16));
    if (f > 0.01f) gfx->fillArc(CX, CX, 232, 222, 270, 270 + f * 360.0f, rc);
  }
  // puntos arriba y 15 puntitos de progreso (verde bien, rojo fallo, aro = ahora)
  char b[24];
  snprintf(b, sizeof(b), XT(X_TGT_FMT), (unsigned)spdTargets);
  drawFit(b, 34, 220, ink, 3);
  for (int i = 0; i < SPD_ROUNDS; i++) {
    int dx = CX - (SPD_ROUNDS - 1) * 7 + i * 14, dy = 84;
    if (spdRes[i] == 1) gfx->fillCircle(dx, dy, 5, okC);
    else if (spdRes[i] == 2) gfx->fillCircle(dx, dy, 5, UI_BAR_BAD);
    else gfx->fillCircle(dx, dy, 4, lerp565(UI_TRACK, UI_INK, 3, 16));
    if (i == spdRound && !spdOverUntil) gfx->drawCircle(dx, dy, 7, ink);
  }
  // el bicho abajo: dispara a cada diana que se toca (ko12.9.5)
  uint32_t lastTap = spdNext ? now - spdTapT[spdNext - 1] : 9999;
  if (pmd.loaded) {
    uint8_t act = spdPhase == SP_FEED && !spdGood ? PMD_HURT
                : lastTap < 260 ? (pmd.has(PMD_SHOOT) ? PMD_SHOOT : PMD_ATTACK) : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdActM(pmd, act, CX, 420, now, true, false, 3, 96);
  }
  if (spdPhase == SP_SHOW || spdPhase == SP_FEED) {
    uint32_t age = now - spdShowAt;
    for (int i = spdN - 1; i >= 0; i--) {
      int x = spdBx[i], y = spdBy[i];
      if (i < spdNext) {  // tocado: estallido (aro que crece y se apaga)
        uint32_t t = now - spdTapT[i];
        if (t < 260) {
          int rr = 26 + (int)(t / 6);
          uint16_t c = lerp565(okC, UI_WHITE, (int)(t / 30), 16);
          gfx->drawCircle(x, y, rr, c);
          gfx->drawCircle(x, y, rr - 1, c);
          for (int k = 0; k < 6; k++) {  // chispas
            float a = k * 1.047f;
            gfx->fillCircle(x + (int)(cosf(a) * (rr + 6)), y + (int)(sinf(a) * (rr + 6)), 3, c);
          }
        }
        continue;
      }
      // aparece con un pequeno salto (escala 2 -> 4,25 -> 4 en ~150 ms, uno tras otro)
      int32_t ta = (int32_t)age - i * 40;
      if (ta < 0) continue;
      int s4 = ta >= 150 ? 16 : ta < 100 ? 8 + (int)(ta * 9 / 100) : 17 - (int)((ta - 100) / 50);
      int half = s4 * 2;
      uiShade(x - half * 3 / 5, y + half - 7, half * 6 / 5, 8, 4, 4);  // sombra pegada a la diana
      drawTarget(x, y, half);  // ko12.9.5: diana (antes pokeball)
      if (s4 >= 14) {
        // ko11.17: numeros que siguen subiendo; solo el PRIMERO de la ronda parpadea
        // (pista de por donde empezar), el resto hay que seguirlo de cabeza
        char nb[6];
        snprintf(nb, sizeof(nb), "%u", (unsigned)(spdBase + i + 1));
        bool hint = i == 0 && spdNext == 0 && ((now / 220) % 2 == 0);
        int dr = strlen(nb) > 1 ? 20 : 17;
        if (i == 0 && spdNext == 0) {  // aro que late alrededor del primero
          int pr = half + 4 + (int)((now / 60) % 6);
          gfx->drawCircle(x, y, pr, C565(0xf8, 0xc8, 0x30));
          gfx->drawCircle(x, y, pr + 1, C565(0xf8, 0xc8, 0x30));
        }
        gfx->fillCircle(x, y, dr, hint ? C565(0xff, 0xd8, 0x40) : UI_WHITE);
        gfx->drawCircle(x, y, dr, UI_INK);
        gfx->drawCircle(x, y, dr - 1, UI_INK);
        uint8_t ts = textW(nb, 3) <= dr * 2 - 6 ? 3 : 2;
        setSize(ts);
        gfx->setTextColor(UI_INK);
        setCur(x - textW(nb, ts) / 2, y - textH(ts) / 2);
        printT(nb);
      }
    }
  }
  // ko12.9.5: el ataque del bicho vuela hasta la ultima diana tocada
  if (spdNext && lastTap < 420) {
    int i = spdNext - 1;
    drawMoveFx(DEX_TBL[pet.speciesId].ptype, CX, 360, spdBx[i], spdBy[i], 120 + lastTap * 2, true, 2,
               moveTier(pet.speciesId), pet.moveVar());
  }
  if (spdPhase == SP_FEED) {
    const char *fb = XT(spdGood ? X_NICE : spdFail == 1 ? X_SPD_WRONG : X_MISS);
    drawFit(fb, 330, 320, spdGood ? C565(0x1a, 0x86, 0x34) : UI_BAR_BAD, 3);
  } else if (spdRound == 0 && spdPhase == SP_WAIT) {
    drawFit(XT(X_TR_SPE_HINT), 200, 320, ink, 2);
  }
  uiFlush();
}

// ---------- ganchos para TamaPoke.ino ----------

bool trainingRender() {
  if (trainMenuOpen) { renderTrainMenu(); return true; }
  if (defOpen) { renderDefense(); return true; }
  if (spdOpen) { renderSpeed(); return true; }
  if (vbOpen) { renderVolley(); return true; }  // ko11.9
  if (shellOpen) { renderShell(); return true; }  // ko12.9.5
  if (simonOpen) { renderSimon(); return true; }
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
  else if (shellOpen) shellPress(x, y);  // ko12.9.5
  else if (simonOpen) simonPress(x, y);
}

// ko9.1: abandonar sin premio (mantener el dedo 2 s, ver handleTouch)
void trainingQuit() { defOpen = spdOpen = vbOpen = shellOpen = simonOpen = false; }

bool trainingSwipe() {
  if (trainMenuOpen) { trainMenuClose(); return true; }
  return trainingFast();
}

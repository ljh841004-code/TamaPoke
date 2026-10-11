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
// ko12.9.14: un rival arriba a la derecha lanza su tecnica hacia el escudo (el centro de la franja)
int16_t defFoeDex = 4;
#define DEF_FOE_X (CX + 128)
#define DEF_FOE_G 158         // suelo del rival
#define DEF_L_X (DEF_FOE_X - 26)  // de donde sale el ataque (a la altura DEF_Y0)

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
// ko12.9.5: en el ataque se perdonan 2 toques equivocados por ronda (el dedo da a la de al lado);
// el tercero falla la ronda. El tiempo sigue corriendo
#define SPD_FORGIVE 2
uint8_t spdWrong = 0, spdWrongI = 0;
uint32_t spdWrongT = 0;
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

// ko12.9.14: [X] para salir sin premio en todos los juegos (antes solo mantener el dedo 2 s).
// Arriba a la izquierda, en la diagonal (en "따라 해 봐!" cae en la raya entre dos botones).
// ko12.9.15: el [X] PARA el juego y pregunta "그만할까요?" [그만하기] / [계속하기] (antes dos toques
// seguidos salian: con prisa podia pasar sin querer). Al seguir, los tiempos del juego se corren lo
// que duro la pausa (no cuenta como fallo ni se acaba el tiempo)
#define TQ_X 82   // ko12.9.15: un poco mas al borde (88,92 -> 82,86): no roza la diana de arriba a la izquierda
#define TQ_Y0 86
#define TQ_Y (vbOpen ? 150 : TQ_Y0)  // en el voleibol, debajo del nombre del jugador
#define TQ_R 20
#define TP_BY 262   // botones del aviso
#define TP_BH 58
#define TP_QX 79
#define TP_RX 239
#define TP_BW 148
static uint32_t trainPauseT = 0;  // 0 = jugando
bool vbPlaying();      // volley.ino
bool shellOverProbe();  // train_more.ino
bool simonOverProbe();
void shellShiftTime(uint32_t d);
void simonShiftTime(uint32_t d);
void vbShiftTime(uint32_t d);
static bool trainQuitShown() {
  return (defOpen && !defOverUntil) || (spdOpen && !spdOverUntil) || (shellOpen && !shellOverProbe()) ||
         (simonOpen && !simonOverProbe()) || vbPlaying();
}
bool trainPaused() { return trainPauseT != 0 && trainingFast(); }
void drawTrainQuit() {
  if (!trainQuitShown()) return;
  gfx->fillCircle(TQ_X, TQ_Y, TQ_R, UI_WHITE);
  gfx->drawCircle(TQ_X, TQ_Y, TQ_R, UI_INK);
  gfx->drawCircle(TQ_X, TQ_Y, TQ_R - 1, UI_INK);
  for (int k = -1; k <= 1; k++) {
    gfx->drawLine(TQ_X - 8 + k, TQ_Y - 8, TQ_X + 8 + k, TQ_Y + 8, UI_INK);
    gfx->drawLine(TQ_X + 8 + k, TQ_Y - 8, TQ_X - 8 + k, TQ_Y + 8, UI_INK);
  }
}
// el aviso (el juego queda parado debajo: solo el paisaje)
void renderTrainPause() {
  drawGameScene();
  uiPanel(56, 120, 354, 218, 18, UI_WHITE, UI_INK);
  drawFit(XT(X_TQ_TITLE), 150, 320, UI_INK, 3);
  drawFit(XT(X_TQ_SUB), 208, 320, C565(0x60, 0x68, 0x70), 2);
  drawBtn(TP_QX, TP_BY, TP_BW, TP_BH, UI_BAR_BAD, UI_WHITE, XT(X_TQ_QUIT));
  drawBtn(TP_RX, TP_BY, TP_BW, TP_BH, UI_BAR_OK, UI_WHITE, XT(X_TQ_RESUME));
  uiFlush();
}
static void trainShiftTime(uint32_t d) {
  if (defOpen) {
    defStart += d; defLastStep += d; defNextSpawn += d;
    if (defDropT) defDropT += d;
    if (defJudgeT) defJudgeT += d;
  }
  if (spdOpen) {
    spdUntil += d; spdShowAt += d;
    for (auto &t : spdTapT) t += d;
    if (spdWrongT) spdWrongT += d;
  }
  if (shellOpen) shellShiftTime(d);
  if (simonOpen) simonShiftTime(d);
  if (vbOpen) vbShiftTime(d);
}
// true = el toque era del [X] o del aviso (no pasa al juego)
bool trainQuitPress(int16_t x, int16_t y) {
  if (trainPaused()) {
    if (y >= TP_BY && y < TP_BY + TP_BH) {
      if (x >= TP_QX && x < TP_QX + TP_BW) {  // [그만하기]
        trainingQuit();
        backToTrainMenu();
        sfxPlay(SFX_DENY);
      } else if (x >= TP_RX && x < TP_RX + TP_BW) {  // [계속하기]
        trainShiftTime(millis() - trainPauseT);
        trainPauseT = 0;
        sfxPlay(SFX_TAP);
      }
    }
    return true;  // con el aviso abierto nada llega al juego
  }
  if (!trainQuitShown()) return false;
  int dx = x - TQ_X, dy = y - TQ_Y, rr = TQ_R + 4;
  if (dx * dx + dy * dy > rr * rr) return false;
  trainPauseT = millis();
  if (!trainPauseT) trainPauseT = 1;
  sfxPlay(SFX_TAP);
  return true;
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

static int16_t defPickFoe() {
  for (int tries = 0; tries < 40; tries++) {
    int16_t d = (int16_t)(1 + random(DEX_COUNT));
    if (d == pet.speciesId || DEX_TBL[d].rarity == R_LEGENDARIO) continue;
    return d;
  }
  return 4;
}

void startDefense() {
  perfReset();  // ko11.3
  defFoeDex = defPickFoe();
  loadFoe(defFoeDex, false);
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
  // ko12.9.14: el rival (arriba a la derecha) lanza su tecnica por una linea que pasa por el
  // centro del escudo; el juicio sigue siendo la altura (defBallY) como antes
  const uint16_t goodC = C565(0xf4, 0xc4, 0x3c), perfC = C565(0x4c, 0xc8, 0x5c), shC = C565(0x8c, 0xd8, 0xff);
  const uint8_t ft = DEX_TBL[defFoeDex].ptype;
  const int lx = DEF_L_X, ly = DEF_Y0, zx = CX, zy = DEF_ZONE_Y;
  const int ey = DEF_ZONE_Y + DEF_GOOD + 20, ex = lx + (zx - lx) * (ey - ly) / (zy - ly);  // fin del recorrido
  uint32_t jt = defJudge ? now - defJudgeT : 9999;
  bool hurt = defJudge == 4 && jt < 500;
  int shake = hurt ? (int)(8 * sinf(jt * 0.09f)) : 0;
  // camino punteado del rival al escudo
  for (int i = 1; i < 9; i++) {
    int px = lx + (zx - lx) * i / 9, py = ly + (zy - ly) * i / 9;
    gfx->fillCircle(px, py, 2, lerp565(UI_WHITE, ink, 3, 16));
  }
  // escudo: aro amarillo (bien) con el centro verde (perfecto)
  gfx->fillCircle(zx, zy, DEF_GOOD + 6, lerp565(shC, UI_WHITE, 8, 16));
  gfx->fillCircle(zx, zy, DEF_GOOD, goodC);
  gfx->fillCircle(zx, zy, DEF_PERFECT + 2, perfC);
  gfx->drawCircle(zx, zy, DEF_GOOD + 6, shC);
  gfx->drawCircle(zx, zy, DEF_GOOD, ink);
  gfx->drawCircle(zx, zy, DEF_PERFECT + 2, ink);
  if (pmd.loaded) {  // x3,5 como en el voleibol
    uint8_t act = hurt ? PMD_HURT : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdActM(pmd, act, CX + shake, 394, now, true, false, 4, 124);
  }
  // el rival: carga (esperando), dispara (al soltar) o quieto
  if (foePmd.loaded) {
    uint8_t act = !defDropT ? PMD_CHARGE : now - defDropT < 300 ? PMD_SHOOT : PMD_IDLE;
    if (act == PMD_SHOOT && !foePmd.has(act)) act = PMD_ATTACK;
    if (!foePmd.has(act)) act = PMD_IDLE;
    drawPmdActM(foePmd, act, DEF_FOE_X, DEF_FOE_G, now, true, false, 3, 100);
  }
  if (!defDropT) {  // cargando: una bola de su color que late
    int r = 6 + (int)((now / 70) % 5);
    gfx->fillCircle(lx, ly, r + 3, lerp565(typeColor(ft), UI_WHITE, 8, 16));
    gfx->fillCircle(lx, ly, r, typeColor(ft));
  } else {  // en vuelo: el efecto de su tipo (el mismo de los combates)
    float p = (defBallY(now) - ly) / (float)(ey - ly);
    if (p <= 1.0f) drawMoveFx(ft, lx, ly, ex, ey, 120 + (uint32_t)(p * 260), false, 2, moveTier(defFoeDex), 0);
  }
  // juicio
  if (defJudge && jt < 650) {
    if (defJudge <= 2) {  // parado: destello del escudo + chispas
      if (jt < 120) gfx->fillCircle(zx, zy, DEF_GOOD + 10, UI_WHITE);
      int r = DEF_GOOD + 8 + (int)(jt / 8);
      uint16_t c = lerp565(shC, UI_WHITE, (int)(jt / 50), 16);
      for (int k = 0; k < 3; k++) gfx->drawCircle(zx, zy, r - k * 2, c);
      for (int k = 0; k < 8; k++) {
        float a = k * 0.785f + 0.39f;
        int r0 = r + 4, r1 = r + 14;
        gfx->drawLine(zx + (int)(cosf(a) * r0), zy + (int)(sinf(a) * r0), zx + (int)(cosf(a) * r1),
                      zy + (int)(sinf(a) * r1), defJudge == 1 ? perfC : goodC);
      }
    } else if (defJudge == 4) {  // le dio: el golpe en el bicho y la pantalla en rojo
      drawMoveFx(ft, lx, ly, CX + shake, 330, 350 + jt, true, 2, moveTier(defFoeDex), 0);
      if (jt < 300) for (int k = 0; k < 4; k++) gfx->drawCircle(CX, CX, 232 - k, UI_BAR_BAD);
    } else if (jt < 300) {  // pronto: se deshace en humo
      for (int k = 0; k < 5; k++)
        gfx->fillCircle(zx + (k - 2) * 9, defJudgeY - 30 - (int)(jt / 12) + (k % 2) * 6, 7 - (int)(jt / 60),
                        lerp565(UI_TRACK, UI_WHITE, 8, 16));
    }
    XId id = defJudge == 1 ? X_DEF_PERFECT : defJudge == 2 ? X_DEF_GOOD : defJudge == 3 ? X_DEF_EARLY : X_DEF_LATE;
    uint8_t sz = defJudge == 1 ? 3 : 2;
    const char *jtx = XT(id);
    int tx0 = zx + DEF_GOOD + 24;
    if (textW(jtx, sz) > LCD_WIDTH - tx0 - 30) sz = 2;
    setSize(sz);
    gfx->setTextColor(defJudge == 1 ? C565(0x1a, 0x86, 0x34) : defJudge == 2 ? C565(0xb8, 0x7a, 0x00) : UI_BAR_BAD);  // texto mas oscuro que el aro
    setCur(tx0, DEF_ZONE_Y - 20 - (int)(jt / 25));
    printT(jtx);
  }
  if (defCombo >= 2) {  // a la izquierda del escudo
    char cb[20];
    snprintf(cb, sizeof(cb), XT(X_DEF_COMBO_FMT), defCombo);
    setSize(2);
    gfx->setTextColor(C565(0x1a, 0x86, 0x34));
    setCur(zx - DEF_GOOD - 24 - textW(cb, 2), DEF_ZONE_Y - 12);
    printT(cb);
  }
  if (now - defStart < 2500) drawFit(XT(X_TR_DEF_HINT), 188, 320, ink, 2);
  drawTrainQuit();  // ko12.9.14
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
  spdWrong = 0;
  spdWrongT = 0;
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
  if (best != spdNext) {
    if (++spdWrong <= SPD_FORGIVE) {  // ko12.9.5: perdonado: tiembla esa diana y sigue la ronda
      spdWrongI = (uint8_t)best;
      spdWrongT = millis();
      sfxPlay(SFX_DENY);
      return;
    }
    spdFail = 1;
    spdResolve(false);
    return;
  }
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

// ko12.9.14: aro de tiempo que se cierra (centro en el de la rejilla de dianas)
#define SPD_RING_CY 222
#define SPD_RING_MAX 226
#define SPD_RING_MIN 30
#define SPD_RING_W 10

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
  // ko12.9.14: el tiempo es un aro grueso que se cierra hacia el centro de las dianas
  // (antes un arco fino en el borde que costaba ver). Verde -> amarillo -> rojo; al final parpadea
  // y hace tic. Se dibuja antes que las dianas: pasa por detras
  if (spdPhase == SP_SHOW) {
    static uint8_t tickLast = 0;
    uint32_t lim = spdLimit(), left = timeLeft(spdUntil);
    float f = lim ? (float)left / lim : 0;
    uint16_t rc = f > 0.5f ? okC : f > 0.25f ? UI_BAR_WARN : UI_BAR_BAD;
    int r = SPD_RING_MIN + (int)((SPD_RING_MAX - SPD_RING_MIN) * f);
    bool blink = f <= 0.25f && (now / 120) % 2;
    if (!blink) gfx->fillArc(CX, SPD_RING_CY, r, r - SPD_RING_W, 0, 360, rc);
    gfx->drawCircle(CX, SPD_RING_CY, r, ink);
    if (f <= 0.25f) {  // tic cada 250 ms en el ultimo cuarto
      uint8_t tk = (uint8_t)(left / 250 + 1);
      if (tk != tickLast) { tickLast = tk; sfxPlay(SFX_TAP); }
    } else tickLast = 0;
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
      bool shake = spdWrong && i == spdWrongI && now - spdWrongT < 350;  // ko12.9.5: toque perdonado
      if (shake) x += (int)(7 * sinf((now - spdWrongT) * 0.09f));
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
  if (spdPhase == SP_SHOW && spdWrong && now - spdWrongT < 700) {  // ko12.9.5: "앗! 다시 (실수 1/2)"
    char ob[40];
    snprintf(ob, sizeof(ob), XT(X_TGT_OOPS_FMT), (unsigned)spdWrong);
    drawFit(ob, 330, 320, UI_BAR_BAD, 2);
  }
  if (spdPhase == SP_FEED) {
    const char *fb = XT(spdGood ? X_NICE : spdFail == 1 ? X_SPD_WRONG : X_MISS);
    drawFit(fb, 330, 320, spdGood ? C565(0x1a, 0x86, 0x34) : UI_BAR_BAD, 3);
  } else if (spdRound == 0 && spdPhase == SP_WAIT) {
    drawFit(XT(X_TR_SPE_HINT), 200, 320, ink, 2);
  }
  drawTrainQuit();  // ko12.9.14
  uiFlush();
}

// ---------- ganchos para TamaPoke.ino ----------

bool trainingRender() {
  if (trainMenuOpen) { renderTrainMenu(); return true; }
  if (trainPaused()) { renderTrainPause(); return true; }  // ko12.9.15
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
void trainingQuit() { defOpen = spdOpen = vbOpen = shellOpen = simonOpen = false; trainPauseT = 0; }

bool trainingSwipe() {
  if (trainMenuOpen) { trainMenuClose(); return true; }
  return trainingFast();
}

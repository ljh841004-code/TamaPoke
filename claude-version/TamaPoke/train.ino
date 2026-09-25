// train.ino - fork KO (ko4): menu de entrenamiento y los juegos de DEF y VEL
// (se concatena tras TamaPoke.ino)
//
//   - Ataque:  saco de golpes (ya existia): toques rapidos en 10 s
//   - Defensa: caen pokeballs del cielo; tocarlas antes de que lleguen al suelo
//   - Velocidad: aparece una pokeball a la izquierda o a la derecha un instante;
//                tocarla a tiempo. 15 rondas, cada vez mas rapido
//   - Pelota: el juego de siempre (ahora solo sube el animo)
//
// Se engancha a TamaPoke.ino por funciones: trainingRender / trainingTap /
// trainingFast / trainingPress / trainingSwipe (como ui_extra.ino).

bool trainMenuOpen = false;
uint32_t trainMsgUntil = 0;  // aviso en el menu ("demasiado cansado")
const char *trainMsg = nullptr;

#define TRM_X 73
#define TRM_W 320
#define TRM_Y 90
#define TRM_H 54
#define TRM_GAP 8

// ---------- defensa: pokeballs que caen ----------
#define DEF_MS 20000UL
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
enum : uint8_t { SP_WAIT = 0, SP_SHOW, SP_FEED };
bool spdOpen = false;
uint8_t spdRound = 0, spdPhase = SP_WAIT, spdSide = 0, spdGain = 0;
uint16_t spdScore = 0;
uint32_t spdUntil = 0, spdOverUntil = 0;
bool spdGood = false, spdNewHi = false;

bool trainingFast() { return defOpen || spdOpen; }  // toques al apoyar el dedo
bool trainingOpen() { return trainMenuOpen || trainingFast(); }

void openTrainMenu() {
  if (pet.isEgg() || pet.ceremony) return;
  cardOpen = false;
  trainMenuOpen = true;
  trainMsgUntil = 0;
}

// ---------- menu ----------

void renderTrainMenu() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
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
  if (timeLeft(trainMsgUntil) && trainMsg) drawFit(trainMsg, 344, 320, UI_BAR_BAD, 2);
  drawFit(T(S_BACK), 380, 220, UI_INK, 2);
  gfx->flush();
}

void trainMenuTap(int16_t x, int16_t y) {
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

void drawTrainResult(const char *score, const char *gain, uint16_t gainCol, bool newHi, uint16_t hi) {
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
    defNextSpawn = now + (gap < 430 ? 430 : gap);
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
    drawTrainResult(s, g, 0x4C98, defNewHi && defScore > 0, pet.defHi);
    return;
  }
  if (now - defStart >= DEF_MS) {  // se acabo: aplicar entrenamiento
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
  drawTimeBar(DEF_MS - (now - defStart), DEF_MS, 60);
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
  spdSide = random(2);
}

void startSpeed() {
  spdOpen = true;
  spdRound = 0;
  spdScore = 0;
  spdOverUntil = 0;
  spdNewHi = false;
  spdNextRound();
  spdUntil += 700;  // un respiro para leer la ayuda
}

static int spdBallX() { return spdSide ? 336 : 130; }
#define SPD_BALL_Y 212

static void spdResolve(bool good) {
  spdGood = good;
  if (good) { spdScore++; sfxPlay(SFX_PLAY); }
  else sfxPlay(SFX_DENY);
  spdPhase = SP_FEED;
  spdUntil = millis() + 450;
}

void speedPress(int16_t x, int16_t y) {
  if (spdOverUntil || spdPhase == SP_FEED) return;
  if (spdPhase == SP_WAIT) { spdResolve(false); return; }  // se adelanto
  int dx = x - spdBallX(), dy = y - SPD_BALL_Y;
  spdResolve(dx * dx + dy * dy <= 80 * 80);
}

void stepSpeed() {
  if (timeLeft(spdUntil)) return;
  if (spdPhase == SP_WAIT) {
    spdPhase = SP_SHOW;
    spdUntil = millis() + spdWindow();
  } else if (spdPhase == SP_SHOW) {
    spdResolve(false);  // no llego a tiempo
  } else if (++spdRound >= SPD_ROUNDS) {
    spdNewHi = spdScore > pet.speHi;
    spdGain = pet.trainSpeed(spdScore);
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
    snprintf(s, sizeof(s), XT(X_SPE_RESULT_FMT), spdScore);
    snprintf(g, sizeof(g), XT(X_SPE_GAIN_FMT), spdGain);
    drawTrainResult(s, g, UI_BAR_WARN, spdNewHi && spdScore > 0, pet.speHi);
    return;
  }
  stepSpeed();
  if (spdOverUntil) return;
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  char b[16];
  snprintf(b, sizeof(b), "%u/%u", spdRound + 1, SPD_ROUNDS);
  drawFit(b, 30, 200, ink, 3);
  snprintf(b, sizeof(b), "%u", spdScore);
  drawFit(b, 70, 200, UI_BAR_OK, 2);
  // el bicho mira hacia donde salio la pokeball
  uint8_t act = spdPhase == SP_SHOW ? (spdSide ? PMD_WALKR : PMD_WALKL) : PMD_IDLE;
  drawTrainPet(CX, act);
  if (spdPhase == SP_SHOW) {
    int bx = spdBallX();
    gfx->fillCircle(bx, SPD_BALL_Y, 44, lerp565(UI_WHITE, UI_BAR_WARN, 5, 16));  // halo
    drawMap(SPR_ICON_PLAY, 16, bx - 32, SPD_BALL_Y - 32, 4, false);
    // la ventana que queda, como un arco que se vacia
    uint32_t left = timeLeft(spdUntil), win = spdWindow();
    int w = (int)(80 * left / win);
    gfx->fillRoundRect(bx - 40, SPD_BALL_Y + 50, 80, 6, 3, UI_TRACK);
    if (w > 1) gfx->fillRoundRect(bx - 40, SPD_BALL_Y + 50, w, 6, 3, UI_BAR_WARN);
  } else if (spdPhase == SP_FEED) {
    drawFit(XT(spdGood ? X_NICE : X_MISS), 200, 300, spdGood ? UI_BAR_OK : UI_BAR_BAD, 3);
  } else if (spdRound == 0) {
    drawFit(XT(X_TR_SPE_HINT), 200, 320, ink, 2);
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
  if (y < 72) { defOpen = spdOpen = false; return; }  // tocar arriba = abandonar sin premio
  if (defOpen) defensePress(x, y);
  else if (spdOpen) speedPress(x, y);
}

bool trainingSwipe() {
  if (trainMenuOpen) { trainMenuOpen = false; return true; }
  return trainingFast();
}

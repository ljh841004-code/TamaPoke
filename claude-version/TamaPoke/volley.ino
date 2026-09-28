// volley.ino - ko11.9: minijuego de voleibol (Pikachu Volleyball, 1997, a la
// pantalla redonda). Izquierda = el que criamos; derecha = un Pokemon al azar
// que maneja la IA. 5 puntos. La fisica y la IA estan en volley.h.
//
// Toques (un solo dedo). ko11.9.1: el nuestro corre solo hacia la pelota.
//   - tocar en cualquier sitio = saltar; si toca la pelota en el aire = remate (spike)
//   - mantener el marcador 2 s = abandonar (sin premio)
#include "volley.h"
void crumb(uint16_t w);  // TamaPoke.ino (ko11.9.2)

#define VBC_RESULT_MIN_MS 1500

bool vbOpen = false;
VolleyGame vb;
uint32_t vbLast = 0, vbOverAt = 0, vbIntroUntil = 0;
int16_t vbFoeDex = 25;
bool vbFoeShiny = false;
bool vbRewarded = false, vbRecord = false;
uint8_t vbGain = 0;
uint8_t vbLastScore[2] = {0, 0};

static float vbClampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static int16_t vbPickFoe() {
  for (int tries = 0; tries < 40; tries++) {
    int16_t d = (int16_t)(1 + random(DEX_COUNT));
    if (d == pet.speciesId) continue;
    if (DEX_TBL[d].rarity == R_LEGENDARIO) continue;  // los legendarios no juegan al voleibol
    return d;
  }
  return 25;
}

void startVolley() {
  perfReset();
  vbFoeDex = vbPickFoe();
  vbFoeShiny = random(64) == 0;
  loadFoe(vbFoeDex, vbFoeShiny);
  uint8_t lvl = pet.vbStreak > 5 ? 5 : (uint8_t)pet.vbStreak;  // cada victoria seguida, rival mejor
  vb.begin((uint32_t)esp_random(), lvl);
  // nuestras estadisticas cuentan un poco (sin desequilibrar)
  vb.p[0].speed = 175 + vbClampf(pet.speStat(), 0, 200) * 0.35f;
  vb.p[0].spikePow = 470 + vbClampf(pet.atkStat(), 0, 200) * 0.6f;
  vb.p[0].hitR = 30 + vbClampf(pet.defStat(), 0, 200) / 40.0f;
  vb.p[0].dig = (uint8_t)(60 + vbClampf(pet.defStat(), 0, 200) / 10.0f);  // 60..80 %
  vb.autoMove0 = true;
  const DexEntry &e = DEX_TBL[vbFoeDex];
  vb.p[1].speed = 170 + vbClampf(e.bSpe, 0, 150) * 0.3f + lvl * 10;  // ko11.9.3: +10
  vb.p[1].spikePow = 460 + vbClampf(e.bAtk, 0, 150) * 0.5f + lvl * 15;
  vb.p[1].hitR = 30 + vbClampf(e.bDef, 0, 150) / 50.0f;
  vb.p[1].dig = (uint8_t)(30 + lvl * 8 + vbClampf(e.bDef, 0, 150) / 15.0f);  // 30..80 % (ko11.9.3: +10)
  vbOpen = true;
  vbLast = millis();
  vbIntroUntil = vbLast + 1600;
  vbOverAt = 0;
  vbRewarded = vbRecord = false;
  vbGain = 0;
  vbLastScore[0] = vbLastScore[1] = 0;
}

// ---- toques ----
void vbPress(int16_t x, int16_t y) {
  crumb(0x010A);
  lastInteract = millis();
  if (vb.state == VB_OVER) {
    if (millis() - vbOverAt > VBC_RESULT_MIN_MS) { vbOpen = false; backToTrainMenu(); }  // ko11.9.3
    return;
  }
  if (timeLeft(vbIntroUntil)) { vbIntroUntil = 0; return; }  // saltar la presentacion
  if (y < 104) return;  // marcador (mantener 2 s = salir)
  if (!vb.airborne(0)) {
    vb.jump(0);
    sfxPlay(SFX_PLAY);
  }
}

// ko11.9.1: moverse ya no depende del dedo
void vbHold(int16_t x, int16_t y) {}
void vbRelease() {}

// ---- fin de la partida ----
static void vbFinish() {
  if (vbRewarded) return;
  crumb(0x0109);
  vbRewarded = true;
  bool won = vb.winner == 0;
  uint16_t best0 = pet.vbBest;
  vbGain = pet.volleyResult(won, vb.score[0]);
  vbRecord = won && pet.vbBest > best0;
  sfxPlay(won ? SFX_MEDAL : SFX_BYE);
}

// ---- dibujo ----
static void vbDrawBall(int x, int y, uint32_t now) {
  if (vb.b.spiked) {  // estela del color del que remato
    int16_t d = vb.b.spikeSide ? vbFoeDex : pet.speciesId;
    uint16_t c = DEX_TBL[d].accent;
    for (int k = 1; k <= 4; k++) {
      int tx = x - (int)(vb.b.vx * 0.018f * k), ty = y - (int)(vb.b.vy * 0.018f * k);
      gfx->fillCircle(tx, ty, VB_BALL_R - k * 2, c);
    }
  }
  drawMap(SPR_ICON_PLAY, 16, x - 16, y - 16, 2, false);  // pokeball
}

static void vbDrawNet() {
  uint16_t pole = C565(0x70, 0x70, 0x78), mesh = C565(0xf4, 0xf4, 0xf4);
  gfx->fillRect(VB_NET_X - 2, VB_NET_TOP - 6, 5, VB_GROUND - VB_NET_TOP + 6, pole);
  gfx->fillRect(VB_NET_X - VB_NET_HW - 3, VB_NET_TOP - 4, 2 * VB_NET_HW + 7, 5, mesh);
  for (int y = VB_NET_TOP + 8; y < VB_GROUND; y += 10)
    gfx->drawFastHLine(VB_NET_X - VB_NET_HW, y, 2 * VB_NET_HW + 1, mesh);
  gfx->drawFastVLine(VB_NET_X - VB_NET_HW, VB_NET_TOP, VB_GROUND - VB_NET_TOP, mesh);
  gfx->drawFastVLine(VB_NET_X + VB_NET_HW, VB_NET_TOP, VB_GROUND - VB_NET_TOP, mesh);
}

static void vbDrawPlayer(int s, uint32_t now) {
  const VbPlayer &q = vb.p[s];
  int x = (int)q.x, g = VB_GROUND - (int)q.y;
  // sombra en el suelo (mas pequena cuanto mas alto)
  int sw = 26 - (int)(q.y / 8);
  if (sw < 10) sw = 10;
  gfx->fillEllipse(x, VB_GROUND + 2, sw, 5, C565(0x40, 0x40, 0x40));
  uint8_t act = PMD_IDLE;
  if (vb.t - q.spikeT < 350 && q.spikeT) act = PMD_ATTACK;
  else if (vb.airborne(s)) act = PMD_HOP;
  else if (q.moving && fabsf(q.targetX - q.x) > 3) act = q.targetX > q.x ? PMD_WALKR : PMD_WALKL;
  if (s == 0) {
    if (pmd.loaded) {
      if (!pmd.has(act)) act = pmd.has(PMD_HOP) && vb.airborne(0) ? PMD_HOP : PMD_IDLE;
      drawPmdAct(act, x, g, now, true, false, 2);
    } else {
      const uint8_t *th = thumbs.get(pet.speciesId);
      if (th) drawThumb(th, x - GAL_CELL / 2, g - GAL_CELL, 2, false);
    }
  } else {
    if (foePmd.loaded) {
      if (!foePmd.has(act)) act = PMD_IDLE;
      drawPmdActM(foePmd, act, x, g, now, true, false, 2, 96);
    } else {
      const uint8_t *th = thumbs.get(vbFoeDex);
      if (th) drawThumb(th, x - GAL_CELL / 2, g - GAL_CELL, 2, false);
    }
  }
}

static void vbDrawScore(uint16_t ink) {
  char sc[16];
  snprintf(sc, sizeof(sc), "%u : %u", vb.score[0], vb.score[1]);
  drawFit(sc, 30, 200, ink, 4);
  gfx->setTextColor(ink);
  setSize(1);
  const char *me = dexName(pet.speciesId), *fo = dexName(vbFoeDex);
  setCur(CX - 70 - textW(me, 1), 82);
  printT(me);
  setCur(CX + 70, 82);
  printT(fo);
}

void renderVolley() {
  uint32_t now = millis();
  uint32_t dt = now - vbLast;
  if (dt > 120) dt = 120;  // tras una pausa larga (pantalla apagada...) no dar un salto
  vbLast = now;
  bool intro = timeLeft(vbIntroUntil) > 0;
  crumb(0x0101);
  if (!intro) vb.step(dt);
  if (vb.state == VB_OVER && !vbOverAt) { vbOverAt = now; vbFinish(); }
  if (vb.score[0] != vbLastScore[0]) sfxPlay(SFX_MEDAL);
  else if (vb.score[1] != vbLastScore[1]) sfxPlay(SFX_DENY);
  vbLastScore[0] = vb.score[0];
  vbLastScore[1] = vb.score[1];

  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  crumb(0x0102);
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, now, night);
  gfx->fillRect(40, VB_GROUND + 1, 386, 3, C565(0xf8, 0xf8, 0xf8));  // linea de la pista
  vbDrawNet();
  crumb(0x0103);
  vbDrawPlayer(0, now);
  crumb(0x0104);
  vbDrawPlayer(1, now);
  crumb(0x0105);
  if (vb.state != VB_OVER) vbDrawBall((int)vb.b.x, (int)vb.b.y, now);
  crumb(0x0106);
  vbDrawScore(ink);

  if (intro) {  // presentacion: VS
    gfx->fillRoundRect(63, 150, 340, 96, 16, UI_WHITE);
    gfx->drawRoundRect(63, 150, 340, 96, 16, UI_INK);
    char vs[64];
    snprintf(vs, sizeof(vs), "%s  VS  %s", dexName(pet.speciesId), dexName(vbFoeDex));
    drawFit(vs, 160, 320, UI_INK, 2);
    drawFit(XT(X_VB_FIRST), 190, 320, C565(0xc8, 0x3c, 0x78), 2);
    drawFit(XT(X_VB_QUIT), 222, 320, 0x8410, 1);
  } else if (vb.state == VB_SERVE && vb.score[0] + vb.score[1] == 0) {  // la ayuda, en el primer saque
    gfx->fillRoundRect(58, 128, 350, 58, 12, UI_WHITE);
    drawFit(XT(X_VB_HINT1), 134, 340, UI_INK, 1);
    drawFit(XT(X_VB_HINT2), 158, 340, UI_INK, 1);
  } else if (vb.state == VB_POINT) {
    bool mine = vb.lastScorer == 0;
    drawFit(XT(mine ? X_VB_POINT : X_VB_LOSTPT), 150, 300, mine ? UI_BAR_OK : UI_BAR_BAD, 3);
  } else if (vb.state == VB_PLAY && vb.b.spiked && vb.t - vb.p[vb.b.spikeSide].spikeT < 600) {
    drawFit(XT(X_VB_SPIKE), 124, 300, C565(0xe8, 0x50, 0x20), 3);
  }

  if (vb.state == VB_OVER) {  // resultado
    bool won = vb.winner == 0;
    gfx->fillRoundRect(58, 110, 350, 250, 20, UI_WHITE);
    gfx->drawRoundRect(58, 110, 350, 250, 20, UI_INK);
    drawFit(XT(won ? X_VB_WIN : X_VB_LOSE), 124, 320, won ? UI_BAR_OK : UI_INK, 4);
    char sc[16];
    snprintf(sc, sizeof(sc), "%u : %u", vb.score[0], vb.score[1]);
    drawFit(sc, 176, 200, UI_INK, 3);
    char l[64];
    snprintf(l, sizeof(l), XT(X_VB_STREAK_FMT), (unsigned)pet.vbStreak, (unsigned)pet.vbBest);
    drawFit(l, 222, 330, UI_INK, 2);
    if (vbRecord) drawFit(pet.lastAllTime ? XT(X_ALL_RECORD) : T(S_NEW_RECORD), 250, 320, UI_BAR_WARN, 2);
    snprintf(l, sizeof(l), XT(X_VB_SPEED_FMT), (unsigned)vbGain);
    drawFit(l, 278, 320, UI_BAR_WARN, 2);
    if (pet.lastTrainExp || pet.lastTrainCandy) {  // como en los entrenamientos
      if (pet.lastTrainCandy) snprintf(l, sizeof(l), XT(X_TRAIN_EXP_CANDY_FMT), (unsigned long)pet.lastTrainExp);
      else snprintf(l, sizeof(l), XT(X_TRAIN_EXP_FMT), (unsigned long)pet.lastTrainExp);
      drawFit(l, 306, 330, UI_BAR_OK, 1);
    }
    if (now - vbOverAt > VBC_RESULT_MIN_MS) drawFit(XT(X_VB_TAP_CLOSE), 332, 300, 0x8410, 1);
  }
  crumb(0x0107);
  gfx->flush();
  crumb(0x0108);
}

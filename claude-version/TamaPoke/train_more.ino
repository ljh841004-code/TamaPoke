// ko12.9.5: dos entrenamientos nuevos (sustituyen a la timing punch y al juego de inclinar de ko12.9.3)
//   - Velocidad: "몬스터볼 찾기". El bicho se mete en una pokeball (rayo rojo, como al guardarlo), las
//     pokeballs se cambian de sitio cada vez mas rapido y hay que tocar la suya. 10 rondas; 3, 4 y luego
//     5 pokeballs. Puntos por lo rapido que se elige (hasta 150 por ronda, 1500 en total: la misma
//     escala que el juego de velocidad de antes, asi el record sigue valiendo).
//   - Juego (animo): "따라 해 봐!". Cuatro botones alrededor del bicho (fuego, agua, planta, electrico);
//     el bicho enseña una secuencia y hay que repetirla. Cada acierto la alarga en uno. 2 vidas: al fallar
//     se repite la misma. Puntos = toques acertados (3 + 4 + 5 ... : la escala del juego de antes).
// El ataque ("번호 과녁") es el juego de dianas numeradas de train.ino.

bool shellOpen = false, simonOpen = false;

// ======================================================================
// velocidad: buscar la pokeball
// ======================================================================
#define SH_ROUNDS 10
#define SH_Y 276          // fila de pokeballs
#define SH_PICK_MS 4000UL // tiempo para elegir
#define SH_FEED_MS 1200UL
enum : uint8_t { SH_SHOW = 0, SH_HIDE, SH_SWAP, SH_PICK, SH_FEED };
static uint8_t shPhase = SH_SHOW, shRound = 0, shN = 3, shPet = 0, shSwapI = 0, shSwapN = 0, shA = 0, shB = 1;
static uint8_t shPick = 255, shHits = 0, shGain = 0;
static uint8_t shRes[SH_ROUNDS];  // 0 pendiente, 1 bien, 2 fallo
static uint16_t shScore = 0, shLastPts = 0;
static uint32_t shT0 = 0, shSwapMs = 400, shRtSum = 0, shOverUntil = 0;
// ko12.9.6: el cambio avanza por FOTOGRAMAS, no solo por tiempo: en la placa un fotograma tarda bastante y
// un cambio de 170 ms se pintaba en 1-2 fotogramas (la pokeball "se teletransportaba"). Ahora cada cambio
// se ve en al menos SH_MIN_FRAMES pasos, y entre cambio y cambio hay una pausa quieta
#define SH_MIN_FRAMES 6
#define SH_PAUSE_MS 90UL  // ko12.9.7: 110 -> 90
static float shP = 0;          // avance del cambio en curso (0..1)
static uint32_t shLastT = 0, shPauseUntil = 0;
static bool shNewHi = false, shOk = false;

static uint8_t shCount(uint8_t r) { return r < 2 ? 3 : r < 6 ? 4 : 5; }  // ko12.9.7: 4 y 5 una ronda antes
static int shGap() { return shN >= 5 ? 82 : shN == 4 ? 96 : 112; }
static int shR() { return shN >= 5 ? 30 : 34; }
static int shSlotX(uint8_t i) { return CX + ((int)i * 2 - (shN - 1)) * shGap() / 2; }

static void shNextSwap() {
  shA = (uint8_t)random(shN);
  if (shRound < 3) {  // al principio solo vecinas (se sigue mejor)
    shB = shA == 0 ? 1 : shA == shN - 1 ? shA - 1 : (random(2) ? shA + 1 : shA - 1);
  } else {
    do shB = (uint8_t)random(shN); while (shB == shA);
  }
  shSwapMs = 520 - shRound * 30;  // ko12.9.7: 520 -> 280 ms (ko12.9.6: 560 -> 320); siempre >= 6 fotogramas
  if (shSwapMs < 280) shSwapMs = 280;
  shP = 0;
}

static void shNewRound(uint32_t now) {
  shN = shCount(shRound);
  shPet = (uint8_t)random(shN);
  shSwapI = 0;
  shSwapN = 4 + (shRound + 1) / 2;  // ko12.9.7: 4 -> 9 cambios (ko12.9.6: 3 -> 8)
  shPick = 255;
  shPhase = SH_SHOW;
  shT0 = now;
}

void startShell() {
  perfReset();
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  shellOpen = true;
  shRound = 0;
  shScore = shHits = 0;
  shRtSum = 0;
  shOverUntil = 0;
  shNewHi = false;
  for (auto &v : shRes) v = 0;
  shNewRound(millis());
}

static void shResolve(bool ok, uint8_t pick, uint32_t now) {
  shOk = ok;
  shPick = pick;
  shRes[shRound] = ok ? 1 : 2;
  shLastPts = 0;
  if (ok) {
    uint32_t rt = now - shT0;
    shLastPts = rt >= 1000 ? 50 : (uint16_t)(150 - rt / 10);
    shScore += shLastPts;
    shHits++;
    shRtSum += rt;
    sfxPlay(SFX_MEDAL);
  } else {
    sfxPlay(SFX_DENY);
  }
  shPhase = SH_FEED;
  shT0 = now;
}

static void stepShell(uint32_t now) {
  uint32_t t = now - shT0;
  if (shPhase == SH_SHOW) {
    if (t >= (shRound == 0 ? 1800UL : 1000UL)) { shPhase = SH_HIDE; shT0 = now; sfxPlay(SFX_TAP); }
  } else if (shPhase == SH_HIDE) {
    if (t >= 450) { shPhase = SH_SWAP; shT0 = shLastT = now; shPauseUntil = 0; shNextSwap(); }
  } else if (shPhase == SH_SWAP) {
    uint32_t dt = now - shLastT;
    shLastT = now;
    if (shPauseUntil) {  // quietas un momento entre dos cambios
      if (!timeLeft(shPauseUntil)) { shPauseUntil = 0; shNextSwap(); }
      return;
    }
    float inc = (float)dt / shSwapMs;
    if (inc > 1.0f / SH_MIN_FRAMES) inc = 1.0f / SH_MIN_FRAMES;  // nunca de golpe
    shP += inc;
    if (shP >= 1) {
      shP = 1;
      if (shPet == shA) shPet = shB;
      else if (shPet == shB) shPet = shA;
      if (++shSwapI >= shSwapN) { shPhase = SH_PICK; shT0 = now; }
      else shPauseUntil = now + SH_PAUSE_MS;
    }
  } else if (shPhase == SH_PICK) {
    if (t >= SH_PICK_MS) shResolve(false, 255, now);  // no eligio
  } else if (shPhase == SH_FEED) {
    if (t >= SH_FEED_MS) {
      if (++shRound >= SH_ROUNDS) {
        shNewHi = shScore > pet.speHi;
        shGain = pet.trainSpeed(shHits + shHits / 2, shScore);  // 10 rondas -> hasta 15 (como las 15 de antes)
        sfxPlay(shNewHi ? SFX_MEDAL : SFX_PLAY);
        shOverUntil = now + 3500;
      } else {
        shNewRound(now);
      }
    }
  }
}

void shellPress(int16_t x, int16_t y) {
  if (shOverUntil || shPhase != SH_PICK) return;
  int best = -1, bd = 52 * 52;
  for (int i = 0; i < shN; i++) {
    int dx = x - shSlotX(i), dy = y - SH_Y, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  if (best < 0) return;  // toque en vacio: no cuenta
  shResolve(best == shPet, (uint8_t)best, millis());
}

// media pokeball (arriba roja / abajo blanca) con su borde
static void halfDisc(int cx, int cy, int r, bool top, uint16_t c) {
  for (int y = 0; y <= r; y++) {
    int w = (int)sqrtf((float)(r * r - y * y));
    gfx->drawFastHLine(cx - w, top ? cy - y : cy + y, 2 * w + 1, c);
  }
}
static void halfRim(int cx, int cy, int r, bool top) {
  for (int a = 0; a <= 180; a += 4) {
    float rad = a * 0.01745f;
    int x = cx + (int)(r * cosf(rad)), y = cy + (int)(r * sinf(rad)) * (top ? -1 : 1);
    gfx->fillCircle(x, y, 1, UI_INK);
  }
}
// abajo (blanca) y tapa (roja); lift = cuanto sube la tapa (0 = cerrada)
static void pokeBallBase(int cx, int cy, int r, bool open) {
  uiShade(cx - r, cy + r - 6, 2 * r, 10, 5, 4);
  if (open) gfx->fillEllipse(cx, cy, r - 2, r / 4 + 1, C565(0x3a, 0x34, 0x40));  // por dentro
  halfDisc(cx, cy, r, false, UI_WHITE);
  halfRim(cx, cy, r, false);
  gfx->fillRect(cx - r, cy - 2, 2 * r + 1, 4, UI_INK);
}
static void pokeBallCap(int cx, int cy, int r, int lift) {
  const uint16_t RED = C565(0xe8, 0x3a, 0x3a);
  int ty = cy - lift;
  halfDisc(cx, ty, r, true, RED);
  halfRim(cx, ty, r, true);
  gfx->fillRect(cx - r, ty - 2, 2 * r + 1, 4, UI_INK);
  gfx->fillCircle(cx - r / 2, ty - r / 2, r / 7 + 1, C565(0xff, 0xa8, 0xa0));  // brillo
  int br = r / 3;
  gfx->fillCircle(cx, ty, br, UI_WHITE);
  gfx->drawCircle(cx, ty, br, UI_INK);
  gfx->drawCircle(cx, ty, br - 1, UI_INK);
}
static void pokeBallV(int cx, int cy, int r, int lift) {
  pokeBallBase(cx, cy, r, lift > 0);
  pokeBallCap(cx, cy, r, lift);
}

static uint8_t petActOr(uint8_t want) { return pmd.has(want) ? want : (uint8_t)PMD_IDLE; }

void renderShell() {
  uint32_t now = millis();
  if (shOverUntil) {
    if (!timeLeft(shOverUntil)) { shellOpen = false; backToTrainMenu(); return; }
    char s[24], g[20], sub[48];
    snprintf(s, sizeof(s), XT(X_SPE_PTS_FMT), shScore);
    snprintf(g, sizeof(g), XT(X_SPE_GAIN_FMT), shGain);
    uint32_t avg = shHits ? shRtSum / shHits : 0;
    snprintf(sub, sizeof(sub), XT(X_SH_SUB_FMT), (unsigned)shHits, (unsigned)SH_ROUNDS, (unsigned)(avg / 1000),
             (unsigned)(avg % 1000 / 10));
    drawTrainResult(s, g, UI_BAR_WARN, shNewHi && shScore > 0, pet.speHi, sub);
    return;
  }
  stepShell(now);
  if (shOverUntil) return;
  uint32_t t = now - shT0;
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  const uint16_t okC = C565(0x4c, 0xc8, 0x5c);
  // tiempo para elegir: aro por el borde (como el ataque)
  if (shPhase == SH_PICK) {
    float f = 1.0f - (float)t / SH_PICK_MS;
    if (f < 0) f = 0;
    uint16_t rc = f > 0.5f ? okC : f > 0.25f ? UI_BAR_WARN : UI_BAR_BAD;
    gfx->fillArc(CX, CX, 232, 222, 0, 360, lerp565(UI_TRACK, UI_INK, 2, 16));
    if (f > 0.01f) gfx->fillArc(CX, CX, 232, 222, 270, 270 + f * 360.0f, rc);
  }
  // puntos y 10 puntitos de progreso
  char b[24];
  snprintf(b, sizeof(b), XT(X_SPE_PTS_FMT), shScore);
  drawFit(b, 34, 220, ink, 3);
  for (int i = 0; i < SH_ROUNDS; i++) {
    int dx = CX - (SH_ROUNDS - 1) * 8 + i * 16, dy = 84;
    if (shRes[i] == 1) gfx->fillCircle(dx, dy, 5, okC);
    else if (shRes[i] == 2) gfx->fillCircle(dx, dy, 5, UI_BAR_BAD);
    else gfx->fillCircle(dx, dy, 4, lerp565(UI_TRACK, UI_INK, 3, 16));
    if (i == shRound) gfx->drawCircle(dx, dy, 7, ink);
  }
  int r = shR();
  // el bicho encima de su pokeball (antes de esconderse)
  if (shPhase == SH_SHOW || (shPhase == SH_HIDE && t < 160)) {
    if (pmd.loaded) drawPmdActM(pmd, petActOr(PMD_HOP), shSlotX(shPet), SH_Y - r + 8, now, true, false, 3, 100);
  }
  // pokeballs (las dos que se cambian van por arcos: una por encima y otra por debajo)
  float p = 0;
  bool swapping = shPhase == SH_SWAP && !shPauseUntil;
  if (swapping) {
    p = shP > 1 ? 1 : shP;
    p = p * p * (3 - 2 * p);
  }
  for (int pass = 0; pass < 2; pass++) {  // 0: las quietas y la de abajo, 1: la de arriba
    for (int i = 0; i < shN; i++) {
      bool moving = swapping && (i == shA || i == shB);
      if ((pass == 1) != (moving && i == shA)) continue;
      int x = shSlotX(i), y = SH_Y, lift = 0;
      if (moving) {
        int xa = shSlotX(shA), xb = shSlotX(shB);
        float s = sinf(p * 3.14159f);
        if (i == shA) { x = xa + (int)((xb - xa) * p); y = SH_Y - (int)(s * 46); }
        else { x = xb + (int)((xa - xb) * p); y = SH_Y + (int)(s * 22); }
        // ko12.9.6: estela (de donde viene), para seguirla con la vista
        int sx = (i == shA ? xb - xa : xa - xb) > 0 ? -1 : 1;
        for (int k = 2; k >= 1; k--)
          gfx->fillCircle(x + sx * k * 14, y + (i == shA ? k * 4 : -k * 2), r - 6 - k * 4,
                          lerp565(C565(0xff, 0xff, 0xff), C565(0xe8, 0x3a, 0x3a), 4 + k * 3, 16));
      }
      if (shPhase == SH_FEED && (i == shPick || i == shPet)) lift = i == shPet ? 96 : 40;
      if (shPhase == SH_HIDE && i == shPet && t > 160) y -= (int)(6 * sinf((t - 160) * 0.06f));  // tiembla
      if (lift && i == shPet) {  // sale de dentro: abajo, el bicho y la tapa por encima de la cabeza
        pokeBallBase(x, y, r, true);
        if (pmd.loaded)
          drawPmdActM(pmd, shOk ? petActOr(PMD_HOP) : petActOr(PMD_HURT), x, y + 6, now, true, false, 3, 80);
        pokeBallCap(x, y, r, lift);
      } else {
        pokeBallV(x, y, r, lift);
      }
    }
  }
  // rayo rojo: el bicho entra en su pokeball
  if (shPhase == SH_HIDE && t >= 120 && t < 420) {
    int x = shSlotX(shPet);
    float q = (t - 120) / 300.0f;
    for (int k = -2; k <= 2; k++) {
      int x0 = x + k * 18, y0 = SH_Y - r - 70 + (int)(q * 60);
      gfx->drawLine(x0, y0, x, SH_Y - 4, C565(0xff, 0x50, 0x50));
      gfx->drawLine(x0 + 1, y0, x + 1, SH_Y - 4, C565(0xff, 0x50, 0x50));
    }
    gfx->fillCircle(x, SH_Y - 4, 8 + (int)(q * 10), C565(0xff, 0xc0, 0xc0));
  }
  const char *msg = nullptr;
  uint16_t mc = ink;
  if (shPhase == SH_SHOW || shPhase == SH_HIDE) msg = XT(shRound == 0 ? X_SH_WATCH : X_SH_FOLLOW);
  else if (shPhase == SH_SWAP) msg = XT(X_SH_FOLLOW);
  else if (shPhase == SH_PICK) msg = XT(X_SH_PICK);
  else if (shOk) { msg = XT(X_SH_FOUND); mc = C565(0x1a, 0x86, 0x34); }
  else { msg = XT(X_SH_WRONG); mc = UI_BAR_BAD; }
  drawFit(msg, 344, 340, mc, 2);
  if (shPhase == SH_FEED && shOk) {
    snprintf(b, sizeof(b), "+%u", (unsigned)shLastPts);
    drawFit(b, 104 - (int)(t / 90), 120, C565(0x1a, 0x86, 0x34), 3);  // encima de la tapa abierta
  }
  uiFlush();
}

// ======================================================================
// juego (animo): "따라 해 봐!" - repetir la secuencia del bicho
// ======================================================================
#define SIM_MAX 32
#define SM_RIN 128        // anillo de botones
#define SM_ROUT 214
#define SM_PET_Y 290      // suelo del bicho (en el centro)
#define SM_IDLE_MS 6000UL // sin tocar = fallo
enum : uint8_t { SM_READY = 0, SM_SHOW, SM_INPUT, SM_GOOD, SM_OOPS };
static uint8_t smPhase = SM_READY, smSeq[SIM_MAX], smLen = 3, smIdx = 0, smLives = 2, smBest = 0, smTapPad = 255;
static uint8_t smSfxIdx = 255;
static uint16_t smScore = 0;
static uint32_t smT0 = 0, smTapT = 0, smOverUntil = 0;
static bool smNewHi = false, smFirst = true;
static const uint16_t SM_COL[4] = { C565(0xe8, 0x48, 0x40), C565(0x40, 0x88, 0xe8), C565(0x48, 0xb8, 0x58),
                                    C565(0xf0, 0xc0, 0x30) };  // arriba fuego, derecha agua, abajo planta, izq. electrico
static const int16_t SM_ANG[4] = { 270, 0, 90, 180 };

// ko12.9.7: cada ronda una secuencia NUEVA (antes la misma alargada en uno: "siempre igual"), con su
// propio generador sembrado al empezar (el momento exacto del toque cambia en cada partida); nunca el
// mismo boton tres veces seguidas
static uint32_t smRng = 1;
static uint8_t smRand4() {
  smRng ^= smRng << 13; smRng ^= smRng >> 17; smRng ^= smRng << 5;
  return (uint8_t)((smRng >> 7) & 3);
}
static void smNewSeq(uint8_t len) {
  for (uint8_t i = 0; i < len && i < SIM_MAX; i++) {
    uint8_t v;
    do v = smRand4(); while (i >= 2 && v == smSeq[i - 1] && v == smSeq[i - 2]);
    smSeq[i] = v;
  }
}

static uint32_t smOnMs() { int v = 560 - smLen * 24; return v < 260 ? 260 : (uint32_t)v; }
#define SM_GAP_MS 170UL

void startSimon() {
  perfReset();
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  simonOpen = true;
  smLen = 3;
  smRng = ((uint32_t)random(0x7fffffff) ^ (micros() * 2654435761u) ^ (smRng * 69069u)) | 1;
  smNewSeq(smLen);
  smIdx = 0;
  smLives = 2;
  smScore = 0;
  smBest = 0;
  smTapPad = 255;
  smOverUntil = 0;
  smNewHi = false;
  smFirst = true;
  smPhase = SM_READY;
  smT0 = millis();
}

static void smEnd(uint32_t now) {
  pet.lastTrainExp = 0;
  pet.lastTrainCandy = 0;
  smNewHi = pet.playResult((uint8_t)(smScore > 255 ? 255 : smScore));
  sfxPlay(smNewHi ? SFX_MEDAL : SFX_LEVEL);
  smOverUntil = now + 4000;
}

static void stepSimon(uint32_t now) {
  uint32_t t = now - smT0;
  if (smPhase == SM_READY) {
    if (t >= (smFirst ? 2200UL : 700UL)) { smFirst = false; smPhase = SM_SHOW; smIdx = 0; smSfxIdx = 255; smT0 = now; }
  } else if (smPhase == SM_SHOW) {
    if (smSfxIdx != smIdx) { smSfxIdx = smIdx; sfxPlay(SFX_PLAY); }
    if (t >= smOnMs() + SM_GAP_MS) {
      smT0 = now;
      if (++smIdx >= smLen) { smPhase = SM_INPUT; smIdx = 0; }
    }
  } else if (smPhase == SM_INPUT) {
    if (t >= SM_IDLE_MS) {  // se quedo quieto
      if (smLives) smLives--;
      smPhase = SM_OOPS; smT0 = now; sfxPlay(SFX_DENY);
    }
  } else if (smPhase == SM_GOOD) {
    if (t >= 800) {
      if (smLen >= SIM_MAX) { smEnd(now); return; }
      smLen++;
      smNewSeq(smLen);  // ko12.9.7: otra secuencia (no la de antes + 1)
      smPhase = SM_READY; smT0 = now;
    }
  } else if (smPhase == SM_OOPS) {
    if (t >= 1200) {
      if (!smLives) { smEnd(now); return; }
      smPhase = SM_READY; smT0 = now;  // la misma secuencia otra vez
    }
  }
}

static int smPadAt(int16_t x, int16_t y) {
  int dx = x - CX, dy = y - CX, d2 = dx * dx + dy * dy;
  if (d2 < (SM_RIN - 16) * (SM_RIN - 16) || d2 > 240 * 240) return -1;
  float a = atan2f((float)dy, (float)dx) * 57.2958f;  // -180..180, 0 = derecha, 90 = abajo
  if (a >= -135 && a < -45) return 0;
  if (a >= -45 && a < 45) return 1;
  if (a >= 45 && a < 135) return 2;
  return 3;
}

void simonPress(int16_t x, int16_t y) {
  if (smOverUntil || smPhase != SM_INPUT) return;
  int p = smPadAt(x, y);
  if (p < 0) return;
  uint32_t now = millis();
  smTapPad = (uint8_t)p;
  smTapT = now;
  smT0 = now;
  if (p == smSeq[smIdx]) {
    smScore++;
    sfxPlay(SFX_TAP);
    if (++smIdx >= smLen) {
      smBest = smLen;
      smPhase = SM_GOOD;
      sfxPlay(SFX_MEDAL);
    }
  } else {
    if (smLives) smLives--;
    smPhase = SM_OOPS;
    sfxPlay(SFX_DENY);
  }
}

// dibujitos de los botones (blancos): llama, gota, hoja, rayo
static void smIcon(int i, int x, int y, uint16_t pad) {
  const uint16_t W = UI_WHITE;
  if (i == 0) {  // llama de tres puntas
    gfx->fillCircle(x, y + 9, 12, W);
    gfx->fillTriangle(x - 12, y + 7, x + 2, y + 7, x - 9, y - 10, W);
    gfx->fillTriangle(x - 7, y + 7, x + 8, y + 7, x + 2, y - 22, W);
    gfx->fillTriangle(x + 1, y + 7, x + 12, y + 7, x + 10, y - 6, W);
    gfx->fillCircle(x, y + 11, 5, pad);
    gfx->fillTriangle(x - 5, y + 10, x + 5, y + 10, x + 1, y - 3, pad);
  } else if (i == 1) {
    gfx->fillCircle(x, y + 6, 12, W);
    gfx->fillTriangle(x - 11, y + 1, x + 11, y + 1, x, y - 20, W);
    gfx->fillCircle(x - 4, y + 6, 3, pad);
  } else if (i == 2) {
    gfx->fillEllipse(x, y, 10, 19, W);
    gfx->drawLine(x, y - 15, x, y + 17, pad);
    gfx->drawLine(x, y - 2, x - 6, y - 9, pad);
    gfx->drawLine(x, y + 6, x + 6, y - 1, pad);
  } else {
    gfx->fillTriangle(x + 5, y - 21, x - 11, y + 4, x + 3, y + 4, W);
    gfx->fillTriangle(x - 3, y - 4, x + 11, y - 4, x - 5, y + 21, W);
  }
}

static void smPads(int lit) {
  gfx->fillArc(CX, CX, SM_ROUT + 10, SM_RIN - 8, 0, 360, C565(0x38, 0x34, 0x48));  // fondo del anillo
  for (int i = 0; i < 4; i++) {
    bool on = i == lit;
    uint16_t c = on ? SM_COL[i] : lerp565(SM_COL[i], C565(0x38, 0x34, 0x48), 8, 16);
    int a0 = SM_ANG[i] - 41, a1;
    if (a0 < 0) a0 += 360;
    a1 = a0 + 82;  // puede pasar de 360 (fillArc lo acepta, como el aro de tiempo)
    if (on) gfx->fillArc(CX, CX, SM_ROUT + 8, SM_RIN - 4, a0, a1, UI_WHITE);
    gfx->fillArc(CX, CX, SM_ROUT, SM_RIN, a0, a1, c);
    float rad = SM_ANG[i] * 0.01745f;
    int mr = (SM_RIN + SM_ROUT) / 2;
    smIcon(i, CX + (int)(mr * cosf(rad)), CX + (int)(mr * sinf(rad)), c);
  }
}

void renderSimon() {
  uint32_t now = millis();
  if (smOverUntil) {
    if (!timeLeft(smOverUntil)) { simonOpen = false; backToTrainMenu(); return; }
    char s[32], sub[40];
    snprintf(s, sizeof(s), XT(X_SM_RES_FMT), (unsigned)smScore);
    snprintf(sub, sizeof(sub), XT(X_SM_SUB_FMT), (unsigned)smBest);
    const char *msg = smNewHi ? XT(X_GAME_REWARD) : smScore ? XT(X_GAME_SMALL) : XT(X_GAME_NO_REWARD);
    drawTrainResult(s, msg, smNewHi ? UI_BAR_OK : UI_INK, smNewHi, pet.gameHi, sub);
    return;
  }
  stepSimon(now);
  if (smOverUntil) return;
  uint32_t t = now - smT0;
  int lit = -1;
  if (smPhase == SM_SHOW && smIdx < smLen && t < smOnMs()) lit = smSeq[smIdx];
  else if (smTapPad < 4 && now - smTapT < 220) lit = smTapPad;
  else if (smPhase == SM_OOPS && smIdx < smLen && (t / 200) % 2 == 0) lit = smSeq[smIdx];  // el que tocaba
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  smPads(lit);
  // centro: puntos, vidas, avance de la secuencia
  char b[24];
  snprintf(b, sizeof(b), XT(X_SM_CNT_FMT), (unsigned)smScore);
  drawFit(b, 136, 160, ink, 2);
  for (int i = 0; i < 2; i++) {
    int x = CX - 34 + i * 36;
    if (i < smLives) drawMap(SPR_HEART, 32, x - 4, 160, 1, false);
    else gfx->drawCircle(x + 12, 174, 9, C565(0x9a, 0x92, 0x88));
  }
  int nd = smLen > 12 ? 12 : smLen;
  for (int i = 0; i < nd; i++) {
    int dx = CX - (nd - 1) * 7 + i * 14, dy = 306;
    bool done = smPhase == SM_INPUT ? i < smIdx : smPhase == SM_SHOW ? i < smIdx : smPhase == SM_GOOD;
    gfx->fillCircle(dx, dy, done ? 5 : 4, done ? C565(0x1a, 0x86, 0x34) : lerp565(UI_TRACK, UI_INK, 3, 16));
  }
  // el bicho mira hacia el boton que enseña
  if (pmd.loaded) {
    int ox = 0, oy = 0;
    uint8_t act = PMD_IDLE;
    if (lit >= 0 && smPhase == SM_SHOW) {
      float rad = SM_ANG[lit] * 0.01745f;
      ox = (int)(12 * cosf(rad));
      oy = (int)(8 * sinf(rad));
      act = petActOr(PMD_ATTACK);
    } else if (smPhase == SM_GOOD) act = petActOr(PMD_HOP);
    else if (smPhase == SM_OOPS) act = petActOr(PMD_HURT);
    drawPmdActM(pmd, act, CX + ox, SM_PET_Y - 8 + oy, now, true, false, 3, 82);
  }
  const char *msg = smPhase == SM_INPUT ? XT(X_SM_YOUR) : smPhase == SM_GOOD ? XT(X_SM_GOOD)
                  : smPhase == SM_OOPS ? XT(X_SM_OOPS) : XT(X_SM_WATCH);
  uint16_t mc = smPhase == SM_GOOD ? C565(0x1a, 0x86, 0x34) : smPhase == SM_OOPS ? UI_BAR_BAD : ink;
  drawFit(msg, 318, 170, mc, 2);
  if (smPhase == SM_READY && smFirst) {  // como se juega (la primera vez)
    uiPanel(43, 206, 380, 56, 16, UI_WHITE, UI_INK);
    drawFit(XT(X_SM_HOW), 222, 360, UI_INK, 2);
  }
  uiFlush();
}

// ---- para las pruebas (test/render) ----
uint8_t shellPetProbe() { return shPet; }
uint8_t shellPhaseProbe() { return shPhase; }
uint8_t shellCountProbe() { return shN; }
int shellSlotXProbe(uint8_t i) { return shSlotX(i); }
uint16_t shellScoreProbe() { return shScore; }
bool shellOverProbe() { return shOverUntil != 0; }
uint8_t simonPhaseProbe() { return smPhase; }
uint8_t simonNextPadProbe() { return smIdx < smLen ? smSeq[smIdx] : 255; }
uint8_t simonLenProbe() { return smLen; }
uint8_t simonLivesProbe() { return smLives; }
uint16_t simonScoreProbe() { return smScore; }
bool simonOverProbe() { return smOverUntil != 0; }
float shellSwapProbe() { return shP; }
void simonSeqProbe(uint8_t *out, uint8_t n) { for (uint8_t i = 0; i < n && i < SIM_MAX; i++) out[i] = smSeq[i]; }

// test/render: dibuja pantallas del firmware en el PC y las guarda como .raw
// (RGB565 466x466). run.sh las convierte a PNG. Usa el MISMO codigo de dibujo
// que la placa (Arduino_GFX real); solo el hardware es de mentira (stubs.cpp).
#include "build/sketch.cpp"
#include <sys/stat.h>

extern uint32_t gMockMillis, gMockEpoch;
extern bool gMockPortal;
extern const char *gSdRoot;

static void shot(const char *name) {
  char path[256];
  snprintf(path, sizeof(path), "build/shots/%s.raw", name);
  FILE *f = fopen(path, "wb");
  fwrite(gfx->getFramebuffer(), 2, LCD_WIDTH * LCD_HEIGHT, f);
  fclose(f);
  printf("  %s\n", name);
}

static void tick(uint32_t ms) { gMockMillis += ms; }

static void closeAll() {
  cardOpen = trainMenuOpen = galleryOpen = clockOpen = false;
  defOpen = spdOpen = sackOpen = gameOpen = vbOpen = false;
  xScreen = XS_NONE;
  toastUntil = 0;
  feedMenuUntil = 0;
}

static void scenes(bool ko, const char *sfx) {
  char n[64];
  setLang(ko ? LANG_KO : LANG_EN);
  applyLangFont();
  if (ko) printf("  (ascenso unifont: %d)\n", gFontAscent);
  closeAll();
  // principal
  gMockEpoch = 1790343900;  // 13:45
  pet.lastSeenEpoch = gMockEpoch;
  render(); snprintf(n, sizeof(n), "01_main%s", sfx); shot(n);
  if (!ko) return;
  gMockEpoch = 1790343900 + 8 * 3600 + 17 * 60;  // 22:02: noche
  pet.lastSeenEpoch = gMockEpoch;
  render(); shot("02_main_night");
  gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
  // ko10.8: comportamientos de la pantalla principal
  {  // ko11.7: aviso de premio de la pokedex
    char t[80];
    snprintf(t, sizeof(t), XT(X_DEXRW_FMT), 50, XT(X_DEXRW_2));
    showToast(t); render(); shot("01b_main_dex_reward"); toastUntil = 0;
  }
  {
    uint8_t j0 = pet.joy, h0 = pet.hygiene;
    pet.joy = 90; pet.hygiene = 90;  // contento (si no, sale la cara triste)
    uint8_t m0 = pet.careMistakes; pet.careMistakes = 90;  // sin boton de evolucionar encima
    beh.mode = 3; beh.act = PMD_ATTACK; beh.t0 = gMockMillis - 1300; beh.until = gMockMillis + 5000;
    beh.bubble = 0;
    render(); shot("60_beh_skill");
    beh.mode = 2; beh.act = pmd.has(PMD_ROTATE) ? PMD_ROTATE : PMD_BREATH; beh.t0 = gMockMillis - 300;
    beh.until = gMockMillis + 5000; beh.bubble = BUB_BORED; beh.bubbleUntil = gMockMillis + 5000;
    render(); shot("61_beh_bored");
    beh.act = PMD_POSE; beh.bubble = BUB_NOTE;
    render(); shot("62_beh_happy");
    beh.act = PMD_BREATH; beh.bubble = BUB_HUNGRY;
    render(); shot("63_beh_hungry");
    beh.act = PMD_HURT; beh.bubble = BUB_RAIN;
    render(); shot("64_beh_rain");
    beh.act = pmd.has(PMD_LAYING) ? PMD_LAYING : PMD_BREATH; beh.bubble = BUB_SUN;
    render(); shot("65_beh_sun");
    beh.mode = 0; beh.until = 0; beh.bubble = 0; pet.joy = j0; pet.hygiene = h0; pet.careMistakes = m0;
  }
  // ficha: combate
  cardOpen = true; cardPage = 1;
  render(); shot("03_card_battle");
  pet.exp = expForLevel(17) + 400;  // fork KO (ko7): barra de EXP
  cardPage = 3;
  render(); shot("03b_card_progress");
  {  // ko11.7: evolucion por amistad (Pichu con poco vinculo)
    int16_t sp = pet.speciesId; uint32_t ex = pet.exp; uint8_t bd = pet.bond;
    pet.speciesId = 172; pet.exp = expForLevel(30); pet.bond = 42; ensureMon();
    render(); shot("03l_card_friend_evo");
    pet.speciesId = sp; pet.exp = ex; pet.bond = bd; ensureMon();
  }
  pet.careMistakes = 2; render(); shot("03g_card_mistakes");  // ko10.6: cuanto falta para perdonar uno
  pet.mistWhy = MW_FOOD; pet.mistEpoch = 1790343900 - 3600; mistWhyUntil = gMockMillis + 5000;
  render(); shot("03h_card_mistake_why");  // ko10.9: causa al tocar
  mistWhyUntil = 0;
  pet.careMistakes = 0;
  pet.berryKnown = true; pet.ageMinutes = 2 * 1440 + 300; pet.bond = 46; pet.streak = 3; pet.bestStreak = 5;
  cardPage = 0; render(); shot("03c_card_profile");
  strcpy(pet.nick, "불꽃이"); render(); shot("03e_card_profile_nick"); pet.nick[0] = 0;
  cardPage = 2; render(); shot("03d_card_medals");
  pet.addCandy(pet.speciesId, 12); cardPage = 4; render(); shot("03f_card_candy");
  pet.careMistakes = 1; pet.addCandy(pet.speciesId, 10); render(); shot("03i_card_candy_mistake"); pet.careMistakes = 0;
  // ko10.11: bolsa de caramelos
  pet.addCandy(25, 7); pet.addCandy(133, 4); pet.addCandy(6, 2); pet.addCandy(92, 11); pet.rareCandy = 2;
  closeAll(); openCandyBag(); render(); shot("03j_candy_bag");
  candyBagTap(BAG_ROW_X + 100, BAG_ROW_Y + 1 * (BAG_ROW_H + BAG_GAP) + 10); render(); shot("03k_candy_bag_trade");
  candyBagTap(0, 0); candyBagTap(BAG_ROW_X + 100, BAG_RARE_Y + 10); render(); shot("03l_candy_bag_rare");
  closeAll(); pet.rareCandy = 0; cardOpen = true; cardPage = 4; render(); shot("03m_card_candy_bagbtn"); closeAll();
  closeAll(); pet.berryKnown = true; feedMenuUntil = gMockMillis + 5000; render(); shot("01b_feed_menu");
  closeAll(); confirmUntil = gMockMillis + 5000; render(); shot("01c_confirm_release"); confirmUntil = 0;
  {  // ko11.9.2: mantener sobre el bicho: circulo que se llena (con un corte del tactil en medio)
    closeAll(); lastInteract = gMockMillis;
    touchSample(true, 233, 220);
    for (int i = 0; i < 40; i++) { tick(20); touchSample(true, 234, 221); }
    touchSample(false, 234, 221); tick(120);           // el tactil "suelta" 120 ms
    touchSample(true, 235, 221);
    for (int i = 0; i < 20; i++) { tick(20); touchSample(true, 235, 222); }
    render(); shot("01d_hold_ring");
    printf("  hold: progreso %.2f, dialogo %d\n", petHoldProgress(), confirmUntil ? 1 : 0);
    for (int i = 0; i < 90; i++) { tick(20); touchSample(true, 235, 222); }
    render(); shot("01e_hold_dialog");
    printf("  hold: dialogo %d\n", confirmUntil ? 1 : 0);
    touchSample(false, 235, 222); tick(200);
    confirmUntil = 0; wasPressed = false;
  }
  closeAll(); openLinkMenu(); render(); shot("24_link_menu");
  closeAll(); cardOpen = true; cardPage = 0; openKeyboard(); nameBuf[0] = 0; nameLen = 0;
  for (uint8_t k : { CJI_K_B, CJI_K_B, CJI_K_I, CJI_K_DOT, CJI_K_O, CJI_K_I, CJI_K_N, CJI_K_N })
    cjiPress(kbCji, k);
  render(); shot("27_keyboard_ko");
  kbCommit(); kbKo = false; render(); shot("28_keyboard_abc");
  kbOpen = false;
  // ko11.14: carga y golpe: unos criticos (tocar en el pico del medidor)
  // ko11.15: golpes -> energia llena (boton de la tecnica) y la tecnica en curso
  closeAll(); startSack(); tick(800);
  for (int i = 0; i < 22; i++) { sackTap(CX, 150); tick(70); }
  render(); shot("25_sack");
  sackTap(CX, SACK_BTN_Y + 20); tick(260); render(); shot("25c_sack_move");
  for (int i = 0; i < 80; i++) { tick(85); render(); }   // se acaba el plazo
  perfFrames = 120; perfRenderSum = 120 * 58; perfRenderMax = 71; perfStallMax = 12;  // ko11.3: linea de medida
  render(); shot("25b_sack_result");
  perfReset();
  closeAll(); startGame(); tick(300); render(); shot("26_ball_game");
  // ko10.4: 3 pelotas cayendo a la vez (un rato despues de empezar)
  for (int f = 0; f < 36; f++) { tick(85); render(); }
  shot("26b_ball_game_3balls");
  gameOpen = false;
  closeAll();
  // entrenamiento
  closeAll(); openTrainMenu();
  pet.allStrHi = 12; pet.allDefHi = 31; pet.allSpeHi = 1180; pet.allGameHi = 22; pet.allVbBest = 14;  // ko11.9.2
  pet.vbStreak = 12; pet.vbBest = 13;  // ko11.9.4: el texto mas largo que cabe
  render(); shot("04_train_menu");
  trainMenuPage = 1; render(); shot("04b_train_menu_battle"); trainMenuPage = 0;
  // ko11.9.4: material para comparar graficos (Ivysaur): pantalla actual, fondo sin
  // bicho (dos mitades), escena sola y el sprite en crudo sobre magenta
  if (ko) {
    int16_t keepSp = pet.speciesId;
    float keepX = beh.x, keepT = beh.targetX;
    closeAll();
    pet.speciesId = 2;
    pmd.load(2, false);
    beh.mode = 0; beh.x = beh.targetX = 233;
    render(); shot("hq_main_cur");
    beh.x = beh.targetX = 40; render(); shot("hq_main_petL");
    beh.x = beh.targetX = 426; render(); shot("hq_main_petR");
    beh.x = beh.targetX = 233;
    bool night = sceneHour() < 6 || sceneHour() >= 20;
    gfx->fillScreen(0);
    drawScene(DEX_TBL[2].biome, gMockMillis, night);
    gfx->flush(); shot("hq_scene");
    gfx->fillScreen(0xF81F);
    drawPmdAct(PMD_IDLE, 233, 400, 0, true, false, 2);
    gfx->flush(); shot("hq_sprite2x");
    printf("  hq: idle h=%u w=%u base=%u\n", pmd.acts[PMD_IDLE].h, pmd.acts[PMD_IDLE].w, pmd.acts[PMD_IDLE].base);
    pet.speciesId = keepSp;
    pmd.load((uint8_t)keepSp, pet.shiny);
    beh.x = keepX; beh.targetX = keepT;
  }
  // ko11.9: voleibol, el que se esta criando (Ivysaur) contra uno al azar (Psyduck)
  if (ko) {
    int16_t keepSp = pet.speciesId;
    pet.speciesId = 2;
    pmd.load(2, false);
    closeAll(); randomSeed(11); startVolley();
    vbFoeDex = 54; loadFoe(54, false);
    tick(300); render(); shot("70_volley_vs");
    tick(1500); render(); shot("71_volley_first_serve");
    // ko11.9.1: el nuestro corre solo; aqui solo se "toca" para saltar cuando la pelota baja cerca
    bool gotSpike = false, gotPoint = false;
    for (int i = 0; i < 20000 && vb.state != VB_OVER; i++) {
      if (vb.state == VB_PLAY && !vb.airborne(0) && vb.b.vy > 0 && vb.b.x < VB_NET_X &&
          fabsf(vb.b.x - vb.p[0].x) < 50 && vb.bodyY(0) - vb.b.y < 170) vbPress(233, 250);
      tick(20); render();
      if (!gotSpike && vb.state == VB_PLAY && vb.b.spiked && vb.b.spikeSide == 0 && vb.t - vb.p[0].spikeT > 60) {
        shot("72_volley_spike"); gotSpike = true;
        vb.b.power = true; render(); shot("72b_volley_power"); vb.b.power = false;  // ko11.9.4
      }
      if (!gotPoint && vb.state == VB_POINT && vb.stateT > 200) { shot("73_volley_point"); gotPoint = true; }
    }
    tick(VBC_RESULT_MIN_MS + 100); render(); shot("74_volley_result");
    closeAll();
    pet.speciesId = keepSp;
    pmd.load((uint8_t)keepSp, pet.shiny);
  }
  // ko11.14: defensa por timing: el balon bajando hacia la franja, un PERFECTO con combo
  closeAll(); startDefense();
  tick(3000); render();
  defHits = 6; defScore = 11; defCombo = 3; defPerfectN = 5; defGoodN = 1; defMissN = 1;
  defV = 260; defDropT = gMockMillis - (uint32_t)((DEF_ZONE_Y - 70 - DEF_Y0) * 1000 / defV);
  render(); shot("05_train_defense");
  tick((uint32_t)(70 * 1000 / defV)); defensePress(CX, 300);
  tick(120); render(); shot("05b_train_defense_perfect");
  defDropT = gMockMillis - (uint32_t)((DEF_ZONE_Y - 50 - DEF_Y0) * 1000 / defV); defensePress(CX, 300);
  tick(120); render(); shot("05c_train_defense_early");
  defScore = 34; defPerfectN = 14; defGoodN = 6; defMissN = DEF_LIVES; render(); tick(85); render(); shot("06_train_defense_result");
  closeAll(); startSpeed();
  for (int i = 0; i < 400 && spdPhase != SP_SHOW; i++) { tick(20); render(); }
  tick(300); speedPress(spdBx[0], spdBy[0]);
  tick(200); render(); shot("07_train_speed");
  // ko10.6: resultado con puntos por reflejos y la media
  for (int r = 0; r < SPD_ROUNDS; r++) {
    for (int i = 0; i < 400 && spdPhase != SP_SHOW; i++) { tick(20); render(); }
    for (int k = 0; k < spdN; k++) { tick(300 + r * 5); speedPress(spdBx[k], spdBy[k]); }
    for (int i = 0; i < 60 && spdPhase == SP_FEED && !spdOverUntil; i++) { tick(20); render(); }
  }
  tick(20); render(); shot("07b_train_speed_result");
  // ko11.9.3: al acabar el resultado se vuelve al menu de entrenamiento
  tick(6000); render(); render(); shot("07c_after_result_menu");
  printf("  tras el resultado: menu=%d spd=%d\n", trainMenuOpen ? 1 : 0, spdOpen ? 1 : 0);
  // batalla salvaje
  closeAll(); pet.energy = 80; startWild();
  bPhase = BP_MENU; txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
  render(); shot("08_battle_menu");
  box.add(bFoe.dex, 5, false, true, 0); box.add(bFoe.dex, 7, false, true, 0); bvOwned = 2; bvOwnedT = gMockMillis;
  render(); shot("08b_battle_owned"); bvOwned = 0;
  // ko11.9.1: rival de la 2a generacion (Totodile) con el nombre en dorado
  if (ko) {
    Battler keepFoe = bFoe;
    bFoe = makeBattler(158, 12, 60, 60, 60);
    bvSetup(bMe, bFoe, nullptr, false);
    bPhase = BP_MENU; txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
    render(); shot("08c_battle_gen2_name");
    bFoe = keepFoe;
    bvSetup(bMe, bFoe, nullptr, false);
    bPhase = BP_MENU; txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
  }
  // fork KO (ko7): efectos de cada tipo (viaje y impacto) + critico/muy eficaz
  for (int ty = -1; ty < PT_COUNT; ty++) {
    bPhase = BP_PLAY; bqAisMe = true; bqN = 1; bqI = 0;
    memset(&bq[0], 0, sizeof(bq[0]));
    bq[0].side = 0; bq[0].kind = EV_HIT; bq[0].move = ty < 0 ? BA_TACKLE : BA_TYPE;
    bq[0].eff = 2; bq[0].dmg = 5; bq[0].hpA = bMe.hp; bq[0].hpB = bFoe.hp;
    if (ty >= 0) bvMeType = (uint8_t)ty;
    bqT = gMockMillis;
    for (uint32_t at : { 260u, 520u, 760u }) {
      gMockMillis = bqT + at;
      render();
      snprintf(n, sizeof(n), "fx_%02d_%u", ty + 1, at);
      shot(n);
    }
  }
  // ko10.4: cada linea evolutiva con su propio ataque en sus tres fases
  {
    int16_t keepSp = pet.speciesId;
    static const int16_t LINES[4][3] = { { 7, 8, 9 }, { 4, 5, 6 }, { 172, 25, 26 }, { 1, 2, 3 } };
    for (auto &line : LINES) {
      for (int tier = 0; tier < 3; tier++) {
        int16_t d = line[tier];
        pet.speciesId = d;
        pmd.load((uint8_t)d, false);
        bMe = makeBattler(d, 30, 60, 60, 60);
        bvSetup(bMe, bFoe, nullptr, false);
        bPhase = BP_PLAY; bqAisMe = true; bqN = 1; bqI = 0;
        memset(&bq[0], 0, sizeof(bq[0]));
        bq[0].side = 0; bq[0].kind = EV_HIT; bq[0].move = BA_TYPE;
        bq[0].eff = 2; bq[0].dmg = 5; bq[0].hpA = bMe.hp; bq[0].hpB = bFoe.hp;
        txFmt(bvL1, sizeof(bvL1), X_USED, bvMeName, moveName(BA_TYPE, bvMeType, bvMeTier));
        bvL2[0] = 0;
        bqT = gMockMillis;
        for (uint32_t at : { 300u, 520u }) {
          gMockMillis = bqT + at;
          render();
          snprintf(n, sizeof(n), "tier_%03d_%u", d, at);
          shot(n);
        }
      }
    }
    pet.speciesId = keepSp;
    pmd.load((uint8_t)keepSp, pet.shiny);
    bMe = makeBattler(keepSp, pet.level(), pet.atkStat(), pet.defStat(), pet.speStat());
    bvSetup(bMe, bFoe, nullptr, false);
  }
  bq[0].crit = true; bq[0].eff = 4; bq[0].move = BA_TACKLE; bqT = gMockMillis;
  gMockMillis = bqT + 480; render(); shot("fx_crit_super");
  // ko11.8: se protege, le pegan y devuelve el golpe (el mio y el del rival)
  for (int who = 0; who < 2; who++) {
    bPhase = BP_PLAY; bqAisMe = true; bqN = 3; bqI = 0;
    memset(bq, 0, sizeof(BEvent) * 3);
    uint8_t g = who ? 1 : 0;  // quien se protege
    bq[0].side = g; bq[0].kind = EV_GUARD; bq[0].move = BA_GUARD; bq[0].eff = 2;
    bq[1].side = g ^ 1; bq[1].kind = EV_HIT; bq[1].move = BA_TACKLE; bq[1].eff = 2; bq[1].dmg = 6;
    bq[2].side = g; bq[2].kind = EV_COUNTER; bq[2].move = BA_TACKLE; bq[2].eff = 2; bq[2].dmg = 4;
    for (int i = 0; i < 3; i++) { bq[i].hpA = bMe.hp; bq[i].hpB = bFoe.hp; }
    bqI = 2; bqT = gMockMillis; evMessages(bq[2]);
    gMockMillis = bqT + 420; render(); shot(who ? "21c_counter_foe" : "21b_counter_me");
  }
  bPhase = BP_MENU; bqN = 0;
  {  // ko11.1: victoria salvaje con objetos por probabilidad
    uint8_t b0 = pet.balls, p0 = pet.potions;
    mockForceRandom(25);
    finishBattle(true, false, false);
    mockClearForcedRandom();
    render(); shot("09a_battle_win_items");
    pet.balls = b0; pet.potions = p0;
    bPhase = BP_MENU; bqN = 0; bRewarded = false; bWon = false; bItems = 0; bNote[0] = 0; bBoxMsg = -1;
  }
  bvFoeHp = bvFoeTgt = bFoe.hp = bFoe.maxHp / 3;
  finishBattle(false, false, true);
  bvFoeCaught = true;
  render(); shot("09_battle_caught");
  bPhase = BP_NEXT; bPhaseT = gMockMillis; render(); shot("09b_battle_next");
  // ko10.4: repetido capturado -> caja o caramelos; luego "seguir?" con lo ganado
  pet.addCandy(bFoe.dex, 4);
  bDupPending = true; bDupCaught = true; bPhase = BP_DUP; bPhaseT = gMockMillis;
  render(); shot("09c_battle_dup");
  dupDecide(false); render(); shot("09d_battle_dup_candy");
  // ko11.8: el vencido quiere venir -> preguntar
  bJoinPending = true; bPhase = BP_JOIN; bPhaseT = gMockMillis;
  render(); shot("09e_battle_join");
  joinDecide(false); render(); shot("09f_battle_join_bye");
  // ko10.1: escenario de la batalla salvaje = habitat del rival; tiempo por fecha
  {
    auto rep = [](int bio) -> int16_t {
      for (int16_t d = 1; d <= DEX_COUNT; d++)
        if (DEX_TBL[d].biome == bio && !(d >= 138 && d <= 141)) return d;
      return 1;
    };
    bPhase = BP_MENU; bvFoeCaught = false; bvFoeFainted = false;
    for (int bio : { 6, 12, 13, 14 }) {
      bvFoeDex = rep(bio);
      render(); snprintf(n, sizeof(n), "40_battle_bio%02d", bio); shot(n);
    }
    struct { uint32_t e; const char *tag; } WX[] = {
      { 1772356800u, "rain" }, { 1767273300u, "snow" }, { 1780304400u, "sunny" },
      { 1772488800u, "rain_night" }, { 1772356200u, "blossom" }, { 1788253500u, "leaves" },
    };
    for (auto &w : WX) {
      gMockEpoch = w.e; pet.lastSeenEpoch = w.e;
      render(); snprintf(n, sizeof(n), "41_battle_%s", w.tag); shot(n);
    }
    closeAll();
    int16_t keep = pet.speciesId;
    gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
    for (int bio = 0; bio < 16; bio++) {
      pet.speciesId = rep(bio);
      render(); snprintf(n, sizeof(n), "42_main_bio%02d", bio); shot(n);
    }
    pet.speciesId = rep(0);
    for (auto &w : WX) {
      gMockEpoch = w.e; pet.lastSeenEpoch = w.e;
      render(); snprintf(n, sizeof(n), "43_main_%s", w.tag); shot(n);
    }
    pet.speciesId = keep;
    gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
  }
  // ko10.4: gimnasios, reto del dia, tiempo en batalla
  {
    closeAll(); pet.energy = 80;
    pet.badges = 0x07;  // 3 medallas
    openGyms(); gymPage = 0; render(); shot("50_gyms_p1");
    gymPage = 1; render(); shot("50b_gyms_p2");
    closeAll(); openDaily(); render(); shot("51_daily");
    closeAll(); trainMenuOpen = true; trainMenuPage = 1; render(); shot("52_battle_page"); trainMenuOpen = false;
    closeAll(); openRegionPick(); regionPage = 1; render(); shot("53_region_locked");
    // gimnasio de Lt. Surge con lluvia: presentacion y resultado con medalla
    closeAll();
    gMockEpoch = 1772356800u; pet.lastSeenEpoch = gMockEpoch;  // lluvia
    pet.badges = 0x03;
    gymPage = 0; gymTap(GY_X + 10, GY_Y + 2 * (GY_H + GY_GAP) + 10);  // 3er gimnasio
    render(); shot("54_gym_intro");
    bPhase = BP_MENU; bvL1[0] = 0;
    bFoe.hp = 0; bvFoeTgt = 0;
    nextTrainerMon(); render(); shot("55_gym_next");
    bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
    finishBattle(true, false, false); render(); shot("56_gym_badge");
    // ko10.11: revanchas y liga
    closeAll();
    gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
    pet.badges = 0xFF;
    pet.gymWins[0] = 3; pet.gymWins[1] = 1; pet.gymDay[1] = (uint16_t)(gMockEpoch / 86400u);
    openGyms(); gymPage = 0; render(); shot("57_gyms_rematch");
    gymPage = 2; render(); shot("58_league_empty");
    fame.add(6, 58, false, false, gMockEpoch - 86400 * 3);
    fame.add(134, 61, true, false, gMockEpoch - 86400);
    fame.add(pet.speciesId, pet.level(), pet.shiny, false, gMockEpoch);
    pet.champWins = 3; pet.champStreak = 2; pet.champBest = 2;
    pet.fameStreak[1] = 1; pet.fameStreak[2] = 2;  // ko11.6.1: rachas
    render(); shot("59_league_fame");
    openFame(); render(); shot("59d_fame_grid");
    fameTap(FM_X + FM_CELL + 10, FM_Y + 10); render(); shot("59e_fame_detail");
    fameClose(); fameClose();
    leagueTap(GY_X + 40, LG_BTN_Y + 10); render(); shot("59b_league_intro");
    bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
    finishBattle(true, false, false); render(); shot("59c_league_win");
    closeAll();
    gymPage = 0; gymTap(GY_X + 10, GY_Y + 1 * (GY_H + GY_GAP) + 10);  // revancha de Misty
    render(); shot("57b_rematch_intro");
    closeAll();
    fame.wipe(); pet.champWins = 0;
    memset(pet.gymWins, 0, sizeof(pet.gymWins)); memset(pet.gymDay, 0, sizeof(pet.gymDay));
    pet.badges = 0;
    gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
  }
  // ko10.1: elegir region y encuentros
  {
    closeAll(); pet.energy = 80;
    openRegionPick(); regionPage = 0; render(); shot("44_region_pick"); regionPage = 1; render(); shot("44b_region_pick2");
    {  // ko11.7: evento del dia bajo el titulo
      uint32_t keep = gMockEpoch; bool kt = gClockTrusted;
      gClockTrusted = true; regionPage = 0;
      gMockEpoch = 1790589600u; pet.lastSeenEpoch = gMockEpoch; gMockMillis += 1100; render(); shot("44c_region_event");
      gMockEpoch = 1790463600u; pet.lastSeenEpoch = gMockEpoch; gMockMillis += 1100; render(); shot("44d_region_moon");
      gMockEpoch = keep; pet.lastSeenEpoch = keep; gClockTrusted = kt; gMockMillis += 1100;
    }
    gMockEpoch = 1772356800u; pet.lastSeenEpoch = gMockEpoch;  // lluvia (marzo)
    startWildIn(6);
    foePmd.unload();
    bFoe = makeBattler(25, 18, 30, 30, 30); bGroup = WG_REGION;
    bvSetup(bMe, bFoe, nullptr, false);
    txFmt(bvL1, sizeof(bvL1), X_WILD_AT, XT(X_REG_6), dexName(25));
    bPhase = BP_INTRO; bPhaseT = gMockMillis;
    render(); shot("45_wild_power_plant_pikachu");
    foePmd.unload();
    bFoe = makeBattler(243, 45, 90, 80, 110); bGroup = WG_RARE;
    bvSetup(bMe, bFoe, nullptr, false);
    txFmt(bvL1, sizeof(bvL1), X_WILD_RARE, XT(X_REG_6), dexName(243));
    bPhase = BP_INTRO; bPhaseT = gMockMillis;
    render(); shot("46_wild_rare_raikou");
    gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
    startWildIn(13);
    foePmd.unload();
    bFoe = makeBattler(147, 20, 40, 40, 40); bGroup = WG_REGION;
    bvSetup(bMe, bFoe, nullptr, false);
    txFmt(bvL1, sizeof(bvL1), X_WILD_AT, XT(X_REG_13), dexName(147));
    bPhase = BP_INTRO; bPhaseT = gMockMillis;
    render(); shot("47_wild_dragon_vale");
    closeAll();
  }
  // caja
  closeAll();
  box.add(16, 14, false, false, gMockEpoch - 86400);
  box.add(129, 9, true, true, gMockEpoch - 3600);
  box.add(143, 22, false, true, gMockEpoch);
  box.add(16, 18, false, true, gMockEpoch);   // ko10.4: repetidos (x2 Pidgey)
  openBox(); render(); shot("10_box");
  boxSel = 1; render(); shot("11_box_detail");
  {  // ko11.15: al soltar un capturado: aviso visible tambien en la caja
    char t[72];
    snprintf(t, sizeof(t), XT(X_EXP_CANDY_FMT), dexName(12), 1u);
    strncat(t, "  ", sizeof(t) - strlen(t) - 1); strncat(t, XT(X_EXP_RARE), sizeof(t) - strlen(t) - 1);
    showToast(t); boxSel = -1; render(); shot("11i_box_release_toast"); toastUntil = 0;
  }
  // ko11.7: expedicion
  expPick = true; render(); shot("11e_exp_pick"); expPick = false;
  expSend(1, 4); boxSel = -1; render(); shot("11f_exp_away");
  pet.exped.end = gMockEpoch; render(); shot("11g_exp_back");
  expCollect(); render(); shot("11h_exp_result"); expResOpen = false;
  // ko10.5: fin de un ciclo -> criado a la caja (corona) y eleccion del siguiente
  boxSel = -1;
  hall.addRaised(6, 36, false, 108, 101, 99, gMockEpoch);
  hall.addRaised(134, 42, true, 104, 99, 107, gMockEpoch);
  pet.markFamRaised(16);  // Pidgey ya criado: en gris
  pet.markFamRaised(6);
  render(); shot("11a_box_tabs");
  boxHall = true; render(); shot("11b_box_hall"); boxHall = false;
  xScreen = XS_NEXTPICK; nextPage = 0; render(); shot("11c_next_pick");
  xScreen = XS_BOX;
  // pokedex
  closeAll();
  for (int d : { 16, 19, 25, 129, 133, 143 }) dexLog.seen(d, gMockEpoch - 86400 * 3);
  dexLog.caught(25, gMockEpoch);
  galleryOpen = true; galleryPage = 0; galleryDetail = 0; galleryDirty = true;
  render(); shot("12_dex_grid");
  galleryDetail = 25; galleryPmd.load(25, false);
  render(); shot("13_dex_detail");
  galleryDetail = 52; galleryPmd.load(52, false);
  render(); shot("14_dex_unknown");
  // ko10: gen 2 en la pokedex
  galleryDetail = 0; galleryPage = 10; galleryDirty = true;
  for (int16_t d : { 172, 175, 176, 179, 181, 196, 197, 208, 212 }) dexLog.seen(d, gMockEpoch);
  render(); shot("30_dex_gen2_grid");
  galleryPage = 9; galleryDirty = true; render(); shot("31b_dex_gen1_last");
  galleryPage = GAL_PAGES - 1; galleryDirty = true; render(); shot("31_dex_gen2_last");
  galleryDetail = 197; galleryPmd.load(197, false); render(); shot("32_dex_umbreon");
  galleryDetail = 0; galleryPage = 0;
  // sonido y hora
  closeAll(); openClock(); render(); shot("15_clock_settings");
  clockDateMode = true; render(); shot("15b_clock_date"); clockDateMode = false;
  closeAll(); openSound(); render(); shot("16_sound");
  closeAll(); openBgmPick(); render(); shot("16b_bgm_pick");  // ko11.8
  audioSetBgmMask(0x02); render(); shot("16c_bgm_pick_one"); audioSetBgmMask(0xFF);
  closeAll(); openNet(); render(); shot("18_net");
  { gMockPortal = true; render(); shot("18b_portal_qr"); gMockPortal = false; }
  // ko11.6: copia de la partida en la SD
  openBackup(); render(); shot("18c_backup");
  bakSel = 1; render(); shot("18d_backup_confirm"); bakSel = -1;
  // ko11.9.2: ultimo reinicio inesperado guardado: boton + ventana
  crashCount = 3; crashReason = 4; crashWhere = 0x0103; crashEpoch = gMockEpoch;
  crashPcN = 3; crashPc[0] = 0x4201a2b4; crashPc[1] = 0x42019f10; crashPc[2] = 0x4200e3c8;
  render(); shot("18f_backup_crash");
  backupTap(BAK_CR_X + 20, BAK_CR_Y + 10); render(); shot("18g_backup_crash_view");
  {  // el texto mas largo de la placa (en el PC resetName dice "other")
    char l[96];
    snprintf(l, sizeof(l), XT(X_CRASH_WHY_FMT), "BROWNOUT (power)");
    printf("  crash why %d px (size 2)\n", textW(l, 2));
    snprintf(l, sizeof(l), XT(X_CRASH_AT_FMT), "train menu", 6u, 10u);
    printf("  crash at %d px (size 2)\n", textW(l, 2));
    snprintf(l, sizeof(l), XT(X_CRASH_LAST_FMT), "2026.09.28 13:45");
    printf("  crash last %d px (size 2)\n", textW(l, 2));
    snprintf(l, sizeof(l), XT(X_CRASH_BTN_FMT), 99u);
    printf("  crash btn %d px (size 2)\n", textW(l, 2));
  }
  backupTap(300, 340); crashCount = 0;
  bakInfo(bakSlots); bakAsk = true; bakAskSlot = 1; render(); shot("18e_backup_boot_ask"); bakAsk = false;
  closeAll(); openReset(); render(); shot("23_reset");
  closeAll(); openUpdate(); render(); shot("19_update");
  updState = UPD_NONE; xScreen = XS_UPD; render(); shot("22_update_nofile");
  closeAll(); galleryOpen = true; galleryDetail = 0; galleryDirty = true; render(); shot("20_dex_grid_hint");
  galleryOpen = false;
  // siguiente tras la despedida: sale de la caja
  closeAll(); galleryPmd.unload();
  pet.startFarewell(); tick(CEREMONY_MS + 50); pet.update(millis()); ensureMon();
  tick(300); render(); shot("17_next_from_box");
}

int main(int argc, char **argv) {
  gSdRoot = argc > 1 ? argv[1] : "sd";
  mkdir("build/shots", 0755);
  gfx->begin();
  pet.begin();
  box.begin();
  dexLog.begin();
  pet.endHook = onPetEnd;
  sdBegin();
  thumbs.load();
  pet.syncClock(gMockEpoch);
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.eggTap(); pet.eggTap(); pet.eggTap();
  pet.exp = expForLevel(18) + 1200;  // Lv.18
  pet.fullness = 72; pet.joy = 88; pet.energy = 54; pet.hygiene = 23;
  pet.balls = 5; pet.potions = 2;
  ensureMon();
  // WIFI_TAP_CHECK: tocar la pildora WiFi del reloj abre la red
  openClock(); onTap(233, 311);
  printf("  wifi tap: clockOpen=%d xScreen=%d (XS_NET=%d)\n", clockOpen, xScreen, XS_NET);
  xScreen = XS_NONE;
  scenes(true, "");
  scenes(false, "_en");
  return 0;
}

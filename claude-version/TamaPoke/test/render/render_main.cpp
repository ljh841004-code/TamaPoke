// test/render: dibuja pantallas del firmware en el PC y las guarda como .raw
// (RGB565 466x466). run.sh las convierte a PNG. Usa el MISMO codigo de dibujo
// que la placa (Arduino_GFX real); solo el hardware es de mentira (stubs.cpp).
#include "build/sketch.cpp"
#include <sys/stat.h>

extern uint32_t gMockMillis, gMockEpoch;
extern bool gMockPortal;
extern const char *gSdRoot;

static void navCheck(const char *name, bool ok) { printf("  NAV %s: %s\n", ok ? "ok  " : "FAIL", name); }
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
  sickDlg = bdayDlg = false;  // ko12.6
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
  {  // ko12.4: habitacion y objetos; menu de decorar; paseo (con y sin sensor)
    pet.roomOn = 1; pet.deco[0] = DECO_CUSHION + 1; pet.deco[1] = DECO_BALL + 1; pet.deco[2] = DECO_PLANT + 1;
    {  // ko12.5: barras, berrinche y pagina "생활"
      uint8_t f0 = pet.fullness > 80 ? 80 : pet.fullness; pet.fullness = f0;
      onTap(150, 318);  // barra de comida
      navCheck("principal: tocar la barra sube 15", pet.fullness == f0 + 15);
      pet.tantrum = TANTRUM_MIN; render(); shot("01y_tantrum");
      onTap(CX, PET_CY);
      navCheck("berrinche: tocar al bicho abre el dialogo", tantrumDlg);
      render(); shot("01y_tantrum_dlg");
      uint8_t d0 = pet.discipline;
      onTap(150, TT_Y + 100);  // [혼내기]
      navCheck("berrinche: reganar educa", !tantrumDlg && pet.tantrum == 0 && pet.discipline == d0 + 20);
      pet.life.meals = 40; pet.life.snacks = 30;
      pet.rtDay = pet.lastSeenEpoch / 86400; pet.rtBits = 1 << RT_MEAL; pet.rtStreak = 3; pet.rtBest = 5;
      toastUntil = 0; cardOpen = true; cardPage = 5; render(); shot("01z_card_life");
      cardOpen = false; cardPage = 0; memset(&pet.life, 0, sizeof(pet.life)); pet.rtBits = 0;
    }
    {  // ko12.5.1: dialogo de decision con dos cuadros grandes a los lados
      choiceKind = 2; choiceUntil = millis() + 12000; render(); shot("01p_choice_farewell");
      onTap(CX, CH_BY + CH_BH / 2);  // entre los dos cuadros: no pasa nada
      navCheck("decision: tocar entre los cuadros no elige", choiceKind == 2);
      uint32_t a0 = pet.ageMinutes;
      onTap(CH_B2X + 40, CH_BY + 40);  // [계속 함께]
      navCheck("decision: quedarse = 24 h sin preguntar", choiceKind == 0 && pet.farewellDeclinedUntil() == a0 + 1440);
      choiceKind = 1; choiceUntil = millis() + 12000; render(); shot("01p_choice_evolve");
      choiceKind = 0;
    }
    {  // ko12.6: resfriado, visita de la caja y cumpleanos
      navCheck("resfriado: se pone malo", pet.catchCold());
      pet.sickDoses = 2; toastUntil = 0; render(); shot("01s_sick");
      onTap(CX, PET_CY);
      navCheck("resfriado: tocar al bicho abre la medicina", sickDlg);
      render(); shot("01s_sick_dlg");
      onTap(150, SK_Y + 100);
      navCheck("resfriado: 1a toma (falta otra)", !sickDlg && pet.sick && pet.sickDoses == 1 && pet.sickWait);
      onTap(CX, PET_CY); render(); shot("01s_sick_wait");
      onTap(150, SK_Y + 100);
      navCheck("resfriado: la 2a toma aun no", sickDlg && pet.sick);
      pet.sickWait = 0; onTap(150, SK_Y + 100);
      navCheck("resfriado: curado", !sickDlg && !pet.sick);
      uint8_t n0 = box.count();
      box.add(25, 18, false, true, gMockEpoch);
      visitStart(box.count() - 1); render(); shot("01v_visit");
      uint8_t j0 = pet.joy > 80 ? 80 : pet.joy; pet.joy = j0;
      onTap(VISIT_X, VISIT_Y);
      navCheck("visita: tocar al amigo = jugar", pet.joy == j0 + 10 && visitByeAt);
      render(); shot("01v_visit_play");
      tick(4100); tamaLoop();
      navCheck("visita: se va al rato", visitDex == 0);
      box.release(box.count() - 1);
      navCheck("visita: la caja queda igual", box.count() == n0);
      uint8_t bm, bd;
      wxDate(gMockEpoch, nullptr, &bm, &bd, nullptr);
      pet.setBirthday(bm, bd); pet.bdayYear = 0;
      toastUntil = 0; render(); render(); shot("01b_bday_party");
      navCheck("cumpleanos: la fiesta sale sola el dia", bdayDlg && bdayGot);
      tick(1500); onTap(CX, 300);
      navCheck("cumpleanos: tocar cierra", !bdayDlg);
      render(); shot("01b_bday_main");
      pet.setBirthday(0, 0);
    }
    {  // ko12.7: final del viaje
      uint8_t h0 = hall.count();
      const int16_t HD[6] = { 1, 25, 133, 143, 149, 151 };
      for (int i = 0; i < 6; i++) hall.addRaised(HD[i], 50, i == 2, 100, 100, 100, gMockEpoch - (6 - i) * 86400, nullptr);
      pet.journeyStart = gMockEpoch - 400UL * 86400;
      pet.pendingEnding = 1; closeAll();
      endingPoll();
      navCheck("final: se abre solo", xScreen == XS_ENDING && endWhich == 1);
      render(); shot("02e_ending_title");
      tick(800); endingTap(CX, CX);
      tick(2600); render(); shot("02e_ending_parade");
      tick(800); endingTap(CX, CX);
      tick(9000); render(); shot("02e_ending_credits");
      tick(800); endingTap(CX, CX);
      tick(2000); render(); shot("02e_ending_final");
      endingTap(CX, CX);
      navCheck("final: al acabar empieza la 2a vuelta", xScreen == XS_NONE && pet.lap == 1 && pet.endSeen == 1 && !pet.pendingEnding);
      toastUntil = 0; render(); shot("02e_main_crown");
      cardOpen = true; cardPage = 5; render(); shot("02e_card_journey"); cardOpen = false; cardPage = 0;
      pet.pickTokens = 2;
      bool egg0 = pet.isEgg();
      if (!egg0) { pet.speciesId = -1; }
      openEggPick(); render(); shot("02e_eggpick");
      eggPickTap(EP_X0 + 50, EP_Y0 + 30);
      navCheck("vale: elegir el huevo", xScreen == XS_NONE && pet.pickTokens == 1);
      if (!egg0) pet.speciesId = 4;
      openEnding(2, true); tick(3000); render(); shot("02e_ending2_title");
      endPhase = 3; endT0 = millis(); tick(2000); render(); shot("02e_ending2_final");
      endingTap(CX, CX);
      navCheck("final 2 visto de nuevo: no cambia la partida", pet.endSeen == 1);
      while (hall.count() > h0) hall.release(hall.count() - 1);
      pet.lap = 0; pet.endSeen = 0; pet.pickTokens = 0; closeAll();
    }
    pet.bgAsked = 0; render(); shot("01q_main_bg_ask");
    onTap(320, BGQ_Y + 160);  // [방]
    navCheck("fondo: elegir habitacion una vez", pet.roomOn == 1 && pet.bgAsked == 1);
    render(); shot("01r_main_room");
    gMockEpoch = 1790343900 + 9 * 3600; pet.lastSeenEpoch = gMockEpoch;  // 22:45
    pet.sleeping = true; render(); shot("01r_main_room_night"); pet.sleeping = false;
    gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
    pet.roomOn = 0; render(); shot("01r_main_outdoor_deco");
    pet.bestStreak = 9; pet.allGameHi = 25;
    openSettings(); render(); shot("01s_settings");
    settingsTap(150, SET_Y0 + 4 * SET_DY + 10);  // [방 꾸미기]
    navCheck("ajustes -> decorar", xScreen == XS_ROOM);
    render(); shot("01t_room_menu");
    roomTap(300, ROOM_TOG_Y + 10);  // [방]
    navCheck("decorar: fondo habitacion", pet.roomOn == 1);
    roomTap(100, ROOM_GRID_Y + 52 + 10);  // pelota
    navCheck("decorar: elegir la pelota", roomSel == DECO_BALL);
    render(); shot("01u_room_pick");
    roomTap(ROOM_SLOT_X[0], ROOM_PAN_Y + 40);  // a la izquierda (y deja el sitio de delante)
    navCheck("decorar: colocar (un sitio por objeto)", pet.deco[0] == DECO_BALL + 1 && pet.deco[1] == 0);
    roomTap(300, ROOM_GRID_Y + 2 * 52 + 10);  // muneco: bloqueado
    navCheck("decorar: bloqueado no se elige", roomSel < 0);
    roomTap(30, NAV_Y); navCheck("decorar: <- vuelve", xScreen == XS_SET);
    settingsTap(320, SET_Y0 + 4 * SET_DY + 10);  // [산책]
    navCheck("ajustes -> paseo", xScreen == XS_WALK);
    render(); shot("01v_walk_nosensor");
    imuAddr = 0x6B;  // como si la placa tuviera el sensor
    pet.walk.day = (uint32_t)(gMockEpoch / 86400); pet.walk.rw = 1;
    const uint16_t W[7] = { 3214, 7430, 2890, 5005, 1830, 6120, 4210 };
    memcpy(pet.walk.days, W, sizeof(W)); pet.walk.total = 152340;
    render(); shot("01w_walk_page");
    xScreen = XS_NONE; render(); shot("01x_main_walk_pill");
    navCheck("principal: el contador abre el paseo", walkPillHit(WALK_RING_X, WALK_RING_Y) && !navHit(NAV_L, WALK_RING_X, WALK_RING_Y)
             && !walkPillHit(26, NAV_Y));
    imuAddr = 0;
    pet.roomOn = 0; memset(pet.deco, 0, sizeof(pet.deco)); memset(&pet.walk, 0, sizeof(pet.walk));
    closeAll();
  }
  {  // ko11.16: orbe equipado en el hueco de abajo a la derecha (ataque = llamas, defensa = brillo)
    uint8_t pt = DEX_TBL[pet.speciesId].ptype;
    pet.orb = orbMake(pt, false, 18); render(); shot("01o_main_orb_atk");
    tick(140); render(); shot("01o_main_orb_atk2");
    pet.orb = orbMake(pt, true, 22); render(); shot("01p_main_orb_def");
    pet.orb = 0;
    // escaparate: 16 tipos, ataque arriba y defensa abajo, en varios instantes (animacion)
    for (int f = 0; f < 12; f++) {
      uiScreenBg();
      for (int t = 0; t < 16; t++) {
        int col = t % 4, row = t / 4;
        int cx = CX + (col * 2 - 3) * 52, cy = 70 + row * 100;
        drawOrb(cx - 0, cy + 10, 18, orbMake(t, (row + col) & 1, 20), gMockMillis + t * 137);
        setSize(1); gfx->setTextColor(UI_INK); setCur(cx - textW(typeName(t), 1) / 2, cy + 42); printT(typeName(t));
      }
      char fn[32]; snprintf(fn, sizeof(fn), "90_orbs_%02d", f); shot(fn);
      tick(60);
    }
  }
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
  {  // ko11.18: condiciones de la despedida: forma final y 3 dias (una si, otra no; luego las dos)
    int16_t sp = pet.speciesId; uint32_t ex = pet.exp, ag = pet.ageMinutes;
    pet.speciesId = 6; pet.exp = expForLevel(40); pet.ageMinutes = 2 * 1440 + 5 * 60; ensureMon();
    render(); shot("03p_progress_farewell_wait");
    pet.ageMinutes = 3 * 1440 + 90; render(); shot("03q_progress_farewell_ready");
    pet.speciesId = sp; pet.exp = ex; pet.ageMinutes = ag; ensureMon();
  }
  {  // ko11.17: Pikachu en la pantalla principal (retoque de color)
    int16_t sp = pet.speciesId; uint32_t ex = pet.exp;
    pet.speciesId = 25; pet.exp = expForLevel(20); ensureMon(); cardOpen = false;
    render(); shot("01k_main_pikachu");
    pet.speciesId = sp; pet.exp = ex; ensureMon(); cardOpen = true;
  }
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
  {  // ko11.31: con 4 movimientos llega uno nuevo: cual olvidar (o no aprenderlo)
    closeAll(); tick(3000);
    while (moveCount(pet.mv) < 4) { uint8_t x = pet.moveRandomNew(); if (!x) break; pet.moveOfferNew(x); }
    pet.moveLearned = 0;
    uint8_t nw = pet.moveRandomNew();
    pet.moveOffer = nw; moveLearnLoop();
    navCheck("movimientos: oferta con 4 -> dialogo", choiceKind == 3);
    render(); shot("03u_move_offer");
    onTap(LD_X0 + 30, LD_Y1 + 20);  // casilla 3 (abajo izquierda)
    navCheck("movimientos: olvida la casilla tocada", choiceKind == 0 && pet.mv[2] == nw && !pet.moveOffer);
    render(); shot("03v_move_learned");
    uint8_t nw2 = pet.moveRandomNew();
    pet.moveOffer = nw2; moveLearnLoop(); onTap(233, LD_NO_Y + 20);  // [배우지 않는다]
    navCheck("movimientos: [배우지 않는다] no cambia nada", choiceKind == 0 && !pet.moveOffer && !movesHas(pet.mv, nw2));
    uint16_t L = pet.level(); pet.moveLv = (uint8_t)(L / 5 * 5); pet.moveOffer = 0;
    pet.addExp(expForLevel(L / 5 * 5 + 5) - pet.exp);
    navCheck("movimientos: al siguiente multiplo de 5 se ofrece otro", pet.moveOffer != 0 || pet.moveLearned != 0);
    pet.moveOffer = 0; pet.moveLearned = 0; toastUntil = 0; closeAll();
    cardOpen = true; cardPage = 0;  // sigue la ficha (comida favorita)
  }
  {  // ko11.27: comida favorita (desconocida / conocida + toque = que significa) y dias juntos
    bool bk = pet.berryKnown;
    pet.berryKnown = false; render(); shot("03r_profile_fav_unknown");
    pet.berryKnown = true; render(); shot("03s_profile_fav_known");
    onTap(CX, PROF_FAV_Y + 8); render(); shot("03t_profile_fav_tap");
    navCheck("perfil: tocar la comida = explicacion", cardOpen && cardPage == 0 && cardMsg == XT(X_FAV_HINT_KNOWN) && timeLeft(cardMsgUntil));
    cardMsgUntil = 0; pet.berryKnown = bk;
  }
  strcpy(pet.nick, "불꽃이"); render(); shot("03e_card_profile_nick"); pet.nick[0] = 0;
  cardPage = 2; render(); shot("03d_card_medals");
  pet.addCandy(pet.speciesId, 12); cardPage = 4; render(); shot("03f_card_candy");
  pet.careMistakes = 1; pet.addCandy(pet.speciesId, 10); render(); shot("03i_card_candy_mistake"); pet.careMistakes = 0;
  // ko10.11: bolsa de caramelos
  pet.addCandy(25, 7); pet.addCandy(133, 4); pet.addCandy(6, 2); pet.addCandy(92, 11); pet.rareCandy = 2;
  closeAll(); openCandyBag(); render(); shot("03j_candy_bag");
  candyBagTap(BAG_ROW_X + 100, BAG_ROW_Y + 1 * (BAG_ROW_H + BAG_GAP) + 10); render(); shot("03k_candy_bag_trade");
  candyBagTap(0, 0); candyBagTap(BAG_ROW_X + 100, BAG_RARE_Y + 10); render(); shot("03l_candy_bag_rare");
  {  // ko11.15.1: sobrantes de muchas familias -> trozos de caramelo raro
    candyBagTap(0, 0);
    for (int16_t f : { 16, 19, 41, 74, 92, 129, 133 }) pet.addCandy(f, (f % 3) + 1);
    pet.rareShards = 4;
    openCandyBag(); render(); shot("03n_candy_bag_leftovers");
    candyBagTap(CX, BAG_SHARD_BTN_Y + 10); render(); shot("03o_candy_bag_shards");
  }
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
  tick(90); render(); shot("25_sack");
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
  tick(300); render(); shot("07e_train_speed_hint");  // ko11.17: solo el 1o parpadea
  speedPress(spdBx[0], spdBy[0]);
  tick(120); render(); shot("07_train_speed");
  // ko10.6: resultado con puntos por reflejos y la media
  for (int r = 0; r < SPD_ROUNDS; r++) {
    if (r == 3) {  // ko11.17: la 2a tanda sigue contando (4-7 o asi) y parpadea el primero
      for (int i = 0; i < 400 && spdPhase != SP_SHOW; i++) { tick(20); render(); }
      tick(300); render(); shot("07f_train_speed_round4");
    }
    if (r == 7) {  // ko11.16: a media partida (puntos de progreso, aro de tiempo)
      for (int i = 0; i < 400 && spdPhase != SP_SHOW; i++) { tick(20); render(); }
      tick(700); render(); shot("07d_train_speed_mid");
    }
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
  battleArtTap(BART_X + 10, BART_Y + 10); render(); shot("08q_battle_art_prg");  // ko11.16: PMD <-> PokeRogue
  battleArtTap(BART_X + 10, BART_Y + 10);
  {  // ko11.31: [싸운다] -> los 4 movimientos; estados junto a las cajas de vida
    onTap(BM_X + 20, BM_Y1 + 20);
    navCheck("batalla: [싸운다] abre los movimientos", bMoveMenu);
    bMe.pp[1] = 2;
    render(); shot("08m_fight_moves");
    {  // ko12.8: marcas: descansa (gris), no le hace nada (aspa), muy eficaz (*), poco eficaz (triangulo)
      Battler keepMe = bMe; uint8_t keepT = bFoe.type;
      const uint8_t mv[4] = { 7, 41, 22, 66 };
      memcpy(bMe.mv, mv, 4); movesFillPP(bMe);
      bFoe.type = PT_GROUND; bMe.restMv = 7; bMe.restT = 1;
      navCheck("ko12.8: marca descanso", moveMarkFor(bMe, bFoe, 0) == MVK_REST);
      navCheck("ko12.8: marca inmune", moveMarkFor(bMe, bFoe, 1) == MVK_NONE);
      navCheck("ko12.8: marca muy eficaz", moveMarkFor(bMe, bFoe, 2) == MVK_SUPER);
      navCheck("ko12.8: marca poco eficaz", moveMarkFor(bMe, bFoe, 3) == MVK_WEAK);
      render(); shot("08m2_fight_marks");
      bMe = keepMe; bFoe.type = keepT;
    }
    onTap(MV_BACK_X + 10, MV_Y0 + 40);
    navCheck("batalla: [<] vuelve al menu", !bMoveMenu);
    bFoe.st = ST_PAR; bMe.st = ST_PSN;
    render(); shot("08n_status_badges");
    bFoe.st = bMe.st = 0;
    movesFillPP(bMe);
    // un movimiento de estado (grunido) y lo que pasa despues
    bPhase = BP_PLAY; bqAisMe = true; bqN = 3; bqI = 0;
    memset(bq, 0, sizeof(BEvent) * 3);
    bq[0].side = 0; bq[0].kind = EV_USE; bq[0].mid = 151;
    bq[1].side = 1; bq[1].kind = EV_STAT; bq[1].mid = 151; bq[1].eff = 0; bq[1].val = -1;
    bq[2].side = 1; bq[2].kind = EV_STATUS; bq[2].mid = 161; bq[2].val = ST_PAR;
    for (int i = 0; i < 3; i++) { bq[i].hpA = bMe.hp; bq[i].hpB = bFoe.hp; }
    for (int i = 0; i < 3; i++) {
      startEvent(i);
      gMockMillis = bqT + 400;
      render();
      snprintf(n, sizeof(n), "08o_status_ev%d", i);
      shot(n);
    }
    bPhase = BP_MENU; bqN = bqI = 0;
    txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName); bvL2[0] = 0;
  }
  bvMeFainted = bvFoeFainted = true; render(); shot("08z_battle_empty");  // fondo sin Pokemon (comparar sprites)
  bvMeFainted = bvFoeFainted = false;
  box.add(bFoe.dex, 5, false, true, 0); box.add(bFoe.dex, 7, false, true, 0); bvOwned = 2; bvOwnedT = gMockMillis;
  render(); shot("08b_battle_owned"); bvOwned = 0;
  pet.orb = orbMake(DEX_TBL[pet.speciesId].ptype, false, 18); render(); shot("08o_battle_orb"); pet.orb = 0;  // ko11.16
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
        // ko11.31: los 3 ataques de su tipo en esa fase (id 1 + tipo*9 + fase*3 + variante)
        for (uint8_t v = 0; v < 3; v++) {
          uint8_t id = moveIdTyped(DEX_TBL[d].ptype, moveTier(d), v);
          bPhase = BP_PLAY; bqAisMe = true; bqN = 1; bqI = 0;
          memset(&bq[0], 0, sizeof(bq[0]));
          bq[0].side = 0; bq[0].kind = EV_HIT; bq[0].move = BA_M0; bq[0].mid = id;
          bq[0].eff = 2; bq[0].dmg = 5; bq[0].hpA = bMe.hp; bq[0].hpB = bFoe.hp;
          fxPreloadAsync(&id, 1); for (int k = 0; k < 4000; k++) fxPump();
          txFmt(bvL1, sizeof(bvL1), X_USED, bvMeName, moveNameId(id));
          bvL2[0] = 0;
          bqT = gMockMillis;
          for (uint32_t at : { 250u, 500u, 750u, 1000u }) {  // ko11.30: 4 momentos (efectos de la SD)
            gMockMillis = bqT + at;
            render();
            if (at == 500u && v == 0 && fxFind(id)) {  // ko11.32: con el fondo copiado sale igual que pintado
              static uint16_t prev[LCD_WIDTH * LCD_HEIGHT];
              battleBgCacheReset();
              render();  // pintado (y guardado)
              memcpy(prev, gfx->getFramebuffer(), sizeof(prev));
              render();  // copiado
              static bool once = false;
              if (!once) { once = true; navCheck("efecto SD: fondo en cache = pintado", memcmp(prev, gfx->getFramebuffer(), sizeof(prev)) == 0); }
            }
            if (v == 0) snprintf(n, sizeof(n), "tier_%03d_%u", d, at);
            else snprintf(n, sizeof(n), "mv_%03d_v%u_%u", d, v, at);
            shot(n);
          }
        }
      }
    }
    // ko11.30: el rival ataca con el efecto de la SD (lado 1: de arriba a abajo)
    bPhase = BP_PLAY; bqN = 1; bqI = 0; bq[0].side = 1; bq[0].mid = movesMain(bFoe);
    fxPreloadAsync(bFoe.mv, 4); for (int k = 0; k < 4000; k++) fxPump();
    txFmt(bvL1, sizeof(bvL1), X_USED, bvFoeName, moveNameId(bq[0].mid));
    bqT = gMockMillis;
    for (uint32_t at : { 250u, 500u, 750u, 1000u }) {
      gMockMillis = bqT + at;
      render();
      snprintf(n, sizeof(n), "fxfoe_%u", at);
      shot(n);
    }
    bq[0].side = 0;
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
    { uint8_t kb = pet.badges; pet.badges = 0xFF; gymPage = 0; render(); shot("50c_gyms_all8"); pet.badges = kb; }
    closeAll(); openDaily(); render(); shot("51_daily");
    closeAll(); trainMenuOpen = true; trainMenuPage = 1; render(); shot("52_battle_page"); trainMenuOpen = false;
    closeAll(); openRegionPick(); regionPage = 1; render(); shot("53_region_locked");
    // gimnasio de Lt. Surge con lluvia: presentacion y resultado con medalla
    closeAll();
    gMockEpoch = 1772356800u; pet.lastSeenEpoch = gMockEpoch;  // lluvia
    pet.badges = 0x03;
    gymPage = 0; gymTap(GY_X + 10, GY_Y + 2 * (GY_H + GY_GAP) + 10);  // 3er gimnasio
    if (xScreen == XS_PARTY) partyStart(true);  // ko11.20: sin ayudantes
    render(); shot("54_gym_intro");
    // ko11.19: [자동] [N마리] y el combate automatico
    tick(2300); updateWild(); render(); shot("54b_gym_auto_menu");
    wildTap(BM_X + BM_W + BM_GAP + 10, BM_Y2 + 10);  // N -> 1 (ko11.31: [N마리] abajo en el centro)
    render(); shot("54c_gym_auto_count");
    navCheck("auto: N마리 1..restantes", autoCount == 1);
    wildTap(BM_X + BM_W + BM_GAP + 10, BM_Y2 + 10);
    wildTap(BM_X + 2 * (BM_W + BM_GAP) + 10, BM_Y1 + 10);  // [자동] (ko11.31: arriba a la derecha)
    navCheck("auto: en marcha", autoLeft == 2);
    render(); shot("54d_gym_auto_on");
    { uint16_t hp0 = bMe.hp; uint8_t p0 = pet.potions; pet.potions = 5;
      bMe.hp = bMe.maxHp / 3; bFoe.hp = bFoe.maxHp;
      bool hard = autoHardFoe();
      navCheck("auto: pocion segun rival", (autoPick() == BA_POTION) == hard);
      bMe.hp = bMe.maxHp / 5;
      navCheck("auto: pocion bajo 30 %", autoPick() == BA_POTION);
      bMe.hp = hp0; pet.potions = p0; }
    tick(800); updateWild();
    navCheck("auto: juega solo", bPhase == BP_PLAY);
    wildTap(200, 200);
    navCheck("auto: tocar para", autoLeft == 0);
    for (int i = 0; i < 40 && bPhase == BP_PLAY; i++) { tick(500); updateWild(); }
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
    {  // ko11.20: el del medio: solo en equipo (plata) con sus ayudantes; el ultimo, los dos
      fame.markFlag(1, BOXF_TEAM);
      FameRec *r = frGetOrAdd(fame.at(1));
      r->solo = 0; r->team = 2; r->help[0] = 7; r->help[1] = 1; r->shiny = 0x02;
      r = frGetOrAdd(fame.at(2));
      r->solo = 3; r->team = 1; r->help[0] = 25; r->help[1] = 0;
      frSave();
    }
    pet.champWins = 3; pet.champStreak = 2; pet.champBest = 2;
    pet.fameStreak[1] = 1; pet.fameStreak[2] = 2;  // ko11.6.1: rachas
    render(); shot("59_league_fame");
    openFame(); render(); shot("59d_fame_grid");
    fameTap(FM_X + FM_CELL + 10, FM_Y + 10); render(); shot("59e_fame_detail");
    fameClose(); fameTap(FM_X + 10, FM_Y + 10); render(); shot("59g_fame_detail_both"); fameClose();
    fameTap(FM_X + FM_CELL + 10, FM_Y + 10);
    tick(450); render(); shot("59f_fame_detail_fx");  // ko11.17: tecnica de su tipo
    for (int f = 0; f < 20; f++) { char fn[32]; snprintf(fn, sizeof(fn), "92_fame_anim_%02d", f); tick(100); render(); shot(fn); }
    fameClose(); fameClose();
    leagueTap(GY_X + 40, LG_BTN_Y + 10);
    if (xScreen == XS_PARTY) partyStart(true);
    render(); shot("59b_league_intro");
    bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
    finishBattle(true, false, false); render(); shot("59c_league_win");
    {  // ko11.20: el mismo Pokemon vuelve a ganar: la misma ficha, una victoria mas
      uint8_t c0 = fame.count();
      int ci = fameCardOfPet();
      uint16_t s0 = ci >= 0 ? fameRecOf(fame.at((uint8_t)ci)).solo : 0;
      bRewarded = false; finishBattle(true, false, false);
      navCheck("salon: una ficha por Pokemon", fame.count() == c0 && ci >= 0 &&
               fameRecOf(fame.at((uint8_t)ci)).solo == s0 + 1);
    }

    closeAll();
    gymPage = 0; gymTap(GY_X + 10, GY_Y + 1 * (GY_H + GY_GAP) + 10);  // revancha de Misty
    if (xScreen == XS_PARTY) partyStart(true);
    render(); shot("57b_rematch_intro");
    closeAll();
    fame.wipe(); pet.champWins = 0;
    memset(pet.gymWins, 0, sizeof(pet.gymWins)); memset(pet.gymDay, 0, sizeof(pet.gymDay));
    pet.badges = 0;
    gMockEpoch = 1790343900; pet.lastSeenEpoch = gMockEpoch;
  }
  // ko11.20: equipo contra entrenadores (ayudantes de la caja, cambio al estilo PokeRogue)
  {
    closeAll(); pet.energy = 80; pet.badges = 0;
    box.wipe();
    box.add(7, 30, false, true, gMockEpoch - 3000);   // Squirtle: agua > roca
    box.add(4, 20, false, true, gMockEpoch - 2000);   // Charmander: fuego < roca
    box.add(1, 16, true, true, gMockEpoch - 1000);    // Bulbasaur: planta > roca
    box.add(25, 18, false, false, gMockEpoch);        // Pikachu
    box.add(133, 12, false, true, gMockEpoch);        // Eevee
    openGyms(); gymPage = 0; gymTap(GY_X + 10, GY_Y + 10);  // Brock (roca)
    navCheck("equipo: gimnasio abre la eleccion", xScreen == XS_PARTY);
    helperUse(box.at(1)); helperUse(box.at(1)); helperUse(box.at(1));  // Charmander: hoy ya 3 veces
    helperUse(box.at(0));
    render(); shot("53a_party_pick");
    partyPickTap(200, PP_ROW_Y + 10);                          // 1a fila
    partyPickTap(200, PP_ROW_Y + (PP_ROW_H + PP_ROW_GAP) + 10);  // 2a fila
    render(); shot("53b_party_pick_two");
    // ko12.2: Brock tiene 2: un ayudante como mucho (el 2o toque cambia el elegido)
    navCheck("equipo: contra 2, un ayudante", ppPickN() == 1 && ppMaxHelpers() == 1);
    partyPickTap(250, PP_BTN_Y + 10);  // [시작 1/1]
    navCheck("equipo: 2 en el combate (como el rival)", xScreen == XS_WILD && pN == 2);
    tick(2300); updateWild(); render(); shot("53c_party_battle");
    bMe.hp = 0; bvMeTgt = 0; bqN = bqI = 0; bPhase = BP_PLAY; bvMeFainted = true;
    updateWild(); render(); shot("53d_party_swap_forced");
    navCheck("equipo: cae el mio -> elegir", bPhase == BP_SWAP && bSwapMode == 0);
    wildTap(90, SW_Y + 10);
    tick(100); render(); shot("53e_party_helper_in");
    navCheck("equipo: sale el ayudante", pCur != 0 && bPhase == BP_MENU);
    bFoe.hp = 0; nextTrainerMon(); tick(2300); updateWild(); render(); shot("53f_party_next_switch");
    // ko12.2: contra 2 vamos 2: caido el mio, no queda a quien cambiar (antes 3)
    navCheck("equipo: rival nuevo -> cambiar? (si queda alguien)", (bPhase == BP_SWAP && bSwapMode == 1) || !partyOtherAlive());
    wildTap(380, SW_Y + 10);  // [그대로]
    wildTap(300, 190);        // mi caja de vida -> cambio a mano
    render(); shot("53g_party_manual");
    navCheck("equipo: cambio a mano (si queda alguien)", (bPhase == BP_SWAP && bSwapMode == 2) || !partyOtherAlive());
    wildTap(380, SW_Y + 10);  // [취소]
    bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
    finishBattle(true, false, false); render(); shot("53h_party_win");
    navCheck("equipo: ayudantes +1 nivel", bPartyNote[0] != 0);
    afterResult();  // vuelve a la lista de gimnasios
    closeAll(); pet.energy = 80; startWildIn(petRegion());  // ko11.20.1: el siguiente salvaje
    navCheck("equipo: luego un salvaje con el que crias", pCur == 0 && bvMeDex == pet.speciesId);
    render(); shot("53i_wild_after_team");
    closeAll(); endBattleScreen();
    box.wipe(); pet.badges = 0;
  }
  {  // ko12.2: liga (6): hasta 5 ayudantes; el cambio con 5 para elegir va compacto
    closeAll(); pet.energy = 80; box.wipe();
    const int16_t ds[6] = { 7, 4, 1, 25, 133, 152 };
    for (int i = 0; i < 6; i++) box.add(ds[i], 20 + i, false, true, gMockEpoch - 50000 - 100 * i);
    const int16_t fd[6] = { 18, 65, 112, 59, 103, 9 };
    Battler team[6];
    for (int j = 0; j < 6; j++) team[j] = makeTrainerMon(fd[j], 40);
    partyOpen(BK_CHAMP, CHAMP_REGION, team, 6, XS_GYM);
    navCheck("liga: eleccion de equipo", xScreen == XS_PARTY && ppMaxHelpers() == 5);
    for (int r = 0; r < PP_ROWS; r++) partyPickTap(200, PP_ROW_Y + r * (PP_ROW_H + PP_ROW_GAP) + 10);
    partyPickTap(300, PP_NAV_Y + 5);  // pagina 2
    partyPickTap(200, PP_ROW_Y + 10);
    render(); shot("53j_party_pick_league");
    navCheck("liga: 5 elegidos", ppPickN() == 5);
    partyPickTap(250, PP_BTN_Y + 10);
    navCheck("liga: 6 en el combate", xScreen == XS_WILD && pN == 6);
    tick(2300); updateWild();
    bMe.st = ST_PSN;  // el mio envenenado: el estado es de cada uno, no del equipo
    bMe.hp = 0; bvMeTgt = 0; bqN = bqI = 0; bPhase = BP_PLAY; bvMeFainted = true;
    updateWild(); render(); shot("53k_party_swap_six");
    navCheck("liga: elegir entre 5", bPhase == BP_SWAP);
    {  // ko12.5.1: el banquillo descansa: +5 % por turno hasta la mitad
      uint8_t j = pCur == 1 ? 2 : 1;
      pMon[j].hp = 1;
      for (int t = 0; t < 30; t++) benchRest();
      navCheck("liga: el banquillo se cura hasta la mitad", pMon[j].hp == pMon[j].maxHp / 2);
      uint16_t h = pMon[j].hp; benchRest();
      navCheck("liga: y no pasa de la mitad", pMon[j].hp == h);
    }
    {
      int16_t xs[PARTY_MAX]; int16_t ws[PARTY_MAX]; int8_t who[PARTY_MAX];
      swapLayout(xs, ws, who);
      wildTap(xs[0] + 5, SW_Y + 10);
      tick(100); updateWild();
      navCheck("liga: el veneno no pasa al siguiente", pCur != 0 && bMe.st == ST_NONE && pMon[0].st == ST_PSN);
    }
    closeAll(); endBattleScreen();
    box.wipe();
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
    // ko11.19: soltar a uno que solo te siguio: regalo pequeno (boton soltar x2)
    for (uint8_t i = 0; i < box.count(); i++)
      if (!(box.at(i).flags & (BOXF_CAUGHT | BOXF_RAISED))) {
        boxSel = i; boxTap(120, 320); boxTap(120, 320);
        render(); shot("11j_box_release_gift"); toastUntil = 0;
        break;
      }
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
  hall.addRaised(25, 50, false, 110, 106, 108, gMockEpoch);  // ko11.21: con las 8 medallas
  hall.markFlag(2, BOXF_PERFECT);
  pet.markFamRaised(16);  // Pidgey ya criado: en gris
  pet.markFamRaised(6);
  render(); shot("11a_box_tabs");
  toastUntil = 0; boxHall = true; render(); shot("11b_box_hall");
  boxSel = 0; render(); shot("11d_hall_detail"); boxSel = -1;  // ko11.17: escarapela
  for (int i = 0; i < hall.count(); i++) if (hall.at((uint8_t)i).flags & BOXF_PERFECT) boxSel = i;
  if (boxSel >= 0) { render(); shot("11e_hall_perfect"); }
  {  // ko12.4: recuerdos de la cinta: una crianza con diario y una de antes (sin)
    memStore.begin();
    MemRec r = {};
    r.life.start = gMockEpoch - 86400UL * 23; r.life.from = LF_EGG; r.life.firstDex = 172;
    r.life.evoDex[0] = 25; r.life.evoT[0] = gMockEpoch - 86400UL * 20;
    r.life.evoDex[1] = 26; r.life.evoT[1] = gMockEpoch - 86400UL * 6;
    r.life.firstWin = gMockEpoch - 86400UL * 19;
    r.life.meals = 214; r.life.snacks = 37; r.life.cleans = 96; r.life.pets = 512; r.life.plays = 64; r.life.trains = 41;
    r.end = gMockEpoch; r.lvl = 50; r.days = 23; r.bond = 92; r.mistakes = 1; r.medals = 8; r.badges = 8;
    r.wins = 133; r.link = 4; r.daily = 12; r.champ = 2;
    r.gameHi = 31; r.strHi = 88; r.defHi = 47; r.speHi = 1320; r.vbBest = 9;
    strcpy(r.nick, "PIKA");
    for (int i = 0; i < hall.count(); i++)
      if (hall.at((uint8_t)i).dex == 25) memStore.put(hall.at((uint8_t)i).epoch, 25, r);
    for (int i = 0; i < hall.count(); i++) if (hall.at((uint8_t)i).dex == 25) boxSel = i;
    render(); shot("11k_hall_mem_btn");
    boxTap(150, 320);  // [추억]
    navCheck("cinta: [추억] abre los recuerdos", memSel == boxSel && memPage == 0);
    render(); shot("11l_hall_memory");
    boxTap(CX, 200); render(); shot("11m_hall_memory2");
    navCheck("cinta: tocar = la otra pagina", memPage == 1);
    boxTap(30, NAV_Y); navCheck("cinta: <- vuelve a la ficha", memSel < 0 && boxSel >= 0);
    for (int i = 0; i < hall.count(); i++) if (hall.at((uint8_t)i).dex == 6) boxSel = i;
    boxTap(150, 320); render(); shot("11n_hall_memory_old");
    memSel = -1;
  }
  boxSel = -1; boxHall = false;
  {  // ko11.16: bolsa de orbes (tercera pestana)
    toastUntil = 0; boxOrb = true; render(); shot("11o_orb_bag_empty");
    uint8_t pt = DEX_TBL[pet.speciesId].ptype;
    pet.orb = orbMake(pt, false, 18);
    const uint8_t T[] = { pt, PT_WATER, PT_GRASS, PT_ELECTRIC, PT_PSYCHIC, PT_DRAGON, PT_ICE, PT_GHOST, PT_ROCK, PT_DARK };
    for (int i = 0; i < 10; i++) pet.gainOrb(orbMake(T[i], i == 0 ? true : (i & 1), (uint8_t)(10 + (i * 7) % 16)));
    render(); shot("11o_orb_bag");
    tick(120); render(); shot("11o_orb_bag2");
    for (int f = 0; f < 14; f++) { char fn[32]; snprintf(fn, sizeof(fn), "91_orbbag_%02d", f); tick(60); render(); shot(fn); }
    orbSel = 0; render(); shot("11p_orb_detail_worn");
    orbSel = 1; render(); shot("11q_orb_detail_fits");
    orbSel = 3; render(); shot("11r_orb_detail_nofit");
    orbSel = -1;
    {  // ko11.19: fusion de orbes: elegir 3 -> animacion -> resultado
      toastUntil = 0;
      orbBagTap(CX, SYN_BTN_Y + 10);  // [구슬 합성]
      int cx, cy;
      for (int i : { 1, 2, 4 }) { orbCell(i, cx, cy); orbBagTap(cx, cy); }
      render(); shot("11s_orb_synth_pick");
      orbBagTap(290, SYN_BTN_Y + 10);  // [합성하기 3/3]
      tick(700); render(); shot("11t_orb_synth_anim");
      tick(900); render(); render(); shot("11u_orb_synth_result");
      orbBagTap(CX, 200);
    }
    {  // ko12.2.1: el arcoiris: en la bolsa, su ventana y el resultado de la fusion
      uint8_t pt = DEX_TBL[pet.speciesId].ptype;
      pet.gainOrb(orbMakeDual(pt, 21));
      orbPage = 0; orbSel = -1; render(); shot("11v_orb_bag_rainbow");
      for (int k = 0; k < (int)orbViewN(); k++) { bool w; if (orbDual(orbViewAt(k, w))) { orbSel = k; break; } }
      render(); shot("11w_orb_detail_rainbow");
      orbSel = -1;
      synRes = 3; synOut = orbMakeDual(pt, 21); synShow = true; render(); shot("11x_orb_synth_rainbow");
      synShow = false;
      navCheck("orbe: arcoiris = ataque+defensa", orbDual(synOut) && pet.orbFits(synOut));
    }
    boxOrb = false;
  }
  xScreen = XS_NEXTPICK; nextPage = 0; render(); shot("11c_next_pick");
  xScreen = XS_BOX;
  // pokedex
  closeAll();
  for (int d : { 16, 19, 25, 129, 133, 143 }) dexLog.seen(d, gMockEpoch - 86400 * 3);
  dexLog.caught(25, gMockEpoch);
  dexLog.caught(4, gMockEpoch);  // ko11.31: circulo en la rejilla
  galleryOpen = true; galleryPage = 0; galleryDetail = 0; galleryDirty = true;
  render(); shot("12_dex_grid");
  galleryDetail = 25; galleryPmd.load(25, false);
  {  // ko11.31: sus movimientos fuertes (2 aprendidos)
    uint8_t mv[4];
    dexTopMoves(25, mv);
    dexLog.learned(25, mv[0]); dexLog.learned(25, mv[2]);
  }
  render(); shot("13_dex_detail");
  {  // ko12.3.3: el toque guarda un millis() posterior al "now" de la vuelta: no debe contar como 5 min sin tocar
    uint32_t keepLI = lastInteract;
    lastInteract = millis() + 20;
    updateBrightness(millis());
    navCheck("brillo: toque despues del inicio de la vuelta no oscurece", dimStage == 0);
    lastInteract = keepLI;
    updateBrightness(millis());
  }
  {  // ko12.2.1: los dibujos de los 16 tipos (+ estado) en los botones de la ficha
    uiScreenBg();
    for (uint8_t t = 0; t <= PT_COUNT; t++) {
      int x = 83 + (t % 5) * 75, y = 83 + (t / 5) * 75;
      uint16_t c = typeColor(t < PT_COUNT ? t : PT_NORMAL);
      gfx->fillCircle(x, y, DEXMV_R, c);
      gfx->drawCircle(x, y, DEXMV_R, UI_INK);
      drawTypeGlyph(x, y, t < PT_COUNT ? t : PT_NORMAL, t == PT_COUNT, c);
    }
    shot("13d_type_glyphs");
  }
  {  // ko12.1.1: el primer fotograma (aun sin el sprite grande): la miniatura debe quedar igual de grande y en el mismo sitio
    galleryPmd.unload(); galleryLoadWant = 0;
    render(); shot("13c_dex_detail_thumb");
    galleryPmd.load(25, false);
  }
  {
    uint8_t mv[4];
    dexTopMoves(25, mv);
    { uint8_t m2[4]; dexTopMoves(25, m2); fxPreloadAsync(m2, 4); }
    {  // ko12.2.1: tocado antes de leerse: espera (sin el de puntos) y luego sale
      bool queued = fxQueued(mv[0]) && !fxFind(mv[0]);
      onTap(DEXMV_XY[0][0], DEXMV_XY[0][1]);
      navCheck("pokedex: aun leyendo -> espera sin efecto dibujado", !queued || (dexMvFx == 0 && dexMvPend == mv[0]));
      if (queued) { render(); shot("13e_dex_move_wait"); }
      for (int k = 0; k < 2000 && dexMvPend; k++) { fxPump(); render(); }
      navCheck("pokedex: leido -> sale el de la SD", dexMvFx == mv[0] && (!queued || dexMvSd == (fxFind(mv[0]) != nullptr)));
      tick(1700); render(); dexMvFx = 0;
    }
    for (int k = 0; k < 2000; k++) fxPump();  // la lectura en segundo plano de sus efectos
    navCheck("pokedex: los aprendidos van primero", dexLog.hasLearned(25, mv[0]) && dexLog.hasLearned(25, mv[1]));
    onTap(DEXMV_XY[0][0], DEXMV_XY[0][1]);
    navCheck("pokedex: tocar un movimiento aprendido lo ensena", dexMvFx == mv[0]);
    tick(500); render(); shot("13b_dex_move_fx");
    tick(1200); dexMvFx = 0;
    onTap(DEXMV_XY[2][0], DEXMV_XY[2][1]);
    navCheck("pokedex: uno sin aprender no se ensena", dexMvFx == 0);
  }
  galleryDetail = 52; galleryPmd.load(52, false);
  render(); shot("14_dex_unknown");
  {  // ko11.31: centro pokemon (30 s)
    galleryOpen = false; galleryDetail = 0; galleryPmd.unload();
    closeAll();
    openRegionPick(); render(); shot("15c_region_center_btn");
    onTap(RG_CTR_X + 20, RG_BACK_Y + 20);
    navCheck("region: [포켓몬센터] abre el centro", xScreen == XS_CENTER);
    pet.pp[0] = 0; pet.pp[1] = 3;
    render(); shot("50_center");
    onTap(110 + 20, CTR_BTN_Y + 20);
    navCheck("centro: [회복하기] empieza a curar", centerUntil != 0);
    navCheck("centro: mientras cura no se lucha", !battleAllowed(false));
    tick(9000); render(); shot("50b_center_healing");
    tick(22000); centerPoll();
    navCheck("centro: a los 30 s, PP llenos", centerUntil == 0 && pet.ppFull());
    render(); shot("50c_center_done");
    closeAll();
  }
  // ko10: gen 2 en la pokedex
  galleryDetail = 0; galleryPage = 10; galleryDirty = true;
  for (int16_t d : { 172, 175, 176, 179, 181, 196, 197, 208, 212 }) dexLog.seen(d, gMockEpoch);
  render(); shot("30_dex_gen2_grid");
  galleryPage = 9; galleryDirty = true; render(); shot("31b_dex_gen1_last");
  galleryPage = GAL_PAGES - 1; galleryDirty = true; render(); shot("31_dex_gen2_last");
  galleryDetail = 197; galleryPmd.load(197, false); render(); shot("32_dex_umbreon");
  galleryDetail = 0; galleryPage = 0;
  // sonido y hora
  closeAll(); openClock(); setBigPart("nvs2"); render(); shot("15z_clock_nvs2"); setBigPart(nullptr); render(); shot("15_clock_settings");
  clockDateMode = true; render(); shot("15b_clock_date"); clockDateMode = false;
  { extern int gMockBatPct; extern bool gMockCharging; closeAll(); gMockBatPct = 100; render(); shot("01m_main_bat100"); gMockBatPct = 78;  // ko11.27: placa de la bateria
    gMockCharging = true; render(); shot("01n_main_bat_charging"); gMockCharging = false;  // ko11.28: cargando (verde oscuro)
    gMockBatPct = 25; render(); shot("01o_main_bat_low"); gMockBatPct = 78; }
  closeAll(); openSettings(); setBigPart("nvs2"); render(); shot("14_settings"); setBigPart(nullptr);  // ko11.26
  closeAll(); openSound(); render(); shot("16_sound");
  { bool a0 = audioEnabled();  // ko11.25: [진동] junto a [소리]; ko11.26: 꺼짐 -> 약 -> 중 -> 강
    vibSetEnabled(true); vibSetLevel(2);
    soundTap(SND_BTN_X1 + 70, 96); render(); shot("16g_sound_vib_off");
    navCheck("sonido: [진동 강] -> apagada (no el sonido)", !vibEnabled() && audioEnabled() == a0);
    soundTap(SND_BTN_X1 + 70, 96); render(); shot("16h_sound_vib_weak");
    navCheck("sonido: apagada -> [진동 약]", vibEnabled() && vibLevel() == 0);
    soundTap(SND_BTN_X1 + 70, 96);
    navCheck("sonido: [진동 약] -> [진동 중]", vibEnabled() && vibLevel() == 1);
    soundTap(SND_BTN_X1 + 70, 96);
    navCheck("sonido: [진동 중] -> [진동 강]", vibEnabled() && vibLevel() == 2); }
  xScreen = XS_BRIGHT; render(); shot("16d_brightness");  // ko11.18
  brightTap(345, 230); render(); shot("16e_brightness_up"); setBrightLevel(7);
  brightTap(290, 338); render(); shot("16f_charge_limit");  // ko11.23.3
  navCheck("brillo: [약 90%] = limite de carga", chargeCap90() && xScreen == XS_BRIGHT);
  brightTap(130, 338);
  navCheck("brillo: [100%] = sin limite", !chargeCap90());
  closeAll(); openBgmPick(); render(); shot("16b_bgm_pick");  // ko11.8
  audioSetBgmMask(0x02); render(); shot("16c_bgm_pick_one"); audioSetBgmMask(0xFF);
  closeAll(); openNet(); render(); shot("18_net");
  { gMockPortal = true; render(); shot("18b_portal_qr"); gMockPortal = false; }
  // ko11.6: copia de la partida en la SD
  openBackup(); render(); shot("18c_backup");
  bakSel = 1; render(); shot("18d_backup_confirm"); bakSel = -1;
  backupTap(233, BAK_NOW_Y + 20); render(); shot("18h_backup_done");  // ko11.19.1: "백업했어요!" (antes salia otro texto)
  navCheck("backup: aviso = hecho", bakMsg == X_BAK_DONE);
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

// ko11.17: [<] vuelve a la pantalla desde la que se abrio cada menu
static void navChecks() {
  const int LX = 20, LY = 200;  // flecha izquierda
  closeAll(); cardOpen = true; cardPage = 3; openBox(); boxTap(LX, LY);
  navCheck("ficha -> caja -> [<]", xScreen == XS_NONE && cardOpen && cardPage == 3);
  closeAll(); cardOpen = true; cardPage = 4; openCandyBag(); candyBagTap(LX, LY);
  navCheck("ficha -> caramelos -> [<]", xScreen == XS_NONE && cardOpen && cardPage == 4);
  closeAll(); openBox(); boxTap(LX, LY);
  navCheck("principal -> caja -> [<]", xScreen == XS_NONE && !cardOpen && !trainMenuOpen);
  closeAll(); cardOpen = true; cardPage = 1; openTrainMenu(); tick(3000); trainMenuTap(LX, LY);
  navCheck("ficha -> entrenamiento -> [<]", !trainMenuOpen && cardOpen && cardPage == 1);
  closeAll(); openTrainMenu(); trainMenuPage = 1; tick(3000); pet.energy = 80;
  openRegionPick(); bool reg = xScreen == XS_REGION; regionTap(LX, LY);
  navCheck("entrenamiento(batallas) -> region -> [<]", reg && xScreen == XS_NONE && trainMenuOpen && trainMenuPage == 1);
  { uint8_t b0 = pet.badges; pet.badges = 0xFF; closeAll(); openGyms();
    navCheck("gimnasios con 8 medallas: empieza en la 1a pagina", gymPage == 0); pet.badges = b0; }
  closeAll(); openTrainMenu(); trainMenuPage = 1; openGyms(); gymPage = 0; gymTap(LX, LY);
  navCheck("entrenamiento -> gimnasios -> [<]", xScreen == XS_NONE && trainMenuOpen && trainMenuPage == 1);
  closeAll(); openTrainMenu(); trainMenuPage = 1; openDaily(); dailyTap(LX, LY);
  navCheck("entrenamiento -> reto -> [<]", xScreen == XS_NONE && trainMenuOpen);
  closeAll(); openTrainMenu(); trainMenuPage = 1; openLinkMenu(); linkMenuTap(LX, LY);
  navCheck("entrenamiento -> tongsin -> [<]", xScreen == XS_NONE && trainMenuOpen);
  closeAll(); cardOpen = true; cardPage = 0; openLinkMenu(); linkMenuTap(LX, LY);
  navCheck("ficha -> tongsin -> [<]", xScreen == XS_NONE && cardOpen);
  // ko11.26: menu de ajustes (flecha de arriba); cada boton y su [<]
  const int SX[2] = { 147, 319 };
  auto setBtn = [&](int i) { settingsTap(i == 10 ? SET_XC + SET_W / 2 : SX[i % 2], SET_Y0 + (i / 2) * SET_DY + SET_H / 2); };  // ko12.6: 6 filas
  closeAll(); tick(3000); onTap(233, 20);
  navCheck("principal: flecha de arriba = ajustes", xScreen == XS_SET && !clockOpen);
  closeAll(); onSwipeV(1);
  navCheck("principal: deslizar abajo = ajustes", xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(0);
  navCheck("ajustes -> [시간] abre la hora", clockOpen && xScreen == XS_NONE);
  clockTap(LX, LY);
  navCheck("ajustes -> hora -> [<]", !clockOpen && xScreen == XS_SET);
  setBtn(0); clockTap(233, 316 + 20);
  navCheck("ajustes -> hora -> [OK]", !clockOpen && xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(1); bool snd = xScreen == XS_VOL; soundTap(LX, LY);
  navCheck("ajustes -> sonido -> [<]", snd && xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(1); openBgmPick(); bgmPickTap(LX, LY);
  navCheck("sonido -> fondos -> [<]", xScreen == XS_VOL);
  soundTap(LX, LY);
  navCheck("... -> sonido -> [<] = ajustes", xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(2); bool br = xScreen == XS_BRIGHT; brightTap(LX, LY);
  navCheck("ajustes -> brillo -> [<]", br && xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(3); bool nt = xScreen == XS_NET; netTap(LX, LY);
  navCheck("ajustes -> red -> [<]", nt && xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(4); bool up = xScreen == XS_UPD; updateTap(LX, LY);
  navCheck("ajustes -> SD update -> [<]", up && xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(5); bool bk = xScreen == XS_BAK; backupTap(LX, LY);
  navCheck("ajustes -> copias -> [<]", bk && xScreen == XS_SET);
  { Lang l0 = gLang; closeAll(); openSettings(); setBtn(6);
    navCheck("ajustes -> [언어] cambia el idioma", gLang != l0 && xScreen == XS_SET);
    while (gLang != l0) setBtn(6); }
  closeAll(); openSettings(); render(); shot("90_settings_ko126");
  setBtn(10); bool bdo = xScreen == XS_BDAY;
  bdayTap(306 + 30, BD_ROW1 + 28); render(); shot("90b_birthday_set");
  bdayTap(96 + 60, 304 + 25);
  navCheck("ajustes -> [내 생일] -> +1 mes -> [저장] = ajustes", bdo && xScreen == XS_SET && pet.bdayM != 0);
  pet.setBirthday(0, 0);
  {  // ko12.8: SD 파일 묶기 (mons.pak)
    closeAll(); openSettings(); setBtn(4); render(); shot("91_update_pakbtn");
    updateTap(113 + 100, UPD_PAK_Y + 20);
    navCheck("SD 업데이트 -> [SD 파일 하나로 묶기]", xScreen == XS_PAK);
    render(); shot("91b_pak");
    pakProgress(120ull << 20, 268ull << 20, 450, 1007); shot("91c_pak_progress");
    pakTap(113 + 100, 320);
    navCheck("묶기: 끝나면 완료 표시", xScreen == XS_PAK && pakResult == 0);
    render(); shot("91d_pak_done");
    pakTap(LX, LY);
    navCheck("묶기 -> [<] = SD 업데이트", xScreen == XS_UPD);
  }
  pet.endSeen = 1; closeAll(); openSettings(); render(); shot("90c_settings_master");
  settingsTap(236 + 60, SET_Y0 + 5 * SET_DY + 20);
  navCheck("ajustes -> [엔딩 다시 보기]", xScreen == XS_ENDING && endReplay);
  closeAll(); openSettings(); settingsTap(100 + 60, SET_Y0 + 5 * SET_DY + 20);
  navCheck("ajustes (maestro) -> [내 생일]", xScreen == XS_BDAY);
  pet.endSeen = 0;
  closeAll(); openSettings(); setBtn(10); bdayTap(LX, LY);
  navCheck("ajustes -> cumpleanos -> [<] = ajustes", xScreen == XS_SET);
  closeAll(); openSettings(); setBtn(7); bool rs = xScreen == XS_RESET; resetTap(LX, LY);
  navCheck("ajustes -> reinicio -> [<]", rs && xScreen == XS_SET);
  settingsTap(LX, LY);
  navCheck("ajustes -> [<] = principal", xScreen == XS_NONE && !clockOpen);
  // ko11.26: la caja tiene la pestana de caramelos
  closeAll(); openBox(); boxTap(342 + 21, 48 + 19);
  navCheck("caja -> [사탕] abre la bolsa", xScreen == XS_CANDY);
  candyBagTap(LX, LY);
  navCheck("caja -> caramelos -> [<] = caja", xScreen == XS_BOX);
  closeAll(); openBox(); boxTap(342 + 21, 48 + 19); onSwipeV(-1);
  navCheck("caja -> caramelos -> deslizar = caja", xScreen == XS_BOX);
  // ko11.26: la ficha de combate ya no tiene botones (tocar = cerrar)
  closeAll(); cardOpen = true; cardPage = 1; onTap(150, 240);
  navCheck("ficha combate: sin botones", !cardOpen && xScreen == XS_NONE && !trainMenuOpen);
  closeAll(); cardOpen = true; cardPage = 0; onTap(LX, LY);
  navCheck("ficha (1a pagina) -> [<]", !cardOpen);
  closeAll(); tick(3000);
}

// ko11.21: modo historia
static void sceneTo(int steps) {  // avanza N toques mostrando el texto entero
  for (int i = 0; i < steps; i++) { stTypeT = millis() - 600000UL; storySceneTap(233, 330); }
  stTypeT = millis() - 600000UL;
}
static bool sceneUntil(bool (*cond)()) {  // toca hasta que se cumpla (o 40 toques)
  for (int i = 0; i < 40 && !cond(); i++) sceneTo(1);
  return cond();
}
// ko11.23: juega un capitulo entero (elige la 1a opcion, gana los combates)
static bool gPickShot = false;
static bool playChapter(uint8_t st, uint8_t c) {
  stResStyle = 0xFF;
  stStart(st, c);
  for (int g = 0; g < 600 && !stEnd; g++) {
    stTypeT = millis() - 600000UL;
    pet.energy = 100; pet.fullness = 100;
    if (xScreen == XS_WILD && bKind == BK_STORY) {  // combate en marcha: ganarlo
      if (pN > PARTY_MAX || pN < 1) return false;
      bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
      finishBattle(true, false, false); afterResult();
      continue;
    }
    if (stPicking) {
      if (!gPickShot) { gPickShot = true; render(); shot("86_story_pick"); }
      storySceneTap(CX, ST_BOX_Y + 70);  // 출발
      if (xScreen != XS_WILD) { printf("  capitulo %d/%d: 출발 no empieza\n", st, c); return false; }
      if (pN != 3) { printf("  capitulo %d/%d: equipo %d\n", st, c, pN); return false; }
      continue;
    }
    if (stChoice) {
      if (stOptN == 3) storySceneTap(ST_BOX_X + 60, ST_BOX_Y + 70);
      else storySceneTap(233, ST_BOX_Y + 56);
      continue;
    }
    if (stBattleWait) {
      storySceneTap(233, 330);
      if (stPicking) continue;
      if (xScreen != XS_WILD || bKind != BK_STORY) { printf("  capitulo %d/%d: no empieza el combate\n", st, c); return false; }
      if (pN > PARTY_MAX || pN < 1) return false;
      bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
      finishBattle(true, false, false); afterResult();
      continue;
    }
    if (stTired) { printf("  capitulo %d/%d: cansado\n", st, c); return false; }
    storySceneTap(233, 330);
  }
  return stEnd && ((stDone[st] >> c) & 1);
}
// ko11.28: 수련 전투 (companero de la historia / inicial de la expedicion)
static void trainWin(bool won) {
  pet.energy = 100; pet.fullness = 100;
  storyTrainTap(233, TR_GO_Y + 20);
  if (xScreen != XS_WILD) return;
  bPhase = BP_MENU; bTeamI = bTeamN - 1;
  if (won) bFoe.hp = 0; else bMe.hp = 0;
  finishBattle(won, false, false); afterResult();
}
static void storyTrainShots() {
  setLang(LANG_KO); applyLangFont();
  closeAll(); pet.energy = 100;
  int16_t pd0 = stPDex[0]; uint32_t pe0 = stPExp[0];
  if (stPDex[0] <= 0) stPDex[0] = 7;  // ya eligio companero (꼬부기)
  openStory(); render(); shot("70b_story_menu_train");
  storyMenuTap(200, ST_CARD_Y + 3 * (ST_CARD_H + ST_CARD_GAP) + 20);
  navCheck("수련: menu -> pantalla", xScreen == XS_STRAIN);
  storyTrainTap(200, TR_ROW_Y + 20);  // juego
  uint16_t lv0 = stTrainLv(0);
  render(); shot("88_story_train");
  pet.energy = 100;
  storyTrainTap(233, TR_GO_Y + 20);
  navCheck("수련: empieza un combate solo con el companero", xScreen == XS_WILD && bKind == BK_STORY && stTraining == 1 &&
           pN == 1 && bMe.dex == stTrainDex(0) && bMe.lvl == lv0);
  render(); shot("88b_story_train_battle");
  bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0; finishBattle(true, false, false); afterResult();
  navCheck("수련: ganar vuelve a la pantalla", xScreen == XS_STRAIN && stTraining == 0 && stTrainPct(0) > 0);
  render(); shot("88c_story_train_win");
  trainWin(true); trainWin(true);
  navCheck("수련: 3 victorias = +1 nivel", stTrainLv(0) == lv0 + 1);
  render(); shot("88d_story_train_lvup");
  uint16_t lv1 = stTrainLv(0); uint8_t pc = stTrainPct(0);
  trainWin(false);
  navCheck("수련: perder no cambia nada", xScreen == XS_STRAIN && stTrainLv(0) == lv1 && stTrainPct(0) == pc);
  // expedicion: el entrenado sube el inicial, no a los rivales
  storyTrainTap(200, TR_ROW_Y + 2 * (TR_ROW_H + TR_ROW_GAP) + 20);
  uint16_t r0 = stTrainLv(2), w0 = rgWaveLv();
  trainWin(true); trainWin(true); trainWin(true);
  navCheck("수련: expedicion +1 nivel", stTrainLv(2) == r0 + 1 && rgWaveLv() == w0 && rgPartnerLv() >= stTrainLv(2) - 1);
  storyTrainTap(20, 200);
  navCheck("수련: [<] = menu de la historia", xScreen == XS_STORY);
  stPDex[0] = pd0; stPExp[0] = pe0; rgTrainExp = 0; stTraining = 0;
  closeAll();
}

// ko11.30: escena de evolucion del companero al empezar un capitulo
static void storyEvoShots() {
  setLang(LANG_KO); applyLangFont();
  closeAll(); pet.energy = 100;
  int16_t pd = stPDex[0], sp = stSeenP[0]; uint32_t pe = stPExp[0]; uint8_t rs = stResStyle;
  stPDex[0] = 7; stPExp[0] = 0; stSeenP[0] = 8; stResStyle = 0xFF;  // visto: 어니부기
  stStart(0, 8);  // 9장: minimo Lv.38 -> 거북왕
  navCheck("evolucion: al empezar el capitulo, escena 어니부기 -> 거북왕", stEvoN == 1 && stEvoFrom[0] == 8 && stEvoTo[0] == 9);
  stTypeT = millis() - 600000UL; render(); shot("89_story_evo_1");
  storySceneTap(233, 330);
  stTypeT = millis() - 600000UL; render(); shot("89b_story_evo_2");
  navCheck("evolucion: 2a frase = 진화했다", stEvoN == 1 && stEvoPhase == 1 && strstr(stText, "거북왕으로 진화했다"));
  storySceneTap(233, 330);
  navCheck("evolucion: luego sigue el capitulo", stEvoN == 0 && xScreen == XS_SCENE && stSeenP[0] == 9);
  stStart(0, 8);
  navCheck("evolucion: no se repite", stEvoN == 0);
  stPDex[0] = pd; stSeenP[0] = sp; stPExp[0] = pe; stResStyle = rs;
  closeAll();
}

static void storyFullRun() {
  stRestartStyle(0); stRestartStyle(1);
  bool ok = true;
  for (uint8_t st = 0; st < STORY_STYLES; st++)
    for (uint8_t c = 0; c < STORY_NCH[st]; c++)
      if (!playChapter(st, c)) { ok = false; printf("  FALLO: estilo %d capitulo %d\n", st, c + 1); }
  navCheck("historia: juego 14 capitulos y anime 15 hasta el final", ok && stDoneCount(0) == 14 && stDoneCount(1) == 15);
  int nj = 0;
  for (int i = 0; i < STORY_JOIN_MAX; i++) nj += stJ[0][i] > 0;
  navCheck("historia: juego, 5 se unieron (구구 피피 이브이 라프라스 잠만보)", nj == 5);
  navCheck("historia: juego, el inicial evoluciono (이상해꽃)", stPDex[0] == 1 && stPartnerDex(0) == 3);
  nj = 0;
  bool butterfree = false, bulbaKeep = false;
  for (int i = 0; i < STORY_JOIN_MAX; i++) {
    nj += stJ[1][i] > 0;
    if ((stJ[1][i] & ~JOIN_KEEP) == 10) butterfree = true;
    if (stJ[1][i] == (1 | JOIN_KEEP)) bulbaKeep = true;
  }
  navCheck("historia: anime, 버터플 se fue y quedan 3", nj == 3 && !butterfree);
  navCheck("historia: anime, 이상해씨 no evoluciona", bulbaKeep);
  stStyle = 1; stCh = 14;
  int16_t cz = 0;
  for (int i = 0; i < STORY_JOIN_MAX; i++) if ((stJ[1][i] & ~JOIN_KEEP) == 4) cz = stJoinDex(stJ[1][i]);
  navCheck("historia: anime, 파이리 ya es 리자몽", cz == 6);
  navCheck("historia: anime, 피카츄 sigue siendo 피카츄", stPartnerDex(1) == 25);
  render(); shot("87_scene_ending");
  xScreen = XS_STORY; render(); shot("70b_story_menu_complete");
  stStyle = 0; stChPage = 2; xScreen = XS_STORYCH; render(); shot("71d_story_chapters_p3");
  storyChaptersTap(ST_PG_LX + 20, ST_RESTART_Y + 18);
  navCheck("historia: pagina anterior", stChPage == 1);
  // un retrato nuevo (칸나)
  stResStyle = 0xFF; stStart(0, 12);
  for (int g = 0; g < 40 && stWho != W_LORELEI; g++) { stTypeT = millis() - 600000UL; storySceneTap(233, 330); }
  stTypeT = millis() - 600000UL; render(); shot("88_scene_lorelei");
  stRestartStyle(0); stRestartStyle(1);
}

static void storyShots() {
  setLang(LANG_KO);
  applyLangFont();
  closeAll(); pet.energy = 80;
  openStory(); render(); shot("70_story_menu");
  navCheck("historia: menu", xScreen == XS_STORY);
  storyMenuTap(200, ST_CARD_Y + 20);
  render(); shot("71_story_chapters_game");
  navCheck("historia: capitulos", xScreen == XS_STORYCH && stStyle == 0);
  storyChaptersTap(200, ST_ROW_Y + 10);  // 1장
  navCheck("historia: escena", xScreen == XS_SCENE);
  tick(300); render(); shot("72_scene_typing");
  sceneTo(0); render(); shot("72b_scene_narr");
  sceneTo(1); render(); shot("73_scene_oak");
  sceneTo(2); render(); shot("74_scene_pet");
  stPDex[0] = 0; stPExp[0] = 0;
  sceneUntil([] { return stChoice; }); render(); shot("74b_scene_starter");
  navCheck("historia: Oak deja elegir entre 3", stChoice && stOptN == 3);
  storySceneTap(ST_BOX_X + 12 + 2 * 118 + 50, ST_BOX_Y + 70);  // 꼬부기
  navCheck("historia: companero elegido", stPDex[0] == 7);
  sceneTo(0); render(); shot("74c_scene_partner");
  navCheck("historia: el companero no sale dos veces (stMon = companero)", stMon == 7);
  sceneTo(1); render(); shot("75_scene_rival");
  sceneUntil([] { return stBattleWait; }); render(); shot("76_scene_battle_wait");
  navCheck("historia: espera el combate", stBattleWait);
  storySceneTap(233, 330);  // -> combate (o elegir ayudantes)
  if (xScreen == XS_PARTY) partyStart(true);
  navCheck("historia: combate", xScreen == XS_WILD && bKind == BK_STORY);
  navCheck("historia: lucha el companero (no el que crias)", bMe.dex == 7 && bMe.lvl == 5 && !pSlot0Pet);
  navCheck("historia: el rival lleva la ventaja", bFoe.dex == 1);
  { Battler ref = makeTrainerMon(1, 5);
    navCheck("historia: rival vida -15%, ataque -10%", bFoe.maxHp == ref.maxHp * 85 / 100 && bFoe.atk == ref.atk * 90 / 100 && bFoe.def == ref.def * 85 / 100); }
  tick(2300); updateWild(); render(); shot("77_story_battle");
  // ko11.23.1: [◀] en el combate: 1er toque avisa, 2o sale (sin derrota)
  { uint8_t lw = stDone[0];
    wildTap(20, NAV_Y); render(); shot("77b_story_battle_quit_armed");
    navCheck("historia: [◀] en combate, 1er toque sigue en el combate", xScreen == XS_WILD);
    wildTap(20, NAV_Y);
    navCheck("historia: [◀] 2o toque vuelve a la escena (espera el combate)", xScreen == XS_SCENE && stBattleWait && pN == 1 && stDone[0] == lw);
    stTypeT = millis() - 600000UL; render(); shot("77c_story_battle_quit_scene");
    stTypeT = millis() - 600000UL; storySceneTap(233, 330);
    navCheck("historia: tras salir, tocar = el mismo combate otra vez", xScreen == XS_WILD && bKind == BK_STORY);
    tick(2300); updateWild(); }
  bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
  finishBattle(true, false, false); afterResult();
  navCheck("historia: vuelve a la escena", xScreen == XS_SCENE);
  navCheck("historia: el companero sube de nivel", stPartnerLv(0) == 6 && pSlot0Pet);
  sceneTo(0); render(); shot("78_scene_after");
  sceneUntil([] { return stChoice; }); render(); shot("79_scene_choice");
  navCheck("historia: eleccion", stChoice);
  storySceneTap(ST_BOX_X + 60, ST_BOX_Y + 56);
  sceneTo(0); render(); shot("79b_scene_choice_a");
  sceneUntil([] { return stEnd; }); render(); shot("79c_scene_clear");
  navCheck("historia: capitulo 1 superado", stDone[0] & 1);
  sceneTo(1); render(); shot("71b_story_chapters_after");
  storyChaptersTap(CX, ST_RESTART_Y + 18); render(); shot("71c_story_restart_armed");
  navCheck("historia: [처음부터] 1er toque no borra", stDone[0] & 1);
  storyChaptersTap(CX, ST_RESTART_Y + 18);
  navCheck("historia: [처음부터] 2o toque borra el estilo", stDone[0] == 0 && stPDex[0] == 0 && stJ[0][0] == 0);
  stDone[0] = 1; stPDex[0] = 7; stPExp[0] = expForLevel(6);
  // ko11.22: 2장: 구구가 동료가 되고, 웅 전에는 동료와 함께 (보관함 도우미 없이)
  stJ[0][0] = stJ[0][1] = 0; stResStyle = 0xFF;
  stStart(0, 1);
  sceneUntil([] { return stJ[0][0] == 16; }); render(); shot("79d_scene_join");
  navCheck("historia: 구구 se une", stJ[0][0] == 16);
  sceneUntil([] { return stBattleWait; });
  storySceneTap(233, 330);
  navCheck("historia: combate directo (sin elegir ayudantes de la caja)", xScreen == XS_WILD && bKind == BK_STORY);
  navCheck("historia: equipo = companero + 구구", pN == 2 && pBox[1] == -1 && pMon[1].dex == 16 && !pSlot0Pet);
  tick(2300); updateWild(); render(); shot("79e_story_battle_team");
  bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0;
  finishBattle(true, false, false); afterResult();
  navCheck("historia: tras el combate, equipo deshecho", pN == 1 && xScreen == XS_SCENE);
  stJ[0][0] = 0; stResStyle = 0xFF;
  // anime: 2화 (로켓단)
  stDone[1] = 1; stPDex[1] = 25; stStart(1, 1);
  sceneTo(1); render(); shot("80_anime_rocket");
  sceneTo(2); render(); shot("80b_anime_rocket2");
  stDone[1] = 0; stDone[0] = 0; stResStyle = 0xFF;
  storyFullRun();
  storyTrainShots();  // ko11.28
  storyEvoShots();  // ko11.30
  stDone[1] = 0; stDone[0] = 0; stResStyle = 0xFF;
  // expedicion
  xScreen = XS_STORY; storyMenuTap(200, ST_CARD_Y + 2 * (ST_CARD_H + ST_CARD_GAP) + 20);
  render(); shot("81_rogue_hub");
  rogueTap(233, RG_BTN_Y + 20);
  render(); shot("81b_rogue_pick");
  navCheck("expedicion: elegir inicial", rgPhase == RG_PICK);
  rogueTap(RG_PICK_X + 3 * RG_PICK_STEP + 30, 230);  // 피카츄
  if (xScreen == XS_PARTY) partyStart(true);
  navCheck("expedicion: oleada 1", xScreen == XS_WILD && bKind == BK_ROGUE);
  navCheck("expedicion: lucha el inicial elegido", bMe.dex == 25 && bMe.lvl == 5);
  tick(2300); updateWild();
  wildTap(20, NAV_Y); wildTap(20, NAV_Y);
  navCheck("expedicion: [◀] x2 vuelve al campamento sin perder la oleada", xScreen == XS_ROGUE && rgOn && rgWave == 1 && rgPhase == RG_HUB);
  rogueTap(100, RG_BTN_Y + 20);  // 이어하기
  navCheck("expedicion: 이어하기 tras salir", xScreen == XS_WILD && bKind == BK_ROGUE);
  tick(2300); updateWild(); render(); shot("82_rogue_wave1");
  bPhase = BP_MENU; bTeamI = bTeamN - 1; bFoe.hp = 0; bMe.hp = bMe.maxHp / 2;
  finishBattle(true, false, false); afterResult();
  render(); shot("83_rogue_reward");
  navCheck("expedicion: premio", xScreen == XS_ROGUE && rgPhase == RG_REWARD && rgWave == 2);
  rogueTap(100, 240);  // curar
  navCheck("expedicion: oleada 2 con la vida guardada", xScreen == XS_WILD && bMe.hp < bMe.maxHp);
  bPhase = BP_MENU; bMe.hp = 0; bTeamI = 0; bFoe.hp = bFoe.maxHp;
  finishBattle(false, false, false); afterResult();
  render(); shot("84_rogue_over");
  navCheck("expedicion: fin", rgPhase == RG_OVER && !rgOn);
  rgPhase = RG_HUB;
  closeAll(); endBattleScreen();
  // pagina de batallas con el boton de la historia
  closeAll(); trainMenuOpen = true; trainMenuPage = 1; render(); shot("52_battle_page"); trainMenuOpen = false;
}

#include "preview_ideas.inc"  // ko12.4: maquetas de ideas

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
  portraits.load();  // ko11.21: retratos de la historia (story.bin en la SD de prueba)
  pet.syncClock(gMockEpoch);
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.eggTap(); pet.eggTap(); pet.eggTap();
  pet.exp = expForLevel(18) + 1200;  // Lv.18
  pet.fullness = 72; pet.joy = 88; pet.energy = 54; pet.hygiene = 23;
  pet.balls = 5; pet.potions = 2;
  pet.bgAsked = 1;  // ko12.4.1: el aviso de elegir fondo tiene su propia captura
  ensureMon();
  navChecks();
  previewShots();
  storyShots();
  scenes(true, "");
  scenes(false, "_en");
  return 0;
}

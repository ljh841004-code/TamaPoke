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
  defOpen = spdOpen = sackOpen = gameOpen = false;
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
  // ficha: combate
  cardOpen = true; cardPage = 1;
  render(); shot("03_card_battle");
  pet.exp = expForLevel(17) + 400;  // fork KO (ko7): barra de EXP
  cardPage = 3;
  render(); shot("03b_card_progress");
  pet.berryKnown = true; pet.ageMinutes = 2 * 1440 + 300; pet.bond = 46; pet.streak = 3; pet.bestStreak = 5;
  cardPage = 0; render(); shot("03c_card_profile");
  strcpy(pet.nick, "불꽃이"); render(); shot("03e_card_profile_nick"); pet.nick[0] = 0;
  cardPage = 2; render(); shot("03d_card_medals");
  closeAll(); pet.berryKnown = true; feedMenuUntil = gMockMillis + 5000; render(); shot("01b_feed_menu");
  closeAll(); confirmUntil = gMockMillis + 5000; render(); shot("01c_confirm_release"); confirmUntil = 0;
  closeAll(); openLinkMenu(); render(); shot("24_link_menu");
  closeAll(); cardOpen = true; cardPage = 0; openKeyboard(); nameBuf[0] = 0; nameLen = 0;
  for (uint8_t k : { CJI_K_B, CJI_K_B, CJI_K_I, CJI_K_DOT, CJI_K_O, CJI_K_I, CJI_K_N, CJI_K_N })
    cjiPress(kbCji, k);
  render(); shot("27_keyboard_ko");
  kbCommit(); kbKo = false; render(); shot("28_keyboard_abc");
  kbOpen = false;
  closeAll(); startSack(); tick(300); render(); shot("25_sack");
  closeAll(); startGame(); tick(300); render(); shot("26_ball_game");
  closeAll();
  // entrenamiento
  closeAll(); openTrainMenu();
  render(); shot("04_train_menu");
  trainMenuPage = 1; render(); shot("04b_train_menu_battle"); trainMenuPage = 0;
  closeAll(); startDefense();
  for (int i = 0; i < 70; i++) { tick(85); render(); }
  defensePress((int16_t)defBall[0].x, (int16_t)defBall[0].y);
  tick(85); render(); shot("05_train_defense");
  gMockMillis += DEF_MS; render(); tick(85); render(); shot("06_train_defense_result");
  closeAll(); startSpeed();
  for (int i = 0; i < 400 && spdPhase != SP_SHOW; i++) { tick(20); render(); }
  tick(200); render(); shot("07_train_speed");
  // batalla salvaje
  closeAll(); pet.energy = 80; startWild();
  bPhase = BP_MENU; txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
  render(); shot("08_battle_menu");
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
  bq[0].crit = true; bq[0].eff = 4; bq[0].move = BA_TACKLE; bqT = gMockMillis;
  gMockMillis = bqT + 480; render(); shot("fx_crit_super");
  bPhase = BP_MENU; bqN = 0;
  bvFoeHp = bvFoeTgt = bFoe.hp = bFoe.maxHp / 3;
  finishBattle(false, false, true);
  bvFoeCaught = true;
  render(); shot("09_battle_caught");
  bPhase = BP_NEXT; bPhaseT = gMockMillis; render(); shot("09b_battle_next");
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
      { 1772459100u, "rain" }, { 1767275100u, "snow" }, { 1780321500u, "sunny" },
      { 1772459100u + 9 * 3600, "night_clear" }, { 1775137500u, "blossom" }, { 1791035100u, "leaves" },
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
  // ko10.1: elegir region y encuentros
  {
    closeAll(); pet.energy = 80;
    openRegionPick(); regionPage = 0; render(); shot("44_region_pick"); regionPage = 1; render(); shot("44b_region_pick2");
    gMockEpoch = 1772459100u; pet.lastSeenEpoch = gMockEpoch;  // lluvia (marzo)
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
  openBox(); render(); shot("10_box");
  boxSel = 1; render(); shot("11_box_detail");
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
  galleryDetail = 0; galleryPage = 11; galleryDirty = true;
  for (int16_t d : { 172, 175, 176, 179, 181, 196, 197, 208, 212 }) dexLog.seen(d, gMockEpoch);
  render(); shot("30_dex_gen2_grid");
  galleryPage = 15; galleryDirty = true; render(); shot("31_dex_gen2_last");
  galleryDetail = 197; galleryPmd.load(197, false); render(); shot("32_dex_umbreon");
  galleryDetail = 0; galleryPage = 0;
  // sonido y hora
  closeAll(); openClock(); render(); shot("15_clock_settings");
  closeAll(); openSound(); render(); shot("16_sound");
  closeAll(); openNet(); render(); shot("18_net");
  { gMockPortal = true; render(); shot("18b_portal_qr"); gMockPortal = false; }
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
  pet.nextPetHook = nextFromBox;
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

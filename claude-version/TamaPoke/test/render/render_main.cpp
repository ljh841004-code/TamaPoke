// test/render: dibuja pantallas del firmware en el PC y las guarda como .raw
// (RGB565 466x466). run.sh las convierte a PNG. Usa el MISMO codigo de dibujo
// que la placa (Arduino_GFX real); solo el hardware es de mentira (stubs.cpp).
#include "build/sketch.cpp"
#include <sys/stat.h>

extern uint32_t gMockMillis, gMockEpoch;
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
  // entrenamiento
  closeAll(); openTrainMenu();
  render(); shot("04_train_menu");
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
  bvFoeHp = bvFoeTgt = bFoe.hp = bFoe.maxHp / 3;
  finishBattle(false, false, true);
  bvFoeCaught = true;
  render(); shot("09_battle_caught");
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
  // sonido y hora
  closeAll(); openClock(); render(); shot("15_clock_settings");
  closeAll(); openSound(); render(); shot("16_sound");
  closeAll(); openNet(); render(); shot("18_net");
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
  pet.ageMinutes = 17 * MINUTES_PER_LEVEL + 20;  // Lv.18
  pet.fullness = 72; pet.joy = 88; pet.energy = 54; pet.hygiene = 23;
  pet.balls = 5; pet.potions = 2;
  ensureMon();
  scenes(true, "");
  scenes(false, "_en");
  return 0;
}

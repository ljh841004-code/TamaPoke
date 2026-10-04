// test/render/perf_main.cpp (ko12.1): cuanto cuesta cada parte del combate en el PC.
// No es el tiempo de la placa, pero si el reparto: lo que pesa aqui pesa alli.
//   test/render/perf.sh <carpeta_sd>
#include "build/sketch.cpp"
#include <chrono>

extern uint32_t gMockMillis, gMockEpoch;
extern const char *gSdRoot;

template <class F> static double timeIt(const char *name, F f, int n = 300) {
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < n; i++) { gMockMillis += 33; f(); }
  double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / n;
  printf("  %-22s %8.1f us\n", name, us);
  return us;
}

int main(int argc, char **argv) {
  gSdRoot = argc > 1 ? argv[1] : "sd";
  gfx->begin();
  pet.begin(); box.begin(); dexLog.begin();
  sdBegin(); thumbs.load();
  pet.syncClock(gMockEpoch);
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.eggTap(); pet.eggTap(); pet.eggTap();
  pet.exp = expForLevel(30);
  ensureMon();
  for (int art = 0; art < 2; art++) {
    gBattleArt = art;
    startWild();
    bPhase = BP_MENU;
    txFmt(bvL1, sizeof(bvL1), X_WHAT_DO, bvMeName);
    printf("== combate (%s), region %u, hora %d\n", art ? "PokeRogue" : "PMD", (unsigned)bRegion, sceneHour());
    int hh = sceneHour(); bool night = hh < 6 || hh >= 20; uint8_t wx = sceneWeather();
    double total = timeIt("renderBattleView", [] { renderBattleView(); });
    double bg = timeIt("drawBattleBg", [] { drawBattleBg(); });
    timeIt("  drawSky", [&] { drawSky(150, hh, night, wx, millis(), false); });
    timeIt("  drawBiome", [&] { drawBiome(bRegion, 150, 262, millis(), night, wx); });
    timeIt("  drawWeather", [&] { drawWeather(wx, 262, millis(), night); });
    double bat = timeIt("drawBattlers", [] { drawBattlers(); });
    double box2 = timeIt("drawHpBox x2", [] {
      drawHpBox(84, 50, 176, bvFoeName, bvFoeLvl, bvFoeHp, bvFoeMax, true, 0);
      drawHpBox(236, 176, 176, bvMeName, bvMeLvl, bvMeHp, bvMeMax, true, 0);
    });
    double menu = timeIt("drawBattleMenu", [] { drawBattleMenu(); });
    printf("  reparto: fondo %.0f%%  pokemon %.0f%%  cajas %.0f%%  menu %.0f%%  resto %.0f%%\n",
           100 * bg / total, 100 * bat / total, 100 * box2 / total, 100 * menu / total,
           100 * (total - bg - bat - box2 - menu) / total);
  }
  return 0;
}

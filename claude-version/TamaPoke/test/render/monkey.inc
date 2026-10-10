// ko12.9.8: prueba de "mono": toques, deslizar, pulsaciones largas y saltos de hora al azar sobre el
// loop() de verdad (con AddressSanitizer en monkey.sh). Busca cuelgues, accesos fuera de memoria,
// valores fuera de rango y pantallas de las que no se sale.
static void mkLoop(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 50) { gMockMillis += 50; loop(); }
}
static int mkScreen() {
  return vbOpen ? 1 : defOpen ? 2 : spdOpen ? 3 : gameOpen ? 4 : sackOpen ? 5 : shellOpen ? 8 : simonOpen ? 9
       : trainMenuOpen ? 6 : cardOpen ? 7 : galleryOpen ? 10 : xScreen ? 20 + xScreen : 0;
}
static int mkFails = 0;
static void mkCheck(bool ok, const char *what, long step) {
  if (ok) return;
  if (mkFails++ < 20) printf("  MONKEY FALLO paso %ld: %s (pantalla %d)\n", step, what, mkScreen());
}
static void monkey(long steps, uint32_t seed) {
  BRng r(seed);
  std::map<int, long> seen;
  int lastScr = -1;
  long sameFor = 0, maxSame = 0;
  int maxSameScr = 0;
  for (long i = 0; i < steps; i++) {
    uint32_t a = r.below(100);
    int x = (int)r.below(466), y = (int)r.below(466);
    if (a < 62) {  // toque
      touchSample(true, x, y); mkLoop(80 + r.below(150)); touchSample(false, x, y); mkLoop(100);
    } else if (a < 76) {  // deslizar
      int dx = (int)r.below(3) - 1, dy = dx ? 0 : (r.below(2) ? 1 : -1);
      touchSample(true, x, y);
      for (int k = 1; k <= 5; k++) { mkLoop(50); touchSample(true, x + dx * k * 30, y + dy * k * 30); }
      touchSample(false, x + dx * 150, y + dy * 150); mkLoop(100);
    } else if (a < 81) {  // pulsacion larga
      touchSample(true, x, y);
      for (int k = 0; k < 14; k++) { mkLoop(200); touchSample(true, x, y); }
      touchSample(false, x, y); mkLoop(100);
    } else if (a < 97) {  // esperar
      mkLoop(200 + r.below(2500));
    } else {  // salto de hora (como apagar y encender un rato)
      gMockEpoch += r.below(5 * 3600);
      pet.syncClock(gMockEpoch);
      mkLoop(200);
    }
    // valores en rango
    mkCheck(pet.fullness <= 100 && pet.joy <= 100 && pet.energy <= 100 && pet.hygiene <= 100, "barra > 100", i);
    mkCheck(pet.isEgg() || (pet.level() >= 1 && pet.level() <= LEVEL_MAX), "nivel fuera de rango", i);
    mkCheck(pet.speciesId >= 0 && pet.speciesId <= DEX_COUNT, "especie fuera de rango", i);
    mkCheck(box.count() <= BOX_MAX && hall.count() <= HALL_MAX, "caja desbordada", i);
    mkCheck(pet.orbN <= ORB_BAG_MAX, "bolsa de orbes desbordada", i);
    int s = mkScreen();
    seen[s]++;
    if (s == lastScr) sameFor++; else { sameFor = 0; lastScr = s; }
    if (sameFor > maxSame) { maxSame = sameFor; maxSameScr = s; }
    if (getenv("MONKEY_DBG") && s == 35 && sameFor > 40 && sameFor % 20 == 0)
      printf("  DBG bak: sel=%d crash=%d msg=%d\n", (int)bakSel, (int)bakCrashView, (int)bakMsg);
  }
  printf("  MONKEY %ld pasos, semilla %u: %d fallos; pantallas visitadas %zu; mas tiempo seguido en %d (%ld pasos)\n",
         steps, seed, mkFails, seen.size(), maxSameScr, maxSame);
  for (auto &kv : seen) printf("    pantalla %d: %ld\n", kv.first, kv.second);
}

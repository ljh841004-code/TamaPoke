// Prueba en el PC de lo que hace la app de Android (sin la ventana): importar mons.pak, arrancar el
// juego con setup() y el reloj de verdad, tocar, y guardar / volver a leer la "NVS" del fichero.
//   android/test/run_desktop.sh <mons.pak>
#include "../jni/host_core.inc"
#include <time.h>

#define PXW (466 * HDK)
static uint16_t gFrame[PXW * PXW];
static int gFrames = 0;
void hostPresent(const uint16_t *fb, int w, int h) {
  if (w == PXW && h == PXW) { memcpy(gFrame, fb, sizeof(gFrame)); gFrames++; }
}
void hostSetBrightness(uint8_t) {}
void hostDisplayOn(bool) {}
uint8_t hostPwrTake() { return 0; }
void hostPumpEvents() {}

static void saveRaw(const char *name) {
  FILE *f = fopen(name, "wb");
  if (f) { fwrite(gFrame, 2, PXW * PXW, f); fclose(f); }
}
static void run(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) { loop(); usleep(16000); }
}
static void tap(int x, int y) { touchSample(true, x, y); run(120); touchSample(false, x, y); run(400); }
static int fails = 0;
static void check(const char *what, bool ok) { printf("  %s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

int main(int argc, char **argv) {
  const char *pak = argc > 1 && argv[1][0] ? argv[1] : nullptr;
  gFiles = "build/desk_files";
  rmTree(gFiles);
  mkdir(gFiles.c_str(), 0755);
  gSdDir = gFiles + "/sd";
  mkdir(gSdDir.c_str(), 0755);
  static std::string root = gSdDir;
  gSdRoot = root.c_str();
  prefsLoadFile();
  {  // fuente HD (en el movil va dentro del APK, assets/)
    FILE *ff = fopen("assets/NotoSansKR-Medium-hd.otf", "rb");
    std::vector<uint8_t> fd;
    if (ff) { uint8_t b[65536]; size_t n; while ((n = fread(b, 1, sizeof(b), ff)) > 0) fd.insert(fd.end(), b, b + n); fclose(ff); }
    hdSetFont(fd.data(), fd.size());
    check("fuente HD", hdFontLoaded());
  }
  if (const char *hd = getenv("HD_PAK")) {  // hd.pak (dibujos grandes): se añade a mons/
    std::vector<int> m{ open(hd, O_RDONLY) };
    std::string e = importPak(m);
    close(m[0]);
    struct stat st;
    check("hd.pak se añade", e.empty() && stat((gSdDir + "/mons/hd/f025.png").c_str(), &st) == 0);
  }
  if (pak) {
    // en trozos (como se envia al movil): todos los argumentos, en orden
    std::vector<int> fds;
    for (int i = 1; i < argc; i++) fds.push_back(open(argv[i], O_RDONLY));
    check("mons.pak abierto", fds[0] >= 0);
    time_t t = time(nullptr);
    std::string err = importPak(fds);
    for (int f : fds) close(f);
    printf("  importar: %s (%ld s)\n", err.empty() ? "bien" : err.c_str(), (long)(time(nullptr) - t));
    check("importar mons.pak", err.empty() && haveMons());
    // un .pak roto: mensaje y sin tocar lo que habia
    FILE *bad = fopen("build/bad.pak", "wb"); fputs("no soy un pak", bad); fclose(bad);
    int bfd = open("build/bad.pak", O_RDONLY);
    std::string e2 = importPak(std::vector<int>{ bfd });
    close(bfd);
    check("pak roto -> mensaje", !e2.empty() && haveMons());
    // music.pak (sin thumbs): se añade sin borrar lo que habia
    if (const char *mp = getenv("MUSIC_PAK")) {
      std::vector<int> m{ open(mp, O_RDONLY) };
      std::string e3 = importPak(m);
      close(m[0]);
      struct stat st;
      check("music.pak se añade", e3.empty() && gImportMerged && stat((gSdDir + "/mons/bgm.wav").c_str(), &st) == 0 &&
            stat((gSdDir + "/mons/thumbs2.bin").c_str(), &st) == 0);
    }
  }
  setup();
  run(1500);
  saveRaw("build/desk_00_boot.raw");
  check("arranca y pinta", gFrames > 0);
  if (pet.awaitingStarter()) { tap(233, 233); run(600); saveRaw("build/desk_01_starter.raw"); }
  for (int i = 0; i < 3 && pet.isEgg(); i++) tap(233, 233);
  run(1500);
  tap(143, 280);  // "배경을 골라 주세요": 야외
  run(3000);
  saveRaw("build/desk_02_main.raw");
  if (pak) check("sprite cargado desde la carpeta importada", pmd.loaded || mon.loaded);
  if (getenv("HD_PAK")) {
    struct stat st;
    check("HD sigue tras mons.pak", stat((gSdDir + "/mons/hd/f025.png").c_str(), &st) == 0);
    for (int k = 0; k < 4; k++) { run(700); char n[64]; snprintf(n, sizeof(n), "build/desk_02_main_%d.raw", k); saveRaw(n); }
  }
  if (getenv("HD_PAK")) {  // HD: dibujo grande quieto (opcion del menu)
    extern bool gHdArt;
    gHdArt = true; run(800); saveRaw("build/desk_02_hdart.raw"); gHdArt = false;
  }
  {  // combate: sin "fantasmas" (cache del fondo durante el efecto de la SD)
    startWild();
    run(4000);
    tap(330, 348);  // [자동]
    for (int k = 0; k < 12; k++) { run(300); char n[64]; snprintf(n, sizeof(n), "build/desk_05_battle_%d.raw", k); saveRaw(n); }
    for (int k = 0; k < 60 && xScreen != 0; k++) { tap(300, 395); run(300); }
    run(1500);
  }
  // ficha (deslizar de abajo arriba) y volver
  touchSample(true, 233, 420);
  for (int k = 1; k <= 6; k++) { run(40); touchSample(true, 233, 420 - k * 40); }
  touchSample(false, 233, 180); run(800);
  saveRaw("build/desk_03_card.raw");
  check("ficha abierta", cardOpen);
  // guardar, borrar la "NVS" en memoria y volver a leerla del fichero
  int16_t sp = pet.speciesId;
  pet.saveNow();
  prefsSaveFile(true);
  std::vector<uint8_t> a; hostPrefsDump(a);
  mockNvsReset();
  prefsLoadFile();
  std::vector<uint8_t> b; hostPrefsDump(b);
  printf("  dump %zu / %zu\n", a.size(), b.size());
  check("NVS fichero ida y vuelta", a == b && a.size() > 100);
  Preferences p; p.begin("tamapoke", true);
  printf("  especie %d, bytes NVS %zu\n", sp, a.size());
  // reloj: la hora del juego es la del sistema (local)
  time_t now = time(nullptr); struct tm lt; localtime_r(&now, &lt);
  long diff = (long)rtcEpoch() - (long)(now + lt.tm_gmtoff);
  check("hora local", labs(diff) <= 2);
  extern std::atomic<uint64_t> gDeskAudioBytes;
  printf("  audio: %llu bytes mezclados\n", (unsigned long long)gDeskAudioBytes.load());
  check("el hilo del audio mezcla", gDeskAudioBytes.load() > 100000);
  {  // la barra de abajo de la app: otro lienzo con las mismas funciones (gfx cambiado un momento)
    TPCanvas *bar = new TPCanvas(466, 80, panel);
    bar->begin();
    TPCanvas *keep = gfx;
    gfx = bar;
    gfx->fillScreen(0);
    drawBtn(16, 10, 138, 58, 0x4C98, 0xFFFF, "메뉴");
    drawBtn(164, 10, 138, 58, 0x6B4D, 0xFFFF, "화면 끄기");
    drawBtn(312, 10, 138, 58, 0xD200, 0xFFFF, "나가기");
    gfx = keep;
    FILE *f = fopen("build/desk_bar.raw", "wb");
    fwrite(bar->hdfb(), 2, PXW * 80 * HDK, f); fclose(f);
    check("barra pintada", bar->getFramebuffer()[40 * 466 + 80] != 0);
  }
  printf("%s (%d fallos)\n", fails ? "FALLOS" : "TODO OK", fails);
  return fails ? 1 : 0;
}

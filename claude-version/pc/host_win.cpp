// TamaPoke para PC (Windows, prueba personal): la parte del JUEGO. El MISMO codigo del firmware que la app de
// Android (modo HD: lienzo 3x, texto Noto, Pokemon 3D animados de mons/ani), la "NVS" en un fichero y la
// importacion de mons.pak / ani.pak. Sin windows.h (choca con CX, CY, INPUT... del firmware): la ventana, el
// raton y los dialogos estan en win_main.cpp y se hablan por las funciones pc* de abajo.
#include "../android/jni/host_core.inc"
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>

extern int gHostBatPct;
extern bool gHostCharging;
extern bool gHdArt;

// fuente HD dentro del .exe (Noto Sans KR, SIL OFL)
extern "C" const uint8_t tpFontStart[], tpFontEnd[];
__asm__(".section .rdata,\"dr\"\n.global tpFontStart\n.global tpFontEnd\n"
        "tpFontStart:\n.incbin \"" TP_FONT_PATH "\"\ntpFontEnd:\n.byte 0\n.text\n");

// win_main.cpp
void winRepaint();
void winPickPak();
void winAskFirstPak();

static const int LCD = 466, PX = LCD * HDK;
static const int BAR_GAP = 20, BAR_H = 80, TOTAL_H = LCD + BAR_GAP + BAR_H;  // logico
static std::vector<uint32_t> gRgb;  // PX x TOTAL_H*HDK, BGRA (lo pinta win_main)
static uint16_t *gLast = nullptr;   // ultimo fotograma HD del juego
static bool gHaveLast = false, gHostScreenOn = true, gMenuOpen = false, gExit = false, gPwrPending = false;
static uint8_t gHostBright = 255;
static TPCanvas *gBar = nullptr;
static bool gBarOff = false, gBarDirty = true;

static void loadOpts() {
  FILE *f = fopen((gFiles + "/pc.txt").c_str(), "r");
  if (f) { int c = fgetc(f); gHdArt = c != '0'; fclose(f); }
}
static void saveOpts() {
  FILE *f = fopen((gFiles + "/pc.txt").c_str(), "w");
  if (f) { fputc(gHdArt ? '1' : '0', f); fclose(f); }
}

// ---------------------------------------------------------------- pintar
#define BAR_BX0 16
#define BAR_BW 138
#define BAR_BDX 148
static void drawBar() {
  if (!gBar) { gBar = new TPCanvas(LCD, BAR_H, panel); gBar->begin(); }
  TPCanvas *keep = gfx;
  gfx = gBar;
  gfx->fillScreen(0x0000);
  drawBtn(BAR_BX0, 10, BAR_BW, 58, 0x4C98, 0xFFFF, "메뉴");
  drawBtn(BAR_BX0 + BAR_BDX, 10, BAR_BW, 58, 0x6B4D, 0xFFFF, screenOff ? "화면 켜기" : "화면 끄기");
  drawBtn(BAR_BX0 + 2 * BAR_BDX, 10, BAR_BW, 58, C565(0xd8, 0x40, 0x30), 0xFFFF, "나가기");
  gfx = keep;
  gBarOff = screenOff;
  gBarDirty = false;
}
static inline uint32_t to32(uint16_t c, uint8_t b) {
  uint32_t r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, bl = (c & 31) * 255 / 31;
  if (b < 250) { r = r * b / 255; g = g * b / 255; bl = bl * b / 255; }
  return (r << 16) | (g << 8) | bl;
}
static void present(const uint16_t *fb) {
  if (gBarDirty || gBarOff != screenOff) drawBar();
  int H = TOTAL_H * HDK;
  gRgb.assign((size_t)PX * H, 0);
  if (gHostScreenOn && fb)
    for (size_t i = 0; i < (size_t)PX * PX; i++) gRgb[i] = to32(fb[i], gHostBright);
  const uint16_t *bar = gBar->hdfb();
  uint32_t *dst = gRgb.data() + (size_t)(LCD + BAR_GAP) * HDK * PX;
  for (size_t i = 0; i < (size_t)PX * BAR_H * HDK; i++) dst[i] = to32(bar[i], 255);
  winRepaint();
}
const uint32_t *pcFrame(int *w, int *h) {
  *w = PX;
  *h = TOTAL_H * HDK;
  return gRgb.empty() ? nullptr : gRgb.data();
}
void pcLogicalSize(int *w, int *h) { *w = LCD; *h = TOTAL_H; }

void hostPresent(const uint16_t *fb, int w, int h) {
  if (w != PX || h != PX) return;
  if (!gLast) gLast = (uint16_t *)malloc((size_t)PX * PX * 2);
  memcpy(gLast, fb, (size_t)PX * PX * 2);
  gHaveLast = true;
  if (!gMenuOpen) present(fb);
}
void hostSetBrightness(uint8_t b) { gHostBright = b < 170 ? 170 : b; }  // PC: el atenuado del aparato apenas (si no, todo se ve negro)
void hostDisplayOn(bool on) {
  if (gHostScreenOn == on) return;
  gHostScreenOn = on;
  if (gHaveLast) present(gLast);
}
uint8_t hostPwrTake() { bool p = gPwrPending; gPwrPending = false; return p ? 1 : 0; }

// ---------------------------------------------------------------- menu ([메뉴] o Esc)
#define HM_X 93
#define HM_W 280
#define HM_H 56
#define HM_N 3
static const int HM_Y[HM_N] = { 150, 220, 290 };
static void drawHostMenu() {
  if (!gHaveLast) return;
  uint16_t *fb = gfx->hdfb();
  static std::vector<uint16_t> save;
  save.assign(fb, fb + (size_t)PX * PX);
  memcpy(fb, gLast, (size_t)PX * PX * 2);
  gfx->fillRoundRect(63, 90, 340, 290, 20, 0x2124);
  drawFit("TamaPoke (PC)", 100, 300, 0xFFFF, 2);
  {
    int hdM3dCount();
    char b[48];
    snprintf(b, sizeof(b), "3D 모델 %d종", hdM3dCount());
    drawFit(b, 128, 300, 0xC618, 1);
  }
  drawBtn(HM_X, HM_Y[0], HM_W, HM_H, 0x4C98, 0xFFFF, "pak 파일 넣기");
  drawBtn(HM_X, HM_Y[1], HM_W, HM_H, 0x6B4D, 0xFFFF, gHdArt ? "그림: 3D 움직임" : "그림: 도트");
  drawBtn(HM_X, HM_Y[2], HM_W, HM_H, 0xFFFF, 0x0000, "닫기");
  bool on = gHostScreenOn;
  gHostScreenOn = true;
  present(gfx->hdfb());
  gHostScreenOn = on;
  fb = gfx->hdfb();
  memcpy(fb, save.data(), (size_t)PX * PX * 2);
}
void pcToggleMenu() { gMenuOpen = !gMenuOpen; if (!gMenuOpen && gHaveLast) present(gLast); }

static std::vector<int> gPakFds;
void pcSetPakFds(const int *fds, int n) {
  for (int f : gPakFds) _close(f);
  gPakFds.assign(fds, fds + n);
}
static void hostMenuTap(int x, int y) {
  int hit = -1;
  for (int i = 0; i < HM_N; i++)
    if (x >= HM_X && x < HM_X + HM_W && y >= HM_Y[i] && y < HM_Y[i] + HM_H) hit = i;
  if (hit < 0) return;
  gMenuOpen = false;
  if (hit == 0) winPickPak();
  if (hit == 1) { gHdArt = !gHdArt; saveOpts(); }
  if (gHaveLast) present(gLast);
}
static void barTap(int x) {
  int i = (x - BAR_BX0) / BAR_BDX;
  if (x < BAR_BX0 || i > 2 || x - BAR_BX0 - i * BAR_BDX > BAR_BW) return;
  if (i == 0) pcToggleMenu();
  if (i == 1) gPwrPending = true;
  if (i == 2) gExit = true;
}

// ---------------------------------------------------------------- raton (coordenadas logicas)
static bool gDown = false, gImporting = false;
static int16_t gTx = 0, gTy = 0;
static uint32_t gHoldT = 0, gDownT = 0;
static bool gUpPending = false;
void pcMouse(int x, int y, int act) {  // 0 abajo, 1 mover, 2 arriba
  if (gImporting) return;
  if (!gDown && y >= LCD) {
    if (act == 2 && y >= LCD + BAR_GAP - 6) barTap(x);
    return;
  }
  if (gMenuOpen) {
    if (act == 2) hostMenuTap(x, y);
    return;
  }
  int16_t gx = (int16_t)(x < 0 ? 0 : x > LCD - 1 ? LCD - 1 : x), gy = (int16_t)(y < 0 ? 0 : y > LCD - 1 ? LCD - 1 : y);
  if (act == 0 || (act == 1 && gDown)) {
    if (!gDown || abs(gx - gTx) > 2 || abs(gy - gTy) > 2) {
      if (!gDown) gDownT = millis();
      gDown = true; gTx = gx; gTy = gy;
      gUpPending = false;
      gHoldT = millis();
      touchSample(true, gx, gy);
    }
  } else if (act == 2 && gDown) {
    gUpPending = true;  // soltar despues de que el juego haya visto el toque (un clic de raton es muy corto)
  }
}
void pcBattery(int pct, bool chg) { gHostBatPct = pct; gHostCharging = chg; }
void pcRequestExit() { gExit = true; }
void pcRepaintLast() { if (gHaveLast) present(gLast); }

// ---------------------------------------------------------------- arranque y bucle (lo llama win_main.cpp)
static bool gStarted = false, gAsked = false;
static uint32_t gLastSave = 0;
void pcInit() {
  gFiles = ".";
  gSdDir = "./sd";
  mkdir(gSdDir.c_str(), 0755);
  static std::string sdRoot = gSdDir;
  gSdRoot = sdRoot.c_str();
  prefsLoadFile();
  loadClockAdj();
  loadOpts();
  hdSetFont(tpFontStart, (size_t)(tpFontEnd - tpFontStart));
}
void pcSave() {
  if (!gStarted) return;
  pet.lastSeenEpoch = clockEpoch();
  pet.saveNow();
  prefsSaveFile(true);
}
// una vuelta del juego; false = salir
bool pcStep() {
  if (!gStarted) {
    setup();
    gStarted = true;
    gLastSave = millis();
  }
  if (!gPakFds.empty()) {
    std::vector<int> fds;
    fds.swap(gPakFds);
    gImporting = true;
    runImport(fds);
    gImporting = false;
    gDown = false;
  }
  if (!gAsked && !haveMons()) {
    gAsked = true;
    winAskFirstPak();
    return true;
  }
  if (gDown && !gUpPending && millis() - gHoldT >= 20) { gHoldT = millis(); touchSample(true, gTx, gTy); }
  loop();
  if (gUpPending && millis() - gDownT >= 60) { touchSample(false, gTx, gTy); gDown = gUpPending = false; }
  if (gMenuOpen) drawHostMenu();
  if (gExit) { pcSave(); return false; }
  if (millis() - gLastSave > 5000) { gLastSave = millis(); prefsSaveFile(false); }
  return true;
}

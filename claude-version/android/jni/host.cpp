// TamaPoke para Android (prueba personal). El MISMO codigo del firmware (sketch preprocesado, como
// test/render) corriendo en el movil: este fichero pone la ventana, el tactil, el ciclo de vida, la
// "NVS" en un fichero y la importacion de mons.pak (se descifra a <files>/sd/mons/ una vez).
#include "host_core.inc"
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <jni.h>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <android/asset_manager.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "TamaPoke", __VA_ARGS__)

void hostAudioPause(bool paused);
extern int gHostBatPct;
extern bool gHostCharging;

static android_app *gApp = nullptr;
static ANativeWindow *gWin = nullptr;
static int gWinW = 0, gWinH = 0, gGeomH = 0, gOffY = 0;
static bool gHostScreenOn = true;
static uint8_t gHostBright = 255;

// ---------------------------------------------------------------- ventana
// Modo HD: el juego sigue pensando en 466x466 (logico), pero el lienzo es 3x (1398x1398, host_hd_canvas.h)
// y la ventana tiene 1398 de ancho. Abajo las medidas en logico salvo PX (pixeles de verdad).
// La ventana tiene 466 (x3) de ancho y el alto proporcional a la pantalla: el sistema la escala sola.
// Arriba el juego (466x466, NEGRO fuera del circulo solo si se elige "둥근 화면"; por defecto entero,
// para que en el movil no se corte nada arriba ni abajo) y debajo una barra con [메뉴] [화면 끄기] [나가기]
static const int LCD = 466, PX = LCD * HDK;
static const int BAR_H = 80, BAR_GAP = 28;
static std::vector<int16_t> gCircleX0;  // por fila HD: primera columna dentro del circulo
static uint16_t gLast[PX * PX];
static bool gHaveLast = false;
static int gBarY = -1;                    // fila de la ventana donde empieza la barra (-1 = no cabe)
static TPCanvas *gBar = nullptr;
static bool gBarDirty = true, gBarOff = false;
static bool gRound = false;               // "둥근 화면" (como la placa) / "네모 화면" (entero, por defecto)

extern bool gHdArt;
static void loadRound() {
  FILE *f = fopen((gFiles + "/round.txt").c_str(), "r");
  if (f) { gRound = fgetc(f) == '1'; gHdArt = fgetc(f) != '0'; fclose(f); }  // 3D: por defecto si
}
static void saveRound() {
  FILE *f = fopen((gFiles + "/round.txt").c_str(), "w");
  if (f) { fputc(gRound ? '1' : '0', f); fputc(gHdArt ? '1' : '0', f); fclose(f); }
}

static void setupWindow() {
  if (!gWin) return;
  ANativeWindow_setBuffersGeometry(gWin, 0, 0, 0);  // primero el tamano real (despues de fijarlo, getWidth da 466)
  gWinW = ANativeWindow_getWidth(gWin);
  gWinH = ANativeWindow_getHeight(gWin);
  if (gWinW <= 0 || gWinH <= 0) return;
  gGeomH = (int)((int64_t)LCD * gWinH / gWinW);
  if (gGeomH < LCD) gGeomH = LCD;
  int block = LCD + BAR_GAP + BAR_H;
  if (gGeomH >= block + 20) {
    gOffY = (gGeomH - block) * 2 / 5;
    gBarY = gOffY + LCD + BAR_GAP;
  } else {
    gOffY = (gGeomH - LCD) / 2;
    gBarY = -1;
  }
  ANativeWindow_setBuffersGeometry(gWin, PX, gGeomH * HDK, WINDOW_FORMAT_RGB_565);
  if (gCircleX0.empty()) {
    gCircleX0.resize(PX);
    for (int y = 0; y < PX; y++) {
      double dy = y + 0.5 - PX / 2.0, r = PX / 2.0;
      double h = dy * dy < r * r ? sqrt(r * r - dy * dy) : 0;
      gCircleX0[y] = (int16_t)(PX / 2.0 - h + 0.5);
    }
  }
}

static inline uint16_t dim565(uint16_t c, uint8_t b) {
  if (b >= 250) return c;
  uint32_t r = (c >> 11) * b / 255, g = ((c >> 5) & 63) * b / 255, bl = (c & 31) * b / 255;
  return (uint16_t)(r << 11 | g << 5 | bl);
}

// barra de abajo: se pinta con las funciones del juego en un lienzo aparte (gfx se cambia un momento)
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

static void blit(const uint16_t *fb) {
  if (!gWin || !gGeomH) return;
  if (gBarY >= 0 && (gBarDirty || gBarOff != screenOff)) drawBar();
  ANativeWindow_Buffer buf;
  if (ANativeWindow_lock(gWin, &buf, nullptr) != 0) return;
  uint16_t *dst = (uint16_t *)buf.bits;
  int h = buf.height < gGeomH * HDK ? buf.height : gGeomH * HDK;
  int w = buf.width < PX ? buf.width : PX;
  const uint16_t *bar = gBar ? gBar->hdfb() : nullptr;
  for (int y = 0; y < h; y++) {
    uint16_t *row = dst + (size_t)y * buf.stride;
    memset(row, 0, buf.width * 2);
    int by = y - gBarY * HDK;
    if (gBarY >= 0 && bar && by >= 0 && by < BAR_H * HDK) { memcpy(row, bar + (size_t)by * PX, w * 2); continue; }
    int gy = y - gOffY * HDK;
    if (gy < 0 || gy >= PX || !gHostScreenOn) continue;
    int x0 = gRound ? gCircleX0[gy] : 0, x1 = PX - x0;
    const uint16_t *src = fb + (size_t)gy * PX;
    for (int x = x0; x < x1 && x < w; x++) row[x] = dim565(src[x], gHostBright);
  }
  ANativeWindow_unlockAndPost(gWin);
}

// el juego hizo flush: guardar el fotograma (el menu de la app se pinta encima de una copia)
static bool gMenuOpen = false;
void drawHostMenu();
void hostPresent(const uint16_t *fb, int w, int h) {
  if (w != PX || h != PX) return;
  memcpy(gLast, fb, sizeof(gLast));
  gHaveLast = true;
  if (!gMenuOpen) blit(fb);
}
void hostSetBrightness(uint8_t b) { gHostBright = b < 40 ? 40 : b; }
void hostDisplayOn(bool on) {  // [화면 끄기] (PWR): el juego apaga el panel -> cuadro negro
  if (gHostScreenOn == on) return;
  gHostScreenOn = on;
  if (gHaveLast) blit(gLast);
}

// ---------------------------------------------------------------- menu de la app ([메뉴] o tecla atras)
// [mons.pak 고르기] [둥근 화면 / 네모 화면] [HD 그림 켜기/끄기] [닫기]. Se dibuja con las mismas funciones del juego
// sobre una copia del ultimo fotograma, y luego se devuelve el lienzo como estaba.
static bool gPwrPending = false;
static void callJava(const char *name);
#define HM_X 93
#define HM_W 280
#define HM_H 56
#define HM_N 4
static const int HM_Y[HM_N] = { 128, 192, 256, 320 };
void drawHostMenu() {
  if (!gHaveLast) return;
  uint16_t *fb = gfx->hdfb();
  static uint16_t save[PX * PX];
  memcpy(save, fb, sizeof(save));
  memcpy(fb, gLast, sizeof(gLast));
  gfx->fillRoundRect(63, 70, 340, 330, 20, 0x2124);
  drawFit("TamaPoke (Android)", 84, 300, 0xFFFF, 2);
  drawBtn(HM_X, HM_Y[0], HM_W, HM_H, 0x4C98, 0xFFFF, "mons.pak 고르기");
  drawBtn(HM_X, HM_Y[1], HM_W, HM_H, 0x6B4D, 0xFFFF, gRound ? "네모 화면으로" : "둥근 화면으로");
  drawBtn(HM_X, HM_Y[2], HM_W, HM_H, 0x6B4D, 0xFFFF, gHdArt ? "그림: 3D 움직임" : "그림: 도트");
  drawBtn(HM_X, HM_Y[3], HM_W, HM_H, 0xFFFF, 0x0000, "닫기");
  bool on = gHostScreenOn;
  gHostScreenOn = true;  // con la pantalla "apagada" el menu se ve igual
  blit(fb);
  gHostScreenOn = on;
  fb = gfx->hdfb();  // lo que se escribio en el proxy durante el menu, fuera
  memcpy(fb, save, sizeof(save));
}
static void hostMenuTap(int x, int y) {
  int hit = -1;
  for (int i = 0; i < HM_N; i++)
    if (x >= HM_X && x < HM_X + HM_W && y >= HM_Y[i] && y < HM_Y[i] + HM_H) hit = i;
  if (hit < 0) return;
  gMenuOpen = false;
  if (hit == 0) callJava("pickPak");
  if (hit == 1) { gRound = !gRound; saveRound(); }
  if (hit == 2) { gHdArt = !gHdArt; saveRound(); }
  if (gHaveLast) blit(gLast);
}

// pwrPoll() de host_stubs devuelve 0; [화면 끄기] hace una pulsacion corta
uint8_t hostPwrTake() { bool p = gPwrPending; gPwrPending = false; return p ? 1 : 0; }
static bool gExit = false;
static void barTap(int x) {
  int i = (x - BAR_BX0) / BAR_BDX;
  if (x < BAR_BX0 || i > 2 || x - BAR_BX0 - i * BAR_BDX > BAR_BW) return;
  if (i == 0) { gMenuOpen = !gMenuOpen; if (!gMenuOpen && gHaveLast) blit(gLast); }
  if (i == 1) gPwrPending = true;
  if (i == 2) gExit = true;  // guardar y cerrar (lo hace el bucle)
}

// ---------------------------------------------------------------- Java
static void callJava(const char *name) {
  JNIEnv *env = nullptr;
  gApp->activity->vm->AttachCurrentThread(&env, nullptr);
  jclass cls = env->GetObjectClass(gApp->activity->clazz);
  jmethodID m = env->GetMethodID(cls, name, "()V");
  if (m) env->CallVoidMethod(gApp->activity->clazz, m);
  env->DeleteLocalRef(cls);
}

static std::mutex gPakM;
static std::vector<int> gPakFds;  // los elegidos (en orden de nombre), a importar en el hilo del juego
extern "C" JNIEXPORT void JNICALL Java_com_tamapoke_ko_MainActivity_nativePakFds(JNIEnv *env, jclass, jintArray arr) {
  jsize n = env->GetArrayLength(arr);
  std::vector<int> v(n);
  env->GetIntArrayRegion(arr, 0, n, (jint *)v.data());
  std::lock_guard<std::mutex> g(gPakM);
  for (int f : gPakFds) close(f);
  gPakFds = v;
}
extern "C" JNIEXPORT void JNICALL Java_com_tamapoke_ko_MainActivity_nativeBattery(JNIEnv *, jclass, jint pct, jboolean chg) {
  gHostBatPct = pct;
  gHostCharging = chg;
}

// ---------------------------------------------------------------- tactil
// como la placa: apoyar / mover / levantar, y con el dedo quieto se repite cada 20 ms (pulsaciones largas)
static bool gImporting = false;
static bool gDown = false;
static int16_t gTx = 0, gTy = 0;
static uint32_t gHoldT = 0;
static void toGame(float px, float py, int16_t &x, int16_t &y) {
  float gx = px * LCD / (gWinW ? gWinW : 1);
  float gy = py * gGeomH / (gWinH ? gWinH : 1) - gOffY;
  x = (int16_t)(gx < 0 ? 0 : gx > LCD - 1 ? LCD - 1 : gx);
  y = (int16_t)(gy < 0 ? 0 : gy > LCD - 1 ? LCD - 1 : gy);
}

static int32_t onInput(android_app *, AInputEvent *ev) {
  if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
    if (AKeyEvent_getKeyCode(ev) == AKEYCODE_BACK) {
      if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) { gMenuOpen = !gMenuOpen; if (!gMenuOpen && gHaveLast) blit(gLast); }
      return 1;  // atras no cierra la app: abre el menu
    }
    return 0;
  }
  if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
  if (gImporting) return 1;
  int32_t act = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
  int16_t x, y;
  toGame(AMotionEvent_getX(ev, 0), AMotionEvent_getY(ev, 0), x, y);
  // la barra de abajo (solo si el dedo no esta ya jugando)
  float wy = AMotionEvent_getY(ev, 0) * gGeomH / (gWinH ? gWinH : 1);
  if (!gDown && gBarY >= 0 && wy >= gBarY - 6) {
    if (act == AMOTION_EVENT_ACTION_UP) barTap((int)(AMotionEvent_getX(ev, 0) * LCD / (gWinW ? gWinW : 1)));
    return 1;
  }
  if (gMenuOpen) {
    if (act == AMOTION_EVENT_ACTION_UP) hostMenuTap(x, y);
    return 1;
  }
  if (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_MOVE) {
    if (!gDown || abs(x - gTx) > 2 || abs(y - gTy) > 2) {
      gDown = true; gTx = x; gTy = y;
      gHoldT = millis();
      touchSample(true, x, y);
    }
  } else if (act == AMOTION_EVENT_ACTION_UP || act == AMOTION_EVENT_ACTION_CANCEL) {
    if (gDown) touchSample(false, gTx, gTy);
    gDown = false;
  }
  return 1;
}

// ---------------------------------------------------------------- ciclo de vida
static bool gStarted = false, gPaused = false;
static int64_t gPauseAt = 0;
static int64_t nowUs() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000; }

static void saveAll() {
  if (!gStarted) return;
  pet.lastSeenEpoch = clockEpoch();
  pet.saveNow();
  prefsSaveFile(true);
}

static void onCmd(android_app *app, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      gWin = app->window;
      setupWindow();
      if (gHaveLast) blit(gLast);
      break;
    case APP_CMD_TERM_WINDOW:
      gWin = nullptr;
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
      setupWindow();
      break;
    case APP_CMD_PAUSE:
      if (!gPaused) { gPaused = true; gPauseAt = nowUs(); saveAll(); hostAudioPause(true); }
      break;
    case APP_CMD_RESUME:
      if (gPaused) {
        gPaused = false;
        hostAudioPause(false);
        gHostPausedUs += nowUs() - gPauseAt;  // el juego no "vio" ese rato en millis...
        if (gStarted) pet.syncClock(clockEpoch());  // ...se aplica como tiempo apagado
      }
      break;
    case APP_CMD_SAVE_STATE:
    case APP_CMD_STOP:
      saveAll();
      break;
    case APP_CMD_DESTROY:
      saveAll();
      break;
  }
}

// mientras se descomprime mons.pak (medio minuto) hay que seguir atendiendo al sistema: si no, "no responde"
void hostPumpEvents() {
  int events;
  android_poll_source *src;
  while (ALooper_pollOnce(0, nullptr, &events, (void **)&src) >= 0) {
    if (src) src->process(gApp, src);
    if (gApp->destroyRequested) break;
  }
}

void android_main(android_app *app) {
  gApp = app;
  app->onAppCmd = onCmd;
  app->onInputEvent = onInput;
  gFiles = app->activity->internalDataPath ? app->activity->internalDataPath : "/data/local/tmp";
  mkdir(gFiles.c_str(), 0755);
  gSdDir = gFiles + "/sd";
  mkdir(gSdDir.c_str(), 0755);
  static std::string sdRoot = gSdDir;
  gSdRoot = sdRoot.c_str();
  prefsLoadFile();
  loadClockAdj();
  loadRound();
  if (AAsset *a = AAssetManager_open(app->activity->assetManager, "NotoSansKR-Medium-hd.otf", AASSET_MODE_BUFFER)) {
    hdSetFont((const uint8_t *)AAsset_getBuffer(a), (size_t)AAsset_getLength(a));  // se copia
    AAsset_close(a);
  }

  uint32_t lastSave = 0;
  bool asked = false;
  while (!app->destroyRequested) {
    int events;
    android_poll_source *src;
    int timeout = (gPaused || !gWin) ? 200 : 0;
    while (ALooper_pollOnce(timeout, nullptr, &events, (void **)&src) >= 0) {
      if (src) src->process(app, src);
      if (app->destroyRequested) break;
      timeout = 0;
    }
    if (app->destroyRequested) break;
    if (gPaused || !gWin) continue;
    if (!gStarted) {
      setup();
      gStarted = true;
      lastSave = millis();
    }
    std::vector<int> fds;
    { std::lock_guard<std::mutex> g(gPakM); fds.swap(gPakFds); }
    if (!fds.empty()) { gImporting = true; runImport(fds); gImporting = false; gDown = false; }
    if (!asked && !haveMons()) {  // primera vez: elegir mons.pak (sin el, el juego va igual con "?")
      asked = true;
      callJava("pickPak");
    }
    if (gDown && millis() - gHoldT >= 20) {  // dedo quieto: pulsaciones largas
      gHoldT = millis();
      touchSample(true, gTx, gTy);
    }
    uint32_t t0 = millis();
    loop();
    if (gMenuOpen) drawHostMenu();
    if (gExit) {  // [나가기]: guardar y cerrar la app (al volver, el tiempo de fuera cuenta como apagado)
      gExit = false;
      saveAll();
      ANativeActivity_finish(app->activity);
    }
    if (millis() - lastSave > 5000) { lastSave = millis(); prefsSaveFile(false); }
    uint32_t spent = millis() - t0;
    if (spent < 16) usleep((16 - spent) * 1000);  // ~60 fps como mucho
  }
  saveAll();
}

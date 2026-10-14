// Android: el audio.cpp de la placa tal cual (musica de la SD, gritos, efectos, mezcla a 16 kHz) en su
// propio hilo. Lo que en la placa iba a la I2S va a un anillo que vacia AAudio (el altavoz del movil).
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "../../TamaPoke/audio.cpp"
#include <aaudio/AAudio.h>
#include <mutex>
#include <condition_variable>
#include <android/log.h>

static std::mutex gRingM;
static std::condition_variable gRingCv;
static const size_t RING = 16384;  // ~250 ms a 16 kHz estereo
static uint8_t gRing[RING];
static size_t gRd = 0, gFill = 0;
static AAudioStream *gStream = nullptr;
static std::atomic<bool> gHostAudioPaused{false};

static aaudio_data_callback_result_t dataCb(AAudioStream *, void *, void *audio, int32_t frames) {
  size_t want = (size_t)frames * 4;
  uint8_t *out = (uint8_t *)audio;
  std::unique_lock<std::mutex> l(gRingM);
  size_t n = want < gFill ? want : gFill;
  for (size_t i = 0; i < n; i++) out[i] = gRing[(gRd + i) % RING];
  gRd = (gRd + n) % RING;
  gFill -= n;
  l.unlock();
  if (n < want) memset(out + n, 0, want - n);  // sin datos: silencio
  gRingCv.notify_all();
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void errCb(AAudioStream *, void *, aaudio_result_t) {}

static bool openStream(uint32_t rate) {
  AAudioStreamBuilder *b = nullptr;
  if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return false;
  AAudioStreamBuilder_setSampleRate(b, (int32_t)rate);
  AAudioStreamBuilder_setChannelCount(b, 2);
  AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_NONE);
  AAudioStreamBuilder_setDataCallback(b, dataCb, nullptr);
  AAudioStreamBuilder_setErrorCallback(b, errCb, nullptr);
  aaudio_result_t r = AAudioStreamBuilder_openStream(b, &gStream);
  AAudioStreamBuilder_delete(b);
  if (r != AAUDIO_OK) { gStream = nullptr; return false; }
  return AAudioStream_requestStart(gStream) == AAUDIO_OK;
}

bool hostAudioStart(uint32_t rate) {
  bool ok = openStream(rate);
  __android_log_print(ANDROID_LOG_INFO, "TamaPoke", "AAudio %s", ok ? "ok" : "fallo");
  return true;  // aunque no haya salida, el juego sigue (el hilo escribe y se descarta)
}

size_t hostAudioWrite(const uint8_t *b, size_t n) {
  std::unique_lock<std::mutex> l(gRingM);
  if (!gStream || gHostAudioPaused) {  // sin salida: al ritmo del reloj (32 bytes/ms * 2 canales)
    l.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(n / 64 ? n / 64 : 1));
    return n;
  }
  if (!gRingCv.wait_for(l, std::chrono::milliseconds(60), [] { return gFill < RING; })) return 0;
  size_t room = RING - gFill;
  size_t k = n < room ? n : room;
  size_t wr = (gRd + gFill) % RING;
  for (size_t i = 0; i < k; i++) gRing[(wr + i) % RING] = b[i];
  gFill += k;
  return k;
}

// la app pasa a segundo plano: silencio (y el hilo del audio no se queda esperando al anillo)
void hostAudioPause(bool paused) {
  gHostAudioPaused = paused;
  if (!gStream) return;
  if (paused) AAudioStream_requestPause(gStream);
  else {
    { std::lock_guard<std::mutex> g(gRingM); gFill = 0; }
    AAudioStream_requestStart(gStream);
  }
  gRingCv.notify_all();
}

// PC (Windows): el sonido del juego (audio.cpp en audio_win.cpp) sale por waveOut. Aparte de lo de Arduino
// porque windows.h choca con sus nombres (CX, CY, INPUT...)
#include <stdint.h>
#include <string.h>
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

static std::mutex gRingM;
static std::condition_variable gRingCv;
static const size_t RING = 16384;  // ~250 ms a 16 kHz estereo
static uint8_t gRing[RING];
static size_t gRd = 0, gFill = 0;
static HWAVEOUT gWo = nullptr;
static std::atomic<bool> gHostAudioPaused{false};
static const int NBUF = 4, BUFB = 2048;  // 4 x 32 ms
static WAVEHDR gHdr[NBUF];
static uint8_t gBuf[NBUF][BUFB];
static HANDLE gEv = nullptr;

static void fillBuf(uint8_t *out, size_t want) {
  std::unique_lock<std::mutex> l(gRingM);
  size_t n = want < gFill ? want : gFill;
  for (size_t i = 0; i < n; i++) out[i] = gRing[(gRd + i) % RING];
  gRd = (gRd + n) % RING;
  gFill -= n;
  l.unlock();
  if (n < want) memset(out + n, 0, want - n);
  gRingCv.notify_all();
}

static void playThread() {
  for (;;) {
    WaitForSingleObject(gEv, 50);
    for (int i = 0; i < NBUF; i++) {
      if (!(gHdr[i].dwFlags & WHDR_DONE)) continue;
      fillBuf(gBuf[i], BUFB);
      gHdr[i].dwFlags &= ~WHDR_DONE;
      waveOutWrite(gWo, &gHdr[i], sizeof(WAVEHDR));
    }
  }
}

bool hostAudioStart(uint32_t rate) {
  WAVEFORMATEX f = {};
  f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 2; f.nSamplesPerSec = rate; f.wBitsPerSample = 16;
  f.nBlockAlign = 4; f.nAvgBytesPerSec = rate * 4;
  gEv = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  if (waveOutOpen(&gWo, WAVE_MAPPER, &f, (DWORD_PTR)gEv, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) { gWo = nullptr; return true; }
  for (int i = 0; i < NBUF; i++) {
    gHdr[i] = {};
    gHdr[i].lpData = (LPSTR)gBuf[i];
    gHdr[i].dwBufferLength = BUFB;
    waveOutPrepareHeader(gWo, &gHdr[i], sizeof(WAVEHDR));
    gHdr[i].dwFlags |= WHDR_DONE;  // vacios: el hilo los llena
  }
  std::thread(playThread).detach();
  SetEvent(gEv);
  return true;
}

size_t hostAudioWrite(const uint8_t *b, size_t n) {
  std::unique_lock<std::mutex> l(gRingM);
  if (!gWo || gHostAudioPaused) {
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

void hostAudioPause(bool paused) {
  gHostAudioPaused = paused;
  if (!gWo) return;
  if (paused) waveOutPause(gWo);
  else { { std::lock_guard<std::mutex> g(gRingM); gFill = 0; } waveOutRestart(gWo); }
  gRingCv.notify_all();
}

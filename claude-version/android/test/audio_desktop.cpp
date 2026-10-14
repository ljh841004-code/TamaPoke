// PC: el audio.cpp de verdad en su hilo, con una "salida" que solo consume al ritmo del reloj
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "../../TamaPoke/audio.cpp"
#include <atomic>
std::atomic<uint64_t> gDeskAudioBytes{0};
bool hostAudioStart(uint32_t) { return true; }
size_t hostAudioWrite(const uint8_t *, size_t n) {
  std::this_thread::sleep_for(std::chrono::microseconds(n * 1000 / 64));
  gDeskAudioBytes += n;
  return n;
}
void hostAudioPause(bool) {}

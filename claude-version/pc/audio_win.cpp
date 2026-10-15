// PC (Windows): el audio.cpp de la placa (musica, gritos, efectos, mezcla a 16 kHz) en su propio hilo, igual
// que en Android. hostAudioStart / hostAudioWrite / hostAudioPause estan en audio_wave.cpp (waveOut)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "../TamaPoke/audio.cpp"

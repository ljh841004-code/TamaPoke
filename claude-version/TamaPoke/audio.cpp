#include "audio.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <atomic>
#include "wav_stream.h"
#include "sd_lock.h"
#include "music_route.h"

// ---------------------------------------------------------------------------
// Audio del TamaPoke: códec ES8311 (DAC -> amplificador PA -> altavoz) por I2S.
// Init del ES8311 portado del driver oficial de Espressif (esp-bsp), fijado a
// MCLK=4.096MHz (256*fs), 16kHz, 16-bit, esclavo I2S. Los efectos son tonos
// cuadrados (estilo Game Boy) sintetizados en una tarea aparte para no
// bloquear el loop de juego.
// ---------------------------------------------------------------------------

#define ES8311_ADDR 0x18
#define SAMPLE_RATE 16000

static I2SClass i2s;
static bool gReady = false;
static std::atomic<bool> gOn{true};
static std::atomic<bool> gSleeping{false};
static QueueHandle_t gQ = nullptr;
// Sin pantalla de volumen en el fork KO: efectos al 100% = la amplitud de v1.17.
static std::atomic<uint8_t> levels[3] = {25, 65, 100};
struct AudioCommand { uint8_t kind, id; int16_t *pcm; uint32_t samples; };
static std::atomic<uint32_t> musicRequest{0}; // bit 0: wild; upper bits: session
static std::atomic<uint32_t> musicReload{0}, uploadRequest{0}, uploadAck{0};
static std::atomic<bool> musicEnabled{false};
static std::atomic<bool> musicPaused{false};  // fork KO (ko5): pantalla apagada
static const char *const volumeKeys[] = {"volBgm", "volCry", "volSfx"};

// El NS4150B tarda bastante mas de 8 ms en estabilizarse tras cada apagado, asi
// que encenderlo justo antes de cada efecto se comia los cortos: el jingle de
// arranque (440 ms) se oia y los pitidos de 35 ms no. Se deja encendido mientras
// haya sonido y la mascota este despierta, como en el ejemplo verificado de
// Waveshare, y solo se apaga al dormir o al silenciar.
static void updateAmplifierPower() {
  // gReady evita encenderlo en una placa donde el codec no arranco: ahi el
  // amplificador solo aportaria siseo, porque no va a sonar nada.
  digitalWrite(PA, (gReady && gOn && !gSleeping) ? HIGH : LOW);
}

// ---- I2C del códec ----
static bool esW(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static uint8_t esR(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Secuencia de init VERIFICADA EN ESTA PLACA (proyecto PlaneRadar2.0, misma
// Waveshare 1.75). Clave: reloj DERIVADO DEL BCLK (reg01=0xBF, sin MCLK externo)
// y referencia interna que alimenta el DAC (reg44=0x58); sin esos dos el codec
// respondia por I2C pero no salia audio. 16kHz, 16-bit, esclavo I2S.
static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // open()
  esW(0x0D, 0xFA); esW(0x44, 0x08); esW(0x44, 0x08);  // power up + quirk de 1a escritura
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24);
  esW(0x04, 0x10); esW(0x05, 0x00); esW(0x0B, 0x00); esW(0x0C, 0x00);
  esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80); esW(0x00, 0x80);                   // reset clock, esclavo
  esW(0x01, 0xBF);                                    // clk src = BCLK (sin MCLK externo)
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }  // SCLK no invertido
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x44, 0x58);                                    // referencia interna -> alimenta el DAC

  // config_sample(): BCLK*8 = DIG_MCLK
  esW(0x02, 0x18); esW(0x05, 0x00); esW(0x03, 0x10); esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }  // bclk_div=4

  // formato I2S 16-bit
  esW(0x09, 0x0C); esW(0x0A, 0x0C);

  // start() DAC esclavo
  esW(0x00, 0x80); esW(0x01, 0xBF); esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A);
  esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);

  // volumen + unmute
  esW(0x32, 0xBF);                                    // volumen DAC ~0 dB
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }  // unmute
  return true;
}

// ---- sintetizador de tono cuadrado ----
struct Note { uint16_t f, ms; };

static const Note N_TAP[]    = {{880, 35}};
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{784, 45}, {988, 60}};
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};
// ko9: aviso de cuidado: dos "ding-dong" cortos, que se oigan sin asustar
static const Note N_ALERT[]  = {{1175, 90}, {880, 110}, {0, 70}, {1175, 90}, {880, 150}};

struct SfxDef { const Note *n; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
  {N_ALERT, 5},
};

// Single task owns all playback state. Commands transfer ownership of PCM buffers.
static void audioTask(void *) {
  int16_t out[256 * 2];
  static WavStream<File> music; // 8 KiB read-ahead, never a whole-song allocation
  int16_t musicBlock[256];
  int16_t *cry = nullptr;
  uint32_t cryLen = 0, cryAt = 0;
  MusicRoute route;
  bool suspended = false;
  int sfx = -1, note = 0;
  uint32_t noteAt = 0, phase = 0;
  for (;;) {
    AudioCommand c;
    if (xQueueReceive(gQ, &c, 0)) {
      if (c.kind == 2) { free(cry); cry = c.pcm; cryLen = c.samples; cryAt = 0; }
      else if (c.id < SFX_COUNT) { sfx = c.id; note = 0; noteAt = phase = 0; }
    }
    bool audible = gOn.load() && !gSleeping.load();
    if (!audible) { sfx = -1; free(cry); cry = nullptr; cryLen = 0; }
    size_t musicSamples = 0;
    uint32_t upload = uploadRequest.load();
    uint32_t request = musicRequest.load(), reload = musicReload.load();
    {
      // Sprite loads and USB writes share the card. Never block I2S on a lock:
      // use read-ahead while busy, then silence without losing the cursor.
      SdCardLock lock(0);
      if (lock) {
        if (upload & 1u) {
          music.close(); suspended = true;
          uploadAck.store(upload); // writer may now safely replace a WAV
        } else {
          uploadAck.store(upload);
          if (suspended || route.changed(request, reload)) {
            uint32_t resume = route.switchTo(request, reload, music.position(), music.valid());
            music.close();
            suspended = false;
            if (musicEnabled.load()) {
              const char *path = request & 1u ? "/mons/battle_wild.wav" : "/mons/bgm.wav";
              if (!music.open(SD_MMC.open(path, FILE_READ), resume))
                Serial.printf("AUDIO invalid/missing WAV: %s\n", path);
            }
          }
          if (audible && !musicPaused.load()) musicSamples = music.read(musicBlock, 256);
        }
      } else if (audible && !musicPaused.load() && !(upload & 1u) && !route.changed(request, reload) && !suspended) {
        musicSamples = music.read(musicBlock, 256, false);
      }
    }
    for (int i = 0; i < 256; ++i) {
      int32_t sample = i < (int)musicSamples ?
          (int32_t)musicBlock[i] * levels[0].load() / 100 : 0;
      if (audible && cry && cryAt < cryLen) {
        sample += (int32_t)cry[cryAt++] * levels[1].load() / 100;
        if (cryAt == cryLen) { free(cry); cry = nullptr; cryLen = 0; }
      }
      if (audible && sfx >= 0) {
        const Note &n = SFX[sfx].n[note];
        uint32_t total = (uint32_t)SAMPLE_RATE * n.ms / 1000;
        if (n.f) {
          phase += n.f;
          phase %= SAMPLE_RATE;
          int32_t v = phase < SAMPLE_RATE / 2 ? 5000 : -5000;
          uint32_t ramp = noteAt < 64 ? noteAt : total - noteAt < 64 ? total - noteAt : 64;
          sample += v * (int32_t)ramp / 64 * levels[2].load() / 100;
        }
        if (++noteAt >= total) {
          noteAt = phase = 0;
          if (++note >= SFX[sfx].len) sfx = -1;
        }
      }
      if (sample > 32767) sample = 32767;
      if (sample < -32768) sample = -32768;
      out[i * 2] = out[i * 2 + 1] = (int16_t)sample;
    }
    size_t sent = 0;
    while (sent < sizeof(out)) {
      size_t written = i2s.write((uint8_t *)out + sent, sizeof(out) - sent);
      if (!written) { vTaskDelay(1); break; }
      sent += written;
    }
  }
}

// Short cry samples retain the v1.18 bounded PCM loader; music uses WavStream.
static void queueWav(const char *path, uint8_t kind) {
  if (!gReady || !gQ) return;
  SdCardLock lock;
  if (!lock) return;
  File f = SD_MMC.open(path, FILE_READ);
  uint8_t h[44];
  if (!f || f.read(h, 44) != 44) return;
  auto u16 = [&](int p) { return (uint16_t)(h[p] | h[p+1] << 8); };
  auto u32 = [&](int p) { return (uint32_t)h[p] | (uint32_t)h[p+1] << 8 |
                               (uint32_t)h[p+2] << 16 | (uint32_t)h[p+3] << 24; };
  uint32_t bytes = u32(40);
  if (memcmp(h,"RIFF",4) || memcmp(h+8,"WAVEfmt ",8) || u32(16) != 16 ||
      u16(20) != 1 || u16(22) != 1 || u32(24) != SAMPLE_RATE ||
      u16(34) != 16 || memcmp(h+36,"data",4) || !bytes || bytes % 2 ||
      bytes > SAMPLE_RATE * 2 * 30 || bytes > f.size() - 44) return;
  int16_t *pcm = (int16_t *)ps_malloc(bytes);
  if (!pcm) return;
  if (f.read((uint8_t *)pcm, bytes) != bytes) { free(pcm); return; }
  AudioCommand c{kind, 0, pcm, bytes / 2};
  if (!xQueueSend(gQ, &c, 0)) free(pcm);
}
void audioLoadMusic() { musicEnabled = true; musicReload.fetch_add(1); }
void audioSetMusicPaused(bool paused) { musicPaused = paused; }
void audioSetBattleMusic(bool active, bool newSession) {
  uint32_t value = musicRequest.load();
  if (newSession) value = (value & ~1u) + 2;
  musicRequest.store((value & ~1u) | (active ? 1u : 0u));
}
bool audioPauseForUpload() {
  if (!gReady) return true;
  uint32_t ticket = uploadRequest.load();
  if (!(ticket & 1u)) ticket = uploadRequest.fetch_add(1) + 1;
  uint32_t start = millis();
  while (uploadAck.load() != ticket) {
    if (millis() - start >= 2000) return false;
    vTaskDelay(1);
  }
  return true;
}
void audioResumeAfterUpload() {
  if (uploadRequest.load() & 1u) uploadRequest.fetch_add(1);
}
void audioCry(uint16_t dex) {
  if (!gOn.load() || gSleeping.load() || !levels[1].load() || dex < 1 || dex > 251) return;  // ko10: gen 1 + 2
  char path[32]; snprintf(path, sizeof(path), "/mons/cry%03u.wav", dex);
  queueWav(path, 2);
}
void audioSetVolume(uint8_t channel, uint8_t percent) {
  if (channel >= 3) return;
  if (percent > 100) percent = 100;
  levels[channel] = percent;
  Preferences p; p.begin("tamapoke", false);
  p.putUChar(volumeKeys[channel], percent); p.end();
}
uint8_t audioVolume(uint8_t channel) { return channel < 3 ? levels[channel].load() : 0; }

void audioBegin() {
  // I2S primero: arranca el MCLK que necesita el códec para engancharse
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);   // hasta saber si el sonido esta activado

  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S begin fallo");
    return;
  }
  if (!es8311Init()) { Serial.println("ES8311 no responde (audio off)"); return; }

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  for (int i = 0; i < 3; ++i) { uint8_t v = p.getUChar(volumeKeys[i], levels[i].load()); levels[i] = v > 100 ? 100 : v; }
  p.end();

  gQ = xQueueCreate(8, sizeof(AudioCommand));
  if (!gQ) return;
  if (xTaskCreatePinnedToCore(audioTask, "audio", 6144, nullptr, 1, nullptr, 0) != pdPASS) {
    vQueueDelete(gQ); gQ = nullptr; return;
  }
  gReady = true;
  updateAmplifierPower();
  sfxPlay(SFX_HATCH);  // jingle de arranque (confirma que suena)
}

void sfxPlay(uint8_t id) {
  AudioCommand c{0, id, nullptr, 0};
  if (gReady && gOn.load() && !gSleeping.load() && gQ) xQueueSend(gQ, &c, 0);  // descarta si la cola esta llena
}

void audioSetEnabled(bool on) {
  gOn = on;
  updateAmplifierPower();
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { return gOn; }

void audioSetSleeping(bool sleeping) {
  if (gSleeping == sleeping) return;
  gSleeping = sleeping;
  updateAmplifierPower();  // durmiendo no hay efectos: amp apagado, sin consumo
}

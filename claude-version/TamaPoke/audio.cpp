#include "audio.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Preferences.h>
#include "bgm_pick.h"
#include <SD_MMC.h>
#include <dirent.h>
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
// ko11.31.4: cuanto lleva la musica leido por adelantado (bytes; 0xFFFFFFFF = no suena) y cuando se
// miro. La tarea de audio lo renueva en cada bloque (16 ms); los demas restan lo que se ha gastado
// desde entonces (32 bytes por ms) y solo leen de la SD si aun quedan mas de MUSIC_SD_MARGIN
#define MUSIC_RING 32768
#define MUSIC_SD_MARGIN 12288  // ~380 ms
static std::atomic<uint32_t> gMusicBuf{0xFFFFFFFFu};
static std::atomic<uint32_t> gMusicBufAt{0};
bool audioSdFree() {
  uint32_t b = gMusicBuf.load();
  if (b == 0xFFFFFFFFu) return true;
  uint32_t used = (millis() - gMusicBufAt.load()) * (SAMPLE_RATE * 2 / 1000);
  return b > used && b - used >= MUSIC_SD_MARGIN;
}

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
static std::atomic<uint8_t> musicTrack{MT_NORMAL};  // ko11: gimnasio / liga / salon
static const char *const volumeKeys[] = {"volBgm", "volCry", "volSfx"};
// ko11.8: fondos normales elegibles (bgm.wav, bgm2.wav ... bgm8.wav)
static std::atomic<uint8_t> bgmAvail{1}, bgmMaskA{0xFF};
static std::atomic<int8_t> bgmForce{-1}, bgmNowA{-1};
static uint16_t bgmSecs[BGM_MAX];
static char bgmTitles[BGM_MAX][28];
static char bgmPaths[BGM_MAX][48];  // ko11.8.1: ruta real (el nombre puede venir cambiado)

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
  static WavStream<File, MUSIC_RING> music; // ko11.31.4: 32 KiB (1 s) read-ahead, never a whole-song allocation
  int16_t musicBlock[256];
  int16_t *cry = nullptr;
  uint32_t cryLen = 0, cryAt = 0;
  MusicRoute route;
  bool suspended = false;
  // ko11: el salon de la fama suena fame.wav / fame2.wav al azar.
  // ko11.8: la musica normal, una al azar de las activadas en la pantalla de
  // sonido (bgm.wav, bgm2.wav ... bgm8.wav); al acabar una se sortea otra
  uint8_t bgmIdx = 0, famePick = 0, seenMask = bgmMaskA.load();
  bool bgmSwap = false;
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
    // ko11: la pista va en los bits altos: cambiarla reabre (y no reanuda la de salvaje)
    uint32_t request = (musicRequest.load() & 0x00FFFFFFu) | ((uint32_t)musicTrack.load() << 24);
    uint32_t reload = musicReload.load();
    // ko11.32: las pistas opcionales que no estan (story_*.wav, fame2.wav...) solo se buscan una
    // vez: cada SD_MMC.exists() de un fichero que falta recorre la carpeta mons entera.
    // Se olvida al recargar la musica o al recibir ficheros
    static const char *noFile[12];
    static uint8_t noFileN = 0;
    static uint32_t noFileGen = 0xFFFFFFFFu;
    if (noFileGen != reload || (upload & 1u)) { noFileN = 0; noFileGen = reload; }
    auto haveFile = [&](const char *p) {
      for (uint8_t i = 0; i < noFileN; i++) if (noFile[i] == p) return false;
      bool ok = SD_MMC.exists(p);
      if (!ok && noFileN < 12) noFile[noFileN++] = p;
      return ok;
    };
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
          // ko11.8: escuchar una desde el menu / se desactivo la que sonaba
          if (!(request & 1u) && (uint8_t)(request >> 24) == MT_NORMAL) {
            int8_t f = bgmForce.exchange(-1);
            uint8_t m = bgmMaskA.load();
            if (f >= 0 && f < BGM_MAX) { bgmIdx = (uint8_t)f; bgmSwap = true; }
            else if (m != seenMask && !(m & (1u << bgmIdx)) && (bgmAvail.load() & m)) {
              bgmIdx = bgmChoose(bgmAvail.load(), m, bgmIdx, esp_random()); bgmSwap = true;
            }
            seenMask = m;
          }
          if (suspended || bgmSwap || route.changed(request, reload)) {
            if (!bgmSwap && route.changed(request, reload) && !(request & 1u)) {
              bgmIdx = bgmChoose(bgmAvail.load(), bgmMaskA.load(), BGM_MAX, esp_random());
              famePick = (uint8_t)(esp_random() & 1u);
            }
            bgmSwap = false;
            uint32_t resume = route.switchTo(request, reload, music.position(), music.valid());
            music.close();
            suspended = false;
            if (musicEnabled.load()) {
              uint8_t tr = (uint8_t)(request >> 24);
              const char *base = request & 1u ? "/mons/battle_wild.wav" : "/mons/bgm.wav";
              const char *path = base;
              static char bgmPathBuf[48];
              // ko11.24: la historia prueba varios ficheros en orden (el primero que haya)
              const char *cand[3] = { nullptr, nullptr, nullptr };
              if (request & 1u) {
                if (tr == MT_SBATTLE) cand[0] = "/mons/story_battle.wav";
                else if (tr == MT_SROCKET) { cand[0] = "/mons/story_rocket.wav"; cand[1] = "/mons/story_battle.wav"; }
                else if (tr == MT_SGYM) { cand[0] = "/mons/story_gym.wav"; cand[1] = "/mons/battle_gym.wav"; }
              } else if (tr == MT_STORY) { cand[0] = "/mons/story.wav"; cand[1] = "/mons/bgm2.wav"; }
              else if (tr == MT_STORY_A) { cand[0] = "/mons/story_anime.wav"; cand[1] = "/mons/story.wav"; cand[2] = "/mons/bgm2.wav"; }
              else if (tr == MT_SLEAGUE) { cand[0] = "/mons/story_league.wav"; cand[1] = "/mons/story.wav"; cand[2] = "/mons/bgm2.wav"; }
              else if (tr == MT_STORY_END) { cand[0] = "/mons/story_end.wav"; cand[1] = "/mons/fame2.wav"; cand[2] = "/mons/fame.wav"; }
              if (cand[0]) {
                for (int k = 0; k < 3 && cand[k]; k++)
                  if (haveFile(cand[k])) { path = cand[k]; break; }
              } else if (request & 1u) {
                if (tr == MT_GYM) path = "/mons/battle_gym.wav";
                else if (tr == MT_CHAMP) path = "/mons/battle_champ.wav";
              } else if (tr == MT_FAME) {
                path = famePick ? "/mons/fame2.wav" : "/mons/fame.wav";  // ko11: 2 al azar
                if (famePick && !haveFile(path)) path = "/mons/fame.wav";
              } else if (bgmIdx) {
                audioBgmPath(bgmIdx, bgmPathBuf, sizeof(bgmPathBuf));  // ko11.8
                path = bgmPathBuf;
              }
              bool opened = music.open(SD_MMC.open(path, FILE_READ), resume);
              if (!opened && path != base) {  // ko11: sin ese fichero, la de siempre
                path = base;
                if (!(request & 1u) && tr == MT_NORMAL) bgmIdx = 0;
                opened = music.open(SD_MMC.open(path, FILE_READ), resume);
              }
              bgmNowA.store(opened && !(request & 1u) && tr == MT_NORMAL ? (int8_t)bgmIdx : (int8_t)-1);
              if (!opened)
                Serial.printf("AUDIO invalid/missing WAV: %s\n", path);
              else {  // ko10.4: la duracion real del fichero (para ver si esta recortado)
                uint32_t sec = music.lengthBytes() / (SAMPLE_RATE * 2);
                Serial.printf("AUDIO %s: %u:%02u\n", path, (unsigned)(sec / 60), (unsigned)(sec % 60));
              }
            }
          }
          if (audible && !musicPaused.load()) {
            uint32_t before = music.loops;
            musicSamples = music.read(musicBlock, 256);
            // ko11: acabo una cancion normal: sortear la siguiente
            uint8_t trk = (uint8_t)(request >> 24);
            if (music.loops != before && !(request & 1u)) {
              if (trk == MT_NORMAL) {  // ko11.8: entre las activadas, sin repetir si hay otra
                uint8_t next = bgmChoose(bgmAvail.load(), bgmMaskA.load(), bgmIdx, esp_random());
                if (next != bgmIdx) { bgmIdx = next; bgmSwap = true; }
              } else if (trk == MT_FAME) {
                uint8_t next = (uint8_t)(esp_random() & 1u);
                if (next != famePick) { famePick = next; bgmSwap = true; }
              }
            }
          }
        }
      } else if (audible && !musicPaused.load() && !(upload & 1u) && !route.changed(request, reload) && !suspended) {
        musicSamples = music.read(musicBlock, 256, false);
      }
    }
    // ko11.31.3: anillo casi lleno (o sin musica): los demas pueden leer de la SD
    gMusicBuf.store(!music.valid() || !audible || musicPaused.load() ? 0xFFFFFFFFu : (uint32_t)music.buffered());
    gMusicBufAt.store(millis());
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
void audioLoadMusic() { audioScanBgm(); musicEnabled = true; musicReload.fetch_add(1); }

// ---- ko11.8: fondos normales elegibles ----
void audioBgmPath(uint8_t i, char *out, size_t n) {
  if (i < BGM_MAX && bgmPaths[i][0]) snprintf(out, n, "%s", bgmPaths[i]);
  else if (i == 0) snprintf(out, n, "/mons/bgm.wav");
  else snprintf(out, n, "/mons/bgm%u.wav", (unsigned)(i + 1));
}
// ko11.8.1: se busca en /mons (y en la raiz, por si se copio ahi desde el PC)
// cualquier bgm*.wav, aunque el nombre venga cambiado ("bgm2 (1).wav"). Antes solo
// se miraba el nombre exacto y un fichero renombrado al descargarlo no aparecia.
void audioScanBgm() {
  static char found[BGM_MAX][48];
  bool exactF[BGM_MAX] = {};
  for (uint8_t i = 0; i < BGM_MAX; i++) found[i][0] = 0;
  static const char *const DIRS[2] = { "/mons", "" };
  {
    SdCardLock lock(pdMS_TO_TICKS(2000));
    if (lock) {
      for (int d = 0; d < 2; d++) {
        char vdir[24];
        snprintf(vdir, sizeof(vdir), "/sdcard%s", DIRS[d]);
        DIR *dir = opendir(vdir);  // POSIX: solo nombres, sin abrir cada fichero (hay ~750)
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir)) != nullptr) {
          bool ex;
          int k = bgmSlotFromName(e->d_name, &ex);
          if (k < 0) continue;
          if (found[k][0] && (exactF[k] || !ex)) continue;  // ya hay uno mejor (o igual de bueno)
          if (snprintf(found[k], sizeof(found[k]), "%s/%s", DIRS[d], e->d_name) >= (int)sizeof(found[k])) continue;  // nombre largo: no cabe
          exactF[k] = ex;
        }
        closedir(dir);
      }
    }
  }
  uint8_t av = 0;
  static uint16_t secs[BGM_MAX];
  static char titles[BGM_MAX][28];
  for (uint8_t i = 0; i < BGM_MAX; i++) {
    secs[i] = 0; titles[i][0] = 0;
    if (!found[i][0]) continue;
    SdCardLock lock(pdMS_TO_TICKS(500));
    if (!lock) continue;
    File f = SD_MMC.open(found[i], FILE_READ);
    uint32_t bytes = 0;
    bool ok = f && wavInfo(f, &bytes, titles[i], sizeof(titles[i]));
    if (f) f.close();
    if (ok) { av |= (uint8_t)(1u << i); secs[i] = (uint16_t)(bytes / (SAMPLE_RATE * 2)); }
    else if (!f && !strcmp(found[i], bgmPaths[i]) && (bgmAvail.load() & (1u << i))) {
      // no se pudo abrir (p. ej. sonando ahora): se queda lo que ya se sabia
      av |= (uint8_t)(1u << i); secs[i] = bgmSecs[i]; memcpy(titles[i], bgmTitles[i], sizeof(titles[i]));
    }
    Serial.printf("BGM %u: %s %s %u s\n", (unsigned)(i + 1), found[i], (av & (1u << i)) ? "ok" : "NO VALE", secs[i]);
  }
  SdCardLock lock(pdMS_TO_TICKS(2000));  // la tarea de audio abre con el candado: copiar dentro
  for (uint8_t i = 0; i < BGM_MAX; i++) {
    memcpy(bgmPaths[i], found[i], sizeof(bgmPaths[i]));
    bgmSecs[i] = secs[i];
    memcpy(bgmTitles[i], titles[i], sizeof(bgmTitles[i]));
  }
  bgmAvail = av ? av : 1;  // sin ninguna valida: se intenta bgm.wav como siempre
  Serial.printf("BGM avail=0x%02x mask=0x%02x\n", av, bgmMaskA.load());
}
uint8_t audioBgmAvail() { return bgmAvail.load(); }
uint8_t audioBgmMask() { return bgmMaskA.load(); }
void audioSetBgmMask(uint8_t mask) {
  if (!mask) return;
  bgmMaskA = mask;
  Preferences p; p.begin("tamapoke", false); p.putUChar("bgmMask", mask); p.end();
}
uint16_t audioBgmSecondsOf(uint8_t i) { return i < BGM_MAX ? bgmSecs[i] : 0; }
const char *audioBgmTitle(uint8_t i) { return i < BGM_MAX ? bgmTitles[i] : ""; }
int8_t audioBgmNow() { return bgmNowA.load(); }
void audioBgmPlay(uint8_t i) { if (i < BGM_MAX) bgmForce = (int8_t)i; }
void audioSetMusicPaused(bool paused) { musicPaused = paused; }
void audioSetMusicTrack(uint8_t track) { musicTrack = track; }
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
  bgmMaskA = p.getUChar("bgmMask", 0xFF);  // ko11.8
  p.end();

  gQ = xQueueCreate(8, sizeof(AudioCommand));
  if (!gQ) return;
  // ko11.2: prioridad 5 (antes 1), por encima del tactil (2) en el mismo nucleo:
  // al aporrear en los entrenamientos el tactil lee el I2C cada 8 ms y dejaba a
  // la musica esperando. La tarea pasa casi todo el tiempo bloqueada en
  // i2s.write (colchon lleno), asi que no le quita tiempo a nadie
  if (xTaskCreatePinnedToCore(audioTask, "audio", 6144, nullptr, 5, nullptr, 0) != pdPASS) {
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

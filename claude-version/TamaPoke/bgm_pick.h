#pragma once
// ko11.8: elegir el fondo normal entre los que el usuario deja activados, y leer
// de un WAV su duracion y su titulo (LIST/INFO/INAM) sin cargarlo. Sin Arduino:
// se prueba en el PC (test/test_audio.cpp).
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static inline uint8_t bgmBits(uint8_t m) {
  uint8_t n = 0;
  for (; m; m &= (uint8_t)(m - 1)) n++;
  return n;
}

// Siguiente cancion: una al azar de (avail & mask). Con dos o mas candidatas no
// repite la actual. Si el usuario no deja ninguna valida, cualquiera de las que
// hay; si no hay ninguna, la 0 (bgm.wav, como siempre).
static inline uint8_t bgmChoose(uint8_t avail, uint8_t mask, uint8_t cur, uint32_t rnd) {
  uint8_t cand = avail & mask;
  if (!cand) cand = avail;
  if (!cand) return 0;
  if (bgmBits(cand) >= 2 && cur < 8) cand &= (uint8_t)~(1u << cur);
  uint8_t k = (uint8_t)(rnd % bgmBits(cand));
  for (uint8_t i = 0; i < 8; i++)
    if (cand & (1u << i)) {
      if (!k) return i;
      k--;
    }
  return 0;
}

// ko11.8.1: que hueco (0 = bgm.wav, 1 = bgm2.wav ... 7 = bgm8.wav) es un fichero,
// aunque el navegador o el PC le hayan cambiado un poco el nombre al guardarlo:
// "bgm2 (1).wav", "BGM2.WAV", "bgm2-1.wav", "bgm2_copy.wav" -> 1; "bgm.wav",
// "bgm1.wav", "bgm (2).wav" -> 0. -1 si no es un fondo (bgm10.wav, ._bgm2.wav...).
// *exact = el nombre es justo el canonico (tiene prioridad si hay dos iguales).
static inline int bgmSlotFromName(const char *name, bool *exact = nullptr) {
  if (exact) *exact = false;
  if (!name) return -1;
  auto low = [](char c) { return (char)(c >= 'A' && c <= 'Z' ? c + 32 : c); };
  if (low(name[0]) != 'b' || low(name[1]) != 'g' || low(name[2]) != 'm') return -1;
  size_t n = strlen(name);
  if (n < 7) return -1;
  const char *ext = name + n - 4;
  if (ext[0] != '.' || low(ext[1]) != 'w' || low(ext[2]) != 'a' || low(ext[3]) != 'v') return -1;
  const char *p = name + 3;
  int num = 0, digits = 0;
  while (p < ext && *p >= '0' && *p <= '9') { num = num * 10 + (*p - '0'); p++; digits++; }
  if (digits > 1) return -1;
  int slot = digits ? num - 1 : 0;
  if (slot < 0 || slot > 7) return -1;
  if (exact) *exact = (p == ext) && (digits ? num >= 2 : true);
  return slot;
}

// Cabecera de un WAV del juego (16 kHz mono 16 bit): bytes de audio y titulo.
// Reader: size(), seek(pos), read(buf, n) (File de SD_MMC o el de los tests).
template<class Reader>
bool wavInfo(Reader &f, uint32_t *dataBytes, char *title, size_t titleLen) {
  auto u32 = [](const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
  };
  if (title && titleLen) title[0] = 0;
  *dataBytes = 0;
  uint8_t h[12];
  if (f.size() < 12 || !f.seek(0) || f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4))
    return false;
  uint32_t end = u32(h + 4) + 8, pos = 12;
  if (end > f.size()) end = (uint32_t)f.size();
  bool fmt = false;
  while (end - pos >= 8) {
    if (!f.seek(pos) || f.read(h, 8) != 8) break;
    uint32_t bytes = u32(h + 4), start = pos + 8;
    if (bytes > end - start) bytes = end - start;
    if (!memcmp(h, "fmt ", 4)) {
      uint8_t q[16];
      if (bytes < 16 || f.read(q, 16) != 16) break;
      fmt = (q[0] | q[1] << 8) == 1 && (q[2] | q[3] << 8) == 1 && u32(q + 4) == 16000 &&
            (q[14] | q[15] << 8) == 16;
    } else if (!memcmp(h, "data", 4)) {
      *dataBytes = bytes;
    } else if (!memcmp(h, "LIST", 4) && bytes >= 4 && title && titleLen > 1) {
      uint8_t t[4];
      if (f.read(t, 4) == 4 && !memcmp(t, "INFO", 4)) {
        uint32_t p = start + 4, lend = start + bytes;
        while (lend - p >= 8 && lend >= p) {
          if (!f.seek(p) || f.read(h, 8) != 8) break;
          uint32_t sb = u32(h + 4);
          if (sb > lend - p - 8) break;
          if (!memcmp(h, "INAM", 4)) {
            size_t n = sb < titleLen - 1 ? sb : titleLen - 1;
            if (f.read((uint8_t *)title, n) != n) n = 0;
            title[n] = 0;
            while (n && (title[n - 1] == 0 || title[n - 1] == ' ')) title[--n] = 0;
            break;
          }
          p += 8 + sb + (sb & 1);
        }
      }
    }
    pos = start + bytes + (bytes & 1);
    if (pos > end) break;
  }
  return fmt && *dataBytes > 0;
}

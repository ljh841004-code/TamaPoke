#include "sdcheck.h"
#include <stdio.h>
#include <string.h>

const char *const SDC_MUSIC_FILES[SDC_MUSIC_N] = {
  "bgm.wav", "bgm2.wav", "battle_wild.wav", "battle_gym.wav", "battle_champ.wav", "fame.wav", "fame2.wav",
  "story.wav", "story_anime.wav", "story_battle.wav", "story_gym.wav", "story_rocket.wav", "story_league.wav",
  "story_end.wav",
};

static void setBit(uint8_t *b, unsigned i) { if (i < 256) b[i >> 3] |= (uint8_t)(1 << (i & 7)); }
static bool getBit(const uint8_t *b, unsigned i) { return i < 256 && ((b[i >> 3] >> (i & 7)) & 1); }

// "<pre>NNN.bin" con NNN en 001..251 -> NNN, si no 0
static unsigned num3(const char *s, const char *pre, const char *ext) {
  size_t lp = strlen(pre), le = strlen(ext);
  if (strncmp(s, pre, lp) || strlen(s) != lp + 3 + le || strcmp(s + lp + 3, ext)) return 0;
  unsigned v = 0;
  for (int i = 0; i < 3; i++) {
    char c = s[lp + i];
    if (c < '0' || c > '9') return 0;
    v = v * 10 + (unsigned)(c - '0');
  }
  return v;
}

void SdInv::add(const char *rel) {
  if (!rel) return;
  unsigned d;
  if ((d = num3(rel, "ps", ".bin")) && d <= SDC_DEX) { setBit(bits[SDC_SPRS], d - 1); return; }
  if ((d = num3(rel, "p", ".bin")) && d <= SDC_DEX) { setBit(bits[SDC_SPR], d - 1); return; }
  if ((d = num3(rel, "rs", ".bin")) && d <= SDC_DEX) { setBit(bits[SDC_BATS], d - 1); return; }
  if ((d = num3(rel, "r", ".bin")) && d <= SDC_DEX) { setBit(bits[SDC_BAT], d - 1); return; }
  if ((d = num3(rel, "cry", ".wav")) && d <= SDC_DEX) { setBit(bits[SDC_CRY], d - 1); return; }
  if (!strcmp(rel, "thumbs.bin")) { setBit(bits[SDC_THUMB], 0); return; }
  if (!strcmp(rel, "story.bin")) { setBit(bits[SDC_STORY], 0); return; }
  for (unsigned i = 0; i < SDC_MUSIC_N; i++)
    if (!strcmp(rel, SDC_MUSIC_FILES[i])) { setBit(bits[SDC_MUSIC], i); return; }
  // efectos: en mons/fx/ o sueltos en mons/ (el instalador web)
  const char *f = strncmp(rel, "fx/", 3) ? rel : rel + 3;
  if (strlen(f) == 9 && !strcmp(f + 5, ".bin") && f[0] == 'f' && f[1] >= '0' && f[1] <= '9' && f[2] >= '0' &&
      f[2] <= '9' && f[3] >= '0' && f[3] <= '2' && f[4] >= '0' && f[4] <= '2') {
    unsigned t = (unsigned)(f[1] - '0') * 10 + (unsigned)(f[2] - '0');
    if (t < 16) setBit(bits[SDC_FX], t * 9 + (unsigned)(f[3] - '0') * 3 + (unsigned)(f[4] - '0'));  // id - 1
  } else if ((d = num3(f, "m", ".bin")) && d >= 145 && d <= SDC_FX_N) {
    setBit(bits[SDC_FX], d - 1);
  }
}

uint16_t SdInv::need(uint8_t cat) {
  switch (cat) {
    case SDC_FX: return SDC_FX_N;
    case SDC_THUMB: case SDC_STORY: return 1;
    case SDC_MUSIC: return SDC_MUSIC_N;
    default: return cat < SDC_CATS ? SDC_DEX : 0;
  }
}

uint16_t SdInv::have(uint8_t cat) const {
  if (cat >= SDC_CATS) return 0;
  uint16_t n = 0;
  for (unsigned i = 0; i < need(cat); i++) n += getBit(bits[cat], i);
  return n;
}

bool SdInv::firstMissing(uint8_t cat, char *out, size_t n) const {
  if (cat >= SDC_CATS || !out || !n) return false;
  for (unsigned i = 0; i < need(cat); i++) {
    if (getBit(bits[cat], i)) continue;
    unsigned d = i + 1;
    switch (cat) {
      case SDC_SPR: snprintf(out, n, "p%03u.bin", d); break;
      case SDC_SPRS: snprintf(out, n, "ps%03u.bin", d); break;
      case SDC_BAT: snprintf(out, n, "r%03u.bin", d); break;
      case SDC_BATS: snprintf(out, n, "rs%03u.bin", d); break;
      case SDC_CRY: snprintf(out, n, "cry%03u.wav", d); break;
      case SDC_THUMB: snprintf(out, n, "thumbs.bin"); break;
      case SDC_STORY: snprintf(out, n, "story.bin"); break;
      case SDC_MUSIC: snprintf(out, n, "%s", SDC_MUSIC_FILES[i]); break;
      case SDC_FX:
        if (d <= 144) snprintf(out, n, "fx/f%02u%u%u.bin", i / 9, (i % 9) / 3, i % 3);
        else snprintf(out, n, "fx/m%03u.bin", d);
        break;
    }
    return true;
  }
  return false;
}

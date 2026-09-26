#include "box.h"
#include <string.h>

// ---------------------------------------------------------------- caja

void Box::begin() {
  memset(mons, 0, sizeof(mons));
  n = 0;
  prefs.begin("tpbox", false);
  uint8_t c = prefs.getUChar("n", 0);
  if (c > BOX_MAX) c = 0;
  if (c && prefs.getBytes("mons", mons, sizeof(BoxMon) * c) != sizeof(BoxMon) * c) c = 0;
  // nada invalido entra en juego aunque la NVS venga danada
  bool fixed = false;
  for (uint8_t i = 0; i < c; i++) {
    if (mons[i].dex < 1 || mons[i].dex > DexLog::N) continue;
    // fork KO (ko7): tope nivel 100. Los de ko6 (nivel por horas, Lv300+)
    // vuelven a empezar en Lv5
    if (mons[i].lvl > 100) { mons[i].lvl = 5; fixed = true; }
    mons[n++] = mons[i];
  }
  if (fixed) save();
}

void Box::save() {
  prefs.putUChar("n", n);
  if (n) prefs.putBytes("mons", mons, sizeof(BoxMon) * n);
  else prefs.remove("mons");
}

bool Box::add(int16_t dex, uint16_t lvl, bool shiny, bool caught, uint32_t epoch) {
  if (full() || dex < 1 || dex > DexLog::N) return false;
  BoxMon &m = mons[n++];
  m.dex = dex;
  m.lvl = lvl < 1 ? 1 : (lvl > 100 ? 100 : lvl);
  m.flags = (shiny ? BOXF_SHINY : 0) | (caught ? BOXF_CAUGHT : 0);
  m.geneAtk = 90 + random(21);
  m.geneDef = 90 + random(21);
  m.geneSpe = 90 + random(21);
  m.epoch = epoch;
  save();
  return true;
}

bool Box::addRaised(int16_t dex, uint16_t lvl, bool shiny, uint8_t gA, uint8_t gD, uint8_t gS,
                    uint32_t epoch) {
  if (full() || dex < 1 || dex > DexLog::N) return false;
  BoxMon &m = mons[n++];
  m.dex = dex;
  m.lvl = lvl < 1 ? 1 : (lvl > 100 ? 100 : lvl);
  m.flags = (shiny ? BOXF_SHINY : 0) | BOXF_RAISED;
  m.geneAtk = gA; m.geneDef = gD; m.geneSpe = gS;
  m.epoch = epoch;
  save();
  return true;
}

bool Box::take(uint8_t i, BoxMon &out) {
  if (i >= n) return false;
  out = mons[i];
  return release(i);
}

bool Box::release(uint8_t i) {
  if (i >= n) return false;
  memmove(&mons[i], &mons[i + 1], sizeof(BoxMon) * (n - i - 1));
  n--;
  save();
  return true;
}

int Box::pickRandom() const { return n ? (int)random(n) : -1; }

void Box::wipe() {
  prefs.clear();
  memset(mons, 0, sizeof(mons));
  n = 0;
}

// ---------------------------------------------------------------- pokedex

void DexLog::begin() {
  memset(first, 0, sizeof(first));
  memset(seenN, 0, sizeof(seenN));
  memset(caughtN, 0, sizeof(caughtN));
  prefs.begin("tpdex", false);
  // ko10: se aceptan los de 151 (ko4-ko9) y los de 251; lo demas, corrupto
  auto load = [&](const char *k, void *buf, size_t el, size_t cap) {
    size_t n = prefs.getBytes(k, buf, cap);
    if (n != 151 * el && n != cap) memset(buf, 0, cap);
  };
  load("first", first, sizeof(first[0]), sizeof(first));
  load("seen", seenN, sizeof(seenN[0]), sizeof(seenN));
  load("caught", caughtN, sizeof(caughtN[0]), sizeof(caughtN));
}

void DexLog::wipe() {
  prefs.clear();
  memset(first, 0, sizeof(first));
  memset(seenN, 0, sizeof(seenN));
  memset(caughtN, 0, sizeof(caughtN));
}

void DexLog::save() {
  prefs.putBytes("first", first, sizeof(first));
  prefs.putBytes("seen", seenN, sizeof(seenN));
  prefs.putBytes("caught", caughtN, sizeof(caughtN));
}

void DexLog::mark(int16_t dex, uint32_t epoch) {
  if (!first[dex - 1] && epoch) first[dex - 1] = epoch;
  if (seenN[dex - 1] < 65535) seenN[dex - 1]++;
}

void DexLog::seen(int16_t dex, uint32_t epoch) {
  if (!ok(dex)) return;
  mark(dex, epoch);
  save();
}

void DexLog::caught(int16_t dex, uint32_t epoch) {
  if (!ok(dex)) return;
  if (!first[dex - 1] && epoch) first[dex - 1] = epoch;
  if (caughtN[dex - 1] < 65535) caughtN[dex - 1]++;
  save();
}

#include "box.h"
#include "battle.h"
#include <string.h>

// ---------------------------------------------------------------- caja

static const char *gBigPart = nullptr;
void setBigPart(const char *label) { gBigPart = label; }
const char *bigPart() { return gBigPart; }

void Box::begin() {
  memset(mons, 0, sizeof(mons));
  n = 0;
  const char *part = bigPart();
  prefs.begin(ns_, false, part);
  // ko11.6: primera vez en la particion grande: se trae lo guardado en la NVS de
  // siempre y SOLO si se copio bien se borra alli ("n" va lo ultimo: es la marca)
  if (part && !prefs.isKey("n")) {
    Preferences old;
    old.begin(ns_, false);
    if (old.isKey("n")) {
      uint8_t oc = old.getUChar("n", 0);
      size_t len = old.getBytesLength("mons");  // tal cual (puede ser el formato de antes)
      if (oc > cap_ || len > sizeof(mons)) len = 0;
      bool ok = true;
      if (len) ok = old.getBytes("mons", mons, len) == len && prefs.putBytes("mons", mons, len) == len;
      if (ok) ok = prefs.putUChar("n", len ? oc : 0) == 1;
      if (ok) old.clear();
      memset(mons, 0, sizeof(mons));
    }
    old.end();
  }
  uint8_t c = prefs.getUChar("n", 0);
  if (c > cap_) c = 0;
  bool fixed = false;
  size_t have = c ? prefs.getBytesLength("mons") : 0;
  if (c && have == (size_t)BOXMON_OLD_SIZE * c) {
    // ko11.31: fichas sin movimientos: se leen tal cual y cada uno recibe los de su especie
    static uint8_t raw[BOX_CAP_MAX * BOXMON_OLD_SIZE];
    if (prefs.getBytes("mons", raw, have) != have) c = 0;
    for (uint8_t i = 0; i < c; i++) {
      memcpy(&mons[i], raw + (size_t)i * BOXMON_OLD_SIZE, BOXMON_OLD_SIZE);
      movesDefault(mons[i].dex, mons[i].lvl, mons[i].mv);
    }
    fixed = c > 0;
  } else if (c && (have != sizeof(BoxMon) * c ||
                   prefs.getBytes("mons", mons, sizeof(BoxMon) * c) != sizeof(BoxMon) * c)) {
    c = 0;
  }
  // nada invalido entra en juego aunque la NVS venga danada
  for (uint8_t i = 0; i < c; i++) {
    if (mons[i].dex < 1 || mons[i].dex > DexLog::N) continue;
    for (uint8_t k = 0; k < 4; k++)
      if (mons[i].mv[k] && !moveValid(mons[i].mv[k])) { mons[i].mv[k] = 0; fixed = true; }
    if (!moveCount(mons[i].mv)) { movesDefault(mons[i].dex, mons[i].lvl, mons[i].mv); fixed = true; }
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

bool Box::add(int16_t dex, uint16_t lvl, bool shiny, bool caught, uint32_t epoch, const uint8_t *mv) {
  if (full() || dex < 1 || dex > DexLog::N) return false;
  BoxMon &m = mons[n++];
  m.dex = dex;
  m.lvl = lvl < 1 ? 1 : (lvl > 100 ? 100 : lvl);
  m.flags = (shiny ? BOXF_SHINY : 0) | (caught ? BOXF_CAUGHT : 0);
  m.geneAtk = 90 + random(21);
  m.geneDef = 90 + random(21);
  m.geneSpe = 90 + random(21);
  m.epoch = epoch;
  if (mv && moveCount(mv)) memcpy(m.mv, mv, 4);  // ko11.31.3: los de cuando se capturo
  else movesDefault(dex, m.lvl, m.mv);  // ko11.31
  save();
  return true;
}

bool Box::addRaised(int16_t dex, uint16_t lvl, bool shiny, uint8_t gA, uint8_t gD, uint8_t gS,
                    uint32_t epoch, const uint8_t *mv) {
  if (full() || dex < 1 || dex > DexLog::N) return false;
  BoxMon &m = mons[n++];
  m.dex = dex;
  m.lvl = lvl < 1 ? 1 : (lvl > 100 ? 100 : lvl);
  m.flags = (shiny ? BOXF_SHINY : 0) | BOXF_RAISED;
  m.geneAtk = gA; m.geneDef = gD; m.geneSpe = gS;
  m.epoch = epoch;
  if (mv && moveCount(mv)) memcpy(m.mv, mv, 4);  // ko11.31: los que sabia
  else movesDefault(dex, m.lvl, m.mv);
  save();
  return true;
}

bool Box::take(uint8_t i, BoxMon &out) {
  if (i >= n) return false;
  out = mons[i];
  return release(i);
}

bool Box::put(const BoxMon &m) {
  if (full() || m.dex < 1 || m.dex > DexLog::N) return false;
  mons[n++] = m;
  if (!moveCount(mons[n - 1].mv)) movesDefault(m.dex, m.lvl, mons[n - 1].mv);  // ko11.31
  save();
  return true;
}

bool Box::set(uint8_t i, const BoxMon &m) {
  if (i >= n) return false;
  mons[i] = m;
  save();
  return true;
}

void Box::markFlag(uint8_t i, uint8_t f) {
  if (i >= n || (mons[i].flags & f) == f) return;
  mons[i].flags |= f;
  save();
}

bool Box::bumpLevel(uint8_t i, uint16_t cap) {
  if (i >= n || mons[i].lvl >= cap || mons[i].lvl >= 100) return false;
  mons[i].lvl++;
  save();
  return true;
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
  const char *part = bigPart();
  prefs.begin("tpdex", false, part);
  // ko11.6: primera vez en la particion grande: traer la pokedex de la NVS de siempre
  if (part && !prefs.isKey("mig")) {
    Preferences old;
    old.begin("tpdex", false);
    static const char *const K[3] = { "first", "seen", "caught" };
    void *const B[3] = { first, seenN, caughtN };
    const size_t S[3] = { sizeof(first), sizeof(seenN), sizeof(caughtN) };
    bool ok = true;
    for (int i = 0; i < 3 && ok; i++) {
      if (!old.isKey(K[i])) continue;
      size_t got = old.getBytes(K[i], B[i], S[i]);
      ok = got > 0 && prefs.putBytes(K[i], B[i], got) == got;
    }
    if (ok && prefs.putUChar("mig", 1) == 1) old.clear();
    old.end();
    memset(first, 0, sizeof(first));
    memset(seenN, 0, sizeof(seenN));
    memset(caughtN, 0, sizeof(caughtN));
  }
  // ko10: se aceptan los de 151 (ko4-ko9) y los de 251; lo demas, corrupto
  auto load = [&](const char *k, void *buf, size_t el, size_t cap) {
    size_t n = prefs.getBytes(k, buf, cap);
    if (n != 151 * el && n != cap) memset(buf, 0, cap);
  };
  load("first", first, sizeof(first[0]), sizeof(first));
  load("seen", seenN, sizeof(seenN[0]), sizeof(seenN));
  load("caught", caughtN, sizeof(caughtN[0]), sizeof(caughtN));
  memset(lrn, 0, sizeof(lrn));
  if (prefs.getBytesLength("lrn") == sizeof(lrn)) prefs.getBytes("lrn", lrn, sizeof(lrn));
}

bool DexLog::learned(int16_t dex, uint8_t id, bool save) {
  if (!ok(dex) || !id || id / 8 >= LRN_BYTES || hasLearned(dex, id)) return false;
  lrn[dex - 1][id / 8] |= (uint8_t)(1 << (id % 8));
  if (save) saveLearned();
  return true;
}
void DexLog::saveLearned() { prefs.putBytes("lrn", lrn, sizeof(lrn)); }

void DexLog::wipe() {
  prefs.clear();
  memset(lrn, 0, sizeof(lrn));
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

#include "collection.h"
#include <string.h>

// Saved by the first collection build, before battle XP was stored separately.
struct LegacyStoredMon {
  uint32_t ageMinutes;
  uint16_t speciesId, medals;
  uint8_t fullness, joy, energy, hygiene;
  uint8_t poops, weight, bond, careMistakes;
  uint8_t geneAtk, geneDef, geneSpe;
  uint8_t trAtk, trDef, trSpe;
  uint8_t shiny, berryKnown;
  char nick[12];
};
void Collection::begin() {
  memset(mons, 0, sizeof(mons));
  prefs.begin("tpbox", false);
  uint8_t version = prefs.getUChar("version", 0);
  if (version == 2) {
    if (prefs.getBytes("mons", mons, sizeof(mons)) != sizeof(mons))
      memset(mons, 0, sizeof(mons));
  } else if (version == 1) {
    static LegacyStoredMon old[151];
    if (prefs.getBytes("mons", old, sizeof(old)) == sizeof(old)) {
      for (int i = 0; i < 151; i++) {
        const LegacyStoredMon &s = old[i];
        StoredMon &d = mons[i];
        d.ageMinutes = s.ageMinutes; d.speciesId = s.speciesId;
        d.medals = s.medals;
        d.fullness = s.fullness; d.joy = s.joy;
        d.energy = s.energy; d.hygiene = s.hygiene;
        d.poops = s.poops; d.weight = s.weight;
        d.bond = s.bond; d.careMistakes = s.careMistakes;
        d.geneAtk = s.geneAtk; d.geneDef = s.geneDef; d.geneSpe = s.geneSpe;
        d.trAtk = s.trAtk; d.trDef = s.trDef; d.trSpe = s.trSpe;
        d.shiny = s.shiny; d.berryKnown = s.berryKnown;
        memcpy(d.nick, s.nick, sizeof(d.nick));
        d.nick[sizeof(d.nick) - 1] = 0;
      }
      save();
    }
  }
  for (int i = 0; i < 151; i++) {
    if (mons[i].speciesId != i + 1) mons[i].speciesId = 0;
  }
}

void Collection::clear() {
  memset(mons, 0, sizeof(mons));
  prefs.clear();
}

void Collection::save() {
  prefs.putBytes("mons", mons, sizeof(mons));
  prefs.putUChar("version", 2);
}

bool Collection::has(int16_t dex) const {
  return dex >= 1 && dex <= 151 && mons[dex - 1].speciesId == dex;
}

const StoredMon *Collection::get(int16_t dex) const {
  return has(dex) ? &mons[dex - 1] : nullptr;
}

uint16_t Collection::count() const {
  uint16_t n = 0;
  for (int16_t dex = 1; dex <= 151; dex++) if (has(dex)) n++;
  return n;
}

void Collection::remember(const Pet &pet) {
  if (pet.isEgg() || pet.speciesId > 151) return;
  StoredMon &m = mons[pet.speciesId - 1];
  m.speciesId = pet.speciesId;
  m.ageMinutes = pet.ageMinutes;
  m.battleXpMinutes = pet.battleXpMinutes;
  m.medals = pet.medals;
  m.fullness = pet.fullness; m.joy = pet.joy;
  m.energy = pet.energy; m.hygiene = pet.hygiene;
  m.poops = pet.poops; m.weight = pet.weight;
  m.bond = pet.bond; m.careMistakes = pet.careMistakes;
  m.geneAtk = pet.geneAtk; m.geneDef = pet.geneDef; m.geneSpe = pet.geneSpe;
  m.trAtk = pet.trAtk; m.trDef = pet.trDef; m.trSpe = pet.trSpe;
  m.shiny = pet.shiny; m.berryKnown = pet.berryKnown;
  memcpy(m.nick, pet.nick, sizeof(m.nick));
}

bool Collection::catchWild(Pet &pet, int16_t dex, uint16_t level, bool shiny) {
  if (dex < 1 || dex > 151) return false;
  pet.registerCaught(dex, shiny);
  if (has(dex)) return true;  // keep the first captured individual
  StoredMon &m = mons[dex - 1];
  m = StoredMon();
  m.speciesId = dex;
  m.ageMinutes = (uint32_t)(level > 0 ? level - 1 : 0) * MINUTES_PER_LEVEL;
  m.shiny = shiny;
  m.geneAtk = 90 + random(21);
  m.geneDef = 90 + random(21);
  m.geneSpe = 90 + random(21);
  save();
  return true;
}

bool Collection::deposit(Pet &pet) {
  if (pet.isEgg() || pet.ceremony != CER_NONE || pet.evolving()) return false;
  remember(pet);
  save();
  pet.newEgg();
  return true;
}

bool Collection::activate(Pet &pet, int16_t dex) {
  if (!has(dex) || pet.ceremony != CER_NONE || pet.evolving()) return false;
  StoredMon selected = mons[dex - 1];
  if (!pet.isEgg()) remember(pet);
  else mons[dex - 1].speciesId = 0;
  if (!pet.isEgg() && pet.speciesId != dex) mons[dex - 1].speciesId = 0;
  pet.speciesId = selected.speciesId;
  pet.prevSpeciesId = -1;
  pet.ageMinutes = selected.ageMinutes;
  pet.battleXpMinutes = selected.battleXpMinutes;
  pet.medals = selected.medals;
  pet.fullness = selected.fullness; pet.joy = selected.joy;
  pet.energy = selected.energy; pet.hygiene = selected.hygiene;
  pet.poops = selected.poops; pet.weight = selected.weight;
  pet.bond = selected.bond; pet.careMistakes = selected.careMistakes;
  pet.geneAtk = selected.geneAtk; pet.geneDef = selected.geneDef;
  pet.geneSpe = selected.geneSpe;
  pet.trAtk = selected.trAtk; pet.trDef = selected.trDef; pet.trSpe = selected.trSpe;
  pet.shiny = selected.shiny; pet.berryKnown = selected.berryKnown;
  memcpy(pet.nick, selected.nick, sizeof(pet.nick));
  pet.nick[sizeof(pet.nick) - 1] = 0;
  pet.sleeping = false;
  pet.resetAfterSwap();
  save();
  return true;
}

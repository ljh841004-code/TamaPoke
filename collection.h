#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "pet.h"

// One stored individual per Kanto species. The active pet lives only in Pet.
struct StoredMon {
  uint32_t ageMinutes = 0;
  uint32_t battleXpMinutes = 0;
  uint16_t speciesId = 0;  // 0 means empty
  uint16_t medals = 0;
  uint8_t fullness = 80, joy = 80, energy = 80, hygiene = 100;
  uint8_t poops = 0, weight = 0, bond = 0, careMistakes = 0;
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t shiny = 0, berryKnown = 0;
  char nick[12] = "";
};

class Collection {
public:
  void begin();
  void clear();
  bool has(int16_t dex) const;
  uint16_t count() const;
  bool catchWild(Pet &pet, int16_t dex, uint16_t level, bool shiny);
  bool deposit(Pet &pet);
  bool activate(Pet &pet, int16_t dex);
  const StoredMon *get(int16_t dex) const;
private:
  Preferences prefs;
  StoredMon mons[151];
  void remember(const Pet &pet);
  void save();
};

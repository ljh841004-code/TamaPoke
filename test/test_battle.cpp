#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../battle.h"
#include "../dex.h"

static Pet fighter(uint16_t level = 1) {
  mockSetMillis(0);
  mockClearForcedRandom();
  Pet p;
  p.speciesId = 1;
  p.ageMinutes = (level - 1) * (uint32_t)MINUTES_PER_LEVEL;
  return p;
}

static void finishTurn(Battle &b, uint32_t start) {
  b.update(start + Battle::STEP_MS);
  b.update(start + 2 * Battle::STEP_MS);
}

TEST(battle, rejects_ineligible_pet_and_reentry) {
  Pet p;
  Battle b;
  CHECK(!b.begin(p));
  p = fighter(); p.sleeping = true;
  CHECK(!b.begin(p));
  p.sleeping = false; p.ceremony = CER_FAREWELL;
  CHECK(!b.begin(p));
  p.ceremony = CER_NONE; p.speciesId = 152;
  CHECK(!b.begin(p));
  p.speciesId = 1;
  CHECK(b.begin(p)); CHECK(!b.begin(p));
  b.end(); CHECK(!b.active()); CHECK(b.begin(p));
}

TEST(battle, snapshot_matches_pet_and_all_levels_are_safe) {
  for (uint16_t lv : {1, 255, 256, 999}) {
    Pet p = fighter(lv);
    p.geneAtk = 110; p.trAtk = 100; p.trDef = 90; p.trSpe = 80;
    Battle b; CHECK(b.begin(p));
    CHECK_EQ(b.player.atk, p.atkStat()); CHECK_EQ(b.player.def, p.defStat());
    CHECK_EQ(b.player.spe, p.speStat()); CHECK_EQ(b.player.level, lv);
    CHECK_EQ(b.player.maxHp, DEX_TBL[1].bHp + 20 + 3 * lv);
    CHECK_RANGE(b.wild.level, max(1, (int)lv - 2), min(999, (int)lv + 2));
    CHECK_EQ(b.wild.atk, DEX_TBL[b.wild.dex].bAtk + b.wild.level);
  }
  CHECK_EQ(Battle::hpFor(255, 65535), 3272);
}

TEST(battle, all_151_species_reachable_with_weighted_roll) {
  bool seen[152] = {};
  Pet p = fighter();
  int total = 0;
  for (int dex = 1; dex <= 151; ++dex)
    total += DEX_TBL[dex].rarity == R_COMUN ? 12 : (DEX_TBL[dex].rarity == R_LEGENDARIO ? 1 : 3);
  for (int roll = 0; roll < total; ++roll) {
    mockForceRandom(roll);
    Battle b; CHECK(b.begin(p)); CHECK_RANGE(b.wild.dex, 1, 151);
    seen[b.wild.dex] = true;
  }
  for (int dex = 1; dex <= 151; ++dex) CHECK(seen[dex]);
  mockClearForcedRandom();
}

TEST(battle, speed_orders_attacks_and_knockout_cancels_retaliation) {
  Pet p = fighter(); Battle b; b.begin(p); mockForceRandom(0);
  b.player.spe = 999; b.wild.spe = 1; b.wild.hp = 1;
  uint16_t hp = b.player.hp;
  CHECK(b.choose(BATTLE_ATTACK, 0)); CHECK_EQ(b.phase, BATTLE_WON);
  CHECK_EQ(b.wild.hp, 0); finishTurn(b, 0); CHECK_EQ(b.player.hp, hp);
  CHECK(!b.choose(BATTLE_ATTACK, 2000));
  b.end(); b.begin(p); b.wild.spe = 999; b.player.spe = 1; b.player.hp = 1;
  hp = b.wild.hp; CHECK(b.choose(BATTLE_ATTACK, 0));
  CHECK_EQ(b.phase, BATTLE_LOST); finishTurn(b, 0); CHECK_EQ(b.wild.hp, hp);
  mockClearForcedRandom();
}

TEST(battle, heavy_has_more_damage_but_lower_accuracy) {
  Pet p = fighter(); Battle normal; normal.begin(p);
  normal.player.spe = 999; normal.player.atk = 50; normal.wild.def = 100;
  Battle heavy = normal; mockForceRandom(0);
  normal.choose(BATTLE_ATTACK, 0); heavy.choose(BATTLE_HEAVY, 0);
  CHECK(heavy.damage > normal.damage);
  Battle miss; miss.begin(p); miss.player.spe = 999;
  mockForceRandom(70); miss.choose(BATTLE_HEAVY, 0); CHECK_EQ(miss.event, BATTLE_MISS);
  Battle hit; hit.begin(p); hit.player.spe = 999;
  hit.choose(BATTLE_ATTACK, 0); CHECK_EQ(hit.event, BATTLE_HIT);
  mockClearForcedRandom();
}

TEST(battle, dodge_priority_counter_and_failure) {
  Pet p = fighter(); Battle b; b.begin(p); b.player.spe = 1; b.wild.spe = 999;
  uint16_t hp = b.player.hp; mockForceRandom(0);
  b.choose(BATTLE_DODGE, 0); CHECK_EQ(b.player.hp, hp); CHECK(b.counterReady);
  CHECK_EQ(b.event, BATTLE_EVADED); finishTurn(b, 0);
  b.choose(BATTLE_ATTACK, 2000); finishTurn(b, 2000); CHECK(!b.counterReady);
  mockForceRandom(99); hp = b.player.hp;
  b.choose(BATTLE_DODGE, 4000); CHECK(b.player.hp < hp); CHECK(!b.counterReady);
  mockClearForcedRandom();
}

TEST(battle, flee_success_and_failure_costs_one_enemy_attack) {
  Pet p = fighter(); Battle b; b.begin(p); mockForceRandom(0);
  b.choose(BATTLE_FLEE, 0); CHECK_EQ(b.phase, BATTLE_ESCAPED);
  b.end(); b.begin(p); mockForceRandom(99);
  uint16_t hp = b.player.hp, enemyHp = b.wild.hp;
  b.choose(BATTLE_FLEE, 0); CHECK_EQ(b.event, BATTLE_ESCAPE_FAILED);
  CHECK_EQ(b.player.hp, hp); finishTurn(b, 0);
  CHECK(b.player.hp < hp); CHECK_EQ(b.wild.hp, enemyHp); CHECK_EQ(b.phase, BATTLE_READY);
  mockClearForcedRandom();
}

TEST(battle, busy_input_and_millis_wrap) {
  Pet p = fighter(); Battle b; b.begin(p); mockForceRandom(0);
  uint32_t start = 0xffffff00UL;
  CHECK(b.choose(BATTLE_ATTACK, start));
  uint16_t playerHp = b.player.hp, wildHp = b.wild.hp;
  CHECK(!b.choose(BATTLE_HEAVY, start + 1));
  b.update(start + 649); CHECK_EQ(b.player.hp, playerHp); CHECK_EQ(b.wild.hp, wildHp);
  finishTurn(b, start); CHECK_EQ(b.phase, BATTLE_READY);
  CHECK(!b.choose((BattleAction)255, 2000));
  mockClearForcedRandom();
}

TEST(battle, battle_never_writes_save_or_changes_care_stats) {
  mockNvsReset(); Pet p = fighter(999); p.begin();
  p.speciesId = 1; p.ageMinutes = 998UL * MINUTES_PER_LEVEL;
  size_t keys = mockNvsKeyCount("tamapoke");
  uint8_t fullness = p.fullness, joy = p.joy, energy = p.energy, hygiene = p.hygiene;
  for (int seed = 1; seed <= 100; ++seed) {
    randomSeed(seed); Battle b; CHECK(b.begin(p));
    for (uint32_t turn = 0; turn < 1000 && !b.finished(); ++turn) {
      b.choose(BATTLE_ATTACK, turn * 2000); finishTurn(b, turn * 2000);
      CHECK(b.player.hp <= b.player.maxHp); CHECK(b.wild.hp <= b.wild.maxHp);
    }
    CHECK(b.finished());
  }
  CHECK_EQ(mockNvsKeyCount("tamapoke"), keys);
  CHECK_EQ(p.fullness, fullness); CHECK_EQ(p.joy, joy);
  CHECK_EQ(p.energy, energy); CHECK_EQ(p.hygiene, hygiene);
  CHECK_EQ(p.level(), 999);
}

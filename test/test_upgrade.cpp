#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#include "../collection.h"
#include "../battle.h"
#include "../types.h"
#include "../adventure_i18n.h"
#include "../i18n.h"
#include <string.h>

TEST(upgrade, seen_species_is_not_detailed_until_caught) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.markSeen(25);
  CHECK(p.isSeen(25));
  CHECK(!p.isRegistered(25));
  CHECK_EQ(p.seenCount(), (uint16_t)1);
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  Pet restored;
  restored.begin();
  CHECK(restored.isSeen(25));
  CHECK(!restored.isRegistered(25));
  restored.registerCaught(25, true);
  CHECK(restored.isRegistered(25));
  CHECK(restored.isShinyRegistered(25));
}

TEST(upgrade, capture_store_and_switch_preserves_one_active_pet) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(1);
  p.eggTap(); p.eggTap(); p.eggTap();
  p.ageMinutes = 300;
  p.battleXpMinutes = 30;
  p.trAtk = 7;
  p.rename("SEED");
  Collection box;
  box.begin();
  CHECK(box.catchWild(p, 4, 12, false));
  CHECK(box.has(4));
  CHECK(p.isRegistered(4));
  CHECK(box.activate(p, 4));
  CHECK_EQ(p.speciesId, (int16_t)4);
  CHECK(box.has(1));
  CHECK(!box.has(4));
  CHECK(box.deposit(p));
  CHECK(p.isEgg());
  CHECK(box.has(4));
  Collection restored;
  restored.begin();
  CHECK(restored.has(1));
  CHECK(restored.has(4));
  CHECK(restored.activate(p, 1));
  CHECK_EQ(p.speciesId, (int16_t)1);
  CHECK_EQ(p.ageMinutes, (uint32_t)300);
  CHECK_EQ(p.battleXpMinutes, (uint32_t)30);
  CHECK_EQ(p.trAtk, (uint8_t)7);
  CHECK_STREQ(p.nick, "SEED");
}

TEST(upgrade, battle_type_and_wait_turn) {
  CHECK_EQ(battleTypeEffectPct(TYPE_FIRE, TYPE_GRASS, TYPE_NONE), (uint16_t)200);
  CHECK_EQ(battleTypeEffectPct(TYPE_ELECTRIC, TYPE_GROUND, TYPE_NONE), (uint16_t)0);
  CHECK(battleTypeEffectPct(TYPE_FIRE, TYPE_WATER, TYPE_NONE) < 100);
  CHECK_EQ(dexType1(4), (uint8_t)TYPE_FIRE);
  BattleStats player = wildBattleStats(4, 10);
  BattleStats enemy = wildBattleStats(1, 10);
  BattleRuntime battle = beginBattleRuntime(player, enemy);
  uint16_t playerBefore = battle.playerHp;
  BattleTurnResult turn = stepBattle(battle, BATTLE_WAIT, 99);
  CHECK_EQ(battle.round, (uint8_t)1);
  CHECK(battle.playerHp < playerBefore);
  CHECK_EQ(turn.playerDamage, (uint16_t)0);
}

TEST(upgrade, typed_move_uses_pp_and_applies_status) {
  BattleStats player = wildBattleStats(4, 10);
  BattleStats enemy = wildBattleStats(1, 10);
  BattleRuntime battle = beginBattleRuntime(player, enemy);
  BattleMove ember = battleMoveFor(4, 1);
  CHECK_EQ(ember.type, (uint8_t)TYPE_FIRE);
  CHECK_EQ(battle.pp[1], (uint8_t)20);
  BattleTurnResult turn = stepBattleMove(battle, ember, 1, 0);
  CHECK_EQ(battle.pp[1], (uint8_t)19);
  CHECK(turn.playerDamage > 0);
  CHECK(turn.statusInflicted);
  CHECK_EQ(battle.enemyStatus, STATUS_BURN);
  CHECK_EQ(battleTypeEffectPct(TYPE_ELECTRIC, TYPE_GROUND, TYPE_NONE), (uint16_t)0);
  uint16_t enemyBefore = battle.enemyHp;
  stepBattle(battle, BATTLE_WAIT, 99);
  CHECK(battle.enemyHp < enemyBefore);
}


TEST(upgrade, sleep_prevents_two_turns_then_clears) {
  BattleRuntime battle = beginBattleRuntime(wildBattleStats(4, 10),
                                            wildBattleStats(1, 10));
  battle.playerStatus = STATUS_SLEEP;
  battle.playerStatusTurns = 2;
  BattleTurnResult first = stepBattle(battle, BATTLE_ATTACK, 99);
  CHECK(first.playerParalyzed);
  CHECK_EQ(first.playerDamage, (uint16_t)0);
  CHECK_EQ(battle.playerStatusTurns, (uint8_t)1);
  BattleTurnResult second = stepBattle(battle, BATTLE_ATTACK, 99);
  CHECK(second.playerParalyzed);
  CHECK_EQ(battle.playerStatus, STATUS_NONE);
  BattleTurnResult third = stepBattle(battle, BATTLE_ATTACK, 99);
  CHECK(third.playerDamage > 0);
}

TEST(upgrade, korean_adventure_labels_follow_language) {
  gLang = LANG_KO;
  CHECK_STREQ(adventureText("FIGHT"), "기술");
  CHECK_STREQ(adventureText("WAVE %u  #%03d"), "%u웨이브  #%03d");
  gLang = LANG_EN;
  CHECK_STREQ(adventureText("FIGHT"), "FIGHT");
}

TEST(upgrade, battle_xp_levels_only_active_without_aging) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(1);
  p.eggTap(); p.eggTap(); p.eggTap();
  Collection box;
  box.begin();
  CHECK(box.catchWild(p, 4, 5, false));
  CHECK(box.activate(p, 4));
  uint32_t ageBefore = p.ageMinutes;
  for (int i = 0; i < 4; i++) p.grantBattleXp();
  CHECK_EQ(p.ageMinutes, ageBefore);
  CHECK_EQ(p.level(), (uint16_t)2);
  CHECK_EQ(box.get(1)->battleXpMinutes, (uint32_t)0);
  p.newEgg();
  CHECK_EQ(p.battleXpMinutes, (uint32_t)0);
}

TEST(upgrade, fire_cannot_burn_fire_type) {
  BattleRuntime battle = beginBattleRuntime(wildBattleStats(4, 10),
                                            wildBattleStats(4, 10));
  BattleTurnResult turn = stepBattleMove(battle, battleMoveFor(4, 1), 1, 0);
  CHECK(turn.playerDamage > 0);
  CHECK(!turn.statusInflicted);
  CHECK_EQ(battle.enemyStatus, STATUS_NONE);
}

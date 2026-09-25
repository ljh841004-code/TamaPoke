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
  CHECK_STREQ(battleTypeName(dexType1(4)), "FIRE");
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
  uint16_t levelBefore = p.level();
  for (int i = 0; i < 4; i++) p.grantBattleXp();
  CHECK_EQ(p.ageMinutes, ageBefore);
  CHECK_EQ(p.level(), (uint16_t)(levelBefore + 1));
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

TEST(upgrade, korean_name_survives_save_and_box) {
  mockNvsReset(); Pet p; p.begin(); p.chooseStarter(1);
  p.eggTap(); p.eggTap(); p.eggTap();
  p.rename("우리집이상해씨별이");
  Pet restored; restored.begin();
  CHECK_EQ(strcmp(restored.nick, "우리집이상해씨별이"), 0);
  Collection box; box.begin(); CHECK(box.deposit(p));
  Collection again; again.begin(); CHECK(again.activate(p, 1));
  CHECK_EQ(strcmp(p.nick, "우리집이상해씨별이"), 0);
  p.rename("가나다라마바사아자차카타파하");
  CHECK_EQ(strlen(p.nick), (size_t)33);
  CHECK_EQ(strcmp(p.nick, "가나다라마바사아자차카"), 0);
}
TEST(upgrade, defense_and_weight_training_are_bounded_and_independent) {
  mockNvsReset(); Pet p; p.begin(); p.chooseStarter(1);
  p.eggTap(); p.eggTap(); p.eggTap(); p.energy=100;
  p.trDef=98; p.trAtk=7; p.trSpe=8;
  CHECK_EQ(p.trainSession(1, 30), (uint8_t)2);
  CHECK_EQ(p.trDef, (uint8_t)100); CHECK_EQ(p.trAtk, (uint8_t)7);
  CHECK_EQ(p.trSpe, (uint8_t)8);
  p.weight=99; CHECK_EQ(p.trainSession(3, 12), (uint8_t)1);
  CHECK_EQ(p.weight, (uint8_t)100);
  p.sleeping=true; CHECK_EQ(p.trainSession(1, 20), (uint8_t)0);
}

TEST(upgrade, four_training_records_survive_restart_and_migrate_old_scores) {
  mockNvsReset(); mockSetMillis(0);
  Pet p; p.begin(); p.chooseStarter(1);
  p.eggTap(); p.eggTap(); p.eggTap(); p.energy = 100;
  p.trainSession(0, 24);
  p.trainSession(1, 13);
  p.playResult(9);
  p.trainSession(3, 7);
  CHECK_EQ(p.trainingBest(0), (uint16_t)24);
  CHECK_EQ(p.trainingBest(1), (uint16_t)13);
  CHECK_EQ(p.trainingBest(2), (uint16_t)9);
  CHECK_EQ(p.trainingBest(3), (uint16_t)7);
  p.trainSession(1, 5);
  CHECK_EQ(p.trainingBest(1), (uint16_t)13);
  Pet restored; restored.begin();
  for (uint8_t i = 0; i < 4; ++i) CHECK_EQ(restored.trainingBest(i), p.trainingBest(i));
  Preferences prefs; prefs.begin("tamapoke", false);
  prefs.remove("th_atk"); prefs.remove("th_spd"); prefs.end();
  Pet migrated; migrated.begin();
  CHECK_EQ(migrated.trainingBest(0), migrated.strHi);
  CHECK_EQ(migrated.trainingBest(2), migrated.gameHi);
}

TEST(upgrade, three_minute_checkpoint_and_level_up_are_durable) {
  mockNvsReset(); mockSetMillis(0);
  Pet p; p.begin(); p.chooseStarter(1);
  p.eggTap(); p.eggTap(); p.eggTap();
  for (int i = 0; i < 3; ++i) { mockAdvanceMillis(PET_TICK_MS); p.update(millis()); }
  CHECK(p.savePending());
  p.flushSave();
  Pet afterCheckpoint; afterCheckpoint.begin();
  CHECK_EQ(afterCheckpoint.ageMinutes, p.ageMinutes);
  for (int i = 0; i < 57; ++i) { mockAdvanceMillis(PET_TICK_MS); p.update(millis()); }
  Pet afterLevel; afterLevel.begin();
  CHECK_EQ(afterLevel.ageMinutes, p.ageMinutes);
}
TEST(upgrade, collection_v2_migrates_without_losing_nick_or_stats) {
  struct Old {
    uint32_t ageMinutes, battleXpMinutes;
    uint16_t speciesId, medals;
    uint8_t fullness,joy,energy,hygiene,poops,weight,bond,careMistakes;
    uint8_t geneAtk,geneDef,geneSpe,trAtk,trDef,trSpe,shiny,berryKnown;
    char nick[12];
  };
  mockNvsReset(); static Old old[151] = {};
  old[24].speciesId=25; old[24].battleXpMinutes=180; old[24].trDef=44;
  strcpy(old[24].nick,"PIKA");
  Preferences prefs; prefs.begin("tpbox",false);
  prefs.putUChar("version",2);prefs.putBytes("mons",old,sizeof(old));prefs.end();
  Collection box;box.begin();CHECK(box.has(25));
  CHECK_EQ(box.get(25)->trDef,(uint8_t)44);
  CHECK_EQ(box.get(25)->battleXpMinutes,(uint32_t)180);
  CHECK_EQ(strcmp(box.get(25)->nick,"PIKA"),0);
  Collection again;again.begin();CHECK(again.has(25));
}

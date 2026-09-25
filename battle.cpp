#include "battle.h"
#include "dex.h"
#include "types.h"

namespace {
constexpr uint8_t MAX_BATTLE_ROUNDS = 50;
constexpr uint8_t MAX_TURN_ROUNDS = 20;

uint16_t hpFor(const BattleStats &stats) {
  if (stats.hp > 0) return stats.hp;
  return (uint16_t)(18 + stats.level * 3 + stats.def / 2);
}

uint8_t clampedLuck(uint8_t luck) {
  return luck > 99 ? 99 : luck;
}

int8_t typeRelation(uint8_t attackType, uint8_t defendType) {
  if (attackType == TYPE_NONE || defendType == TYPE_NONE) return 0;
  switch (attackType) {
    case TYPE_NORMAL:
      if (defendType == TYPE_ROCK || defendType == TYPE_STEEL) return -1;
      if (defendType == TYPE_GHOST) return -2;
      return 0;
    case TYPE_FIRE:
      if (defendType == TYPE_BUG || defendType == TYPE_STEEL || defendType == TYPE_GRASS || defendType == TYPE_ICE) return 1;
      if (defendType == TYPE_ROCK || defendType == TYPE_FIRE || defendType == TYPE_WATER || defendType == TYPE_DRAGON) return -1;
      return 0;
    case TYPE_WATER:
      if (defendType == TYPE_GROUND || defendType == TYPE_ROCK || defendType == TYPE_FIRE) return 1;
      if (defendType == TYPE_WATER || defendType == TYPE_GRASS || defendType == TYPE_DRAGON) return -1;
      return 0;
    case TYPE_ELECTRIC:
      if (defendType == TYPE_FLYING || defendType == TYPE_WATER) return 1;
      if (defendType == TYPE_GRASS || defendType == TYPE_ELECTRIC || defendType == TYPE_DRAGON) return -1;
      if (defendType == TYPE_GROUND) return -2;
      return 0;
    case TYPE_GRASS:
      if (defendType == TYPE_GROUND || defendType == TYPE_ROCK || defendType == TYPE_WATER) return 1;
      if (defendType == TYPE_FLYING || defendType == TYPE_POISON || defendType == TYPE_BUG ||
          defendType == TYPE_STEEL || defendType == TYPE_FIRE || defendType == TYPE_GRASS ||
          defendType == TYPE_DRAGON) return -1;
      return 0;
    case TYPE_ICE:
      if (defendType == TYPE_FLYING || defendType == TYPE_GROUND || defendType == TYPE_GRASS || defendType == TYPE_DRAGON) return 1;
      if (defendType == TYPE_STEEL || defendType == TYPE_FIRE || defendType == TYPE_WATER || defendType == TYPE_ICE) return -1;
      return 0;
    case TYPE_FIGHTING:
      if (defendType == TYPE_NORMAL || defendType == TYPE_ROCK || defendType == TYPE_STEEL ||
          defendType == TYPE_ICE || defendType == TYPE_DARK) return 1;
      if (defendType == TYPE_FLYING || defendType == TYPE_POISON || defendType == TYPE_BUG ||
          defendType == TYPE_PSYCHIC || defendType == TYPE_FAIRY) return -1;
      if (defendType == TYPE_GHOST) return -2;
      return 0;
    case TYPE_POISON:
      if (defendType == TYPE_GRASS || defendType == TYPE_FAIRY) return 1;
      if (defendType == TYPE_POISON || defendType == TYPE_GROUND || defendType == TYPE_ROCK || defendType == TYPE_GHOST) return -1;
      if (defendType == TYPE_STEEL) return -2;
      return 0;
    case TYPE_GROUND:
      if (defendType == TYPE_POISON || defendType == TYPE_ROCK || defendType == TYPE_STEEL ||
          defendType == TYPE_FIRE || defendType == TYPE_ELECTRIC) return 1;
      if (defendType == TYPE_BUG || defendType == TYPE_GRASS) return -1;
      if (defendType == TYPE_FLYING) return -2;
      return 0;
    case TYPE_FLYING:
      if (defendType == TYPE_FIGHTING || defendType == TYPE_BUG || defendType == TYPE_GRASS) return 1;
      if (defendType == TYPE_ROCK || defendType == TYPE_STEEL || defendType == TYPE_ELECTRIC) return -1;
      return 0;
    case TYPE_PSYCHIC:
      if (defendType == TYPE_FIGHTING || defendType == TYPE_POISON) return 1;
      if (defendType == TYPE_STEEL || defendType == TYPE_PSYCHIC) return -1;
      if (defendType == TYPE_DARK) return -2;
      return 0;
    case TYPE_BUG:
      if (defendType == TYPE_GRASS || defendType == TYPE_PSYCHIC || defendType == TYPE_DARK) return 1;
      if (defendType == TYPE_FIGHTING || defendType == TYPE_FLYING || defendType == TYPE_POISON ||
          defendType == TYPE_GHOST || defendType == TYPE_STEEL || defendType == TYPE_FIRE ||
          defendType == TYPE_FAIRY) return -1;
      return 0;
    case TYPE_ROCK:
      if (defendType == TYPE_FLYING || defendType == TYPE_BUG || defendType == TYPE_FIRE || defendType == TYPE_ICE) return 1;
      if (defendType == TYPE_FIGHTING || defendType == TYPE_GROUND || defendType == TYPE_STEEL) return -1;
      return 0;
    case TYPE_GHOST:
      if (defendType == TYPE_GHOST || defendType == TYPE_PSYCHIC) return 1;
      if (defendType == TYPE_DARK) return -1;
      if (defendType == TYPE_NORMAL) return -2;
      return 0;
    case TYPE_DRAGON:
      if (defendType == TYPE_DRAGON) return 1;
      if (defendType == TYPE_STEEL) return -1;
      if (defendType == TYPE_FAIRY) return -2;
      return 0;
    case TYPE_DARK:
      if (defendType == TYPE_GHOST || defendType == TYPE_PSYCHIC) return 1;
      if (defendType == TYPE_FIGHTING || defendType == TYPE_DARK || defendType == TYPE_FAIRY) return -1;
      return 0;
    case TYPE_STEEL:
      if (defendType == TYPE_ROCK || defendType == TYPE_ICE || defendType == TYPE_FAIRY) return 1;
      if (defendType == TYPE_STEEL || defendType == TYPE_FIRE || defendType == TYPE_WATER || defendType == TYPE_ELECTRIC) return -1;
      return 0;
    case TYPE_FAIRY:
      if (defendType == TYPE_FIGHTING || defendType == TYPE_DRAGON || defendType == TYPE_DARK) return 1;
      if (defendType == TYPE_POISON || defendType == TYPE_STEEL || defendType == TYPE_FIRE) return -1;
      return 0;
  }
  return 0;
}

uint16_t effectPctFor(uint8_t attackType, uint8_t defendType1, uint8_t defendType2) {
  uint32_t pct = 100;
  const uint8_t defendTypes[2] = { defendType1, defendType2 };
  for (uint8_t i = 0; i < 2; i++) {
    int8_t relation = typeRelation(attackType, defendTypes[i]);
    if (relation > 0) pct *= 2;
    else if (relation == -1) pct /= 2;
    else if (relation == -2) return 0;
  }
  return (uint16_t)pct;
}
uint16_t applyTypeMultiplier(uint16_t damage, const BattleStats &attacker, const BattleStats &defender) {
  uint16_t pct = effectPctFor(attacker.type1, defender.type1, defender.type2);
  if (pct == 0) return 0;
  uint32_t scaled = (uint32_t)damage * pct / 100;
  if (scaled < 1) scaled = 1;
  return scaled > 65535 ? 65535 : (uint16_t)scaled;
}

uint16_t damageFor(const BattleStats &attacker, const BattleStats &defender, uint8_t luck) {
  uint16_t level = attacker.level > 0 ? attacker.level : 1;
  uint32_t pressure = (uint32_t)attacker.atk * (10 + level / 4);
  uint32_t guard = (uint32_t)defender.def + 35;
  uint32_t base = 1 + pressure / guard;
  uint32_t variation = 90 + (clampedLuck(luck) % 21);  // 90..110 %
  uint32_t damage = base * variation / 100;
  if (damage == 0) damage = 1;
  if (damage > 65535) damage = 65535;
  return applyTypeMultiplier((uint16_t)damage, attacker, defender);
}

void applyHit(uint16_t &hpLeft, uint16_t damage, uint16_t &damageTotal) {
  uint16_t dealt = damage > hpLeft ? hpLeft : damage;
  hpLeft -= dealt;
  damageTotal += dealt;
}

uint16_t turnHpFor(const BattleStats &stats) {
  uint16_t level = stats.level > 0 ? stats.level : 1;
  uint32_t scaled = 30 + (uint32_t)level * 5 + stats.def;
  uint16_t legacy = hpFor(stats);
  if (scaled < legacy) scaled = legacy;
  return scaled > 65535 ? 65535 : (uint16_t)scaled;
}

uint16_t cappedTurnDamage(const BattleStats &attacker,
                          const BattleStats &defender,
                          uint16_t defenderMaxHp,
                          uint8_t luck,
                          bool counter,
                          uint8_t powerPct) {
  uint16_t damage = damageFor(attacker, defender, luck);
  damage = (uint16_t)((uint32_t)damage * powerPct / 100);
  if (damage == 0) return 0;
  if (counter) damage = (uint16_t)((uint32_t)damage * 3 / 2);
  uint16_t cap = (uint16_t)((uint32_t)defenderMaxHp * 35 / 100);
  if (cap < 1) cap = 1;
  if (damage > cap) damage = cap;
  return damage;
}

bool finished(const BattleRuntime &battle) {
  return battle.playerHp == 0 || battle.enemyHp == 0 || battle.round >= MAX_TURN_ROUNDS;
}

bool winner(const BattleRuntime &battle) {
  if (battle.enemyHp == 0) return true;
  if (battle.playerHp == 0) return false;
  return battle.playerHp >= battle.enemyHp;
}

bool isAttackAction(BattleAction action) {
  return action == BATTLE_ATTACK || action == BATTLE_ATTACK_QUICK || action == BATTLE_ATTACK_HEAVY;
}

uint8_t attackPowerPct(BattleAction action) {
  if (action == BATTLE_ATTACK_QUICK) return 85;
  if (action == BATTLE_ATTACK_HEAVY) return 125;
  return 100;
}

uint8_t enemyDodgeChance(const BattleRuntime &battle, BattleAction action) {
  int chance = battle.enemy.spe > battle.player.spe ? 18 : 10;
  if (action == BATTLE_ATTACK_QUICK) chance -= 9;
  else if (action == BATTLE_ATTACK_HEAVY) chance += 22;
  if (chance < 0) chance = 0;
  if (chance > 45) chance = 45;
  return (uint8_t)chance;
}
}  // namespace

uint16_t battleTypeEffectPct(uint8_t attackType, uint8_t defendType1, uint8_t defendType2) {
  return effectPctFor(attackType, defendType1, defendType2);
}

bool canStartWildBattle(bool isEgg, bool sleeping, uint8_t ceremony) {
  return !isEgg && !sleeping && ceremony == 0;
}

uint8_t wildLevelFor(uint8_t petLevel, uint8_t luckRoll) {
  int base = (int)(petLevel ? petLevel : 1);
  int delta;
  if (luckRoll < 55) delta = (int)(luckRoll % 3) - 1;       // -1..+1, most common
  else if (luckRoll < 85) delta = -2 - (int)(luckRoll % 3); // -2..-4, fair catch-up fights
  else delta = 2 + (int)(luckRoll % 2);                     // +2..+3, occasional danger
  int level = base + delta;
  if (level < 1) level = 1;
  return level > 100 ? 100 : (uint8_t)level;
}

int16_t pickWildSpecies(uint8_t roll) {
  int16_t pool[DEX_COUNT];
  int count = 0;
  uint8_t targetRarity = (roll % 100) < 25 ? R_RARO : R_COMUN;
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++) {
    if (DEX_TBL[dex].rarity == targetRarity) pool[count++] = dex;
  }
  if (count == 0 && targetRarity == R_RARO) {
    for (int16_t dex = 1; dex <= DEX_COUNT; dex++)
      if (DEX_TBL[dex].rarity == R_COMUN) pool[count++] = dex;
  }
  return count > 0 ? pool[roll % count] : 1;
}

BattleStats wildBattleStats(int16_t dex, uint8_t level) {
  if (dex < 1 || dex > DEX_COUNT) dex = 1;
  const DexEntry &entry = DEX_TBL[dex];
  uint8_t lvl = level ? level : 1;
  BattleStats stats = {};
  stats.hp = 0;
  stats.atk = entry.bAtk + lvl;
  stats.def = entry.bDef + lvl;
  stats.spe = entry.bSpe + lvl;
  stats.level = lvl;
  stats.type1 = dexType1(dex);
  stats.type2 = dexType2(dex);
  return stats;
}

BattleRuntime beginBattleRuntime(const BattleStats &player, const BattleStats &enemy) {
  BattleRuntime battle = {};
  battle.player = player;
  battle.enemy = enemy;
  battle.playerMaxHp = turnHpFor(player);
  battle.enemyMaxHp = turnHpFor(enemy);
  battle.playerHp = battle.playerMaxHp;
  battle.enemyHp = battle.enemyMaxHp;
  battle.restUsesLeft = 2;
  battle.pp[0] = 30; battle.pp[1] = 20; battle.pp[2] = 20; battle.pp[3] = 10;
  battle.counterReady = false;
  return battle;
}

static BattleStatus statusForType(uint8_t type);
static bool canReceiveStatus(BattleStatus status, const BattleStats &target);

BattleTurnResult stepBattle(BattleRuntime &battle, BattleAction action, uint8_t luckRoll) {
  BattleTurnResult turn = {};
  if (finished(battle)) {
    turn.battleEnded = true;
    turn.playerWon = winner(battle);
    return turn;
  }

  uint8_t luck = clampedLuck(luckRoll);
  turn.playerTypePct = battleTypeEffectPct(battle.player.type1, battle.enemy.type1, battle.enemy.type2);
  turn.enemyTypePct = battleTypeEffectPct(battle.enemy.type1, battle.player.type1, battle.player.type2);
  if (action == BATTLE_REST && battle.restUsesLeft == 0) {
    turn.playerRested = true;
    turn.restFailed = true;
    return turn;
  }

  battle.round++;
  if ((battle.playerStatus == STATUS_SLEEP || battle.playerStatus == STATUS_FREEZE) &&
      battle.playerStatusTurns) {
    action = BATTLE_WAIT;
    turn.playerParalyzed = true;
    if (--battle.playerStatusTurns == 0) battle.playerStatus = STATUS_NONE;
  } else if (battle.playerStatus == STATUS_PARALYSIS && luck < 25 && isAttackAction(action)) {
    action = BATTLE_WAIT;
    turn.playerParalyzed = true;
  }

  if (action == BATTLE_REST) {
    turn.playerRested = true;
    battle.restUsesLeft--;
    uint16_t heal = (uint16_t)((uint32_t)battle.playerMaxHp * 28 / 100);
    if (heal < 6) heal = 6;
    uint16_t missing = battle.playerMaxHp - battle.playerHp;
    turn.playerHeal = heal > missing ? missing : heal;
    battle.playerHp += turn.playerHeal;
  } else if (isAttackAction(action)) {
    uint8_t dodgeChance = enemyDodgeChance(battle, action);
    turn.enemyDodged = ((uint16_t)luck + battle.enemy.spe / 2) % 100 < dodgeChance;
    if (!turn.enemyDodged) {
      turn.counterUsed = battle.counterReady;
      turn.playerDamage = cappedTurnDamage(battle.player, battle.enemy, battle.enemyMaxHp,
                                           luck, battle.counterReady, attackPowerPct(action));
      battle.counterReady = false;
      if (battle.playerStatus == STATUS_BURN && turn.playerDamage) {
        turn.playerDamage = (turn.playerDamage * 3) / 4;
        if (!turn.playerDamage) turn.playerDamage = 1;
      }
      applyHit(battle.enemyHp, turn.playerDamage, battle.playerDamageTotal);
    }
  }

  bool enemyActs = battle.enemyHp > 0 && (!turn.enemyDodged || action == BATTLE_ATTACK_HEAVY);
  if ((battle.enemyStatus == STATUS_SLEEP || battle.enemyStatus == STATUS_FREEZE) && battle.enemyStatusTurns) {
    enemyActs = false;
    if (--battle.enemyStatusTurns == 0) battle.enemyStatus = STATUS_NONE;
  }
  if (battle.enemyStatus == STATUS_PARALYSIS && luck >= 75) enemyActs = false;
  if (enemyActs) {
    uint16_t enemyHit = cappedTurnDamage(battle.enemy, battle.player, battle.playerMaxHp, 99 - luck, false, 100);
    if (action == BATTLE_DODGE) {
      if (luck < 85) {
        turn.playerDodged = true;
        battle.counterReady = true;
        turn.counterReady = true;
        enemyHit = 0;
      } else {
        enemyHit = enemyHit > 2 ? enemyHit / 3 : 1;
      }
    } else if (action == BATTLE_ATTACK_QUICK) {
      turn.quickGuard = true;
      enemyHit = (uint16_t)((uint32_t)enemyHit * 85 / 100);
      if (enemyHit == 0) enemyHit = 1;
    } else if (action == BATTLE_ATTACK_HEAVY) {
      turn.heavyRisk = true;
      enemyHit = (uint16_t)((uint32_t)enemyHit * 120 / 100);
      if (enemyHit == 0) enemyHit = 1;
    } else if (action == BATTLE_REST) {
      turn.playerGuarded = true;
      enemyHit = (uint16_t)((uint32_t)enemyHit * 70 / 100);
      if (enemyHit == 0) enemyHit = 1;
    }
    if (battle.enemyStatus == STATUS_BURN) enemyHit = (enemyHit * 3) / 4;
    if (enemyHit > 0) {
      turn.enemyDamage = enemyHit;
      applyHit(battle.playerHp, turn.enemyDamage, battle.enemyDamageTotal);
      if (battle.playerHp && battle.playerStatus == STATUS_NONE &&
          ((uint16_t)luck * 7 + battle.round * 11) % 100 < 12) {
        BattleStatus inflicted = statusForType(battle.enemy.type1);
        if (canReceiveStatus(inflicted, battle.player)) {
          battle.playerStatus = inflicted;
          if (inflicted == STATUS_SLEEP || inflicted == STATUS_FREEZE)
            battle.playerStatusTurns = 2;
        }
      }
    }
  }

  if (battle.enemyHp && (battle.enemyStatus == STATUS_POISON || battle.enemyStatus == STATUS_BURN)) {
    uint16_t dot = battle.enemyMaxHp / 16; if (!dot) dot = 1;
    applyHit(battle.enemyHp, dot, battle.playerDamageTotal);
    turn.playerDamage += dot;
  }
  if (battle.playerHp && (battle.playerStatus == STATUS_POISON || battle.playerStatus == STATUS_BURN)) {
    uint16_t dot = battle.playerMaxHp / 16; if (!dot) dot = 1;
    applyHit(battle.playerHp, dot, battle.enemyDamageTotal);
    turn.enemyDamage += dot;
  }
  turn.battleEnded = finished(battle);
  turn.playerWon = turn.battleEnded ? winner(battle) : false;
  return turn;
}

BattleResult resolveBattle(const BattleStats &player,
                           const BattleStats &enemy,
                           const BattleOptions &options) {
  BattleResult result = {};
  result.playerHpLeft = hpFor(player);
  result.enemyHpLeft = hpFor(enemy);

  const uint8_t luck = clampedLuck(options.luckRoll);
  const bool playerFirst = player.spe == enemy.spe ? luck >= 50 : player.spe > enemy.spe;
  const uint16_t playerHit = damageFor(player, enemy, luck);
  const uint16_t enemyHit = damageFor(enemy, player, 99 - luck);

  for (uint8_t round = 0; round < MAX_BATTLE_ROUNDS; round++) {
    result.rounds = round + 1;
    if (playerFirst) {
      applyHit(result.enemyHpLeft, playerHit, result.playerDamage);
      if (result.enemyHpLeft == 0) break;
      applyHit(result.playerHpLeft, enemyHit, result.enemyDamage);
      if (result.playerHpLeft == 0) break;
    } else {
      applyHit(result.playerHpLeft, enemyHit, result.enemyDamage);
      if (result.playerHpLeft == 0) break;
      applyHit(result.enemyHpLeft, playerHit, result.playerDamage);
      if (result.enemyHpLeft == 0) break;
    }
  }

  if (result.enemyHpLeft == 0) {
    result.playerWon = true;
  } else if (result.playerHpLeft == 0) {
    result.playerWon = false;
  } else if (result.enemyHpLeft != result.playerHpLeft) {
    result.playerWon = result.enemyHpLeft < result.playerHpLeft;
  } else {
    result.playerWon = playerFirst;
  }

  return result;
}


static const char *typeMoveName(uint8_t type) {
  switch (type) {
    case TYPE_FIRE: return "EMBER";
    case TYPE_WATER: return "WATER GUN";
    case TYPE_ELECTRIC: return "THUNDER";
    case TYPE_GRASS: return "VINE WHIP";
    case TYPE_ICE: return "ICE BEAM";
    case TYPE_FIGHTING: return "KARATE CHOP";
    case TYPE_POISON: return "POISON STING";
    case TYPE_GROUND: return "MUD SLAP";
    case TYPE_FLYING: return "GUST";
    case TYPE_PSYCHIC: return "CONFUSION";
    case TYPE_BUG: return "BUG BITE";
    case TYPE_ROCK: return "ROCK THROW";
    case TYPE_GHOST: return "SHADOW BALL";
    case TYPE_DRAGON: return "DRAGON BREATH";
    default: return "TACKLE";
  }
}

static BattleStatus statusForType(uint8_t type) {
  switch (type) {
    case TYPE_FIRE: return STATUS_BURN;
    case TYPE_POISON: return STATUS_POISON;
    case TYPE_ELECTRIC: return STATUS_PARALYSIS;
    case TYPE_GRASS: return STATUS_SLEEP;
    case TYPE_ICE: return STATUS_FREEZE;
    default: return STATUS_NONE;
  }
}

static bool canReceiveStatus(BattleStatus status, const BattleStats &target) {
  uint8_t a = target.type1, b = target.type2;
  if (status == STATUS_BURN && (a == TYPE_FIRE || b == TYPE_FIRE)) return false;
  if (status == STATUS_POISON &&
      (a == TYPE_POISON || b == TYPE_POISON || a == TYPE_STEEL || b == TYPE_STEEL)) return false;
  if (status == STATUS_PARALYSIS && (a == TYPE_ELECTRIC || b == TYPE_ELECTRIC)) return false;
  if (status == STATUS_FREEZE && (a == TYPE_ICE || b == TYPE_ICE)) return false;
  return status != STATUS_NONE;
}
BattleMove battleMoveFor(int16_t dex, uint8_t slot) {
  uint8_t first = dexType1(dex);
  uint8_t second = dexType2(dex);
  if (slot == 0) return { "TACKLE", TYPE_NORMAL, 90, 100, 30, 0, STATUS_NONE };
  if (slot == 1) {
    BattleStatus s = statusForType(first);
    return { typeMoveName(first), first, 115, 95, 20, s == STATUS_NONE ? (uint8_t)0 : (uint8_t)20, s };
  }
  if (slot == 2) {
    uint8_t type = second == TYPE_NONE ? TYPE_NORMAL : second;
    BattleStatus s = statusForType(type);
    return { second == TYPE_NONE ? "QUICK HIT" : typeMoveName(type), type,
             100, 100, 20, s == STATUS_NONE ? (uint8_t)0 : (uint8_t)15, s };
  }
  return { "POWER STRIKE", first, 155, 75, 10, 0, STATUS_NONE };
}

BattleTurnResult stepBattleMove(BattleRuntime &battle, const BattleMove &move,
                                uint8_t slot, uint8_t luckRoll) {
  BattleTurnResult turn = {};
  if (slot >= 4 || battle.pp[slot] == 0) {
    turn.restFailed = true;
    return turn;
  }
  battle.pp[slot]--;
  if (luckRoll >= move.accuracy) {
    turn = stepBattle(battle, BATTLE_WAIT, luckRoll);
    turn.missed = true;
    return turn;
  }
  uint8_t originalType = battle.player.type1;
  uint16_t originalAtk = battle.player.atk;
  battle.player.type1 = move.type;
  uint32_t scaledAtk = (uint32_t)originalAtk * move.powerPct / 100;
  battle.player.atk = scaledAtk > 65535 ? 65535 : (uint16_t)scaledAtk;
  turn = stepBattle(battle, BATTLE_ATTACK, luckRoll);
  battle.player.type1 = originalType;
  battle.player.atk = originalAtk;
  if (turn.playerDamage && battle.enemyHp && battle.enemyStatus == STATUS_NONE &&
      canReceiveStatus(move.status, battle.enemy) &&
      ((uint16_t)luckRoll * 13 + battle.round * 7) % 100 < move.statusChance) {
    battle.enemyStatus = move.status;
    battle.enemyStatusTurns = (move.status == STATUS_SLEEP || move.status == STATUS_FREEZE) ? 2 : 0;
    turn.statusInflicted = true;
  }
  return turn;
}

const char *battleTypeName(uint8_t type) {
  switch (type) {
    case TYPE_NORMAL: return "NORMAL";
    case TYPE_FIRE: return "FIRE";
    case TYPE_WATER: return "WATER";
    case TYPE_ELECTRIC: return "ELECTRIC";
    case TYPE_GRASS: return "GRASS";
    case TYPE_ICE: return "ICE";
    case TYPE_FIGHTING: return "FIGHTING";
    case TYPE_POISON: return "POISON";
    case TYPE_GROUND: return "GROUND";
    case TYPE_FLYING: return "FLYING";
    case TYPE_PSYCHIC: return "PSYCHIC";
    case TYPE_BUG: return "BUG";
    case TYPE_ROCK: return "ROCK";
    case TYPE_GHOST: return "GHOST";
    case TYPE_DRAGON: return "DRAGON";
    default: return "?";
  }
}

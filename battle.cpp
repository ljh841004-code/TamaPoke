#include "battle.h"
#include "dex.h"

namespace {
uint8_t weightFor(uint8_t rarity) {
  return rarity == R_COMUN ? 12 : (rarity == R_LEGENDARIO ? 1 : 3);
}

int16_t pickWild() {
  // All 151 remain reachable; evolved/rare forms and legends are less common.
  uint16_t total = 0;
  for (int i = 1; i <= DEX_COUNT; ++i) total += weightFor(DEX_TBL[i].rarity);
  long roll = random(total);
  for (int i = 1; i <= DEX_COUNT; ++i) {
    roll -= weightFor(DEX_TBL[i].rarity);
    if (roll < 0) return i;
  }
  return 1;
}

uint16_t hitFor(const BattleFighter &a, const BattleFighter &d, uint16_t power) {
  uint32_t hit = (6UL + a.level / 3) * a.atk / (d.def + 30UL) + 2;
  hit = hit * power / 100;
  hit = hit * (uint32_t)random(90, 111) / 100;
  // Avoid one-hit knockouts, including extreme genes/training or base stats.
  uint32_t cap = max(1UL, (unsigned long)d.maxHp * 35 / 100);
  return (uint16_t)max(1UL, min((unsigned long)hit, (unsigned long)cap));
}
}

bool Battle::canStart(const Pet &pet) {
  return pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT && !pet.sleeping &&
         !pet.evolving() && !pet.eating() && !pet.ceremony;
}

uint16_t Battle::hpFor(uint8_t base, uint16_t level) {
  // Same additive level scale as Pet stats; safe through Pet's level cap (999).
  return base + 20 + 3 * min((uint16_t)999, max((uint16_t)1, level));
}

bool Battle::begin(const Pet &pet) {
  if (active() || !canStart(pet)) return false;
  end();
  player.dex = pet.speciesId;
  player.level = pet.level();
  player.hp = player.maxHp = hpFor(DEX_TBL[player.dex].bHp, player.level);
  player.atk = pet.atkStat();
  player.def = pet.defStat();
  player.spe = pet.speStat();
  wild.dex = pickWild();
  wild.level = (uint16_t)max(1, min(999, (int)player.level + (int)random(-2, 3)));
  const DexEntry &d = DEX_TBL[wild.dex];
  wild.hp = wild.maxHp = hpFor(d.bHp, wild.level);
  wild.atk = d.bAtk + wild.level;
  wild.def = d.bDef + wild.level;
  wild.spe = d.bSpe + wild.level;
  phase = BATTLE_READY;
  return true;
}

bool Battle::choose(BattleAction action, uint32_t now) {
  if (phase != BATTLE_READY || action > BATTLE_FLEE) return false;
  action_ = action;
  phase = BATTLE_TURN;
  eventAt = now;
  event = BATTLE_NONE;
  damage = 0;
  if (action == BATTLE_FLEE) {
    // Escape has priority; a failed attempt costs an enemy attack.
    playerActing = true;
    if (random(100) < (player.spe >= wild.spe ? 85 : 60)) {
      phase = BATTLE_ESCAPED;
      return true;
    }
    event = BATTLE_ESCAPE_FAILED;
    pending_ = 1;
    nextPlayer_ = false;
  } else if (action == BATTLE_DODGE) {
    // Guard is active before the enemy attacks, regardless of speed.
    counterReady = false;
    pending_ = 0;
    act(false, now);
  } else {
    bool first = player.spe == wild.spe ? random(2) == 0 : player.spe > wild.spe;
    pending_ = 1;
    nextPlayer_ = !first;
    act(first, now);
  }
  return true;
}

void Battle::act(bool isPlayer, uint32_t now) {
  playerActing = isPlayer;
  eventAt = now;
  damage = 0;
  event = BATTLE_HIT;
  BattleFighter &a = isPlayer ? player : wild;
  BattleFighter &d = isPlayer ? wild : player;
  if (!isPlayer && action_ == BATTLE_DODGE && random(100) < 75) {
    event = BATTLE_EVADED;
    counterReady = true;
    return;
  }
  uint16_t power = 100;
  if (isPlayer) {
    if (action_ == BATTLE_HEAVY) power = 160;
    if (counterReady) power = power * 3 / 2;
    counterReady = false;  // consumed even on a miss
    if (random(100) >= (action_ == BATTLE_HEAVY ? 65 : 95)) {
      event = BATTLE_MISS;
      return;
    }
  }
  damage = min(d.hp, hitFor(a, d, power));
  d.hp -= damage;
  // Do not resolve the second attack after a knockout.
  if (!d.hp) {
    pending_ = 0;
    phase = isPlayer ? BATTLE_WON : BATTLE_LOST;
  }
}

void Battle::update(uint32_t now) {
  if (phase != BATTLE_TURN || (uint32_t)(now - eventAt) < STEP_MS) return;
  if (pending_) {
    pending_ = 0;
    act(nextPlayer_, now);
  } else {
    phase = BATTLE_READY;
    event = BATTLE_NONE;
    damage = 0;
  }
}

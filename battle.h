#pragma once
#include "pet.h"

enum BattleAction : uint8_t { BATTLE_ATTACK, BATTLE_HEAVY, BATTLE_DODGE, BATTLE_FLEE };
enum BattlePhase : uint8_t { BATTLE_OFF, BATTLE_READY, BATTLE_TURN, BATTLE_WON, BATTLE_LOST, BATTLE_ESCAPED };
enum BattleEvent : uint8_t { BATTLE_NONE, BATTLE_HIT, BATTLE_MISS, BATTLE_EVADED, BATTLE_ESCAPE_FAILED };

struct BattleFighter {
  int16_t dex = 0;
  uint16_t level = 1, hp = 0, maxHp = 0, atk = 0, def = 0, spe = 0;
};

// Transient snapshot only: never writes to Pet or NVS. UI owns sprite memory.
class Battle {
public:
  static constexpr uint32_t STEP_MS = 650;
  BattleFighter player, wild;
  BattlePhase phase = BATTLE_OFF;
  BattleEvent event = BATTLE_NONE;
  bool playerActing = true, counterReady = false;
  uint16_t damage = 0;
  uint32_t eventAt = 0;

  static bool canStart(const Pet &pet);
  static uint16_t hpFor(uint8_t base, uint16_t level);
  bool begin(const Pet &pet);
  bool choose(BattleAction action, uint32_t now);
  void update(uint32_t now);
  void end() { *this = Battle(); }
  bool active() const { return phase != BATTLE_OFF; }
  bool finished() const { return phase >= BATTLE_WON; }

private:
  BattleAction action_ = BATTLE_ATTACK;
  uint8_t pending_ = 0;
  bool nextPlayer_ = false;
  void act(bool isPlayer, uint32_t now);
};

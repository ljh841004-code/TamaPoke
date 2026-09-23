# Wild battle V1

Open the pet card, swipe to the battle stats page and tap **BATTLE**. The existing
strength-training button remains above it. Eggs, sleeping/eating/evolving pets
and pets in a farewell ceremony cannot enter battle.

## Rules

- All 151 species can appear. Each common species has weight 12, each evolved or
  rare species weight 3, and each legendary species weight 1.
- Wild level is the pet's level plus -2 to +2, clamped to 1..999.
- HP is `bHp + 20 + 3 * level`. Player ATK/DEF/SPD come directly from the existing
  `Pet` methods, including genes and training. Wild stats are DEX base + level.
- Attack: 95% accuracy. Heavy: 65% accuracy, 160% damage. Faster fighters act
  first; equal speed uses a coin flip. A knockout cancels the remaining attack.
- Dodge: acts before the enemy regardless of speed, with a 75% chance to avoid
  damage. Success prepares a 150% counter bonus for the next attack (consumed
  even if it misses). Another dodge replaces the previous counter opportunity.
- Flee: 85% success when at least as fast as the enemy, otherwise 60%. Failure
  costs one enemy attack.
- Normal damage is `(6 + level / 3) * ATK / (DEF + 30) + 2`, scaled by heavy /
  counter bonuses and a 90..110% random factor, with a minimum of 1 and a cap of
  35% of the target's maximum HP per hit. There are no type multipliers.
- Input is locked during the two 650 ms attack beats. A result returns to the
  stats card after three seconds, or on tap after the final 650 ms beat.

## Compatibility and resources

Battle is a transient snapshot: no HP, records, rewards, captures or experience
are saved. Existing care ticks continue, and the NVS format and `Pet` code are
unchanged. Wild encounters do not register as raised Pokemon in the Pokedex.
Lifecycle changes, including serial debug commands that change species or start
a ceremony, close the battle safely.

The main pet keeps its existing `pmd`. A separate `wildPmd` is loaded at entry
and unloaded through the single `closeBattle()` path on result, tap, or lifecycle
interruption. Attack/hurt actions fall back to idle if absent. Missing SD assets
or allocation failure show a small placeholder and do not block play. Battle
sprites fit into bounded viewports, including large attack sheets.

The ShadowEnemyx fork was consulted for the idea of a standalone transient
battle runtime. This implementation uses v1.17's actual `DexEntry` fields and
does not import its type-based combat logic.

## Validation

`test/test_battle.cpp` runs with the existing PC suite (`make -C test`). It covers
entry guards, all 151 encounters, level 1/255/256/999, inherited stats, speed,
knockouts, accuracy, dodge/counter, flee, repeated input, clock wrap and complete
fights without care-state changes. Existing CI includes it automatically.

Compile with the repository's existing ESP32 core 3.3.10 and board profile:

```sh
arduino-cli compile --warnings=all \
  --fqbn 'esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB' .
```

Physical-board checks still required:

- Start from the stats card; verify both training and battle touch regions.
- Try all four actions, victory, defeat, fleeing, and returning to the card.
- Repeat battles and inspect free PSRAM/heap for leaks.
- Check missing SD assets, large PMD sheets, and screen sleep/wake during combat.
- Verify card text and touch layout in the configured language.
- Revisit feeding, bathing, sleep, evolution, gallery and both existing games.

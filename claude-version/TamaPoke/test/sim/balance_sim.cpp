// Simulador de equilibrio de combate (PC). No es un test: imprime tasas de victoria.
//   make -C test/sim        -> compila y ejecuta
// El tuyo: una especie al azar en la forma que toca a ese nivel, genes 100, entrenamiento ~0,6 x nivel
// (tope 100), elige siempre lo mejor (whim 0). El salvaje: de la rareza pedida, ajustado con
// wildMatchPower como en el juego y con el descuido del juego (35; el legendario 0).
#include "../../battle.h"
#include "../../dex.h"
#include <stdio.h>
#include <stdlib.h>

static const int FOE_WHIM = 35;  // foeSloppy() de ui_extra.ino

static int16_t formAt(int16_t d, int lv, BRng &rng) {
  for (int g = 0; g < 3 && DEX_TBL[d].evolvesTo; g++) {
    uint8_t need = evoLevel(d);
    if (!need || lv < need) break;
    int16_t o[8];
    int k = dexEvoOptions(d, o);
    d = o[k > 1 ? rng.below(k) : 0];
  }
  return d;
}

static Battler makeMe(int lv, BRng &rng, int16_t *pick) {
  int16_t d;
  do { d = (int16_t)(1 + rng.below(DEX_COUNT)); } while (DEX_TBL[d].rarity == R_LEGENDARIO || dexPrevo(d));
  d = formAt(d, lv, rng);
  if (pick) *pick = d;
  const DexEntry &e = DEX_TBL[d];
  int tr = lv * 6 / 10; if (tr > 100) tr = 100;
  auto st = [&](uint8_t b) { return battleStat(b, 100, (uint16_t)lv, (uint8_t)tr); };  // ko12.10: formula de los juegos
  Battler b = makeBattler(d, (uint16_t)lv, st(e.bAtk), st(e.bDef), st(e.bSpe));
  return b;
}

static int16_t foeOf(uint8_t rar, BRng &rng) {
  int16_t pool[DEX_COUNT]; int n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    uint8_t r = DEX_TBL[d].rarity;
    bool ok = rar == R_COMUN ? (r == R_COMUN || r == R_EVO) : r == rar;
    if (ok) pool[n++] = d;
  }
  return pool[rng.below(n)];
}

// 0 = gana el tuyo
static int fight(Battler a, Battler b, int whimB, uint32_t seed) {
  BRng rng(seed);
  BEvent tmp[BATTLE_MAX_EVENTS];
  for (int t = 0; t < 60 && a.hp && b.hp; t++) {
    BAct x = battleAi(a, b, rng, 0);
    BAct y = battleAi(b, a, rng, (uint8_t)whimB);
    battleTurn(a, b, x, y, rng, tmp, BATTLE_MAX_EVENTS, false);
  }
  if (!b.hp && a.hp) return 0;
  if (!a.hp) return 1;
  return (uint32_t)a.hp * 1000 / a.maxHp >= (uint32_t)b.hp * 1000 / b.maxHp ? 0 : 1;
}

int main(int argc, char **argv) {
  int N = argc > 1 ? atoi(argv[1]) : 800;
  const int LV[] = { 5, 15, 30, 50, 75, 100, 125, 150 };
  const char *RN[] = { "", "normal", "rare", "legend" };
  printf("Lv   | normal  rare  legend | ventaja neutro desventaja (normal+rare) | turnos\n");
  for (int lv : LV) {
    if (lv > LEVEL_MAX) continue;
    int win[4] = {0}, tot[4] = {0}, tw[3] = {0}, tt[3] = {0};
    long turns = 0; int nt = 0;
    for (uint8_t rar = R_COMUN; rar <= R_LEGENDARIO; rar++) {
      if (rar == R_LEGENDARIO && lv < 30) continue;
      for (int i = 0; i < N; i++) {
        BRng rng((uint32_t)(lv * 7919 + rar * 104729 + i * 2654435761u) | 1);
        Battler me = makeMe(lv, rng, nullptr);
        int16_t fd = foeOf(rar, rng);
        int flv = lv - 4 + (int)rng.below(6); if (flv < 2) flv = 2; if (flv > LEVEL_MAX) flv = LEVEL_MAX;
        Battler foe = makeTrainerMon(fd, (uint16_t)flv);
        bool legend = rar == R_LEGENDARIO;
        wildMatchPower(foe, me, rng, legend);
        int w = fight(me, foe, legend ? 0 : FOE_WHIM, rng.next()) == 0;
        win[rar] += w; tot[rar]++;
        if (!legend) {
          int m = typeMatchMoves(me, foe) + 1;  // 0 desventaja, 1 neutro, 2 ventaja (ko12.10: con sus movimientos)
          tw[m] += w; tt[m]++;
        }
        (void)RN;
      }
    }
    auto pc = [](int a, int b) { return b ? 100 * a / b : -1; };
    printf("%4d | %5d%% %5d%% %5d%% | %6d%% %6d%% %6d%%\n", lv, pc(win[1], tot[1]), pc(win[2], tot[2]),
           pc(win[3], tot[3]), pc(tw[2], tt[2]), pc(tw[1], tt[1]), pc(tw[0], tt[0]));
    (void)turns; (void)nt;
  }
  // ko12.10: gimnasios (equipo fijo, uno tras otro, +20 % de vida al caer cada rival) con tu Pokemon al
  // nivel del mas fuerte del lider
  printf("\ngimnasio | nivel | gana\n");
  for (int g = 0; g < GYM_COUNT; g++) {
    const GymDef &G = GYMS[g];
    int top = 0;
    for (int k = 0; k < G.n; k++) if (G.lv[k] > top) top = G.lv[k];
    for (int dl = -2; dl <= 2; dl += 2) {
      int wins = 0, M = N / 2;
      for (int i = 0; i < M; i++) {
        BRng rng((uint32_t)(g * 7717 + i * 2654435761u + dl * 31) | 1);
        Battler me = makeMe(top + dl, rng, nullptr);
        bool alive = true;
        for (int k = 0; k < G.n && alive; k++) {
          Battler foe = makeTrainerMon(G.dex[k], G.lv[k]);
          BRng r2(rng.next());
          BEvent tmp[BATTLE_MAX_EVENTS];
          for (int t = 0; t < 60 && me.hp && foe.hp; t++) {
            BAct x = battleAi(me, foe, r2, 0), y = battleAi(foe, me, r2, 10);
            battleTurn(me, foe, x, y, r2, tmp, BATTLE_MAX_EVENTS, false);
          }
          if (!me.hp || foe.hp) alive = false;
          uint16_t add = (uint16_t)((uint32_t)me.maxHp * DAILY_HEAL_PCT / 100);
          me.hp = me.hp + add > me.maxHp ? me.maxHp : me.hp + add;
          battleClearVolatile(me);
        }
        wins += alive;
      }
      printf("%8d | %5d | %3d%%\n", g, top + dl, 100 * wins / M);
    }
  }
  return 0;
}
const char *typeName(uint8_t) { return "?"; }  // ko12.10: dexTypesStr (aqui no se escribe)

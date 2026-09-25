#pragma once
// Batallas por turnos (yasaeng / tongsin). Logica pura, sin pantalla ni radio:
// compila en el PC para los tests (test/test_battle.cpp).
//
// Todo es aritmetica ENTERA y con un RNG propio (xorshift32): la batalla por
// ESP-NOW se simula por separado en las dos placas con la misma semilla, y
// tienen que salir exactamente los mismos golpes en ambas.
#include <stdint.h>

// acciones de un turno
// fork KO (ko4): BA_POTION y BA_BALL solo en batallas salvajes (canRun); en
// tongsin se tratan como placaje, igual que huir
enum BAct : uint8_t { BA_TACKLE = 0, BA_TYPE, BA_GUARD, BA_RUN, BA_POTION, BA_BALL, BA_COUNT };

// eventos que la UI reproduce en orden
enum BEvKind : uint8_t {
  EV_HIT = 0,    // golpe: dmg, eff (0 inmune, 1 poco eficaz, 2 normal, 4 muy eficaz), crit
  EV_MISS,       // fallo
  EV_GUARD,      // se protege (+cura)
  EV_RUN_OK,     // huye con exito
  EV_RUN_FAIL,   // no pudo huir
  EV_FAINT,      // se debilita
  EV_HEAL,       // bebe una pocion: dmg = vida recuperada
  EV_CATCH,      // la pokeball atrapa al rival (fin de la batalla)
  EV_BREAK,      // el rival se escapa de la pokeball
};

struct Battler {
  int16_t dex = 1;
  uint16_t lvl = 1;
  uint16_t maxHp = 1, hp = 1;
  uint16_t atk = 1, def = 1, spe = 1;
  uint8_t type = 0;   // PT_*
  bool guard = false; // protegido este turno
};

struct BEvent {
  uint8_t side;     // 0 = a, 1 = b (quien actua)
  uint8_t kind;     // BEvKind
  uint8_t move;     // BA_* usado
  uint8_t eff;      // eficacia x2 (0,1,2,4)
  bool crit;
  uint16_t dmg;     // dano hecho (o curado en EV_GUARD)
  uint16_t hpA, hpB;  // HP de cada lado DESPUES del evento (para animar barras)
};

struct BRng {
  uint32_t s;
  explicit BRng(uint32_t seed = 1) : s(seed ? seed : 0x9E3779B9u) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
};

#define BATTLE_MAX_EVENTS 8   // por turno (2 acciones + desmayo + margen)
#define BATTLE_AUTO_TURNS 40  // tope de la batalla automatica (tongsin)

// eficacia del tipo atacante contra el defensor, en mitades: 0, 1, 2 o 4
uint8_t typeEff(uint8_t atkType, uint8_t defType);

// HP maximo: formula gen 1 con el nivel topado en 100 (aqui el nivel son horas
// de vida y puede pasar de 100; el ataque/defensa ya lo suman aparte)
uint16_t battleHp(uint8_t baseHp, uint16_t lvl);

// monta un combatiente con las stats ya calculadas
Battler makeBattler(int16_t dex, uint16_t lvl, uint16_t atk, uint16_t def, uint16_t spe);

// rival salvaje acorde al nivel (formas base y sus evoluciones; legendarios muy raros)
Battler makeWild(uint16_t petLvl, BRng &rng);

// eleccion de la IA (rival salvaje y ambos lados en la batalla automatica).
// whim = % de veces que elige al azar en vez del mejor golpe
BAct battleAi(const Battler &self, const Battler &foe, BRng &rng, uint8_t whim = 20);

// resuelve un turno: a hace actA, b hace actB. Rellena ev[] y devuelve cuantos.
// canRun: solo en batallas salvajes (en tongsin BA_RUN se trata como placaje)
int battleTurn(Battler &a, Battler &b, BAct actA, BAct actB, BRng &rng,
               BEvent *ev, int maxEv, bool canRun);

// fork KO (ko4): probabilidad (%) de capturar al rival con una pokeball. Sube
// cuanta menos vida le quede; los raros cuestan mas y los legendarios mucho mas.
uint8_t catchChance(const Battler &foe);

// batalla completa entre dos IA (tongsin). Devuelve ganador: 0 = a, 1 = b.
// ev puede ser nullptr (solo el resultado); nEv recibe cuantos eventos se escribieron.
uint8_t battleAuto(Battler a, Battler b, uint32_t seed, BEvent *ev, int maxEv, int *nEv);

// ---- fork KO (ko7): niveles por experiencia, como PokeRogue/los juegos ----
// curva "media-rapida": el nivel n empieza en n^3 de EXP (tope nivel 100)
#define LEVEL_MAX 100
uint32_t expForLevel(uint16_t lvl);      // EXP donde empieza ese nivel (1 -> 0)
uint16_t levelForExp(uint32_t exp);      // nivel que da esa EXP (1..LEVEL_MAX)
// EXP de derrotar/capturar a un rival: rendimiento de la especie x nivel / 4
uint32_t battleExp(int16_t foeDex, uint16_t foeLvl);
// EXP por una hora despierto y bien cuidado: 1/4 del nivel actual
uint32_t careExp(uint16_t lvl);
// nivel al que evoluciona esta especie (0 = forma final). Lineas de 3 fases:
// la base a 16 y la intermedia a su nivel original (minimo 20). Lineas de 2
// fases: el nivel original.
uint8_t evoLevel(int16_t dex);

// potencia y precision de cada movimiento
#define MOVE_TACKLE_POW 40
#define MOVE_TACKLE_ACC 95
#define MOVE_TYPE_POW 65
#define MOVE_TYPE_ACC 88

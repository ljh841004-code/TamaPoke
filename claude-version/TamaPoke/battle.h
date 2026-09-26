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

// ---- ko10.1: encuentros por region (una por tipo, = escenario 0..15) ----
// En cada encuentro, por orden:
//   1. RARO:     solo si se cumple su condicion (region + hora + tiempo + estacion), ~0,5-1 %
//   2. HORA:     los de esa region a esa hora (manana 6-10, dia 10-20, noche 20-6), 20 %
//   3. REGION:   los de su tipo (comunes x3, raros x1), 50 %
//   4. COMUN:    los de cualquier sitio (Pidgey, Rattata...), el resto
#define REGION_COUNT 16
enum : uint8_t { WG_COMMON = 0, WG_REGION, WG_TIME, WG_RARE };
enum : uint8_t { WS_MORNING = 1, WS_DAY = 2, WS_NIGHT = 4, WS_ANY = 7 };
#define WILD_TIME_PCT 20
#define WILD_REGION_PCT 50
#define WILD_LEGEND_MIN_LVL 40
uint8_t wildSlot(uint8_t hour);
// wx = WX_* de weather.h, season = SEASON_*; group (opcional) dice de que grupo salio
Battler makeWildIn(uint8_t region, uint16_t petLvl, uint8_t hour, uint8_t wx, uint8_t season,
                   BRng &rng, uint8_t *group);
// ---- ko10.4: gimnasios (8 medallas de Kanto) y reto del dia
#define GYM_COUNT 8
#define GYM_MAX_TEAM 3
struct GymDef { uint8_t region, n; int16_t dex[GYM_MAX_TEAM]; uint8_t lv[GYM_MAX_TEAM]; };
extern const GymDef GYMS[GYM_COUNT];
Battler makeTrainerMon(int16_t dex, uint16_t lvl);  // stats fijas (genes 105)
uint8_t badgeCount(uint8_t badges);
// medallas necesarias para ir a la region (0 = abierta desde el principio)
uint8_t regionBadgesNeeded(uint8_t region);
bool regionUnlocked(uint8_t region, uint8_t badges);
// reto del dia: region y 3 rivales salen del dia (igual en todos los aparatos)
#define DAILY_TEAM 3
uint8_t dailyRegion(uint32_t day);
void dailyTeam(uint32_t day, uint16_t petLvl, Battler out[DAILY_TEAM]);

// probabilidad (por mil) de la especie "dex" en ese momento, antes de evolucionar
// por nivel (para tests y para el comando serie WILD)
uint16_t wildPermil(int16_t dex, uint8_t region, uint16_t petLvl, uint8_t hour, uint8_t wx,
                    uint8_t season);

// eleccion de la IA (rival salvaje y ambos lados en la batalla automatica).
// whim = % de veces que elige al azar en vez del mejor golpe
BAct battleAi(const Battler &self, const Battler &foe, BRng &rng, uint8_t whim = 20);

// ko10.4: nivel del ataque de tipo segun la fase evolutiva (0 basico, 1 medio,
// 2 definitivo). Basica = primera fase de una linea; final = ultima; sin
// evoluciones: 1 (legendarios 2). Ej.: Squirtle 0 (Pistola Agua), Wartortle 1
// (Hidropulso), Blastoise 2 (Hidrobomba). Solo cambia nombre y efecto, no el dano
uint8_t moveTier(int16_t dex);

// ko10.4: tiempo de la batalla (WX_* de weather.h). Lluvia: agua x1,5, fuego x0,5;
// sol: fuego x1,5, agua x0,5; nieve: hielo x1,5. battleAuto (tongsin) usa siempre buen tiempo
void battleSetWeather(uint8_t wx);
uint8_t battleWeather();
uint8_t weatherMul(uint8_t wx, uint8_t moveType);  // en medios (3 = x1,5)

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
// ko9: tiempo de crianza que cuesta subir del nivel lvl al siguiente sin
// batallas: CARE_MIN_PER_LVL x nivel (ko10.2: 15 min; 1->2 un cuarto de hora, 2->3 media hora...)
#define CARE_MIN_PER_LVL 15UL  // ko10.2: minutos de crianza por nivel (antes 30)
uint32_t careMinutesForLevel(uint16_t lvl);
// nivel al que evoluciona esta especie (0 = forma final). Lineas de 3 fases:
// la base a 16 y la intermedia a su nivel original (minimo 20). Lineas de 2
// fases: el nivel original.
uint8_t evoLevel(int16_t dex);

// potencia y precision de cada movimiento
#define MOVE_TACKLE_POW 40
#define MOVE_TACKLE_ACC 95
#define MOVE_TYPE_POW 65
#define MOVE_TYPE_ACC 88

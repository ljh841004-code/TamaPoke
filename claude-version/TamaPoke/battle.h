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
// ko11.31: BA_M0..BA_M3 = el movimiento de esa casilla (Battler::mv). BA_TACKLE y BA_TYPE
// quedan como atajos (placaje / el ataque de su tipo) para el codigo y los tests de antes
enum BAct : uint8_t { BA_TACKLE = 0, BA_TYPE, BA_GUARD, BA_RUN, BA_POTION, BA_BALL,
                      BA_M0, BA_M1, BA_M2, BA_M3, BA_COUNT };

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
  EV_COUNTER,    // ko11.8: se protegio, le dieron y devuelve el golpe (dmg, eff; nunca falla ni es critico)
  // ko11.31: movimientos de estado y estados alterados. side = a quien le pasa
  EV_USE,        // usa un movimiento de estado (mid): solo el mensaje y el efecto
  EV_STAT,       // cambia una caracteristica: eff = 0 atq / 1 def / 2 vel, val = cambio (0 = ya no puede mas)
  EV_STATUS,     // queda con un estado: val = ST_*
  EV_NOEFFECT,   // el movimiento de estado no hace nada (inmune, ya tenia estado, se protegia)
  EV_STDMG,      // le hace dano su estado (veneno / quemadura): dmg, val = ST_*
  EV_CANT,       // no puede moverse: val = ST_PAR / ST_SLP / ST_FRZ / ST_FLINCH
  EV_CURE,       // se le pasa: val = ST_SLP (despierta), ST_FRZ (se descongela), ST_CNF
  EV_CONFHIT,    // confuso: se golpea a si mismo (dmg)
  EV_RECOIL,     // dano de retroceso (dmg)
  EV_DRAIN,      // recupera vida al drenar (dmg = curado)
};

// ko11.31: estados alterados (val de los eventos). Los de batalla se quitan al terminar
enum : uint8_t { ST_NONE = 0, ST_PSN, ST_BRN, ST_PAR, ST_SLP, ST_FRZ, ST_CNF, ST_FLINCH, ST_RECHARGE };

struct Battler {
  int16_t dex = 1;
  uint16_t lvl = 1;
  uint16_t maxHp = 1, hp = 1;
  uint16_t atk = 1, def = 1, spe = 1;
  uint8_t type = 0;   // PT_*
  bool guard = false; // protegido este turno
  // ko11.31: sus 4 movimientos (0 = hueco) y los PP que les quedan
  uint8_t mv[4] = { 0, 0, 0, 0 };
  uint8_t pp[4] = { 0, 0, 0, 0 };
  int8_t stg[3] = { 0, 0, 0 };  // atq, def, vel: -6..+6
  uint8_t st = 0, stT = 0;      // estado (ST_PSN..ST_FRZ) y turnos de sueno
  uint8_t cnf = 0;              // turnos de confusion
  bool flinch = false;
  bool recharge = false;        // tras hiperrayo y similares: pierde el turno siguiente
};

struct BEvent {
  uint8_t side;     // 0 = a, 1 = b (quien actua)
  uint8_t kind;     // BEvKind
  uint8_t move;     // BA_* usado
  uint8_t mid;      // ko11.31: id del movimiento (0 = ninguno)
  int8_t val;       // ko11.31: ver cada evento
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

#define BATTLE_MAX_EVENTS 16  // por turno (2 acciones con efectos + estados + desmayo)
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
//   4. COMUN:    comunes de esa region (ko11.16: antes los mismos en todas partes), el resto
#define REGION_COUNT 16
enum : uint8_t { WG_COMMON = 0, WG_REGION, WG_TIME, WG_RARE };
enum : uint8_t { WS_MORNING = 1, WS_DAY = 2, WS_NIGHT = 4, WS_ANY = 7 };
#define WILD_TIME_PCT 20
#define WILD_REGION_PCT 50
#define WILD_LEGEND_MIN_LVL 40
uint8_t wildSlot(uint8_t hour);
// wx = WX_* de weather.h, season = SEASON_*; group (opcional) dice de que grupo salio
// ko11.7: eventos del dia (la hora LOCAL). Lunes agua, martes fuego, miercoles
// planta, jueves electrico, viernes psiquico; sabado y domingo shiny x2. Y las
// noches de luna llena, los legendarios salen el triple
enum : uint8_t { DEV_NONE = 0, DEV_TYPE, DEV_SHINY };
struct DayEvent { uint8_t kind; uint8_t ptype; bool moonNight; };
bool fullMoon(uint32_t epoch);            // luna llena ese dia (±1 dia)
DayEvent dayEvent(uint32_t localEpoch);   // 0 = sin reloj: sin evento
// ko11.19: combate automatico (entrenadores): medicina segun lo dificil que sea
#define AUTO_POTION_HP 30        // usa pocion por debajo del 30 % ...
#define AUTO_POTION_HP_HARD 50   // ... o del 50 % si el rival es fuerte
#define AUTO_POTION_MAX 3        // pociones por combate (rival normal); fuerte: sin limite
#define AUTO_STEP_MS 700         // pausa entre turnos del combate automatico
#define DAILY_HEAL_PCT 35                 // ko11.18: reto del dia: entre rivales, +35 % de la vida que queda
#define EVENT_TYPE_PCT 25                 // dia de un tipo: 1 de cada 4 es de ese tipo
#define EVENT_MOON_MULT 3
Battler makeWildIn(uint8_t region, uint16_t petLvl, uint8_t hour, uint8_t wx, uint8_t season,
                   BRng &rng, uint8_t *group, const DayEvent *ev = nullptr);
// ---- ko11.7: expediciones (un Pokemon de la caja se va unas horas a su
// region y vuelve con cosas). Horas: 2, 4 u 8
struct ExpReward { uint8_t candy, balls, potions, rare; bool newMon; };
ExpReward expeditionReward(uint8_t hours, uint16_t lvl, BRng &rng);
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
// ko10.11: ajusta el nivel del salvaje para que su fuerza (vida + ataque + defensa
// + velocidad) quede cerca de la tuya (90-105 %): especies flojas salen con mas
// nivel, y las fuertes con menos (entre -6 y +10 del nivel que traia)
#define WILD_LVL_OVER 3  // ko11.12: el salvaje como mucho 3 niveles por encima del tuyo
void wildMatchPower(Battler &foe, const Battler &me, BRng &rng);
uint32_t battlerPower(const Battler &b);
// ---- ko10.11: revancha de gimnasio (con la medalla) y liga (con las 8)
extern const uint8_t GYM_TYPE[GYM_COUNT];   // tipo de cada gimnasio (roca, agua...)
#define REMATCH_MAX 4
#define CHAMP_TEAM 6
#define CHAMP_REGION 13                     // valle del dragon
// revancha: 3-4 del tipo del gimnasio (sin legendarios), a tu nivel (-1..+2,
// el ultimo el mas fuerte). Devuelve cuantos
uint8_t gymRematchTeam(uint8_t gym, uint16_t petLvl, uint32_t seed, Battler out[REMATCH_MAX]);
// liga: 6 formas finales fuertes (sin legendarios) de tipos distintos, nivel +2..+5
void championTeam(uint16_t petLvl, uint32_t seed, Battler out[CHAMP_TEAM]);

// ---- ko11.20: equipo (el que crias + hasta 2 ayudantes de la caja) contra entrenadores
#define PARTY_MAX 6     // ko12.2: el que crias + hasta 5 ayudantes (tantos como el rival: liga 6)
#define TEAM_HELPERS 5  // ko12.2: ayudantes de la caja en un combate (como mucho rival - 1)
#define PARTY_HELPERS 2 // ayudantes que recuerda la ficha del salon de la liga (formato guardado: 2)
#define HELPER_USES_PER_DAY 3  // cada ayudante: 3 combates al dia
// un Pokemon de la caja como luchador: su nivel no pasa del de tu Pokemon (asi un
// Lv70 de la caja no gana solo) y sus genes cuentan como en los salvajes
Battler makeBoxBattler(int16_t dex, uint16_t lvl, uint16_t petLvl, uint8_t gA, uint8_t gD, uint8_t gS);
extern const Battler *gStoryFlyAt;  // ko11.23.3: historia: la familia Pidgey (solo la mia) x2 contra hierba
// tipo mio contra el tipo del rival: +1 ventaja (le pego muy eficaz y el a mi no),
// -1 desventaja (al reves), 0 igual
int8_t typeMatch(uint8_t mine, uint8_t foe);
// el que entra al cambiar voluntariamente pierde el turno: solo ataca el rival (a = yo)
int battleFoeOnly(Battler &a, Battler &b, BAct actB, BRng &rng, BEvent *ev, int maxEv);
// liga (como PokeRogue): de los que le quedan (team[from..n-1]) el campeon saca el
// que mejor le va contra mi tipo; empate = el primero (el orden de siempre)
uint8_t pickNextFoe(const Battler *team, uint8_t from, uint8_t n, uint8_t myType);

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
// ko11.31: variante del ataque de tipo (0..2) de un Pokemon que no es el que crias: cambia cada 5 niveles
uint8_t moveVarFor(int16_t dex, uint16_t lvl);
// ko10.5: primera forma de su linea (siguiendo las preevoluciones: Raichu ->
// Pichu, Hitmonchan -> Tyrogue, Blastoise -> Squirtle)
int16_t dexFirstForm(int16_t dex);

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
uint16_t counterDamage(const Battler &at, const Battler &df, BRng &rng, uint8_t *effOut);  // ko11.8

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

// potencia y precision de cada movimiento (ko11.31: solo el contraataque usa estos;
// el resto sale de MOVE_TBL)
#define MOVE_TACKLE_POW 40
#define MOVE_TACKLE_ACC 95
#define MOVE_TYPE_POW 65
#define MOVE_TYPE_ACC 88

// ---- ko11.31: movimientos (4 por Pokemon, con PP). Ids: moves_data.h
//   1..144 ataque de tipo (1 + tipo*9 + fase*3 + variante), 145 placaje, 146.. de estado
#define STATUS_CURE_PCT 16  // ko11.31: % por turno de que se pase el veneno / quemadura / paralisis
#define MOVE_TACKLE 145
#define MOVE_STATUS0 146
#define MOVE_STRUGGLE 255
// banderas de MoveDef::flags
enum : uint8_t { MF_HICRIT = 1, MF_DRAIN = 2, MF_RECOIL = 4, MF_FIXLVL = 8, MF_FIX = 16, MF_STATUS = 32, MF_RECHARGE = 64,
                 MF_SELFCNF = 128 };
// estado alterado (ail): 1 veneno, 2 quemadura, 3 paralisis, 4 sueno, 5 congelado, 6 confusion
struct MoveDef {
  uint8_t type, pow, acc, pp;
  int8_t prio;
  uint8_t flags;
  uint8_t ail, ailCh;      // estado y su % (0 = siempre si es de estado)
  uint8_t st; int8_t stD;  // caracteristica (0 atq, 1 def, 2 vel) y cambio
  uint8_t stSelf, stCh;    // 1 = a si mismo; % (0 = siempre si es de estado)
  uint8_t flinch, drain;   // % de retroceso; % drenado (MF_DRAIN) o de retroceso (MF_RECOIL)
};
const MoveDef &moveDef(uint8_t id);
uint8_t moveIdTyped(uint8_t type, uint8_t tier, uint8_t var);
bool moveValid(uint8_t id);                         // 1..MOVE_N-1
bool moveIsTyped(uint8_t id);                       // 1..144
bool moveIsStatus(uint8_t id);
uint8_t moveType(uint8_t id);
uint8_t movePP(uint8_t id);                         // PP maximos
// tipo, fase y variante de un ataque de tipo (para nombre y efecto); false si no lo es
bool moveDecode(uint8_t id, uint8_t *type, uint8_t *tier, uint8_t *var);
// lo que puede aprender: placaje + los de su tipo hasta su fase + los que aprende en los juegos
bool moveCanLearn(int16_t dex, uint8_t id);
uint8_t movePool(int16_t dex, uint8_t *out, uint8_t max);
// los 4 de un Pokemon que no es el que crias (rivales, ayudantes, historia): fijos por
// especie y tramo de 5 niveles. Huecos = 0
void movesDefault(int16_t dex, uint16_t lvl, uint8_t out[4]);
uint8_t moveCount(const uint8_t mv[4]);
void movesFillPP(Battler &b);                       // PP llenos
bool movesHas(const uint8_t mv[4], uint8_t id);
// el que se ve en los dibujos de "su ataque" (el primero de su tipo; si no, el de su fase)
uint8_t movesMain(const Battler &b);
// accion -> casilla (0..3) o -1; BA_TYPE/BA_TACKLE buscan ese movimiento entre los suyos
int8_t battleSlot(const Battler &b, BAct a);
bool battleHasPP(const Battler &b);                 // false: solo puede forcejear
// dano esperado de un movimiento (para la IA y para marcar "muy eficaz" en el menu)
uint8_t moveEffAgainst(uint8_t id, const Battler &df);
void battleClearVolatile(Battler &b);  // al terminar: fuera estados y cambios

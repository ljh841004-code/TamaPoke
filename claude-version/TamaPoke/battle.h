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
  // ko12.10: efectos de los movimientos originales: val = MSG_*, mid = el movimiento, eff = un numero
  // (golpes, cuenta atras, tipo...) y dmg si quita o cura vida (a quien dice side)
  EV_MSG,
};

// ko12.10: mensajes de EV_MSG
enum : uint8_t {
  MSG_FAIL = 0,     // pero ha fallado
  MSG_CHARGE,       // carga (rayo solar, viento cortante, cabezazo, ataque aereo: mid dice cual)
  MSG_FLY,          // vuela alto
  MSG_DIG,          // se mete bajo tierra
  MSG_PROTECT,      // se protege (proteccion / detectar)
  MSG_PROTECTED,    // side se protegio del ataque
  MSG_ENDURE,       // aguante: se prepara
  MSG_ENDURED,      // aguanto el golpe con 1 PS
  MSG_TRAPPED,      // side queda atrapado (mid = atadura, constriccion...)
  MSG_TRAPDMG,      // le hace dano estar atrapado (dmg)
  MSG_TRAPEND,      // se libera
  MSG_SEEDED,       // drenadoras plantadas en side
  MSG_SEEDDMG,      // drenadoras: side pierde dmg (y el otro lo recupera)
  MSG_WX_RAIN, MSG_WX_SUN, MSG_WX_SAND,  // empieza a llover / hace sol / tormenta de arena
  MSG_WX_END,       // el tiempo vuelve a ser el de antes
  MSG_SANDDMG,      // la arena le hace dano (dmg)
  MSG_PERISH,       // canto mortal: eff = cuenta (3, 2, 1, 0 = se debilita)
  MSG_LOCKON,       // apunta (fijar blanco / telepatia)
  MSG_SUB,          // sustituto (dmg = vida que pone)
  MSG_SUBHIT,       // el sustituto recibe el golpe
  MSG_SUBBREAK,     // el sustituto se rompe
  MSG_DISABLE,      // anula el movimiento mid del rival
  MSG_DISABLE_END,
  MSG_ENCORE,       // otra vez: side repite mid
  MSG_ENCORE_END,
  MSG_SPIKES,       // puas en el lado de side
  MSG_SPIKEDMG,     // las puas le hacen dano al salir (dmg)
  MSG_CURSE,        // maldicion (fantasma): side maldito
  MSG_CURSEDMG,
  MSG_NIGHTMARE,
  MSG_NIGHTMAREDMG,
  MSG_FORESIGHT,    // profecia: side ya no esquiva / el fantasma recibe normal y lucha
  MSG_DBOND,        // mismodestino: se prepara
  MSG_DBOND_HIT,    // se lleva al rival con el
  MSG_BIDE,         // venganza: aguanta
  MSG_BIDE_GO,      // venganza: suelta el dano
  MSG_REST,         // descanso: duerme y se cura
  MSG_TRANSFORM,    // se transforma en el rival (eff = 0)
  MSG_MIMIC,        // aprende mid (mimetismo / esquema)
  MSG_CONVERSION,   // cambia a tipo eff
  MSG_HAZE,         // niebla: todos los cambios fuera
  MSG_MIST,         // neblina: no le bajan las caracteristicas
  MSG_MISTED,       // la neblina lo protege
  MSG_LSCREEN, MSG_REFLECT, MSG_SAFEGUARD,
  MSG_SAFEGUARDED,  // velo sagrado lo protege
  MSG_SCREEN_END,   // se acaba eff: 0 pantalla luz, 1 reflejo, 2 neblina, 3 velo sagrado
  MSG_LOVE,         // se enamora
  MSG_BELLYDRUM,    // tambor: pierde la mitad y ataque al maximo
  MSG_PSYCHUP,      // copia los cambios del rival
  MSG_PAINSPLIT,    // divide el dolor
  MSG_HITS,         // golpeo eff veces
  MSG_OHKO,         // fulminante
  MSG_SPLASH,       // no pasa nada
  MSG_RAMPAGE_END,  // se cansa y se confunde
  MSG_FOCUS,        // foco energia
  MSG_HEALBELL,     // campana cura: se le pasa el estado
  MSG_SPITE,        // rencor: eff PP menos a mid
  MSG_FUTURE,       // premonicion: lo ve venir
  MSG_FUTURE_HIT,   // premonicion: llega (dmg)
  MSG_MAGNITUDE,    // magnitud eff
  MSG_PRESENT_HEAL, // presente: le cura (dmg)
  MSG_CRASH,        // falla y se estrella (dmg)
  MSG_MEANLOOK,     // side ya no puede huir
  MSG_RAPIDSPIN,    // giro rapido: se libera
  MSG_PAYDAY,       // dia de pago: monedas
  MSG_SKETCH,       // esquema: lo aprende para siempre (mid)
  MSG_NOESCAPE,     // no puede huir (atrapado / mal de ojo)
  MSG_SWAGGER,      // contoneo
  MSG_SLEEPTALK,    // sonambulo: mid = el que sale
  MSG_METRONOME,    // metronomo: mid = el que sale
  MSG_MIRRORMOVE,   // mov. espejo: mid = el que sale
  MSG_RAGE,         // furia: sube el ataque (al recibir un golpe)
  MSG_PERISH_SING,  // canto mortal: los dos caen en 3 turnos
  MSG_N
};

// ko11.31: estados alterados (val de los eventos). Los de batalla se quitan al terminar
// ko12.8: val de EV_HIT (bits): el otro se protegia (dano a la mitad) / ese movimiento descansa
enum : uint8_t { HIT_GUARDED = 1, HIT_REST = 2 };
// ko12.9.13: val de EV_RUN_FAIL: el rival le pego al huir (dmg) o lo esquivo
enum : uint8_t { RUN_FAIL_HIT = 1, RUN_FAIL_DODGE = 2 };
enum : uint8_t { ST_NONE = 0, ST_PSN, ST_BRN, ST_PAR, ST_SLP, ST_FRZ, ST_CNF, ST_FLINCH, ST_RECHARGE,
                 ST_LOVE, ST_TOX, ST_DISABLED };  // ko12.10: los 3 ultimos solo en eventos (enamorado, toxico, anulado)
// ko12.10: caracteristicas con cambios (-6..+6), como en el original (2a gen: at. esp. y def. esp. aparte)
enum : uint8_t { STG_ATK = 0, STG_DEF, STG_SPE, STG_SPA, STG_SPD, STG_ACC, STG_EVA, STG_N };

struct Battler {
  int16_t dex = 1;
  uint16_t lvl = 1;
  uint16_t maxHp = 1, hp = 1;
  uint16_t atk = 1, def = 1, spe = 1;
  uint8_t type = 0;   // PT_* (el primero; = DEX_TBL.ptype)
  uint8_t type2 = 0xFF;  // ko12.10: segundo tipo (PT_FLYING...) o 0xFF
  bool guard = false; // protegido este turno
  // ko11.31: sus 4 movimientos (0 = hueco) y los PP que les quedan
  uint8_t mv[4] = { 0, 0, 0, 0 };
  uint8_t pp[4] = { 0, 0, 0, 0 };
  int8_t stg[STG_N] = { 0, 0, 0, 0, 0, 0, 0 };  // STG_*: -6..+6
  uint8_t st = 0, stT = 0;      // estado (ST_PSN..ST_FRZ) y turnos de sueno
  uint8_t cnf = 0;              // turnos de confusion
  bool flinch = false;
  // ko12.8: tras hiperrayo y similares (si el otro sigue en pie): ESE movimiento no se puede
  // usar el turno siguiente (los demas y protegerse, si). restT: turnos que le quedan
  uint8_t restMv = 0, restT = 0;
  // ko12.10: lo que piden los movimientos originales (todo se borra al acabar o al cambiar)
  uint8_t bond = 70;            // amistad (retribucion / frustracion)
  uint8_t hpType = 0;           // tipo de poder oculto
  uint8_t tox = 0;              // veneno grave: cuenta (0 = normal)
  uint8_t trapT = 0, trapMv = 0;  // atrapado (atadura...): turnos y movimiento
  uint8_t chargeMv = 0;         // cargando (2 turnos)
  uint8_t invul = 0;            // 1 en el aire (vuelo), 2 bajo tierra (excavar)
  uint8_t protN = 0;            // protecciones seguidas
  uint8_t reflT = 0, lscrT = 0, mistT = 0, safeT = 0;  // reflejo, pantalla luz, neblina, velo sagrado
  uint8_t disMv = 0, disT = 0;  // anulado
  uint8_t encMv = 0, encT = 0;  // otra vez
  uint8_t lastMv = 0;           // el ultimo que uso
  uint8_t rampMv = 0, rampT = 0;  // golpe/danza petalo/enfado
  uint8_t rollT = 0;            // rodar: golpes seguidos
  uint8_t furyN = 0;            // corte furia: aciertos seguidos
  uint8_t bideT = 0;            // venganza: turnos aguantando
  uint8_t futT = 0;             // premonicion que le va a llegar (turnos)
  uint8_t perish = 0;           // canto mortal: cuenta + 1
  uint8_t spikes = 0;           // puas en su lado
  uint8_t sketched = 0;         // esquema: lo aprendio para siempre (el que crias)
  uint8_t fieldWx = 0, fieldWxT = 0;  // tiempo de los movimientos (los dos lados llevan copia: cada placa el suyo)
  uint16_t subHp = 0;           // sustituto
  uint16_t lastDmgP = 0, lastDmgS = 0;  // dano fisico / especial recibido este turno (contraataque, manto espejo)
  uint16_t bideDmg = 0, futDmg = 0;
  int16_t xform = 0;            // transformado en esa especie (0 = no)
  bool protect = false, endure = false, focus = false, seeded = false, lockon = false, foresight = false;
  bool nightmare = false, cursed = false, dbond = false, attract = false, meanLook = false, rage = false;
  bool curled = false, spkPend = false, minimized = false;
  // lo de antes de transformarse
  uint8_t oType = 0, oType2 = 0xFF, oMv[4] = { 0, 0, 0, 0 }, oPp[4] = { 0, 0, 0, 0 };
  uint16_t oAtk = 0, oDef = 0, oSpe = 0;
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

#define BATTLE_MAX_EVENTS 32  // por turno (ko12.10: golpes multiples y efectos de fin de turno)
#define BATTLE_AUTO_TURNS 40  // tope de la batalla automatica (tongsin)

// eficacia del tipo atacante contra el defensor, en mitades: 0, 1, 2 o 4
uint8_t typeEff(uint8_t atkType, uint8_t defType);
// ko12.10: contra dos tipos, en CUARTOS: 0, 1 (x1/4), 2 (x1/2), 4 (normal), 8 (x2), 16 (x4)
uint8_t typeEffQ(uint8_t atkType, uint8_t t1, uint8_t t2);
// cuartos -> la eficacia de siempre (mitades, para mensajes y marcas): 0, 1 poco, 2 normal, 4 mucho
uint8_t effCat(uint8_t q);
bool battlerHasType(const Battler &b, uint8_t t);

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
struct DayEvent { uint8_t kind; uint8_t ptype; bool moonNight;
                  bool lure = false;                    // ko12.9.11: "부르는 구슬" en uso
                  bool (*isNew)(int16_t dex) = nullptr; // especie aun no vista: x2 mas
};
// ko12.9.11: "부르는 구슬" (lure): N combates salvajes con raros x2 y legendarios x3; los que aun no
// has visto, x2 mas. Con la luna llena el legendario no pasa de x6 (las condiciones de lugar/hora/
// tiempo/Lv40 siguen igual: solo sube la probabilidad)
#define LURE_BATTLES 5  // ko12.10.15: 10 -> 5
#define LURE_RARE_MULT 2
#define LURE_LEGEND_MULT 3
#define LURE_NEW_MULT 2
#define LURE_LEGEND_CAP 6
#define LURE_RARE_CAP 4
#define LURE_MAX 99
#define LURE_CHAMP_PCT 25   // ganar la liga
#define LURE_DAILY_PCT 20   // premio del reto del dia
#define LURE_EXPED8_PCT 30  // expedicion de 8 h
uint32_t wildRareMult(int16_t dex, const DayEvent *ev);  // multiplicador de un raro de WILD_RARE
// ko12.9.14: rango del salvaje vencido: 0 normal, 1 raro (salio del grupo WG_RARE), 2 legendario o shiny.
// Cuanto mas raro, mas quiere unirse a la caja y mas premio da
enum : uint8_t { WT_NORMAL = 0, WT_RARE, WT_SPECIAL };
#define WILD_JOIN_PCT 15          // normal (antes 20 para todos)
#define WILD_JOIN_RARE_PCT 25
#define WILD_JOIN_SPECIAL_PCT 35
#define WILD_RARE_CANDY 3         // caramelos de su familia al ganar a un raro
#define WILD_SPECIAL_CANDY 5      // ... a un legendario / shiny (y un orbe de su tipo seguro)
uint8_t wildTier(int16_t dex, bool rareGroup, bool shiny);
uint8_t wildJoinPct(uint8_t tier);
bool fullMoon(uint32_t epoch);            // luna llena ese dia (±1 dia)
DayEvent dayEvent(uint32_t localEpoch);   // 0 = sin reloj: sin evento
// ko11.19: combate automatico (entrenadores): medicina segun lo dificil que sea
#define AUTO_POTION_HP 30        // usa pocion por debajo del 30 % ...
#define AUTO_POTION_HP_HARD 50   // ... o del 50 % si el rival es fuerte
#define AUTO_POTION_MAX 3        // pociones por combate (rival normal); fuerte: sin limite
#define AUTO_STEP_MS 700         // pausa entre turnos del combate automatico
#define DAILY_HEAL_PCT 20                 // ko12.5.1: gimnasio, liga y reto del dia: al caer un rival, +20 % de la vida MAXIMA
                                          // (antes solo el reto del dia, +35 % de la que quedaba: casi nada con poca vida)
#define BENCH_HEAL_PCT 5                  // ko12.5.1: el que descansa en el banquillo recupera 5 % por turno...
#define BENCH_HEAL_CAP 50                 // ...hasta la mitad de su vida (cambiar sin parar no cura del todo)
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
// ko12.9.23: fuerza del salvaje en % de la tuya (+0..15 al azar). Ajustado con test/sim (make -C test/sim)
// ko12.10: con los movimientos originales, el tope 100 y la formula de los juegos (sim, 800 por casilla):
// comun ~72 %, raro ~63 %, legendario ~20 %; con ventaja de tipo ~90 %, igual ~68 %, desventaja ~45 %
#define WILD_POW_LOW 80        // comun, tu nivel < 20
#define WILD_POW_NORMAL 81     // comun
#define WILD_POW_RARE 88       // raro (un poco mas fuerte que el comun)
#define WILD_POW_LEGEND 116    // legendario a Lv100; mas abajo hasta -12
#define WILD_POW_TYPE_BAD 87   // te gana por tipo (con sus movimientos): x0,87
#define WILD_POW_TYPE_GOOD 104 // le ganas por tipo: x1,04
#define WILD_LVL_OVER 3  // ko11.12: el salvaje como mucho 3 niveles por encima del tuyo
void wildMatchPower(Battler &foe, const Battler &me, BRng &rng, bool legend = false);
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
int8_t typeMatchD(int16_t mine, int16_t foe);  // ko12.10: especie contra especie (dos tipos)
// ko12.10: con sus movimientos de verdad: +1 si el mejor golpe mio es muy eficaz y el suyo no (o el suyo
// no le hace nada), -1 al reves, 0 igual. Sin movimientos de dano: por especie (typeMatchD)
int8_t typeMatchMoves(const Battler &me, const Battler &foe);
int8_t typeMatchTT(uint8_t m1, uint8_t m2, uint8_t f1, uint8_t f2);  // tipos (0xFF = sin segundo)
uint8_t dexType2(int16_t dex);  // ko12.10: segundo tipo de la especie (0xFF = ninguno)
const char *dexTypesStr(int16_t dex, char *buf, int n);  // "불꽃/비행"
// el que entra al cambiar voluntariamente pierde el turno: solo ataca el rival (a = yo)
int battleFoeOnly(Battler &a, Battler &b, BAct actB, BRng &rng, BEvent *ev, int maxEv);
// liga (como PokeRogue): de los que le quedan (team[from..n-1]) el campeon saca el
// que mejor le va contra mi tipo; empate = el primero (el orden de siempre)
uint8_t pickNextFoe(const Battler *team, uint8_t from, uint8_t n, int16_t myDex);

// probabilidad (por mil) de la especie "dex" en ese momento, antes de evolucionar
// por nivel (para tests y para el comando serie WILD)
// ko12.9.6: % de salvajes que se quedan en una fase anterior (formas intermedias a cualquier nivel)
#define WILD_STAY_PCT 30
// ko12.9.6: pista de la pokedex: donde y cuando sale (o su forma base). region 0xFF = cualquiera;
// wx: 0xFF cualquiera, 0xFE lluvia/nieve, 0xFD sin lluvia, si no WX_*. viaBase = la forma base de la que
// sale evolucionado (0 = sale tal cual); minLvl = nivel de tu bicho necesario (0 = cualquiera)
struct WildHint { uint8_t region, slots, wx; bool rare; };
int wildHints(int16_t dex, WildHint *out, int max, int16_t *viaBase, uint8_t *minLvl);
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
uint8_t battleWeather();  // ko12.10: el que cuenta ahora (danza lluvia / dia soleado / tormenta arena: 5 turnos)
#define WX_SAND 6         // ko12.10: tormenta de arena (solo en batalla; weather.h no la tiene)
// ko12.10: rugido / remolino / teletransporte acaban la batalla: solo contra salvajes (por defecto, fallan)
void battleSetFleeMoves(bool ok);
uint8_t weatherMul(uint8_t wx, uint8_t moveType);  // en medios (3 = x1,5)

// resuelve un turno: a hace actA, b hace actB. Rellena ev[] y devuelve cuantos.
// canRun: solo en batallas salvajes (en tongsin BA_RUN se trata como placaje)
// ko12.9.13: huir cuesta mas (35% + media diferencia de velocidad, 15..75; antes 50, 25..95)
// y al fallar el rival pega flojo a suertes (50%: 1/16..1/8 de la vida, nunca debilita)
#define RUN_BASE 35
#define RUN_MIN 15
#define RUN_MAX 75
#define RUN_HIT_PCT 50
#define RUN_HIT_DIV 16
int runChance(uint32_t speMe, uint32_t speFoe);
int battleTurn(Battler &a, Battler &b, BAct actA, BAct actB, BRng &rng,
               BEvent *ev, int maxEv, bool canRun);

// fork KO (ko4): probabilidad (%) de capturar al rival con una pokeball. Sube
// cuanta menos vida le quede; los raros cuestan mas y los legendarios mucho mas.
uint8_t catchChance(const Battler &foe);
uint16_t counterDamage(const Battler &at, const Battler &df, BRng &rng, uint8_t *effOut);  // ko11.8

// batalla completa entre dos IA (tongsin). Devuelve ganador: 0 = a, 1 = b.
// ev puede ser nullptr (solo el resultado); nEv recibe cuantos eventos se escribieron.
uint8_t battleAuto(Battler a, Battler b, uint32_t seed, BEvent *ev, int maxEv, int *nEv);

// ---- ko12.9.16: tongsin por turnos, equipo de 3 contra 3 ----
// Cada placa elige su accion; se intercambian y las dos resuelven el MISMO turno con la misma
// semilla (lado A = la placa de MAC menor). Primero los cambios, luego los ataques.
#define LINK_TEAM 3
#define LA_SWAP 100     // 100 + j: sale el j del equipo
#define LA_NONE 250     // no hace nada (el otro elige quien sale tras un desmayo)
#define LA_GIVEUP 251   // se rinde
#define LA_WAIT 0xFE    // aun no ha elegido
struct LinkSide {
  Battler mon[LINK_TEAM];
  uint8_t n = 1, cur = 0;
  Battler &act() { return mon[cur < n ? cur : 0]; }
};
bool linkSideAlive(const LinkSide &s);       // le queda alguien en pie
bool linkSideCanSwap(const LinkSide &s);     // tiene otro en pie aparte del que lucha
// aplica el cambio de un lado (true si cambio). Un cambio a uno caido o inexistente no hace nada,
// salvo si el que lucha esta caido: entonces sale el primero en pie
bool linkApplySwap(LinkSide &s, uint8_t act);
// los ataques del turno (los cambios ya aplicados): ev igual que battleTurn (lado 0 = A)
int linkAttacks(LinkSide &A, LinkSide &B, uint8_t actA, uint8_t actB, BRng &rng, BEvent *ev, int maxEv);

// ---- fork KO (ko7): niveles por experiencia, como PokeRogue/los juegos ----
// curva "media-rapida": el nivel n empieza en n^3 de EXP (tope LEVEL_MAX)
#define LEVEL_MAX 100  // ko12.10: 100 como en los juegos (ko12.9.23: 150). El tiempo hasta el tope, el mismo que a 150
uint32_t expForLevel(uint16_t lvl);      // EXP donde empieza ese nivel (1 -> 0)
uint16_t levelForExp(uint32_t exp);      // nivel que da esa EXP (1..LEVEL_MAX)
// EXP de derrotar/capturar a un rival: rendimiento de la especie x nivel / 4
uint32_t battleExp(int16_t foeDex, uint16_t foeLvl);
// ko9: tiempo de crianza que cuesta subir del nivel lvl al siguiente sin
// batallas: CARE_MIN_PER_LVL x nivel (ko10.2: 15 min; 1->2 un cuarto de hora, 2->3 media hora...)
#define CARE_MIN_PER_LVL 34UL  // ko12.10: minutos de crianza por nivel (ko10.2: 15; con tope 150): Lv1->100 ~ 117 dias, como antes a 150
// ko12.10: caracteristica como en los juegos: (2 x base + genes como IV 0..31 + entrenamiento como EV) x nivel / 100 + 5.
// tr = entrenamiento (0..100, como 0..252 EV)
uint16_t battleStat(uint8_t base, uint8_t gene, uint16_t lvl, uint8_t tr);
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
// ko12.10: los 251 de Pokemon Cristal, con el id de los juegos (PokeAPI): 33 placaje, 165 forcejeo
#define STATUS_CURE_PCT 16  // ko11.31: % por turno de que se pase el veneno / quemadura / paralisis
#define MOVE_TACKLE 33
#define MOVE_STRUGGLE 165
// banderas de MoveDef::flags
enum : uint8_t { MF_HICRIT = 1, MF_DRAIN = 2, MF_RECOIL = 4, MF_SELF = 8, MF_THAW = 16, MF_STATUS = 32,
                 MF_RECHARGE = 64, MF_NOMISS = 128 };
// ko12.10: efecto especial de cada movimiento (MoveDef::fx), lo hace battle.cpp
enum : uint8_t {
  MX_NONE = 0,
  // de ataque
  MX_OHKO, MX_SONICBOOM, MX_DRAGONRAGE, MX_LEVEL, MX_PSYWAVE, MX_SUPERFANG, MX_COUNTER, MX_MIRRORCOAT, MX_BIDE,
  MX_CHARGE, MX_SOLAR, MX_FLY, MX_DIG, MX_RAMPAGE, MX_TRAP, MX_CRASH, MX_SELFKO, MX_RAGE, MX_FURYCUTTER,
  MX_ROLLOUT, MX_FLAIL, MX_RETURN, MX_FRUSTRATION, MX_PRESENT, MX_MAGNITUDE, MX_HIDDENPOWER, MX_FALSESWIPE,
  MX_PURSUIT, MX_SNORE, MX_DREAMEATER, MX_TRIATTACK, MX_FUTURESIGHT, MX_THUNDER, MX_RAPIDSPIN, MX_ALLUP,
  MX_TRIPLEKICK, MX_BEATUP, MX_TWISTER, MX_QUAKE, MX_STRUGGLE, MX_PAYDAY, MX_THIEF, MX_STOMP,
  // de estado
  MX_SLEEPTALK, MX_TOXIC, MX_HEAL, MX_HEALWX, MX_REST, MX_LEECHSEED, MX_DISABLE, MX_MIST, MX_LIGHTSCREEN,
  MX_REFLECT, MX_HAZE, MX_FOCUS, MX_METRONOME, MX_MIRRORMOVE, MX_TRANSFORM, MX_SPLASH, MX_CONVERSION,
  MX_CONVERSION2, MX_SUBSTITUTE, MX_MIMIC, MX_MEANLOOK, MX_LOCKON, MX_NIGHTMARE, MX_CURSE, MX_SPITE,
  MX_PROTECT, MX_ENDURE, MX_BELLYDRUM, MX_SPIKES, MX_FORESIGHT, MX_DESTINYBOND, MX_PERISH, MX_SANDSTORM,
  MX_RAINDANCE, MX_SUNNYDAY, MX_SWAGGER, MX_ATTRACT, MX_HEALBELL, MX_SAFEGUARD, MX_PAINSPLIT, MX_BATONPASS,
  MX_ENCORE, MX_PSYCHUP, MX_ROAR, MX_TELEPORT,
  MX_COUNT
};
// estado alterado (ail): 1 veneno, 2 quemadura, 3 paralisis, 4 sueno, 5 congelado, 6 confusion
struct MoveDef {
  uint8_t type, pow, acc, pp;  // acc 0 = no falla (o es sobre si mismo)
  int8_t prio;
  uint8_t flags;
  uint8_t ail, ailCh;      // estado y su % (0 = siempre si es de estado)
  uint8_t st; int8_t stD;  // caracteristica (STG_*) y cambio
  uint8_t stSelf, stCh;    // 1 = a si mismo; % (0 = siempre si es de estado)
  uint8_t flinch, drain;   // % de retroceso; % drenado (MF_DRAIN) o de retroceso (MF_RECOIL)
  uint8_t st2; int8_t stD2;  // ko12.10: segundo cambio (desarrollo, maldicion...)
  uint8_t hits;            // golpes: max << 4 | min (0 = uno)
  uint8_t fx;              // MX_*
  uint8_t fam;             // dibujo de reserva (FX2_* + 1 de ui_moves.ino; 0 = el de su tipo)
};
#define MOVE_N 252          // ids 1..251 (0 = vacio); la tabla, en moves_data.h
const MoveDef &moveDef(uint8_t id);
const char *moveNameKo(uint8_t id);
const char *moveNameEn(uint8_t id);
const char *moveDescKo(uint8_t id);
const char *moveDescEn(uint8_t id);
// ko12.10: las 57 MT/MO de Cristal (MT01..50 = 0..49, MO01..07 = 50..56)
#define TM_COUNT 57
#define TM_HM_FIRST 50
uint8_t tmMove(uint8_t tm);
bool tmCompatible(int16_t dex, uint8_t tm);
const char *tmLabel(uint8_t tm, bool ko);   // "기술머신01" / "TM01"
int8_t tmOfMove(uint8_t id);                // -1 = no es de ninguna MT/MO
bool moveValid(uint8_t id);                         // 1..MOVE_N-1
bool moveIsStatus(uint8_t id);
uint8_t moveType(uint8_t id);
#define PP_MULT 2            // ko12.6: PP de la tabla x2
#define PP_LOSS_MIN_PCT 50   // ko12.6: al perder, los PP vuelven al menos a la mitad
uint8_t movePP(uint8_t id);                         // PP maximos
// ko12.10: lo que puede aprender en Cristal (subiendo de nivel, MT/MO, tutor y lo de sus preevoluciones)
bool moveCanLearn(int16_t dex, uint8_t id);
uint8_t movePool(int16_t dex, uint8_t *out, uint8_t max);
// ko12.10: lo que aprende subiendo de nivel (como en Cristal): lv[i], mv[i] por orden; devuelve cuantos
uint8_t moveLearnset(int16_t dex, uint8_t *lv, uint8_t *mv, uint8_t max);
// el que aprende justo a ese nivel (0 = ninguno; si hay varios, el k-esimo)
uint8_t moveAtLevel(int16_t dex, uint16_t lvl, uint8_t k = 0);
uint8_t moveNextAttack(int16_t dex, uint16_t lvl, uint8_t *atLv);  // ko12.10.15
// ko12.10: aprende a ese nivel si es "sin original" (10 niveles sin nada nuevo desde Lv.75): uno de los suyos
#define MOVE_FALLBACK_LV 75
#define MOVE_FALLBACK_GAP 10
// lo de antes (ko11.31..ko12.9) -> el original mas parecido que pueda aprender esa especie (0 = nada)
uint8_t moveMigrate(int16_t dex, uint8_t oldId);
// los 4 de un Pokemon que no es el que crias (rivales, ayudantes, historia): como en los juegos, los
// 4 ultimos que aprende subiendo de nivel hasta su nivel. Huecos = 0
void movesDefault(int16_t dex, uint16_t lvl, uint8_t out[4]);
uint8_t moveCount(const uint8_t mv[4]);
void movesFillPP(Battler &b);                       // PP llenos
bool movesHas(const uint8_t mv[4], uint8_t id);
// el que se ve en los dibujos de "su ataque" (el mas fuerte de su tipo; si no, el mas fuerte)
uint8_t movesMain(const Battler &b);
uint8_t movesMainOf(int16_t dex, const uint8_t mv[4]);
// accion -> casilla (0..3) o -1; BA_TYPE/BA_TACKLE buscan ese movimiento entre los suyos
int8_t battleSlot(const Battler &b, BAct a);
bool battleHasPP(const Battler &b);                 // false: solo puede forcejear
bool moveIsSpecial(uint8_t id);
bool moveDominated(const uint8_t mv[4], uint8_t id);  // ko12.8: otro suyo del mismo tipo lo deja sin sentido                    // ko12.8: usa at. esp. / def. esp.
uint32_t spStat(const Battler &b, uint32_t v, bool defense);  // ko12.8: v (atq o def) -> su especial
bool battleResting(const Battler &b, uint8_t id);   // ko12.8: ese movimiento descansa este turno (tras hiperrayo)
// dano esperado de un movimiento (para la IA y para marcar "muy eficaz" en el menu)
uint8_t moveEffAgainst(uint8_t id, const Battler &df);
uint8_t moveTypeFor(const Battler &at, uint8_t id);
// ko12.10: para el dibujo de reserva de un golpe (sin animacion en la SD): su tipo (0..15; volador -> 0xFF,
// el golpe generico), "fase" por la potencia (0 < 65 <= 1 < 100 <= 2) y variante. false = de estado
bool moveFxInfo(uint8_t id, uint8_t *type, uint8_t *tier, uint8_t *var);  // ko12.10: poder oculto: el tipo de ese Pokemon
uint16_t movePowerShown(const Battler &at, uint8_t id);  // ko12.10: la potencia que tiene ahora (0 = no hace dano normal)
void battleClearVolatile(Battler &b);  // al terminar: fuera estados y cambios
// ko12.10: cambio de Pokemon: el que se va pierde lo de la batalla (cambios, confusion, atrapado...) y el
// que entra hereda lo del lado (puas, reflejo, pantalla luz, neblina, velo sagrado). Las puas le
// hacen dano al empezar el turno siguiente
void battleSwitchIn(Battler &out, Battler &in);
bool battleCanEscape(const Battler &b);   // atrapado o mal de ojo: no puede huir
bool battleLocked(const Battler &b);      // sigue con el mismo movimiento (carga, enfado, rodar, venganza)
uint8_t battleLockedMove(const Battler &b);

#pragma once
#define GAME_SMALL_ENERGY 5  // ko10.3: juego de pelota sin record (con al menos 1 toque)
#include <Arduino.h>
#include <Preferences.h>
#include "battle.h"  // fork KO (ko7): curva de EXP y niveles de evolucion

// 1 tick = 1 minuto de juego. Baja este valor para probar mas rapido
// (p. ej. 5000UL = las estadisticas caen 12x mas rapido).
#define PET_TICK_MS 60000UL
// fork KO (ko7): el nivel ya no son horas de vida sino EXP (batallas + cada
// hora despierto y bien cuidado), tope 100. Ver expForLevel() en battle.h.
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
#define EVOLVE_ANIM_MS 5200UL              // animacion de evolucion (mas larga = mas epica)
#define CEREMONY_MS 10000UL                // duracion de la despedida en pantalla
#define FAREWELL_AGE_MIN (3UL * 24 * 60)   // se despide a los 3 dias de juego (en forma final)
#define RUNAWAY_TICKS 60                   // se escapa tras 1 h con TODO a cero

// milisegundos que faltan hasta `deadline` (0 si ya paso, o si deadline==0:
// "temporizador inactivo"). Todos los temporizadores del juego (dialogos,
// animaciones) se guardan como un instante absoluto "deadline = millis() +
// duracion" y se comparaban con millis() < deadline / millis() > deadline;
// eso se rompe cuando millis() da la vuelta a los ~49,7 dias de uptime. La
// resta sin signo, reinterpretada como con signo, es segura frente a esa
// vuelta mientras el intervalo real sea muy inferior a ~24,8 dias (aqui el
// temporizador mas largo son ~12 s), sin cambiar como se guarda el deadline.
static inline uint32_t timeLeft(uint32_t deadline) {
  if (!deadline) return 0;
  int32_t left = (int32_t)(deadline - millis());
  return left > 0 ? (uint32_t)left : 0;
}

// ceremonias de fin de ciclo
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };

// medallas del individuo (bitmask)
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
};
#define MED_COUNT 8

// datos de un Pokemon que viaja en un intercambio por ESP-NOW (empaquetado:
// es exactamente lo que va por la radio, ver link.h)
struct __attribute__((packed)) TradePet {
  int16_t dex;
  uint8_t shiny;
  uint32_t ageMinutes;
  uint8_t geneAtk, geneDef, geneSpe;
  uint8_t trAtk, trDef, trSpe;
  uint8_t weight;
  char nick[20];  // ko8: apodo en hangul (UTF-8, hasta 6 silabas); antes 12
};

// ko10: pokedex de gen 1 + gen 2 (dex.h: DEX_COUNT). Los bitmaps crecen de 19
// a 32 bytes; los guardados viejos (19) se leen igual (el resto queda a 0).
#define PET_DEX_MAX 251
#define PET_DEX_BYTES ((PET_DEX_MAX + 7) / 8)
// ko10.4: usos de los caramelos (de la familia del Pokemon que crias)
enum : uint8_t { CU_EXP = 0, CU_GAUGE, CU_GENES, CU_SHINY, CU_EVO, CU_COUNT };
static const uint8_t CANDY_COST[CU_COUNT] = { 3, 1, 5, 10, 5 };
#define CANDY_MAX 999
#define CANDY_KEEP 1   // quedarse el repetido (a la caja) da 1 caramelo
#define CANDY_GENE_MAX 115  // los genes nacen en 90-110; con caramelos hasta 115

// batallas: de donde viene el resultado
enum : uint8_t { BATTLE_WILD = 0, BATTLE_LINK };

class Pet {
public:
  // Estadisticas 0..100
  uint8_t fullness = 80;  // comida
  uint8_t joy = 80;       // felicidad
  uint8_t energy = 80;    // energia
  uint8_t hygiene = 100;  // limpieza
  uint8_t poops = 0;      // cacas en pantalla (max 3)
  uint8_t weight = 0;     // 0-100: las chuches engordan, el minijuego quema
  // genes (90-110%, se tiran al eclosionar) y entrenamiento (0-100)
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // ya descubrio su baya favorita
  bool shiny = false;       // variante de color rara (se sortea en el huevo)
  uint32_t ageMinutes = 0;     // edad (despedida); el nivel sale de exp
  uint32_t exp = 0;            // fork KO (ko7): experiencia (nivel = levelForExp)
  int16_t speciesId = -1;      // numero de Pokedex (1-251), -1 = huevo
  int16_t prevSpeciesId = -1;  // para la animacion de evolucion
  uint8_t careMistakes = 0;   // descuidos: cada uno retrasa la evolucion 1 nivel
  bool sleeping = false;
  uint32_t lastSeenEpoch = 0;   // ultima hora RTC vista (para progresion offline)
  uint8_t ceremony = CER_NONE;  // despedida/escapada/liberacion en curso
  uint8_t lastEnd = CER_NONE;   // como acabo la anterior (afecta al huevo)
  uint8_t dexReg[PET_DEX_BYTES] = { 0 };       // pokedex de criados (bitmap)
  uint8_t dexShinyReg[PET_DEX_BYTES] = { 0 };  // criados en version shiny
  // ko10.4: caramelos por familia (indice = DEX_FAM: el dex mas bajo de la linea).
  // Se ganan al capturar repetidos (o al soltar de la caja) y se gastan en el
  // Pokemon que crias si es de esa familia. Del jugador: persisten entre crianzas
  uint16_t candy[PET_DEX_MAX + 1] = { 0 };
  bool shinyCharm = false;  // el proximo huevo tiene 4 veces mas opciones de shiny
  // ko10.4: medallas de gimnasio (bit i = gimnasio i) y reto del dia (del jugador)
  uint8_t badges = 0;
  uint32_t dailyDoneDay = 0;   // dia (epoch/86400) del ultimo reto superado
  uint16_t dailyClears = 0;    // retos del dia superados en total
  // racha de cuidado diario (del jugador: persiste entre crianzas)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // vinculo (del bicho: sube lento con cuidado, se resetea al nacer otro)
  uint8_t bond = 0;
  char nick[20] = "";    // apodo (vacio = nombre de especie). ko8: admite hangul
  // medallas: del individuo + contador acumulado entre todas las crianzas
  uint16_t medals = 0, totalMedals = 0;
  uint16_t newMedal = 0;   // recien conseguida(s), para celebrar
  uint16_t lastMilestone = 0;  // hito de racha ya celebrado
  uint16_t gameHi = 0;     // record del minijuego (del jugador)
  uint16_t strHi = 0;      // record de golpes al saco
  // batallas e intercambios (del jugador: persisten entre crianzas)
  uint16_t wildWins = 0;   // victorias contra salvajes
  uint16_t linkWins = 0;   // victorias en tongsin (ESP-NOW)
  uint16_t linkBattles = 0;
  uint16_t trades = 0;     // intercambios completados
  // fork KO (ko4): objetos de batalla y records de los entrenamientos nuevos
  uint8_t balls = 5;       // pokeballs (se ganan 2 por victoria salvaje)
  uint8_t potions = 2;     // pociones: curan la mitad de la vida en batalla
  uint16_t defHi = 0;      // record del entrenamiento de defensa (pokeballs paradas)
  uint16_t speHi = 0;      // record del entrenamiento de velocidad (reflejos)

  void begin();                 // carga estado de NVS (o crea el primer huevo)
  void update(uint32_t nowMs);  // llamar en cada loop()

  // Acciones (botones tactiles)
  void feed();              // baya roja (compatibilidad)
  void feedBerry(uint8_t color);  // 0 roja, 1 azul, 2 verde
  void feedCandy();
  // fork KO (ko9): comida favorita entre las 4 del menu (0 roja, 1 azul,
  // 2 verde, 3 chuche). Sale de la forma base de la linea, asi no cambia al
  // evolucionar (antes era dex % 3: al evolucionar pasaba a otra baya).
  uint8_t favFood() const;
  bool lovesBerry(uint8_t item) const { return !isEgg() && favFood() == item; }
  // juego de pelota. ko9.2: SOLO si bate el record sube el animo y la energia
  // (devuelve true); si no, no pasa nada (ni premio ni cansancio)
  // ---- ko10.4: caramelos
  uint16_t candyOf(int16_t dex) const;          // de la familia de dex
  void addCandy(int16_t dex, uint16_t n);       // tope CANDY_MAX
  static uint16_t dupCandy(bool shiny, uint16_t lvl);  // por cambiar un repetido
  bool candyCanUse(uint8_t use) const;          // CU_*: hay caramelos y tiene efecto
  bool candyUse(uint8_t use);                   // gasta y aplica (false si no se puede)
  bool playResult(uint8_t score);  // true = record (animo + energia); si no, un poco de energia
  uint8_t trainStrength(uint16_t hits);  // saco de entrenamiento (entrena FUE)
  uint8_t trainDefense(uint16_t blocked);  // fork KO: pokeballs que caen (entrena DEF)
  uint8_t trainSpeed(uint16_t hits);       // fork KO: reflejos izq/dcha (entrena VEL)

  // stats de combate: base real de gen 1 x genes + nivel + entrenamiento
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  void play();

  // batallas (yasaeng / tongsin) e intercambio
  uint16_t hpStat() const;
  bool canBattle() const {  // despierto, nacido y sin nada en curso
    return !isEgg() && !sleeping && ceremony == CER_NONE && !evolving() && !starterPick;
  }
  bool tooTiredToBattle() const { return energy < 15; }
  // aplica premio/coste. caught: capturado con pokeball (premio de victoria sin
  // objetos). Perder o huir no cuesta nada (fork KO, ko4).
  // foeDex/foeLvl: el rival, para la EXP (fork KO, ko7)
  void battleResult(uint8_t kind, bool won, bool fled, bool caught = false,
                    int16_t foeDex = 0, uint16_t foeLvl = 0);
  // fork KO (ko7): suma EXP; devuelve cuantos niveles subio (suena al subir)
  uint16_t addExp(uint32_t x);
  // lo que dio la ultima batalla (pantalla de resultado)
  uint32_t lastExpGain = 0;
  uint16_t lastLvlUp = 0;
  // ko9: EXP por tiempo de crianza, repartida minuto a minuto. careAcc cuenta
  // en unidades de 1/(30*nivel) de EXP (sin decimales)
  uint32_t careAcc = 0;
  uint32_t careMinutesLeft() const;  // minutos de crianza que faltan para subir
  bool useBall();    // gasta una pokeball (false si no quedan)
  bool usePotion();  // gasta una pocion
  // fork KO (ko4): el siguiente a criar sale de la caja (tras la despedida)
  void adoptMon(int16_t dex, uint16_t lvl, bool shiny, uint8_t gA, uint8_t gD, uint8_t gS);
  // lo llama update() al acabar una DESPEDIDA; si devuelve false, huevo nuevo
  bool (*nextPetHook)(Pet &) = nullptr;
  void exportTrade(TradePet &t) const;
  bool importTrade(const TradePet &t, uint16_t lvl = 1);  // recibe el Pokemon del otro (evoluciona si toca)
  // evolucion por intercambio: a que especie (0 = ninguna). Gen 1: Kadabra,
  // Machoke, Graveler, Haunter. ko10 (gen 2): Onix, Scyther, Seadra, Porygon,
  // Poliwhirl (Politoed) y Slowpoke (Slowking)
  static int16_t tradeTarget(int16_t dex) {
    switch (dex) {
      case 64: return 65;   case 67: return 68;   case 75: return 76;   case 93: return 94;
      case 95: return 208;  case 123: return 212; case 117: return 230; case 137: return 233;
      case 61: return 186;  case 79: return 199;
      default: return 0;
    }
  }
  static bool tradeEvolves(int16_t dex) { return tradeTarget(dex) != 0; }
  void toggleLight();  // dormir / despertar
  void clean();
  void caress();  // tocar al bicho
  void eggTap();  // tocar el huevo: 3 toques y eclosiona
  void newEgg();   // empezar de cero con un inicial aleatorio
  void release();  // soltar (pulsacion larga + confirmar)
  void syncClock(uint32_t nowEpoch);  // aplica el tiempo transcurrido apagado
  void setClock(uint32_t nowEpoch);   // fija la hora sin aplicar progresion
  void startFarewell();  // tambien usable desde la consola serie (BYE)
  void startRunaway();   // tambien usable desde la consola serie (RUN)

  bool isEgg() const { return speciesId < 0; }
  uint8_t eggCracks() const { return eggTaps; }
  bool eating() const { return timeLeft(eatUntil) > 0; }
  bool showHeart() const { return timeLeft(heartUntil) > 0; }
  bool evolving() const { return timeLeft(evolveUntil) > 0; }
  float evolveT() const {     // progreso de la animacion de evolucion 0..1
    return 1.0f - (float)timeLeft(evolveUntil) / (float)EVOLVE_ANIM_MS;
  }
  bool canEvolveNow() const;  // condiciones de evolucion cumplidas (lista)
  void evolve();              // dispara la transformacion (la llama un toque del usuario)
  bool canFarewellNow() const;  // forma final + 7 dias: lista para despedirse (boton)
  bool canRunawayNow() const;   // abandono total 1h: lista para escaparse (boton triste)
  // el usuario decide en un dialogo; "mantener/quedaros" pospone y re-ofrece luego
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); }              // re-ofrece al subir de nivel
  void declineFarewell() { farDeclinedAge = ageMinutes + 1440; } // re-ofrece dentro de 1 dia
  // primera partida: el jugador elige inicial (Bulbasaur/Charmander/Squirtle)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex) { eggTarget = dex; starterPick = false; save(); }
  void factoryReset() { prefs.clear(); }  // borra la NVS (test: comando serie WIPE)
  // fork KO (ko8): [nuevo comienzo]. Borra la partida entera pero conserva los
  // ajustes que comparten este espacio de la NVS (sonido, volumenes, idioma) y
  // la ultima hora vista (resiembra del RTC). Al reiniciar, eleccion de inicial.
  void wipeGameKeepSettings();
  void dbgRunawayReady() { fullness = joy = energy = hygiene = 0; neglectTicks = RUNAWAY_TICKS; }  // test
  uint16_t level() const { return levelForExp(exp); }
  // nivel necesario para evolucionar (con el retraso de los descuidos; 0 = final)
  uint16_t evolveNeed() const;
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= PET_DEX_MAX && (dexReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  bool isShinyRegistered(int16_t dex) const {
    return dex >= 1 && dex <= PET_DEX_MAX && (dexShinyReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  uint16_t registeredCount() const;
  bool lineHasUnregistered(int16_t base) const;
  uint8_t eggRarity() const;       // rareza del huevo actual (sin revelar especie)
  int16_t pickEggSpecies();        // publica para poder simular tiradas (EGGS)
  uint8_t lowestStat() const { return min(min(fullness, joy), min(energy, hygiene)); }
  PetMood mood() const;
  // progreso de la ceremonia de despedida/escapada, 0..1 (para animarla)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    return 1.0f - (float)timeLeft(ceremonyUntil) / (float)CEREMONY_MS;
  }

  // racha / vinculo / medallas / nombre
  void rename(const char *name);
  bool hasMedal(uint16_t m) const { return medals & m; }
  bool showMedal() const { return timeLeft(medalUntil) > 0; }
  bool showMilestone() const { return timeLeft(milestoneUntil) > 0; }
  int careBonus() const;  // mejora del huevo por racha + vinculo

  // guardado periodico: tick() lo marca cada minuto de juego y el loop lo
  // vuelca en el acto (fork KO, ko4: antes esperaba a que la pantalla se
  // atenuara y un corte de luz perdia hasta varios minutos)
  bool savePending() const { return pendingSave; }
  void saveNow() { save(); }  // boton de encendido, apagado, etc.
  // ultima hora real persistida; sirve para resembrar un RTC que perdio la hora
  uint32_t savedEpoch() { return prefs.getUInt("seen", 0); }
  void flushSave();

private:
  Preferences prefs;
  uint32_t lastTick = 0;
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t evolveUntil = 0;
  int16_t eggTarget = 1;       // dex oculto que saldra del huevo
  bool eggShiny = false;       // sorpresa sorteada al crear el huevo
  uint8_t eggTaps = 0;
  uint8_t mistakeCooldown = 0;
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // guardado periodico pendiente de volcar
  uint16_t evoDeclinedLv = 0;   // "mantener forma": no ofrecer evolucion hasta subir de nivel
  uint32_t farDeclinedAge = 0;  // "quedaros juntos": no ofrecer despedida hasta esta edad
  bool starterPick = false;     // primera partida: esperando que el jugador elija inicial
  uint8_t neglectTicks = 0;
  uint16_t goodTicks = 0;  // racha bien cuidado: forja la DEF
  uint32_t ceremonyUntil = 0;
  uint8_t bondToday = 0;       // tope diario de subida de vinculo
  uint32_t medalUntil = 0;     // celebracion de medalla en pantalla
  uint32_t milestoneUntil = 0; // celebracion de hito de racha

  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  void registerCare();   // primer cuidado del dia: racha + vinculo
  void addBond(uint8_t amt);
  void checkMedals();
  void tick();
  void careTick();  // ko9
  void hatch();
  void registerSpecies(int16_t dex);
  void save();
  void load();
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
};

// story.h - ko11.21: modo historia (datos). El motor y las pantallas estan en ui_story.ino
//
// Cada capitulo es una lista de pasos que se ejecutan en orden:
//   ST_BG     a = region del fondo (0..15)
//   ST_SAY    who = quien habla (retrato STORY_WHO o W_PET = tu Pokemon), t = texto
//             ({1} = el nombre de tu Pokemon, con particulas coreanas {1}{이}, {1}{은}...)
//   ST_NARR   t = narracion (sin personaje)
//   ST_MON    c = Pokemon que aparece en la escena (0 = ninguno)
//   ST_CHOICE t = "pregunta|opcion A|opcion B[|opcion C]", a/b/c = etiqueta a la que salta cada opcion
//   ST_LABEL  a = etiqueta
//   ST_GOTO   a = etiqueta
//   ST_BATTLE a = equipo (STORY_TEAMS), who = retrato del rival
//   ST_GIVE   a = SG_* , b = cuantos, c = especie (caramelos)
//   ST_END    fin del capitulo (se marca como superado)
//   ST_STARTER c = especie del companero de la historia (el que lucha en este estilo)
//   ST_JOIN   c = un Pokemon que se une al equipo de la historia (como en el original; max STORY_JOIN_MAX)
//             a = 1: no evoluciona con el nivel (los del anime: el Bulbasaur de Ash sigue siendolo)
//   ST_LEAVE  c = (forma base) se va del equipo (la despedida de Butterfree)
//   ST_BATTLE b = 1: se sigue gane o pierda (la liga del anime)
//
// ko11.21: cada estilo tiene SU companero (juego: el inicial elegido con Oak, anime:
// Pikachu); los premios van al Pokemon que crias. CHOICE con 3 opciones: "p|A|B|C", c = 3a etiqueta
#pragma once
#include <stdint.h>

enum : uint8_t { ST_BG = 0, ST_SAY, ST_NARR, ST_MON, ST_CHOICE, ST_LABEL, ST_GOTO, ST_BATTLE, ST_GIVE, ST_END, ST_STARTER, ST_JOIN, ST_LEAVE };
enum : uint8_t { SG_BALL = 1, SG_POTION, SG_CANDY, SG_RARE, SG_EXP };

// retratos (orden de tools/pack_story.py -> /mons/story.bin, id 1..N)
enum : uint8_t {
  W_NONE = 0, W_OAK, W_RIVAL, W_RED, W_GRUNT, W_GIO, W_BROCK, W_MISTY, W_ASH, W_TR,
  W_SURGE, W_ERIKA, W_KOGA, W_SABRINA, W_BLAINE, W_BRUNO, W_LANCE,
  W_LORELEI, W_AGATHA, W_GARY, W_RITCHIE, W_LEAGUE,  // ko11.23
  W_COUNT,
  W_PET = 100,  // tu Pokemon (su sprite)
};

struct SStep {
  uint8_t op, who, a, b;
  int16_t c;
  const char *t;
};

// equipo de un combate de la historia: niveles fijos (como en el original)
// dex = -1: el inicial del rival, el que tiene ventaja sobre tu companero (y evoluciona con el nivel)
#define STORY_TEAM_MAX 4
#define STORY_JOIN_MAX 6  // ko11.23: los que se unen en la historia (hasta 6; a cada combate van 2 elegidos)
#define STORY_PICK 2      // companero + 2 = 3, como el equipo de combate
#define JOIN_KEEP 0x4000  // en stJ: no evoluciona
struct STeam {
  uint8_t n;
  int16_t dex[STORY_TEAM_MAX];
  int8_t lv[STORY_TEAM_MAX];
  uint8_t region;
};

// estilos: 0 = juego (Rojo/Azul), 1 = anime. El 2 (PokeRogue) es la expedicion, sin guion
#define STORY_STYLES 2
#define STORY_CH_MAX 15   // ko11.23: juego 14 capitulos, anime 15
#define STORY_PAGE 5      // capitulos por pagina en la lista
struct SChapter {
  const char *title;
  const SStep *steps;
  uint16_t n;
};
extern const SChapter STORY[STORY_STYLES][STORY_CH_MAX];
extern const uint8_t STORY_NCH[STORY_STYLES];  // capitulos de cada estilo
extern const STeam STORY_TEAMS[];
extern const uint8_t STORY_TEAM_COUNT;
extern const char *const STORY_WHO_NAME[W_COUNT];
extern const char *const STORY_STYLE_NAME[3];
extern const char *const STORY_STYLE_SUB[3];
// nivel minimo del companero en cada capitulo (para que la historia siempre se pueda seguir)
extern const uint8_t STORY_FLOOR[STORY_STYLES][STORY_CH_MAX];
extern const int16_t STORY_PARTNER0[STORY_STYLES];  // companero por defecto (antes de elegir)

// textos de las pantallas de la historia (solo coreano, como los guiones)
enum : uint8_t {
  SX_TITLE = 0, SX_DONE, SX_LOCKED, SX_RESUME, SX_PROG_FMT, SX_BEST_FMT, SX_VS_FMT, SX_WILD_GROUP,
  SX_TAP_BATTLE, SX_TIRED, SX_LOST, SX_CLEAR_FMT, SX_GOT_BALL, SX_GOT_POTION, SX_GOT_CANDY, SX_GOT_RARE,
  SX_GOT_EXP, SX_INTRO_FMT, SX_TAP_NEXT, SX_NEW, SX_JOIN_FMT, SX_RESTART, SX_RESTART_SURE, SX_RESTART_DONE,
  SX_LEAVE_FMT, SX_PICK, SX_GO, SX_COMPLETE, SX_PAGE_FMT, SX_QUIT_SURE, SX_QUIT_DONE,
  SX_TR_TITLE, SX_TR_SUB, SX_TR_NOTE, SX_TR_GO, SX_TR_LOCK, SX_TR_UP_FMT, SX_TR_EXP_FMT, SX_TR_LOST, SX_TR_QUIT,
  SX_TR_GAME, SX_TR_ANIME, SX_TR_ROGUE, SX_TR_PICK, SX_TR_MAX,  // ko11.28: 수련 전투
  SX_EVO_1, SX_EVO_2,  // ko11.30: 진화 장면
  SX_COUNT
};
extern const char *const STX[SX_COUNT];

// expedicion (estilo PokeRogue)
enum : uint8_t {
  RX_BEST_FMT = 0, RX_START, RX_CONT, RX_GIVEUP, RX_WAVE_FMT, RX_NEXT, RX_PICK, RX_HEAL, RX_BALL, RX_CANDY,
  RX_OVER, RX_RESULT_FMT, RX_NEWBEST, RX_TIRED, RX_WILD_FMT, RX_TRAINER, RX_BOSS_FMT, RX_INFO, RX_WON_FMT,
  RX_STARTER, RX_COUNT
};
extern const char *const RGX[RX_COUNT];

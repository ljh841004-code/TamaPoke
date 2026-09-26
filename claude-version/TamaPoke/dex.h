#pragma once
#include <stdint.h>
#include "i18n.h"  // gLang

// GENERADO por tools/gen_dex.py desde tools/dex_data.py - no editar

#define DEX_COUNT 251
#define DEX_BYTES 32  // bitmap de la pokedex
#define DEX_EEVEE 133

// rareza: 0 = solo por evolucion, 1 = comun, 2 = raro, 3 = legendario
enum : uint8_t { R_EVO = 0, R_COMUN, R_RARO, R_LEGENDARIO };

// tipo primario (gen 1-2) para las batallas
enum : uint8_t { PT_NORMAL, PT_FIRE, PT_WATER, PT_GRASS, PT_ELECTRIC, PT_ICE, PT_FIGHT, PT_POISON, PT_GROUND, PT_PSYCHIC, PT_BUG, PT_ROCK, PT_GHOST, PT_DRAGON, PT_DARK, PT_STEEL, PT_COUNT };

struct DexEntry {
  const char *name;
  uint8_t evolvesTo;    // numero de dex, 0 = forma final
  uint8_t evolveLevel;
  uint8_t rarity;       // sale de huevo si > 0
  uint16_t accent;      // color RGB565 del tipo para la UI
  uint8_t bHp, bAtk, bDef, bSpe;  // base stats reales de gen 1
  uint8_t biome;        // 0 pradera 1 playa 2 bosque 3 volcan 4 montana 5 nieve
  uint8_t ptype;        // tipo primario PT_* (batallas)
};

static const DexEntry DEX_TBL[DEX_COUNT + 1] = {
  { "?", 0, 0, 0, 0x2946, 50, 50, 50, 50, 0, PT_NORMAL },  // 0: sin usar
  { "BULBASAUR", 2, 16, R_COMUN, 0x3C49, 45, 49, 49, 45, 2, PT_GRASS },  // 1 planta
  { "IVYSAUR", 3, 32, R_EVO, 0x3C49, 60, 62, 63, 60, 2, PT_GRASS },  // 2 planta
  { "VENUSAUR", 0, 0, R_EVO, 0x3C49, 80, 82, 83, 80, 2, PT_GRASS },  // 3 planta
  { "CHARMANDER", 5, 16, R_COMUN, 0xEA87, 39, 52, 43, 65, 3, PT_FIRE },  // 4 fuego
  { "CHARMELEON", 6, 36, R_EVO, 0xEA87, 58, 64, 58, 80, 3, PT_FIRE },  // 5 fuego
  { "CHARIZARD", 0, 0, R_EVO, 0xEA87, 78, 84, 78, 100, 3, PT_FIRE },  // 6 fuego
  { "SQUIRTLE", 8, 16, R_COMUN, 0x4C98, 44, 48, 65, 43, 1, PT_WATER },  // 7 agua
  { "WARTORTLE", 9, 36, R_EVO, 0x4C98, 59, 63, 80, 58, 1, PT_WATER },  // 8 agua
  { "BLASTOISE", 0, 0, R_EVO, 0x4C98, 79, 83, 100, 78, 1, PT_WATER },  // 9 agua
  { "CATERPIE", 11, 7, R_COMUN, 0x7CC4, 45, 30, 35, 45, 2, PT_BUG },  // 10 bicho
  { "METAPOD", 12, 10, R_EVO, 0x7CC4, 50, 20, 55, 30, 2, PT_BUG },  // 11 bicho
  { "BUTTERFREE", 0, 0, R_EVO, 0x7CC4, 60, 45, 50, 70, 2, PT_BUG },  // 12 bicho
  { "WEEDLE", 14, 7, R_COMUN, 0x7CC4, 40, 35, 30, 50, 2, PT_BUG },  // 13 bicho
  { "KAKUNA", 15, 10, R_EVO, 0x7CC4, 45, 25, 50, 35, 2, PT_BUG },  // 14 bicho
  { "BEEDRILL", 0, 0, R_EVO, 0x7CC4, 65, 90, 40, 75, 2, PT_BUG },  // 15 bicho
  { "PIDGEY", 17, 18, R_COMUN, 0x8C4D, 40, 45, 40, 56, 0, PT_NORMAL },  // 16 normal
  { "PIDGEOTTO", 18, 36, R_EVO, 0x8C4D, 63, 60, 55, 71, 0, PT_NORMAL },  // 17 normal
  { "PIDGEOT", 0, 0, R_EVO, 0x8C4D, 83, 80, 75, 101, 0, PT_NORMAL },  // 18 normal
  { "RATTATA", 20, 20, R_COMUN, 0x8C4D, 30, 56, 35, 72, 0, PT_NORMAL },  // 19 normal
  { "RATICATE", 0, 0, R_EVO, 0x8C4D, 55, 81, 60, 97, 0, PT_NORMAL },  // 20 normal
  { "SPEAROW", 22, 20, R_COMUN, 0x8C4D, 40, 60, 30, 70, 0, PT_NORMAL },  // 21 normal
  { "FEAROW", 0, 0, R_EVO, 0x8C4D, 65, 90, 65, 100, 0, PT_NORMAL },  // 22 normal
  { "EKANS", 24, 22, R_COMUN, 0x8A73, 35, 60, 44, 55, 0, PT_POISON },  // 23 veneno
  { "ARBOK", 0, 0, R_EVO, 0x8A73, 60, 95, 69, 80, 0, PT_POISON },  // 24 veneno
  { "PIKACHU", 26, 30, R_COMUN, 0xBCA1, 35, 55, 40, 90, 0, PT_ELECTRIC },  // 25 electrico
  { "RAICHU", 0, 0, R_EVO, 0xBCA1, 60, 90, 55, 110, 0, PT_ELECTRIC },  // 26 electrico
  { "SANDSHREW", 28, 22, R_COMUN, 0xB447, 50, 75, 85, 40, 4, PT_GROUND },  // 27 tierra
  { "SANDSLASH", 0, 0, R_EVO, 0xB447, 75, 100, 110, 65, 4, PT_GROUND },  // 28 tierra
  { "NIDORAN H", 30, 16, R_COMUN, 0x8A73, 55, 47, 52, 41, 0, PT_POISON },  // 29 veneno
  { "NIDORINA", 31, 30, R_EVO, 0x8A73, 70, 62, 67, 56, 0, PT_POISON },  // 30 veneno
  { "NIDOQUEEN", 0, 0, R_EVO, 0x8A73, 90, 92, 87, 76, 0, PT_POISON },  // 31 veneno
  { "NIDORAN M", 33, 16, R_COMUN, 0x8A73, 46, 57, 40, 50, 0, PT_POISON },  // 32 veneno
  { "NIDORINO", 34, 30, R_EVO, 0x8A73, 61, 72, 57, 65, 0, PT_POISON },  // 33 veneno
  { "NIDOKING", 0, 0, R_EVO, 0x8A73, 81, 102, 77, 85, 0, PT_POISON },  // 34 veneno
  { "CLEFAIRY", 36, 30, R_COMUN, 0x8C4D, 70, 45, 48, 35, 0, PT_NORMAL },  // 35 normal
  { "CLEFABLE", 0, 0, R_EVO, 0x8C4D, 95, 70, 73, 60, 0, PT_NORMAL },  // 36 normal
  { "VULPIX", 38, 30, R_COMUN, 0xEA87, 38, 41, 40, 65, 3, PT_FIRE },  // 37 fuego
  { "NINETALES", 0, 0, R_EVO, 0xEA87, 73, 76, 75, 100, 3, PT_FIRE },  // 38 fuego
  { "JIGGLYPUFF", 40, 30, R_COMUN, 0x8C4D, 115, 45, 20, 20, 0, PT_NORMAL },  // 39 normal
  { "WIGGLYTUFF", 0, 0, R_EVO, 0x8C4D, 140, 70, 45, 45, 0, PT_NORMAL },  // 40 normal
  { "ZUBAT", 42, 22, R_COMUN, 0x8A73, 40, 45, 35, 55, 0, PT_POISON },  // 41 veneno
  { "GOLBAT", 169, 36, R_EVO, 0x8A73, 75, 80, 70, 90, 0, PT_POISON },  // 42 veneno
  { "ODDISH", 44, 21, R_COMUN, 0x3C49, 45, 50, 55, 30, 2, PT_GRASS },  // 43 planta
  { "GLOOM", 45, 36, R_EVO, 0x3C49, 60, 65, 70, 40, 2, PT_GRASS },  // 44 planta
  { "VILEPLUME", 0, 0, R_EVO, 0x3C49, 75, 80, 85, 50, 2, PT_GRASS },  // 45 planta
  { "PARAS", 47, 24, R_COMUN, 0x7CC4, 35, 70, 55, 25, 2, PT_BUG },  // 46 bicho
  { "PARASECT", 0, 0, R_EVO, 0x7CC4, 60, 95, 80, 30, 2, PT_BUG },  // 47 bicho
  { "VENONAT", 49, 31, R_COMUN, 0x7CC4, 60, 55, 50, 45, 2, PT_BUG },  // 48 bicho
  { "VENOMOTH", 0, 0, R_EVO, 0x7CC4, 70, 65, 60, 90, 2, PT_BUG },  // 49 bicho
  { "DIGLETT", 51, 26, R_COMUN, 0xB447, 10, 55, 25, 95, 4, PT_GROUND },  // 50 tierra
  { "DUGTRIO", 0, 0, R_EVO, 0xB447, 35, 100, 50, 120, 4, PT_GROUND },  // 51 tierra
  { "MEOWTH", 53, 28, R_COMUN, 0x8C4D, 40, 45, 35, 90, 0, PT_NORMAL },  // 52 normal
  { "PERSIAN", 0, 0, R_EVO, 0x8C4D, 65, 70, 60, 115, 0, PT_NORMAL },  // 53 normal
  { "PSYDUCK", 55, 33, R_COMUN, 0x4C98, 50, 52, 48, 55, 1, PT_WATER },  // 54 agua
  { "GOLDUCK", 0, 0, R_EVO, 0x4C98, 80, 82, 78, 85, 1, PT_WATER },  // 55 agua
  { "MANKEY", 57, 28, R_COMUN, 0xA2A5, 40, 80, 35, 70, 0, PT_FIGHT },  // 56 lucha
  { "PRIMEAPE", 0, 0, R_EVO, 0xA2A5, 65, 105, 60, 95, 0, PT_FIGHT },  // 57 lucha
  { "GROWLITHE", 59, 30, R_RARO, 0xEA87, 55, 70, 45, 60, 3, PT_FIRE },  // 58 fuego
  { "ARCANINE", 0, 0, R_EVO, 0xEA87, 90, 110, 80, 95, 3, PT_FIRE },  // 59 fuego
  { "POLIWAG", 61, 25, R_COMUN, 0x4C98, 40, 50, 40, 90, 1, PT_WATER },  // 60 agua
  { "POLIWHIRL", 62, 36, R_EVO, 0x4C98, 65, 65, 65, 90, 1, PT_WATER },  // 61 agua
  { "POLIWRATH", 0, 0, R_EVO, 0x4C98, 90, 95, 95, 70, 1, PT_WATER },  // 62 agua
  { "ABRA", 64, 16, R_COMUN, 0xD28F, 25, 20, 15, 90, 0, PT_PSYCHIC },  // 63 psiquico
  { "KADABRA", 65, 40, R_EVO, 0xD28F, 40, 35, 30, 105, 0, PT_PSYCHIC },  // 64 psiquico
  { "ALAKAZAM", 0, 0, R_EVO, 0xD28F, 55, 50, 45, 120, 0, PT_PSYCHIC },  // 65 psiquico
  { "MACHOP", 67, 28, R_COMUN, 0xA2A5, 70, 80, 50, 35, 0, PT_FIGHT },  // 66 lucha
  { "MACHOKE", 68, 40, R_EVO, 0xA2A5, 80, 100, 70, 45, 0, PT_FIGHT },  // 67 lucha
  { "MACHAMP", 0, 0, R_EVO, 0xA2A5, 90, 130, 80, 55, 0, PT_FIGHT },  // 68 lucha
  { "BELLSPROUT", 70, 21, R_COMUN, 0x3C49, 50, 75, 35, 40, 2, PT_GRASS },  // 69 planta
  { "WEEPINBELL", 71, 36, R_EVO, 0x3C49, 65, 90, 50, 55, 2, PT_GRASS },  // 70 planta
  { "VICTREEBEL", 0, 0, R_EVO, 0x3C49, 80, 105, 65, 70, 2, PT_GRASS },  // 71 planta
  { "TENTACOOL", 73, 30, R_COMUN, 0x4C98, 40, 40, 35, 70, 1, PT_WATER },  // 72 agua
  { "TENTACRUEL", 0, 0, R_EVO, 0x4C98, 80, 70, 65, 100, 1, PT_WATER },  // 73 agua
  { "GEODUDE", 75, 25, R_COMUN, 0x9407, 40, 80, 100, 20, 4, PT_ROCK },  // 74 roca
  { "GRAVELER", 76, 40, R_EVO, 0x9407, 55, 95, 115, 35, 4, PT_ROCK },  // 75 roca
  { "GOLEM", 0, 0, R_EVO, 0x9407, 80, 120, 130, 45, 4, PT_ROCK },  // 76 roca
  { "PONYTA", 78, 40, R_RARO, 0xEA87, 50, 85, 55, 90, 3, PT_FIRE },  // 77 fuego
  { "RAPIDASH", 0, 0, R_EVO, 0xEA87, 65, 100, 70, 105, 3, PT_FIRE },  // 78 fuego
  { "SLOWPOKE", 80, 37, R_COMUN, 0x4C98, 90, 65, 65, 15, 1, PT_WATER },  // 79 agua
  { "SLOWBRO", 0, 0, R_EVO, 0x4C98, 95, 75, 110, 30, 1, PT_WATER },  // 80 agua
  { "MAGNEMITE", 82, 30, R_COMUN, 0xBCA1, 25, 35, 70, 45, 0, PT_ELECTRIC },  // 81 electrico
  { "MAGNETON", 0, 0, R_EVO, 0xBCA1, 50, 60, 95, 70, 0, PT_ELECTRIC },  // 82 electrico
  { "FARFETCHD", 0, 0, R_RARO, 0x8C4D, 52, 90, 55, 60, 0, PT_NORMAL },  // 83 normal
  { "DODUO", 85, 31, R_COMUN, 0x8C4D, 35, 85, 45, 75, 0, PT_NORMAL },  // 84 normal
  { "DODRIO", 0, 0, R_EVO, 0x8C4D, 60, 110, 70, 110, 0, PT_NORMAL },  // 85 normal
  { "SEEL", 87, 34, R_COMUN, 0x4C98, 65, 45, 55, 45, 1, PT_WATER },  // 86 agua
  { "DEWGONG", 0, 0, R_EVO, 0x4C98, 90, 70, 80, 70, 1, PT_WATER },  // 87 agua
  { "GRIMER", 89, 38, R_RARO, 0x8A73, 80, 80, 50, 25, 0, PT_POISON },  // 88 veneno
  { "MUK", 0, 0, R_EVO, 0x8A73, 105, 105, 75, 50, 0, PT_POISON },  // 89 veneno
  { "SHELLDER", 91, 30, R_COMUN, 0x4C98, 30, 65, 100, 40, 1, PT_WATER },  // 90 agua
  { "CLOYSTER", 0, 0, R_EVO, 0x4C98, 50, 95, 180, 70, 1, PT_WATER },  // 91 agua
  { "GASTLY", 93, 25, R_COMUN, 0x6AD3, 30, 35, 30, 80, 0, PT_GHOST },  // 92 fantasma
  { "HAUNTER", 94, 40, R_EVO, 0x6AD3, 45, 50, 45, 95, 0, PT_GHOST },  // 93 fantasma
  { "GENGAR", 0, 0, R_EVO, 0x6AD3, 60, 65, 60, 110, 0, PT_GHOST },  // 94 fantasma
  { "ONIX", 208, 40, R_RARO, 0x9407, 35, 45, 160, 70, 4, PT_ROCK },  // 95 roca
  { "DROWZEE", 97, 26, R_COMUN, 0xD28F, 60, 48, 45, 42, 0, PT_PSYCHIC },  // 96 psiquico
  { "HYPNO", 0, 0, R_EVO, 0xD28F, 85, 73, 70, 67, 0, PT_PSYCHIC },  // 97 psiquico
  { "KRABBY", 99, 28, R_COMUN, 0x4C98, 30, 105, 90, 50, 1, PT_WATER },  // 98 agua
  { "KINGLER", 0, 0, R_EVO, 0x4C98, 55, 130, 115, 75, 1, PT_WATER },  // 99 agua
  { "VOLTORB", 101, 30, R_COMUN, 0xBCA1, 40, 30, 50, 100, 0, PT_ELECTRIC },  // 100 electrico
  { "ELECTRODE", 0, 0, R_EVO, 0xBCA1, 60, 50, 70, 150, 0, PT_ELECTRIC },  // 101 electrico
  { "EXEGGCUTE", 103, 30, R_COMUN, 0x3C49, 60, 40, 80, 40, 2, PT_GRASS },  // 102 planta
  { "EXEGGUTOR", 0, 0, R_EVO, 0x3C49, 95, 95, 85, 55, 2, PT_GRASS },  // 103 planta
  { "CUBONE", 105, 28, R_COMUN, 0xB447, 50, 50, 95, 35, 4, PT_GROUND },  // 104 tierra
  { "MAROWAK", 0, 0, R_EVO, 0xB447, 60, 80, 110, 45, 4, PT_GROUND },  // 105 tierra
  { "HITMONLEE", 0, 0, R_RARO, 0xA2A5, 50, 120, 53, 87, 0, PT_FIGHT },  // 106 lucha
  { "HITMONCHAN", 0, 0, R_RARO, 0xA2A5, 50, 105, 79, 76, 0, PT_FIGHT },  // 107 lucha
  { "LICKITUNG", 0, 0, R_RARO, 0x8C4D, 90, 55, 75, 30, 0, PT_NORMAL },  // 108 normal
  { "KOFFING", 110, 35, R_COMUN, 0x8A73, 40, 65, 95, 35, 0, PT_POISON },  // 109 veneno
  { "WEEZING", 0, 0, R_EVO, 0x8A73, 65, 90, 120, 60, 0, PT_POISON },  // 110 veneno
  { "RHYHORN", 112, 42, R_RARO, 0xB447, 80, 85, 95, 25, 4, PT_GROUND },  // 111 tierra
  { "RHYDON", 0, 0, R_EVO, 0xB447, 105, 130, 120, 40, 4, PT_GROUND },  // 112 tierra
  { "CHANSEY", 242, 40, R_RARO, 0x8C4D, 250, 5, 5, 50, 0, PT_NORMAL },  // 113 normal
  { "TANGELA", 0, 0, R_RARO, 0x3C49, 65, 55, 115, 60, 2, PT_GRASS },  // 114 planta
  { "KANGASKHAN", 0, 0, R_RARO, 0x8C4D, 105, 95, 80, 90, 0, PT_NORMAL },  // 115 normal
  { "HORSEA", 117, 32, R_COMUN, 0x4C98, 30, 40, 70, 60, 1, PT_WATER },  // 116 agua
  { "SEADRA", 230, 40, R_EVO, 0x4C98, 55, 65, 95, 85, 1, PT_WATER },  // 117 agua
  { "GOLDEEN", 119, 33, R_COMUN, 0x4C98, 45, 67, 60, 63, 1, PT_WATER },  // 118 agua
  { "SEAKING", 0, 0, R_EVO, 0x4C98, 80, 92, 65, 68, 1, PT_WATER },  // 119 agua
  { "STARYU", 121, 30, R_COMUN, 0x4C98, 30, 45, 55, 85, 1, PT_WATER },  // 120 agua
  { "STARMIE", 0, 0, R_EVO, 0x4C98, 60, 75, 85, 115, 1, PT_WATER },  // 121 agua
  { "MR. MIME", 0, 0, R_RARO, 0xD28F, 40, 45, 65, 90, 0, PT_PSYCHIC },  // 122 psiquico
  { "SCYTHER", 212, 40, R_RARO, 0x7CC4, 70, 110, 80, 105, 2, PT_BUG },  // 123 bicho
  { "JYNX", 0, 0, R_RARO, 0x4DB8, 65, 50, 35, 95, 5, PT_ICE },  // 124 hielo
  { "ELECTABUZZ", 0, 0, R_RARO, 0xBCA1, 65, 83, 57, 105, 0, PT_ELECTRIC },  // 125 electrico
  { "MAGMAR", 0, 0, R_RARO, 0xEA87, 65, 95, 57, 93, 3, PT_FIRE },  // 126 fuego
  { "PINSIR", 0, 0, R_RARO, 0x7CC4, 65, 125, 100, 85, 2, PT_BUG },  // 127 bicho
  { "TAUROS", 0, 0, R_RARO, 0x8C4D, 75, 100, 95, 110, 0, PT_NORMAL },  // 128 normal
  { "MAGIKARP", 130, 20, R_COMUN, 0x4C98, 20, 10, 55, 80, 1, PT_WATER },  // 129 agua
  { "GYARADOS", 0, 0, R_EVO, 0x4C98, 95, 125, 79, 81, 1, PT_WATER },  // 130 agua
  { "LAPRAS", 0, 0, R_RARO, 0x4C98, 130, 85, 80, 60, 1, PT_WATER },  // 131 agua
  { "DITTO", 0, 0, R_RARO, 0x8C4D, 48, 48, 48, 48, 0, PT_NORMAL },  // 132 normal
  { "EEVEE", 134, 30, R_COMUN, 0x8C4D, 55, 55, 50, 55, 0, PT_NORMAL },  // 133 normal
  { "VAPOREON", 0, 0, R_EVO, 0x4C98, 130, 65, 60, 65, 1, PT_WATER },  // 134 agua
  { "JOLTEON", 0, 0, R_EVO, 0xBCA1, 65, 65, 60, 130, 0, PT_ELECTRIC },  // 135 electrico
  { "FLAREON", 0, 0, R_EVO, 0xEA87, 65, 130, 60, 65, 3, PT_FIRE },  // 136 fuego
  { "PORYGON", 233, 40, R_RARO, 0x8C4D, 65, 60, 70, 40, 0, PT_NORMAL },  // 137 normal
  { "OMANYTE", 139, 40, R_RARO, 0x9407, 35, 40, 100, 35, 1, PT_ROCK },  // 138 roca
  { "OMASTAR", 0, 0, R_EVO, 0x9407, 70, 60, 125, 55, 1, PT_ROCK },  // 139 roca
  { "KABUTO", 141, 40, R_RARO, 0x9407, 30, 80, 90, 55, 1, PT_ROCK },  // 140 roca
  { "KABUTOPS", 0, 0, R_EVO, 0x9407, 60, 115, 105, 80, 1, PT_ROCK },  // 141 roca
  { "AERODACTYL", 0, 0, R_RARO, 0x9407, 80, 105, 65, 130, 4, PT_ROCK },  // 142 roca
  { "SNORLAX", 0, 0, R_RARO, 0x8C4D, 160, 110, 65, 30, 0, PT_NORMAL },  // 143 normal
  { "ARTICUNO", 0, 0, R_LEGENDARIO, 0x4DB8, 90, 85, 100, 85, 5, PT_ICE },  // 144 hielo
  { "ZAPDOS", 0, 0, R_LEGENDARIO, 0xBCA1, 90, 90, 85, 100, 0, PT_ELECTRIC },  // 145 electrico
  { "MOLTRES", 0, 0, R_LEGENDARIO, 0xEA87, 90, 100, 90, 90, 3, PT_FIRE },  // 146 fuego
  { "DRATINI", 148, 30, R_RARO, 0x5A98, 41, 64, 45, 50, 1, PT_DRAGON },  // 147 dragon
  { "DRAGONAIR", 149, 55, R_EVO, 0x5A98, 61, 84, 65, 70, 1, PT_DRAGON },  // 148 dragon
  { "DRAGONITE", 0, 0, R_EVO, 0x5A98, 91, 134, 95, 80, 1, PT_DRAGON },  // 149 dragon
  { "MEWTWO", 0, 0, R_LEGENDARIO, 0xD28F, 106, 110, 90, 130, 0, PT_PSYCHIC },  // 150 psiquico
  { "MEW", 0, 0, R_LEGENDARIO, 0xD28F, 100, 100, 100, 100, 0, PT_PSYCHIC },  // 151 psiquico
  { "CHIKORITA", 153, 16, R_COMUN, 0x3C49, 45, 49, 65, 45, 2, PT_GRASS },  // 152 planta
  { "BAYLEEF", 154, 32, R_EVO, 0x3C49, 60, 62, 80, 60, 2, PT_GRASS },  // 153 planta
  { "MEGANIUM", 0, 0, R_EVO, 0x3C49, 80, 82, 100, 80, 2, PT_GRASS },  // 154 planta
  { "CYNDAQUIL", 156, 14, R_COMUN, 0xEA87, 39, 52, 43, 65, 3, PT_FIRE },  // 155 fuego
  { "QUILAVA", 157, 36, R_EVO, 0xEA87, 58, 64, 58, 80, 3, PT_FIRE },  // 156 fuego
  { "TYPHLOSION", 0, 0, R_EVO, 0xEA87, 78, 84, 78, 100, 3, PT_FIRE },  // 157 fuego
  { "TOTODILE", 159, 18, R_COMUN, 0x4C98, 50, 65, 64, 43, 1, PT_WATER },  // 158 agua
  { "CROCONAW", 160, 30, R_EVO, 0x4C98, 65, 80, 80, 58, 1, PT_WATER },  // 159 agua
  { "FERALIGATR", 0, 0, R_EVO, 0x4C98, 85, 105, 100, 78, 1, PT_WATER },  // 160 agua
  { "SENTRET", 162, 15, R_COMUN, 0x8C4D, 35, 46, 34, 20, 0, PT_NORMAL },  // 161 normal
  { "FURRET", 0, 0, R_EVO, 0x8C4D, 85, 76, 64, 90, 0, PT_NORMAL },  // 162 normal
  { "HOOTHOOT", 164, 20, R_COMUN, 0x8C4D, 60, 30, 30, 50, 0, PT_NORMAL },  // 163 normal
  { "NOCTOWL", 0, 0, R_EVO, 0x8C4D, 100, 50, 50, 70, 0, PT_NORMAL },  // 164 normal
  { "LEDYBA", 166, 18, R_COMUN, 0x7CC4, 40, 20, 30, 55, 2, PT_BUG },  // 165 bicho
  { "LEDIAN", 0, 0, R_EVO, 0x7CC4, 55, 35, 50, 85, 2, PT_BUG },  // 166 bicho
  { "SPINARAK", 168, 22, R_COMUN, 0x7CC4, 40, 60, 40, 30, 2, PT_BUG },  // 167 bicho
  { "ARIADOS", 0, 0, R_EVO, 0x7CC4, 70, 90, 70, 40, 2, PT_BUG },  // 168 bicho
  { "CROBAT", 0, 0, R_EVO, 0x8A73, 85, 90, 80, 130, 0, PT_POISON },  // 169 veneno
  { "CHINCHOU", 171, 27, R_COMUN, 0x4C98, 75, 38, 38, 67, 1, PT_WATER },  // 170 agua
  { "LANTURN", 0, 0, R_EVO, 0x4C98, 125, 58, 58, 67, 1, PT_WATER },  // 171 agua
  { "PICHU", 25, 25, R_COMUN, 0xBCA1, 20, 40, 15, 60, 0, PT_ELECTRIC },  // 172 electrico
  { "CLEFFA", 35, 25, R_COMUN, 0x8C4D, 50, 25, 28, 15, 0, PT_NORMAL },  // 173 normal
  { "IGGLYBUFF", 39, 25, R_COMUN, 0x8C4D, 90, 30, 15, 15, 0, PT_NORMAL },  // 174 normal
  { "TOGEPI", 176, 25, R_COMUN, 0x8C4D, 35, 20, 65, 20, 0, PT_NORMAL },  // 175 normal
  { "TOGETIC", 0, 0, R_EVO, 0x8C4D, 55, 40, 85, 40, 0, PT_NORMAL },  // 176 normal
  { "NATU", 178, 25, R_COMUN, 0xD28F, 40, 50, 45, 70, 0, PT_PSYCHIC },  // 177 psiquico
  { "XATU", 0, 0, R_EVO, 0xD28F, 65, 75, 70, 95, 0, PT_PSYCHIC },  // 178 psiquico
  { "MAREEP", 180, 15, R_COMUN, 0xBCA1, 55, 40, 40, 35, 0, PT_ELECTRIC },  // 179 electrico
  { "FLAAFFY", 181, 30, R_EVO, 0xBCA1, 70, 55, 55, 45, 0, PT_ELECTRIC },  // 180 electrico
  { "AMPHAROS", 0, 0, R_EVO, 0xBCA1, 90, 75, 85, 55, 0, PT_ELECTRIC },  // 181 electrico
  { "BELLOSSOM", 0, 0, R_EVO, 0x3C49, 75, 80, 95, 50, 2, PT_GRASS },  // 182 planta
  { "MARILL", 184, 18, R_COMUN, 0x4C98, 70, 20, 50, 40, 1, PT_WATER },  // 183 agua
  { "AZUMARILL", 0, 0, R_EVO, 0x4C98, 100, 50, 80, 50, 1, PT_WATER },  // 184 agua
  { "SUDOWOODO", 0, 0, R_RARO, 0x9407, 70, 100, 115, 30, 4, PT_ROCK },  // 185 roca
  { "POLITOED", 0, 0, R_EVO, 0x4C98, 90, 75, 75, 70, 1, PT_WATER },  // 186 agua
  { "HOPPIP", 188, 18, R_COMUN, 0x3C49, 35, 35, 40, 50, 2, PT_GRASS },  // 187 planta
  { "SKIPLOOM", 189, 27, R_EVO, 0x3C49, 55, 45, 50, 80, 2, PT_GRASS },  // 188 planta
  { "JUMPLUFF", 0, 0, R_EVO, 0x3C49, 75, 55, 70, 110, 2, PT_GRASS },  // 189 planta
  { "AIPOM", 0, 0, R_COMUN, 0x8C4D, 55, 70, 55, 85, 0, PT_NORMAL },  // 190 normal
  { "SUNKERN", 192, 30, R_COMUN, 0x3C49, 30, 30, 30, 30, 2, PT_GRASS },  // 191 planta
  { "SUNFLORA", 0, 0, R_EVO, 0x3C49, 75, 75, 55, 30, 2, PT_GRASS },  // 192 planta
  { "YANMA", 0, 0, R_COMUN, 0x7CC4, 65, 65, 45, 95, 2, PT_BUG },  // 193 bicho
  { "WOOPER", 195, 20, R_COMUN, 0x4C98, 55, 45, 45, 15, 1, PT_WATER },  // 194 agua
  { "QUAGSIRE", 0, 0, R_EVO, 0x4C98, 95, 85, 85, 35, 1, PT_WATER },  // 195 agua
  { "ESPEON", 0, 0, R_EVO, 0xD28F, 65, 65, 60, 110, 0, PT_PSYCHIC },  // 196 psiquico
  { "UMBREON", 0, 0, R_EVO, 0x5A48, 95, 65, 110, 65, 0, PT_DARK },  // 197 siniestro
  { "MURKROW", 0, 0, R_RARO, 0x5A48, 60, 85, 42, 91, 0, PT_DARK },  // 198 siniestro
  { "SLOWKING", 0, 0, R_EVO, 0x4C98, 95, 75, 80, 30, 1, PT_WATER },  // 199 agua
  { "MISDREAVUS", 0, 0, R_RARO, 0x6AD3, 60, 60, 60, 85, 0, PT_GHOST },  // 200 fantasma
  { "UNOWN", 0, 0, R_RARO, 0xD28F, 48, 72, 48, 48, 0, PT_PSYCHIC },  // 201 psiquico
  { "WOBBUFFET", 0, 0, R_RARO, 0xD28F, 190, 33, 58, 33, 0, PT_PSYCHIC },  // 202 psiquico
  { "GIRAFARIG", 0, 0, R_RARO, 0x8C4D, 70, 80, 65, 85, 0, PT_NORMAL },  // 203 normal
  { "PINECO", 205, 31, R_COMUN, 0x7CC4, 50, 65, 90, 15, 2, PT_BUG },  // 204 bicho
  { "FORRETRESS", 0, 0, R_EVO, 0x7CC4, 75, 90, 140, 40, 2, PT_BUG },  // 205 bicho
  { "DUNSPARCE", 0, 0, R_COMUN, 0x8C4D, 100, 70, 70, 45, 0, PT_NORMAL },  // 206 normal
  { "GLIGAR", 0, 0, R_RARO, 0xB447, 65, 75, 105, 85, 4, PT_GROUND },  // 207 tierra
  { "STEELIX", 0, 0, R_EVO, 0x8C95, 75, 85, 200, 30, 4, PT_STEEL },  // 208 acero
  { "SNUBBULL", 210, 23, R_COMUN, 0x8C4D, 60, 80, 50, 30, 0, PT_NORMAL },  // 209 normal
  { "GRANBULL", 0, 0, R_EVO, 0x8C4D, 90, 120, 75, 45, 0, PT_NORMAL },  // 210 normal
  { "QWILFISH", 0, 0, R_RARO, 0x4C98, 65, 95, 85, 85, 1, PT_WATER },  // 211 agua
  { "SCIZOR", 0, 0, R_EVO, 0x7CC4, 70, 130, 100, 65, 2, PT_BUG },  // 212 bicho
  { "SHUCKLE", 0, 0, R_RARO, 0x7CC4, 20, 10, 230, 5, 2, PT_BUG },  // 213 bicho
  { "HERACROSS", 0, 0, R_RARO, 0x7CC4, 80, 125, 75, 85, 2, PT_BUG },  // 214 bicho
  { "SNEASEL", 0, 0, R_RARO, 0x5A48, 55, 95, 55, 115, 0, PT_DARK },  // 215 siniestro
  { "TEDDIURSA", 217, 30, R_COMUN, 0x8C4D, 60, 80, 50, 40, 0, PT_NORMAL },  // 216 normal
  { "URSARING", 0, 0, R_EVO, 0x8C4D, 90, 130, 75, 55, 0, PT_NORMAL },  // 217 normal
  { "SLUGMA", 219, 38, R_COMUN, 0xEA87, 40, 40, 40, 20, 3, PT_FIRE },  // 218 fuego
  { "MAGCARGO", 0, 0, R_EVO, 0xEA87, 60, 50, 120, 30, 3, PT_FIRE },  // 219 fuego
  { "SWINUB", 221, 33, R_COMUN, 0x4DB8, 50, 50, 40, 50, 5, PT_ICE },  // 220 hielo
  { "PILOSWINE", 0, 0, R_EVO, 0x4DB8, 100, 100, 80, 50, 5, PT_ICE },  // 221 hielo
  { "CORSOLA", 0, 0, R_RARO, 0x4C98, 65, 55, 95, 35, 1, PT_WATER },  // 222 agua
  { "REMORAID", 224, 25, R_COMUN, 0x4C98, 35, 65, 35, 65, 1, PT_WATER },  // 223 agua
  { "OCTILLERY", 0, 0, R_EVO, 0x4C98, 75, 105, 75, 45, 1, PT_WATER },  // 224 agua
  { "DELIBIRD", 0, 0, R_RARO, 0x4DB8, 45, 55, 45, 75, 5, PT_ICE },  // 225 hielo
  { "MANTINE", 0, 0, R_RARO, 0x4C98, 85, 40, 70, 70, 1, PT_WATER },  // 226 agua
  { "SKARMORY", 0, 0, R_RARO, 0x8C95, 65, 80, 140, 70, 4, PT_STEEL },  // 227 acero
  { "HOUNDOUR", 229, 24, R_COMUN, 0x5A48, 45, 60, 30, 65, 0, PT_DARK },  // 228 siniestro
  { "HOUNDOOM", 0, 0, R_EVO, 0x5A48, 75, 90, 50, 95, 0, PT_DARK },  // 229 siniestro
  { "KINGDRA", 0, 0, R_EVO, 0x4C98, 75, 95, 95, 85, 1, PT_WATER },  // 230 agua
  { "PHANPY", 232, 25, R_COMUN, 0xB447, 90, 60, 60, 40, 4, PT_GROUND },  // 231 tierra
  { "DONPHAN", 0, 0, R_EVO, 0xB447, 90, 120, 120, 50, 4, PT_GROUND },  // 232 tierra
  { "PORYGON2", 0, 0, R_EVO, 0x8C4D, 85, 80, 90, 60, 0, PT_NORMAL },  // 233 normal
  { "STANTLER", 0, 0, R_RARO, 0x8C4D, 73, 95, 62, 85, 0, PT_NORMAL },  // 234 normal
  { "SMEARGLE", 0, 0, R_RARO, 0x8C4D, 55, 20, 35, 75, 0, PT_NORMAL },  // 235 normal
  { "TYROGUE", 106, 20, R_RARO, 0xA2A5, 35, 35, 35, 35, 0, PT_FIGHT },  // 236 lucha
  { "HITMONTOP", 0, 0, R_EVO, 0xA2A5, 50, 95, 95, 70, 0, PT_FIGHT },  // 237 lucha
  { "SMOOCHUM", 124, 30, R_COMUN, 0x4DB8, 45, 30, 15, 65, 5, PT_ICE },  // 238 hielo
  { "ELEKID", 125, 30, R_COMUN, 0xBCA1, 45, 63, 37, 95, 0, PT_ELECTRIC },  // 239 electrico
  { "MAGBY", 126, 30, R_COMUN, 0xEA87, 45, 75, 37, 83, 3, PT_FIRE },  // 240 fuego
  { "MILTANK", 0, 0, R_RARO, 0x8C4D, 95, 80, 105, 100, 0, PT_NORMAL },  // 241 normal
  { "BLISSEY", 0, 0, R_EVO, 0x8C4D, 255, 10, 10, 55, 0, PT_NORMAL },  // 242 normal
  { "RAIKOU", 0, 0, R_LEGENDARIO, 0xBCA1, 90, 85, 75, 115, 0, PT_ELECTRIC },  // 243 electrico
  { "ENTEI", 0, 0, R_LEGENDARIO, 0xEA87, 115, 115, 85, 100, 3, PT_FIRE },  // 244 fuego
  { "SUICUNE", 0, 0, R_LEGENDARIO, 0x4C98, 100, 75, 115, 85, 1, PT_WATER },  // 245 agua
  { "LARVITAR", 247, 30, R_RARO, 0x9407, 50, 64, 50, 41, 4, PT_ROCK },  // 246 roca
  { "PUPITAR", 248, 55, R_EVO, 0x9407, 70, 84, 70, 51, 4, PT_ROCK },  // 247 roca
  { "TYRANITAR", 0, 0, R_EVO, 0x9407, 100, 134, 110, 61, 4, PT_ROCK },  // 248 roca
  { "LUGIA", 0, 0, R_LEGENDARIO, 0xD28F, 106, 90, 130, 110, 0, PT_PSYCHIC },  // 249 psiquico
  { "HO-OH", 0, 0, R_LEGENDARIO, 0xEA87, 106, 130, 90, 90, 3, PT_FIRE },  // 250 fuego
  { "CELEBI", 0, 0, R_LEGENDARIO, 0xD28F, 100, 100, 100, 100, 0, PT_PSYCHIC },  // 251 psiquico
};

// Nombres oficiales por idioma. FR y DE son los unicos latinos que difieren
// del ingles en gen 1 (ES/IT/PT usan el de DEX_TBL); JA y KO van en UTF-8,
// que se pinta con la fuente U8g2. nullptr = sin nombre propio.
static const char *const DEX_NAME_FR[DEX_COUNT + 1] = {
  nullptr, "BULBIZARRE", "HERBIZARRE", "FLORIZARRE",
  "SALAMECHE", "REPTINCEL", "DRACAUFEU", "CARAPUCE",
  "CARABAFFE", "TORTANK", "CHENIPAN", "CHRYSACIER",
  "PAPILUSION", "ASPICOT", "COCONFORT", "DARDARGNAN",
  "ROUCOOL", "ROUCOUPS", "ROUCARNAGE", nullptr,
  "RATTATAC", "PIAFABEC", "RAPASDEPIC", "ABO",
  nullptr, nullptr, nullptr, "SABELETTE",
  "SABLAIREAU", nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, "MELOFEE",
  "MELODELFE", "GOUPIX", "FEUNARD", "RONDOUDOU",
  "GRODOUDOU", "NOSFERAPTI", "NOSFERALTO", "MYSTHERBE",
  "ORTIDE", "RAFFLESIA", nullptr, nullptr,
  "MIMITOSS", "AEROMITE", "TAUPIQUEUR", "TRIOPIKEUR",
  "MIAOUSS", nullptr, "PSYKOKWAK", "AKWAKWAK",
  "FEROSINGE", "COLOSSINGE", "CANINOS", "ARCANIN",
  "PTITARD", "TETARTE", "TARTARD", nullptr,
  nullptr, nullptr, "MACHOC", "MACHOPEUR",
  "MACKOGNEUR", "CHETIFLOR", "BOUSTIFLOR", "EMPIFLOR",
  nullptr, nullptr, "RACAILLOU", "GRAVALANCH",
  "GROLEM", nullptr, "GALOPA", "RAMOLOSS",
  "FLAGADOSS", "MAGNETI", nullptr, "CANARTICHO",
  nullptr, nullptr, "OTARIA", "LAMANTINE",
  "TADMORV", "GROTADMORV", "KOKIYAS", "CRUSTABRI",
  "FANTOMINUS", "SPECTRUM", "ECTOPLASMA", nullptr,
  "SOPORIFIK", "HYPNOMADE", nullptr, "KRABBOSS",
  "VOLTORBE", nullptr, "NOEUNOEUF", "NOADKOKO",
  "OSSELAIT", "OSSATUEUR", "KICKLEE", "TYGNON",
  "EXCELANGUE", "SMOGO", "SMOGOGO", "RHINOCORNE",
  "RHINOFEROS", "LEVEINARD", "SAQUEDENEU", "KANGOUREX",
  "HYPOTREMPE", "HYPOCEAN", "POISSIRENE", "POISSOROY",
  "STARI", "STAROSS", "M. MIME", "INSECATEUR",
  "LIPPOUTOU", "ELEKTEK", nullptr, "SCARABRUTE",
  nullptr, "MAGICARPE", "LEVIATOR", "LOKHLASS",
  "METAMORPH", "EVOLI", "AQUALI", "VOLTALI",
  "PYROLI", nullptr, "AMONITA", "AMONISTAR",
  nullptr, nullptr, "PTERA", "RONFLEX",
  "ARTIKODIN", "ELECTHOR", "SULFURA", "MINIDRACO",
  "DRACO", "DRACOLOSSE", nullptr, nullptr,
  "GERMIGNON", "MACRONIUM", nullptr, "HERICENDRE",
  "FEURISSON", nullptr, "KAIMINUS", "CROCRODIL",
  "ALIGATUEUR", "FOUINETTE", "FOUINAR", nullptr,
  "NOARFANG", "COXY", "COXYCLAQUE", "MIMIGAL",
  "MIGALOS", "NOSTENFER", "LOUPIO", nullptr,
  nullptr, "MELO", "TOUDOUDOU", nullptr,
  nullptr, nullptr, nullptr, "WATTOUAT",
  "LAINERGIE", "PHARAMP", "JOLIFLOR", nullptr,
  nullptr, "SIMULARBRE", "TARPAUD", "GRANIVOL",
  "FLORAVOL", "COTOVOL", "CAPUMAIN", "TOURNEGRIN",
  "HELIATRONC", nullptr, "AXOLOTO", "MARAISTE",
  "MENTALI", "NOCTALI", "CORNEBRE", "ROIGADA",
  "FEUFOREVE", "ZARBI", "QULBUTOKE", nullptr,
  "POMDEPIK", "FORETRESS", "INSOLOURDO", "SCORPLANE",
  nullptr, nullptr, nullptr, nullptr,
  "CIZAYOX", "CARATROC", "SCARHINO", "FARFURET",
  nullptr, nullptr, "LIMAGMA", "VOLCAROPOD",
  "MARCACRIN", "COCHIGNON", "CORAYON", nullptr,
  nullptr, "CADOIZO", "DEMANTA", "AIRMURE",
  "MALOSSE", "DEMOLOSSE", "HYPOROI", nullptr,
  nullptr, nullptr, "CERFROUSSE", "QUEULORIOR",
  "DEBUGANT", "KAPOERA", "LIPPOUTI", nullptr,
  nullptr, "ECREMEUH", "LEUPHORIE", nullptr,
  nullptr, nullptr, "EMBRYLEX", "YMPHECT",
  "TYRANOCIF", nullptr, nullptr, nullptr,
};

static const char *const DEX_NAME_DE[DEX_COUNT + 1] = {
  nullptr, "BISASAM", "BISAKNOSP", "BISAFLOR",
  "GLUMANDA", "GLUTEXO", "GLURAK", "SCHIGGY",
  "SCHILLOK", "TURTOK", "RAUPY", "SAFCON",
  "SMETTBO", "HORNLIU", "KOKUNA", "BIBOR",
  "TAUBSI", "TAUBOGA", "TAUBOSS", "RATTFRATZ",
  "RATTIKARL", "HABITAK", "IBITAK", "RETTAN",
  nullptr, nullptr, nullptr, "SANDAN",
  "SANDAMER", nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, "PIEPI",
  "PIXI", nullptr, "VULNONA", "PUMMELUFF",
  "KNUDDELUFF", nullptr, nullptr, "MYRAPLA",
  "DUFLOR", "GIFLOR", nullptr, "PARASEK",
  "BLUZUK", "OMOT", "DIGDA", "DIGDRI",
  "MAUZI", "SNOBILIKAT", "ENTON", "ENTORON",
  "MENKI", "RASAFF", "FUKANO", "ARKANI",
  "QUAPSEL", "QUAPUTZI", "QUAPPO", nullptr,
  nullptr, "SIMSALA", "MACHOLLO", "MASCHOCK",
  "MACHOMEI", "KNOFENSA", "ULTRIGARIA", "SARZENIA",
  "TENTACHA", "TENTOXA", "KLEINSTEIN", "GEOROK",
  "GEOWAZ", "PONITA", "GALLOPA", "FLEGMON",
  "LAHMUS", "MAGNETILO", nullptr, "PORENTA",
  "DODU", "DODRI", "JUROB", "JUGONG",
  "SLEIMA", "SLEIMOK", "MUSCHAS", "AUSTOS",
  "NEBULAK", "ALPOLLO", nullptr, nullptr,
  "TRAUMATO", nullptr, nullptr, nullptr,
  "VOLTOBAL", "LEKTROBAL", "OWEI", "KOKOWEI",
  "TRAGOSSO", "KNOGGA", "KICKLEE", "NOCKCHAN",
  "SCHLURP", "SMOGON", "SMOGMOG", "RIHORN",
  "RIZEROS", "CHANEIRA", nullptr, "KANGAMA",
  "SEEPER", "SEEMON", "GOLDINI", "GOLKING",
  "STERNDU", nullptr, "PANTIMOS", "SICHLOR",
  "ROSSANA", "ELEKTEK", nullptr, nullptr,
  nullptr, "KARPADOR", "GARADOS", nullptr,
  nullptr, "EVOLI", "AQUANA", "BLITZA",
  "FLAMARA", nullptr, "AMONITAS", "AMOROSO",
  nullptr, nullptr, nullptr, "RELAXO",
  "ARKTOS", nullptr, "LAVADOS", nullptr,
  "DRAGONIR", "DRAGORAN", "MEWTU", nullptr,
  "ENDIVIE", "LORBLATT", "MEGANIE", "FEURIGEL",
  "IGELAVAR", "TORNUPTO", "KARNIMANI", "TYRACROC",
  "IMPERGATOR", "WIESOR", "WIESENIOR", nullptr,
  "NOCTUH", nullptr, nullptr, "WEBARAK",
  nullptr, "IKSBAT", "LAMPI", nullptr,
  nullptr, "PII", "FLUFFELUFF", nullptr,
  nullptr, nullptr, nullptr, "VOLTILAMM",
  "WAATY", nullptr, "BLUBELLA", nullptr,
  nullptr, "MOGELBAUM", "QUAXO", "HOPPSPROSS",
  "HUBELUPF", "PAPUNGHA", "GRIFFEL", "SONNKERN",
  "SONNFLORA", nullptr, "FELINO", "MORLORD",
  "PSIANA", "NACHTARA", "KRAMURX", "LASCHOKING",
  "TRAUNFUGIL", "ICOGNITO", "WOINGENAU", nullptr,
  "TANNZA", "FORSTELLKA", "DUMMISEL", "SKORGLA",
  "STAHLOS", nullptr, nullptr, "BALDORFISH",
  "SCHEROX", "POTTROTT", "SKARABORN", "SNIEBEL",
  nullptr, nullptr, "SCHNECKMAG", nullptr,
  "QUIEKEL", "KEIFEL", "CORASONN", nullptr,
  nullptr, "BOTOGEL", "MANTAX", "PANZAERON",
  "HUNDUSTER", "HUNDEMON", "SEEDRAKING", nullptr,
  nullptr, nullptr, "DAMHIRPLEX", "FARBEAGLE",
  "RABAUZ", "KAPOERA", "KUSSILLA", nullptr,
  nullptr, nullptr, "HEITEIRA", nullptr,
  nullptr, nullptr, nullptr, nullptr,
  "DESPOTAR", nullptr, nullptr, nullptr,
};

static const char *const DEX_NAME_JA[DEX_COUNT + 1] = {
  nullptr, "フシギダネ", "フシギソウ", "フシギバナ",
  "ヒトカゲ", "リザード", "リザードン", "ゼニガメ",
  "カメール", "カメックス", "キャタピー", "トランセル",
  "バタフリー", "ビードル", "コクーン", "スピアー",
  "ポッポ", "ピジョン", "ピジョット", "コラッタ",
  "ラッタ", "オニスズメ", "オニドリル", "アーボ",
  "アーボック", "ピカチュウ", "ライチュウ", "サンド",
  "サンドパン", "ニドランF", "ニドリーナ", "ニドクイン",
  "ニドランM", "ニドリーノ", "ニドキング", "ピッピ",
  "ピクシー", "ロコン", "キュウコン", "プリン",
  "プクリン", "ズバット", "ゴルバット", "ナゾノクサ",
  "クサイハナ", "ラフレシア", "パラス", "パラセクト",
  "コンパン", "モルフォン", "ディグダ", "ダグトリオ",
  "ニャース", "ペルシアン", "コダック", "ゴルダック",
  "マンキー", "オコリザル", "ガーディ", "ウインディ",
  "ニョロモ", "ニョロゾ", "ニョロボン", "ケーシィ",
  "ユンゲラー", "フーディン", "ワンリキー", "ゴーリキー",
  "カイリキー", "マダツボミ", "ウツドン", "ウツボット",
  "メノクラゲ", "ドククラゲ", "イシツブテ", "ゴローン",
  "ゴローニャ", "ポニータ", "ギャロップ", "ヤドン",
  "ヤドラン", "コイル", "レアコイル", "カモネギ",
  "ドードー", "ドードリオ", "パウワウ", "ジュゴン",
  "ベトベター", "ベトベトン", "シェルダー", "パルシェン",
  "ゴース", "ゴースト", "ゲンガー", "イワーク",
  "スリープ", "スリーパー", "クラブ", "キングラー",
  "ビリリダマ", "マルマイン", "タマタマ", "ナッシー",
  "カラカラ", "ガラガラ", "サワムラー", "エビワラー",
  "ベロリンガ", "ドガース", "マタドガス", "サイホーン",
  "サイドン", "ラッキー", "モンジャラ", "ガルーラ",
  "タッツー", "シードラ", "トサキント", "アズマオウ",
  "ヒトデマン", "スターミー", "バリヤード", "ストライク",
  "ルージュラ", "エレブー", "ブーバー", "カイロス",
  "ケンタロス", "コイキング", "ギャラドス", "ラプラス",
  "メタモン", "イーブイ", "シャワーズ", "サンダース",
  "ブースター", "ポリゴン", "オムナイト", "オムスター",
  "カブト", "カブトプス", "プテラ", "カビゴン",
  "フリーザー", "サンダー", "ファイヤー", "ミニリュウ",
  "ハクリュー", "カイリュー", "ミュウツー", "ミュウ",
  "チコリータ", "ベイリーフ", "メガニウム", "ヒノアラシ",
  "マグマラシ", "バクフーン", "ワニノコ", "アリゲイツ",
  "オーダイル", "オタチ", "オオタチ", "ホーホー",
  "ヨルノズク", "レディバ", "レディアン", "イトマル",
  "アリアドス", "クロバット", "チョンチー", "ランターン",
  "ピチュー", "ピィ", "ププリン", "トゲピー",
  "トゲチック", "ネイティ", "ネイティオ", "メリープ",
  "モココ", "デンリュウ", "キレイハナ", "マリル",
  "マリルリ", "ウソッキー", "ニョロトノ", "ハネッコ",
  "ポポッコ", "ワタッコ", "エイパム", "ヒマナッツ",
  "キマワリ", "ヤンヤンマ", "ウパー", "ヌオー",
  "エーフィ", "ブラッキー", "ヤミカラス", "ヤドキング",
  "ムウマ", "アンノーン", "ソーナンス", "キリンリキ",
  "クヌギダマ", "フォレトス", "ノコッチ", "グライガー",
  "ハガネール", "ブルー", "グランブル", "ハリーセン",
  "ハッサム", "ツボツボ", "ヘラクロス", "ニューラ",
  "ヒメグマ", "リングマ", "マグマッグ", "マグカルゴ",
  "ウリムー", "イノムー", "サニーゴ", "テッポウオ",
  "オクタン", "デリバード", "マンタイン", "エアームド",
  "デルビル", "ヘルガー", "キングドラ", "ゴマゾウ",
  "ドンファン", "ポリゴン２", "オドシシ", "ドーブル",
  "バルキー", "カポエラー", "ムチュール", "エレキッド",
  "ブビィ", "ミルタンク", "ハピナス", "ライコウ",
  "エンテイ", "スイクン", "ヨーギラス", "サナギラス",
  "バンギラス", "ルギア", "ホウオウ", "セレビィ",
};

static const char *const DEX_NAME_KO[DEX_COUNT + 1] = {
  nullptr, "이상해씨", "이상해풀", "이상해꽃",
  "파이리", "리자드", "리자몽", "꼬부기",
  "어니부기", "거북왕", "캐터피", "단데기",
  "버터플", "뿔충이", "딱충이", "독침붕",
  "구구", "피죤", "피죤투", "꼬렛",
  "레트라", "깨비참", "깨비드릴조", "아보",
  "아보크", "피카츄", "라이츄", "모래두지",
  "고지", "니드런 암", "니드리나", "니드퀸",
  "니드런 수", "니드리노", "니드킹", "삐삐",
  "픽시", "식스테일", "나인테일", "푸린",
  "푸크린", "주뱃", "골뱃", "뚜벅쵸",
  "냄새꼬", "라플레시아", "파라스", "파라섹트",
  "콘팡", "도나리", "디그다", "닥트리오",
  "나옹", "페르시온", "고라파덕", "골덕",
  "망키", "성원숭", "가디", "윈디",
  "발챙이", "슈륙챙이", "강챙이", "캐이시",
  "윤겔라", "후딘", "알통몬", "근육몬",
  "괴력몬", "모다피", "우츠동", "우츠보트",
  "왕눈해", "독파리", "꼬마돌", "데구리",
  "딱구리", "포니타", "날쌩마", "야돈",
  "야도란", "코일", "레어코일", "파오리",
  "두두", "두트리오", "쥬쥬", "쥬레곤",
  "질퍽이", "질뻐기", "셀러", "파르셀",
  "고오스", "고우스트", "팬텀", "롱스톤",
  "슬리프", "슬리퍼", "크랩", "킹크랩",
  "찌리리공", "붐볼", "아라리", "나시",
  "탕구리", "텅구리", "시라소몬", "홍수몬",
  "내루미", "또가스", "또도가스", "뿔카노",
  "코뿌리", "럭키", "덩쿠리", "캥카",
  "쏘드라", "시드라", "콘치", "왕콘치",
  "별가사리", "아쿠스타", "마임맨", "스라크",
  "루주라", "에레브", "마그마", "쁘사이저",
  "켄타로스", "잉어킹", "갸라도스", "라프라스",
  "메타몽", "이브이", "샤미드", "쥬피썬더",
  "부스터", "폴리곤", "암나이트", "암스타",
  "투구", "투구푸스", "프테라", "잠만보",
  "프리져", "썬더", "파이어", "미뇽",
  "신뇽", "망나뇽", "뮤츠", "뮤",
  "치코리타", "베이리프", "메가니움", "브케인",
  "마그케인", "블레이범", "리아코", "엘리게이",
  "장크로다일", "꼬리선", "다꼬리", "부우부",
  "야부엉", "레디바", "레디안", "페이검",
  "아리아도스", "크로뱃", "초라기", "랜턴",
  "피츄", "삐", "푸푸린", "토게피",
  "토게틱", "네이티", "네이티오", "메리프",
  "보송송", "전룡", "아르코", "마릴",
  "마릴리", "꼬지모", "왕구리", "통통코",
  "두코", "솜솜코", "에이팜", "해너츠",
  "해루미", "왕자리", "우파", "누오",
  "에브이", "블래키", "니로우", "야도킹",
  "무우마", "안농", "마자용", "키링키",
  "피콘", "쏘콘", "노고치", "글라이거",
  "강철톤", "블루", "그랑블루", "침바루",
  "핫삼", "단단지", "헤라크로스", "포푸니",
  "깜지곰", "링곰", "마그마그", "마그카르고",
  "꾸꾸리", "메꾸리", "코산호", "총어",
  "대포무노", "딜리버드", "만타인", "무장조",
  "델빌", "헬가", "킹드라", "코코리",
  "코리갑", "폴리곤2", "노라키", "루브도",
  "배루키", "카포에라", "뽀뽀라", "에레키드",
  "마그비", "밀탱크", "해피너스", "라이코",
  "앤테이", "스이쿤", "애버라스", "데기라스",
  "마기라스", "루기아", "칠색조", "세레비",
};

// Nombre de la especie en el idioma activo (cae al de DEX_TBL si ese
// idioma no tiene nombre propio para ella).
static inline const char *dexName(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return DEX_TBL[0].name;
  const char *n = (gLang == LANG_FR)   ? DEX_NAME_FR[dex]
                  : (gLang == LANG_DE) ? DEX_NAME_DE[dex]
                  : (gLang == LANG_JA) ? DEX_NAME_JA[dex]
                  : (gLang == LANG_KO) ? DEX_NAME_KO[dex]
                                       : nullptr;
  return n ? n : DEX_TBL[dex].name;
}

// evoluciones alternativas (la principal esta en DEX_TBL.evolvesTo)
struct EvoAlt { uint8_t from, to; };
static const EvoAlt EVO_ALT[] = {
  { 133, 135 },
  { 133, 136 },
  { 133, 196 },
  { 133, 197 },
  { 79, 199 },
  { 61, 186 },
  { 44, 182 },
  { 236, 107 },
  { 236, 237 },
};
#define EVO_ALT_COUNT 9

// familia de cada especie: el dex mas bajo de su linea evolutiva
static const uint8_t DEX_FAM[DEX_COUNT + 1] = {
  0, 1, 1, 1, 4, 4, 4, 7, 7, 7, 10, 10, 10, 13, 13, 13, 16, 16, 16, 19, 19,
  21, 21, 23, 23, 25, 25, 27, 27, 29, 29, 29, 32, 32, 32, 35, 35, 37, 37, 39, 39,
  41, 41, 43, 43, 43, 46, 46, 48, 48, 50, 50, 52, 52, 54, 54, 56, 56, 58, 58, 60,
  60, 60, 63, 63, 63, 66, 66, 66, 69, 69, 69, 72, 72, 74, 74, 74, 77, 77, 79, 79,
  81, 81, 83, 84, 84, 86, 86, 88, 88, 90, 90, 92, 92, 92, 95, 96, 96, 98, 98, 100,
  100, 102, 102, 104, 104, 106, 106, 108, 109, 109, 111, 111, 113, 114, 115, 116, 116, 118, 118, 120,
  120, 122, 123, 124, 125, 126, 127, 128, 129, 129, 131, 132, 133, 133, 133, 133, 137, 138, 138, 140,
  140, 142, 143, 144, 145, 146, 147, 147, 147, 150, 151, 152, 152, 152, 155, 155, 155, 158, 158, 158,
  161, 161, 163, 163, 165, 165, 167, 167, 41, 170, 170, 25, 35, 39, 175, 175, 177, 177, 179, 179,
  179, 43, 183, 183, 185, 60, 187, 187, 187, 190, 191, 191, 193, 194, 194, 133, 133, 198, 79, 200,
  201, 202, 203, 204, 204, 206, 207, 95, 209, 209, 211, 123, 213, 214, 215, 216, 216, 218, 218, 220,
  220, 222, 223, 223, 225, 226, 227, 228, 228, 116, 231, 231, 137, 234, 235, 106, 106, 124, 125, 126,
  241, 113, 243, 244, 245, 246, 246, 246, 249, 250, 251,
};

// opciones de evolucion de dex (principal + ramas). Devuelve cuantas.
static inline int dexEvoOptions(int16_t dex, int16_t out[8]) {
  int n = 0;
  if (dex < 1 || dex > DEX_COUNT || !DEX_TBL[dex].evolvesTo) return 0;
  out[n++] = DEX_TBL[dex].evolvesTo;
  for (int i = 0; i < EVO_ALT_COUNT && n < 8; i++)
    if (EVO_ALT[i].from == dex) out[n++] = EVO_ALT[i].to;
  return n;
}

// de quien evoluciona (0 = forma base)
static inline int16_t dexPrevo(int16_t dex) {
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].evolvesTo == dex) return d;
  for (int i = 0; i < EVO_ALT_COUNT; i++)
    if (EVO_ALT[i].to == dex) return EVO_ALT[i].from;
  return 0;
}

// el primer huevo de la partida: iniciales clasicos
static const int16_t CLASSIC_DEX[] = { 1, 4, 7, 25, 133 };
#define NUM_CLASSIC_DEX 5

#!/usr/bin/env python3
"""Genera dex.h (tabla de la Pokedex para el firmware) desde dex_data.py.

  python3 tools/gen_dex.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from dex_data import DEX, TYPE_ACCENTS, CLASSIC, RARE, LEGENDARY, EVO_ALT
from dex_stats import BASE_STATS
from dex_names import LOCAL_NAMES


def rgb565(hexcol):
    r, g, b = int(hexcol[1:3], 16), int(hexcol[3:5], 16), int(hexcol[5:7], 16)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


# bioma de fondo por tipo (la luz la pone la hora real del RTC)
# 0 PRADERA, 1 PLAYA, 2 BOSQUE, 3 VOLCAN, 4 MONTANA, 5 NIEVE
TYPE_BIOME = {
    'agua': 1, 'planta': 2, 'bicho': 2, 'fuego': 3,
    'roca': 4, 'tierra': 4, 'dragon': 1, 'hielo': 5,  # los dragones gen1 (Dratini) viven en el agua
    'normal': 0, 'electrico': 0, 'lucha': 0, 'veneno': 0,
    'psiquico': 0, 'fantasma': 0,
    'siniestro': 0, 'acero': 4,  # ko10
}

# tipo (primario, gen 1) para las batallas: indice en PT_* de dex.h
TYPE_ENUM = ['normal', 'fuego', 'agua', 'planta', 'electrico', 'hielo', 'lucha',
             'veneno', 'tierra', 'psiquico', 'bicho', 'roca', 'fantasma', 'dragon',
             'siniestro', 'acero']
TYPE_CNAME = ['PT_NORMAL', 'PT_FIRE', 'PT_WATER', 'PT_GRASS', 'PT_ELECTRIC', 'PT_ICE',
              'PT_FIGHT', 'PT_POISON', 'PT_GROUND', 'PT_PSYCHIC', 'PT_BUG', 'PT_ROCK',
              'PT_GHOST', 'PT_DRAGON', 'PT_DARK', 'PT_STEEL']

# excepciones por dex# (el tipo no basta): fosiles marinos roca/agua -> playa
BIOME_OVERRIDE = {138: 1, 139: 1, 140: 1, 141: 1}  # Omanyte, Omastar, Kabuto, Kabutops


def main():
    out = []
    out.append("#pragma once\n#include <stdint.h>\n#include \"i18n.h\"  // gLang\n\n")
    out.append("// GENERADO por tools/gen_dex.py desde tools/dex_data.py - no editar\n\n")
    n = len(DEX)
    out.append(f"#define DEX_COUNT {n}\n")
    out.append(f"#define DEX_BYTES {(n + 7) // 8}  // bitmap de la pokedex\n")
    out.append("#define DEX_EEVEE 133\n\n")
    out.append(
        "// rareza: 0 = solo por evolucion, 1 = comun, 2 = raro, 3 = legendario\n"
        "enum : uint8_t { R_EVO = 0, R_COMUN, R_RARO, R_LEGENDARIO };\n\n"
        "// tipo primario (gen 1-2) para las batallas\n"
        "enum : uint8_t { " + ", ".join(TYPE_CNAME) + ", PT_COUNT };\n\n"
        "struct DexEntry {\n"
        "  const char *name;\n"
        "  uint8_t evolvesTo;    // numero de dex, 0 = forma final\n"
        "  uint8_t evolveLevel;\n"
        "  uint8_t rarity;       // sale de huevo si > 0\n"
        "  uint16_t accent;      // color RGB565 del tipo para la UI\n"
        "  uint8_t bHp, bAtk, bDef, bSpe;  // base stats reales de gen 1\n"
        "  uint8_t biome;        // 0 pradera 1 playa 2 bosque 3 volcan 4 montana 5 nieve\n"
        "  uint8_t ptype;        // tipo primario PT_* (batallas)\n"
        "};\n\n")
    # formas base = las que no son evolucion de nadie (las ramas tambien lo son).
    # ko10: los gen 1 que ahora tienen "bebe" de gen 2 (Pikachu, Clefairy,
    # Jigglypuff, Hitmonlee...) siguen saliendo de huevo como antes
    edges = [(d[0], d[4]) for d in DEX if d[4]] + list(EVO_ALT)
    evolved = {to for fr, to in edges if not (fr > 151 and to <= 151)}
    rarities = []
    out.append("static const DexEntry DEX_TBL[DEX_COUNT + 1] = {\n")
    out.append('  { "?", 0, 0, 0, 0x2946, 50, 50, 50, 50, 0, PT_NORMAL },  // 0: sin usar\n')
    for num, slug, display, typ, evo, lvl in DEX:
        acc = rgb565(TYPE_ACCENTS[typ])
        if num in evolved:
            rar = 'R_EVO'
        elif num in LEGENDARY:
            rar = 'R_LEGENDARIO'
        elif num in RARE:
            rar = 'R_RARO'
        else:
            rar = 'R_COMUN'
        rarities.append(rar)
        hp, atk, df, spe = BASE_STATS[num]
        bio = BIOME_OVERRIDE.get(num, TYPE_BIOME[typ])
        out.append(f'  {{ "{display}", {evo}, {lvl}, {rar}, 0x{acc:04X}, {hp}, {atk}, {df}, {spe}, {bio}, {TYPE_CNAME[TYPE_ENUM.index(typ)]} }},  // {num} {typ}\n')
    out.append("};\n\n")

    # Solo FR y DE tienen nombre propio en gen 1; ES/IT/PT usan el ingles.
    out.append(
        "// Nombres oficiales por idioma. FR y DE son los unicos latinos que difieren\n"
        "// del ingles en gen 1 (ES/IT/PT usan el de DEX_TBL); JA y KO van en UTF-8,\n"
        "// que se pinta con la fuente U8g2. nullptr = sin nombre propio.\n")
    for lg in ('fr', 'de', 'ja', 'ko'):
        out.append(f"static const char *const DEX_NAME_{lg.upper()}[DEX_COUNT + 1] = {{\n")
        fila = []
        for num in range(0, n + 1):
            nm = LOCAL_NAMES.get(num, {}).get(lg)
            fila.append(f'"{nm}"' if nm else 'nullptr')
            if len(fila) == 4:
                out.append("  " + ", ".join(fila) + ",\n")
                fila = []
        if fila:
            out.append("  " + ", ".join(fila) + ",\n")
        out.append("};\n\n")
    out.append(
        "// Nombre de la especie en el idioma activo (cae al de DEX_TBL si ese\n"
        "// idioma no tiene nombre propio para ella).\n"
        "static inline const char *dexName(int16_t dex) {\n"
        "  if (dex < 1 || dex > DEX_COUNT) return DEX_TBL[0].name;\n"
        "  const char *n = (gLang == LANG_FR)   ? DEX_NAME_FR[dex]\n"
        "                  : (gLang == LANG_DE) ? DEX_NAME_DE[dex]\n"
        "                  : (gLang == LANG_JA) ? DEX_NAME_JA[dex]\n"
        "                  : (gLang == LANG_KO) ? DEX_NAME_KO[dex]\n"
        "                                       : nullptr;\n"
        "  return n ? n : DEX_TBL[dex].name;\n"
        "}\n\n")

    # ko10: ramas de evolucion y "familia" (el numero mas bajo de toda la linea,
    # asi un bebe de gen 2 comparte gustos con su evolucion de gen 1)
    out.append("// evoluciones alternativas (la principal esta en DEX_TBL.evolvesTo)\n")
    out.append("struct EvoAlt { uint8_t from, to; };\n")
    out.append("static const EvoAlt EVO_ALT[] = {\n")
    for fr, to in EVO_ALT:
        out.append(f"  {{ {fr}, {to} }},\n")
    out.append("};\n")
    out.append(f"#define EVO_ALT_COUNT {len(EVO_ALT)}\n\n")
    parent = {}
    for fr, to in edges:
        parent.setdefault(to, fr)
    def root(d):
        while d in parent:
            d = parent[d]
        return d
    fam = {}
    for num, *_ in DEX:
        r = root(num)
        fam.setdefault(r, []).append(num)
    anchor = {}
    for r, members in fam.items():
        for m in members:
            anchor[m] = min(members)
    out.append("// familia de cada especie: el dex mas bajo de su linea evolutiva\n")
    out.append("static const uint8_t DEX_FAM[DEX_COUNT + 1] = {\n  0,")
    for num, *_ in DEX:
        out.append(f" {anchor[num]},")
        if num % 20 == 0:
            out.append("\n ")
    out.append("\n};\n\n")
    out.append(
        "// opciones de evolucion de dex (principal + ramas). Devuelve cuantas.\n"
        "static inline int dexEvoOptions(int16_t dex, int16_t out[8]) {\n"
        "  int n = 0;\n"
        "  if (dex < 1 || dex > DEX_COUNT || !DEX_TBL[dex].evolvesTo) return 0;\n"
        "  out[n++] = DEX_TBL[dex].evolvesTo;\n"
        "  for (int i = 0; i < EVO_ALT_COUNT && n < 8; i++)\n"
        "    if (EVO_ALT[i].from == dex) out[n++] = EVO_ALT[i].to;\n"
        "  return n;\n"
        "}\n\n"
        "// de quien evoluciona (0 = forma base)\n"
        "static inline int16_t dexPrevo(int16_t dex) {\n"
        "  for (int16_t d = 1; d <= DEX_COUNT; d++)\n"
        "    if (DEX_TBL[d].evolvesTo == dex) return d;\n"
        "  for (int i = 0; i < EVO_ALT_COUNT; i++)\n"
        "    if (EVO_ALT[i].to == dex) return EVO_ALT[i].from;\n"
        "  return 0;\n"
        "}\n\n")
    out.append("// el primer huevo de la partida: iniciales clasicos\n")
    out.append("static const int16_t CLASSIC_DEX[] = { %s };\n" % ", ".join(map(str, CLASSIC)))
    out.append(f"#define NUM_CLASSIC_DEX {len(CLASSIC)}\n")
    from collections import Counter
    c = Counter(rarities)
    print(f"bases: {c['R_COMUN']} comunes, {c['R_RARO']} raras, {c['R_LEGENDARIO']} legendarias, {c['R_EVO']} solo-evolucion")

    path = os.path.join(os.path.dirname(__file__), '..', 'dex.h')
    open(path, 'w').write(''.join(out))
    print(f"guardado {os.path.normpath(path)} ({len(DEX)} especies)")


if __name__ == '__main__':
    main()

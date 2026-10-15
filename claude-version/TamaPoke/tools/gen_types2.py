#!/usr/bin/env python3
"""ko12.10: tipos de 2a generacion (dos tipos, con volador) de cada especie -> ../dex_types.h

  python3 gen_types2.py <cache>     (<cache>/pk/NNN.json = PokeAPI /pokemon/NNN, ver gen_moves2.py)

PokeAPI da los tipos de hoy; past_types dice los que tenia hasta una generacion (p. ej. Clefairy era
normal hasta la 5a). Hada no existe en 2a: no deberia quedar ninguno (si queda, normal).
Orden de tipos = PT_* de dex.h, y 16 = volador (PT_FLYING).
"""
import json
import os
import sys

TYPES = ['normal', 'fire', 'water', 'grass', 'electric', 'ice', 'fighting', 'poison', 'ground',
         'psychic', 'bug', 'rock', 'ghost', 'dragon', 'dark', 'steel', 'flying']
GEN = ['generation-i', 'generation-ii', 'generation-iii', 'generation-iv', 'generation-v', 'generation-vi',
       'generation-vii', 'generation-viii', 'generation-ix']
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'dex_types.h')


def gen2_types(d):
    best = None
    for pt in d.get('past_types', []):
        g = GEN.index(pt['generation']['name'])
        if g >= 1 and (best is None or g < best[0]):
            best = (g, pt['types'])
    ts = best[1] if best else d['types']
    ts = sorted(ts, key=lambda t: t['slot'])
    out = []
    for t in ts:
        n = t['type']['name']
        n = 'normal' if n == 'fairy' else n
        if n not in out:
            out.append(n)
    return out


def main():
    cache = sys.argv[1]
    t1, t2, names = [0], [255], ['?']
    for i in range(1, 252):
        d = json.load(open(os.path.join(cache, 'pk', '%03d.json' % i)))
        ts = gen2_types(d)
        t1.append(TYPES.index(ts[0]))
        t2.append(TYPES.index(ts[1]) if len(ts) > 1 else 255)
        names.append('%s %s' % (d['name'], '/'.join(ts)))
    L = ['// generado por tools/gen_types2.py (PokeAPI, tipos de 2a generacion): no editar a mano', '#pragma once',
         '#include <stdint.h>', '',
         '// tipo 1 y tipo 2 (255 = solo uno) por especie dex; 16 = volador (PT_FLYING)',
         'static const uint8_t DEX_T1[252] = {']
    L += ['  %d,  // %d %s' % (t1[i], i, names[i]) for i in range(252)]
    L += ['};', 'static const uint8_t DEX_T2[252] = {']
    L += ['  %d,' % t2[i] for i in range(252)]
    L += ['};', '']
    open(OUT, 'w').write('\n'.join(L))
    print('dex_types.h: %d con dos tipos, %d voladores' % (sum(1 for x in t2 if x != 255), sum(1 for i in range(252) if 16 in (t1[i], t2[i]))))


if __name__ == '__main__':
    main()

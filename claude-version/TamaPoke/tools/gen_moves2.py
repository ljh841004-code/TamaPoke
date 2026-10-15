#!/usr/bin/env python3
"""Tabla de movimientos "como en el original" (2a generacion, Pokemon Cristal) a partir de PokeAPI.

  python3 gen_moves2.py <cache>      -> ../moves2_data.h

<cache> es una carpeta con los JSON de PokeAPI (se descargan si faltan):
  pk/NNN.json      /pokemon/NNN        (1..251)
  pkm/<move>.json  /move/<nombre>      (los 250 que salen en Cristal)
  pkm/mach/N.json  /machine/N          (MT/MO de Cristal)

Lo que sale (solo datos y nombres oficiales):
  MV2[1..]     cada movimiento de Cristal (orden de PokeAPI): tipo (16 nuestros + 16 = volador),
               clase (0 fisico, 1 especial, 2 estado), potencia, precision, PP con los valores
               de 2a generacion (past_values), nombre coreano y una frase de descripcion
  LS_*         por especie: lo que aprende SUBIENDO de nivel en Cristal (nivel, movimiento)
  TM_*         las 57 MT/MO de Cristal y, por especie, cuales puede aprender
"""
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'moves2_data.h')
API = 'https://pokeapi.co/api/v2/'
TYPES = ['normal', 'fire', 'water', 'grass', 'electric', 'ice', 'fighting', 'poison', 'ground',
         'psychic', 'bug', 'rock', 'ghost', 'dragon', 'dark', 'steel', 'flying']
CRYSTAL = 4  # id del grupo de versiones


def fetch(path, fn):
    if not os.path.exists(fn):
        os.makedirs(os.path.dirname(fn), exist_ok=True)
        req = urllib.request.Request(API + path, headers={'User-Agent': 'Mozilla/5.0'})
        with open(fn, 'wb') as f:
            f.write(urllib.request.urlopen(req, timeout=30).read())
    with open(fn, encoding='utf-8') as f:
        return json.load(f)


def vg_id(ref):
    return int(ref['url'].rstrip('/').split('/')[-1])


def gen2(m, key, cur):
    """valor en Cristal: el de past_values del primer cambio posterior a Cristal (si lo hay)"""
    best = None
    for pv in m.get('past_values', []):
        v = vg_id(pv['version_group'])
        if v > CRYSTAL and pv.get(key) is not None and (best is None or v < best[0]):
            best = (v, pv[key])
    return best[1] if best else cur


def ko(entries, field='name'):
    for e in entries:
        if e['language']['name'] == 'ko':
            return e[field]
    return None


def main():
    cache = sys.argv[1]
    mons = [fetch('pokemon/%d' % i, os.path.join(cache, 'pk', '%03d.json' % i)) for i in range(1, 252)]
    names = set()
    for d in mons:
        for mv in d['moves']:
            if any(v['version_group']['name'] == 'crystal' for v in mv['version_group_details']):
                names.add(mv['move']['name'])
    moves = {n: fetch('move/' + n, os.path.join(cache, 'pkm', n + '.json')) for n in names}
    order = sorted(names, key=lambda n: moves[n]['id'])
    idx = {n: i + 1 for i, n in enumerate(order)}
    tms = []
    for n in order:
        for x in moves[n].get('machines', []):
            if x['version_group']['name'] == 'crystal':
                mid = x['machine']['url'].rstrip('/').split('/')[-1]
                mc = fetch('machine/' + mid, os.path.join(cache, 'pkm', 'mach', mid + '.json'))
                tms.append((mc['item']['name'], n))
    tms.sort(key=lambda t: (t[0][:2] == 'hm', int(t[0][2:])))
    tmi = {n: i for i, (_, n) in enumerate(tms)}

    L = ['// generado por tools/gen_moves2.py (PokeAPI, Pokemon Cristal): no editar a mano', '#pragma once',
         '#include <stdint.h>', '',
         '#define MV2_N %d  // 1..MV2_N-1 (0 = vacio)' % (len(order) + 1),
         '#define MV2_FLYING 16  // tipo volador (los demas: PT_*)',
         'enum : uint8_t { MV2_PHYS = 0, MV2_SPEC, MV2_STAT };',
         'struct Mv2Def { uint8_t type, cls, pow, acc, pp; };', '',
         'static const Mv2Def MV2[MV2_N] = {', '  { 0, 0, 0, 0, 0 },']
    kon, desc = ['""'], ['""']
    for n in order:
        m = moves[n]
        t = gen2(m, 'type', m['type'])
        t = t['name'] if isinstance(t, dict) else t
        t = 'normal' if t in ('fairy', 'unknown', 'shadow') else t  # 저주(curse)는 2세대에서 ??? 타입
        cls = {'physical': 0, 'special': 1, 'status': 2}[m['damage_class']['name']]
        pw = gen2(m, 'power', m['power']) or 0
        ac = gen2(m, 'accuracy', m['accuracy']) or 0
        pp = gen2(m, 'pp', m['pp']) or 0
        L.append('  { %d, %d, %d, %d, %d },  // %d %s' % (TYPES.index(t), cls, pw, ac, pp, idx[n], n))
        kon.append(json.dumps(ko(m['names']) or n, ensure_ascii=False))
        fl = [f for f in m['flavor_text_entries'] if f['language']['name'] == 'ko' and '사용할 수 없는' not in f['flavor_text']]
        d = fl[-1]['flavor_text'].replace('\n', ' ').replace('  ', ' ').strip() if fl else ''
        desc.append(json.dumps(d, ensure_ascii=False))
    L += ['};', '', 'static const char *const MV2_KO[MV2_N] = {']
    L += ['  %s,' % k for k in kon] + ['};', '', 'static const char *const MV2_DESC[MV2_N] = {']
    L += ['  %s,' % k for k in desc] + ['};', '']

    # subir de nivel en Cristal
    flat, off = [], [0]
    for d in mons:
        ls = set()
        for mv in d['moves']:
            for v in mv['version_group_details']:
                if v['version_group']['name'] == 'crystal' and v['move_learn_method']['name'] == 'level-up':
                    ls.add((v['level_learned_at'], idx[mv['move']['name']]))
        best = {}  # el mismo movimiento a nivel 1 (al evolucionar) y mas tarde: se queda el nivel mas alto
        for lv, mv in ls:
            best[mv] = max(best.get(mv, 0), lv)
        flat += sorted((lv, mv) for mv, lv in best.items())
        off.append(len(flat))
    L.append('// por especie dex (1..251): MV2_LS[MV2_LS_OFF[dex-1] .. MV2_LS_OFF[dex]) = { nivel, movimiento }')
    L.append('static const uint16_t MV2_LS_OFF[252] = { %s };' % ', '.join(map(str, off)))
    L.append('static const uint8_t MV2_LS[%d][2] = {' % len(flat))
    for i in range(0, len(flat), 8):
        L.append('  ' + ' '.join('{ %d, %d },' % p for p in flat[i:i + 8]))
    L += ['};', '']

    L.append('#define TM_N %d  // MT01..50 + MO01..07 de Cristal' % len(tms))
    L.append('static const uint8_t TM_MOVE[TM_N] = { %s };' % ', '.join(str(idx[n]) for _, n in tms))
    L.append('static const char *const TM_NAME[TM_N] = { %s };' %
             ', '.join('"%s%s"' % ('기술머신' if t[:2] == 'tm' else '비전머신', t[2:]) for t, _ in tms))
    L.append('static const uint8_t TM_BITS[252][8] = {')
    L.append('  { 0 },')
    for d in mons:
        b = [0] * 8
        for mv in d['moves']:
            n = mv['move']['name']
            if n in tmi and any(v['version_group']['name'] == 'crystal' and v['move_learn_method']['name'] == 'machine'
                                for v in mv['version_group_details']):
                b[tmi[n] // 8] |= 1 << (tmi[n] % 8)
        L.append('  { %s },  // %s' % (', '.join(str(x) for x in b), d['name']))
    L += ['};', '']
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(L))
    print('moves2_data.h: %d movimientos, %d de nivel, %d MT/MO' % (len(order), len(flat), len(tms)))


if __name__ == '__main__':
    main()

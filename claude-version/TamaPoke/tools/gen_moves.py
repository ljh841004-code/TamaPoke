#!/usr/bin/env python3
"""ko11.31: tabla de movimientos (moves_data.h) a partir de PokeAPI.

Ids (uint8_t):
  1..144  los ataques de tipo de siempre: 1 + tipo*9 + fase*3 + variante
          (nombres de i18n_ext.cpp; el TIPO es el nuestro, no el de PokeAPI)
  145     placaje
  146..   movimientos de estado (subir/bajar caracteristicas, estados alterados)
  255     forcejeo (sin PP)
Para cada uno: potencia, precision, PP, prioridad y efecto (estado alterado con
su probabilidad, cambio de caracteristica, drenaje/retroceso, golpes multiples,
critico alto, retroceso del rival). Y para cada especie (1..251), un mapa de bits
con los movimientos de la lista que aprende en los juegos (cualquier metodo y
generacion): el "como en el original".

Solo datos (numeros y nombres oficiales); la cache tools/moves_cache no se sube.
Uso: python3 gen_moves.py   -> ../moves_data.h
"""
import json
import os
import re
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, 'moves_cache')
SRC = os.path.join(HERE, '..', 'i18n_ext.cpp')
OUT = os.path.join(HERE, '..', 'moves_data.h')
API = 'https://pokeapi.co/api/v2/'

TYPES = ['normal', 'fire', 'water', 'grass', 'electric', 'ice', 'fighting', 'poison', 'ground',
         'psychic', 'bug', 'rock', 'ghost', 'dragon', 'dark', 'steel']
TYPE_FIX = {'flying': 'normal', 'fairy': 'normal'}   # no hay esos tipos en el juego

ALIAS = {
    'THUNDERSHOCK': 'thunder-shock', 'SOLARBEAM': 'solar-beam', 'DYNAMICPUNCH': 'dynamic-punch',
    'ANCIENTPOWER': 'ancient-power', 'FAINT ATTACK': 'feint-attack', 'DRAGONBREATH': 'dragon-breath',
    'THUNDERPUNCH': 'thunder-punch',
}

# movimientos de estado (orden = id 146..)
STATUS = ['swords-dance', 'iron-defense', 'agility', 'growth', 'harden', 'growl', 'tail-whip',
          'leer', 'string-shot', 'scary-face', 'screech', 'metal-sound', 'poison-powder', 'toxic',
          'poison-gas', 'thunder-wave', 'stun-spore', 'glare', 'sleep-powder', 'hypnosis', 'sing',
          'spore', 'will-o-wisp', 'confuse-ray', 'supersonic', 'sweet-kiss']

AIL = {'none': 0, 'poison': 1, 'burn': 2, 'paralysis': 3, 'sleep': 4, 'freeze': 5, 'confusion': 6}
STAT = {'attack': 0, 'special-attack': 0, 'defense': 1, 'special-defense': 1, 'speed': 2}

# potencia de los que en PokeAPI no tienen (dano fijo o variable)
FIXED = {'night-shade': 'lvl', 'seismic-toss': 'lvl', 'dragon-rage': 40, 'sonic-boom': 20,
         'psywave': 'lvl'}
POW_FIX = {'magnitude': 70, 'rollout': 60, 'fury-cutter': 60, 'present': 60, 'return': 70,
           'frustration': 70, 'low-kick': 60, 'hidden-power': 60,
           'heavy-slam': 80, 'fissure': 110}
ACC_FIX = {'fissure': 55}   # un golpe KO no encaja: fuerte y poco preciso

F_HICRIT, F_DRAIN, F_RECOIL, F_FIXLVL, F_FIX, F_RECHARGE, F_SELFCNF = 1, 2, 4, 8, 16, 64, 128
# pierden un turno: recargar despues (hiperrayo...) o cargar antes / golpe retardado (aqui: despues)
RECHARGE = {'hyper-beam', 'giga-impact', 'blast-burn', 'hydro-cannon', 'frenzy-plant', 'rock-wrecker'}
# ko12.8: los que en los juegos se CARGAN antes (solar-beam, future-sight, shadow-force, fly, dig...)
# ya no descansan despues: aqui son golpes normales (antes perdian el turno siguiente y salian
# peores que uno de 90 de potencia)
# se descontrolan unos turnos y acaban confusos (aqui: confusos al momento)
SELFCNF = {'petal-dance', 'outrage', 'thrash'}


def slug(n):
    return ALIAS.get(n) or re.sub(r'[^a-z0-9]+', '-', n.lower()).strip('-')


def get(path):
    dest = os.path.join(CACHE, path.strip('/').replace('/', '_') + '.json')
    if not os.path.exists(dest):
        os.makedirs(CACHE, exist_ok=True)
        for k in range(4):
            try:
                req = urllib.request.Request(API + path, headers={'User-Agent': 'TamaPoke-gen_moves'})
                data = urllib.request.urlopen(req, timeout=30).read()
                break
            except Exception as e:
                if k == 3:
                    raise
                time.sleep(2 ** k)
        open(dest, 'wb').write(data)
    return json.load(open(dest))


def type_moves():
    src = open(SRC, encoding='utf-8').read()

    def block(tag):
        i = src.index(tag)
        return re.findall(r'"([^"]*)"', src[i:src.index('};', i)])
    m1, m2 = block('MOVES_EN[3][PT_COUNT] ='), block('MOVES2_EN[3][PT_COUNT][2] =')
    out = {}
    for t in range(16):
        for s in range(3):
            out[1 + t * 9 + s * 3 + 0] = m1[s * 16 + t]
            out[1 + t * 9 + s * 3 + 1] = m2[(s * 16 + t) * 2]
            out[1 + t * 9 + s * 3 + 2] = m2[(s * 16 + t) * 2 + 1]
    return out


def ko_name(d):
    for n in d['names']:
        if n['language']['name'] == 'ko':
            return n['name']
    return d['name']


def mdef(d, our_type=None):
    meta = d.get('meta') or {}
    s = d['name']
    t = our_type
    if t is None:
        pt = TYPE_FIX.get(d['type']['name'], d['type']['name'])
        t = TYPES.index(pt)
    pow_ = d['power'] or 0
    flags = 0
    if s in FIXED:
        flags |= F_FIXLVL if FIXED[s] == 'lvl' else F_FIX
        pow_ = 0 if FIXED[s] == 'lvl' else FIXED[s]
    elif s in POW_FIX:
        pow_ = POW_FIX[s]
    if s in RECHARGE:
        flags |= F_RECHARGE
    if s in SELFCNF:
        flags |= F_SELFCNF
    hits = 1
    if meta.get('min_hits'):
        lo, hi = meta['min_hits'], meta['max_hits']
        hits = 3 if hi >= 5 else hi  # 2-5 golpes: de media ~3
    acc = ACC_FIX.get(s) or d['accuracy'] or 100
    if (meta.get('crit_rate') or 0) > 0:
        flags |= F_HICRIT
    drain = meta.get('drain') or 0
    if drain > 0:
        flags |= F_DRAIN
    elif drain < 0:
        flags |= F_RECOIL
    ail = AIL.get((meta.get('ailment') or {}).get('name', 'none'), 0)
    ail_ch = meta.get('ailment_chance') or 0
    cat = d['damage_class']['name']
    if ail and cat == 'status':
        ail_ch = 0  # 0 = siempre (si acierta)
    st_idx, st_d, st_self, st_ch = 0, 0, 0, 0
    if d['stat_changes']:
        sc = d['stat_changes'][0]
        if sc['stat']['name'] in STAT:
            st_idx, st_d = STAT[sc['stat']['name']], sc['change']
            st_self = 1 if (d['target']['name'] in ('user', 'users-field', 'user-and-allies') or
                            (meta.get('category') or {}).get('name') == 'damage-raise') else 0
            st_ch = meta.get('stat_chance') or 0
            if cat == 'status':
                st_ch = 0
    flinch = meta.get('flinch_chance') or 0
    return dict(type=t, pow=min(pow_ * hits, 250), acc=acc, pp=d['pp'] or 10, prio=d['priority'],
                flags=flags, ail=ail, ailch=ail_ch, st=st_idx, std=st_d, stself=st_self, stch=st_ch,
                flinch=flinch, drain=abs(drain), status=(cat == 'status'), special=(cat == 'special'))


def main():
    tm = type_moves()
    moves = {}
    names = {}
    for i in range(1, 145):
        d = get('move/' + slug(tm[i]))
        moves[i] = mdef(d, (i - 1) // 9)
        names[i] = (d['name'], None, None)
    d = get('move/tackle')
    moves[145] = mdef(d)
    names[145] = ('tackle', ko_name(d), 'TACKLE')
    for k, s in enumerate(STATUS):
        d = get('move/' + s)
        moves[146 + k] = mdef(d)
        names[146 + k] = (s, ko_name(d), s.replace('-', ' ').upper())
    nmov = 146 + len(STATUS)
    slug_id = {names[i][0]: i for i in moves}
    nbytes = (nmov + 7) // 8
    learn, learn_lv = [], []
    for dex in range(1, 252):
        p = get('pokemon/%d' % dex)
        bits, lv = [0] * nbytes, [0] * nbytes
        for m in p['moves']:
            i = slug_id.get(m['move']['name'])
            if i:
                bits[i // 8] |= 1 << (i % 8)
                if any(v['move_learn_method']['name'] == 'level-up' for v in m['version_group_details']):
                    lv[i // 8] |= 1 << (i % 8)
        learn.append(bits)
        learn_lv.append(lv)
    L = ['// generado por tools/gen_moves.py (PokeAPI): no editar a mano', '#pragma once',
         '#include <stdint.h>', '',
         '#define MOVE_N %d          // ids 1..MOVE_N-1 (0 = vacio)' % nmov,
         '#define MOVE_TACKLE 145', '#define MOVE_STATUS0 146', '#define MOVE_STRUGGLE 255',
         '#define MOVE_LEARN_BYTES %d' % nbytes, '',
         '#include "battle.h"  // MoveDef y MF_*', '',
         'static const MoveDef MOVE_TBL[MOVE_N] = {',
         '  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },  // 0']
    for i in range(1, nmov):
        m = moves[i]
        fl = m['flags'] | (32 if m['status'] else 0)
        L.append('  { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d },  // %d %s' % (
            m['type'], m['pow'], m['acc'], m['pp'], m['prio'], fl, m['ail'], m['ailch'], m['st'],
            m['std'], m['stself'], m['stch'], m['flinch'], m['drain'], i, names[i][0]))
    L.append('};')
    L.append('')
    L.append('// ko12.8: especiales (ataque especial contra defensa especial); bit i = id i')
    sp = [0] * ((nmov + 7) // 8)
    for i in range(1, nmov):
        if moves[i].get('special'):
            sp[i // 8] |= 1 << (i % 8)
    L.append('static const uint8_t MOVE_SPECIAL[%d] = { %s };' % (len(sp), ', '.join('0x%02x' % b for b in sp)))
    L.append('')
    L.append('// nombres de los que no son de tipo (145.. : placaje y los de estado)')
    L.append('static const char *const MOVE_X_KO[MOVE_N - MOVE_TACKLE] = {')
    L += ['  "%s",' % names[i][1] for i in range(145, nmov)]
    L.append('};')
    L.append('static const char *const MOVE_X_EN[MOVE_N - MOVE_TACKLE] = {')
    L += ['  "%s",' % names[i][2] for i in range(145, nmov)]
    L.append('};')
    L.append('')
    L.append('// lo que cada especie aprende en los juegos (bit i = id i)')
    L.append('static const uint8_t MOVE_LEARN[252][MOVE_LEARN_BYTES] = {')
    L.append('  { %s },' % ', '.join(['0'] * nbytes))
    for dex, bits in enumerate(learn, 1):
        L.append('  { %s },  // %d' % (', '.join('0x%02x' % b for b in bits), dex))
    L.append('};')
    L.append('// solo los que aprende subiendo de nivel (los rivales llevan de estos)')
    L.append('static const uint8_t MOVE_LEARN_LV[252][MOVE_LEARN_BYTES] = {')
    L.append('  { %s },' % ', '.join(['0'] * nbytes))
    for dex, bits in enumerate(learn_lv, 1):
        L.append('  { %s },  // %d' % (', '.join('0x%02x' % b for b in bits), dex))
    L.append('};')
    open(OUT, 'w').write('\n'.join(L) + '\n')
    nz = [moves[i] for i in range(1, 146) if moves[i]['pow'] == 0 and not moves[i]['flags'] & (F_FIX | F_FIXLVL)]
    print('ok: %d movimientos, %s' % (nmov - 1, OUT))
    if nz:
        print('sin potencia:', nz)


if __name__ == '__main__':
    main()

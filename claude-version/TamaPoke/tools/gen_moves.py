#!/usr/bin/env python3
"""ko12.10: tabla de movimientos (moves_data.h) "como en el original": los 251 de Pokemon Cristal.

  python3 gen_moves.py <cache>      -> ../moves_data.h

<cache> = carpeta con los JSON de PokeAPI (se descargan si faltan; ver tools/gen_moves2.py antes):
  pk/NNN.json      /pokemon/NNN        (1..251)
  pkm/<move>.json  /move/<nombre>
  pkm/mach/N.json  /machine/N          (MT/MO de Cristal)

Ids (uint8_t) = id de PokeAPI (1..251, el mismo orden que en los juegos: 33 placaje, 165 forcejeo).
Para cada uno, con los valores de 2a generacion (past_values) y algunos arreglos a mano de lo que
PokeAPI solo da con valores modernos:
  tipo (16 nuestros + 16 volador), potencia, precision (0 = no falla), PP, prioridad, efecto
  (estado alterado y su %, hasta dos cambios de caracteristica, retroceso del rival, drenaje,
  retroceso propio, golpes multiples, critico alto), MX_* = efecto especial que hace el motor
  (battle.cpp) y FAM = el dibujo de reserva (ui_moves.ino) cuando no hay animacion en la SD.
Y por especie: lo que aprende subiendo de nivel en Cristal (nivel, movimiento), lo que puede
aprender (nivel + MT/MO + tutor de Cristal + lo de sus preevoluciones), y las 57 MT/MO.
MOVE_OLD2NEW: los ids de antes (ko11.31..ko12.9: ataques de tipo por fase) -> el original mas parecido.

Solo datos (numeros, nombres y descripciones oficiales); la cache no se sube.
"""
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'moves_data.h')
API = 'https://pokeapi.co/api/v2/'
TYPES = ['normal', 'fire', 'water', 'grass', 'electric', 'ice', 'fighting', 'poison', 'ground',
         'psychic', 'bug', 'rock', 'ghost', 'dragon', 'dark', 'steel', 'flying']
SPECIAL_T = {'fire', 'water', 'grass', 'electric', 'ice', 'psychic', 'dragon', 'dark'}  # 2a gen: por tipo
CRYSTAL = 4
NMOVES = 251

AIL = {'none': 0, 'poison': 1, 'burn': 2, 'paralysis': 3, 'sleep': 4, 'freeze': 5, 'confusion': 6}
STAT = {'attack': 0, 'defense': 1, 'speed': 2, 'special-attack': 3, 'special-defense': 4,
        'accuracy': 5, 'evasion': 6}
F_HICRIT, F_DRAIN, F_RECOIL, F_SELF, F_STATUS, F_RECHARGE, F_NOMISS, F_THAW = 1, 2, 4, 8, 32, 64, 128, 16

# efecto especial -> movimientos (los nombres MX_* son el enum de battle.h)
MX = {
    'MX_OHKO': ['guillotine', 'horn-drill', 'fissure'],
    'MX_SONICBOOM': ['sonic-boom'], 'MX_DRAGONRAGE': ['dragon-rage'],
    'MX_LEVEL': ['seismic-toss', 'night-shade'], 'MX_PSYWAVE': ['psywave'], 'MX_SUPERFANG': ['super-fang'],
    'MX_COUNTER': ['counter'], 'MX_MIRRORCOAT': ['mirror-coat'], 'MX_BIDE': ['bide'],
    'MX_CHARGE': ['razor-wind', 'skull-bash', 'sky-attack'], 'MX_SOLAR': ['solar-beam'],
    'MX_FLY': ['fly'], 'MX_DIG': ['dig'],
    'MX_RAMPAGE': ['thrash', 'petal-dance', 'outrage'],
    'MX_TRAP': ['bind', 'wrap', 'fire-spin', 'clamp', 'whirlpool'],
    'MX_CRASH': ['jump-kick', 'high-jump-kick'], 'MX_SELFKO': ['self-destruct', 'explosion'],
    'MX_RAGE': ['rage'], 'MX_FURYCUTTER': ['fury-cutter'], 'MX_ROLLOUT': ['rollout'],
    'MX_FLAIL': ['flail', 'reversal'], 'MX_RETURN': ['return'], 'MX_FRUSTRATION': ['frustration'],
    'MX_PRESENT': ['present'], 'MX_MAGNITUDE': ['magnitude'], 'MX_HIDDENPOWER': ['hidden-power'],
    'MX_FALSESWIPE': ['false-swipe'], 'MX_PURSUIT': ['pursuit'], 'MX_SNORE': ['snore'],
    'MX_SLEEPTALK': ['sleep-talk'], 'MX_DREAMEATER': ['dream-eater'], 'MX_TRIATTACK': ['tri-attack'],
    'MX_FUTURESIGHT': ['future-sight'], 'MX_THUNDER': ['thunder'], 'MX_RAPIDSPIN': ['rapid-spin'],
    'MX_ALLUP': ['ancient-power'], 'MX_TRIPLEKICK': ['triple-kick'], 'MX_BEATUP': ['beat-up'],
    'MX_TWISTER': ['gust', 'twister'], 'MX_QUAKE': ['earthquake'], 'MX_STRUGGLE': ['struggle'],
    'MX_PAYDAY': ['pay-day'], 'MX_THIEF': ['thief'], 'MX_STOMP': ['stomp'],
    # estado
    'MX_TOXIC': ['toxic'], 'MX_HEAL': ['recover', 'soft-boiled', 'milk-drink'],
    'MX_HEALWX': ['morning-sun', 'synthesis', 'moonlight'], 'MX_REST': ['rest'],
    'MX_LEECHSEED': ['leech-seed'], 'MX_DISABLE': ['disable'], 'MX_MIST': ['mist'],
    'MX_LIGHTSCREEN': ['light-screen'], 'MX_REFLECT': ['reflect'], 'MX_HAZE': ['haze'],
    'MX_FOCUS': ['focus-energy'], 'MX_METRONOME': ['metronome'], 'MX_MIRRORMOVE': ['mirror-move'],
    'MX_TRANSFORM': ['transform'], 'MX_SPLASH': ['splash'], 'MX_CONVERSION': ['conversion'],
    'MX_CONVERSION2': ['conversion-2'], 'MX_SUBSTITUTE': ['substitute'], 'MX_MIMIC': ['mimic', 'sketch'],
    'MX_MEANLOOK': ['spider-web', 'mean-look'], 'MX_LOCKON': ['mind-reader', 'lock-on'],
    'MX_NIGHTMARE': ['nightmare'], 'MX_CURSE': ['curse'], 'MX_SPITE': ['spite'],
    'MX_PROTECT': ['protect', 'detect'], 'MX_ENDURE': ['endure'], 'MX_BELLYDRUM': ['belly-drum'],
    'MX_SPIKES': ['spikes'], 'MX_FORESIGHT': ['foresight'], 'MX_DESTINYBOND': ['destiny-bond'],
    'MX_PERISH': ['perish-song'], 'MX_SANDSTORM': ['sandstorm'], 'MX_RAINDANCE': ['rain-dance'],
    'MX_SUNNYDAY': ['sunny-day'], 'MX_SWAGGER': ['swagger'], 'MX_ATTRACT': ['attract'],
    'MX_HEALBELL': ['heal-bell'], 'MX_SAFEGUARD': ['safeguard'], 'MX_PAINSPLIT': ['pain-split'],
    'MX_BATONPASS': ['baton-pass'], 'MX_ENCORE': ['encore'], 'MX_PSYCHUP': ['psych-up'],
    'MX_ROAR': ['roar', 'whirlwind'], 'MX_TELEPORT': ['teleport'],
}
MXOF = {n: k for k, v in MX.items() for n in v}

# dibujo de reserva (FX2_* + 1 de ui_moves.ino; 0 = el efecto de su tipo)
FAM = {
    'FX2_PUNCH': ['comet-punch', 'mega-punch', 'fire-punch', 'ice-punch', 'thunder-punch', 'dizzy-punch',
                  'mach-punch', 'dynamic-punch', 'pound', 'double-slap', 'karate-chop', 'cross-chop',
                  'counter', 'bide', 'beat-up', 'rock-smash'],
    'FX2_KICK': ['double-kick', 'mega-kick', 'jump-kick', 'rolling-kick', 'high-jump-kick', 'low-kick',
                 'triple-kick', 'stomp'],
    'FX2_SLASH': ['scratch', 'vice-grip', 'guillotine', 'cut', 'slash', 'fury-swipes', 'false-swipe',
                  'fury-cutter', 'metal-claw', 'crabhammer', 'razor-wind', 'thief', 'fury-attack'],
    'FX2_BITE': ['bite', 'crunch', 'hyper-fang', 'super-fang'],
    'FX2_BODY': ['tackle', 'body-slam', 'take-down', 'double-edge', 'headbutt', 'skull-bash', 'slam',
                 'strength', 'quick-attack', 'extreme-speed', 'rage', 'thrash', 'return', 'frustration',
                 'flail', 'reversal', 'submission', 'seismic-toss', 'vital-throw', 'pursuit', 'feint-attack',
                 'horn-attack', 'rapid-spin', 'struggle', 'self-destruct', 'pay-day', 'present', 'horn-drill',
                 'spike-cannon', 'barrage', 'egg-bomb', 'wing-attack', 'sky-attack', 'rollout', 'steel-wing',
                 'iron-tail'],
    'FX2_SOUND': ['growl', 'roar', 'sing', 'supersonic', 'screech', 'perish-song', 'snore', 'heal-bell',
                  'sonic-boom', 'attract', 'sweet-kiss', 'lovely-kiss'],
    'FX2_POWDER': ['poison-powder', 'stun-spore', 'sleep-powder', 'spore', 'cotton-spore', 'sand-attack',
                   'sweet-scent', 'string-shot', 'spider-web', 'poison-gas', 'spikes'],
    'FX2_STATUP': ['swords-dance', 'growth', 'meditate', 'agility', 'harden', 'defense-curl', 'withdraw',
                   'sharpen', 'double-team', 'minimize', 'focus-energy', 'belly-drum', 'acid-armor',
                   'amnesia', 'barrier', 'psych-up', 'curse', 'conversion', 'conversion-2', 'mind-reader',
                   'lock-on', 'baton-pass', 'transform', 'mimic', 'sketch', 'metronome', 'mirror-move',
                   'splash', 'teleport', 'sleep-talk', 'rain-dance', 'sunny-day', 'sandstorm', 'haze'],
    'FX2_STATDOWN': ['tail-whip', 'leer', 'scary-face', 'charm', 'flash', 'kinesis', 'swagger', 'smokescreen',
                     'glare', 'mean-look', 'foresight', 'disable', 'encore', 'spite', 'confuse-ray',
                     'hypnosis', 'nightmare', 'destiny-bond'],
    'FX2_HEAL': ['recover', 'soft-boiled', 'milk-drink', 'rest', 'morning-sun', 'synthesis', 'moonlight',
                 'pain-split'],
    'FX2_BARRIER': ['protect', 'detect', 'endure', 'reflect', 'light-screen', 'mist', 'safeguard',
                    'substitute'],
    'FX2_DRAIN': ['absorb', 'mega-drain', 'giga-drain', 'leech-life', 'dream-eater', 'leech-seed'],
}
FAMS = ['FX2_PUNCH', 'FX2_KICK', 'FX2_SLASH', 'FX2_BITE', 'FX2_BODY', 'FX2_SOUND', 'FX2_POWDER',
        'FX2_STATUP', 'FX2_STATDOWN', 'FX2_HEAL', 'FX2_BARRIER', 'FX2_DRAIN']
FAMOF = {n: FAMS.index(k) + 1 for k, v in FAM.items() for n in v}

# 2a generacion: lo que PokeAPI solo da con valores de ahora (efectos secundarios)
G2 = {
    'double-edge': dict(drain=-25), 'low-kick': dict(power=50, flinch=30),
    'acid': dict(stats=[('defense', -1)], chance=10), 'crunch': dict(stats=[('special-defense', -1)]),
    'growth': dict(stats=[('special-attack', 1)]), 'sweet-scent': dict(stats=[('evasion', -1)]),
    'rapid-spin': dict(stats=[]), 'waterfall': dict(flinch=0), 'sky-attack': dict(flinch=0, crit=0),
    'rock-smash': dict(power=20), 'curse': dict(type='normal'), 'razor-wind': dict(crit=1),
    'struggle': dict(drain=-25), 'present': dict(power=0), 'magnitude': dict(power=0),
    'hidden-power': dict(power=0), 'return': dict(power=0), 'frustration': dict(power=0),
    'flail': dict(power=0), 'reversal': dict(power=0), 'beat-up': dict(power=10, hits=(2, 4)),
    'ancient-power': dict(stats=[]), 'swagger': dict(stats=[]), 'tri-attack': dict(ail='none', ailCh=20),
    'skull-bash': dict(stats=[('defense', 1)], chance=100),
}

# ids de antes (1..171): nombre de entonces -> el original de Cristal mas parecido
OLD = ('headbutt quick-attack scratch body-slam slash take-down hyper-beam giga-impact double-edge ember '
       'fire-spin fire-punch flamethrower heat-wave flame-wheel fire-blast blast-burn overheat water-gun bubble '
       'aqua-jet water-pulse surf aqua-tail hydro-pump hydro-cannon waterfall vine-whip absorb bullet-seed '
       'razor-leaf giga-drain petal-dance solar-beam frenzy-plant leaf-storm thunder-shock spark thunder-punch '
       'thunderbolt wild-charge discharge thunder volt-tackle zap-cannon icy-wind ice-shard ice-punch ice-beam '
       'ice-fang aurora-beam blizzard icicle-crash freeze-dry karate-chop low-kick rock-smash cross-chop '
       'double-kick aura-sphere dynamic-punch close-combat focus-blast poison-sting acid poison-jab sludge '
       'cross-poison sludge-wave sludge-bomb gunk-shot venoshock mud-slap bone-club mud-shot magnitude '
       'bonemerang bulldoze earthquake fissure earth-power confusion psywave zen-headbutt psybeam psycho-cut '
       'future-sight psychic psycho-boost psyshock pin-missile bug-bite leech-life fury-cutter x-scissor '
       'signal-beam megahorn bug-buzz lunge rock-throw rock-tomb rollout ancient-power power-gem rock-blast '
       'rock-slide stone-edge head-smash lick astonish shadow-punch night-shade shadow-claw ominous-wind '
       'shadow-ball shadow-force hex twister dragon-breath dragon-tail dragon-rage dragon-claw dragon-pulse '
       'outrage dragon-rush draco-meteor bite thief sucker-punch feint-attack dark-pulse pursuit crunch '
       'foul-play night-daze metal-claw bullet-punch mirror-shot steel-wing iron-head flash-cannon iron-tail '
       'meteor-mash heavy-slam tackle swords-dance iron-defense agility growth harden growl tail-whip leer '
       'string-shot scary-face screech metal-sound poison-powder toxic poison-gas thunder-wave stun-spore glare '
       'sleep-powder hypnosis sing spore will-o-wisp confuse-ray supersonic sweet-kiss').split()
OLD_FIX = {
    'giga-impact': 'hyper-beam', 'heat-wave': 'flamethrower', 'blast-burn': 'fire-blast', 'overheat': 'fire-blast',
    'aqua-jet': 'water-gun', 'water-pulse': 'bubble-beam', 'aqua-tail': 'waterfall', 'hydro-cannon': 'hydro-pump',
    'bullet-seed': 'razor-leaf', 'frenzy-plant': 'solar-beam', 'leaf-storm': 'petal-dance', 'wild-charge': 'spark',
    'discharge': 'thunderbolt', 'volt-tackle': 'thunder', 'ice-shard': 'powder-snow', 'ice-fang': 'ice-punch',
    'icicle-crash': 'blizzard', 'freeze-dry': 'ice-beam', 'aura-sphere': 'cross-chop', 'close-combat': 'cross-chop',
    'focus-blast': 'dynamic-punch', 'poison-jab': 'sludge', 'cross-poison': 'sludge', 'sludge-wave': 'sludge-bomb',
    'gunk-shot': 'sludge-bomb', 'venoshock': 'sludge', 'mud-shot': 'mud-slap', 'bulldoze': 'magnitude',
    'earth-power': 'earthquake', 'zen-headbutt': 'psybeam', 'psycho-cut': 'psybeam', 'psycho-boost': 'psychic',
    'psyshock': 'psychic', 'bug-bite': 'fury-cutter', 'x-scissor': 'fury-cutter', 'signal-beam': 'pin-missile',
    'bug-buzz': 'megahorn', 'lunge': 'leech-life', 'rock-tomb': 'rock-throw', 'power-gem': 'ancient-power',
    'rock-blast': 'rock-slide', 'stone-edge': 'rock-slide', 'head-smash': 'rock-slide', 'astonish': 'lick',
    'shadow-punch': 'shadow-ball', 'shadow-claw': 'shadow-ball', 'ominous-wind': 'shadow-ball',
    'shadow-force': 'shadow-ball', 'hex': 'night-shade', 'dragon-tail': 'twister', 'dragon-claw': 'dragon-breath',
    'dragon-pulse': 'dragon-breath', 'dragon-rush': 'outrage', 'draco-meteor': 'outrage',
    'sucker-punch': 'feint-attack', 'dark-pulse': 'crunch', 'foul-play': 'crunch', 'night-daze': 'crunch',
    'bullet-punch': 'metal-claw', 'mirror-shot': 'steel-wing', 'iron-head': 'iron-tail', 'flash-cannon': 'iron-tail',
    'meteor-mash': 'iron-tail', 'heavy-slam': 'iron-tail', 'iron-defense': 'harden', 'metal-sound': 'screech',
    'will-o-wisp': 'ember',
}
STRUGGLE = {'id': 165, 'name': 'struggle', 'type': {'name': 'normal'}, 'power': 50, 'accuracy': None, 'pp': 1,
            'priority': 0, 'damage_class': {'name': 'physical'}, 'target': {'name': 'selected-pokemon'},
            'meta': {'drain': -25}, 'stat_changes': [], 'past_values': [], 'machines': [],
            'names': [{'language': {'name': 'ko'}, 'name': '발버둥'}, {'language': {'name': 'en'}, 'name': 'Struggle'}],
            'flavor_text_entries': [
                {'language': {'name': 'ko'}, 'flavor_text': '쓸 수 있는 기술이 없을 때 마구 날뛰어 공격한다. 자신도 데미지를 입는다.'},
                {'language': {'name': 'en'}, 'flavor_text': 'Used only if all PP are gone. It also hurts the user a little.'}]}


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
    best = None
    for pv in m.get('past_values', []):
        v = vg_id(pv['version_group'])
        if v > CRYSTAL and pv.get(key) is not None and (best is None or v < best[0]):
            best = (v, pv[key])
    return best[1] if best else cur


def lang(entries, code, field='name'):
    for e in entries:
        if e['language']['name'] == code:
            return e[field]
    return None


def flavor(m, code):
    bad = ('사용할 수 없는', 'cannot be used', 'can no longer be used')
    fl = [f for f in m['flavor_text_entries'] if f['language']['name'] == code and not any(b in f['flavor_text'] for b in bad)]
    if not fl:
        return ''
    return ' '.join(fl[-1]['flavor_text'].replace('\n', ' ').replace('­', '').split())


def c(s):
    return json.dumps(s, ensure_ascii=False)


def mdef(m):
    n = m['name']
    fx = G2.get(n, {})
    me = m.get('meta') or {}
    t = gen2(m, 'type', m['type'])
    t = t['name'] if isinstance(t, dict) else t
    t = fx.get('type', 'normal' if t in ('fairy', 'unknown', 'shadow') else t)
    status = m['damage_class']['name'] == 'status'
    pw = fx.get('power', gen2(m, 'power', m['power']) or 0)
    ac = gen2(m, 'accuracy', m['accuracy']) or 0
    pp = gen2(m, 'pp', m['pp']) or 0
    ech = gen2(m, 'effect_chance', None)
    flags = 0
    tgt = m['target']['name']
    if status:
        flags |= F_STATUS
    if tgt in ('user', 'users-field', 'entire-field', 'user-or-ally', 'opponents-field', 'all-other-pokemon'):
        flags |= F_SELF  # no le afecta el protegerse / sustituto del rival
    if n in ('perish-song', 'haze', 'heal-bell', 'curse', 'transform', 'mirror-move', 'metronome', 'sleep-talk'):
        flags |= F_SELF
    if not ac and not status:
        flags |= F_NOMISS
    if fx.get('crit', me.get('crit_rate', 0)):
        flags |= F_HICRIT
    if n in ('flame-wheel', 'sacred-fire'):
        flags |= F_THAW
    if n == 'hyper-beam':
        flags |= F_RECHARGE
    drain = fx.get('drain', me.get('drain', 0) or 0)
    dr = 0
    if drain > 0:
        flags |= F_DRAIN
        dr = drain
    elif drain < 0:
        flags |= F_RECOIL
        dr = -drain
    ail = AIL.get(fx.get('ail', (me.get('ailment') or {}).get('name', 'none')), 0)
    ailCh = fx.get('ailCh', me.get('ailment_chance', 0) or 0)
    if ail and not status and ech and not ailCh:
        ailCh = ech
    if ail and not status and ailCh == 0:
        ailCh = 100
    if status:
        ailCh = 0
    if n in ('attract', 'nightmare', 'foresight', 'perish-song', 'disable', 'leech-seed'):
        ail = 0
    if n == 'swagger':
        ail = AIL['confusion']
    stats = fx.get('stats', [(s['stat']['name'], s['change']) for s in m['stat_changes']])
    stCh = fx.get('chance', me.get('stat_chance', 0) or 0)
    if stats and not status and not stCh:
        stCh = ech or 100
    if status:
        stCh = 0
    stSelf = 1 if (tgt == 'user' or me.get('category', {}).get('name') == 'damage-raise' or n == 'skull-bash') else 0
    st = [(STAT[k], v) for k, v in stats][:2]
    st += [(0, 0)] * (2 - len(st))
    flinch = fx.get('flinch', me.get('flinch_chance', 0) or 0)
    hits = fx.get('hits', (me.get('min_hits') or 0, me.get('max_hits') or 0))
    hb = (hits[1] << 4 | hits[0]) if hits[1] else 0
    mx = MXOF.get(n, 'MX_NONE')
    fam = FAMOF.get(n, 0)
    return ('{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, 0x%02x, %s, %d }' %
            (TYPES.index(t), pw, ac, pp, m['priority'], flags, ail, ailCh, st[0][0], st[0][1], stSelf, stCh,
             flinch, dr, st[1][0], st[1][1], hb, mx, fam)), t, status, pw


def main():
    cache = sys.argv[1]
    mons = [fetch('pokemon/%d' % i, os.path.join(cache, 'pk', '%03d.json' % i)) for i in range(1, 252)]
    byname = {}
    for i in range(1, NMOVES + 1):
        if i == 165:
            byname['struggle'] = STRUGGLE
            continue
        fn = None
        for d in mons:
            for mv in d['moves']:
                if mv['move']['url'].rstrip('/').endswith('/%d' % i):
                    fn = mv['move']['name']
                    break
            if fn:
                break
        m = fetch('move/%d' % i, os.path.join(cache, 'pkm', fn + '.json'))
        assert m['id'] == i, (i, m['id'])
        byname[m['name']] = m
    order = sorted(byname, key=lambda n: byname[n]['id'])
    idx = {n: byname[n]['id'] for n in order}
    tms = []
    for n in order:
        for x in byname[n].get('machines', []):
            if x['version_group']['name'] == 'crystal':
                mid = x['machine']['url'].rstrip('/').split('/')[-1]
                mc = fetch('machine/' + mid, os.path.join(cache, 'pkm', 'mach', mid + '.json'))
                tms.append((mc['item']['name'], n))
    tms.sort(key=lambda t: (t[0][:2] == 'hm', int(t[0][2:])))
    tmi = {n: i for i, (_, n) in enumerate(tms)}

    L = ['// generado por tools/gen_moves.py (PokeAPI, Pokemon Cristal): no editar a mano', '#pragma once',
         '#include <stdint.h>', '', '#include "battle.h"  // MoveDef, MF_*, MX_*', '',
         '#define MOVE_N %d          // ids 1..%d = los de PokeAPI (0 = vacio)' % (NMOVES + 1, NMOVES),
         '#define MOVE_LEARN_BYTES 32', '',
         '// tipo, pot, prec (0 = no falla), pp, prio, flags, estado, %, carac, cambio, a si, %, retroceso,',
         '// drenaje/retroceso propio %, carac2, cambio2, golpes (max<<4|min), efecto especial, dibujo',
         'static const MoveDef MOVE_TBL[MOVE_N] = {',
         '  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MX_NONE, 0 },  // 0']
    kon, enn, kod, end = ['""'], ['""'], ['""'], ['""']
    for n in order:
        row, t, status, pw = mdef(byname[n])
        L.append('  %s,  // %d %s' % (row, idx[n], n))
        m = byname[n]
        kon.append(c(lang(m['names'], 'ko') or n))
        enn.append(c(lang(m['names'], 'en') or n))
        kod.append(c(flavor(m, 'ko')))
        end.append(c(flavor(m, 'en')))
    L += ['};', '']
    for nm, arr in (('MOVE_KO', kon), ('MOVE_EN', enn), ('MOVE_DESC_KO', kod), ('MOVE_DESC_EN', end)):
        L.append('static const char *const %s[MOVE_N] = {' % nm)
        L += ['  %s,' % k for k in arr]
        L += ['};', '']

    # subir de nivel en Cristal
    def lvset(d):
        ls = set()
        for mv in d['moves']:
            for v in mv['version_group_details']:
                if v['version_group']['name'] == 'crystal' and v['move_learn_method']['name'] == 'level-up':
                    ls.add((v['level_learned_at'], idx[mv['move']['name']]))
        return ls

    def methods(d, which):
        s = set()
        for mv in d['moves']:
            if mv['move']['name'] not in idx:
                continue
            for v in mv['version_group_details']:
                if v['version_group']['name'] == 'crystal' and v['move_learn_method']['name'] in which:
                    s.add(idx[mv['move']['name']])
        return s

    prevo = {}
    for i in range(1, 252):
        sp = fetch('pokemon-species/%d' % i, os.path.join(cache, 'pks', '%03d.json' % i))
        if sp.get('evolves_from_species'):
            pv = int(sp['evolves_from_species']['url'].rstrip('/').split('/')[-1])
            if pv <= 251:  # las crias de 4a gen (Munchlax...) no estan
                prevo[i] = pv

    flat, off = [], [0]
    for d in mons:
        flat += sorted(lvset(d))  # todos (esquema de Smeargle a 1, 11, 21...; fortaleza de Kakuna a 1 y 7)
        off.append(len(flat))
    L.append('// por especie dex (1..251): MOVE_LS[MOVE_LS_OFF[dex-1] .. MOVE_LS_OFF[dex]) = { nivel, movimiento }')
    L.append('static const uint16_t MOVE_LS_OFF[252] = { %s };' % ', '.join(map(str, off)))
    L.append('static const uint8_t MOVE_LS[%d][2] = {' % len(flat))
    for i in range(0, len(flat), 8):
        L.append('  ' + ' '.join('{ %d, %d },' % p for p in flat[i:i + 8]))
    L += ['};', '']

    # todo lo que puede aprender (nivel + MT/MO + tutor + lo de sus preevoluciones): bit i = id i
    L.append('static const uint8_t MOVE_LEARN[252][MOVE_LEARN_BYTES] = {')
    L.append('  { 0 },')
    for i, d in enumerate(mons, 1):
        s = {mv for _, mv in lvset(d)} | methods(d, ('machine', 'tutor'))
        p = prevo.get(i)
        while p:
            s |= {mv for _, mv in lvset(mons[p - 1])}
            p = prevo.get(p)
        b = [0] * 32
        for mv in s:
            b[mv // 8] |= 1 << (mv % 8)
        L.append('  { %s },  // %d %s' % (', '.join('0x%02x' % x for x in b), i, d['name']))
    L += ['};', '']

    L.append('#define TM_N %d  // MT01..50 + MO01..07 de Cristal' % len(tms))
    L.append('#define TM_HM0 50  // desde aqui, MO (no se gastan)')
    L.append('static const uint8_t TM_MOVE[TM_N] = { %s };' % ', '.join(str(idx[n]) for _, n in tms))
    L.append('static const char *const TM_NAME[TM_N] = { %s };' %
             ', '.join('"%s%s"' % ('기술머신' if t[:2] == 'tm' else '비전머신', t[2:]) for t, _ in tms))
    L.append('static const char *const TM_NAME_EN[TM_N] = { %s };' %
             ', '.join('"%s%s"' % (t[:2].upper(), t[2:]) for t, _ in tms))
    L.append('static const uint8_t TM_BITS[252][8] = {')
    L.append('  { 0 },')
    for d in mons:
        b = [0] * 8
        for n in methods(d, ('machine',)):
            nm = order[[idx[o] for o in order].index(n)]
            if nm in tmi:
                b[tmi[nm] // 8] |= 1 << (tmi[nm] % 8)
        L.append('  { %s },  // %s' % (', '.join(str(x) for x in b), d['name']))
    L += ['};', '']

    old = [0]
    for n in OLD:
        k = OLD_FIX.get(n, n)
        old.append(idx[k])
    assert len(old) == 172
    L.append('// ids de antes (ko11.31..ko12.9, 1..171) -> el original mas parecido (0 = vacio)')
    L.append('#define MOVE_OLD_N 172')
    L.append('static const uint8_t MOVE_OLD2NEW[MOVE_OLD_N] = { %s };' % ', '.join(map(str, old)))
    L.append('')
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(L))
    print('moves_data.h: %d movimientos, %d de nivel, %d MT/MO' % (len(order), len(flat), len(tms)))


if __name__ == '__main__':
    main()

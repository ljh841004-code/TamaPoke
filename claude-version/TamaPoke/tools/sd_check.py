#!/usr/bin/env python3
"""ko12.8: comprueba en el PC que la SD tiene todo lo que usa el firmware (como "SD 파일 점검").

Uso:
  python3 tools/sd_check.py <raiz de la SD o carpeta mons>      (carpeta mons/ y, si esta, mons.pak)
  python3 tools/sd_check.py <...> --pass FRASE                   (mons.pak con otra frase)
Imprime cuantos hay de cada clase y los que faltan.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

DEX = 251
MUSIC = ['bgm.wav', 'bgm2.wav', 'battle_wild.wav', 'battle_gym.wav', 'battle_champ.wav', 'fame.wav', 'fame2.wav',
         'story.wav', 'story_anime.wav', 'story_battle.wav', 'story_gym.wav', 'story_rocket.wav',
         'story_league.wav', 'story_end.wav']
FX_N = 171


def expected():
    cats = {
        'p (포켓몬 그림)': ['p%03d.bin' % d for d in range(1, DEX + 1)],
        'ps (색이 다른 그림)': ['ps%03d.bin' % d for d in range(1, DEX + 1)],
        'r (배틀 그림)': ['r%03d.bin' % d for d in range(1, DEX + 1)],
        'rs (배틀 색이 다른)': ['rs%03d.bin' % d for d in range(1, DEX + 1)],
        'cry (울음소리)': ['cry%03d.wav' % d for d in range(1, DEX + 1)],
        'fx (기술 이펙트)': ['fx/f%02d%d%d.bin' % (k // 9, (k % 9) // 3, k % 3) for k in range(144)] +
                        ['fx/m%03d.bin' % i for i in range(145, FX_N + 1)],
        'thumbs.bin (작은 그림)': ['thumbs.bin'],
        'story.bin (스토리 인물)': ['story.bin'],
    }
    return cats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('path')
    ap.add_argument('--pass', dest='pw', default=None)
    a = ap.parse_args()
    root = a.path
    mons = root if os.path.basename(os.path.normpath(root)) == 'mons' else os.path.join(root, 'mons')
    names = set()
    if os.path.isdir(mons):
        for sub in ('', 'fx/'):
            d = os.path.join(mons, sub)
            if os.path.isdir(d):
                names |= {sub + n for n in os.listdir(d) if os.path.isfile(os.path.join(d, n))}
    pak = os.path.join(os.path.dirname(os.path.normpath(mons)), 'mons.pak')
    if os.path.exists(pak):
        import make_pak
        _, _, idx = make_pak.read_index(pak, a.pw or make_pak.DEFAULT_PASS)
        names |= {n for n, _, _ in idx}
        print('mons.pak: %d 파일' % len(idx))
    loose = {n[3:] if n.startswith('fx/') else n for n in names}
    bad = 0
    for cat, want in expected().items():
        miss = [w for w in want if w not in names and not (w.startswith('fx/') and w[3:] in loose)]
        ok = len(want) - len(miss)
        bad += len(miss)
        print('%s %-22s %3d/%3d %s' % ('OK ' if not miss else '-- ', cat, ok, len(want),
                                         ' '.join(miss[:8]) + (' ...' if len(miss) > 8 else '')))
    mus = [m for m in MUSIC if m in names]
    print('   %-22s %3d/%3d (없어도 됨: 기본 음악)' % ('음악', len(mus), len(MUSIC)))
    print('모두 있어요!' if not bad else '없는 파일 %d개' % bad)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""ko11.21: /mons/story.bin - retratos de los personajes de la historia.

Los sprites de entrenador (c) Nintendo/Game Freak se bajan de Pokemon Showdown
a tools/story_cache/ (NO van al repositorio: uso personal) y se empaquetan en el
mismo formato que thumbs.bin (TPTH), un retrato por id (orden de STORY_WHO).

  python3 tools/pack_story.py [carpeta_salida]   -> <salida>/story.bin (defecto tools/sdcard/mons)
"""
import os, struct, sys, urllib.request
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, 'story_cache')
URL = 'https://play.pokemonshowdown.com/sprites/trainers/%s.png'
# el orden es el de STORY_WHO en story.h (id 1..N)
WHO = ['oak', 'blue', 'red', 'rocketgrunt', 'giovanni', 'brock', 'misty', 'ash', 'teamrocket',
       'ltsurge', 'erika', 'koga', 'sabrina', 'blaine', 'bruno', 'lance',
       # ko11.23: 칸나, 국화, 바람(애니), 리치(애니), 리그 예선 트레이너
       'lorelei-gen3', 'agatha-gen3', 'blue-gen3', 'acetrainer', 'acetrainer-gen3']


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def fetch(name):
    os.makedirs(CACHE, exist_ok=True)
    p = os.path.join(CACHE, name + '.png')
    if not os.path.exists(p):
        req = urllib.request.Request(URL % name, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=30) as r, open(p, 'wb') as f:
            f.write(r.read())
    return p


def blob(path):
    im = Image.open(path).convert('RGBA')
    bb = im.getbbox()
    if bb: im = im.crop(bb)
    w, h = im.size
    pal, used, data = [], {}, bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            if a < 128:
                data.append(0xFF); continue
            c = rgb565(r, g, b)
            if c not in used:
                if len(pal) >= 255:  # paleta llena: el mas parecido
                    c = min(pal, key=lambda q: abs((q >> 11) - (c >> 11)) + abs(((q >> 5) & 63) - ((c >> 5) & 63)) + abs((q & 31) - (c & 31)))
                else:
                    used[c] = len(pal); pal.append(c)
            data.append(used[c])
    return struct.pack('<3B', w, h, len(pal)) + struct.pack(f'<{len(pal)}H', *pal) + bytes(data)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'sdcard', 'mons')
    os.makedirs(out_dir, exist_ok=True)
    blobs = [blob(fetch(n)) for n in WHO]
    n = len(blobs)
    pos = 4 + 2 + 4 * n
    offs = []
    for b in blobs:
        offs.append(pos); pos += len(b)
    out = os.path.join(out_dir, 'story.bin')
    with open(out, 'wb') as f:
        f.write(b'TPTH'); f.write(struct.pack('<H', n)); f.write(struct.pack(f'<{n}I', *offs))
        for b in blobs: f.write(b)
    print(f'guardado {out}: {pos / 1024:.0f} KB, {n} retratos')


if __name__ == '__main__':
    main()

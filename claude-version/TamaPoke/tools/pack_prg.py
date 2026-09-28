#!/usr/bin/env python3
"""ko11.16: sprites de COMBATE al estilo de los juegos (delante y de espaldas).

Descarga de pokerogue.net los sprites animados de frente (rival) y de espaldas
(tu Pokemon) y los empaqueta en el mismo formato TPK2 de pack_pmd.py:
  accion 0  = frente (idle animado)
  accion 16 = espalda (idle animado)
Salida: sdcard/mons/rNNN.bin y rsNNN.bin (shiny). Van en la carpeta mons de la
SD JUNTO a lo demas (no sustituyen a pNNN.bin: esos siguen para la pantalla
principal y los entrenamientos).

Los graficos son de Nintendo / Game Freak: SOLO USO PERSONAL. No se suben al
repositorio (tools/prg_cache y tools/sdcard estan en .gitignore).

Uso: python3 pack_prg.py            (1..251, normal y shiny)
     python3 pack_prg.py 4 25 normal (solo esos, sin shiny)
     PMD_OUT=/ruta python3 pack_prg.py ...
"""
import json
import os
import struct
import sys
import urllib.request

from PIL import Image

BASE = 'https://pokerogue.net/images/pokemon'
HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, 'prg_cache')
OUT = os.environ.get('PMD_OUT') or os.path.join(HERE, 'sdcard', 'mons')
MAX_FRAMES = 24      # limite del firmware (PmdAct.ms[24])
FRAME_MS = 83        # ~12 fps, el ritmo de la animacion original
ALPHA_T = 128
ACT_FRONT, ACT_BACK = 0, 16


def fetch(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return True
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        data = urllib.request.urlopen(req, timeout=30).read()
        open(dest, 'wb').write(data)
        return True
    except Exception:
        return False


def rgb565(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def frames_of(sub, dexnum, shiny):
    """Frames RGBA (lienzo completo) de la animacion; None si no existe."""
    kind = (sub + '/' if sub else '') + ('shiny/' if shiny else '')
    tag = f'{sub or "front"}{"_s" if shiny else ""}_{dexnum}'
    png = os.path.join(CACHE, tag + '.png')
    js = os.path.join(CACHE, tag + '.json')
    if not (fetch(f'{BASE}/{kind}{dexnum}.png', png) and fetch(f'{BASE}/{kind}{dexnum}.json', js)):
        return None
    try:
        j = json.load(open(js))
    except Exception:
        return None
    t = j['textures'][0] if 'textures' in j else j
    frs = t['frames']
    if isinstance(frs, dict):
        frs = [dict(v, filename=k) for k, v in sorted(frs.items())]
    frs = sorted(frs, key=lambda f: f.get('filename', ''))
    sheet = Image.open(png).convert('RGBA')
    out = []
    for fr in frs:
        f, ss, sp = fr['frame'], fr['sourceSize'], fr['spriteSourceSize']
        c = sheet.crop((f['x'], f['y'], f['x'] + f['w'], f['y'] + f['h']))
        if fr.get('rotated'):
            c = c.rotate(90, expand=True)
        full = Image.new('RGBA', (ss['w'], ss['h']), (0, 0, 0, 0))
        full.paste(c, (sp['x'], sp['y']))
        out.append(full)
    return out or None


def pick(frames):
    """Como mucho MAX_FRAMES, repartidos por toda la animacion (y su duracion)."""
    n = len(frames)
    if n <= MAX_FRAMES:
        return frames, [FRAME_MS] * n
    step = n / MAX_FRAMES
    idx = [int(i * step) for i in range(MAX_FRAMES)]
    return [frames[i] for i in idx], [round(FRAME_MS * step)] * MAX_FRAMES


def crop_common(frames):
    """Recorta todos los frames al mismo rectangulo (union de lo visible)."""
    box = None
    for f in frames:
        b = f.getchannel('A').point(lambda a: 255 if a >= ALPHA_T else 0).getbbox()
        if b:
            box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    if box is None:
        return frames
    return [f.crop(box) for f in frames]


OUTLINE = os.environ.get('PRG_OUTLINE', '1') != '0'


def add_outline(frames):
    """ko11.16.1: borde oscuro de 1 px por FUERA de la silueta (como los PMD): en la
    pantalla pequena los de PokeRogue (borde de color en el lado con luz) se veian
    palidos. Se amplia el lienzo 1 px por lado"""
    out = []
    for f in frames:
        w, h = f.size
        g = Image.new('RGBA', (w + 2, h + 2), (0, 0, 0, 0))
        g.paste(f, (1, 1))
        a = g.getchannel('A').point(lambda v: 255 if v >= ALPHA_T else 0)
        px, ap = g.load(), a.load()
        for y in range(h + 2):
            for x in range(w + 2):
                if ap[x, y]:
                    continue
                if any(0 <= x + dx < w + 2 and 0 <= y + dy < h + 2 and ap[x + dx, y + dy]
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    px[x, y] = (24, 24, 32, 255)
        out.append(g)
    return out


def pack(dexnum, shiny=False):
    acts = []
    for aid, sub in ((ACT_FRONT, ''), (ACT_BACK, 'back')):
        fr = frames_of(sub, dexnum, shiny) or frames_of(sub, f'{dexnum}-a', shiny)  # Unown: por forma (201-a)
        if not fr:
            continue
        fr, ms = pick(fr)
        fr = crop_common(fr)
        if OUTLINE:
            fr = add_outline(fr)
        acts.append((aid, fr, ms))
    if not any(a[0] == ACT_FRONT for a in acts):
        raise RuntimeError('sin frente')

    colmap, pal, packed = {}, [], []
    for aid, frames, ms in acts:
        w, h = frames[0].size
        if w > 255 or h > 255:
            raise RuntimeError(f'frame {w}x{h} demasiado grande')
        data = bytearray()
        for fr in frames:
            for px in fr.getdata():
                if px[3] < ALPHA_T:
                    data.append(0xFF)
                    continue
                k = px[:3]
                if k not in colmap:
                    if len(pal) >= 255:
                        k2 = min(colmap, key=lambda c: sum((a - b) ** 2 for a, b in zip(c, k)))
                        colmap[k] = colmap[k2]
                    else:
                        colmap[k] = len(pal)
                        pal.append(k)
                data.append(colmap[k])
        packed.append((aid, w, h, len(frames), ms, bytes(data)))

    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, f'r{"s" if shiny else ""}{dexnum:03d}.bin')
    with open(path, 'wb') as f:
        f.write(b'TPK2')
        f.write(struct.pack('<BH', len(packed), len(pal)))
        for r, g, b in pal:
            f.write(struct.pack('<H', rgb565(r, g, b)))
        for aid, w, h, nf, ms, data in packed:
            f.write(struct.pack('<4B', aid, w, h, nf))
            f.write(struct.pack(f'<{nf}H', *ms))
            f.write(data)
    kb = os.path.getsize(path) / 1024
    print(f"  -> r{'s' if shiny else ''}{dexnum:03d}.bin: {len(packed)} acciones, {len(pal)} colores, {kb:.0f} KB")


if __name__ == '__main__':
    args = sys.argv[1:]
    solo_normal = 'normal' in args
    nums = [int(a) for a in args if a.isdigit()] or list(range(1, 252))
    fallos = []
    for n in nums:
        for sh in ([False] if solo_normal else [False, True]):
            try:
                print(f"#{n:03d}{' shiny' if sh else ''}")
                pack(n, sh)
            except Exception as e:
                print(f"  FALLO: {e}")
                fallos.append((n, sh))
    print(f"FALLOS: {fallos}" if fallos else "TODOS EMPAQUETADOS")

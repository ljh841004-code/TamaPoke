#!/usr/bin/env python3
"""ko12.10.7: sprites de combate "3D" (modelos 3D renderizados, como los juegos de 3DS).

Lee los GIF animados (frente / espalda, normal / shiny) de una carpeta y los
empaqueta en TPK3 (TPK2 de pack_prg.py con los frames comprimidos por filas), YA ESCALADOS al tamano de la
pantalla (el firmware los pinta 1:1, sin ampliar):
  fNNN.bin / fsNNN.bin  accion 0  = frente (rival y ficha de la pokedex)
  bNNN.bin / bsNNN.bin  accion 16 = espalda (el tuyo)
Van en la carpeta mons de la SD junto a lo demas. Si falta uno, el firmware usa
el "원작" (rNNN.bin) o el dibujo PMD.

Los graficos son de Nintendo / Game Freak: SOLO USO PERSONAL. No se suben al
repositorio.

Uso: python3 pack_3d.py <carpeta GIF> <salida mons> [numeros...]
  GIF esperados: fNNN.gif (frente), bNNN.gif (espalda), sfNNN.gif, sbNNN.gif (shiny)
"""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageSequence

MAX_FRAMES = 24   # limite del firmware (PmdAct.ms[24])
ALPHA_T = 128
# escala y tope (ancho, alto) en pixeles de pantalla: el rival de frente, el tuyo de espaldas
SPEC = {'f': (0, 1.5, 172, 126), 'b': (16, 1.9, 210, 170)}


def rgb565(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def gif_frames(path):
    im = Image.open(path)
    out = []
    for fr in ImageSequence.Iterator(im):
        out.append((fr.convert('RGBA'), max(10, fr.info.get('duration', 30) or 30)))
    return out


def pick(frames):
    """Como mucho MAX_FRAMES repartidos por el TIEMPO de la animacion (ms iguales)."""
    total = sum(d for _, d in frames)
    n = min(MAX_FRAMES, len(frames))
    if len(frames) <= MAX_FRAMES:
        return [f for f, _ in frames], [d for _, d in frames]
    starts, t = [], 0
    for _, d in frames:
        starts.append(t)
        t += d
    out = []
    for i in range(n):
        ti = total * i / n
        k = max(j for j in range(len(starts)) if starts[j] <= ti)
        out.append(frames[k][0])
    return out, [round(total / n)] * n


def union_box(frames):
    box = None
    for f in frames:
        b = f.getchannel('A').point(lambda a: 255 if a >= ALPHA_T else 0).getbbox()
        if b:
            box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    return box


def scaled(frames, sc, mw, mh):
    box = union_box(frames)
    if box is None:
        return None
    frames = [f.crop(box) for f in frames]
    w, h = frames[0].size
    sc = min(sc, mw / w, mh / h)
    W, H = max(1, round(w * sc)), max(1, round(h * sc))
    # alfa premultiplicado: sin halo oscuro en el borde al escalar
    return [f.convert('RGBa').resize((W, H), Image.LANCZOS).convert('RGBA') for f in frames]


def act_of(src, tag, num):
    path = os.path.join(src, f'{tag}{num:03d}.gif')
    if not os.path.exists(path):
        return None
    aid, sc, mw, mh = SPEC[tag[-1]]
    fr, ms = pick(gif_frames(path))
    fr = scaled(fr, sc, mw, mh)
    if not fr:
        return None
    return aid, fr, ms


def rle(idx):
    """ko12.10.11 (TPK3): por fila, tramos [salto transparente, n, n indices] hasta llegar a w
    (el ultimo puede ser [salto, 0]). Asi el ~55% transparente no ocupa (ficheros a menos de la mitad)"""
    out = bytearray()
    h, w = idx.shape
    for r in range(h):
        row = idx[r].tolist()
        x = 0
        while x < w:
            k = x
            while k < w and row[k] == 0xFF and k - x < 255:
                k += 1
            m = k
            while m < w and row[m] != 0xFF and m - k < 255:
                m += 1
            out += bytes((k - x, m - k)) + bytes(row[k:m])
            x = m
    return bytes(out)


def write(path, acts):
    # paleta comun de 255 colores para todos los frames (sin tramado: el tramado parpadea al animar)
    px = np.concatenate([np.asarray(f).reshape(-1, 4) for _, fr, _ in acts for f in fr])
    px = px[px[:, 3] >= ALPHA_T][:, :3]
    if len(px) > 400000:
        px = px[np.linspace(0, len(px) - 1, 400000).astype(int)]
    side = int(np.ceil(np.sqrt(len(px))))
    pad = np.zeros((side * side, 3), np.uint8)
    pad[:len(px)] = px
    pad[len(px):] = px[0]
    q = Image.fromarray(pad.reshape(side, side, 3)).quantize(255, method=Image.Quantize.MEDIANCUT)
    pal = np.array(q.getpalette()[:255 * 3], np.uint8).reshape(-1, 3)
    with open(path, 'wb') as f:
        f.write(b'TPK3')
        f.write(struct.pack('<BH', len(acts), len(pal)))
        for r, g, b in pal:
            f.write(struct.pack('<H', rgb565(int(r), int(g), int(b))))
        for aid, frames, ms in acts:
            w, h = frames[0].size
            assert w <= 255 and h <= 255 and len(frames) <= MAX_FRAMES
            f.write(struct.pack('<4B', aid, w, h, len(frames)))
            f.write(struct.pack(f'<{len(frames)}H', *ms))
            blobs = []
            for fr in frames:
                a = np.asarray(fr)
                idx = np.asarray(fr.convert('RGB').quantize(palette=q, dither=Image.Dither.NONE), np.uint8).copy()
                idx[a[:, :, 3] < ALPHA_T] = 0xFF
                blobs.append(rle(idx))
            offs, o = [], 0
            for b in blobs:
                offs.append(o)
                o += len(b)
            offs.append(o)
            f.write(struct.pack(f'<{len(offs)}I', *offs))
            for b in blobs:
                f.write(b)
    return os.path.getsize(path)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    nums = [int(a) for a in sys.argv[3:]] or list(range(1, 252))
    os.makedirs(out, exist_ok=True)
    falta, total = [], 0
    for n in nums:
        for tag in ('f', 'b', 'sf', 'sb'):
            a = act_of(src, tag, n)
            if not a:
                falta.append(f'{tag}{n:03d}')
                continue
            name = f'{tag[-1]}{"s" if len(tag) == 2 else ""}{n:03d}.bin'
            total += write(os.path.join(out, name), [a])
        print(f'#{n:03d} ok', flush=True)
    print(f'TOTAL {total / 1e6:.1f} MB; faltan: {falta or "ninguno"}')

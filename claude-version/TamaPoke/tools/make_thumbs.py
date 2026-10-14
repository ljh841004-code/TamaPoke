#!/usr/bin/env python3
"""Genera /mons/thumbs2.bin: miniaturas de los 251 para la galeria (ko10: gen 1 + 2).

ko12.9.23: se recorta al bicho (sin el marco vacio del sprite PMD) y el lado mayor llega a 48 px. Antes se
encogia el marco entero a 40: los de marco alto (Mewtwo 32x64, Lugia 72x96...) salian rotos y el resto
pequeno. Se llama thumbs2.bin porque el firmware lo busca antes que thumbs.bin, y un mons.pak con el
thumbs.bin viejo dentro tapaba un thumbs.bin suelto nuevo.

Se derivan del frame frontal (Idle, frame 0) de los sprites PMD ya empaquetados
(tools/sdcard/mons/pNNN.bin, formato TPK2) -> miniaturas legales (CC BY-NC), mismo
estilo que la pantalla principal. Formato TPTH (little-endian):

  char[4] "TPTH"
  uint16  count
  uint32  offset[count]    (desde el inicio del archivo, 1-based: offset[0]=dex 1)
  blobs:  u8 w, u8 h, u8 palCount, u16 pal[palCount], u8 data[w*h] (0xFF transp.)

  python3 tools/make_thumbs.py [carpeta_con_pNNN.bin]   -> <carpeta>/thumbs2.bin
"""
import os
import struct
import sys

DIR = os.path.join(os.path.dirname(__file__), 'sdcard', 'mons')
CELL = 48
OUT_NAME = 'thumbs2.bin'
N = 251  # ko10


def read_pmd_idle_frame0(path):
    """Frame 0 de la accion Idle (id 0 = vista frontal) de un sprite PMD TPK2."""
    with open(path, 'rb') as f:
        buf = f.read()
    if buf[:4] != b'TPK2':
        raise ValueError('magic TPK2')
    nacts = buf[4]
    (palcount,) = struct.unpack_from('<H', buf, 5)
    pal = list(struct.unpack_from(f'<{palcount}H', buf, 7))
    p = 7 + palcount * 2
    for _ in range(nacts):
        aid, w, h, nf = buf[p], buf[p + 1], buf[p + 2], buf[p + 3]
        p += 4 + nf * 2  # cabecera + ms[]
        if aid == 0:     # PMD_IDLE
            return w, h, pal, buf[p:p + w * h]  # frame 0
        p += w * h * nf
    raise ValueError('sin accion Idle (id 0)')


def crop(w, h, data):
    """Recorta al recuadro de lo visible (indices != 0xFF)."""
    xs = [i % w for i, v in enumerate(data) if v != 0xFF]
    ys = [i // w for i, v in enumerate(data) if v != 0xFF]
    if not xs:
        return w, h, data
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    out = bytearray()
    for y in range(y0, y1):
        out += data[y * w + x0:y * w + x1]
    return x1 - x0, y1 - y0, bytes(out)


def luma(c):
    return ((c >> 11) & 31) * 2 + ((c >> 5) & 63) + (c & 31) * 2  # aprox. 0..250


def shrink(w, h, pal, data):
    """Recorta y, si pasa de CELL, encoge por bloques: en cada bloque gana el color mas repetido; si hay
    borde oscuro en el bloque y es casi la mitad, gana el borde (el contorno no se pierde a trozos). Lo
    transparente solo gana si es mayoria."""
    w, h, data = crop(w, h, data)
    scale = min(CELL / w, CELL / h, 1.0)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    out = bytearray()
    used = {}
    newpal = []
    for y in range(nh):
        sy0 = int(y * h / nh); sy1 = max(sy0 + 1, int((y + 1) * h / nh))
        for x in range(nw):
            sx0 = int(x * w / nw); sx1 = max(sx0 + 1, int((x + 1) * w / nw))
            cnt = {}
            tot = 0
            for yy in range(sy0, sy1):
                for xx in range(sx0, sx1):
                    v = data[yy * w + xx]
                    cnt[v] = cnt.get(v, 0) + 1
                    tot += 1
            if cnt.get(0xFF, 0) * 2 > tot:
                out.append(0xFF)
                continue
            opaque = {k: n for k, n in cnt.items() if k != 0xFF}
            idx = max(opaque, key=lambda k: (opaque[k], -luma(pal[k])))
            dark = min(opaque, key=lambda k: luma(pal[k]))
            if luma(pal[dark]) < 60 and opaque[dark] * 5 >= sum(opaque.values()) * 2:
                idx = dark
            c = pal[idx]
            if c not in used:
                used[c] = len(newpal)
                newpal.append(c)
            out.append(used[c])
    return nw, nh, newpal, bytes(out)


def main():
    global DIR
    if len(sys.argv) > 1:
        DIR = sys.argv[1]
    blobs = []
    for dex in range(1, N + 1):
        path = os.path.join(DIR, f'p{dex:03d}.bin')
        w, h, pal, data = read_pmd_idle_frame0(path)
        nw, nh, npal, ndata = shrink(w, h, pal, data)
        if len(npal) > 255:
            raise ValueError(f'{dex}: paleta {len(npal)}')
        blob = struct.pack('<3B', nw, nh, len(npal))
        blob += struct.pack(f'<{len(npal)}H', *npal)
        blob += ndata
        blobs.append(blob)

    head = 4 + 2 + 4 * N
    offsets, pos = [], head
    for b in blobs:
        offsets.append(pos)
        pos += len(b)

    out = os.path.join(DIR, OUT_NAME)
    with open(out, 'wb') as f:
        f.write(b'TPTH')
        f.write(struct.pack('<H', N))
        f.write(struct.pack(f'<{N}I', *offsets))
        for b in blobs:
            f.write(b)
    print(f"guardado {out}: {pos / 1024:.0f} KB, {len(blobs)} miniaturas")


if __name__ == '__main__':
    main()

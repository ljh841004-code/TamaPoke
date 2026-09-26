#!/usr/bin/env python3
"""Genera font_ko.h: fuente coreana Noto Sans KR (Medium) con antialias de 2 bits.

fork KO (ko8): letra mas grande y limpia. Antes el hangul era Noto de 16/20 px
a 1 bit y el ASCII la unifont (pixelada). Ahora hangul Y ASCII salen de la
misma fuente, suavizados (4 niveles de gris mezclados con el fondo al pintar).

Tamanos (tamano pedido por setSize -> px):
    1 -> 16   2 -> 20   3 -> 26   4-5 -> 36   6 -> 48   7 -> 60
  - hangul: las 2350 silabas de KS X 1001 (y los jamo sueltos, para el teclado
    cheonjiin) en 16, 20 y 26 px; en 36 px solo las
    que salen en los textos del firmware (titulos grandes); 48/60 solo ASCII
  - ASCII 32..126 en todos los tamanos, recortado a su caja de tinta

Fuente: paquete npm @fontsource/noto-sans-kr (SIL OFL 1.1), ficheros
files/noto-sans-kr-*-500-normal.woff2 (la fuente viene troceada por rangos).
    npm pack @fontsource/noto-sans-kr@5.3.0 && tar xzf noto-sans-kr-5.3.0.tgz
    python3 tools/gen_font_ko.py package/files
Necesita Pillow, fontTools y brotli (pip install pillow fonttools brotli).
"""
import glob
import io
import os
import re
import sys

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.dirname(HERE)

HANGUL_FULL = (16, 20, 26)
HANGUL_SUB = (36,)
ASCII_ONLY = (48, 60)
TIERS = HANGUL_FULL + HANGUL_SUB + ASCII_ONLY
WEIGHT = 500


def ks_syllables():
    return [cp for cp in range(0xAC00, 0xD7A4) if len(chr(cp).encode('euc-kr')) == 2]


def ks_jamo():
    """jamo sueltos (teclado cheonjiin: "ㄱ", "ㅏ" y el punto "ㆍ" mientras se escribe)"""
    return list(range(0x3131, 0x3164)) + [0x318D]


def firmware_syllables():
    """silabas que aparecen en los literales del firmware (para el tamano 36)"""
    out = set()
    files = glob.glob(os.path.join(SKETCH, '*.cpp')) + glob.glob(os.path.join(SKETCH, '*.ino'))
    files += [os.path.join(SKETCH, 'dex.h')]
    for f in files:
        with open(f, encoding='utf-8', errors='ignore') as fh:
            src = fh.read()
        for lit in re.findall(r'"((?:[^"\\\n]|\\.)*)"', src):
            out |= {ord(c) for c in lit if 0xAC00 <= ord(c) <= 0xD7A3}
    return sorted(out)


class FontSet:
    """la fuente troceada: para cada codepoint, el trozo que lo tiene"""

    def __init__(self, folder):
        self.by_cp = {}
        self.data = {}
        pat = os.path.join(folder, f'noto-sans-kr-*-{WEIGHT}-normal.woff2')
        for path in sorted(glob.glob(pat)):
            t = TTFont(path)
            t.flavor = None
            buf = io.BytesIO()
            t.save(buf)
            self.data[path] = buf.getvalue()
            for cp in t.getBestCmap():
                self.by_cp.setdefault(cp, path)
        if not self.by_cp:
            raise SystemExit(f'no encuentro {pat}')
        self.cache = {}

    def font(self, cp, px):
        path = self.by_cp.get(cp)
        if path is None:
            raise SystemExit(f'U+{cp:04X} no esta en la fuente')
        key = (path, px)
        if key not in self.cache:
            self.cache[key] = ImageFont.truetype(io.BytesIO(self.data[path]), px)
        return self.cache[key]


def base_of(px):
    return round(px * 0.88)  # caja ideografica de Noto CJK: 880/1000 sobre la base


def quant(a):
    return (a * 3 + 127) // 255  # 0..255 -> 0..3


def pack2(img, w, h):
    rb = (w + 3) // 4
    px = img.load()
    out = bytearray(rb * h)
    for y in range(h):
        for x in range(w):
            q = quant(px[x, y])
            if q:
                out[y * rb + x // 4] |= q << (6 - 2 * (x % 4))
    return bytes(out)


def hangul_cell(fs, cp, px):
    img = Image.new('L', (px, px), 0)
    ImageDraw.Draw(img).text((0, base_of(px)), chr(cp), font=fs.font(cp, px), fill=255, anchor='ls')
    return pack2(img, px, px)


def ascii_glyph(fs, cp, px):
    f = fs.font(cp, px)
    adv = max(1, round(f.getlength(chr(cp))))
    pad = px
    img = Image.new('L', (adv + 2 * pad, 3 * px), 0)
    ImageDraw.Draw(img).text((pad, pad + base_of(px)), chr(cp), font=f, fill=255, anchor='ls')
    # solo los pixeles que quedan visibles tras cuantizar
    mask = img.point(lambda a: 255 if quant(a) else 0)
    bb = mask.getbbox()
    if not bb:
        return adv, 0, 0, 0, 0, b''
    x0, y0, x1, y1 = bb
    crop = img.crop(bb)
    return adv, x1 - x0, y1 - y0, x0 - pad, y0 - (pad + base_of(px)), pack2(crop, x1 - x0, y1 - y0)


def c_bytes(b, indent='  '):
    lines = []
    for i in range(0, len(b), 24):
        lines.append(indent + ','.join(str(v) for v in b[i:i + 24]) + ',')
    return '\n'.join(lines)


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'fonts_src')
    fs = FontSet(folder)
    ks = ks_jamo() + ks_syllables()
    assert len(ks) == 2350 + 52
    sub = firmware_syllables()
    missing = [c for c in sub if c not in ks]
    if missing:
        raise SystemExit('silabas fuera de KS X 1001: ' + ''.join(map(chr, missing)))
    out = os.path.join(SKETCH, 'font_ko.h')
    total = 0
    with open(out, 'w', encoding='utf-8') as fh:
        w = fh.write
        w('// GENERADO por tools/gen_font_ko.py: no editar a mano.\n')
        w('// Noto Sans KR Medium (SIL OFL 1.1, ver fonts-OFL.txt), 2 bits por pixel\n')
        w('// (4 niveles, el bit alto a la izquierda: 4 pixeles por byte).\n')
        w('#pragma once\n#include <stdint.h>\n\n')
        w(f'#define FKO_KS_COUNT {len(ks)}\n')
        w('// jamo + silabas de KS X 1001 en orden (busqueda binaria); indice = posicion\n')
        w('static const uint16_t FKO_KS_CP[FKO_KS_COUNT] = {\n')
        for i in range(0, len(ks), 12):
            w('  ' + ','.join(f'0x{c:04X}' for c in ks[i:i + 12]) + ',\n')
        w('};\n\n')
        w(f'#define FKO_SUB_COUNT {len(sub)}\n')
        w('// silabas del tamano 36 (las de los textos del firmware)\n')
        w('static const uint16_t FKO_SUB_CP[FKO_SUB_COUNT] = {\n')
        for i in range(0, len(sub), 12):
            w('  ' + ','.join(f'0x{c:04X}' for c in sub[i:i + 12]) + ',\n')
        w('};\n\n')
        w('struct FkoGlyph { uint8_t adv, w, h; int8_t xo, yo; uint32_t off; };\n')
        w('struct FkoTier {\n'
          '  uint8_t px, base;          // alto de la caja y linea base (desde arriba)\n'
          '  uint8_t hangul;            // 0 sin hangul, 1 las 2350, 2 solo FKO_SUB_CP\n'
          '  const uint8_t *hg;         // celdas px*px de hangul, ((px+3)/4)*px bytes cada una\n'
          '  const FkoGlyph *ascii;     // 95 glifos (32..126)\n'
          '  const uint8_t *abits;\n'
          '};\n\n')
        for px in TIERS:
            cps = ks if px in HANGUL_FULL else sub if px in HANGUL_SUB else []
            if cps:
                cell = ((px + 3) // 4) * px
                w(f'static const uint8_t FKO_HG{px}[{len(cps)} * {cell}] = {{\n')
                for cp in cps:
                    b = hangul_cell(fs, cp, px)
                    assert len(b) == cell
                    w(c_bytes(b) + '\n')
                    total += cell
                w('};\n')
            meta, bits = [], bytearray()
            for cp in range(32, 127):
                adv, gw, gh, xo, yo, b = ascii_glyph(fs, cp, px)
                meta.append((adv, gw, gh, xo, yo, len(bits)))
                bits += b
            total += len(bits) + 9 * 95
            w(f'static const uint8_t FKO_AB{px}[{max(1, len(bits))}] = {{\n{c_bytes(bits) if bits else "  0,"}\n}};\n')
            w(f'static const FkoGlyph FKO_AG{px}[95] = {{\n')
            for m in meta:
                w('  {%d,%d,%d,%d,%d,%d},\n' % m)
            w('};\n\n')
        w('static const FkoTier FKO_TIERS[] = {\n')
        for px in TIERS:
            mode = 1 if px in HANGUL_FULL else 2 if px in HANGUL_SUB else 0
            hg = f'FKO_HG{px}' if mode else 'nullptr'
            w(f'  {{ {px}, {base_of(px)}, {mode}, {hg}, FKO_AG{px}, FKO_AB{px} }},\n')
        w('};\n')
        w(f'#define FKO_TIER_COUNT {len(TIERS)}\n')
    print(f'{out}: {len(TIERS)} tamanos, {len(sub)} silabas en 36 px, ~{total // 1024} KB de glifos')


if __name__ == '__main__':
    main()

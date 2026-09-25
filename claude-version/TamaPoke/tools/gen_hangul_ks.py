#!/usr/bin/env python3
"""Genera hangul_ks.h: glifos Noto Sans CJK KR de las 2350 silabas de KS X 1001.

fork KO (ko4): la fuente unifont se veia tosca. TamaPoke v1.23 (raiz del repo)
trae los 11172 glifos en hangul_font.h (16x16) y hangul_font20.h (20x20), ~1 MB
de flash. Aqui solo hacen falta las silabas de KS X 1001 (EUC-KR): son todas las
que usa el firmware, y test_tools.py ya obliga a que los textos nuevos quepan en
ese conjunto. Asi la tabla ocupa ~220 KB.

Uso:  python3 tools/gen_hangul_ks.py [ruta/a/hangul_font.h] [ruta/a/hangul_font20.h]
(por defecto, los de la raiz del repositorio: ../../hangul_font*.h)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(SKETCH))


def ks_syllables():
    out = []
    for cp in range(0xAC00, 0xD7A4):
        # el codec euc-kr de Python codifica TODAS las silabas: las que no son
        # de KS X 1001 salen como secuencias de 8 bytes. Solo valen las de 2.
        if len(chr(cp).encode('euc-kr')) == 2:
            out.append(cp)
    return out


def read_rows(path, per_glyph):
    with open(path, encoding='utf-8') as fh:
        src = fh.read()
    body = src[src.index('= {') + 3:]
    rows = re.findall(r'\{([0-9,\s]+)\}', body)
    glyphs = [[int(v) for v in r.split(',') if v.strip()] for r in rows]
    if len(glyphs) != 11172 or any(len(g) != per_glyph for g in glyphs):
        raise SystemExit(f'{path}: formato inesperado ({len(glyphs)} glifos)')
    return glyphs


def main():
    f16 = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, 'hangul_font.h')
    f20 = sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, 'hangul_font20.h')
    g16 = read_rows(f16, 16)
    g20 = read_rows(f20, 60)
    cps = ks_syllables()
    assert len(cps) == 2350, len(cps)
    out = os.path.join(SKETCH, 'hangul_ks.h')
    with open(out, 'w', encoding='utf-8') as fh:
        fh.write('// GENERADO por tools/gen_hangul_ks.py: no editar a mano.\n')
        fh.write('// Noto Sans CJK KR (SIL OFL 1.1, ver fonts-OFL.txt), solo las 2350\n')
        fh.write('// silabas de KS X 1001. HANGUL_KS_CP esta ordenado (busqueda binaria).\n')
        fh.write('#pragma once\n#include <stdint.h>\n\n')
        fh.write(f'#define HANGUL_KS_COUNT {len(cps)}\n')
        fh.write('static const uint16_t HANGUL_KS_CP[HANGUL_KS_COUNT] = {\n')
        for i in range(0, len(cps), 12):
            fh.write('  ' + ','.join(f'0x{c:04X}' for c in cps[i:i + 12]) + ',\n')
        fh.write('};\n\n// 16x16, una fila por uint16 (bit 15 = columna izquierda)\n')
        fh.write('static const uint16_t HANGUL_KS16[HANGUL_KS_COUNT][16] = {\n')
        for c in cps:
            fh.write('{' + ','.join(str(v) for v in g16[c - 0xAC00]) + '},\n')
        fh.write('};\n\n// 20x20, 3 bytes por fila (drawBitmap de Arduino_GFX, MSB primero)\n')
        fh.write('static const uint8_t HANGUL_KS20[HANGUL_KS_COUNT][60] = {\n')
        for c in cps:
            fh.write('{' + ','.join(str(v) for v in g20[c - 0xAC00]) + '},\n')
        fh.write('};\n')
    print(f'{out}: {len(cps)} glifos')


if __name__ == '__main__':
    main()

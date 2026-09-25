#!/usr/bin/env python3
"""Saca de U8g2 (u8g2_fonts.c, 40 MB) solo las dos fuentes que usa el sketch."""
import sys

src, out = sys.argv[1], sys.argv[2]
want = ['u8g2_font_unifont_t_korean2', 'u8g2_font_unifont_t_japanese3']
text = open(src, encoding='latin-1').read()


def literal(name):
    start = text.index('const uint8_t ' + name + '[')
    i = text.index('=', start) + 1
    in_str = esc = False
    while True:  # hasta el ';' que queda fuera de las cadenas
        c = text[i]
        if in_str:
            if esc: esc = False
            elif c == '\\': esc = True
            elif c == '"': in_str = False
        elif c == '"': in_str = True
        elif c == ';': return text[text.index('=', start) + 1:i].strip()
        i += 1


with open(out, 'w', encoding='latin-1') as o:
    o.write('#include <stdint.h>\n')
    for name in want:
        o.write(f'const uint8_t {name}[] = {literal(name)};\n')

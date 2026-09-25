#!/usr/bin/env python3
"""Build all 11,172 modern Hangul syllables at 16px from Noto Sans CJK KR (OFL)."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

font = ImageFont.truetype(sys.argv[1], 16)
rows = []
for cp in range(0xAC00, 0xD7A4):
    im = Image.new('1', (16, 16))
    draw = ImageDraw.Draw(im)
    draw.text((0, -3), chr(cp), font=font, fill=1)
    rows.append(','.join(str(sum(im.getpixel((x, y)) << (15-x) for x in range(16))) for y in range(16)))
Path(__file__).resolve().parents[1].joinpath('hangul_font.h').write_text(
    '// Generated from Noto Sans CJK KR, SIL OFL 1.1. See fonts-OFL.txt.\n'
    '#pragma once\n#include <stdint.h>\nstatic const uint16_t HANGUL_BITMAP[11172][16] = {\n'
    + '\n'.join('{' + row + '},' for row in rows) + '\n};\n')

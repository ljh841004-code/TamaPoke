#!/usr/bin/env python3
"""Fuente del modo HD (Android): Noto Sans KR Medium (la misma que font_ko.h) con ASCII, las 2350
silabas KS X 1001, los jamo y todo lo que no es ASCII en el codigo del firmware. SIL OFL 1.1.
  python3 android/tools_make_font.py <fontsource/files> android/assets/NotoSansKR-Medium-hd.otf"""
import glob, io, os, sys
from fontTools import subset
from fontTools.merge import Merger
from fontTools.ttLib import TTFont

src, out = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
chars = {chr(c) for c in range(0x20, 0x7F)} | {chr(c) for c in range(0x3131, 0x318F)}
for c in range(0xAC00, 0xD7A4):
    try:
        chr(c).encode('euc-kr'); chars.add(chr(c))
    except UnicodeEncodeError:
        pass
for p in glob.glob(os.path.join(here, '..', 'TamaPoke', '*.*')):
    if p.endswith(('.ino', '.cpp', '.h')) and 'font_ko' not in p:
        chars |= set(open(p, encoding='utf-8', errors='ignore').read())
cps = {ord(c) for c in chars if ord(c) >= 0x20 and not (0x3040 <= ord(c) <= 0x30FF or 0x3400 <= ord(c) <= 0x9FFF or 0xF900 <= ord(c) <= 0xFAFF or 0xFF00 <= ord(c) <= 0xFFEF)}  # sin japones (solo coreano)
parts = []
for path in sorted(glob.glob(os.path.join(src, 'noto-sans-kr-*-500-normal.woff2'))):
    f = TTFont(path)
    have = set(f.getBestCmap()) & cps
    if not have:
        continue
    opt = subset.Options(); opt.flavor = None; opt.layout_features = []; opt.notdef_outline = True; opt.hinting = False
    s = subset.Subsetter(opt); s.populate(unicodes=have); s.subset(f)
    f.flavor = None
    p = os.path.join(here, '_hdpart%d.otf' % len(parts)); f.save(p); parts.append(p)
m = Merger().merge(parts)
for p in parts: os.remove(p)
m.flavor = None
m.save(out)
print(out, os.path.getsize(out), len(cps), 'chars')

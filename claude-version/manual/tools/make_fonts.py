#!/usr/bin/env python3
"""매뉴얼에 쓰인 글자만 담은 Noto Sans KR woff2를 만든다 (fonts/).

fontsource 패키지(@fontsource/noto-sans-kr)의 files/ 폴더를 받아
굵기별로 조각 파일들을 필요한 글자만 남겨 합친다.

  python3 tools/make_fonts.py <fontsource/package/files>
  python3 tools/make_fonts.py <files> --out pocket/fonts pocket/pocket.html pocket/pocket.css   (손바닥 가이드)
"""
import glob
import io
import os
import re
import sys

from fontTools import subset
from fontTools.merge import Merger
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WEIGHTS = (400, 700, 900)


def used_chars(files=('manual.html', 'style.css')):
    text = ''
    for f in files:
        with open(os.path.join(ROOT, f), encoding='utf-8') as fh:
            text += fh.read()
    text = re.sub(r'<[^>]+>', ' ', text)
    chars = set(text) | set(chr(c) for c in range(0x20, 0x7F))
    return {c for c in chars if ord(c) >= 0x20}


def build(src, weight, chars, outdir):
    cps = {ord(c) for c in chars}
    parts = []
    for path in sorted(glob.glob(os.path.join(src, f'noto-sans-kr-*-{weight}-normal.woff2'))):
        f = TTFont(path)
        have = set(f.getBestCmap()) & cps
        if not have:
            continue
        opt = subset.Options()
        opt.flavor = None
        opt.layout_features = ['*']
        opt.notdef_outline = True
        s = subset.Subsetter(opt)
        s.populate(unicodes=have)
        s.subset(f)
        f.flavor = None
        buf = io.BytesIO()
        f.save(buf)
        parts.append(buf)
    tmp = []
    for i, b in enumerate(parts):
        p = os.path.join(HERE, f'_part{i}.ttf')
        with open(p, 'wb') as fh:
            fh.write(b.getvalue())
        tmp.append(p)
    merged = Merger().merge(tmp)
    for p in tmp:
        os.remove(p)
    merged.flavor = 'woff2'
    out = os.path.join(outdir, f'NotoSansKR-{weight}.woff2')
    merged.save(out)
    return out


def main():
    args = sys.argv[1:]
    src = args.pop(0)
    outdir = os.path.join(ROOT, 'fonts')
    if args[:1] == ['--out']:
        outdir = os.path.join(ROOT, args[1])
        args = args[2:]
    os.makedirs(outdir, exist_ok=True)
    chars = used_chars(args) if args else used_chars()
    css = []
    for w in WEIGHTS:
        out = build(src, w, chars, outdir)
        css.append("@font-face { font-family: 'Noto Sans KR'; font-weight: %d; font-style: normal;\n"
                   "  src: url(%s) format('woff2'); }" % (w, os.path.basename(out)))
        print(out, os.path.getsize(out))
    with open(os.path.join(outdir, 'fonts.css'), 'w', encoding='utf-8') as fh:
        fh.write('/* tools/make_fonts.py 가 만든 파일 (Noto Sans KR, SIL OFL 1.1) */\n' + '\n'.join(css) + '\n')


if __name__ == '__main__':
    main()

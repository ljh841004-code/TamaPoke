#!/usr/bin/env python3
"""인쇄소(재단)용 손바닥 가이드 PDF

  python3 make_print.py        # pocket.html 이 먼저 있어야 해요 (build_pocket.sh)

만드는 파일:
  TamaPoke_손바닥가이드_인쇄소_도련3mm.pdf   쪽마다 80x111mm (재단 74x105 + 사방 도련 3mm). 인쇄소에 주는 파일
  TamaPoke_손바닥가이드_인쇄소_재단표시.pdf  같은 쪽에 재단 표시(모서리 선)를 더한 96x127mm. 확인·교정용
  인쇄소_사양.txt                          주문할 때 그대로 보여 주는 메모
"""
import io
import os
import subprocess
import sys

from pypdf import PageObject, PdfReader, PdfWriter, Transformation

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from impose_a7 import chrome  # noqa: E402

MM = 72 / 25.4
BLEED = 3.0
TW, TH = 74.0, 105.0                 # 재단 크기
BW, BH = TW + 2 * BLEED, TH + 2 * BLEED
PAD = 8.0                            # 재단 표시 칸
HERE = os.path.dirname(os.path.abspath(__file__))

SPEC = """다마포케 손바닥 가이드 - 인쇄소 주문 메모
==========================================

파일      TamaPoke_손바닥가이드_인쇄소_도련3mm.pdf  (낱장 {n}쪽, 1쪽 = 1페이지, 순서대로)
재단 크기 74 x 105 mm (A7)
작업 크기 80 x 111 mm (사방 도련 3mm 포함)
쪽 수     {n}쪽 (표지 포함, 4의 배수)
제본      중철 (가운데 스테이플 2곳)
색        4도 양면 (컬러)
종이 추천 표지 250g 아트지 + 무광 코팅 / 내지 120g 모조지 또는 스노우지
선택      표지 왼쪽 위 지름 5mm 구멍 (고리용, 점선 동그라미 자리) / 귀돌이(모서리 둥글게)

확인용    TamaPoke_손바닥가이드_인쇄소_재단표시.pdf 는 같은 내용에 재단 표시를 넣은 것 (96 x 127 mm)
안전선    글자와 사진은 재단선 안쪽 4mm 안에 있어요

* 중철은 쪽 수가 많으면 안쪽 종이가 바깥으로 밀려 나와요 (크리프). {n}쪽 120g이면 약 0.5mm라
  인쇄소에서 보정해 주는 경우가 많아요. 물어보세요.
* 화면 사진의 포켓몬 그림은 개인이 즐기는 용도예요. 판매용으로 찍지 마세요.
"""


def marks_page():
    """96x127mm 한 쪽: 재단 모서리 표시 (검정 실선)"""
    W, H = BW + 2 * PAD, BH + 2 * PAD
    x0, y0 = PAD + BLEED, PAD + BLEED          # 재단선 왼쪽 아래
    x1, y1 = x0 + TW, y0 + TH
    ops = ['0 G 0.25 w']
    gap, ln = BLEED + 0.5, 5.0
    for cx, sx in ((x0, -1), (x1, 1)):
        for cy, sy in ((y0, -1), (y1, 1)):
            ops.append('%.2f %.2f m %.2f %.2f l S' % ((cx + sx * gap) * MM, cy * MM, (cx + sx * (gap + ln)) * MM, cy * MM))
            ops.append('%.2f %.2f m %.2f %.2f l S' % (cx * MM, (cy + sy * gap) * MM, cx * MM, (cy + sy * (gap + ln)) * MM))
    stream = '\n'.join(ops).encode()
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>', b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
            ('<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] /Contents 4 0 R >>' % (W * MM, H * MM)).encode(),
            b'<< /Length %d >>\nstream\n' % len(stream) + stream + b'\nendstream']
    out = io.BytesIO()
    out.write(b'%PDF-1.4\n')
    offs = []
    for k, o in enumerate(objs):
        offs.append(out.tell())
        out.write(b'%d 0 obj\n' % (k + 1) + o + b'\nendobj\n')
    xref = out.tell()
    out.write(b'xref\n0 %d\n0000000000 65535 f \n' % (len(objs) + 1))
    for o in offs:
        out.write(b'%010d 00000 n \n' % o)
    out.write(b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, xref))
    return PdfReader(io.BytesIO(out.getvalue())).pages[0], W, H


def main():
    src = os.path.join(HERE, 'pocket.html')
    html = open(src, encoding='utf-8').read().replace(
        '<link rel="stylesheet" href="pocket.css">',
        '<link rel="stylesheet" href="pocket.css"><link rel="stylesheet" href="pocket_bleed.css">')
    tmp = os.path.join(HERE, '.pocket_bleed_tmp.html')
    with open(tmp, 'w', encoding='utf-8') as f:
        f.write(html)
    bleed = os.path.join(HERE, 'TamaPoke_손바닥가이드_인쇄소_도련3mm.pdf')
    try:
        subprocess.run([chrome(), '--headless', '--no-sandbox', '--disable-gpu', '--no-pdf-header-footer',
                        '--print-to-pdf=' + bleed, 'file://' + tmp], check=True, capture_output=True)
    finally:
        os.remove(tmp)
    pages = PdfReader(bleed).pages
    n = len(pages)
    pw = float(pages[0].mediabox.width) / MM
    if abs(pw - BW) > 0.5:
        sys.exit('쪽 크기가 %.1fmm (80mm여야 해요)' % pw)
    wr = PdfWriter()
    for p in pages:
        mk, W, H = marks_page()
        pg = PageObject.create_blank_page(width=W * MM, height=H * MM)
        pg.merge_transformed_page(p, Transformation().translate(PAD * MM, PAD * MM))
        pg.merge_page(mk)
        wr.add_page(pg)
    with open(os.path.join(HERE, 'TamaPoke_손바닥가이드_인쇄소_재단표시.pdf'), 'wb') as fh:
        wr.write(fh)
    with open(os.path.join(HERE, '인쇄소_사양.txt'), 'w', encoding='utf-8') as f:
        f.write(SPEC.format(n=n))
    print('인쇄소용 %d쪽 (80x111mm, 재단 표시판 96x127mm)' % n)


if __name__ == '__main__':
    main()

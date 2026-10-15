#!/usr/bin/env python3
"""다마포케 사용 설명서 A6 (105 x 148 mm) - 손바닥 가이드(A7)를 그대로 √2배 키운 책

  python3 make_a6.py      # ../pocket/build_pocket.sh 를 먼저 (pocket.html, img/, fonts/)

A7 -> A6은 가로세로 모두 √2배라 쪽 배치가 그대로 맞아요 (글자 6.6pt -> 약 9.3pt).
고리 구멍 표시는 지워요. 만드는 파일 (이 폴더):
  TamaPoke_사용설명서_A6.pdf             낱장 105x148mm, 순서대로 (화면 보기 / 확인용)
  TamaPoke_사용설명서_A6_도련3mm.pdf     인쇄소용 낱장 111x154mm (사방 도련 3mm)
  인쇄용_A6_전체_60쪽.pdf / 인쇄용_A6_본문_56쪽.pdf / 인쇄용_A6_표지_펼침.pdf
                                          인쇄소에 올리는 파일: CMYK 300dpi 그림, 글꼴 없음, 잉크 240% 이하
  인쇄소_사양_A6.txt                      주문 옵션 메모
"""
import os
import re
import subprocess
import sys

from pypdf import PageObject, PdfReader, PdfWriter, Transformation

HERE = os.path.dirname(os.path.abspath(__file__))
POCKET = os.path.join(HERE, '..', 'pocket')
sys.path.insert(0, POCKET)
from impose_a7 import chrome  # noqa: E402

MM = 72 / 25.4
K = 2 ** 0.5
A6W, A6H, B = 105.0, 148.0, 3.0

# 재단 오차(1~2mm)에 안 잘리게 글자는 재단선에서 A7 기준 3mm(= A6 약 4.2mm) 안쪽으로. B = 도련(0 또는 3mm)
NOHOLE = '''<style>
.hole, .cover .holemark, .cover .holetxt { display: none !important; }
.odd .hd .t { left: calc(VARmm) !important; }
.hd .t { top: calc(BBmm + 3.1mm) !important; } .hd .ch { top: calc(BBmm + 3.7mm) !important; }
.ft { bottom: calc(BBmm + 3mm) !important; }
.odd .ft { right: calc(BBmm + 4mm) !important; } .even .ft { left: calc(BBmm + 4mm) !important; }
</style>'''

SPEC = """다마포케 사용 설명서 (A6) - 인쇄소 주문 메모 (디티피아 소량 디지털 책자)
=========================================================================

[올릴 파일] 인쇄용_ 으로 시작하는 파일만 올려요 (편집 없이 그대로 인쇄되는 파일)
  하나로 올릴 때   인쇄용_A6_전체_{n}쪽.pdf    낱장 {n}쪽, 1쪽 = 앞표지 ... {n}쪽 = 뒤표지, 순서대로
  표지/본문 따로   표지: 인쇄용_A6_표지_펼침.pdf  (1면 바깥 = 뒤표지|앞표지, 2면 안쪽 = 앞표지 안|뒤표지 안)
                   본문: 인쇄용_A6_본문_{body}쪽.pdf

[파일 상태] (사이트 안내사항 확인 완료)
  - 재단 105 x 148 mm, 작업(도련 포함) 111 x 154 mm. 글자는 재단선에서 4mm 이상 안쪽
  - CMYK (SWOP), 300dpi, 잉크 총량 최대 약 240% (한도 250%)
  - 글꼴 없음 (모두 그림으로 바꿈 = 아웃라인과 같음), 투명·그림자 효과 없음, 흰 바탕

[옵션 표]
  규격          비규격  -> 재단사이즈 105*148 / 작업사이즈 111*154
                표지사이즈 210*148 (펼침, 작업 216*154) / 세네카 0 (중철은 책등 없음)
  주문수량      원하는 권수 (2권부터)
  표지          고급지 (스노우지 또는 아트지 200~250g) / 양면 / 칼라 / 단면무광코팅 / 오시 있음
  본문1 내지(컬러)  고급지 (스노우지 100~120g) / 양면 / 칼라 / {body}페이지
  본문2 내지(흑백)  0페이지
  면지          없음
  제본          중철 / 세로형 좌철

* A6가 규격 목록에 없어서 비규격으로 넣어요. 제본방향 안내에 A6가 없으니
  "105x148 중철 가능한지"를 주문 전에 문의 게시판/전화로 꼭 확인하세요.
* 두꺼운 표지는 접는 곳이 갈라질 수 있어 "오시 있음"을 추천해요.
* 화면 사진의 포켓몬 그림은 개인이 즐기는 용도예요. 판매용으로 찍지 마세요.
"""


def render(css_extra, left, out, bb):
    html = open(os.path.join(POCKET, 'pocket.html'), encoding='utf-8').read()
    html = re.sub(r'<br><b>고리</b>[^<]*(<[^/][^>]*>[^<]*</[^>]+>)*[^<]*', '', html)  # 차례의 고리 안내
    html = html.replace('<div class="c">손바닥 가이드</div>', '<div class="c">사용 설명서</div>')
    html = html.replace('<title>다마포케 손바닥 가이드</title>', '<title>다마포케 사용 설명서</title>')
    html = html.replace('</head>', css_extra + NOHOLE.replace('VAR', left).replace('BB', bb) + '</head>')
    tmp = os.path.join(POCKET, '.a6_tmp.html')
    with open(tmp, 'w', encoding='utf-8') as f:
        f.write(html)
    try:
        subprocess.run([chrome(), '--headless', '--no-sandbox', '--disable-gpu', '--no-pdf-header-footer',
                        '--print-to-pdf=' + out, 'file://' + tmp], check=True, capture_output=True)
    finally:
        os.remove(tmp)


def scale(src, out, W, H):
    """A7 쪽을 √2배 해서 W x H mm 가운데에"""
    w = PdfWriter()
    for p in PdfReader(src).pages:
        pw, ph = float(p.mediabox.width) * K, float(p.mediabox.height) * K
        pg = PageObject.create_blank_page(width=W * MM, height=H * MM)
        pg.merge_transformed_page(p, Transformation().scale(K).translate((W * MM - pw) / 2, (H * MM - ph) / 2))
        w.add_page(pg)
    with open(out, 'wb') as fh:
        w.write(fh)
    return len(w.pages)


def pick(src, out, idx):
    r = PdfReader(src)
    w = PdfWriter()
    for i in idx:
        w.add_page(r.pages[i])
    with open(out, 'wb') as fh:
        w.write(fh)


DPI = 300
INK_MAX = 2.4  # 잉크 총량 240% (디티피아 한도 250%에 여유)


def to_cmyk(rgb):
    """RGB(uint8 HxWx3) -> CMYK(uint8). 표준 인쇄 프로필(SWOP, MuPDF 내장 ICC)로 바꾸고,
    잉크 총량이 넘는 곳은 회색 성분을 검정(K)으로 옮겨(UCR) 240% 이하로"""
    import numpy as np
    import pymupdf
    pymupdf.TOOLS.set_icc(True)
    h, w, _ = rgb.shape
    src = pymupdf.Pixmap(pymupdf.csRGB, w, h, np.ascontiguousarray(rgb).tobytes(), False)
    cm = pymupdf.Pixmap(pymupdf.csCMYK, src)
    a = np.frombuffer(cm.samples, np.uint8).reshape(h, w, cm.n)[..., :4].astype(np.float32) / 255.0
    cmy, k = a[..., :3], a[..., 3]
    tot = cmy.sum(axis=2) + k
    x = np.clip(np.minimum(np.minimum(cmy.min(axis=2), (tot - INK_MAX) / 2), 1 - k), 0, None)
    cmy = cmy - x[..., None]
    k = k + x
    s = cmy.sum(axis=2)
    over = s + k > INK_MAX
    cmy = cmy * np.where(over, (INK_MAX - k) / np.maximum(s, 1e-6), 1.0)[..., None]
    out = np.concatenate([cmy, k[..., None]], axis=2)
    return (np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)


def raster(pdf):
    """쪽마다 300dpi RGB 배열"""
    import numpy as np
    import pymupdf
    pages = []
    for p in pymupdf.open(pdf):
        pix = p.get_pixmap(dpi=DPI, alpha=False)
        pages.append(np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width, 3).copy())
    return pages


def write_cmyk(arrays, out, w_mm, h_mm):
    """CMYK 그림(300dpi, 무손실 압축)만 든 PDF: 글꼴·투명·효과가 없어서 인쇄소가 손댈 게 없어요"""
    import pymupdf
    doc = pymupdf.open()
    for a in arrays:
        c = to_cmyk(a)
        h, w, _ = c.shape
        pix = pymupdf.Pixmap(pymupdf.csCMYK, w, h, c.tobytes(), False)
        pix.set_dpi(DPI, DPI)
        pg = doc.new_page(width=w_mm * MM, height=h_mm * MM)
        pg.insert_image(pg.rect, pixmap=pix)
    doc.save(out, deflate=True, garbage=3)


def print_ready(bleed_pdf):
    import numpy as np
    pages = raster(bleed_pdf)
    n = len(pages)
    W, H = A6W + 2 * B, A6H + 2 * B
    write_cmyk(pages, os.path.join(HERE, '인쇄용_A6_전체_%d쪽.pdf' % n), W, H)
    write_cmyk(pages[2:n - 2], os.path.join(HERE, '인쇄용_A6_본문_%d쪽.pdf' % (n - 4)), W, H)
    # 표지 펼침면 (가운데 접는 곳에는 도련 없이 맞붙임): 바깥 [뒤표지 | 앞표지], 안쪽 [앞표지 안쪽 | 뒤표지 안쪽]
    cut = round(B / 25.4 * DPI)
    def spread(l, r):
        return np.concatenate([pages[l][:, :pages[l].shape[1] - cut], pages[r][:, cut:]], axis=1)
    write_cmyk([spread(n - 1, 0), spread(1, n - 2)], os.path.join(HERE, '인쇄용_A6_표지_펼침.pdf'),
               2 * A6W + 2 * B, H)


def main():
    a7 = os.path.join(HERE, '.a7.pdf')
    a7b = os.path.join(HERE, '.a7_bleed.pdf')
    render('', '4', a7, '0')
    render('<link rel="stylesheet" href="pocket_bleed.css">', '7', a7b, '3')
    n = scale(a7, os.path.join(HERE, 'TamaPoke_사용설명서_A6.pdf'), A6W, A6H)
    bleed = os.path.join(HERE, 'TamaPoke_사용설명서_A6_도련3mm.pdf')
    scale(a7b, bleed, A6W + 2 * B, A6H + 2 * B)
    os.remove(a7)
    os.remove(a7b)
    print_ready(bleed)
    with open(os.path.join(HERE, '인쇄소_사양_A6.txt'), 'w', encoding='utf-8') as f:
        f.write(SPEC.format(n=n, body=n - 4))
    print('A6 %d쪽 (표지 4 + 본문 %d)' % (n, n - 4))


if __name__ == '__main__':
    main()

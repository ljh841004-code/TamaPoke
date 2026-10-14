#!/usr/bin/env python3
"""A7 쪽 PDF(74x105mm)를 집 프린터용 A4 배열로 바꾼다.

  python3 impose_a7.py 입력_A7.pdf 출력폴더

만드는 파일 (출력폴더 안):
  TamaPoke_손바닥가이드_A4양면.pdf   A4 4장 양면 (짧은 쪽으로 넘기기). 추천
  TamaPoke_손바닥가이드_A4단면풀칠.pdf  A4 8장 한 면만. 양면이 안 되는 프린터: 앞/뒤 조각을 풀로 붙여요
  TamaPoke_손바닥가이드_만드는법.pdf  A4 1장 안내

- A4 가로 한 면에 펼침면(A7 두 쪽) 4개 (2x2). 프린터 여백 때문에 92%로 줄여 놓아요 (완성 약 68x97mm)
- 빨간 점선 = 자르는 선 (종이 끝까지 이어짐), 파란 점선 = 접는 곳 (바깥 여백에만)
- 겹치는 순서 = 종이 번호 (1이 가장 바깥). 가운데를 접고 등에 스테이플 2곳 (중철)
쪽 수는 4의 배수 (모자라면 빈 쪽). 선과 글자는 Chromium으로 그린 투명 PDF를 겹쳐요 (한글).
"""
import os
import subprocess
import sys
import tempfile
from glob import glob

from pypdf import PageObject, PdfReader, PdfWriter, Transformation

MM = 72 / 25.4
A4W, A4H = 297.0, 210.0       # mm
SC = 0.92                     # 프린터 여백
GW, GH = 8.0, 6.0             # 펼침면 사이 (자르는 곳), mm
RED, BLUE = '#e0301e', '#1f6fe0'


def chrome():
    c = os.environ.get('CHROME') or next(iter(glob('/opt/pw-browsers/chromium-*/chrome-linux/chrome')), None)
    for alt in ('chromium', 'google-chrome', 'chromium-browser'):
        if c:
            break
        c = subprocess.run(['sh', '-c', 'command -v ' + alt], capture_output=True, text=True).stdout.strip()
    if not c:
        sys.exit('Chromium이 필요해요 (CHROME=경로)')
    return c


def html_to_pdf(html, out, here):
    fd, path = tempfile.mkstemp(suffix='.html', dir=here)
    with os.fdopen(fd, 'w', encoding='utf-8') as f:
        f.write(html)
    try:
        subprocess.run([chrome(), '--headless', '--no-sandbox', '--disable-gpu', '--no-pdf-header-footer',
                        '--print-to-pdf=' + out, 'file://' + path], check=True, capture_output=True)
    finally:
        os.remove(path)


HEAD = ('<!doctype html><html><head><meta charset="utf-8"><link rel="stylesheet" href="fonts/fonts.css"><style>'
        '@page{size:%smm %smm;margin:0}*{box-sizing:border-box}body{margin:0;font-family:"Noto Sans KR",sans-serif}'
        '.p{position:relative;width:%smm;height:%smm;overflow:hidden;page-break-after:always}'
        '.v,.h{position:absolute;border:0 dashed ' + RED + '}.v{top:0;bottom:0;border-left-width:.35mm}'
        '.h{left:0;right:0;border-top-width:.35mm}'
        '.f{position:absolute;width:0;border-left:.35mm dashed ' + BLUE + '}'
        '.t{position:absolute;font-size:2.3mm;line-height:1;color:#444;white-space:nowrap;font-weight:700}'
        '.sc{position:absolute;font-size:3.4mm;line-height:1;color:' + RED + '}'
        '</style></head><body>')


def layout():
    w, h = 148 * SC, 105 * SC
    x0 = (A4W - 2 * w - GW) / 2
    y0 = (A4H - 2 * h - GH) / 2
    xs, ys = [x0, x0 + w + GW], [y0, y0 + h + GH]   # 위에서부터 (HTML 좌표)
    return w, h, xs, ys


def marks_html(labels):
    """labels: 자리(0..3)마다 글자 또는 None. 자르는 선·접는 선·글자를 그린 A4 한 쪽"""
    w, h, xs, ys = layout()
    o = ['<div class="p">']
    for x in sorted(set(xs + [a + w for a in xs])):
        o.append('<div class="v" style="left:%.2fmm"></div>' % x)
    for y in sorted(set(ys + [a + h for a in ys])):
        o.append('<div class="h" style="top:%.2fmm"></div>' % y)
    for j in range(4):
        x, y = xs[j % 2], ys[j // 2]
        mx = x + w / 2
        top = 0 if j < 2 else ys[0] + h          # 접는 선: 펼침면 위아래 여백에만
        o.append('<div class="f" style="left:%.2fmm;top:%.2fmm;height:%.2fmm"></div>' % (mx, top, y - top))
        bot = ys[1] if j < 2 else A4H
        o.append('<div class="f" style="left:%.2fmm;top:%.2fmm;height:%.2fmm"></div>' % (mx, y + h, bot - y - h))
        if labels[j]:
            o.append('<div class="t" style="left:%.2fmm;top:%.2fmm">%s</div>' % (x + 1.5, y + h + 1.2, labels[j]))
    o.append('<div class="sc" style="left:.8mm;top:%.2fmm">✂</div>' % (ys[0] - 3.9))
    o.append('<div class="t" style="left:%.2fmm;top:%.2fmm;color:%s">빨간 점선 = 자르기</div>'
             % (xs[1] + w - 26, ys[1] + h + 1.2, RED))
    o.append('<div class="t" style="left:%.2fmm;top:%.2fmm;color:%s">파란 점선 = 접기</div>'
             % (xs[1] + w - 51, ys[1] + h + 1.2, BLUE))
    o.append('</div>')
    return ''.join(o)


def place(pg, page, x_mm, y_top_mm):
    """A7 쪽을 A4 위 (x, 위에서 y)에 SC 배율로 (pypdf는 아래에서 위로)"""
    pw = float(page.mediabox.width)
    ph = float(page.mediabox.height)
    y = A4H * MM - (y_top_mm * MM + ph * SC)
    pg.merge_transformed_page(page, Transformation().scale(SC).translate(x_mm * MM, y))
    return pw


def build(pages, plan, out, here):
    """plan: A4 쪽마다 [(자리, 왼쪽 idx, 오른쪽 idx, 글자)]"""
    w, h, xs, ys = layout()
    ov = os.path.join(here, '.marks_tmp.pdf')
    html = HEAD % (A4W, A4H, A4W, A4H) + ''.join(
        marks_html([next((t for s, _l, _r, t in sheet if s == j), None) for j in range(4)])
        if any(t for *_x, t in sheet) else '<div class="p"></div>' for sheet in plan) + '</body></html>'
    html_to_pdf(html, ov, here)
    marks = PdfReader(ov).pages
    wr = PdfWriter()
    for k, sheet in enumerate(plan):
        pg = PageObject.create_blank_page(width=A4W * MM, height=A4H * MM)
        for slot, left, right, _t in sheet:
            x, y = xs[slot % 2], ys[slot // 2]
            for i, idx in enumerate((left, right)):
                if idx is not None and pages[idx] is not None:
                    place(pg, pages[idx], x + i * 74 * SC, y)
        pg.merge_page(marks[k])
        wr.add_page(pg)
    with open(out, 'wb') as fh:
        wr.write(fh)
    os.remove(ov)


def guide_pdf(out, here, n, sheets, a4):
    steps = [
        ('1. 인쇄', '<b>양면</b> · <b>짧은 쪽으로 넘기기</b> · <b>실제 크기(100%)</b>. "페이지에 맞춤"은 끄기. '
                   '처음엔 1장만 찍어 빛에 비춰 앞뒤 선이 겹치는지 확인'),
        ('2. 자르기', '<span style="color:%s">빨간 점선</span>을 따라 자르기. 한 장에서 4조각, 모두 %d조각' % (RED, sheets)),
        ('3. 겹치기', '번호는 여백에 있어서 자르면 없어져요. <b>A4 1장씩</b> 자르고 바로 '
                   '<b>왼쪽 위 → 오른쪽 위 → 왼쪽 아래 → 오른쪽 아래</b> 순서로 아래에 쌓기 '
                   '(종이 1이 맨 위 = 가장 바깥). 다음 A4 조각은 그 밑으로'),
        ('4. 접기', '<span style="color:%s">파란 점선</span>이 가리키는 가운데를 반으로. 손톱으로 꾹 눌러 주기' % BLUE),
        ('5. 묶기', '접힌 등에 <b>스테이플 2곳</b>. 스테이플러를 펼쳐 안쪽에서 찍거나 지우개 위에서. '
                   '고리를 달려면 표지 왼쪽 위 점선 동그라미에 구멍(5mm)'),
    ]
    rows = ''.join('<tr><th>%s</th><td>%s</td></tr>' % s for s in steps)
    html = (HEAD % (210, 297, 210, 297) + '<div class="p" style="padding:16mm 16mm;font-size:4mm;line-height:1.55">'
            '<h1 style="margin:0 0 2mm;color:#e3350d;font-size:8mm">다마포케 손바닥 가이드 만들기</h1>'
            '<p style="margin:0 0 6mm;color:#555">%d쪽 · 완성 약 68×97mm · A4 %d장(양면)</p>'
            '<table style="border-collapse:collapse;width:100%%">%s</table>'
            '<h2 style="font-size:5mm;margin:8mm 0 2mm">종이</h2><p style="margin:0">일반 80g 복사지로 충분해요. 100~120g이면 뒷면이 덜 비쳐요. '
            '표지만 두꺼운 종이로 하려면 종이 1이 들어간 A4 1장만 두꺼운 종이로.</p>'
            '<h2 style="font-size:5mm;margin:8mm 0 2mm">양면 인쇄가 안 되는 프린터</h2><p style="margin:0">'
            '<b>수동 양면</b>: 홀수 쪽(앞면)만 찍고 → 나온 종이 묶음을 그대로 <b>짧은 쪽 기준으로 뒤집어</b> 다시 넣고 → 짝수 쪽(뒷면). '
            '그래도 어려우면 <b>단면 풀칠</b> 파일: A4 %d장을 한 면만 찍고, 한 줄에 나란히 있는 "앞"과 "뒤" 조각을 '
            '자르자마자 "뒤" 조각을 책장 넘기듯 옆으로 뒤집어 <b>빈 뒷면끼리 풀로</b> 붙여요 (네 모서리를 맞춰서, 딱풀을 얇게). 위 줄 = 종이 1, 아래 줄 = 종이 2 … 마르면 3번부터 같아요.</p>'
            '<div style="margin-top:10mm;padding:3mm 4mm;background:#fde3dc;border-radius:2mm">'
            '쪽 순서가 이상하면 프린터가 "긴 쪽으로 넘기기"였을 수 있어요. 설정을 바꿔 다시 찍어 주세요.</div>'
            '</div></body></html>') % (n, a4, rows, a4 * 2)
    html = html.replace('<th>', '<th style="text-align:left;vertical-align:top;padding:2.5mm 4mm 2.5mm 0;'
                                'white-space:nowrap;color:#e3350d;border-bottom:.3mm solid #eee">') \
               .replace('<td>', '<td style="padding:2.5mm 0;border-bottom:.3mm solid #eee">')
    html_to_pdf(html, out, here)


def main():
    src, outdir = sys.argv[1], sys.argv[2]
    here = os.path.dirname(os.path.abspath(__file__))
    pages = list(PdfReader(src).pages)
    while len(pages) % 4:
        pages.append(None)
    n = len(pages)
    sheets = n // 4
    a4 = (sheets + 3) // 4

    # 양면: 종이 i 앞면 = (n-1-2i, 2i), 뒷면 = (2i+1, n-2-2i). 짧은 쪽 넘기기라 뒷면은 좌우 자리가 바뀜
    duplex = []
    for k in range(a4):
        for side in (0, 1):
            sheet = []
            for j in range(4):
                i = k * 4 + j
                if i >= sheets:
                    continue
                c, r = j % 2, j // 2
                if side == 0:
                    sheet.append((r * 2 + c, n - 1 - 2 * i, 2 * i, '종이 %d / %d' % (i + 1, sheets)))
                else:
                    sheet.append((r * 2 + (1 - c), 2 * i + 1, n - 2 - 2 * i, None))
            duplex.append(sheet)
    build(pages, duplex, os.path.join(outdir, 'TamaPoke_손바닥가이드_A4양면.pdf'), here)

    # 단면 풀칠: 한 줄에 종이 i의 앞 · 뒤 조각
    single = []
    for k in range((sheets + 1) // 2):
        sheet = []
        for r in range(2):
            i = k * 2 + r
            if i >= sheets:
                continue
            sheet.append((r * 2, n - 1 - 2 * i, 2 * i, '종이 %d 앞 (풀은 뒷면에)' % (i + 1)))
            sheet.append((r * 2 + 1, 2 * i + 1, n - 2 - 2 * i, '종이 %d 뒤' % (i + 1)))
        single.append(sheet)
    build(pages, single, os.path.join(outdir, 'TamaPoke_손바닥가이드_A4단면풀칠.pdf'), here)

    guide_pdf(os.path.join(outdir, 'TamaPoke_손바닥가이드_만드는법.pdf'), here, n, sheets, a4)
    print('%d쪽 -> 종이 %d조각 -> A4 %d장 양면 (단면 풀칠 %d장)' % (n, sheets, a4, len(single)))


if __name__ == '__main__':
    main()

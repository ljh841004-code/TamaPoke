#!/usr/bin/env python3
"""A7 쪽 PDF(74x105mm)를 집에서 인쇄해 만드는 중철 책자 배열로 바꾼다.

  python3 impose_a7.py 입력_A7.pdf 출력_A4.pdf

- A4 가로 한 면에 펼침면(A7 두 쪽 = 148x105mm) 4개 (2x2). 양면이라 A4 한 장 = A6 종이 4장 = 16쪽
- 인쇄: 양면, "짧은 쪽으로 넘기기", 실제 크기(100%). 프린터 여백 때문에 97%로 줄여서 놓아요
- 자르는 선(회색 모서리 표시)대로 4조각 → 종이 번호(①이 가장 바깥) 순서로 겹쳐 → 가운데 점선으로 접기
  → 접힌 등에 스테이플 2곳 (중철) → 표지 왼쪽 위 점선 동그라미에 구멍
쪽 수는 4의 배수 (모자라면 빈 쪽).
"""
import io
import sys

from pypdf import PageObject, PdfReader, PdfWriter, Transformation

MM = 72 / 25.4
A4W, A4H = 297 * MM, 210 * MM
SC = 0.97              # 프린터 여백
GAP = 6 * MM           # 펼침면 사이 (자르는 곳)


def marks_page(boxes, labels):
    """자르는 선(모서리 표시), 접는 선(점선), 종이 번호를 그린 A4 한 쪽 (작은 PDF를 직접 만들어 읽음)"""
    ops = ['0.6 G 0.3 w']
    L = 4 * MM
    for x, y, w, h in boxes:
        for cx, cy, dx, dy in ((x, y, -1, -1), (x + w, y, 1, -1), (x, y + h, -1, 1), (x + w, y + h, 1, 1)):
            ops.append('%.2f %.2f m %.2f %.2f l S' % (cx + dx * MM, cy, cx + dx * (MM + L), cy))
            ops.append('%.2f %.2f m %.2f %.2f l S' % (cx, cy + dy * MM, cx, cy + dy * (MM + L)))
        mx = x + w / 2  # 접는 선: 위아래 바깥에 짧은 점선
        ops.append('[1.5 1.5] 0 d %.2f %.2f m %.2f %.2f l S %.2f %.2f m %.2f %.2f l S [] 0 d'
                   % (mx, y - MM, mx, y - 4 * MM, mx, y + h + MM, mx, y + h + 4 * MM))
    ops.append('0.35 g')
    for x, y, t in labels:
        ops.append('BT /F1 7 Tf %.2f %.2f Td (%s) Tj ET' % (x, y, t))
    stream = '\n'.join(ops).encode('latin-1')
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>',
            b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
            ('<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] /Contents 4 0 R '
             '/Resources << /Font << /F1 5 0 R >> >> >>' % (A4W, A4H)).encode(),
            b'<< /Length %d >>\nstream\n' % len(stream) + stream + b'\nendstream',
            b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
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
    return PdfReader(io.BytesIO(out.getvalue())).pages[0]


def main():
    src, out = sys.argv[1], sys.argv[2]
    r = PdfReader(src)
    pages = list(r.pages)
    pw = float(pages[0].mediabox.width)
    ph = float(pages[0].mediabox.height)
    while len(pages) % 4:
        pages.append(None)
    n = len(pages)
    sheets = n // 4
    sw, sh = 2 * pw * SC, ph * SC                      # 펼침면 크기
    x0 = (A4W - 2 * sw - GAP) / 2
    y0 = (A4H - 2 * sh - GAP) / 2
    slot_xy = {(c, rr): (x0 + c * (sw + GAP), y0 + (1 - rr) * (sh + GAP)) for c in (0, 1) for rr in (0, 1)}
    w = PdfWriter()
    for a4 in range((sheets + 3) // 4):
        for side in (0, 1):
            pg = PageObject.create_blank_page(width=A4W, height=A4H)
            boxes, labels = [], []
            for j in range(4):
                i = a4 * 4 + j
                if i >= sheets:
                    continue
                c, rr = j % 2, j // 2
                if side == 0:
                    left, right = n - 1 - 2 * i, 2 * i
                else:
                    left, right = 2 * i + 1, n - 2 - 2 * i
                    c = 1 - c                              # 짧은 쪽 넘기기: 뒷면은 좌우가 바뀐 자리
                sx, sy = slot_xy[(c, rr)]
                for k, idx in enumerate((left, right)):
                    p = pages[idx]
                    if p is None:
                        continue
                    pg.merge_transformed_page(p, Transformation().scale(SC).translate(sx + k * pw * SC, sy))
                boxes.append((sx, sy, sw, sh))
                if side == 0:
                    labels.append((sx + sw / 2 - 6, sy - 3.4 * MM, 'sheet %d / %d' % (i + 1, sheets)))
            pg.merge_page(marks_page(boxes, labels))
            w.add_page(pg)
    with open(out, 'wb') as fh:
        w.write(fh)
    print('%d쪽 -> A6 종이 %d장 -> A4 %d장 (양면)' % (n, sheets, (sheets + 3) // 4))


if __name__ == '__main__':
    main()

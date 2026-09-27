#!/usr/bin/env python3
"""A5 쪽 PDF를 A4 가로 한 장에 2쪽씩, 반으로 접어 엮는 순서(중철)로 배열한다.

  python3 tools/impose.py 입력_A5.pdf 출력_A4.pdf

쪽 수는 4의 배수여야 한다 (모자라면 빈 쪽을 채움).
인쇄: 양면, "짧은 쪽으로 넘기기", 실제 크기(100%). 한꺼번에 반으로 접어 가운데를 스테이플.
"""
import sys

from pypdf import PageObject, PdfReader, PdfWriter, Transformation

MM = 72 / 25.4
A4W, A4H = 297 * MM, 210 * MM


def main():
    src, out = sys.argv[1], sys.argv[2]
    r = PdfReader(src)
    pages = list(r.pages)
    pw = float(pages[0].mediabox.width)
    ph = float(pages[0].mediabox.height)
    while len(pages) % 4:
        pages.append(None)
    n = len(pages)
    order = []
    for i in range(n // 4):
        order.append((n - 1 - 2 * i, 2 * i))          # 앞면: 왼쪽 = 뒤쪽 쪽, 오른쪽 = 앞쪽 쪽
        order.append((2 * i + 1, n - 2 - 2 * i))      # 뒷면
    w = PdfWriter()
    sx = (A4W / 2) / pw
    sy = A4H / ph
    s = min(sx, sy)
    for left, right in order:
        sheet = PageObject.create_blank_page(width=A4W, height=A4H)
        for slot, idx in enumerate((left, right)):
            p = pages[idx]
            if p is None:
                continue
            x0 = slot * A4W / 2 + (A4W / 2 - pw * s) / 2
            y0 = (A4H - ph * s) / 2
            sheet.merge_transformed_page(p, Transformation().scale(s).translate(x0, y0))
        w.add_page(sheet)
    with open(out, 'wb') as fh:
        w.write(fh)
    print(f'{n}쪽 -> A4 {len(order)}면 ({len(order) // 2}장)')


if __name__ == '__main__':
    main()

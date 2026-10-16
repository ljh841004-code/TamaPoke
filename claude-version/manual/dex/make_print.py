#!/usr/bin/env python3
"""인쇄소에 올리는 파일 (편집 없이 그대로 인쇄): CMYK 300dpi 그림만 든 PDF, 글꼴·투명 효과 없음

  인쇄용_몬도감_본문_<n>쪽.pdf          낱장 111x154mm (도련 3mm), 1쪽부터 순서대로
  인쇄용_몬도감_표지_책등<s>mm.pdf      표지 펼침 1면 (뒤표지 | 책등 | 앞표지), 책등 두께별
"""
import os, sys, zlib
import numpy as np
import pymupdf
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'a6'))
from make_a6 import to_cmyk, DPI, MM  # noqa: E402


def _png_up(c):
    """PNG 'Up' 필터 (행마다 바로 위 행과의 차): 흰 바탕·같은 색 면이 0이 되어 무손실로 아주 작아져요"""
    h, w, n = c.shape
    flat = c.reshape(h, w * n)
    diff = flat.copy()
    diff[1:] = (flat[1:].astype(np.int16) - flat[:-1]).astype(np.uint8)
    out = np.empty((h, w * n + 1), np.uint8)
    out[:, 0] = 2
    out[:, 1:] = diff
    return zlib.compress(out.tobytes(), 9)


def _raw_image(doc, pg, rect, c, pno, jpeg=False):
    """CMYK 배열 c 를 rect 에 넣어요. jpeg=False: 무손실 Flate + PNG 예측자 / True: 고화질 JPEG"""
    h, w, _ = c.shape
    cp = pymupdf.Pixmap(pymupdf.csCMYK, w, h, np.ascontiguousarray(c).tobytes(), False)
    if jpeg:
        pg.insert_image(rect, stream=cp.tobytes('jpeg', jpg_quality=95))
        return
    # 자리 그림은 쪽마다 달라야 해요 (같으면 PyMuPDF가 하나로 합쳐서 모든 쪽이 같은 그림이 돼요)
    ph = pymupdf.Pixmap(pymupdf.csCMYK, 1, 1, bytes([pno & 255, pno >> 8, 7, 9]), False)
    xref = pg.insert_image(rect, pixmap=ph)
    doc.update_stream(xref, _png_up(c), compress=False)
    for k, v in (('Width', str(w)), ('Height', str(h)), ('ColorSpace', '/DeviceCMYK'), ('BitsPerComponent', '8'),
                 ('Filter', '/FlateDecode'), ('DecodeParms', '<</Predictor 15/Colors 4/BitsPerComponent 8/Columns %d>>' % w),
                 ('SMask', 'null')):
        doc.xref_set_key(xref, k, v)


def convert(src, out):
    """쪽 전체를 300dpi CMYK 무손실 그림으로 (글자 · 그림 모두 그대로)"""
    doc = pymupdf.open()
    s = pymupdf.open(src)
    for pno, p in enumerate(s):
        pix = p.get_pixmap(dpi=DPI, alpha=False)
        a = np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width, 3)
        pg = doc.new_page(width=p.rect.width, height=p.rect.height)
        _raw_image(doc, pg, pg.rect, to_cmyk(a), pno)
    doc.save(out, garbage=3)
    return len(s)


def view_small(src, out, dpi=150):
    """보기용 (폰 · PC 화면): 150dpi JPEG 로 가볍게"""
    doc = pymupdf.open()
    for p in pymupdf.open(src):
        pix = p.get_pixmap(dpi=dpi, alpha=False)
        pg = doc.new_page(width=p.rect.width, height=p.rect.height)
        pg.insert_image(pg.rect, stream=pix.tobytes('jpeg', jpg_quality=88))
    doc.save(out, garbage=3, deflate=True)


def main():
    n = convert(os.path.join(HERE, '.dex_bleed.pdf'), os.path.join(HERE, '.body.pdf'))
    os.replace(os.path.join(HERE, '.body.pdf'), os.path.join(HERE, '인쇄용_몬도감_본문_%d쪽.pdf' % n))
    os.remove(os.path.join(HERE, '.dex_bleed.pdf'))
    for s in ('6', '6.5', '7', '7.5', '8', '8.5', '9'):
        src = os.path.join(HERE, '.cover_%s.pdf' % s)
        convert(src, os.path.join(HERE, '인쇄용_몬도감_표지_책등%smm.pdf' % s))
        os.remove(src)
    v = os.path.join(HERE, '다마포케_몬도감_A6_보기용.pdf')
    view_small(v, v + '.tmp')
    os.replace(v + '.tmp', v)
    print('본문 %d쪽 + 표지 7종' % n)


if __name__ == '__main__':
    main()

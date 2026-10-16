#!/usr/bin/env python3
"""다마포케 몬 도감 (A6 105x148mm, 무선 제본) HTML 만들기 - 펌웨어 데이터 그대로

  python3 gen_dex.py <dex.json> <그림 폴더(oaN.png)> <펌 버전> [bleed_mm]
    dex.json: test/render/run.sh 를 DEXJSON=<파일> 로 돌려서 (펌웨어의 진짜 값: 이름·타입·능력치·진화·
              출현 장소·레벨업 기술·기술머신·상성)
  -> dex.html (본문, 쪽마다 .pg) / cover.html (표지 펼침: 뒤표지 | 책등 | 앞표지)
그림은 저장소에 올리지 않아요 (개인 소장용).
"""
import json, os, sys, html

HERE = os.path.dirname(os.path.abspath(__file__))
TC = {'노말': '#9a9a72', '불꽃': '#ee7a2e', '물': '#5a86e8', '풀': '#5fb544', '전기': '#e0b000', '얼음': '#4fb8b8',
      '격투': '#c03028', '독': '#a040a0', '땅': '#c09a48', '비행': '#8c78e8', '에스퍼': '#f05080', '벌레': '#93a818',
      '바위': '#a89030', '고스트': '#6a5494', '드래곤': '#6a38f0', '악': '#6a5444', '강철': '#9898b8'}
RK = {3: ('전설', '#d49a10'), 2: ('희귀', '#9a64ee'), 1: ('일반', '#5fa86a'), 0: ('진화', '#8a8aa4')}
FRIEND = {172, 173, 174, 175, 42, 113}
STL = ('체력', '공격', '방어', '스피드')


def esc(s): return html.escape(str(s))


def iga(w):
    """받침 있으면 이, 없으면 가"""
    c = w[-1] if w else ''
    return '이' if '가' <= c <= '힣' and (ord(c) - 0xAC00) % 28 else '가'


def chip(t): return f'<span class="chip" style="background:{TC.get(t, "#999")}">{esc(t)}</span>'


def load(js):
    d = json.load(open(js, encoding='utf-8'))
    return {m['no']: m for m in d}


def evo_cond(D, frm, to):
    """frm -> to 진화 조건 글"""
    lv = D[frm]['evoLv']
    if frm == 133:
        if to == 196: return '친밀도 70 · 낮'
        if to == 197: return '친밀도 70 · 밤'
        return f'Lv{lv} · 셋 중 하나'
    s = f'Lv{lv}'
    if frm in FRIEND: s = f'친밀도 70 · Lv{lv}'
    if len(D[frm]['evo']) > 1: s += ' · 갈래'
    return s


def evo_line(D, n):
    m = D[n]
    parts = []
    p = m['prevo']
    if p:
        pp = D[p]['prevo']
        if pp: parts.append(f'{esc(D[pp]["name"])}<small>({evo_cond(D, pp, p)})</small>')
        parts.append(f'{esc(D[p]["name"])}<small>({evo_cond(D, p, n)})</small>')
    parts.append(f'<b class="me">{esc(m["name"])}</b>')
    nxt = m['evo']
    if nxt:
        parts.append(' / '.join(f'{esc(D[t]["name"])}<small>({evo_cond(D, n, t)})</small>' for t in nxt))
        if len(nxt) == 1 and D[nxt[0]]['evo']:
            t = nxt[0]
            parts.append(' / '.join(f'{esc(D[u]["name"])}<small>({evo_cond(D, t, u)})</small>' for u in D[t]['evo']))
    if len(parts) == 1: return '진화 없음'
    return ' <span class="ar">▶</span> '.join(parts)


def where_text(D, m):
    out = []
    for w in m['where']:
        s = esc(w['reg'])
        if w['badges']: s += f' <small>(배지 {w["badges"]}개)</small>'
        extra = [x for x in (w['time'], w['wx']) if x]
        if extra: s += ' · ' + ' · '.join(esc(x) for x in extra)
        if w['rare']: s += ' · <span class="rare">희귀 출현</span>'
        out.append(s)
    if not out: return '야생에서는 안 나와요 (알 · 진화로 만나요)'
    txt = ' / '.join(out[:3])
    if len(out) > 3: txt += f' 외 {len(out) - 3}곳'
    pre = []
    if m['via']: v = D[m["via"]]["name"]; pre.append(f'{esc(v)}{iga(v)} 나오는 곳에서 진화한 모습으로')
    if m['minLv']: pre.append(f'내 몬 Lv{m["minLv"]}↑')
    if pre: txt += ' <small>(' + ', '.join(pre) + ')</small>'
    return txt


def pick_moves(m, k=4):
    t = {m['t1'], m['t2']} - {''}
    mv = [x for x in m['moves'] if x[3] > 0] or list(m['moves'])  # 공격 기술이 없으면 (단데기 등) 있는 그대로
    seen, uniq = set(), []
    for x in mv:
        if x[1] in seen: continue
        seen.add(x[1]); uniq.append(x)
    best = sorted(uniq, key=lambda x: ((x[2] in t) * 200 + x[3] + x[0] * .5), reverse=True)[:k]
    return sorted(best, key=lambda x: x[0])


def tip(m):
    weak = [(t, q) for t, q in m['def'].items() if q >= 8]
    weak.sort(key=lambda x: -x[1])
    none = [t for t, q in m['def'].items() if q == 0]
    s = []
    if weak: s.append('약점&nbsp;' + ' '.join(chip(t) + ('<b class="x4">×4</b>' if q >= 16 else '') for t, q in weak))
    if none: s.append('<span style="white-space:nowrap">안&nbsp;통함 ' + ' '.join(chip(t) for t in none) + '</span>')
    return ' &nbsp; '.join(s)


def bar(lbl, v):
    return (f'<div class="st"><span>{lbl}</span><i><b style="width:{min(100, v * 100 // 160)}%"></b></i>'
            f'<em>{v}</em></div>')


def half(D, n, art):
    m = D[n]
    r = RK.get(m['rarity'], RK[0])
    chips = chip(m['t1']) + (chip(m['t2']) if m['t2'] else '')
    mv = ' · '.join(f'{esc(x[1])}<small>{x[0]}</small>' for x in pick_moves(m)) or '-'
    return f'''<div class="mon">
<div class="l"><div class="no">No.{n:03d}</div><img src="{art}/oa{n}.png"><div class="rk" style="background:{r[1]}">{r[0]}</div></div>
<div class="r"><div class="nm">{esc(m["name"])}</div><div class="ch">{chips}</div>
{"".join(bar(l, v) for l, v in zip(STL, m["st"]))}
<div class="ln"><b>진화</b> {evo_line(D, n)}</div>
<div class="ln"><b>장소</b> {where_text(D, m)}</div>
<div class="ln"><b>기술</b> {mv} <span class="tm">· 기술머신 {m["tm"]}개</span></div>
<div class="ln tip">{tip(m)}</div></div></div>'''


CSS = open(os.path.join(HERE, 'dex.css'), encoding='utf-8').read()


def page(inner, cls='', num=None):
    f = f'<div class="pn">{num}</div>' if num else ''
    return f'<section class="pg {cls}"><div class="in">{inner}</div>{f}</section>'


def build(js, art, ver, bleed):
    D = load(js)
    pages = []
    def add(inner, cls=''):
        n = len(pages) + 1
        side = 'odd' if n % 2 else 'even'
        pages.append(page(inner, f'{cls} {side}', n if 'nonum' not in cls else None))
    # 1 속표지
    add(f'''<div class="title"><div class="t1">다마포케</div><div class="t2">몬 도감</div>
<div class="t3">No.001 – 251 · 1세대 + 2세대</div><img class="hero" src="{art}/oa25.png">
<div class="t4">펌웨어 v1.17-{esc(ver)} 기준 · 개인 소장용</div></div>''', 'nonum')
    # 2 읽는 법
    add(f'''<h2>이 책 보는 법</h2>{half(D, 25, art)}
<ul class="how"><li><b>일반</b> 알에서 나올 수 있어요 · <b>진화</b> 진화해서 만나요 · <b>희귀</b> 드물게 나와요 · <b>전설</b> 아주 드물게 (내 몬 Lv40↑)</li>
<li><b>능력치</b> 막대 끝 = 160 (넘으면 꽉 차요)</li>
<li><b>진화</b> 괄호 안이 조건. <b>갈래</b>: 둘 이상 중 하나 (도감에 없는 쪽이 먼저)</li>
<li><b>장소</b> 기기의 지역 화면 이름 그대로. 배지가 필요한 곳은 (배지 N개)</li>
<li><b>기술</b> 레벨업으로 배우는 대표 기술 (숫자 = 배우는 레벨). 전체는 기기 도감 [기술표]</li>
<li><b>약점</b> 이 몬이 맞으면 2배 (×4 = 4배) · <b>안 통함</b> 맞아도 피해 0</li></ul>''')
    # 3-4 목차
    idx = [f'<div><span>{n:03d}</span>{esc(D[n]["name"])}<em>{{p{n}}}</em></div>' for n in range(1, 252)]
    add('<h2>차례</h2><div class="idx">' + ''.join(idx[:128]) + '</div>')
    add('<div class="idx" style="margin-top:2mm">' + ''.join(idx[128:]) + '</div>')
    pageof = {}
    def gen(title, sub, a, b, arts):
        add(f'''<div class="div"><div class="g1">{title}</div><div class="g2">{sub}</div>
<div class="arts">{"".join(f'<img src="{art}/oa{x}.png">' for x in arts)}</div></div>''', 'divpg nonum')
        ns = list(range(a, b + 1))
        for i in range(0, len(ns), 2):
            pair = ns[i:i + 2]
            for x in pair: pageof[x] = len(pages) + 1
            add(''.join(half(D, x, art) for x in pair) + ('<div class="mon memo">메모</div>' if len(pair) == 1 else ''))
    gen('1세대', '관동 · No.001 – 151', 1, 151, [1, 4, 7, 25, 150, 151])
    gen('2세대', '성도 · No.152 – 251', 152, 251, [152, 155, 158, 249, 250, 251])
    # 상성표: 펌웨어의 표 그대로 (dex.json.types.json, 반 단위 0/1/2/4)
    T = json.load(open(js + '.types.json', encoding='utf-8'))
    types = T['names']
    rows = ''
    for ai, a in enumerate(types):
        cells = ''.join('<td>' + {0: '<i class="z">✕</i>', 1: '<i class="h">▽</i>', 4: '<i class="s">●</i>'}.get(T['eff'][ai][di], '')
                        + '</td>' for di in range(len(types)))
        rows += f'<tr><th>{chip(a)}</th>{cells}</tr>'
    head = ''.join(f'<th class="v"><span style="background:{TC[t]}">{t}</span></th>' for t in types)
    add(f'''<h2>타입 상성표</h2><div class="cap">왼쪽 = 공격 기술 타입, 위 = 맞는 몬 타입 · ● 2배 ▽ 절반 ✕ 0</div>
<table class="tt"><tr><th></th>{head}</tr>{rows}</table>
<div class="cap">두 타입 몬은 곱해요 (예: 리자몽 불꽃·비행 ← 바위 ×4)</div>''')
    while len(pages) % 4: add('<h2>나의 기록</h2><div class="lines">' + '<div></div>' * 18 + '</div>')
    body = '\n'.join(pages)
    for n, p in pageof.items(): body = body.replace('{p%d}' % n, str(p))
    open(os.path.join(HERE, 'dex.html'), 'w', encoding='utf-8').write(doc(body, bleed, 105, 148))
    return len(pages)


def doc(body, bleed, w, h):
    return (f'<!doctype html><html><head><meta charset="utf-8"><title>다마포케 몬 도감</title>'
            f'<link rel="stylesheet" href="fonts/fonts.css"><style>:root{{--b:{bleed}mm;--w:{w}mm;--h:{h}mm}}'
            f'@page{{size:calc({w}mm + {2 * bleed}mm) calc({h}mm + {2 * bleed}mm);margin:0}}{CSS}</style></head>'
            f'<body>{body}</body></html>')


def cover(js, art, ver, bleed, spine):
    D = load(js)
    W = 105 * 2 + spine
    front = f'''<div class="cf"><div class="ct">다마포케 몬 도감</div><div class="cs">No.001 – 251 · 1세대 + 2세대</div>
<img style="left:6mm;top:44mm;width:52mm" src="{art}/oa25.png"><img style="right:4mm;top:40mm;width:46mm" src="{art}/oa6.png">
<img style="left:20mm;top:84mm;width:40mm" src="{art}/oa150.png"><img style="right:8mm;top:88mm;width:42mm" src="{art}/oa249.png">
<div class="cb">TAMAPOKE · 펌웨어 v1.17-{esc(ver)} 기준 · 개인 소장용</div></div>'''
    back = f'''<div class="ck"><div class="kt">251마리 모두 한 권에</div>
<div class="kl">능력치 · 진화 조건 · 만나는 장소<br>대표 기술 · 약점 · 타입 상성표</div>
<div class="kg">{"".join(f'<img src="{art}/oa{x}.png">' for x in (1, 4, 7, 152, 155, 158, 133, 143, 131, 196, 197, 248))}</div>
<div class="cb">다마포케 사용 설명서와 같은 A6 크기</div></div>'''
    sp = f'<div class="sp"><span>다마포케 몬 도감 · No.001–251</span></div>'
    body = (f'<section class="cv" style="width:calc({W}mm + 2*var(--b));height:calc(148mm + 2*var(--b))">'
            f'<div class="cvi" style="grid-template-columns:calc(105mm + var(--b)) {spine}mm calc(105mm + var(--b))">'
            f'{back}{sp}{front}</div></section>')
    open(os.path.join(HERE, f'cover_{spine}.html'), 'w', encoding='utf-8').write(doc(body, bleed, W, 148))


if __name__ == '__main__':
    js, art, ver = sys.argv[1], sys.argv[2], sys.argv[3]
    b = float(sys.argv[4]) if len(sys.argv) > 4 else 0
    n = build(js, art, ver, b)
    for s in (6, 6.5, 7, 7.5, 8, 8.5, 9):
        cover(js, art, ver, b, s)
    print(n, 'pages')

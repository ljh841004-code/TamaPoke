#!/usr/bin/env python3
"""몬 도감 주문 안내서 (A4) HTML: python3 order.py <쪽수> <펌 버전>"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
n, ver = int(sys.argv[1]), sys.argv[2]
css = open(os.path.join(HERE, '..', 'a6', '주문서_A6.html'), encoding='utf-8').read()
css = css[css.find('<style'):css.find('</style>') + 8]
spines = ['6', '6.5', '7', '7.5', '8', '8.5', '9']
rows = ''.join(f'<tr><td>{s} mm</td><td>인쇄용_몬도감_표지_책등{s}mm.pdf</td><td>{210 + float(s):g} × 148 (작업 {216 + float(s):g} × 154)</td></tr>' for s in spines)
html = f'''<!doctype html><html><head><meta charset="utf-8"><title>다마포케 몬 도감 주문서</title>
<link rel="stylesheet" href="fonts/fonts.css">{css}</head><body>
<h1>다마포케 몬 도감 A6 — 주문서</h1>
<p class="sub">디티피아 · 소량 디지털 책자 (dtpia.co.kr/Order/Book/CommonBookDigital.aspx) · <b>무선 제본</b> · 펌웨어 v1.17-{ver} 기준 · 내지 {n}쪽 + 표지 · 사용 설명서(A6)와 같은 크기</p>

<h2>1. 책 정보</h2>
<table>
<tr><th>완성 크기</th><td class="big">105 × 148 mm (A6) · 세로형</td></tr>
<tr><th>작업 크기</th><td>111 × 154 mm (사방 도련 3mm 포함)</td></tr>
<tr><th>쪽 수</th><td><b>내지 {n}쪽</b> (1쪽 속표지 … {n}쪽 나의 기록) + <b>표지 따로</b> (뒤표지 | 책등 | 앞표지 한 장)</td></tr>
<tr><th>제본</th><td><b>무선 제본</b> (책등을 풀로 붙이는 방식), 세로형 좌철</td></tr>
<tr><th>색</th><td>내지 양면 컬러 · 표지 단면(바깥) 컬러</td></tr>
</table>

<h2 class="g">2. 옵션표 입력값</h2>
<table>
<tr><th>규격</th><td><b>비규격</b> → 재단사이즈 <b>105*148</b> · 작업사이즈 <b>111*154</b></td></tr>
<tr><th>세네카(책등)</th><td>인쇄소가 알려 주는 값 (보통 6~9mm). <b>아래 3번 표에서 같은 숫자의 표지 파일</b>을 골라요</td></tr>
<tr><th>표지사이즈</th><td>펼친 크기 = 105 + 세네카 + 105 (예: 세네카 7mm → <b>217*148</b>)</td></tr>
<tr><th>주문수량</th><td>원하는 권수</td></tr>
<tr><th>표지</th><td><b>고급지</b> (아트지 또는 스노우지 250g 안팎) / <b>단면</b> / 칼라 / <b>단면 코팅</b> (유광 = 잡지처럼 반짝 · 무광 = 차분)</td></tr>
<tr><th>본문1 내지(컬러)</th><td><b>고급지</b> (스노우지 100g, 또는 120g) / <b>양면</b> / 칼라 / <b>{n}페이지</b></td></tr>
<tr><th>본문2 내지(흑백)</th><td>0페이지</td></tr>
<tr><th>면지</th><td>없음</td></tr>
<tr><th>제본</th><td><b>무선</b> · 제본방향 세로형 좌철</td></tr>
</table>
<p class="small">스노우지는 매끈하고 그림 색이 선명해서 컬러 잡지 같은 느낌이에요. 120g은 조금 더 두껍고 비침이 적은 대신 책등이 약 1mm 두꺼워져요.</p>

<h2 class="p">3. 올릴 파일 (편집 없이 그대로 인쇄)</h2>
<table>
<tr><th>본문</th><td><b>인쇄용_몬도감_본문_{n}쪽.pdf</b> — {n}쪽 낱장, 1쪽부터 순서대로</td></tr>
<tr><th>표지</th><td><b>세네카 값과 같은 숫자</b>의 파일 1개만:</td></tr>
</table>
<table><tr><th>세네카</th><th>표지 파일</th><th>펼친 크기 (mm)</th></tr>{rows}</table>
<p class="small">올리지 말 것: 다마포케_몬도감_A6_보기용.pdf (화면 확인용, 도련 없음), 이 주문서. 파일 여러 개를 한 번에 올려야 하면 ZIP 하나로 묶어요.</p>

<h2 class="k">4. 파일 점검 결과</h2>
<table>
<tr><th>색</th><td>CMYK (SWOP 인쇄 프로필) ✔</td></tr>
<tr><th>해상도</th><td>300 dpi, 무손실 ✔</td></tr>
<tr><th>잉크 총량</th><td>가장 진한 곳 약 240% (한도 250%) ✔</td></tr>
<tr><th>글꼴 · 효과</th><td>글꼴 없음 (모두 그림 = 아웃라인과 같음), 투명 · 그림자 효과 없음 ✔</td></tr>
<tr><th>재단 여유</th><td>도련 3mm · 글자와 그림은 재단선에서 바깥 5mm, 제본 쪽 9mm 이상 안쪽 (무선 제본은 안쪽이 덜 펴져요) ✔</td></tr>
<tr><th>쪽 확인</th><td>{n}쪽 모두 넘치거나 잘린 곳 없음 (자동 검사 + 눈 검사) ✔ · 쪽 수 4의 배수 ✔</td></tr>
</table>

<h2 class="d">5. 주문 전에 문의할 것 (게시판 / 전화에 그대로 붙여 넣기)</h2>
<div class="msg">안녕하세요. 소량 디지털 책자 무선 제본 주문 문의드립니다.
- 크기: 비규격 105 x 148 mm (A6), 세로형 좌철, 무선 제본
- 내지: {n}쪽, 양면 칼라, 스노우지 100g / 표지: 250g 단면 칼라, 단면 코팅
- 파일: 도련 3mm 포함, CMYK 300dpi PDF (본문 {n}쪽 한 파일, 표지 펼침 한 파일)
1. 이 크기로 무선 제본이 가능한가요?
2. 이 종이와 쪽수일 때 세네카(책등) 두께는 몇 mm인가요? (6~9mm 사이 0.5mm 단위로 표지 파일을 준비해 두었어요)
3. 표지 파일은 [뒤표지 | 책등 | 앞표지] 펼침 한 장, 도련 3mm 포함으로 올리면 되나요?
4. 본문 파일이 약 46MB인데 그대로 올려도 되나요?
감사합니다.</div>
<p class="small">답을 받으면: 2번 숫자와 같은 표지 파일을 올려요 (예: "7mm" → 인쇄용_몬도감_표지_책등7mm.pdf). 0.5mm 사이 값(예: 7.2mm)이면 가까운 쪽(7mm)을 골라요. 4번이 안 되면 "웹하드/메일로 받아 주실 수 있나요?"라고 물어보세요.</p>

<h2>6. 결제 직전 확인</h2>
<ul class="ck">
<li>규격: 비규격 105*148 / 111*154</li>
<li>제본: 무선 · 세로형 좌철</li>
<li>본문1 컬러 {n}페이지, 본문2 흑백 0페이지</li>
<li>표지: 단면 칼라 · 코팅 · 세네카 값과 표지 파일 숫자가 같은지</li>
<li>올린 파일 이름이 "인쇄용_" 으로 시작하는지</li>
<li>견적 금액과 수량 확인 후 결제</li>
</ul>

<h2 class="g">7. 받은 뒤 확인</h2>
<ul class="ck">
<li>책등 글자 "다마포케 몬 도감 · No.001–251"이 가운데에 있는지</li>
<li>쪽 순서: 1쪽 속표지 → 이 책 보는 법 → 차례 → 1세대 → … → 타입 상성표 → 나의 기록</li>
<li>잘린 글자가 없는지 (특히 쪽 번호, 맨 아래 줄)</li>
<li>문제가 있으면 사진을 찍어 바로 문의</li>
</ul>
<p class="small">몬 그림은 개인이 즐기는 용도예요. 판매용으로 찍지 마세요. 펌웨어가 바뀌어 몬 정보가 달라지면 도감도 새로 만들어 드려요.</p>
</body></html>'''
open(os.path.join(HERE, '주문서_몬도감.html'), 'w', encoding='utf-8').write(html)

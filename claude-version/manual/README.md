# 다마포케 사용 설명서 (A5 책자)

펌웨어 **v1.17-ko11.7** 기준, 모든 기능을 담은 36쪽 컬러 설명서예요.

| 파일 | 용도 |
|---|---|
| `TamaPoke_사용설명서_A5.pdf` | 화면으로 보기 / A5 용지에 바로 인쇄 (36쪽) |
| `TamaPoke_사용설명서_A4접지용.pdf` | **A4 용지에 인쇄해서 반으로 접는 책자** (A4 9장, 양면) |
| `manual.html`, `style.css` | 원본 (브라우저로 열어 볼 수 있어요) |
| `img/` | 화면 사진 (PC 렌더러로 그린 실제 펌웨어 화면) |
| `build.sh` | PDF 다시 만들기 (Chromium + python3 + pypdf) |

## A4 접지용 인쇄 방법

1. `TamaPoke_사용설명서_A4접지용.pdf`를 열고 인쇄
2. 용지 **A4**, 방향 **가로**, 크기 **실제 크기(100%)** (“페이지에 맞춤” 끄기)
3. **양면 인쇄 → 짧은 쪽으로 넘기기** (Flip on short edge)
   - 양면 인쇄가 안 되는 프린터: 홀수 쪽만 인쇄 → 종이 묶음을 그대로 뒤집어 다시 넣고 짝수 쪽 인쇄
     (프린터마다 넣는 방향이 달라요. 먼저 2장으로 시험해 보세요)
4. 9장을 순서대로 겹친 채 **한꺼번에 반으로 접기** → 접힌 가운데를 스테이플러 2곳 (중철)
5. 1쪽(표지)이 앞, 36쪽(뒤표지)이 뒤로 오면 성공

## 다시 만들기

글을 고친 뒤:

```sh
./build.sh                                   # 글꼴은 그대로
./build.sh <@fontsource/noto-sans-kr/files>  # 새 글자를 썼으면 글꼴도 다시 (fonttools 필요)
```

화면 사진은 `TamaPoke/test/render/run.sh`로 새로 그려서 `img/`에 복사해요.

## 펌 배포 묶음 (ko12.10.1부터)

**사용자가 쓰는 기능이 추가된 펌**(또는 설명서를 요청받았을 때)만 zip에 **펌 파일 4개 + QR 설명서 `manual.pdf` + A6 설명서(인쇄용)**를 같이 넣어요. 오류 수정만 한 펌은 펌 파일 4개만:
`TamaPoke/tools/pack_release.sh <버전> <결과 폴더> <렌더 shots> [fontsource/files]`. 설명서 표지 버전은 펌의 FW_VERSION에서 저절로 들어가요.
와이파이 업데이트 화면(`…/fw`)에서 update.bin과 manual.pdf를 같이 고르면 QR 설명서도 새 버전으로 바뀌어요.

## A6 사용 설명서 (인쇄소 주문용, 펌웨어 v1.17-ko12.9.27)

손바닥 가이드(A7)를 그대로 √2배 키운 105×148mm, 60쪽(표지 4 + 본문 56) 중철 책자예요. 글은 `pocket/gen_pocket.py` 하나로 두 책이 같이 바뀌어요.

```sh
pocket/build_pocket.sh <렌더 결과 shots> [fontsource/files]   # 먼저 손바닥 가이드
python3 a6/make_a6.py                                         # 그다음 A6
```

`a6/` 에 생기는 파일: `인쇄용_A6_전체_60쪽.pdf`(인쇄소에 올리는 파일: CMYK 300dpi, 글꼴 없음, 잉크 240% 이하, 도련 3mm),
`인쇄용_A6_본문_56쪽.pdf` + `인쇄용_A6_표지_펼침.pdf`(표지·본문 따로 올릴 때), 확인용 `TamaPoke_사용설명서_A6.pdf`, 주문 옵션 메모 `인쇄소_사양_A6.txt`.

## 몬 도감 (dex/, A6 무선 제본 136쪽)

251마리 한 권 (2마리/쪽): 능력치 · 진화 조건 · 만나는 장소 · 대표 기술 · 약점 + 타입 상성표. 값은 모두 펌웨어에서 뽑아요.

1. 데이터: `cd TamaPoke && DEXJSON=<dex.json> bash test/render/run.sh <SD>` (dex.json + dex.json.types.json)
2. 그림: 공식 일러스트 oa1.png … oa251.png 를 한 폴더에 (약 380px로 줄여서). 그림은 저장소에 올리지 않아요 (개인 소장용)
3. `dex/build.sh <dex.json> <그림 폴더> <펌 버전> [fontsource/files]`
   -> 다마포케_몬도감_A6_보기용.pdf, 인쇄용_몬도감_본문_136쪽.pdf, 인쇄용_몬도감_표지_책등6~9mm.pdf (0.5mm 단위),
      주문안내_몬도감_인쇄소.pdf. 넘치는 칸이 있으면 멈춰요 (check.js)

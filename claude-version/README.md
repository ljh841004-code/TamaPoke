# TamaPoke KO v1.17-ko7 (클로드 버전)

> 이 폴더는 **클로드(Claude) 버전**이에요. 저장소 루트의 TamaPoke v1.23과는 따로 관리하는 별도 갈래예요.
> v1.23의 어드벤처·컬렉션 기능은 이 버전에 없어요. (소리와 한글 글꼴은 옮겨 왔어요)

원본 [socquique/TamaPoke](https://github.com/socquique/TamaPoke) v1.17을 바탕으로
야생 배틀, 통신 대전/교환(ESP-NOW), WiFi 시간 자동 맞춤, 한국어 기본, SD카드 소리(배경음·배틀 배경음·울음소리),
보관함·도감·배틀 아이템·훈련 3종·소리 설정·실시간 저장·큰 시계·SD카드 업데이트,
경험치 레벨(ko7)과 타입별 기술 이펙트(ko7)를 더했어요.
화면 모습은 `screenshots/` 폴더에 있어요 (PC 렌더러로 그린 실제 화면).

## 폴더 구성

| 파일 | 내용 |
|---|---|
| `update.bin` | **SD카드 업데이트용**. 웹 설치 페이지로 보내고 [SD 업데이트] (아래 A) |
| `tamapoke-ko-v1.17-ko7-app-0xe000.bin` | **esptool 업데이트용**. 주소 **0xe000**. 포켓몬은 그대로 남아요 |
| `tamapoke-ko-v1.17-ko7.bin` | **새로 설치용** 통합 이미지. 주소 **0x0**. 저장 데이터가 초기화돼요 |
| `TamaPoke_v1.17-ko7_정리.txt` | 기능·사용법 요약 메모 |
| `TamaPoke/` | 전체 소스 (Arduino 스케치 폴더). 자세한 설명은 `TamaPoke/README_KO.md` |

## SD카드에 파일 넣기 (울음소리, 배경음, update.bin)

1. 보드가 평소 화면인 상태에서 USB로 PC에 연결 (esptool 등 포트를 쓰는 창은 닫기)
2. Chrome에서 [웹 설치 페이지](https://socquique.github.io/TamaPoke/web/) → **2단계 Connect board** → 포트 선택
   (**Load sprites**는 누르지 않기: 스프라이트 40MB를 다시 받아요)
3. **pick them manually** → 파일 형식을 **"모든 파일 (\*.\*)"**로 바꾸고 파일 선택 → SD카드 `mons` 폴더에 저장돼요

## 펌웨어 올리기

### A. SD카드로 (ko5.1부터, 추천)
1. 위 방법으로 `update.bin`을 보내요 (`mons/update.bin`이 돼요)
2. 아래로 스와이프 → WiFi → **[SD 업데이트]** → **[업데이트]**
3. 진행 막대가 끝나면 자동으로 다시 켜져요. 포켓몬은 그대로, 다 쓴 파일은 `update_done.bin`으로 바뀌어요
   (새 펌웨어를 예비 영역에 다 쓰고 검사가 끝난 뒤에만 바꿔요. 도중에 실패하면 기존 펌웨어 그대로)

### B. esptool로
1. Chrome/Edge에서 [ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/) → Baudrate **115200** → Connect → 포트 선택
2. 주소 **`0xe000`**에 `tamapoke-ko-v1.17-ko7-app-0xe000.bin` → Program (**Erase Flash 누르지 않기**)
   (0x10000이 아니라 0xe000: SD 업데이트 뒤에도 esptool로 올린 펌웨어가 켜지게 부팅 선택 정보를 같이 써요.
   이 파일을 0x10000이나 0x0에 올리면 켜지지 않아요)
3. 처음 설치하거나 초기화하고 싶다면: 주소 `0x0`에 `tamapoke-ko-v1.17-ko7.bin`
4. 끝나면 USB를 뽑고 전원 버튼을 6초 눌러 껐다 켜기. 시계 화면 아래쪽에 `v1.17-ko7`가 보이면 성공

> **ko3~ko5.1에서 ko6으로 올릴 때만** esptool이 바로 연결되지 않아요 (그 버전들의 USB 방식 때문).
> 장치 관리자에서 COM 번호 확인 → esptool 창을 닫고 PowerShell에서
> `$p = New-Object System.IO.Ports.SerialPort COM5,1200; $p.Open(); Start-Sleep 1; $p.Close()`
> → 화면이 꺼지면 esptool에서 Connect. **ko6부터는 원래 USB 방식이라 Connect만 누르면 돼요.**

> **켜지지 않을 때 (복구)**: esptool에서 파일 3개를 저장 영역을 건드리지 않고 올려요 —
> `0x0` 부트로더, `0x8000` 파티션 표, `0xe000` app 파일 (빌드 폴더의 `TamaPoke.ino.bootloader.bin`,
> `TamaPoke.ino.partitions.bin`, 또는 통합 이미지에서 잘라낸 것).

## 레벨과 진화 (ko7)

- 레벨은 **경험치(EXP)**로 올라요 (최대 **Lv.100**). 원작처럼 레벨 n이 되려면 n³ EXP.
  - **배틀**: 이기거나 잡으면 상대 종류 × 상대 레벨만큼 EXP (통신 대전은 절반, 지면 0)
  - **시간**: 깨어 있고 모든 게이지가 40 이상이면 1시간마다 레벨 1/4만큼
  - 진행도 카드(위로 스와이프 → 4쪽)에 파란 EXP 막대와 "다음 레벨까지 ○ EXP"
- **진화 레벨**: 3단계 포켓몬은 기본형 → 1차 진화 **Lv.16**, 최종 진화는 원작 레벨
  (예: 파이리 16 → 리자드 36, 미뇽 16 → 신뇽 55. 원작이 20 미만이면 20).
  2단계 포켓몬은 원작 레벨 그대로 (예: 피카츄 30, 꼬렛 20). 돌봄 실수 1번마다 1레벨 늦어져요
- 야생 포켓몬도 내 레벨 −4 ~ +1 (최대 100)
- **ko6 이하에서 올리면** 지금 포켓몬은 종류는 그대로, **Lv.1부터** 다시 시작해요.
  보관함에서 Lv.100이 넘던 포켓몬은 Lv.5가 돼요
- ko6의 Lv.338 원인: 저장 기록이 없을 때 WiFi 시간 맞춤이 "2026-01-01부터 꺼져 있었다"로 보고
  2주치 시간을 한꺼번에 더했어요. ko7에서 고쳤어요

## 배틀 이펙트 (ko7)

타입 기술마다 이펙트가 달라요 (불꽃: 불씨와 불길, 물: 물방울과 물보라, 풀: 덩굴채찍, 전기: 번개,
얼음: 냉동빔과 얼음 결정, 독: 거품, 땅: 진흙과 흙먼지, 에스퍼: 고리, 벌레: 바늘, 바위: 떨어지는 돌,
고스트: 그림자, 드래곤: 푸른 불꽃, 노말·격투·몸통박치기: 충격). **급소**는 화면이 흔들리고,
**효과가 굉장했다**는 흰 충격파가 퍼져요. 모습은 `screenshots/21_battle_fx.png`.

## 사이트가 닫히면 (대처 방법)

웹 설치 페이지나 esptool-js 사이트가 없어져도 올릴 수 있어요:

1. **웹 설치 페이지를 내 PC에서 열기**: 이 저장소의 `TamaPoke/web/` 폴더에서
   `python -m http.server 8000` → Chrome에서 `http://localhost:8000` (Web Serial은 localhost에서도 돼요)
2. **Espressif Flash Download Tool** (Windows 프로그램, 인터넷 필요 없음): ESP32-S3 선택 →
   파일과 주소(위 표) 입력 → START
3. **esptool (파이썬)**: `pip install esptool` →
   `esptool --chip esp32s3 write-flash 0xe000 tamapoke-ko-v1.17-ko7-app-0xe000.bin`
4. **Arduino IDE**로 `TamaPoke/` 소스를 직접 올리기 (`TamaPoke/README_KO.md`의 설정 참고)
5. **SD카드에 파일 보내기**: SD카드를 빼서 PC에 꽂고 `mons` 폴더에 직접 복사하거나,
   `.bin` 파일(update.bin 등)은 `TamaPoke/tools/sdcard/mons/`에 두고
   `python TamaPoke/tools/send_sd.py --port COM5` (pyserial 필요)로 USB로 보내요
6. 켜지지 않을 때는 위 "복구" (파일 3개)

## 소리

SD카드 `mons` 폴더에 `bgm.wav`(평소), `battle_wild.wav`(배틀 중), `cry001.wav`~`cry151.wav`(울음소리).
형식은 **16kHz 모노 16비트 WAV**, 변환 방법은 `TamaPoke/README_KO.md`의 "SD카드 소리"를 보세요.

최신 울음소리: `TamaPoke/tools/get_cries.py`를 실행하면 PokeAPI에서 1~151번의 최신 울음소리를 받아
`mons/cry001.wav`~`cry151.wav`로 변환해요 (Python + ffmpeg 필요). 게임 원본 음원(© Nintendo/Game Freak)이라
**개인용으로만** 쓰고 저장소에는 올리지 않아요.

## 버전 기록

- **ko7** – 경험치 레벨 (배틀 + 시간, 최대 Lv.100, 1차 진화 Lv.16 / 최종 진화 원작 레벨),
  Lv.338 버그 수정 (WiFi 시간 맞춤이 시간을 한꺼번에 더하던 문제), 타입별 배틀 이펙트·급소 흔들림·
  효과 굉장 충격파, 배틀 결과에 EXP 표시, WiFi 설정 페이지에서 **WiFi 목록 선택** + 다시 검색
- **ko7** – 시계 화면의 버전 표시가 잘리던 문제 수정 ("ko6.1"이 "ko6."로 보였음),
  SD 업데이트 화면에 현재 버전과 **파일 안의 버전** 표시 (같으면 "이미 같은 버전이에요")
- **ko6.1** – [WiFi 설정하기]가 켜지자마자 꺼지던 버그 수정 (원본 ko1부터 있던 시간 계산 오류).
  같은 오류로 통신 2인이 바로 "연결이 끊겼어요"가 될 수 있던 것도 수정
- **ko6** – USB 드라이브 모드 제거 (실기기에서 동작하지 않았음), USB 방식을 원래대로 되돌림
  (esptool Connect가 다시 바로 됨). 파일은 웹 설치 페이지로, 업데이트는 SD카드(`mons/update.bin`)로
- **ko5.1** – SD 업데이트가 `mons/update.bin`도 찾음 (웹 설치 페이지로 보낼 수 있게)
- **ko5** – SD카드 업데이트, 배틀 승리 시 20% 확률로 보관함 합류(포획은 항상), 도감 두 번 탭 나가기,
  화면 끈 동안 배경음 멈춤, 최신 울음소리 스크립트, 영문/숫자 줄 맞춤
- **ko4** – 큰 시계, 보관함(다음 육성 포켓몬), 도감 기록, 소리 설정 메뉴, 훈련 3종, 실시간 저장,
  배틀 아이템(물약·포켓볼·포획), 패배 불이익 없음, 검정 글자·Noto 한글, 게이지 수치
- **ko3** – (ko6에서 제거) SD카드 USB 드라이브
- **ko2** – SD카드 소리 추가 (v1.23에서 이식), 시리얼 `VOL` 명령, SD카드 접근 잠금
- **ko1** – 야생 배틀, 통신 대전/교환, WiFi 시간 맞춤, 한국어 기본

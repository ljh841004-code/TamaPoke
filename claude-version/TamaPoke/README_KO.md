# TamaPoke KO (v1.17-ko1)

[socquique/TamaPoke](https://github.com/socquique/TamaPoke) v1.17을 바탕으로 기능을 더한 포크입니다.
보드는 원본과 같은 **Waveshare ESP32-S3-Touch-AMOLED-1.75** (표준 또는 -G)입니다.

## 추가된 기능

| 기능 | 내용 |
|---|---|
| 🇰🇷 한국어 기본 | 처음 켜면 한국어로 시작해요. 시간 화면(아래로 스와이프)에서 언어 바꾸기 가능 |
| 🕒 WiFi 시간 자동 맞춤 | 켤 때와 하루 한 번 NTP로 시계를 맞춰요 (기본 UTC+9). 끝나면 WiFi를 바로 꺼서 배터리 절약 |
| ⚔️ 야생 배틀 | 턴제 배틀. 몸통박치기 / 타입 기술 / 방어 / 도망. 1세대 타입 상성 적용 |
| 🔗 통신 대전 (2인) | TamaPoke 두 대를 ESP-NOW로 연결해 자동 대전 (공유기 필요 없음) |
| 🔁 통신 교환 (2인) | 포켓몬을 서로 교환. 윤겔라·근육몬·데구리·고우스트는 교환하면 진화 |

## 설치 (한 번만)

1. **스프라이트 넣기** – [원본 웹 설치 페이지](https://socquique.github.io/TamaPoke/web/)에서
   1단계 Install → 2단계 Load sprites 로 SD카드에 스프라이트를 넣어요. (스프라이트 형식은 원본과 같아요)
2. **이 펌웨어 올리기** – 아래 중 하나:
   - **브라우저로:** `tamapoke-ko.bin`을 [ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/)에서
     주소 **0x0**에 올리기 (Chrome/Edge)
   - **로컬 설치 페이지로:** 이 소스의 `web` 폴더에서 `python3 -m http.server 8000` 실행 →
     Chrome에서 `http://localhost:8000` 열고 Install (펌웨어가 이 포크 버전으로 바뀌어 있어요.
     zip에는 스프라이트 묶음 `sprites.pak`(40MB)이 빠져 있으니 스프라이트는 1번처럼 원본 페이지에서 넣어요)
   - **Arduino IDE로:** 보드 *ESP32S3 Dev Module*, Flash 16MB, PSRAM **OPI PSRAM**,
     Partition *16M Flash (3MB APP/9MB FATFS)*, USB CDC On Boot *Enabled*.
     라이브러리: GFX Library for Arduino, SensorLib, XPowersLib, U8g2. ESP32 코어 3.3.x

> 이미 원본을 쓰고 있었다면 2단계만 하면 돼요. 키우던 포켓몬·도감은 그대로 남아요
> (단, "Erase device"를 체크하면 지워져요).

## 사용법

### WiFi 시간 맞추기
1. 메인 화면에서 **아래로 스와이프** → 시간 설정 화면 → 가운데 **WiFi** 버튼
2. **WiFi 설정하기** → 화면에 나온 `TamaPoke-XXXX` WiFi에 휴대폰으로 연결 (비밀번호 `tamapoke`)
3. 설정 페이지가 자동으로 열려요 (안 열리면 브라우저에서 `192.168.4.1`) → 집 WiFi 이름·비밀번호·시간대 입력 → 저장
4. 바로 시간을 맞추고, 이후엔 켤 때마다 + 하루 한 번 자동으로 맞춰요

※ **2.4GHz WiFi만** 됩니다 (ESP32 제한). 시리얼로도 설정 가능: `WIFI 이름|비밀번호`

### 야생 배틀
- **위로 스와이프** (스탯 카드) → 옆으로 넘겨 **배틀** 페이지 → **야생 배틀**
- 또는 가끔 메인 화면에 뜨는 초록색 **"! 야생 포켓몬 출현 !"** 버튼을 탭 (5분 뒤 사라져요)
- 이기면 공격/방어/속도 훈련치 상승 + 기분·유대감 증가. 싸우면 기력과 포만감이 조금 줄어요
- 기력이 15 미만이거나, 알·수면 중이면 싸울 수 없어요

### 통신 (2인)
- 두 기기 모두 **배틀 페이지 → 통신 2인** → 같은 메뉴(**대전** 또는 **교환**) 선택
- 서로 찾으면 자동으로 연결돼요 (가까이, 몇 m 이내)
- **대전:** 양쪽 기기가 같은 배틀을 동시에 보여줘요. 이기면 통신 전적과 훈련치가 올라요
- **교환:** 상대 포켓몬을 보고 **예**를 누르면, 둘 다 수락했을 때 교환돼요.
  지금 키우는 포켓몬이 상대에게 가니 신중하게! 받은 포켓몬은 도감에 등록돼요

### 시리얼 명령 (115200bps, 디버깅용)
`WIFI 이름|비번` · `WIFIOFF` · `NTP` (지금 동기화) · `TZ 540` (분 단위 시간대) · `NET` (상태) ·
`WILD` (바로 야생 배틀) · `ALERT` (야생 출현 알림) — 원본 명령(`STATS`, `HATCH`, `LVL 16` 등)도 그대로 동작

## 검증한 것 / 못 한 것

- ✅ ESP32 코어 3.3.10으로 펌웨어 컴파일 (`--warnings=all`), 앱 크기 1.56MB / 3MB
- ✅ PC 테스트 130개 통과 (AddressSanitizer 포함): 배틀 계산, 타입 상성, 교환 데이터 검증,
  한국어 조사(은/는, 이/가…), **두 기기 통신 프로토콜을 패킷 60% 손실 상황까지 시뮬레이션**
- ✅ 실제 Arduino_GFX 그리기 코드를 PC에서 돌려 화면 캡처로 레이아웃 확인 (한국어/영어)
- ❌ **실기기 테스트는 아직 못 했어요.** 특히 WiFi 포털, ESP-NOW 실제 전파, 한국어 폰트 크기는
  보드에서 확인이 필요해요. 문제가 있으면 시리얼 로그(`NET`, `LINK ...`)와 함께 알려주세요

## 파일 구성 (새로 추가/변경)

- `battle.h/.cpp` – 배틀 엔진 (정수 연산만, 두 기기에서 똑같이 계산되도록)
- `link.h/.cpp`, `link_core.h/.cpp` – ESP-NOW 통신 (상태 머신은 PC 테스트 가능하게 분리)
- `net.h/.cpp` – WiFi, NTP, 설정용 웹 포털
- `i18n_ext.h/.cpp` – 새 기능 문구 (한국어/영어, 한국어 조사 자동 처리)
- `ui_extra.ino` – 새 화면들 (네트워크, 배틀, 통신)
- `pet.h/.cpp` – 배틀 보상, 교환, 전적 저장 / `dex.h` – 타입 정보 추가 (`tools/gen_dex.py`로 생성)
- `test/test_battle.cpp`, `test/test_link.cpp`, `test/test_i18n_ext.cpp` – 새 테스트

## 라이선스

원본과 같아요: 코드는 MIT, 스프라이트는 PMD SpriteCollab (CC BY-NC, 포켓몬 © Nintendo/Game Freak).
개인·비상업용 팬 프로젝트입니다.

# TamaPoke KO (v1.17-ko12.9.2)

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
| ⬆️ SD카드 업데이트 (ko5) | `update.bin`을 SD카드에 넣고 네트워크 화면 [SD 업데이트] (esptool 필요 없음) |
| 🕐 큰 시계 (ko4) | 기본 화면 하늘에 현재 시간을 크게 표시 |
| 📦 보관함 (ko4) | 포켓볼로 잡은 포켓몬은 항상, 이긴 포켓몬은 20% 확률로 보관 (최대 60). 최종 진화 후 작별하면 다음 육성 포켓몬이 여기서 무작위로 나와요 |
| 📖 도감 (ko4) | 만난 포켓몬 기록: 타입, 희귀도, 기본 능력치, 진화, 처음 발견한 날, 만남/포획 횟수 |
| 🔊 소리 설정 (ko4) | 배경음(배틀 포함) / 포켓몬 목소리 / 시스템음 볼륨 메뉴 |
| 🏋️ 훈련 3종 (ko4) | 공격: 연타, 방어: 떨어지는 볼 터치, 속도: 좌우에 잠깐 나타나는 볼 터치 |
| 💾 실시간 저장 (ko4) | 1분마다 + 행동할 때마다 저장. 전원 버튼을 누르면 바로 저장 |
| 🎒 배틀 아이템 (ko4) | 이기면 포켓볼 +2, 물약 +2. 물약(체력 절반 회복), 포켓볼(포획 → 보관함). 지거나 도망쳐도 불이익 없음 |
| 🔤 글꼴 (ko4) | 한글을 Noto Sans KR로 깔끔하게, 글자색 검정, 게이지에 수치(0~100) 표시 |
| 🔊 SD카드 소리 (ko2) | 배경음, 배틀 배경음, 포켓몬 울음소리를 SD카드 WAV로 재생 (TamaPoke v1.23에서 이식) |

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
     Partition *16M Flash (3MB APP/9MB FATFS)*, USB CDC On Boot *Enabled*,
     USB Mode는 기본값 *Hardware CDC and JTAG* (ko6에서 원래대로)
     라이브러리: GFX Library for Arduino, SensorLib, XPowersLib, U8g2. ESP32 코어 3.3.x

> 이미 원본을 쓰고 있었다면 2단계만 하면 돼요. 키우던 포켓몬·도감은 그대로 남아요
> (단, "Erase device"를 체크하면 지워져요).

## 사용법

### WiFi 시간 맞추기
1. 메인 화면에서 **아래로 스와이프** → 시간 설정 화면 → 가운데 **WiFi** 버튼
2. **WiFi 설정하기** → 화면에 나온 `TamaPoke-XXXX` WiFi에 휴대폰으로 연결 (비밀번호 `tamapoke`)
   (ko8: 화면의 큰 **QR코드**를 휴대폰 카메라로 찍으면 바로 연결돼요)
3. 설정 페이지가 자동으로 열려요 (안 열리면 브라우저에서 `192.168.4.1`) → **WiFi 목록에서 고르기**
   (ko7, 안 보이면 [다시 검색] 또는 이름 직접 입력) → 비밀번호·시간대 입력 → 저장
   (ko8: WiFi는 **5개까지** 저장되고, 켤 때 신호가 가장 센 것으로 맞춰요. 저장된 게 안 되면
   비밀번호 없는 WiFi로 시간만 받아요. 네트워크 화면 [개방 WiFi 켬/끔])
4. 바로 시간을 맞추고, 이후엔 켤 때마다 + 하루 한 번 자동으로 맞춰요

※ **2.4GHz WiFi만** 됩니다 (ESP32 제한). 시리얼로도 설정 가능: `WIFI 이름|비밀번호`

### 야생 배틀
- **위로 스와이프** (스탯 카드) → 옆으로 넘겨 **배틀** 페이지 → **야생 배틀**
  → ko10.1: **"어디로 갈까?"**에서 지역 고르기 (8곳씩 2쪽, 옆으로 넘기기). 지역마다 나오는 포켓몬이 달라요
  (그 속성 50%, 시간대 20%, 레어는 지역·시간·날씨·계절 조건). 배틀 후 [계속 만나기]는 같은 지역
- 또는 가끔 메인 화면에 뜨는 초록색 **"! 야생 포켓몬 출현 !"** 버튼을 탭 (30초 뒤 사라져요, ko9.1)
- 이기면 공격/방어/속도 훈련치 상승 + 기분·유대감 증가. 싸우면 기력과 포만감이 조금 줄어요
- 기력이 15 미만이거나, 알·수면 중이면 싸울 수 없어요
- ko7: 이기거나 잡으면 **경험치**를 받아 레벨이 올라요 (최대 Lv.100). 깨어 있고 게이지가 모두 40 이상이면
  1시간마다 조금씩도 올라요. 진화: 3단계는 1차 진화 Lv.16 / 최종 진화 원작 레벨, 2단계는 원작 레벨
- ko7: 타입마다 기술 이펙트가 달라요. 급소는 화면 흔들림, 효과가 굉장하면 흰 충격파

### 통신 (2인)
- 두 기기 모두 **배틀 페이지 → 통신 2인** → 같은 메뉴(**대전** 또는 **교환**) 선택
- 서로 찾으면 자동으로 연결돼요 (가까이, 몇 m 이내)
- **대전:** 양쪽 기기가 같은 배틀을 동시에 보여줘요. 이기면 통신 전적과 훈련치가 올라요
- **교환:** 상대 포켓몬을 보고 **예**를 누르면, 둘 다 수락했을 때 교환돼요.
  지금 키우는 포켓몬이 상대에게 가니 신중하게! 받은 포켓몬은 도감에 등록돼요

### SD카드 소리 (ko2)
SD카드 `/mons/` 폴더에 넣고 재부팅하면 자동으로 적용돼요. 없는 파일은 조용히 건너뛰고 기본 효과음은 그대로 나와요.

| 파일 | 언제 |
|---|---|
| `/mons/bgm.wav` | 평소 배경음 (반복) |
| `/mons/battle_wild.wav` | 야생 배틀·통신 대전 중 (반복). 결과 화면부터 평소 배경음으로 돌아가요 |
| `/mons/cry001.wav` ~ `cry151.wav` | 울음소리: 켤 때, 쓰다듬을 때, 부화·진화·교환으로 종이 바뀔 때, 배틀 상대가 나올 때 |

- 형식: **WAV, 16kHz, 모노, 16비트 PCM**. 울음소리는 30초 이하
- 변환 (ffmpeg): `ffmpeg -i 원본.mp3 -ac 1 -ar 16000 -c:a pcm_s16le -map_metadata -1 -fflags +bitexact -flags:a +bitexact cry025.wav`
  - 뒤쪽 `-map_metadata -1 -fflags +bitexact -flags:a +bitexact`를 빼면 울음소리가 재생되지 않아요 (메타데이터가 붙음)
- 볼륨: 시리얼 `VOL 배경 울음 효과` (각 0~100, 기본 `VOL 25 65 100`), `VOL`만 치면 현재 값
- 자는 동안이나 소리를 끄면 모두 멈춰요. **전원 버튼을 짧게 눌러 화면을 끄면 배경음이 멈추고**, 켜면 멈춘 곳부터 다시 나와요 (ko5)
- 최신 울음소리: `tools/get_cries.py` (PokeAPI, 게임 원본 음원이라 개인용으로만)

### 보관함과 다음 포켓몬 (ko4)
- 위로 스와이프 → 옆으로 넘겨 **배틀** 페이지 → **보관함**
- 야생 배틀에서 **포켓볼로 잡으면** 항상, **이기면 20% 확률로** ("동료가 되고 싶어 해요!") 보관함에 들어가요 (최대 60마리)
- 목록을 탭하면 정보가 보이고, **놓아주기**를 두 번 누르면 풀어줘요
- 최종 진화한 포켓몬과 **작별**(3일 뒤 나오는 금색 버튼)하면, 알 대신 보관함에서 **무작위로 한 마리**가
  다음 육성 포켓몬으로 나와요. 보관함이 비어 있으면 예전처럼 알이 나와요
  (도망/놓아주기로 끝나면 알)

### 배틀 아이템 (ko4)
- 배틀 메뉴: 몸통박치기 / 타입 기술 / 방어 · **물약** / **볼** / 도망
- **물약**: 체력 절반 회복 (그 턴에 상대가 공격해요). **볼**: 상대 체력이 적을수록 잘 잡혀요
  (희귀는 어렵고, 전설은 아주 어려워요). 잡으면 배틀 끝 → 보관함
- 이기면 **포켓볼 +2, 물약 +2**. 처음에는 포켓볼 5개, 물약 2개가 있어요
- **지거나 도망쳐도 불이익 없음** (기력·포만감·기분 그대로)

### 훈련 (ko4)
- 기본 화면의 **포켓볼 버튼** 또는 배틀 페이지의 **훈련** → 훈련 선택
- **공격**: 10초 동안 최대한 빠르게 연타 / **방어**: 20초 동안 떨어지는 포켓볼을 터치 /
  **속도**: 왼쪽·오른쪽에 잠깐 나타나는 볼을 터치 (15라운드, 점점 빨라져요) / **공놀이**: 기분만 올라요
- 한 번에 최대 +18. 기력이 10 미만이면 훈련할 수 없어요. 화면 위쪽을 탭하면 그만둬요

### 소리 설정 (ko4)
- 아래로 스와이프 → 왼쪽 **소리** 버튼 → 소리 켜기/끄기와 볼륨 3개 (-/+ 버튼 또는 막대를 탭, 10% 단위)

### 도감 (ko4)
- 기본 화면에서 옆으로 스와이프. 배틀에서 만나거나, 잡거나, 키운 포켓몬은 색으로 보여요
- **두 번 탭하면 도감에서 나가요** (ko5)
- 탭하면 타입·희귀도·기본 능력치·진화·처음 발견한 날·만남/포획 횟수

### 저장 (ko4)
- 1분마다 자동 저장 (화면이 켜져 있어도), 먹이·훈련·배틀 등 행동할 때마다 저장
- 전원 버튼을 짧게 눌러도 저장, **길게 누르면(전원 끄기) 꺼지기 전에 저장**
- 갑자기 전원이 끊겨도 잃는 건 최대 1분이에요

### SD카드에 파일 넣기 (ko6)
USB 드라이브 모드는 ko6에서 뺐어요 (실제 보드에서 동작하지 않았어요). 대신 원본의 웹 설치 페이지로 보내요:
1. 보드는 평소 화면, USB로 PC에 연결 (esptool 창은 닫기)
2. Chrome에서 [웹 설치 페이지](https://socquique.github.io/TamaPoke/web/) → 2단계 **Connect board**
   (**Load sprites**는 누르지 않기)
3. **pick them manually** → 파일 형식 **"모든 파일"** → 파일 선택 → SD카드 `mons` 폴더에 저장

### 펌웨어 업데이트
- **SD카드로**: 위 방법으로 `update.bin`을 보내고 → WiFi 화면 **[SD 업데이트]** → **[업데이트]**
- **esptool로**: 주소 **`0xe000`**에 `...-app-0xe000.bin` (0x10000이나 0x0에 올리면 켜지지 않아요)
- ko6부터 USB 방식이 원래대로라 esptool에서 **Connect만 누르면** 연결돼요

### 시리얼 명령 (115200bps, 디버깅용)
`WIFI 이름|비번` · `WIFIOFF` · `NTP` (지금 동기화) · `TZ 540` (분 단위 시간대) · `NET` (상태) ·
`WILD` (바로 야생 배틀) · `ALERT` (야생 출현 알림) · `VOL` (볼륨, 화면 메뉴와 같음) — 원본 명령(`STATS`, `HATCH`, `LVL 16` 등)도 그대로 동작

## 검증한 것 / 못 한 것

- ✅ ESP32 코어 3.3.10으로 펌웨어 컴파일 (`--warnings=all`), 앱 크기 1.56MB / 3MB
- ✅ PC 테스트 130개 통과 (AddressSanitizer 포함): 배틀 계산, 타입 상성, 교환 데이터 검증,
  한국어 조사(은/는, 이/가…), **두 기기 통신 프로토콜을 패킷 60% 손실 상황까지 시뮬레이션**
- ✅ 실제 Arduino_GFX 그리기 코드를 PC에서 돌려 화면 캡처로 레이아웃 확인 (한국어/영어)
- ✅ ko11: 체육관 재대전·챔피언 리그·명예의 전당, 사탕 가방, 야생 실력 맞춤·진화 모습·새 포켓몬 우선, 터치 디바운스, 상황별 배경음. PC 테스트 208개
- ✅ ko11.1: 명예의 전당 화면(3×3 목록, 큰 그림 + 왕관, 몇 번째 챔피언·날짜·능력 편차), 겹침 수정, 배틀 아이템 줄이기(야생 볼 40%·물약 30%, 상한 20/10). PC 테스트 210개
- ✅ ko11.2: 훈련 중 음악·화면 끊김 줄이기 (음악 작업 우선순위 5, SD 미리 읽기 2 KiB씩 채우기, 훈련 중 자동 저장 미루기). PC 테스트 211개
- ✅ ko11.3: 훈련 중 렉 — I2C 100→400kHz(실패 시 100), 훈련 중 RTC 읽기 대신 millis, PWR 1초·배터리 10초, 결과 화면에 그리기/멈춤 최대 ms. PC 테스트 211개
- ✅ ko11.4: 빠른 연타 렉 원인 — SensorLib이 Wire.readBytes()를 써서 NACK 시 Stream 타임아웃(1000ms) 바쁜 대기 → Wire.setTimeout(3). PC 테스트 211개
- ✅ ko11.5: 부팅 단계 기록(RTC_NOINIT) + 화면/콘솔 표시, 재부팅 원인(esp_reset_reason), 2회 연속 실패 시 안전 모드(SD·WiFi·소리 끔). PC 테스트 211개
- ✅ ko11.6: SD 세이브 백업(2슬롯·CRC·자동/수동·부팅 복원 질문), 파티션 재배치(app 6MB×2 + nvs2 256KB, nvs/otadata 그대로, 박스·도감 nvs2로 이전), 터치 이벤트 시각 기록, 위쪽 두 번 탭 나가기. PC 테스트 217개
- ✅ ko11.6.1: ◀ 뒤로가기(시간·네트워크·백업), 챔피언 연승(chs/chb/fstk), 백업 flags(수동/자동), screenBase 배경색 직접 채움(깜박임), 포털 TX 8.5dBm, 물약 35%·15, nvs2 표시 점. PC 테스트 219개
- ✅ ko12.9.2: 포털 /fw (UpdStream: SD와 같은 분류, 0x0 거절, 0xe000 받음, 배터리 20%, 음악 멈춤, 저장 후 재시작), 사탕 카드 2열 타일(candyTileAt), fameGenesChanged(사탕으로 유전자 바뀌면 명예의 전당 카드도), clampGene 상한 115
- ✅ ko12.9.1: 상태 글자 테두리 2px+굵게, drawFavBadge(좋아하는 먹이/?) — 연속 일수 배지 대신, 방문 visitPending(낮에 자는 중·화면 꺼짐에도 추첨 → 기본 화면에서 도착, 놀기 전 잠들면 다시), GAUGE_TAP_GAIN_BIG 25(포만·기분·청결)
- ✅ ko12.9: 진단 sd(시도)x(연결 ms) pak(ms), 진단 줄 두 줄로, /mons 있으면 mkdir 안 함, 새 보드 설치 안내(0x0 통합 이미지 + FAT32 SD에 mons.pak)
- ✅ ko12.8.5: /mons.pak.map (TPMP: 솔트·크기·시작 클러스터·FAT 형태가 같을 때만 재사용, 구간 연결 검사, 목록이 안 맞으면 지우고 다시 만듦, 묶을 때 삭제), 진단 bgm/lp/dr/spr ms
- ✅ ko12.8.4: update.bin = TamaPoke.ino.bin(앱 그대로), sdupdate updAppOffset(0xe000 파일 = +0x2000), 발행 파일 전체 검사 테스트, pakDiag(빠른 읽기 여부·이유·io/aes 시간)·bootDurMs(단계별 ms) 묶기 화면·PAKINFO, 구간 버퍼 늘려 가며
- ✅ ko12.8.3: pak_core pakChainExtents/pakExtRead(FAT16/32 체인 → 연속 구간, 테스트 4개), pak.cpp fastBuild(f_open 직접 → sclust·geo, ff_mutex + disk_read) / PakFileImpl 직접 읽기, sdRemount 전에 pakUnload
- ✅ ko12.8.2: bakAutoLoop는 xScreen==XS_NONE·의식 없음·gNextPickPending 아님·8초 무입력일 때만, pakLoad 시간 로그
- ✅ ko12.8.1: pak.cpp listMons를 FatFs f_readdir 한 번으로 (이름·크기), 안 되면 예전 stat() 방식. 시리얼 "PAK lista: N ficheros en M ms"
- ✅ ko12.8: 반동 = 그 기술만 restT(KO면 없음)·HIT_GUARDED/HIT_REST, dex_special.h(특수공격·특수방어), moveDominated 기술 정리, 엔딩(pendingEnding·lap·pickTokens), 알 천장(legDry 12), 감기 약 3분, sdcheck(SdInv)·mons.pak(pak.cpp, AES-CTR), UI_GEOM 둥근 화면 검사, 설정 SET_ORDER, 오디오 스택 8KB. PC 테스트 302개
- ✅ ko12.6: Pet::sick/sickDoses/sickWait/sickMin(tick 위험도, 2시간=MW_SICK, 약 1~2번 30분 간격, careTick 절반), 비 산책 5%, visitPoll(보관함 랜덤 15분, 하루 3번)·visitTap(friendPlay), 생일 bdm/bdd(새로 시작에도 유지)·XS_BDAY·폭죽·birthdayGift(연 1회), 설정 6줄(SET_N 11), PP_MULT 2·패배 시 절반. PC 테스트 284개, 화면 검사 159개
- ✅ ko12.5.1: championTeam +0..+4, DAILY_HEAL_PCT 20 (최대 체력, 체육관·리그·오늘), benchRest(5%/턴, 50%까지), 질문 창 CH_* 좌우 박스, fdcl/evdl 저장, 반동 문구. PC 테스트 280개, 화면 검사 147개
- ✅ ko12.5: Pet::gaugeTap(+15), discipline/tantrum(tick, 15분, scold/soothe, 훈련 EXP +25%), routineDo(RT_MEAL/PLAY/BED, 시간대, 연속), personalityOf(LifeLog, 훈육), 카드 6쪽 renderCardLife, 걸음 버퍼(흔들기 무시)·걸음 링·배경 고르기(bgask). PC 테스트 279개, 화면 검사 143개
- ✅ ko12.4: LifeLog(pet "life")→MemRec(nvs2 "tpmem", 키 m+epoch+dex, SD 백업 포함), 리본 상세 [추억] 2쪽. ui_home.ino: drawRoom(방 배경, 잠들면 불 끔)·drawDecor(3자리, DECO 6종, decoUnlocked)·XS_ROOM/XS_WALK, 설정 5줄(SET_N 10). QMI8658(0x6B/6A, WHO_AM_I 0x05, CTRL2 0x17 ±4g 62.5Hz) 걸음(피크 0.28~2초, 4걸음 연속부터)·흔들기(1초에 4번 ±1.2g → wakeAuto/쓰다듬기), Pet::walk(7일, 2천/5천/1만 보상). hwScan(부팅 로그·HW 명령). 폰트 재생성. PC 테스트 275개, 화면 검사 139개
- ✅ ko12.3.3: updateBrightness idle = (int32_t)(now - lastInteract) 음수면 0 (루프 시작 now < handleTouch의 millis() → wrap → dimStage 2 → 밝기 8 깜박임). 자동 잠자기 판정도 부호 있게. 화면 검사 "brillo" 추가 (128)
- ✅ ko12.3.2: dexMvPoll/탭 — fxFind && !fxLoading()일 때만 시작(큐 전체 읽기 끝난 뒤). 로그상 깜박임은 SD 읽기 중 시작한 첫 사용에서만
- ✅ ko12.3.1: DEXMV_WAIT_MS 1500→6000, 대기 중 버튼 둘레 회전 점(화면 검사 13e), 로그 PERF dexfx / DEX tap / PERF dexframe(>70ms)
- ✅ ko12.3: 무지개구슬 ORB_DUAL(bit13, orbMakeDual/orbDual, orbSame 마스크 0x6F00, orbAtkPct·orbDefPct 둘 다), synthOrbs 성공의 ORB_SYNTH_DUAL_PCT 4% → 3 반환, drawOrb 무지개 테두리·금별·orbHue, 진화 시 ORB_EVO_DUAL_CANDY 10 + 30% 만능(orbEvoNote 3/4). 도감 기술 버튼 drawTypeGlyph(16속성+변화). 도감 dexMvPend: 읽는 중이면 최대 1.5초 대기(fxQueued). PC 테스트 270개, 화면 검사 127개
- ✅ ko12.2: pet autoSleep(AUTO_SLEEP_MS 3분, 배부름>30·청결≥30, 배부름≤30이면 깸, 터치로 wakeAuto, 저장 키 aslp), RUNAWAY_TICKS 720(12시간). TEAM_HELPERS 5 / PARTY_MAX 6: 도우미 수 = 상대 수-1(ppMaxHelpers), 교체 칸 좁으면 작은 버튼. 깜박임: 전투 bvFxNow 잠금·도감 dexMvSd, 도감 미리보기 큰 그림 위치(DEX_FEET_Y). PC 테스트 268개, 화면 검사 123개
- ✅ ko12.1: 전투(XS_WILD) 중 uiFlush = 이중 버퍼 + 코어0 flush 태스크(panel->draw16bitRGBBitmap), 밝기 전 flushWaitIdle, 전투 밖 flushPipeStop(마지막 화면을 원래 버퍼로). PERF bat wait=. test/render/perf.sh(부분별 PC 측정). PC 테스트 266개, 화면 검사 119개
- ✅ ko12.0.1: savebak_sd addEntry 블롭 버퍼 4KB 고정 → 실제 크기(PSRAM, 최소 8KB). DexLog lrn 6KB 때문에 ko11.31부터 백업 전체 실패하던 문제
- ✅ ko12.0: 정리판 — 미사용 함수(drawMedalBadge·medalLabel·FxAnim::load/loadId·fxParse·fxPreload·vbHold/vbRelease 등)·X_/S_ 문구 52개 삭제(8개 언어 덤프 비교), -Wall 경고 31→1, symbols 3개만, README/버전기록.md 분리. PC 테스트 266개, 화면 검사 119개
- ✅ ko11.32: sdMaybe/sdMarkMissing/sdOpenKnown(없는 경로 FNV 해시 128개, PUT·재마운트 때 비움), fxOpen mons/fx 폴더 확인 1번, 음악 haveFile 캐시, battleFxPlaying(이펙트 중 fxPump 안 함 + drawBattleBg 하늘·배경 PSRAM 복사), pack_fx 프레임 1/2·배경 1/2·700KB 상한, PERF bat/setup/fx 로그. PC 테스트 266개, 화면 검사 119개
- ✅ ko11.31.4: WavStream<File,32768> (malloc, PSRAM), audioSdFree = 남은 음악 바이트 - 경과시간×32 ≥ 12KB, fxPump 20ms 한도(12조각), fxWant(이번 턴 EV_USE·도감 버튼), MT_SLEAGUE(story_league.wav, 게임 stCh 12·13 장면). PC 테스트 266개, 화면 검사 118개
- ✅ ko11.31.3: audioSdFree/audioSdUsed(음악 링 버퍼 가득일 때만 fxPump·PmdMon 조각 읽기), Box::add(mv), 포획 시 dexRecordMoves(bFoe.mv), DexLog::anyLearned + 예전 포획 기본 기술 채우기, galleryLoadPending(화면 먼저), storyBattleTrack 애니 13화까지 MT_NORMAL. PC 테스트 266개
- ✅ ko11.31.2: fxPreloadAsync + fxPump(4) 루프(8KB씩), PmdMon::load 8KB씩 SD 잠금 해제, 스토리 동료 battleFacing 뒷모습, dexTopMoves 아는 기술 먼저, 도감 drawBg. PC 테스트 265개, 화면 검사 118개
- ✅ ko11.31.1: fxPreload/fxFind 캐시(10개, 3MB, bvSetup·partySwitchTo·도감 상세), fxParse 8KB씩 SD 잠금 해제, STATUS_CURE_PCT 16 (EV_CURE PSN/BRN/PAR), thumbCenter ○, DEXMV_XY 안쪽. PC 테스트 265개, 화면 검사 117개
- ✅ ko11.31: gen_moves.py(PokeAPI)→moves_data.h 171개, Battler mv/pp/stg/st/cnf/recharge, BA_M0..3, 이벤트 EV_USE..EV_DRAIN, AI(상태/능력/단점 가중), Pet mv4/mvo2, BoxMon 16B(12B 변환), DexLog lrn, 싸운다/자동/기술 창, 잊을 기술 대화상자, XS_CENTER 30초, 도감 ○·기술 버튼, mNNN.bin, LINK_PROTO_VER 6. PC 테스트 264개, 화면 검사 117개
- ✅ ko11.30.1: FxAnim::load가 /mons/fx/ 없으면 /mons/fTTSV.bin (웹 설치 페이지는 /mons/에 파일만). PC 테스트 255개, 화면 검사 109개
- ✅ ko11.30: 기술 3종(MOVES2_*, moveName var, Pet moveK/moveLv/moveOffer/moveLearned, mvk/mvl/mvo, 5레벨마다 제안 choiceKind 3, moveVarFor), drawMoveFxVar 36개, SD 기술 이펙트(fxanim TFX2 + tools/pack_fx.py, 두 방향·배경), 스토리 진화 장면(stSeenP/J sp0/sp1/sj0/sj1, SX_EVO_1/2, {로}), 삐삐·이브이 JOINK. PC 테스트 255개, 화면 검사 109개
- ✅ ko11.29: 진동 패턴 엔진 (VibStep{pct,ms} 최대 12단계, vibPlay(cut), 정지→작은 세기면 킥, vibPulse는 패턴으로), vibBattle (EV_HIT 내/상대·crit·eff 4·eff 1, EV_COUNTER, EV_FAINT, 승리), drawBattery 낮 진한 색. PC 테스트 255개, 화면 검사 101개
- ✅ ko11.28: XS_STRAIN 수련 전투 (stTraining 1/2/3, BK_STORY 1:1, storyAddParty 생략, 승리 = 1/3 레벨, stPExp / rgTrainExp "rx"), rgWaveLv(상대) vs rgPartnerLv=max(웨이브, 수련), 스토리 메뉴 카드 4개. PC 테스트 255개, 화면 검사 101개
- ✅ ko11.27: 프로필 PROF_FAV_Y/DAY_Y/HINT_Y (좋아하는 먹이 + SPR_ICON_* 아이콘, 탭 = cardMsg 4초, X_DAYS_WITH_FMT + Pet::raiseStartEpoch), drawBattery 둥근 판(y 25, 반지름 232 안), openGyms 배지 8개면 gymPage 0. PC 테스트 255개, 화면 검사 94개
- ✅ ko11.26: XS_SET 설정 화면(NAV_TOP·아래로 밀기, RET_SET, 시간은 clockClose), 시간 화면 정리, 소리 화면 밝기 버튼·네트워크 화면 업데이트/백업 버튼 이동, 카드 능력치 쪽 버튼 제거, 보관함 BOX_TAB_X4 사탕 탭(bagFromBox), 진동 세기 vibLv 0..2(LEDC 20kHz, 115/180/255, 30ms 킥, KEEP_U8). PC 테스트 255개, 화면 검사 92개
- ✅ ko11.25: VIB_PIN 18 (Grove 진동 모듈), vibPulse(ms,n,gap) 논블로킹 + vibLoop, "vib" NVS(기본 켬, 새로 시작 유지), 켤 때 180ms, 야생 200×3, 타격 70/140ms, 소리 끔 + 새 똥 250×2, 소리 화면 [진동 켜짐/꺼짐]. PC 테스트 255개
- ✅ ko11.24.1: Wire.setTimeOut 50→200 (crash.txt ko11.17·ko11.24 같은 pc 0x4037a37f = i2c_master_isr_handler_default / i2c_isr_receive_handler, 포기된 터치 읽기의 늦은 응답). PC 테스트 255개
- ✅ ko11.24: MT_STORY/STORY_A/STORY_END/SBATTLE/SROCKET/SGYM (audio.cpp 후보 파일 순서대로, 없으면 기존), storyBattleTrack/storySceneTrack, batSetChargeLimit(4.1V/4.2V, "chg"), KEEP_U8에 bri·chg. PC 테스트 255개
- ✅ ko11.23.3: STORY_FOE_ATK_EASE 10, gStoryFlyAt(&bMe, BK_STORY) + rawDamage storyFly(구구 계열 BA_TYPE vs PT_GRASS → eff 4). PC 테스트 255개
- ✅ ko11.23.2: STORY_FOE_EASE 15 (stStartBattle에서 상대 maxHp/atk/def/spe ×0.85). PC 테스트 254개
- ✅ ko11.23.1: bQuitArm(BK_STORY/BK_ROGUE, NAV_L 두 번) → endBattleScreen + storyBattleQuit, 장면 stMon이 동료면 안 그림. PC 테스트 254개
- ✅ ko11.23: STORY_CH_MAX 15·STORY_NCH{14,15}, stDone 32bit("g2"/"a2", 옛 "g"/"a" 읽기), STORY_JOIN_MAX 6·JOIN_KEEP·ST_LEAVE·stPick(0x80=고름)·고르기 패널, BATTLE_ANY(b=1), 상대 진화는 라이벌 시작 포켓몬만, 애니 피카츄 진화 없음, 장 목록 페이지, W_LORELEI/AGATHA/GARY/RITCHIE/LEAGUE. 렌더러: 전 장 자동 통과 검사. PC 테스트 254개
- ✅ ko11.22.1: stRestartStyle(장 목록 [처음부터], 두 번 누르기 3초), doResetGame에서 "tpstory"/"tpparty"/"tpteam"(nvs2) clear. PC 테스트 254개
- ✅ ko11.22: ST_JOIN(stJ[2][2], NVS j00~j11, SX_JOIN_FMT), 스토리 전투는 partyOpen 없이 startTrainer + storyAddParty(동료 레벨-2, pBox -1), foeMoveRule(BK_STORY·상대 Lv<8은 BA_TYPE→BA_TACKLE). 원정은 그대로 도우미. PC 테스트 254개
- ✅ ko11.21.1: bakNewestAuto — 하루 1번 자동 백업은 자동 칸만 보고 판단, 수동 백업은 bakKnownDay에 안 셈 (수동 하루 = 자동 건너뜀 버그). PC 테스트 254개
- ✅ ko11.21: 스토리 모드 story.h/story_ko.cpp(대본 표: BG/SAY/NARR/MON/CHOICE(2·3칸)/LABEL/GOTO/BATTLE/GIVE/END/STARTER)/ui_story.ino, XS_STORY/STORYCH/SCENE/ROGUE, BK_STORY/BK_ROGUE, 동료 stPDex/stPExp(STORY_FLOOR, 승리 +1레벨, pSlot0Pet=false로 출전, 보상은 pet), 원정 rgStarter·rgPartnerLv=4+웨이브, 인물 그림 SdThumbs portraits(/mons/story.bin, tools/pack_story.py), NVS "tpstory"(SD 백업 포함), 조사 {아}. PC 테스트 253개
- ✅ ko11.20.1: partyEnd()(afterResult 트레이너·endBattleScreen·startWildIn·startLinkBattle) — pCur 남아 야생에서 도우미 모습, drawBadge 검은 테두리·회색→은색, BOXF_PERFECT 16(onPetEnd 메달 8개) 리본 전당 금빛 효과. PC 테스트 256개
- ✅ ko11.20: 팀 pMon[3]/pBox/pCur/pUsed, XS_PARTY(ppPick, 하루 HELPER_USES_PER_DAY 3 "tpparty"), BP_SWAP(모드 0 강제/1 다음 상대/2 수동=battleFoeOnly), makeBoxBattler(레벨 캡), typeMatch, pickNextFoe(챔피언), Box::bumpLevel/markFlag/set, 명예의 전당 FameRec("tpteam" nvs2, 1마리 1장·solo/team·도우미, 옛 카드 합치기 1회), 금/은관, SD 백업에 tpteam. PC 테스트 256개
- ✅ ko11.19.1: bakMsg/bBoxMsg int8_t → int16_t (X_BAK_DONE 311이 X_CANT_NOW 55로 잘림), bakPickSlot(수동·자동 각자 칸, 테스트), 버전 녹색 점 왼쪽·함께 가운데. PC 테스트 249개
- ✅ ko11.19: 자동 전투 autoCount/autoLeft/autoPick(AUTO_POTION_HP 30·HARD 50·MAX 3, AUTO_STEP_MS 700, autoHardFoe), battleDoAction 분리, synthOrbs(ORB_SYNTH_FAIL_PCT 20·GREAT 5), releaseGift, giveItems 넘침 전환·addShards, syncClock 밤 22~7 잠·낮 최저 30·sleptOffline. PC 테스트 248개
- ✅ ko11.18: brightLevel/setBrightLevel(NVS "bri" 1~10, 30+22×단계) XS_BRIGHT, drawBattery % (printOutlined), touchSample screenOff면 무시, renderFarewellTable(FAREWELL_AGE_MIN), DAILY_HEAL_PCT 35(팀 다음 상대 전), 실수 판정 포만·기분·청결 최저만, S_STREAK_FMT "연속 돌봄". PC 테스트 243개
- ✅ ko11.17: retMark()/goBack()(RET_MAIN/CARD/TRAIN/CLOCK), trainFromCard·reopenTrainMenu, 모든 메뉴 NAV_L(1페이지=뒤로), 렌더러 navChecks 14개, 속도 spdBase(SPD_RANDOM_FROM 7, 1~60)·첫 볼 깜박, drawRibbon(보관함 리본 탭·목록·상세·다음 파트너), fameDetail 속성 빛·물결·drawMoveFx·반짝이. PC 테스트 242개
- ✅ ko11.16.2: sdTryMount 6회(BOARD_MAX→20MHz→10MHz, 대기 150+150i ms), sdRemount(잠금·end·재마운트·sdDirty), sdWatch(기본 화면에서 스프라이트/SD 없으면 10초×6 뒤 60초마다, 음악 일시정지 후 재마운트·audioLoadMusic), X_BART 그림: 도트/원작. PC 테스트 242개
- ✅ ko11.16.1: drawPrgBattler 정수 배율(s4/4*4, 줄일 때 -4)·EPX 끔(gSmoothGfx 잠시 false), 상대 x3 최대 124px, pack_prg add_outline(PRG_OUTLINE, 바깥 1px (24,24,32), 안농 201-a), 지역 화면 RG_ART_X 버튼(battleArtToggle), X_BART 문구 귀여운/원작풍 그림. PC 테스트 242개
- ✅ ko11.16: 구슬 orbMake(0x8000|def 0x4000|type<<8|pct), Pet orb/orbBag[32]/orbN(NVS orb·orbn·orbs), gainOrb(같은 종류 높은 %만, 나머지 사탕 1), equipOrb(ptype 일치), 진화 속성 변경 → 사탕 3/만능 10%(orbEvoNote 토스트), applyOrb(wildMatchPower 뒤), drawOrb(공격 불꽃 3겹+불똥, 방어 후광+반짝이)·drawOrbSlot, 보관함 탭 3(BOX_TAB_X3, 구슬 가방 4×3·상세), 드롭 야생 8%·재대전 50%·탐험 10/20/35%, 전투 그림 gBattleArt(NVS "bart", BART 칩, prgLoadFor), WILD_COMMON[지역][8], 속도 spdPlace 3×3 칸·spdCount 3/7. PC 테스트 242개
- ✅ ko11.15.1: pwrPoll 0xFF/동시 눌림 무시, 사탕 조각(rareShards "rshd", 10=만능 1), NAV_TOP(시간)·NAV_R(보관함)·밤 화살표 어두운 원, 공격 foePmd 상대(sackPickFoe, typeEff), PMD_*_UR/_DL(id 16~21, pack_pmd 행 3·7) 전투 battleFacing. PC 테스트 239개
- ✅ ko11.15: 공격 golpes+tecnica(1타 1피해·기+10, 기 100이면 SACK_BTN 버튼에 moveName, 12+i 피해+drawMoveFx, 샌드백 8+2i·5초-0.08i), 속도 번호 볼 가운데(볼 4배·숫자 3배, 다음 볼 표시 없음)·테두리 시간 링(fillArc)·진행 점 15개·등장 팝·터짐 효과, uiFlush가 drawToast(모든 화면). PC 테스트 239개
- ✅ ko11.14: 공격 carga y golpe(sackPower 삼각파, 90/70/40/20 → 5/3/2/1/0, 샌드백 10+2i·6초-0.12i·주기 1.5초-0.05i), 방어 timing(레일 1볼, ±10 퍼펙트 2·±28 좋아 1, 170→640px/s ±12%), 속도 en orden(3~5볼, 볼당 평균 점수), 기록 키 sb2/as2·dh2/ad2·vp2/ap2, 놓아주기(BOXF_CAUGHT만) 사탕 토스트+만능 사탕 30%/10%, uiFlush=flush(페이드 제거). PC 테스트 239개
- ✅ ko11.13: uiFlush(gfx->flush 대체, screenSig 변화 시 260ms 페이드, fastGameNow 제외, gUiFade로 PC 렌더 끔), uiShadeSpan/uiMixSpan(565 3채널 묶음 곱셈 1번), uiScreenBg(R-1/G-2/B-1 동시 단계 + 4줄 디더링), perfRenderSum 평균. PC 테스트 239개
- ✅ ko11.12: UI 키트(uiShade 프레임버퍼 어둡게, uiShadow, uiGradRRect, uiButton, uiPanel, uiGauge, uiLerp 8비트 회색 보정) 38곳+게이지 9곳, 토스트 반투명, 선택 창 뒤 베일; 배구 VB_MON_MAXS 4·FITH 124, VB_BALL_R 20·그림 44px(drawMapQ); wildMatchPower 레벨 상한 내 레벨+WILD_LVL_OVER(3), 남는 차이 능력치 x0.6~2. PC 테스트 239개
- ✅ ko11.11: PET_MAXS 6·PET_FITH 176(smoothBlitQ 1/4배율), 시계 38x64·2px 테두리(하늘색 기반), 헤더 위로, drawRidge 2겹·drawGroundTex(LCG 30개 원근)·나무/산/화산/선인장 음영, backToTrainMenu 뒤 2초 바깥 탭 닫기 막음(navGuard 600ms). PC 테스트 238개
- ✅ ko11.10: smoothBlit(EPX x2/x4, PSRAM 작업 버퍼, drawPmdActM·drawThumb·drawMap·drawPetSD), 하늘·땅 2px 그라데이션(땅 원근), 해 halo, 발밑 그림자. PC 테스트 238개
- ✅ ko11.9.4: 배구 메뉴 현재 연승(X_VB_NOW_FMT), 강스파이크(VB_POWER_PCT 25·속도 ×1.22·받기 -20, drawMoveFx 공 따라가기, X_VB_POWER_FMT). PC 테스트 238개
- ✅ ko11.9.3: backToTrainMenu(결과·그만두기 뒤), 배구 AI 반응 320-46L·스파이크 12+9L·받기 +10·속도 +10, sdBegin 4회 재시도, panicrec(set_arduino_panic_handler → RTC PC 6개 → tpdiag "pc"·crash.txt·창 표시), symbols/*.elf.xz. PC 테스트 237개
- ✅ ko11.9.2: 훈련 기록 포켓몬마다(resetTrainRecords, all*Hi, rpp 마이그레이션), 짧은 보내주기(isShortStay 24h·evolvedHere, 계열 표시 안 함, 다음 알 보통), 왕관=작별만(onPetEnd), 길게 누르기 원(petHoldProgress, 400ms 끊김 이어 붙이기, 탭 지연), 재부팅 기록(rbWhere/crumb, tpdiag NVS, crash.txt, 백업 화면 창). PC 테스트 237개
- ✅ ko11.9.1: 배구 autoMove0(thinkAuto)·톡 = 점프·공중 접촉 자동 스파이크, AI 약하게(반응 380-56L ms, 스파이크 5+9L%, 받기 20+8L%), 스파이크 최고 속도 1000, 전투 2세대 이름 금색(nameInkFor). PC 테스트 234개
- ✅ ko11.9: 배구 미니게임(volley.h 물리·AI, volley.ino 화면, 조준 스파이크·리시브 실패, vbStreak/vbBest), 똥 막기 전투 화면 전체+2분(battleSeen), 배경음 파일 이름 인식(bgmSlotFromName, opendir). PC 테스트 233개
- ✅ ko11.8: 배경음 선택(bgm_pick.h, bgmMask, bgm.wav~bgm8.wav, WAV INAM 제목, prep_music --title), 따라오기 팝업(BP_JOIN), 방어 반격(EV_COUNTER·counterDamage, LINK_PROTO_VER 5), 전투 중 똥 없음(holdPoop), 보관함 ◀·탭 y44. PC 테스트 229개
- ✅ ko11.7: 탐험(pet.exped, Box::put, expeditionReward), 친밀도 진화(FRIEND_EVO_BOND 70, 이브이 낮/밤), dayEvent(요일 타입·주말 샤이니·보름달 전설), 도감 보상(dxrw), 훈련 최소 +3. PC 테스트 224개
- ✅ ko10.10: 사탕 메뉴 이름 "실수 만회 1회", 샤이니 UP 보존(보관함 선택 시). PC 테스트 204개
- ✅ ko10.9: 실수 원인 기록·표시, 육성 일차 날짜 기준, 수동 시간 설정 시 오프라인 반영, 아래 버튼 배치. PC 테스트 204개 (+2)
- ✅ ko10.8.1: 가장자리 화살표 탭 이동 (기본·카드·도감·훈련 메뉴), 느린 드래그 인식. PC 테스트 202개
- ✅ ko10.8: 터치 전용 작업(8ms), 기본 화면 행동·말풍선, SD 그림 EXT1 새 동작 4개(예전 펌웨어 호환 확인). PC 테스트 202개
- ✅ ko10.7: 공격 훈련 버티기(샌드백 연속 격파), 훈련 보상 경험치·사탕. PC 테스트 202개 (+1)
- ✅ ko10.6: 12시간 케어로 실수 -1(저장 포함), 방어 훈련 목숨 3개, 속도 훈련 반응속도 점수. PC 테스트 201개 (+2)
- ✅ ko10.5: 다음 파트너 선택·키운 계열·첫 모습 Lv.1, 👑 왕관 보관함(최대 251), 날씨 5분. PC 테스트 199개 (+3)
- ✅ ko10.4: 컴파일 2.75MB / 3MB, PC 테스트 196개 (+8: 날씨 배틀, 체육관·지역 열기, 오늘의 도전, 사탕 2, 기술 단계,
  날짜 입력, 통신 시간 공유). 새 화면은 모두 PC 렌더러로 확인
- ✅ ko10.3: 공놀이 손 대는 순간 판정, 보상(기록 갱신 = 기분+기력 / 아니면 기력 +5), 시간 화면 배치. PC 테스트 188개
- ✅ ko10.2: 육성 레벨 15분×레벨, 배틀 경험치 ×0.75. PC 테스트 188개 (+1: 이전 육성 누적 유지)
- ✅ ko10.1: 컴파일 2.72MB / 3MB, PC 테스트 187개 (+8: 날짜·계절·날씨, 속성별 배경, 지역 출현 확률·레어 조건,
  공놀이 기록 보상). 지역 선택·날씨·배경 16종은 PC 렌더러로 확인
- ✅ ko10: 컴파일 2.71MB / 3MB, PC 테스트 179개 (+4: 151→251 저장 이전, 갈래·새끼·교환 진화, 2세대 상성, 교환).
  2세대 그림·도감 작은 그림은 PC 렌더러로 확인 (1세대 작은 그림은 기존 thumbs.bin과 똑같이 나옴)
- ✅ ko9: 컴파일 2.68MB / 3MB, PC 테스트 175개 (좋아하는 음식 4종, 육성 시간 레벨업 30분×레벨 등)
- ✅ ko8: 컴파일 2.68MB / 3MB (글꼴 약 1MB), PC 테스트 174개 (+7: WiFi 선택 순서, WiFi 5개 기억,
  새로 시작, 천지인 조합 3종, 한글 이름 교환) + 파이썬 23개. WiFi QR은 PC에서 그린 화면을 OpenCV로 읽어 확인
- ✅ ko7: 컴파일 1.83MB / 3MB, PC 테스트 167개 (+9: 경험치 곡선, Lv.100 상한, 배틀/시간 EXP,
  진화 레벨 규칙, 옛 저장 Lv.1, 보관함 Lv.5). 이펙트 14종은 PC 렌더러로 그림 확인
- ✅ ko6: USB 드라이브 제거, 기본 USB 방식으로 컴파일 1.82MB / 3MB, PC 테스트 158개
- ✅ ko5: 컴파일 1.88MB / 3MB, PC 테스트 158개 (+2: 업데이트 파일 검사, 실제 배포 파일로도 확인)
- ✅ ko4: 컴파일 1.87MB / 3MB. PC 테스트 156개 (+17: 보관함, 도감 기록, 물약/포획, 보상, 불이익 없음,
  훈련, 다음 포켓몬, 1분 저장). 화면은 PC 렌더러(`test/render/`)로 실제 그리기 코드를 돌려 확인
- ✅ ko3: TinyUSB 모드로 컴파일 (1.64MB / 3MB), 기존 USB 모드에서도 빌드됨 (드라이브 기능만 빠짐).
  새 화면 문구가 한국어 폰트에 있는지 테스트, 둥근 화면 안에 들어가는지 계산으로 확인
- ✅ ko2: ESP32 코어 3.3.10으로 다시 컴파일 (1.58MB / 3MB), 소리 스트리밍 테스트 9개 추가해 139개 통과
- ❌ ko3~ko5.1의 USB 드라이브는 실제 보드에서 PC가 열지 못해서 ko6에서 뺐어요
- ❌ **실기기 테스트는 일부만 했어요.** 특히 WiFi 포털, ESP-NOW 실제 전파, 한국어 폰트 크기는
  보드에서 확인이 필요해요. 문제가 있으면 시리얼 로그(`NET`, `LINK ...`)와 함께 알려주세요

## 파일 구성 (새로 추가/변경)

- `battle.h/.cpp` – 배틀 엔진 (정수 연산만, 두 기기에서 똑같이 계산되도록)
- `link.h/.cpp`, `link_core.h/.cpp` – ESP-NOW 통신 (상태 머신은 PC 테스트 가능하게 분리)
- `net.h/.cpp` – WiFi, NTP, 설정용 웹 포털
- `i18n_ext.h/.cpp` – 새 기능 문구 (한국어/영어, 한국어 조사 자동 처리)
- `ui_extra.ino` – 새 화면들 (네트워크, 배틀, 통신)
- `audio.h/.cpp`, `wav_stream.h`, `music_route.h`, `sd_lock.h` – SD카드 소리 (v1.23에서 이식, ko2) / `sdmon.cpp` – SD 접근 잠금
- `sdupdate.h/.cpp` – SD카드 업데이트 (ko5) / `tools/get_cries.py` – 최신 울음소리 받기 (ko5)
- `box.h/.cpp` – 보관함과 도감 기록 (ko4) / `train.ino` – 훈련 메뉴와 방어·속도 게임 (ko4) /
  `ui_more.ino` – 보관함·소리 설정 화면 (ko4) / `font_ko.h` – Noto Sans KR 부드러운 글꼴 (ko8, `tools/gen_font_ko.py`로 생성) / `cji.h` – 천지인 키보드 (ko8) / `net_pick.h` – WiFi 선택 순서 (ko8)
- `test/render/` – 화면을 PC에서 그려 PNG로 저장 (`run.sh <SD 폴더>`)
- `pet.h/.cpp` – 배틀 보상, 교환, 전적 저장 / `dex.h` – 타입 정보 추가 (`tools/gen_dex.py`로 생성)
- `test/test_battle.cpp`, `test/test_link.cpp`, `test/test_i18n_ext.cpp` – 새 테스트

## 라이선스

원본과 같아요: 코드는 MIT, 스프라이트는 PMD SpriteCollab (CC BY-NC, 포켓몬 © Nintendo/Game Freak).
개인·비상업용 팬 프로젝트입니다.

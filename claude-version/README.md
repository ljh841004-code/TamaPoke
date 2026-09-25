# TamaPoke KO v1.17-ko5.1 (클로드 버전)

> 이 폴더는 **클로드(Claude) 버전**이에요. 저장소 루트의 TamaPoke v1.23과는 따로 관리하는 별도 갈래예요.
> v1.23의 어드벤처·컬렉션·한글 폰트 기능은 이 버전에 없어요. (소리 기능만 ko2에서 옮겨 왔어요)

원본 [socquique/TamaPoke](https://github.com/socquique/TamaPoke) v1.17을 바탕으로
야생 배틀, 통신 대전/교환(ESP-NOW), WiFi 시간 자동 맞춤, 한국어 기본, SD카드 소리(배경음·배틀 배경음·울음소리), SD카드 USB 드라이브, 보관함·도감·배틀 아이템·훈련 3종·소리 설정·실시간 저장·큰 시계를 더했어요.
화면 모습은 `screenshots/` 폴더에 있어요 (PC 렌더러로 그린 실제 화면).

## 폴더 구성

| 파일 | 내용 |
|---|---|
| `update.bin` | **SD카드 업데이트용** (ko5부터). SD카드 맨 위 폴더에 넣고 네트워크 화면의 [SD 업데이트] |
| `tamapoke-ko-v1.17-ko5.1-app-0xe000.bin` | **esptool 업데이트용**. 주소 **0xe000**. 포켓몬은 그대로 남아요 |
| `tamapoke-ko-v1.17-ko5.1.bin` | **새로 설치용** 통합 이미지. 주소 **0x0**. 저장 데이터가 초기화돼요 |
| `TamaPoke_v1.17-ko5.1_정리.txt` | 기능·사용법 요약 메모 |
| `TamaPoke/` | 전체 소스 (Arduino 스케치 폴더). 자세한 설명은 `TamaPoke/README_KO.md` |

## 펌웨어 올리기

### A. SD카드로 (ko5부터, 추천)
1. `update.bin`을 SD카드에 넣기: 둘 중 하나
   - 아래로 스와이프 → WiFi → **[USB 드라이브]** → PC에 생긴 드라이브의 **맨 위 폴더**에 복사
   - 또는 [웹 설치 페이지](https://socquique.github.io/TamaPoke/web/) 2단계 **Connect board** → **pick them manually** →
     파일 형식 "모든 파일" → `update.bin` 선택 (SD카드 `mons` 폴더로 들어가요, ko5.1부터 인식)
2. PC에서 꺼내기 → 네트워크 화면에서 **[SD 업데이트]** → **[업데이트]**
3. 진행 막대가 끝나면 자동으로 다시 켜져요. 포켓몬은 그대로, 다 쓴 파일은 `update_done.bin`으로 바뀌어요
   (새 펌웨어를 예비 영역에 다 쓰고 검사가 끝난 뒤에만 바꿔요. 도중에 실패하면 기존 펌웨어 그대로)

### B. esptool로 (ko4 → ko5는 이 방법으로 한 번 필요)

1. Chrome/Edge에서 [ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/) 열기
2. Connect → 보드 포트 선택
3. 아래 중 하나 → Program
   - 이전 버전을 쓰고 있다면: 주소 **`0xe000`**에 `tamapoke-ko-v1.17-ko5.1-app-0xe000.bin` (Erase Flash 누르지 않기)
     (0x10000이 아니라 0xe000이에요: SD 업데이트 후에도 esptool로 올린 펌웨어가 확실히 켜지게, 부팅 선택 정보를 같이 써요)
   - 처음 설치하거나 초기화하고 싶다면: 주소 `0x0`에 `tamapoke-ko-v1.17-ko5.1.bin`
4. "Hard resetting via RTS pin..."이 뜨면 완료. 화면이 안 바뀌면 RESET 버튼 또는 USB 다시 연결
5. 시계 화면(아래로 스와이프) 아래쪽에 `v1.17-ko5.1`이 보이면 성공

> **esptool이 연결되지 않으면 (ko3부터)**: 분해하지 않고 PowerShell로 업로드 모드에 넣을 수 있어요.
> 장치 관리자에서 COM 번호 확인 → esptool 창을 닫고 PowerShell에서
> `$p = New-Object System.IO.Ports.SerialPort COM4,1200; $p.Open(); Start-Sleep 1; $p.Close()`
> → 화면이 꺼지면 esptool(115200)에서 Connect → 새 포트 선택 → Program → USB 다시 꽂기.

스프라이트는 SD카드에 있어야 해요 ([원본 웹 설치 페이지](https://socquique.github.io/TamaPoke/web/)의 Load sprites).
`TamaPoke/web/` 설치 페이지를 쓰려면 원본의 `sprites.pak`이 필요해요 (용량 때문에 이 폴더에는 없어요).

## 소리 넣기 (ko2) – USB 드라이브로 (ko3)

1. 아래로 스와이프 → **WiFi** → 맨 아래 **USB 드라이브 (SD카드)** → PC에 드라이브가 생겨요
2. `mons` 폴더에 WAV 복사 → PC에서 **꺼내기** → 보드가 바로 새 소리를 적용

울음소리 파일도 웹 설치 페이지의 **pick them manually**(파일 형식 "모든 파일")로 보낼 수 있어요.

최신 울음소리: `TamaPoke/tools/get_cries.py`를 실행하면 PokeAPI에서 1~151번의 최신 울음소리를 받아
`mons/cry001.wav`~`cry151.wav`로 변환해요 (Python + ffmpeg 필요). 게임 원본 음원(© Nintendo/Game Freak)이라
**개인용으로만** 쓰고 저장소에는 올리지 않아요.

SD카드 `/mons/` 폴더에 `bgm.wav`(평소), `battle_wild.wav`(배틀 중), `cry001.wav`~`cry151.wav`(울음소리)를 넣어요 (USB 드라이브로 넣었다면 재부팅 필요 없음).
형식은 **16kHz 모노 16비트 WAV**, 변환 방법은 `TamaPoke/README_KO.md`의 "SD카드 소리"를 보세요.

## 버전 기록

- **ko5.1** – USB 드라이브 읽기/쓰기를 여러 구역씩 한 번에 (PC에서 드라이브가 열리지 않던 문제),
  드라이브 화면에 읽기·쓰기 KB/오류 표시, SD 업데이트가 `mons/update.bin`도 찾음 (웹 설치 페이지로도 보낼 수 있게)
- **ko5** – SD카드 업데이트, 배틀 승리 시 20% 확률로 보관함 합류(포획은 항상), 도감 두 번 탭 나가기,
  화면 끈 동안 배경음 멈춤, 최신 울음소리 스크립트, 영문/숫자 줄 맞춤,
  USB 드라이브가 PC에서 용량 없이 열리지 않던 문제 수정 (드라이브 모드 시작/끝에 USB 재연결)
- **ko4** – 큰 시계, 보관함(다음 육성 포켓몬), 도감 기록, 소리 설정 메뉴, 훈련 3종, 실시간 저장,
  배틀 아이템(물약·포켓볼·포획), 패배 불이익 없음, 검정 글자·Noto 한글, 게이지 수치
- **ko3** – SD카드를 PC에서 USB 드라이브로 열기 (USB를 TinyUSB 방식으로 변경)
- **ko2** – SD카드 소리 추가 (v1.23에서 이식), 시리얼 `VOL` 명령, SD카드 접근 잠금
- **ko1** – 야생 배틀, 통신 대전/교환, WiFi 시간 맞춤, 한국어 기본

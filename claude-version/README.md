# TamaPoke KO v1.17-ko4 (클로드 버전)

> 이 폴더는 **클로드(Claude) 버전**이에요. 저장소 루트의 TamaPoke v1.23과는 따로 관리하는 별도 갈래예요.
> v1.23의 어드벤처·컬렉션·한글 폰트 기능은 이 버전에 없어요. (소리 기능만 ko2에서 옮겨 왔어요)

원본 [socquique/TamaPoke](https://github.com/socquique/TamaPoke) v1.17을 바탕으로
야생 배틀, 통신 대전/교환(ESP-NOW), WiFi 시간 자동 맞춤, 한국어 기본, SD카드 소리(배경음·배틀 배경음·울음소리), SD카드 USB 드라이브, 보관함·도감·배틀 아이템·훈련 3종·소리 설정·실시간 저장·큰 시계를 더했어요.
화면 모습은 `screenshots-ko4/` 폴더에 있어요 (PC 렌더러로 그린 실제 화면).

## 폴더 구성

| 파일 | 내용 |
|---|---|
| `tamapoke-ko-v1.17-ko4-app-0x10000.bin` | **업데이트용** (ko1~ko3 → ko4). 주소 **0x10000**. 키우던 포켓몬이 그대로 남아요 |
| `tamapoke-ko-v1.17-ko4.bin` | **새로 설치용** 통합 이미지. 주소 **0x0**. 저장 데이터가 초기화돼요 |
| `TamaPoke_v1.17-ko4_정리.txt` | 기능·사용법 요약 메모 |
| `TamaPoke/` | 전체 소스 (Arduino 스케치 폴더). 자세한 설명은 `TamaPoke/README_KO.md` |

## 펌웨어 올리기

1. Chrome/Edge에서 [ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/) 열기
2. Connect → 보드 포트 선택
3. 아래 중 하나 → Program
   - 이전 버전을 쓰고 있다면: 주소 `0x10000`에 `tamapoke-ko-v1.17-ko4-app-0x10000.bin` (Erase Flash 누르지 않기)
   - 처음 설치하거나 초기화하고 싶다면: 주소 `0x0`에 `tamapoke-ko-v1.17-ko4.bin`
4. "Hard resetting via RTS pin..."이 뜨면 완료. 화면이 안 바뀌면 RESET 버튼 또는 USB 다시 연결
5. 시계 화면(아래로 스와이프) 아래쪽에 `v1.17-ko4`가 보이면 성공

> **ko3을 올린 다음부터는** 업데이트할 때 BOOT 버튼이 필요할 수 있어요:
> BOOT를 누른 채 RESET을 눌렀다 떼기 → Connect → 새로 보이는 포트 선택 → Program → 끝나면 RESET.

스프라이트는 SD카드에 있어야 해요 ([원본 웹 설치 페이지](https://socquique.github.io/TamaPoke/web/)의 Load sprites).
`TamaPoke/web/` 설치 페이지를 쓰려면 원본의 `sprites.pak`이 필요해요 (용량 때문에 이 폴더에는 없어요).

## 소리 넣기 (ko2) – USB 드라이브로 (ko3)

1. 아래로 스와이프 → **WiFi** → 맨 아래 **USB 드라이브 (SD카드)** → PC에 드라이브가 생겨요
2. `mons` 폴더에 WAV 복사 → PC에서 **꺼내기** → 보드가 바로 새 소리를 적용

SD카드 `/mons/` 폴더에 `bgm.wav`(평소), `battle_wild.wav`(배틀 중), `cry001.wav`~`cry151.wav`(울음소리)를 넣어요 (USB 드라이브로 넣었다면 재부팅 필요 없음).
형식은 **16kHz 모노 16비트 WAV**, 변환 방법은 `TamaPoke/README_KO.md`의 "SD카드 소리"를 보세요.

## 버전 기록

- **ko4** – 큰 시계, 보관함(다음 육성 포켓몬), 도감 기록, 소리 설정 메뉴, 훈련 3종, 실시간 저장,
  배틀 아이템(물약·포켓볼·포획), 패배 불이익 없음, 검정 글자·Noto 한글, 게이지 수치
- **ko3** – SD카드를 PC에서 USB 드라이브로 열기 (USB를 TinyUSB 방식으로 변경)
- **ko2** – SD카드 소리 추가 (v1.23에서 이식), 시리얼 `VOL` 명령, SD카드 접근 잠금
- **ko1** – 야생 배틀, 통신 대전/교환, WiFi 시간 맞춤, 한국어 기본

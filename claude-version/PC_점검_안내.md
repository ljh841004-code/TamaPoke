# PC에 연결해서 실제 보드 점검하기 (ko12.9.8부터)

보드를 USB로 PC에 꽂아 두면, PC에서 실행한 Claude가 보드 화면을 찍고 눌러 보며 직접 점검해요.

## 준비 (한 번만)
1. 보드에 **ko12.9.8 이상**을 설치 (설정 화면 아래 `v1.17-ko12.9.8` 확인)
2. PC에 **Python 3** 설치 후 명령창에서: `pip install pyserial pillow`
3. PC에 이 저장소를 받기 (`git clone` 후 브랜치 `claude/funny-davinci-7owx9c`)
4. **Claude 데스크톱 앱**에서 그 폴더를 열기
   (또는 그 폴더의 터미널에서 `claude remote-control` → Claude Code 앱에 나타남)

## 점검할 때
1. 보드를 **데이터 되는 USB 케이블**로 PC에 연결 (충전 전용 케이블은 안 돼요)
2. Claude에게 "보드 점검 시작해줘"라고 말하기
3. 그동안 보드는 그대로 두면 돼요 (화면이 꺼지지 않게 가끔 보기만)

## Claude가 쓰는 도구: `TamaPoke/tools/devtest.py`
| 명령 | 하는 일 |
|---|---|
| `python TamaPoke/tools/devtest.py info` | 칩·배터리·메모리·SD·WiFi·버전 |
| `... shot` | 화면 찍기 (`devtest_out/`에 저장) |
| `... tap X Y` / `swipe X0 Y0 X1 Y1` / `hold X Y` | 화면 누르기·밀기·길게 누르기 |
| `... scr` | 지금 어느 화면인지 |
| `... tour` | 카드·도감·설정을 돌며 사진 찍기 |
| `... soak 60` | 60분 켜 두고 메모리·배터리 기록 |
| `... monkey 200 --i-made-a-backup` | 무작위 터치 (게임 상태가 바뀌어요: **먼저 세이브 백업**) |

## 안전
- 저장을 지우거나 바꾸는 콘솔 명령(WIPE, SPEC, LVL 등)은 이 도구가 막아요 (`--force` 없이는 안 보냄)
- 무작위 터치는 길게 누르기를 쓰지 않아요 (포켓몬 놓아주기·새로 시작은 길게 눌러야 해서 안 일어남)
- Erase Flash, 0x0 통합 이미지 쓰기는 절대 하지 않아요

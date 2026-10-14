# 다마포케 안드로이드 (개인 테스트용 APK)

펌웨어와 **같은 게임 코드**를 폰에서 돌리는 앱이에요. 플레이스토어용이 아니에요.

## 설치
1. `TamaPoke.apk`를 폰으로 옮겨서 눌러 설치 ("출처를 알 수 없는 앱" 허용)
2. 처음 켜면 파일 고르기 화면이 떠요 → `mons.pak` 선택 → 30초~1분 동안 풀어요
   - 다마포케 SD카드의 `mons.pak`을 복사해 와도 되고(음악까지 들어 있어요), 따로 받은 `mons.pak`도 돼요
   - 나중에 바꾸려면: 뒤로 버튼 → [mons.pak 고르기]
3. 폰의 **뒤로 버튼** = 앱 메뉴: [mons.pak 고르기] / [화면 끄기·켜기 (PWR)] / [닫기]

## 기기와 다른 점
- 시간 = 폰 시계. 앱을 닫아 둔 동안은 기기를 꺼 둔 것처럼 계산해요 (최대 2주)
- 저장 = 앱 안 (`nvs.bin`). 기기 세이브와는 따로예요
- 없는 것: 통신 대전/교환, WiFi 설정·업데이트, 걸음 센서·기울기(산책은 "센서 없음"), 진동
- 화면: 둥근 화면 그대로, 폰 가운데보다 조금 위

## 만들기 (PC)
```
ANDROID_SDK=<sdk> android/build_apk.sh        # -> android/build/TamaPoke.apk
android/test/run_desktop.sh <mons.pak>          # 같은 코드를 PC에서 점검 (pak 풀기, 실행, 저장, 소리 스레드)
```
SDK: platforms;android-34, build-tools;34.0.0, ndk;26.3.11579264 + JDK + arduino-cli.
`debug.keystore`는 개인 테스트용 서명 키예요 (같은 키로 서명해야 덮어쓰기 설치가 돼요).

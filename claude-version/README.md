# TamaPoke KO v1.17-ko1 (클로드 버전)

> 이 폴더는 **클로드(Claude) 버전**이에요. 저장소 루트의 TamaPoke v1.23과는 따로 관리하는 별도 갈래예요.
> v1.23의 어드벤처·컬렉션·음악·한글 폰트 기능은 이 버전에 없어요.

원본 [socquique/TamaPoke](https://github.com/socquique/TamaPoke) v1.17을 바탕으로
야생 배틀, 통신 대전/교환(ESP-NOW), WiFi 시간 자동 맞춤, 한국어 기본을 더했어요.

## 폴더 구성

| 파일 | 내용 |
|---|---|
| `tamapoke-ko-v1.17-ko1.bin` | 바로 올릴 수 있는 펌웨어 (주소 **0x0**, 부트로더 포함 통합 이미지) |
| `TamaPoke_v1.17-ko1_정리.txt` | 기능·사용법 요약 메모 |
| `TamaPoke/` | 전체 소스 (Arduino 스케치 폴더). 자세한 설명은 `TamaPoke/README_KO.md` |

## 펌웨어 올리기

1. Chrome/Edge에서 [ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/) 열기
2. Connect → 보드 포트 선택
3. 주소 `0x0`에 `tamapoke-ko-v1.17-ko1.bin` 선택 → Program
4. "Hard resetting via RTS pin..."이 뜨면 완료. 화면이 안 바뀌면 RESET 버튼 또는 USB 다시 연결

스프라이트는 SD카드에 있어야 해요 ([원본 웹 설치 페이지](https://socquique.github.io/TamaPoke/web/)의 Load sprites).
`TamaPoke/web/` 설치 페이지를 쓰려면 원본의 `sprites.pak`이 필요해요 (용량 때문에 이 폴더에는 없어요).

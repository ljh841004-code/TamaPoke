# GPS 시간 맞춤 계획 (보류: GPS 모듈 장착 후 작업)

- 보드: Waveshare ESP32-S3-Touch-AMOLED-1.75, **GPS 없는 버전** (케이스 없음)
- 이유: 시계 칩(PCF85063)이 메인 배터리로만 전원을 받아 배터리를 빼면 시간이 초기화됨
- 연결 (아래쪽 8핀 단자): GPS VCC → 3V3, GND → GND, GPS TX → **IO17**, GPS RX → **IO18**(선택)
  - IO16·17·18은 현재 펌웨어에서 사용 안 함. RXD/TXD는 부팅 메시지가 나와서 피함
- 모듈 후보: 3.3V + UART(NMEA) 출력 (ATGM336H, u-blox NEO-6M/M8N, LC76G 등)
- 펌웨어 할 일:
  - UART1(IO17/18)로 NMEA $GxRMC 읽기 → 날짜·시간(UTC) → 설정된 시간대 적용
  - WiFi 시간과 같은 처리(applyNetTime): 시계 맞춤 + 배터리 뺀 동안의 시간 반영
  - 켤 때와 하루 한 번만 받고, 받은 뒤 GPS 대기 명령(모듈 종류에 맞게)
  - 설정 화면에 "GPS 시간 받음" 표시
- 사용자가 모듈을 장착하면 모듈 이름을 받아서 작업

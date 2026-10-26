# 하드웨어 사실 — NU40-DK (고양이 명함 키링)

> 보드 변경: NU87-TinyDK(Realtek) → **NU40-DK(Nordic nRF52840)**. 2026-10-06.
> 단계 3부터(펌웨어) 이 값을 쓴다. 단계 1~2(PC 미리보기·QR)는 보드 무관이라 영향 없음.

## MCU
- **Nordic nRF52840**, Cortex-M4F @64MHz, **단정밀도 FPU만** (double 피하고 float)
- RAM 256KB / Flash 1MB
- **로직 3.3V** — 5V 모듈 직결 금지
- 동작 1.7~3.6V → LiPo 직결 가능

## Arduino 설치 / 빌드
- 보드매니저 URL:
  `https://raw.githubusercontent.com/Nucode01/Adafruit_nRF52_Arduino/refs/heads/master/package_nuduino_index.json`
- 패키지 `nucode` (Adafruit nRF52 Arduino 코어 포크), 보드 "NU40DK nRF52840", board id `nu40dk`
- FQBN: **`nucode:nrf52:nu40dk`** ✅확정. 코어 `nucode:nrf52` 1.0.2 설치됨. 보드는 "NU40DK nRF52840"로 인식된다.
- arduino-cli: Arduino IDE 에 들어 있는 것을 써도 된다.
- 업로드: DFU 패키지 자동(1200bps touch). `arduino-cli upload -p /dev/cu.usbmodem101 --fqbn nucode:nrf52:nu40dk <sketch>` 정상 동작 확인.
- 라이브러리: **Adafruit_SSD1306 + Adafruit_GFX 우선**, 안 되면 U8g2
- **BLE 사용 안 함 — Bluefruit 시작하지 말 것**

## 핀
- 핀 규칙: **Arduino 핀 N = P0.N**, **Arduino 핀 32+n = P1.n**
- I2C (OLED + IMU 공유 버스, **400kHz**): **SDA = 26 (P0.26), SCL = 27 (P0.27)** — 코어 기본 Wire
- IMU 인터럽트(선택): **Arduino 32 (P1.00)**
- 온보드 LED1~4 = **13~16** (디버깅용으로 사용 가능)

### 사용 금지 핀
P0.11 / P0.12 / P0.24 / P0.25 (버튼), P0.13~16 (LED, 디버깅 외 금지),
P0.18 (RESET), P0.00 / P0.01 (32.768kHz 크리스털),
P0.17 · P0.19~23 (QSPI 예약), P0.09 / P0.10 (NFC)

## 주변장치 (I2C 스캔으로 확정 2026-10-06)
- OLED: **0x3C 확정**, SSD1306 128x64 (Adafruit_SSD1306 2.5.17로 init 성공). ⚠️한글 폰트 없음 → OLED 글자는 ASCII만.
- IMU: **0x6A = LSM6DS3TR-C 확정** (WHO_AM_I=0x6A). 내장 wake-up 있음(단계8 활용). 레지스터 직접 접근으로 읽음(OUTX_L_G 0x22부터 자이로3+가속도3). 보정값 → firmware/imu_calibration.h
- 전원: 150mAh LiPo를 보드 배터리 커넥터에 직결. **충전 회로 없음**

## 저전력 (단계 8)
- IMU 인터럽트(32번) 연결 시: nRF52840 **System ON sleep** 대기 → 인터럽트로 깨우기
- 인터럽트 미연결 시: 저주기 폴링
- IMU가 LSM6DS3 계열이면 칩 내장 wake-up 사용 여부 판단해 제안

## 만들 순서 변경점
- 단계 3이 "**보드 패키지 설치** + I2C 스캐너 + OLED 글자 테스트"로 확장
- **단계 8 추가**: 저전력 대기 (System ON sleep + 인터럽트 깨우기)
- 각 단계에서 **NU40DK 컴파일 성공까지** 확인

/*
 * 단계 3-A: I2C 스캐너 (NU40-DK / nRF52840)
 *
 * 목적
 *   1) 보드에 스케치가 제대로 올라가는지 확인
 *   2) 같은 I2C 버스에 붙은 OLED / IMU 의 주소를 찾아 모델을 추정
 *
 * 배선 (프롬프트 확정값)
 *   SDA = 26 (P0.26), SCL = 27 (P0.27)  ← nucode 코어 기본 Wire 핀
 *   I2C 속도 400kHz, OLED와 IMU가 같은 버스 공유
 *
 * 결과는 시리얼(115200)로 2초마다 반복 출력한다.
 * (모니터를 언제 열어도 결과를 볼 수 있게 계속 스캔)
 */

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  // nRF52 USB 시리얼이 붙을 시간을 조금 주되, 안 붙어도 계속 진행(무한 대기 금지)
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0 < 2000)) { }

  Wire.begin();              // 기본 핀 = SDA 26 / SCL 27 (nu40dk 코어 기본)
  Wire.setClock(400000);     // 400kHz

  Serial.println();
  Serial.println(F("=== NU40-DK I2C 스캐너 시작 ==="));
}

// 찾은 주소로 부품 종류를 추정해 한 줄 설명
void guess(uint8_t addr) {
  switch (addr) {
    case 0x3C:
    case 0x3D: Serial.print(F("  -> OLED 로 추정 (SSD1306/SH1106)")); break;
    case 0x68:
    case 0x69: Serial.print(F("  -> IMU 로 추정 (MPU6050 계열)")); break;
    case 0x6A:
    case 0x6B: Serial.print(F("  -> IMU 로 추정 (LSM6DS3 계열, wake-up 내장)")); break;
    default: break;
  }
}

void loop() {
  Serial.println(F("\n--- 스캔 ---"));
  uint8_t count = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();   // 0 = 응답 있음(장치 존재)
    if (err == 0) {
      Serial.print(F("발견: 0x"));
      if (addr < 16) Serial.print('0');
      Serial.print(addr, HEX);
      guess(addr);
      Serial.println();
      count++;
    }
  }

  if (count == 0) {
    Serial.println(F("장치 없음. 배선(SDA26/SCL27)·전원·풀업 확인."));
  } else {
    Serial.print(F("총 "));
    Serial.print(count);
    Serial.println(F("개 발견."));
  }

  delay(2000);
}

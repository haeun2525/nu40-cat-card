/*
 * 단계 3-B: OLED 표시 + IMU(LSM6DS3) 동작 확인 + 보정
 *
 * 확인된 하드웨어 (I2C 스캔 결과)
 *   OLED : 0x3C  (SSD1306 128x64)
 *   IMU  : 0x6A  (LSM6DS3 계열, wake-up 내장)
 *   I2C  : SDA 26 / SCL 27, 400kHz
 *
 * 하는 일
 *   1) OLED 에 글자를 띄워 '디스플레이가 뜨는지' 확인
 *   2) IMU 를 레지스터 직접 접근으로 읽어 '동작하는지' 확인 (라이브러리 의존 없음)
 *   3) 부팅 시 자동 보정: 평평하게 정지한 상태에서 가속도/자이로 바이어스를 측정
 *      -> 보정값을 시리얼에 #define 형태로 출력하고 OLED 에도 표시
 *   4) 이후 실시간으로 |가속도|(g) 와 자이로를 보여주고, 흔들면 "SHAKE" 표시
 *
 * 시리얼 명령: c = 다시 보정
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- 설정 ----------
#define OLED_ADDR 0x3C
#define IMU_ADDR  0x6A

// LSM6DS3 레지스터
#define REG_WHO_AM_I 0x0F   // LSM6DS3=0x69, LSM6DS3TR-C=0x6A
#define REG_CTRL1_XL 0x10   // 가속도 설정
#define REG_CTRL2_G  0x11   // 자이로 설정
#define REG_CTRL3_C  0x12   // 자동 주소증가 등
#define REG_OUTX_L_G 0x22   // 자이로 6바이트, 이어서 가속도 6바이트(0x28~)

// 감도(풀스케일 기본값: 가속도 ±2g, 자이로 250dps)
const float ACC_SENS = 0.061e-3f; // g / LSB
const float GYR_SENS = 8.75e-3f;  // dps / LSB

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// 보정값(바이어스). 가속도는 '평평할 때 (0,0,1g)' 기준.
float accOff[3] = {0, 0, 0};
float gyrOff[3] = {0, 0, 0};
float restMag = 1.0f;     // 정지 시 |a| (정상이면 ~1.0g)
float accNoise = 0.0f;    // 정지 시 |a| 표준편차 -> 흔들기 임계값 참고
bool  imuOK = false;

// ---------- I2C 로우레벨 ----------
void imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}
uint8_t imuRead8(uint8_t reg) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)IMU_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}
// 자이로3 + 가속도3 = int16 6개를 한 번에 읽음
void imuReadAll(float g[3], float a[3]) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(REG_OUTX_L_G);
  Wire.endTransmission(false);
  Wire.requestFrom((int)IMU_ADDR, 12);
  int16_t raw[6];
  for (int i = 0; i < 6; i++) {
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    raw[i] = (int16_t)((hi << 8) | lo);
  }
  g[0] = raw[0] * GYR_SENS; g[1] = raw[1] * GYR_SENS; g[2] = raw[2] * GYR_SENS;
  a[0] = raw[3] * ACC_SENS; a[1] = raw[4] * ACC_SENS; a[2] = raw[5] * ACC_SENS;
}

bool imuBegin() {
  uint8_t who = imuRead8(REG_WHO_AM_I);
  Serial.print(F("IMU WHO_AM_I = 0x")); Serial.println(who, HEX);
  if (who != 0x69 && who != 0x6A) return false;   // LSM6DS3 / TR-C
  imuWrite(REG_CTRL3_C, 0x44);  // BDU=1, IF_INC=1 (연속읽기 안정)
  imuWrite(REG_CTRL1_XL, 0x40); // 가속도 104Hz, ±2g
  imuWrite(REG_CTRL2_G,  0x40); // 자이로 104Hz, 250dps
  delay(50);
  return true;
}

// ---------- OLED 보조 ----------
void oledMsg(const char* l1, const char* l2 = "", const char* l3 = "") {
  oled.clearDisplay();
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);  oled.println(l1);
  oled.setCursor(0, 20); oled.println(l2);
  oled.setCursor(0, 40); oled.println(l3);
  oled.display();
}

// ---------- 보정 ----------
void calibrate() {
  // 1) 카운트다운(평평·정지 유도)
  for (int s = 3; s >= 1; s--) {
    char buf[24]; snprintf(buf, sizeof(buf), "%d", s);
    oledMsg("CALIB: keep flat", "hold STILL", buf);
    Serial.print(F("보정 시작까지... ")); Serial.println(s);
    delay(1000);
  }
  oledMsg("Measuring...", "", "");

  // 2) 샘플 수집
  const int N = 256;
  double sg[3] = {0,0,0}, sa[3] = {0,0,0};
  double sMag = 0, sMag2 = 0;
  float gmaxAbs = 0;                 // 자이로 최대치(움직임 감지용)
  for (int i = 0; i < N; i++) {
    float g[3], a[3];
    imuReadAll(g, a);
    for (int k = 0; k < 3; k++) { sg[k] += g[k]; sa[k] += a[k]; }
    float mag = sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
    sMag += mag; sMag2 += (double)mag * mag;
    for (int k = 0; k < 3; k++) gmaxAbs = max(gmaxAbs, fabsf(g[k]));
    delay(5);
  }
  float ma[3] = { (float)(sa[0]/N), (float)(sa[1]/N), (float)(sa[2]/N) };
  for (int k = 0; k < 3; k++) gyrOff[k] = (float)(sg[k]/N);
  restMag = (float)(sMag / N);
  accNoise = sqrtf(max(0.0, sMag2/N - (sMag/N)*(sMag/N)));

  // 가속도 바이어스: 평평할 때 (0,0,1g) 가 되도록
  accOff[0] = ma[0] - 0.0f;
  accOff[1] = ma[1] - 0.0f;
  accOff[2] = ma[2] - 1.0f;

  // 3) 품질 판정(보정 중 움직였는지)
  bool stable = (gmaxAbs < 5.0f) && (accNoise < 0.03f); // 자이로<5dps, |a|노이즈<0.03g
  float shakeSug = max(0.35f, accNoise * 8.0f);          // 흔들기 임계값 제안

  // 4) 결과 출력(복붙용 #define)
  Serial.println(F("\n===== IMU 보정값 ====="));
  Serial.print(F("WHO_AM_I OK, 정지 |a| = ")); Serial.print(restMag, 3);
  Serial.print(F(" g (정상 ~1.0),  노이즈 = ")); Serial.print(accNoise, 4); Serial.println(F(" g"));
  Serial.print(F("자이로 최대치(보정중) = ")); Serial.print(gmaxAbs, 2); Serial.println(F(" dps"));
  Serial.println(stable ? F(">> 안정적으로 보정됨") : F(">> 불안정! 움직였을 수 있음. 'c'로 다시 보정"));
  Serial.println(F("--- 코드 config 에 넣을 값 ---"));
  Serial.print(F("#define IMU_ACC_OFF_X  ")); Serial.println(accOff[0], 5);
  Serial.print(F("#define IMU_ACC_OFF_Y  ")); Serial.println(accOff[1], 5);
  Serial.print(F("#define IMU_ACC_OFF_Z  ")); Serial.println(accOff[2], 5);
  Serial.print(F("#define IMU_GYR_OFF_X  ")); Serial.println(gyrOff[0], 4);
  Serial.print(F("#define IMU_GYR_OFF_Y  ")); Serial.println(gyrOff[1], 4);
  Serial.print(F("#define IMU_GYR_OFF_Z  ")); Serial.println(gyrOff[2], 4);
  Serial.print(F("#define SHAKE_THRESHOLD ")); Serial.print(shakeSug, 2);
  Serial.println(F("   // |a|-1g 가 이보다 크면 피크(권장 시작값)"));
  Serial.println(F("======================\n"));

  char b1[24], b2[24];
  snprintf(b1, sizeof(b1), "|a|=%.2fg no=%.3f", restMag, accNoise);
  snprintf(b2, sizeof(b2), stable ? "OK  th=%.2f" : "UNSTABLE press c", shakeSug);
  oledMsg("IMU calib done", b1, b2);
  delay(1500);
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  Wire.begin();            // SDA26/SCL27 (nu40dk 기본)
  Wire.setClock(400000);

  // --- OLED ---
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED(0x3C) 초기화 실패! 배선/주소 확인"));
  } else {
    Serial.println(F("OLED OK"));
    oledMsg("NU40-DK", "OLED OK (0x3C)", "IMU check...");
    delay(1200);
  }

  // --- IMU ---
  imuOK = imuBegin();
  if (!imuOK) {
    Serial.println(F("IMU(0x6A) 초기화 실패!"));
    oledMsg("IMU FAIL", "0x6A no resp", "check wiring");
    return;
  }
  Serial.println(F("IMU OK (LSM6DS3)"));

  calibrate();
}

void loop() {
  if (Serial.available() && Serial.read() == 'c') calibrate();
  if (!imuOK) { delay(500); return; }

  float g[3], a[3];
  imuReadAll(g, a);
  // 보정 적용
  float ax = a[0]-accOff[0], ay = a[1]-accOff[1], az = a[2]-accOff[2];
  float mag = sqrtf(ax*ax + ay*ay + az*az);
  float dev = fabsf(mag - 1.0f);                 // 1g 에서 벗어난 정도
  float shakeSug = max(0.35f, accNoise * 8.0f);
  bool shake = dev > shakeSug;

  // 시리얼
  Serial.print(F("|a|=")); Serial.print(mag, 2);
  Serial.print(F("g dev=")); Serial.print(dev, 2);
  Serial.print(F(" gz=")); Serial.print(g[2]-gyrOff[2], 1);
  Serial.println(shake ? F("  <== SHAKE") : F(""));

  // OLED: 숫자 + 막대 + SHAKE
  oled.clearDisplay();
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);  oled.print(F("IMU LSM6DS3 0x6A"));
  oled.setCursor(0, 14); oled.print(F("|a|=")); oled.print(mag, 2); oled.print(F("g"));
  oled.setCursor(0, 28); oled.print(F("dev=")); oled.print(dev, 2);
  int bar = (int)(min(dev, 1.0f) * 120);          // 막대 길이
  oled.drawRect(0, 42, 122, 8, SSD1306_WHITE);
  oled.fillRect(1, 43, bar, 6, SSD1306_WHITE);
  oled.setTextSize(2);
  if (shake) { oled.setCursor(40, 52); oled.print(F("SHAKE")); }
  oled.display();

  delay(120);
}

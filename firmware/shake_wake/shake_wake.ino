/*
 * 단계 6: 흔들어서 켜기 (OFF → 흔들기 감지 → 등장 연출 → 명함 표시)
 *
 * 상태
 *   OFF  : OLED 꺼짐. IMU만 감시. 흔들림 감지되면 깨어남.
 *   (연출): 걸어나와 인사→앉기→이름→QR (단계5와 동일, 느린 걷기 적용)
 *   SHOW : 명함 유지. (30초 타이머/잠들기는 단계7에서 추가)
 *
 * 흔들기 판정 (설정값)
 *   |가속도| 가 1g 에서 SHAKE_THRESHOLD 이상 벗어나는 '피크' 가
 *   SHAKE_WINDOW_MS(1초) 안에 SHAKE_COUNT(3)번 이상이면 켬.
 *   방향 무관(|a| 크기 사용) → 키링이 어떤 각도여도 동작.
 *
 * 테스트용 시리얼: s = 강제로 끄기(OFF),  r = 연출 다시 재생
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "cat_bitmaps.h"
#include "cat_qr.h"
#include "imu_calibration.h"   // SHAKE_THRESHOLD, IMU_ACC_SCALE 등

#define OWNER_NAME "Emily, Kim"
#define OLED_ADDR  0x3C
#define IMU_ADDR   0x6A

// --- 흔들기 설정 ---  (임계값은 보정 헤더의 SHAKE_THRESHOLD 사용)
#define SHAKE_COUNT       3
#define SHAKE_WINDOW_MS   1000

// --- 명함 표시(SHOW) 설정 ---
#define SHOW_TIMEOUT_MS   30000   // 이 시간 지나면 잠들고 꺼짐
#define BLINK_MIN_MS      3000    // 눈 깜빡임 간격 랜덤 범위
#define BLINK_MAX_MS      6000
#define BLINK_DUR_MS      150     // 눈 감고 있는 시간

// --- 연출 타이밍(ms) --- (걷기 느리게)
#define T_WALK_IN    2200
#define T_WAVE       1000
#define T_WALK_LEFT  1500
#define T_NAME        500
#define T_QR          500
#define FRAME_MS       33

// --- 위치(앵커=바닥 중앙) ---
const float ENTER_X = 140, ENTER_Y = 60;
const float CENTER_X = 64,  CENTER_Y = 61;
const float LEFT_X  = 31,  LEFT_Y  = 61;

// --- IMU 레지스터 ---
#define REG_WHO_AM_I 0x0F
#define REG_CTRL1_XL 0x10
#define REG_CTRL3_C  0x12
#define REG_OUTX_L_XL 0x28
const float ACC_SENS = 0.061e-3f;  // ±2g, g/LSB

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

enum State { ST_OFF, ST_SHOW };
State state = ST_OFF;

// ---------------- IMU ----------------
void imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_ADDR); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}
uint8_t imuRead8(uint8_t reg) {
  Wire.beginTransmission(IMU_ADDR); Wire.write(reg); Wire.endTransmission(false);
  Wire.requestFrom((int)IMU_ADDR, 1); return Wire.available() ? Wire.read() : 0;
}
bool imuBegin() {
  uint8_t who = imuRead8(REG_WHO_AM_I);
  if (who != 0x69 && who != 0x6A) return false;
  imuWrite(REG_CTRL3_C, 0x44);   // BDU + 자동증가
  imuWrite(REG_CTRL1_XL, 0x40);  // 가속도 104Hz ±2g
  delay(30);
  return true;
}
// 보정 반영한 |가속도|(g)
float readAccMag() {
  Wire.beginTransmission(IMU_ADDR); Wire.write(REG_OUTX_L_XL); Wire.endTransmission(false);
  Wire.requestFrom((int)IMU_ADDR, 6);
  int16_t r[3];
  for (int i = 0; i < 3; i++) { uint8_t lo = Wire.read(), hi = Wire.read(); r[i] = (int16_t)((hi << 8) | lo); }
  float ax = r[0] * ACC_SENS, ay = r[1] * ACC_SENS, az = r[2] * ACC_SENS;
  return sqrtf(ax*ax + ay*ay + az*az) * IMU_ACC_SCALE;
}

// ---------------- 흔들기 감지 ----------------
unsigned long peakTimes[8] = {0};
int  peakHead = 0;
bool armed = true;

void clearPeaks() { for (int i = 0; i < 8; i++) peakTimes[i] = 0; armed = true; }

// 피크를 모으다가 1초 내 3번 이상이면 true
bool pollShake() {
  float dev = fabsf(readAccMag() - 1.0f);     // 1g 에서 벗어난 정도
  float hi = SHAKE_THRESHOLD, lo = SHAKE_THRESHOLD * 0.5f;  // 히스테리시스
  if (dev < lo) armed = true;                  // 충분히 가라앉으면 재무장
  if (armed && dev > hi) {
    armed = false;
    unsigned long now = millis();
    peakTimes[peakHead] = now; peakHead = (peakHead + 1) % 8;
    int cnt = 0;
    for (int i = 0; i < 8; i++) if (peakTimes[i] && now - peakTimes[i] <= SHAKE_WINDOW_MS) cnt++;
    Serial.print(F("peak! dev=")); Serial.print(dev, 2);
    Serial.print(F(" cnt=")); Serial.println(cnt);
    if (cnt >= SHAKE_COUNT) { clearPeaks(); return true; }
  }
  return false;
}

// ---------------- 그리기(단계5와 동일) ----------------
float smooth(float p){ if(p<0)p=0; if(p>1)p=1; return p*p*(3-2*p); }
void drawCat(const uint8_t* b,int w,int h,int ax,int ay,float tx,float ty){
  oled.drawBitmap((int)lroundf(tx-ax),(int)lroundf(ty-ay),b,w,h,SSD1306_WHITE);
}
void drawName(int n){
  int fw=(int)strlen(OWNER_NAME)*6, x=(64-fw)/2; if(x<0)x=0;
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE); oled.setCursor(x,8);
  for(int i=0;i<n&&OWNER_NAME[i];i++) oled.write(OWNER_NAME[i]);
}
void drawQR(int rows){
  if(rows<=0)return;
  oled.drawBitmap(64,0,QR_BMP,QR_W,QR_H,SSD1306_WHITE);
  if(rows<64) oled.fillRect(64,rows,64,64-rows,SSD1306_BLACK);
}
void drawHi(){
  oled.fillRoundRect(78,16,28,13,3,SSD1306_WHITE);
  oled.fillTriangle(80,27,72,31,82,29,SSD1306_WHITE);
  oled.setTextSize(1); oled.setTextColor(SSD1306_BLACK); oled.setCursor(82,19); oled.print("Hi!");
}
void walkTo(float ax,float ay,float bx,float by,int dur){
  unsigned long t0=millis();
  for(;;){
    float raw=(millis()-t0)/(float)dur; if(raw>1)raw=1;
    float s=smooth(raw), x=ax+(bx-ax)*s, y=ay+(by-ay)*s;
    int step=((millis()-t0)/160)%2;
    oled.clearDisplay();
    if(step==0) drawCat(CAT_WALK0,CAT_WALK0_W,CAT_WALK0_H,CAT_WALK0_AX,CAT_WALK0_AY,x,y);
    else        drawCat(CAT_WALK1,CAT_WALK1_W,CAT_WALK1_H,CAT_WALK1_AX,CAT_WALK1_AY,x,y);
    oled.display();
    if(raw>=1)break; delay(FRAME_MS);
  }
}
// 명함 한 장 (eye=blink면 눈 감은 고양이)
void drawCardWithEye(bool blink){
  oled.clearDisplay();
  drawCat(blink?CAT_BLINK:CAT_SIT, CAT_SIT_W,CAT_SIT_H,CAT_SIT_AX,CAT_SIT_AY,LEFT_X,LEFT_Y);
  drawName(strlen(OWNER_NAME));
  drawQR(64);
  oled.display();
}

// 엎드려 잠드는 짧은 연출(약 1초)
void playSleep(){
  Serial.println(F("30 s passed -> sleep"));
  drawCardWithEye(true); delay(350);            // 눈 감기
  oled.clearDisplay();                           // 엎드려 자기
  drawCat(CAT_SLEEP,CAT_SLEEP_W,CAT_SLEEP_H,CAT_SLEEP_AX,CAT_SLEEP_AY,33,62);
  oled.display(); delay(700);
}
void playIntro(){
  walkTo(ENTER_X,ENTER_Y,CENTER_X,CENTER_Y,T_WALK_IN);
  unsigned long t0=millis();
  while(millis()-t0<T_WAVE){
    int ph=((millis()-t0)/150)%2;
    oled.clearDisplay();
    drawCat(ph?CAT_GREET1:CAT_GREET0,CAT_GREET1_W,CAT_GREET1_H,CAT_GREET1_AX,CAT_GREET1_AY,CENTER_X,CENTER_Y);
    drawHi(); oled.display(); delay(FRAME_MS);
  }
  walkTo(CENTER_X,CENTER_Y,LEFT_X,LEFT_Y,T_WALK_LEFT);
  int total=strlen(OWNER_NAME); t0=millis();
  for(;;){ float raw=(millis()-t0)/(float)T_NAME; if(raw>1)raw=1;
    oled.clearDisplay(); drawCat(CAT_SIT,CAT_SIT_W,CAT_SIT_H,CAT_SIT_AX,CAT_SIT_AY,LEFT_X,LEFT_Y);
    drawName((int)lroundf(raw*total)); oled.display(); if(raw>=1)break; delay(FRAME_MS); }
  t0=millis();
  for(;;){ float raw=(millis()-t0)/(float)T_QR; if(raw>1)raw=1;
    oled.clearDisplay(); drawCat(CAT_SIT,CAT_SIT_W,CAT_SIT_H,CAT_SIT_AX,CAT_SIT_AY,LEFT_X,LEFT_Y);
    drawName(total); drawQR((int)lroundf(smooth(raw)*64)); oled.display(); if(raw>=1)break; delay(FRAME_MS); }
}

// ---------------- 전원 상태 ----------------
void screenOff(){ oled.clearDisplay(); oled.display(); oled.ssd1306_command(SSD1306_DISPLAYOFF); }
void screenOn(){  oled.ssd1306_command(SSD1306_DISPLAYON); }

// SHOW 상태 타이머
unsigned long showStart = 0;     // 꺼짐까지 재는 기준(흔들면 리셋)
unsigned long nextBlinkAt = 0;   // 다음 눈 깜빡임 시각
bool blinkShowing = false;
unsigned long blinkOffAt = 0;

void enterShow(){
  state = ST_SHOW;
  clearPeaks();                              // 켤 때 쓴 피크 잔상 제거
  showStart = millis();
  blinkShowing = false;
  nextBlinkAt = millis() + random(BLINK_MIN_MS, BLINK_MAX_MS);
  drawCardWithEye(false);
}

void wake(){
  Serial.println(F("Shake detected -> wake up"));
  screenOn();
  playIntro();
  enterShow();
}
void goOff(){
  Serial.println(F("-> OFF (waiting for a shake)"));
  screenOff();
  clearPeaks();
  state = ST_OFF;
}

void setup(){
  Serial.begin(115200);
  Wire.begin(); Wire.setClock(400000);
  if(!oled.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR)){ Serial.println(F("OLED not found")); return; }
  if(!imuBegin()){ Serial.println(F("IMU not found")); return; }
  randomSeed(micros());
  Serial.println(F("Ready. Screen is off - shake to wake."));
  goOff();
}

void loop(){
  if(Serial.available()){
    char c=Serial.read();
    if(c=='s') goOff();
    else if(c=='r'){ screenOn(); playIntro(); enterShow(); }
  }

  if(state==ST_OFF){
    if(pollShake()) wake();
    delay(8);                 // ~100Hz 감시
    return;
  }

  // ---- ST_SHOW ----
  // 흔들면 연출은 다시 안 하고, 꺼지는 타이머만 처음부터 다시 셈
  if(pollShake()){
    showStart = millis();
    Serial.println(F("Shake while showing -> 30 s timer reset"));
  }

  // 가끔 눈 깜빡임 (전환 시에만 다시 그림 → 깜빡임 최소)
  unsigned long now = millis();
  if(!blinkShowing && now >= nextBlinkAt){
    blinkShowing = true; blinkOffAt = now + BLINK_DUR_MS;
    drawCardWithEye(true);
  } else if(blinkShowing && now >= blinkOffAt){
    blinkShowing = false;
    drawCardWithEye(false);
    nextBlinkAt = now + random(BLINK_MIN_MS, BLINK_MAX_MS);
  }

  // 30초 지나면 잠들고 꺼짐
  if(now - showStart >= SHOW_TIMEOUT_MS){
    playSleep();
    goOff();
  }

  delay(10);
}

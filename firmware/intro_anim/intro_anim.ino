/*
 * 단계 5: 등장 연출 (실제 OLED)
 *
 * 흐름 (PC 미리보기와 동일, 모든 시간은 아래 설정값 ms)
 *   1) 오른쪽 아래 밖 → 화면 가운데로 걸어 들어옴
 *   2) 가운데서 앞발 흔들며 "Hi!"
 *   3) 왼쪽으로 걸어가 앉음
 *   4) 이름 한 글자씩
 *   5) QR 위 → 아래로
 *   끝나면 완성된 명함 화면 유지.
 *
 * 재생: 부팅 시 1회 자동, 이후 시리얼 'r' 로 다시 재생.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "cat_bitmaps.h"
#include "cat_qr.h"

#define OWNER_NAME "Emily, Kim"
#define OLED_ADDR  0x3C

// --- 연출 타이밍(ms) ---  (미리보기 CONFIG 와 대응)
#define T_WALK_IN    2200
#define T_WAVE       1000
#define T_WALK_LEFT  1500
#define T_NAME        500
#define T_QR          500
#define FRAME_MS       33   // ~30fps

// --- 위치(앵커=바닥 중앙) ---
const float ENTER_X = 140, ENTER_Y = 60;   // 화면 밖 오른쪽 아래
const float CENTER_X = 64,  CENTER_Y = 61;  // 가운데 멈춤
const float LEFT_X  = 31,  LEFT_Y  = 61;   // 왼쪽 앉는 자리

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

float smooth(float p) { if (p < 0) p = 0; if (p > 1) p = 1; return p * p * (3 - 2 * p); }

// 앵커(바닥 중앙)가 (tx,ty)에 오도록 비트맵을 그림
void drawCat(const uint8_t* bmp, int w, int h, int ax, int ay, float tx, float ty) {
  oled.drawBitmap((int)lroundf(tx - ax), (int)lroundf(ty - ay), bmp, w, h, SSD1306_WHITE);
}

// 이름 앞에서부터 n글자, 왼쪽 64px 가운데 정렬
void drawName(int n) {
  int fullW = (int)strlen(OWNER_NAME) * 6;
  int x = (64 - fullW) / 2; if (x < 0) x = 0;
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE); oled.setCursor(x, 8);
  for (int i = 0; i < n && OWNER_NAME[i]; i++) oled.write(OWNER_NAME[i]);
}

// QR 위에서부터 rows 픽셀만 보이게
void drawQR(int rows) {
  if (rows <= 0) return;
  oled.drawBitmap(64, 0, QR_BMP, QR_W, QR_H, SSD1306_WHITE);
  if (rows < 64) oled.fillRect(64, rows, 64, 64 - rows, SSD1306_BLACK); // 아직 안 나온 부분 가림
}

// 머리 옆 "Hi!" 말풍선
void drawHi() {
  oled.fillRoundRect(78, 16, 28, 13, 3, SSD1306_WHITE);
  oled.fillTriangle(80, 27, 72, 31, 82, 29, SSD1306_WHITE);   // 꼬리
  oled.setTextSize(1); oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(82, 19); oled.print("Hi!");
}

// a→b 로 걸어 이동(옆모습 걷기 프레임 교대)
void walkTo(float ax, float ay, float bx, float by, int dur) {
  unsigned long t0 = millis();
  for (;;) {
    float raw = (millis() - t0) / (float)dur; if (raw > 1) raw = 1;
    float s = smooth(raw);
    float x = ax + (bx - ax) * s, y = ay + (by - ay) * s;
    int step = ((millis() - t0) / 160) % 2;      // 160ms마다 다리 교대
    oled.clearDisplay();
    if (step == 0) drawCat(CAT_WALK0, CAT_WALK0_W, CAT_WALK0_H, CAT_WALK0_AX, CAT_WALK0_AY, x, y);
    else           drawCat(CAT_WALK1, CAT_WALK1_W, CAT_WALK1_H, CAT_WALK1_AX, CAT_WALK1_AY, x, y);
    oled.display();
    if (raw >= 1) break;
    delay(FRAME_MS);
  }
}

void playIntro() {
  // 1) 걸어 들어오기
  walkTo(ENTER_X, ENTER_Y, CENTER_X, CENTER_Y, T_WALK_IN);

  // 2) 인사 + Hi!
  unsigned long t0 = millis();
  while (millis() - t0 < T_WAVE) {
    int ph = ((millis() - t0) / 150) % 2;        // 앞발 올렸다 내렸다
    oled.clearDisplay();
    const uint8_t* b = ph ? CAT_GREET1 : CAT_GREET0;
    drawCat(b, CAT_GREET1_W, CAT_GREET1_H, CAT_GREET1_AX, CAT_GREET1_AY, CENTER_X, CENTER_Y);
    drawHi();
    oled.display();
    delay(FRAME_MS);
  }

  // 3) 왼쪽으로 걸어가 앉기
  walkTo(CENTER_X, CENTER_Y, LEFT_X, LEFT_Y, T_WALK_LEFT);

  // 4) 이름 한 글자씩
  int total = strlen(OWNER_NAME);
  t0 = millis();
  for (;;) {
    float raw = (millis() - t0) / (float)T_NAME; if (raw > 1) raw = 1;
    int n = (int)lroundf(raw * total);
    oled.clearDisplay();
    drawCat(CAT_SIT, CAT_SIT_W, CAT_SIT_H, CAT_SIT_AX, CAT_SIT_AY, LEFT_X, LEFT_Y);
    drawName(n);
    oled.display();
    if (raw >= 1) break;
    delay(FRAME_MS);
  }

  // 5) QR 위→아래
  t0 = millis();
  for (;;) {
    float raw = (millis() - t0) / (float)T_QR; if (raw > 1) raw = 1;
    int rows = (int)lroundf(smooth(raw) * 64);
    oled.clearDisplay();
    drawCat(CAT_SIT, CAT_SIT_W, CAT_SIT_H, CAT_SIT_AX, CAT_SIT_AY, LEFT_X, LEFT_Y);
    drawName(total);
    drawQR(rows);
    oled.display();
    if (raw >= 1) break;
    delay(FRAME_MS);
  }
  Serial.println(F("등장 연출 완료 (다시 보려면 'r')"));
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED 초기화 실패")); return;
  }
  playIntro();            // 부팅 시 1회 재생
}

void loop() {
  if (Serial.available() && Serial.read() == 'r') playIntro();
}

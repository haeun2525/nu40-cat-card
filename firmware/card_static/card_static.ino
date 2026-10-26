/*
 * 단계 4: 정지 명함 화면 (이름 + 고양이 + 진짜 QR) — 실제 OLED
 *
 * 레이아웃 (128x64)
 *   왼쪽 64px : 위=이름(가운데 정렬), 아래=앉은 고양이
 *   오른쪽 64px: QR (흰 바탕 + 검은 점) — cat_qr.h (실제 LinkedIn URL)
 *
 * 비트맵은 PC 미리보기에서 그대로 내보낸 것:
 *   cat_bitmaps.h : 고양이 스프라이트(여기선 CAT_SIT 사용)
 *   cat_qr.h      : QR (1비트=흰 픽셀)
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "cat_bitmaps.h"
#include "cat_qr.h"

#define OWNER_NAME "Emily, Kim"   // 화면 이름(영문). 바꾸면 위치는 자동 가운데정렬.
#define OLED_ADDR  0x3C

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

void drawCard() {
  oled.clearDisplay();

  // --- 이름: 왼쪽 64px 영역 가운데 정렬 (기본 폰트 6px/글자) ---
  int nameW = (int)strlen(OWNER_NAME) * 6;
  int nameX = (64 - nameW) / 2;
  if (nameX < 0) nameX = 0;
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(nameX, 8);
  oled.print(OWNER_NAME);

  // --- 고양이: 왼쪽 영역 아래쪽에 앉힘 (미리보기와 동일 위치) ---
  // 폭32 → 가운데 x=(64-32)/2=16 근처, 바닥이 y=63 에 닿도록 top=27
  oled.drawBitmap(15, 27, CAT_SIT, CAT_SIT_W, CAT_SIT_H, SSD1306_WHITE);

  // --- QR: 오른쪽 64x64 영역. 1비트=흰 픽셀이라 WHITE 로 그리면 흰바탕+검은점 ---
  oled.drawBitmap(64, 0, QR_BMP, QR_W, QR_H, SSD1306_WHITE);

  oled.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();              // SDA26/SCL27
  Wire.setClock(400000);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED 초기화 실패"));
    return;
  }
  drawCard();
  Serial.println(F("명함 화면 표시됨"));
}

void loop() {
  // 정지 화면 — 아무것도 안 함
}

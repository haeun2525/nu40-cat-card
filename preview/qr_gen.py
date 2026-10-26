# -*- coding: utf-8 -*-
"""
단계 2: 진짜 QR 생성.

URL 을 받아 OLED 오른쪽 64px 영역에 들어갈 QR 비트맵을 미리 만든다.
  - 오류정정 레벨 L
  - 모듈 1칸 = 2px
  - OLED는 검은 바탕이라 그냥 그리면 반전 QR → 일부 폰이 못 읽음.
    그래서 "흰 바탕(=켜진 흰 픽셀) + 검은 점(=꺼진 픽셀)" 으로 만든다.
  - 흰 여백(quiet zone)을 64px 안에서 최대한 확보
  - 결과물:
      out/qr_scan.png : 폰으로 스캔 테스트용(큰 이미지, 표준 여백)  ← 지금 바로 테스트
      out/qr_oled.png : 실제 OLED에 찍힐 64x64 비트맵(확인용으로 확대본도)
      cat_qr.h        : 아두이노 drawBitmap 용 C 헤더 (1비트=흰 픽셀)

URL 만 바꿔서 다시 돌리면 재생성된다.
"""

import segno
from PIL import Image

# ===================== 설정 =====================
QR_URL = "https://www.linkedin.com/in/haeun-kim-634aa42b9"   # #define QR_URL
MODULE_PX = 2          # 모듈 1칸 = 2px
FIELD_PX = 64          # OLED 오른쪽 영역 폭(=QR 흰 바탕 한 변)
# ================================================


def build():
    qr = segno.make(QR_URL, error="l")       # 오류정정 L
    matrix = [list(row) for row in qr.matrix]  # 0/1 2차원 (여백 제외)
    n = len(matrix)                            # 한 변 모듈 수
    qr_px = n * MODULE_PX                       # 여백 제외 QR 픽셀 크기

    print(f"URL         : {QR_URL}  ({len(QR_URL)} bytes)")
    print(f"QR version  : {qr.version}  (모듈 {n}x{n})")
    print(f"QR 크기     : {qr_px}x{qr_px}px  (모듈 {MODULE_PX}px 기준)")

    # 64px 안에 센터링했을 때 양쪽 여백
    if qr_px > FIELD_PX:
        border = 0
        print(f"⚠️  경고: QR {qr_px}px 가 {FIELD_PX}px 영역을 넘습니다! "
              f"URL을 줄이거나 MODULE_PX=1 을 고려하세요. (일단 잘려서 생성)")
    else:
        border = (FIELD_PX - qr_px) // 2
        qz_modules = border / MODULE_PX
        print(f"흰 여백     : 양쪽 {border}px  (= {qz_modules:.1f} 모듈)")
        if qz_modules < 2:
            print(f"⚠️  참고: 여백이 권장(4모듈)보다 좁습니다. 흰 바탕이라 대개 읽히지만 "
                  f"폰 스캔 테스트 필수. 안 읽히면 URL 단축을 권장.")

    # --- OLED용 64x64 비트맵: 전부 흰색(1)에서 시작, QR 모듈만 검정(0) ---
    # (1 = 켜진 흰 픽셀 → OLED에서 빛남,  0 = 검정)
    oled = Image.new("1", (FIELD_PX, FIELD_PX), 1)  # 흰 바탕 + 여백
    px = oled.load()
    for my in range(n):
        for mx in range(n):
            if matrix[my][mx]:  # 검은 모듈
                for dy in range(MODULE_PX):
                    for dx in range(MODULE_PX):
                        x = border + mx * MODULE_PX + dx
                        y = border + my * MODULE_PX + dy
                        if 0 <= x < FIELD_PX and 0 <= y < FIELD_PX:
                            px[x, y] = 0   # 검정 점
    oled.save("out/qr_oled.png")
    # 확인용 확대본
    oled.resize((FIELD_PX * 6, FIELD_PX * 6), Image.NEAREST).save("out/qr_oled_big.png")

    # --- 폰 스캔 테스트용 큰 표준 QR (넉넉한 여백) ---
    qr.save("out/qr_scan.png", scale=14, border=4,
            dark="black", light="white")

    # --- 아두이노 C 헤더 (drawBitmap: 1비트 = 흰 픽셀) ---
    export_header(oled, "cat_qr.h")
    print("saved: out/qr_scan.png, out/qr_oled.png, cat_qr.h")
    return oled


def export_header(img, path):
    """64x64 1비트 이미지를 C 바이트 배열 헤더로.  MSB first, 1=흰 픽셀(그림)."""
    w, h = img.size
    px = img.load()
    row_bytes = (w + 7) // 8
    lines = []
    lines.append("// 자동 생성됨 (qr_gen.py). URL 바꾸면 다시 생성됨.")
    lines.append(f"// QR -> {QR_URL}")
    lines.append(f"// {w}x{h}, 1비트. 1=흰 픽셀(그림). Adafruit_GFX drawBitmap(x,y,QR_BMP,{w},{h},SSD1306_WHITE)")
    lines.append(f"#define QR_W {w}")
    lines.append(f"#define QR_H {h}")
    lines.append("static const unsigned char QR_BMP[] = {")
    for y in range(h):
        row = []
        for b in range(row_bytes):
            byte = 0
            for bit in range(8):
                x = b * 8 + bit
                if x < w and px[x, y]:        # 1 = 흰 픽셀
                    byte |= (0x80 >> bit)
            row.append(f"0x{byte:02X}")
        lines.append("  " + ", ".join(row) + ",")
    lines.append("};")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    import os
    os.makedirs("out", exist_ok=True)
    build()

# -*- coding: utf-8 -*-
"""
PC 미리보기의 고양이 스프라이트(cat_sprites.py)를 아두이노 C 헤더로 내보낸다.

Adafruit_GFX drawBitmap(x, y, bmp, w, h, color) 포맷:
  - 한 바이트 = 가로 8픽셀, MSB first
  - 한 행은 ceil(w/8) 바이트로 패딩
  - 비트 1 = 그 색으로 찍음.  여기선 1 = 흰 픽셀(고양이 몸).  color=WHITE 로 그리면 됨.

출력: firmware/card_static/cat_bitmaps.h  (전 프레임 + 크기 + 앵커)
"""
import os
import cat_sprites

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware", "card_static", "cat_bitmaps.h")


def to_bytes(img):
    w, h = img.size
    px = img.load()
    row_bytes = (w + 7) // 8
    data = []
    for y in range(h):
        for b in range(row_bytes):
            byte = 0
            for bit in range(8):
                x = b * 8 + bit
                if x < w and px[x, y]:  # 1 = 흰 픽셀
                    byte |= (0x80 >> bit)
            data.append(byte)
    return data, w, h


def emit(name, sprite, lines):
    img, ax, ay = sprite
    data, w, h = to_bytes(img)
    up = name.upper()
    lines.append(f"// {name}: {w}x{h}, anchor(bottom-center)=({ax},{ay})")
    lines.append(f"#define CAT_{up}_W {w}")
    lines.append(f"#define CAT_{up}_H {h}")
    lines.append(f"#define CAT_{up}_AX {ax}")
    lines.append(f"#define CAT_{up}_AY {ay}")
    lines.append(f"static const unsigned char CAT_{up}[] = {{")
    row_bytes = (w + 7) // 8
    for r in range(0, len(data), row_bytes):
        row = data[r:r + row_bytes]
        lines.append("  " + ", ".join(f"0x{v:02X}" for v in row) + ",")
    lines.append("};")
    lines.append("")


def main():
    frames = cat_sprites.all_frames()  # walk0,walk1,greet0,greet1,sit,blink,sleep
    lines = ["// 자동 생성됨 (export_sprites.py). 고양이 모양 바꾸면 다시 생성.",
             "#pragma once", ""]
    for name, sprite in frames.items():
        emit(name, sprite, lines)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("saved", OUT, f"({len(frames)} frames)")


if __name__ == "__main__":
    main()

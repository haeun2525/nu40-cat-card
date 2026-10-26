# -*- coding: utf-8 -*-
"""스프라이트 점검용 시트: 모든 포즈를 크게 확대해 한 장의 PNG로."""
from PIL import Image, ImageDraw
import cat_sprites

SCALE = 8
PAD = 16

frames = cat_sprites.all_frames()
cell_w = max(im.width for im, _, _ in frames.values())
cell_h = max(im.height for im, _, _ in frames.values())

cols = len(frames)
sheet = Image.new("RGB", (cols * (cell_w * SCALE + PAD) + PAD,
                          cell_h * SCALE + PAD * 3), (30, 30, 30))
draw = ImageDraw.Draw(sheet)

x = PAD
for name, (im, ax, ay) in frames.items():
    # 1비트 → 흰 픽셀만 보이게 확대(검정 배경)
    big = im.convert("L").resize((im.width * SCALE, im.height * SCALE), Image.NEAREST)
    box = Image.new("RGB", (cell_w * SCALE, cell_h * SCALE), (0, 0, 0))
    box.paste(big.convert("RGB"), (0, 0))
    sheet.paste(box, (x, PAD * 2))
    draw.text((x, PAD // 2), f"{name} {im.width}x{im.height}", fill=(200, 200, 200))
    x += cell_w * SCALE + PAD

sheet.save("out/sprite_sheet.png")
print("saved out/sprite_sheet.png", sheet.size)

# -*- coding: utf-8 -*-
"""
단계 1: PC 미리보기.

128x64 OLED를 흉내 내어, 흔들어서 켜진 뒤의 '등장 연출 → 명함 표시 → 잠들기 → 꺼짐'
전체 흐름을 GIF 와 스틸 PNG 로 뽑는다.  실제 보드 없이 모양·레이아웃·타이밍을 확정하는 용도.

모든 타이밍/좌표는 이 파일 맨 위 CONFIG 한 곳에 모아 두었다.
아두이노 쪽 #define 과 1:1 로 대응시키려고 이름도 비슷하게 맞췄다.
"""

import os
from PIL import Image, ImageDraw, ImageFont
import cat_sprites

# ===================== CONFIG (여기 값만 바꾸면 됨) =====================
OWNER_NAME = "Emily, Kim"          # #define OWNER_NAME
SCREEN_W, SCREEN_H = 128, 64
SCALE = 6                          # 미리보기 확대 배율
FPS = 25
MS = int(1000 / FPS)

# 연출 구간 길이(초). 아두이노 타이밍 설정값과 대응.
T_WALK_IN   = 1.5   # 0.0~1.5  오른쪽 아래에서 걸어 들어와 중앙에서 멈춤
T_WAVE      = 1.0   # 1.5~2.5  앞발 들어 인사 + "Hi!"
T_WALK_LEFT = 1.0   # 2.5~3.5  왼쪽으로 걸어가 앉음
T_NAME      = 0.5   # 3.5~4.0  이름 한 글자씩
T_QR        = 0.5   # 4.0~4.5  QR 위→아래로
T_IDLE      = 2.5   # 명함 표시(눈 깜빡임 포함). 실제로는 SHOW_TIMEOUT_MS=30초
T_SLEEP     = 1.0   # 엎드려 잠드는 연출
T_OFF       = 0.5   # 화면 꺼짐(검정)

# 위치: 앵커(바닥 중앙) 기준 (x, y)
POS_ENTER  = (140, 60)   # 화면 밖 오른쪽 아래
POS_CENTER = (64, 61)    # 화면 가운데에서 멈춤
POS_LEFT   = (31, 61)    # 왼쪽 영역에 앉는 자리

# 오른쪽 QR 영역: qr_gen.py 가 만든 64x64 비트맵을 오른쪽 절반에 그대로.
QR_ORIGIN = (64, 0)   # (x, y) 오른쪽 64px 영역 좌상단
# ======================================================================

FONT = ImageFont.load_default()   # 미리보기용(실제 OLED는 6x8 폰트). 대략 비슷.


def seconds(t):
    """초 → 프레임 수."""
    return max(1, int(round(t * FPS)))


def smoothstep(p):
    """0~1 을 부드럽게(처음·끝 느리게)."""
    p = max(0.0, min(1.0, p))
    return p * p * (3 - 2 * p)


def lerp(a, b, p):
    return a + (b - a) * p


def blit(dst, sprite, anchor_x, anchor_y):
    """스프라이트(이미지, ax, ay)를 앵커가 (anchor_x, anchor_y)에 오도록 얹는다."""
    im, ax, ay = sprite
    x = int(round(anchor_x - ax))
    y = int(round(anchor_y - ay))
    # "1" 이미지를 마스크로 써서 켜진 픽셀만 흰색으로 찍는다.
    dst.paste(1, (x, y), im)


def load_qr():
    """qr_gen.py 가 만든 진짜 QR 비트맵을 불러온다. 없으면 안내 후 흰 박스."""
    path = "out/qr_oled.png"
    if os.path.exists(path):
        return Image.open(path).convert("1")
    print("⚠️  out/qr_oled.png not found -> run 'python3 qr_gen.py' first. (Using a blank box for now)")
    im = Image.new("1", (64, 64), 1)
    return im


QR_IMG = load_qr()


def text_width(s):
    b = FONT.getbbox(s)
    return b[2] - b[0]


def draw_name(dst, reveal_chars):
    """이름을 왼쪽 영역 가운데 정렬로, 앞에서부터 reveal_chars 글자만."""
    full_w = text_width(OWNER_NAME)
    x = 32 - full_w // 2            # 왼쪽 64px 영역의 중앙(=32) 기준
    if x < 1:
        x = 1
    shown = OWNER_NAME[:reveal_chars]
    d = ImageDraw.Draw(dst)
    d.text((x, 8), shown, fill=1, font=FONT)


def draw_qr(dst, reveal_rows):
    """QR 를 위에서부터 reveal_rows 픽셀만큼만 내려 보여준다."""
    if reveal_rows <= 0:
        return
    rows = min(reveal_rows, QR_IMG.height)
    crop = QR_IMG.crop((0, 0, QR_IMG.width, rows))
    dst.paste(crop, QR_ORIGIN)


def draw_hi_bubble(dst):
    """고양이 머리 오른쪽에 'Hi!' 말풍선."""
    d = ImageDraw.Draw(dst)
    bx0, by0, bx1, by1 = 78, 18, 104, 30
    d.rounded_rectangle([bx0, by0, bx1, by1], radius=3, fill=1)
    d.rounded_rectangle([bx0, by0, bx1, by1], radius=3, outline=0)
    d.polygon([(bx0 + 2, by1 - 2), (bx0 - 4, by1 + 4), (bx0 + 7, by1)], fill=1)  # 꼬리
    d.text((bx0 + 4, by0 + 2), "Hi!", fill=0, font=FONT)


def new_screen():
    return Image.new("1", (SCREEN_W, SCREEN_H), 0)  # 전부 꺼짐(검정)


def scale_rgb(img):
    """1비트 → 확대 RGB(흰 픽셀만 보이게). GIF/PNG 저장용."""
    big = img.convert("L").resize((SCREEN_W * SCALE, SCREEN_H * SCALE), Image.NEAREST)
    return big.convert("RGB")


# ----------------------- 프레임 생성 -----------------------
frames = []           # (초단위길이 상관없이) 스크린 이미지 모음
stills = {}           # 단계별 스틸 저장


def phase_walk(a, b, n, store_mid=None):
    """a→b 로 걸어 이동. 옆모습 걷기 프레임 교대."""
    for i in range(n):
        p = smoothstep(i / max(1, n - 1))
        x = lerp(a[0], b[0], p)
        y = lerp(a[1], b[1], p)
        scr = new_screen()
        step = (i // 4) % 2                     # 4프레임마다 다리 교대
        blit(scr, cat_sprites.walk(step), x, y)
        frames.append(scr)
        if store_mid is not None and i == n // 2:
            stills[store_mid] = scr.copy()


def phase_wave(n):
    """중앙에서 앞발 들어 흔들기 + Hi! 말풍선."""
    import math
    for i in range(n):
        scr = new_screen()
        # 손 흔들기: 0.5~1.0 사이를 왕복
        wave = 0.75 + 0.25 * math.sin(i / FPS * 2 * math.pi * 2)  # 2Hz
        blit(scr, cat_sprites.greet(wave), *POS_CENTER)
        draw_hi_bubble(scr)
        frames.append(scr)
        if i == n // 2:
            stills["wave"] = scr.copy()


def phase_name(n):
    """앉은 상태에서 이름 한 글자씩."""
    total = len(OWNER_NAME)
    for i in range(n):
        p = (i + 1) / n
        chars = int(round(p * total))
        scr = new_screen()
        blit(scr, cat_sprites.sit("open"), *POS_LEFT)
        draw_name(scr, chars)
        frames.append(scr)


def phase_qr(n):
    """이름 유지 + QR 위→아래."""
    for i in range(n):
        p = smoothstep((i + 1) / n)
        rows = int(round(p * QR_IMG.height))
        scr = new_screen()
        blit(scr, cat_sprites.sit("open"), *POS_LEFT)
        draw_name(scr, len(OWNER_NAME))
        draw_qr(scr, rows)
        frames.append(scr)
    stills["card"] = frames[-1].copy()   # 완성된 명함 스틸


def full_card(eye="open"):
    scr = new_screen()
    blit(scr, cat_sprites.sit(eye), *POS_LEFT)
    draw_name(scr, len(OWNER_NAME))
    draw_qr(scr, QR_IMG.height)
    return scr


def phase_idle(n):
    """명함 표시: 가끔 눈 깜빡임."""
    blink_at = {int(n * 0.35), int(n * 0.36), int(n * 0.7), int(n * 0.71)}
    for i in range(n):
        eye = "blink" if i in blink_at else "open"
        scr = full_card(eye)
        frames.append(scr)
        if i == int(n * 0.35):
            stills["blink"] = scr.copy()


def phase_sleep(n):
    """앉기 → 엎드려 잠듦(간단히 sit→sleep 전환)."""
    for i in range(n):
        scr = new_screen()
        if i < n // 3:
            blit(scr, cat_sprites.blink(), *POS_LEFT)       # 눈 감기
            draw_name(scr, len(OWNER_NAME))
            draw_qr(scr, QR_IMG.height)
        else:
            blit(scr, cat_sprites.sleep(), POS_LEFT[0] + 2, 62)  # 엎드림
        frames.append(scr)
        if i == n - 1:
            stills["sleep"] = scr.copy()


def phase_off(n):
    for _ in range(n):
        frames.append(new_screen())   # 완전 검정 = OLED off


# ----------------------- 타임라인 실행 -----------------------
phase_walk(POS_ENTER, POS_CENTER, seconds(T_WALK_IN), store_mid="walkin")
phase_wave(seconds(T_WAVE))
phase_walk(POS_CENTER, POS_LEFT, seconds(T_WALK_LEFT))
phase_name(seconds(T_NAME))
phase_qr(seconds(T_QR))
phase_idle(seconds(T_IDLE))
phase_sleep(seconds(T_SLEEP))
phase_off(seconds(T_OFF))

# ----------------------- 저장 -----------------------
os.makedirs("out", exist_ok=True)

gif_frames = [scale_rgb(f) for f in frames]
gif_frames[0].save("out/demo.gif", save_all=True, append_images=gif_frames[1:],
                   duration=MS, loop=0, optimize=True)
print(f"saved out/demo.gif  ({len(frames)} frames, {len(frames)/FPS:.1f}s)")

# 단계별 스틸 → 한 장의 스트립으로
order = ["walkin", "wave", "card", "blink", "sleep"]
labels = {"walkin": "walk-in", "wave": "wave + Hi!", "card": "FINAL CARD",
          "blink": "blink", "sleep": "sleep"}
pad = 12
cells = [scale_rgb(stills[k]) for k in order if k in stills]
cw, ch = cells[0].size
strip = Image.new("RGB", (cw + pad * 2, (ch + pad * 2) * len(cells)), (30, 30, 30))
dd = ImageDraw.Draw(strip)
for i, k in enumerate([k for k in order if k in stills]):
    y = i * (ch + pad * 2) + pad
    strip.paste(cells[i], (pad, y))
    dd.text((pad, y - 10), labels[k], fill=(200, 200, 200))
strip.save("out/stages.png")
print("saved out/stages.png", strip.size)

# 완성 명함 단독 스틸
scale_rgb(stills["card"]).save("out/card.png")
print("saved out/card.png")

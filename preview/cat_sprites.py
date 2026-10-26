# -*- coding: utf-8 -*-
"""
오리지널 1비트 픽셀 고양이 스프라이트 정의.

- 모든 그림은 "1" 모드(흑백 1비트) PIL 이미지로 그린다. (OLED와 동일한 1비트)
- 켜진 픽셀 = 흰색(값 1), 꺼진 픽셀 = 검정(값 0).  OLED에서는 흰색이 '빛나는' 픽셀.
- 각 스프라이트는 (이미지, 앵커x, 앵커y) 로 돌려준다.
  앵커 = 바닥 중앙. 화면에 놓을 때 '발이 닿는 지점'을 기준으로 배치하면 편하다.
- 캐릭터는 기존 캐릭터를 따라 하지 않은 둥근얼굴·큰눈·삼각귀·꼬리 고양이다.

실제 아두이노용 비트맵은 나중에 이 모양을 그대로 바이트 배열로 뽑아 쓸 것이다.
여기서는 '모양을 확정'하는 단계이므로 사람이 눈으로 보고 수정하기 쉽게 도형으로 그린다.
"""

from PIL import Image, ImageDraw


def _canvas(w, h):
    """빈 1비트 캔버스(전부 꺼짐)와 그리기 핸들을 만든다."""
    img = Image.new("1", (w, h), 0)
    return img, ImageDraw.Draw(img)


# ----------------------------------------------------------------------------
# 얼굴 부품(눈/코/입)을 흰 몸통 위에 '꺼진 픽셀(검정)'로 찍어 표정을 만든다.
# eye_state: "open"(동그란 눈) / "blink"(감은 눈=가로줄)
# ----------------------------------------------------------------------------
def _draw_face(d, cx, cy, eye_state="open"):
    # 두 눈의 중심 x 좌표 (얼굴 중앙 cx 기준 좌우로 벌림)
    lx, rx = cx - 5, cx + 5
    ey = cy
    if eye_state == "open":
        # 큰 눈: 검정 타원 + 가운데 작은 흰 반짝임
        for ex in (lx, rx):
            d.ellipse([ex - 2, ey - 3, ex + 2, ey + 3], fill=0)  # 눈(검정)
            d.point((ex + 1, ey - 1), fill=1)                     # 반짝임(흰)
    else:  # blink : 눈을 감아 가로줄로
        for ex in (lx, rx):
            d.line([ex - 2, ey, ex + 2, ey], fill=0)

    # 코: 작은 검정 점 (두 눈 사이 아래)
    ny = cy + 4
    d.point((cx, ny), fill=0)
    d.point((cx, ny + 1), fill=0)
    # 입: 고양이 'ω' 느낌으로 양쪽 아래로 짧게
    d.line([cx, ny + 1, cx - 2, ny + 3], fill=0)
    d.line([cx, ny + 1, cx + 2, ny + 3], fill=0)


def _draw_ears(d, cx, top):
    """머리 위 삼각귀 두 개(흰색 채움). top = 귀 밑변이 닿는 y."""
    # 왼쪽 귀
    d.polygon([(cx - 9, top + 2), (cx - 6, top - 7), (cx - 1, top)], fill=1)
    # 오른쪽 귀
    d.polygon([(cx + 9, top + 2), (cx + 6, top - 7), (cx + 1, top)], fill=1)


def sit(eye_state="open"):
    """앉아 있는 정면 고양이. (명함 표시 상태의 기본 포즈)"""
    W, H = 32, 36
    img, d = _canvas(W, H)
    cx = W // 2  # 16

    # 꼬리: 몸 오른쪽을 감싸 올라오는 흰 곡선(두껍게)
    d.arc([cx + 2, 16, cx + 16, 34], start=-70, end=120, fill=1, width=3)

    # 몸통: 아래가 넓은 앉은 자세(흰색 채움)
    d.polygon([(cx - 9, 34), (cx + 9, 34), (cx + 7, 20), (cx - 7, 20)], fill=1)
    d.ellipse([cx - 10, 24, cx + 10, 35], fill=1)   # 엉덩이 둥글게
    # 앞발 두 개(바닥 중앙)
    d.ellipse([cx - 6, 30, cx - 1, 35], fill=1)
    d.ellipse([cx + 1, 30, cx + 6, 35], fill=1)
    d.line([cx, 31, cx, 35], fill=0)                # 두 발 사이 틈(검정)

    # 귀 + 머리
    _draw_ears(d, cx, top=5)
    d.ellipse([cx - 10, 3, cx + 10, 21], fill=1)    # 둥근 얼굴

    # 표정
    _draw_face(d, cx, cy=11, eye_state=eye_state)
    return img, cx, H - 2  # 앵커: 바닥 중앙


def blink():
    """앉은 채 눈 감은 프레임."""
    return sit(eye_state="blink")


def greet(raise_level=1):
    """정면 인사 포즈. raise_level 0→1 로 오른 앞발이 올라간다."""
    img, ax, ay = sit(eye_state="open")
    d = ImageDraw.Draw(img)
    cx = 16
    # 오른쪽 앞발을 머리 옆까지 들어 올림(흔드는 손)
    # raise_level 0: 가슴 근처 / 1: 머리 옆
    paw_y = int(22 - 12 * raise_level)
    d.line([cx + 6, 24, cx + 11, paw_y + 3], fill=1, width=3)   # 팔
    d.ellipse([cx + 9, paw_y, cx + 14, paw_y + 5], fill=1)      # 손(발)
    return img, ax, ay


def _draw_walk(step):
    """옆모습 걷기. step 0/1 로 다리 위치가 바뀐다. 왼쪽을 보고 걷는다."""
    W, H = 36, 26
    img, d = _canvas(W, H)

    # 꼬리: 뒤(오른쪽)로 올라간 흰 곡선
    d.arc([24, 2, 36, 20], start=-120, end=60, fill=1, width=3)

    # 몸통(가로 타원)
    d.ellipse([6, 6, 30, 19], fill=1)
    # 머리(왼쪽 앞)
    d.ellipse([0, 4, 13, 17], fill=1)
    # 귀 하나(옆모습)
    d.polygon([(2, 5), (4, -1), (8, 4)], fill=1)
    # 눈 하나(검정 점) + 코
    d.ellipse([3, 8, 5, 11], fill=0)
    d.point((0, 11), fill=0)

    # 다리 4개: step 에 따라 앞/뒤로 엇갈리게
    if step == 0:
        legs = [(10, -2), (16, 2), (22, -2), (28, 2)]
    else:
        legs = [(10, 2), (16, -2), (22, 2), (28, -2)]
    for lx, dx in legs:
        d.line([lx, 18, lx + dx, 24], fill=1, width=2)
        d.point((lx + dx, 24), fill=1)  # 발끝
    return img, 18, H - 2  # 앵커: 바닥 중앙


def walk(step):
    return _draw_walk(step)


def sleep():
    """엎드려 자는 옆모습. 머리를 오른쪽에 뉘였다."""
    W, H = 38, 20
    img, d = _canvas(W, H)
    # 납작한 몸통
    d.ellipse([1, 8, 30, 18], fill=1)
    # 머리(오른쪽에 기대어)
    d.ellipse([22, 3, 36, 16], fill=1)
    # 귀
    d.polygon([(33, 4), (37, -1), (31, 2)], fill=1)
    # 감은 눈(검정 가로줄)
    d.line([26, 9, 30, 9], fill=0)
    # 꼬리를 몸에 붙여 둥글게
    d.arc([0, 4, 14, 18], start=90, end=260, fill=1, width=3)
    # Zzz (흰색, 머리 위)
    d.text((30, 0), "z", fill=1)
    return img, 19, H - 2


def all_frames():
    """확인용: 모든 포즈를 한 번에 돌려준다."""
    return {
        "walk0": walk(0),
        "walk1": walk(1),
        "greet0": greet(0.0),
        "greet1": greet(1.0),
        "sit": sit("open"),
        "blink": blink(),
        "sleep": sleep(),
    }

#!/usr/bin/env python3
"""Draws the Horník (miner), Rytíř (knight) and Robot player skins.

Writes 64x64 PNG skins in the usual layout (the one engine/Skin.h describes) to
assets_src/skins/ and a 3D preview to docs/img/skins_preview.png.
Everything is drawn here pixel by pixel, nothing is taken from other games.

    python3 tools/player_skins.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets_src", "skins")

# Parts: (u, v, w, h, d) as in engine/Skin.h.
HEAD, HAT = (0, 0, 8, 8, 8), (32, 0, 8, 8, 8)
BODY, JACKET = (16, 16, 8, 12, 4), (16, 32, 8, 12, 4)
R_ARM, R_SLEEVE = (40, 16, 4, 12, 4), (40, 32, 4, 12, 4)
L_ARM, L_SLEEVE = (32, 48, 4, 12, 4), (48, 48, 4, 12, 4)
R_LEG, R_PANTS = (0, 16, 4, 12, 4), (0, 32, 4, 12, 4)
L_LEG, L_PANTS = (16, 48, 4, 12, 4), (0, 48, 4, 12, 4)
SIDES = ("right", "front", "left", "back")


def face_rect(part, face):
    u, v, w, h, d = part
    return {
        "top": (u + d, v, w, d),
        "bottom": (u + d + w, v, w, d),
        "right": (u, v + d, d, h),
        "front": (u + d, v + d, w, h),
        "left": (u + d + w, v + d, d, h),
        "back": (u + 2 * d + w, v + d, w, h),
    }[face]


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def shade(c, f):
    return tuple(max(0, min(255, int(round(x * f)))) for x in c[:3])


class Skin:
    def __init__(self, seed):
        self.img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        self.px = self.img.load()
        self.rng = random.Random(seed)

    def paint(self, part, fn, grain=6):
        """Calls fn(face, x, y, w, h) for every pixel of every face of a part.
        fn returns an RGB(A) colour or None (transparent)."""
        for face in ("top", "bottom") + SIDES:
            fx, fy, fw, fh = face_rect(part, face)
            for y in range(fh):
                for x in range(fw):
                    c = fn(face, x, y, fw, fh)
                    if c is None:
                        continue
                    a = c[3] if len(c) > 3 else 255
                    n = self.rng.randint(-grain, grain) if grain else 0
                    self.px[fx + x, fy + y] = tuple(max(0, min(255, c[i] + n)) for i in range(3)) + (a,)

    def save(self, name):
        path = os.path.join(OUT, name + ".png")
        self.img.save(path)
        return path


# ---------------------------------------------------------------- Horník ---

def miner():
    s = Skin(21)
    skin, skin_dk = (196, 140, 102), (170, 116, 82)
    hair, stubble = (58, 40, 26), (120, 84, 60)
    shirt, shirt_dk = (62, 70, 92), (46, 52, 70)
    vest, vest_dk = (240, 122, 28), (200, 96, 20)
    reflect = (214, 220, 226)
    pants, pants_dk = (58, 60, 68), (42, 44, 50)
    boot, boot_dk, sole = (104, 70, 40), (80, 52, 30), (34, 30, 28)
    glove, glove_dk = (150, 104, 58), (118, 80, 42)
    dirt = (92, 74, 58)

    def head(face, x, y, w, h):
        if face == "top":
            return hair
        if face == "bottom":
            return skin_dk
        if face == "back":
            return hair if y < 7 else skin_dk
        if face in ("right", "left"):
            if y < 2 or (y < 5 and ((face == "right" and x < 5) or (face == "left" and x > 2))):
                return hair
            if y >= 5 and ((face == "right" and x == 7) or (face == "left" and x == 0)):
                return stubble  # sideburn into the beard
            return skin
        # front
        if y == 0 or y == 1:
            return hair if y == 0 or x in (0, 7) else skin
        if y == 3 and x in (1, 2, 5, 6):
            return (52, 36, 24)  # eyebrows
        if y == 4:
            return {1: (238, 238, 232), 2: (60, 90, 60), 5: (60, 90, 60), 6: (238, 238, 232)}.get(x, skin)
        if y == 5:
            if x in (3, 4):
                return skin_dk  # nose
            if x == 6:
                return dirt  # coal smudge
            return skin
        if y == 6:
            if x in (2, 3, 4, 5):
                return (86, 58, 40)  # moustache
            return stubble
        if y == 7:
            return (150, 96, 80) if x in (3, 4) else stubble
        return skin

    s.paint(HEAD, head)

    hat_y, hat_dk, hat_lt = (246, 196, 30), (206, 156, 16), (255, 226, 96)

    def helmet(face, x, y, w, h):
        if face == "top":
            if x in (3, 4):
                return hat_lt  # ridge
            return hat_y
        if face == "bottom":
            return None
        if y > 2:
            return None
        if y == 2:
            return hat_dk  # brim
        if face == "front":
            if x in (2, 5):
                return (70, 72, 78)  # lamp housing
            if x in (3, 4):
                return (255, 252, 214) if y == 1 else (255, 244, 150)
        if face == "back" and y == 1 and x in (2, 3, 4, 5):
            return (40, 40, 44)  # strap with the lamp battery
        return hat_y if y == 0 else mix(hat_y, hat_dk, 0.4)

    s.paint(HAT, helmet, grain=4)

    def body(face, x, y, w, h):
        if face == "top":
            return vest if x in (0, 1, 6, 7) else shirt
        if face == "bottom":
            return pants
        if y == 9:
            if face == "front" and x in (3, 4):
                return (200, 170, 60)  # buckle
            return (74, 50, 30)  # belt
        if y >= 10:
            return pants
        if face == "front":
            if x in (3, 4):
                return shirt_dk if y > 0 else shirt  # open vest
            if y in (6, 7):
                return reflect
            if x in (5, 6) and y in (2, 3):
                return vest_dk if y == 2 else mix(vest_dk, vest, 0.4)  # pocket
            if x == 5 and y == 1:
                return (40, 40, 40)  # pencil in the pocket
            if y == 8 and x in (0, 7):
                return vest_dk
            return vest
        if face == "back":
            if y in (6, 7):
                return reflect
            if y in (2, 3) and 1 <= x <= 6:
                return (40, 40, 44) if y == 2 else vest_dk  # "VC" panel line
            return vest
        # sides
        if y in (6, 7):
            return reflect
        return vest_dk if x in (0, 3) else vest

    s.paint(BODY, body)

    def arm(face, x, y, w, h):
        if face == "top":
            return shirt
        if face == "bottom":
            return glove_dk
        if y < 6:
            return shirt if y != 5 else shirt_dk
        if y == 6:
            return mix(shirt, (255, 255, 255), 0.15)  # rolled up cuff
        if y in (7, 8):
            if y == 8 and face == "front" and x == 1:
                return dirt
            return skin
        if y == 9:
            return glove_dk
        return glove

    s.paint(R_ARM, arm)
    s.paint(L_ARM, arm)

    def leg(face, x, y, w, h):
        if face == "top":
            return pants
        if face == "bottom":
            return sole
        if y == 11:
            return sole
        if y >= 8:
            if face == "front" and y == 10:
                return (130, 134, 140)  # steel toe cap
            if face == "front" and y == 8 and x in (1, 2):
                return (210, 200, 170)  # laces
            return boot if y > 8 else boot_dk
        if face == "front" and y in (4, 5):
            return (36, 36, 40) if y == 4 else (54, 54, 58)  # knee pad
        if face in ("right", "left") and x in (1, 2) and y in (1, 2):
            return pants_dk  # side pocket
        c = pants
        if s.rng.random() < 0.08:
            c = dirt
        return c

    s.paint(R_LEG, leg)
    s.paint(L_LEG, leg)
    return s


# ----------------------------------------------------------------- Rytíř ---

def knight():
    s = Skin(42)
    steel, steel_lt, steel_dk = (176, 182, 192), (214, 220, 228), (112, 118, 130)
    black = (18, 18, 24)
    gold, gold_dk = (232, 188, 62), (176, 132, 34)
    blue, blue_dk = (38, 62, 150), (26, 42, 104)
    mail, mail_dk = (130, 134, 140), (86, 90, 98)
    leather = (92, 58, 34)

    def steel_at(face, x, y, w, h):
        # Light from above, rounded edges a bit darker.
        c = mix(steel_lt, steel, min(1.0, y / max(1, h - 1) * 1.4))
        if face in ("front", "back") and x in (0, w - 1):
            c = shade(c, 0.86)
        if face in ("right", "left"):
            c = shade(c, 0.92)
        return c

    def chainmail(x, y):
        return mail if (x + y) % 2 == 0 else mail_dk

    def helmet(face, x, y, w, h):
        if face == "top":
            return steel_lt if x in (3, 4) else steel
        if face == "bottom":
            return steel_dk
        if y == 7:
            return gold_dk if face == "front" else steel_dk  # rim of the gorget
        if face == "front":
            if y == 3 and 1 <= x <= 6:
                return black  # visor slit
            if y == 2 and 1 <= x <= 6:
                return steel_dk
            if (x, y) in ((1, 5), (2, 6), (5, 6), (6, 5)):
                return black  # breathing holes
            if x in (3, 4) and y != 3:
                return steel_lt  # central ridge
        if face in ("right", "left") and y == 3 and x in (3, 4):
            return gold  # visor hinge
        if face == "back" and x in (3, 4) and y < 7:
            return steel_dk
        return steel_at(face, x, y, w, h)

    s.paint(HEAD, helmet, grain=3)

    red, red_dk = (196, 34, 40), (140, 20, 28)

    def plume(face, x, y, w, h):
        if face == "top":
            return red if x in (3, 4) else None
        if face == "back" and x in (3, 4) and y < 4:
            return red_dk if y == 3 else red
        if face == "front" and x in (3, 4) and y == 0:
            return red_dk
        return None

    s.paint(HAT, plume, grain=8)

    def body(face, x, y, w, h):
        if face == "top":
            return steel
        if face == "bottom":
            return mail_dk
        if y == 9:
            if face == "front" and x in (3, 4):
                return gold
            return leather
        if y >= 10:
            return chainmail(x, y)
        if face in ("front", "back"):
            if y < 1 or x in (0, w - 1):
                return steel_at(face, x, y, w, h)
            # tabard with a gold hem at the top
            if y == 1:
                return gold_dk if face == "back" else gold
            if face == "front":
                if x in (3, 4) and 2 <= y <= 7:
                    return gold if y != 7 else gold_dk  # cross
                if y == 4 and x in (2, 5):
                    return gold
            return blue if y < 8 else blue_dk
        return steel_at(face, x, y, w, h)

    s.paint(BODY, body, grain=3)

    def arm(face, x, y, w, h):
        if face == "top":
            return steel_lt
        if face == "bottom":
            return steel_dk
        if y < 3:
            if y == 2 and face in ("front", "right", "left") and x in (1, 2) and w == 4:
                return gold if y == 2 and x == 1 else steel_dk
            return mix(steel_lt, steel, y / 3)  # pauldron
        if y == 3:
            return steel_dk
        if y in (4, 5):
            return chainmail(x, y)
        if y in (6, 7, 8):
            return steel_at(face, x, y - 6, w, 4)  # vambrace
        if y == 9:
            return steel_dk
        # gauntlet
        if face == "front" and y == 10:
            return steel_lt if x % 2 == 0 else steel
        return mix(steel, steel_dk, 0.4)

    s.paint(R_ARM, arm, grain=3)
    s.paint(L_ARM, arm, grain=3)

    def leg(face, x, y, w, h):
        if face == "top":
            return mail
        if face == "bottom":
            return steel_dk
        if y < 2:
            return chainmail(x, y)
        if y == 5:
            if face == "front" and x in (1, 2):
                return gold if x == 1 else steel_lt  # knee cop rivet
            return steel_lt
        if y in (2, 3, 4):
            return steel_at(face, x, y - 2, w, 4)
        if y >= 10:
            if face == "front" and y == 11:
                return steel_dk
            return mix(steel, steel_dk, 0.5) if y == 10 else steel_dk  # sabaton
        return steel_at(face, x, y - 6, w, 4)  # greave

    s.paint(R_LEG, leg, grain=3)
    s.paint(L_LEG, leg, grain=3)
    return s


# ----------------------------------------------------------------- Robot ---

def robot():
    s = Skin(7)
    metal, metal_lt, metal_dk = (156, 170, 186), (196, 208, 220), (98, 108, 124)
    seam = (64, 72, 86)
    dark = (30, 34, 44)
    glow, glow_lt = (60, 230, 250), (200, 255, 255)
    orange = (246, 140, 34)
    green = (80, 230, 110)

    def plate(face, x, y, w, h):
        c = mix(metal_lt, metal, min(1.0, y / max(1, h - 1) * 1.6))
        if face in ("front", "back") and x in (0, w - 1):
            c = shade(c, 0.9)
        if face in ("right", "left"):
            c = shade(c, 0.93)
        return c

    def head(face, x, y, w, h):
        if face == "top":
            if x in (3, 4) and y in (3, 4):
                return (230, 60, 50) if (x + y) % 2 else (255, 120, 100)  # antenna light
            if y == 1 or y == 6:
                return seam
            return metal_lt
        if face == "bottom":
            return metal_dk
        if face == "front":
            if 2 <= y <= 4 and 1 <= x <= 6:
                if y == 3 and x in (1, 2, 5, 6):
                    return glow_lt if x in (2, 5) else glow  # eyes
                return dark  # visor
            if y == 6 and 2 <= x <= 5:
                return dark if x % 2 == 0 else metal_dk  # speaker grille
            if (x, y) in ((0, 0), (7, 0), (0, 7), (7, 7)):
                return metal_dk  # rivets
        if face in ("right", "left"):
            if x in (3, 4) and y in (3, 4):
                return orange if (x + y) % 2 else shade(orange, 0.8)  # ear bolt
            if y == 1:
                return seam
        if face == "back":
            if y == 1 or (x in (1, 6) and y > 1):
                return seam
            if y == 5 and x in (3, 4):
                return green
        if y == 7:
            return metal_dk
        return plate(face, x, y, w, h)

    s.paint(HEAD, head, grain=2)

    def body(face, x, y, w, h):
        if face == "top":
            return orange if x in (0, 7) else metal_lt
        if face == "bottom":
            return metal_dk
        if face == "front":
            if 2 <= y <= 6 and 2 <= x <= 5:
                if y in (2, 6) or x in (2, 5):
                    return seam  # screen frame
                return glow if (x, y) in ((3, 4), (4, 3), (4, 5)) else (20, 60, 70)
            if y == 0 and x in (0, 1, 6, 7):
                return orange
            if y in (8, 9) and 1 <= x <= 6:
                return dark if (x % 2 == 1) else metal_dk  # vents
            if y == 11:
                return seam
        if face == "back":
            if 1 <= y <= 8 and 2 <= x <= 5:
                if y in (1, 8) or x in (2, 5):
                    return seam  # battery pack
                if y == 3 and x == 3:
                    return green
                if y == 3 and x == 4:
                    return (240, 220, 60)
                return metal_dk
            if y == 0 and x in (0, 1, 6, 7):
                return orange
        if face in ("right", "left") and y in (3, 7):
            return seam
        return plate(face, x, y, w, h)

    s.paint(BODY, body, grain=2)

    def arm(face, x, y, w, h):
        if face == "top":
            return orange
        if face == "bottom":
            return dark
        if y == 0:
            return orange
        if y in (1, 2, 3):
            return plate(face, x, y - 1, w, 3)
        if y in (4, 5):
            return dark if (y == 4) else metal_dk  # elbow joint
        if y in (6, 7, 8):
            if face == "front" and y == 7 and x in (1, 2):
                return glow
            return plate(face, x, y - 6, w, 3)
        if y == 9:
            return seam
        return dark if (x + y) % 2 == 0 else (54, 60, 72)  # gripper

    s.paint(R_ARM, arm, grain=2)
    s.paint(L_ARM, arm, grain=2)

    def leg(face, x, y, w, h):
        if face == "top":
            return metal_dk
        if face == "bottom":
            return dark
        if y in (0, 1, 2, 3):
            return plate(face, x, y, w, 4)
        if y in (4, 5):
            if face == "front" and y == 4 and x in (1, 2):
                return orange  # knee
            return dark if y == 5 else metal_dk
        if y in (6, 7, 8):
            return plate(face, x, y - 6, w, 3)
        if y == 9:
            return seam
        if face == "front" and y == 10:
            return glow if x in (1, 2) else dark
        return (44, 48, 58)  # heavy foot

    s.paint(R_LEG, leg, grain=2)
    s.paint(L_LEG, leg, grain=2)
    return s


# --------------------------------------------------------------- preview ---

# Model boxes: (texture part, overlay part, x0, y0, z0, w, h, d) in model pixels.
# x grows to the character's left, y up, z towards the front.
MODEL = [
    (HEAD, HAT, -4, 24, -4, 8, 8, 8),
    (BODY, JACKET, -4, 12, -2, 8, 12, 4),
    (R_ARM, R_SLEEVE, -8, 12, -2, 4, 12, 4),
    (L_ARM, L_SLEEVE, 4, 12, -2, 4, 12, 4),
    (R_LEG, R_PANTS, -4, 0, -2, 4, 12, 4),
    (L_LEG, L_PANTS, 0, 0, -2, 4, 12, 4),
]

FACE_LIGHT = {"top": 1.0, "front": 0.9, "back": 0.9, "left": 0.75, "right": 0.75}


def box_texels(part, x0, y0, z0, w, h, d):
    """Yields (face, normal, texture pixel, 4 corners) for each texel."""
    u, v = part[0], part[1]
    x1, y1, z1 = x0 + w, y0 + h, z0 + d
    for i in range(w):
        for j in range(h):
            yield "front", (0, 0, 1), (u + d + i, v + d + j), [
                (x0 + i, y1 - j, z1), (x0 + i + 1, y1 - j, z1), (x0 + i + 1, y1 - j - 1, z1), (x0 + i, y1 - j - 1, z1)]
            yield "back", (0, 0, -1), (u + 2 * d + w + i, v + d + j), [
                (x1 - i, y1 - j, z0), (x1 - i - 1, y1 - j, z0), (x1 - i - 1, y1 - j - 1, z0), (x1 - i, y1 - j - 1, z0)]
    for i in range(d):
        for j in range(h):
            yield "left", (1, 0, 0), (u + d + w + i, v + d + j), [
                (x1, y1 - j, z1 - i), (x1, y1 - j, z1 - i - 1), (x1, y1 - j - 1, z1 - i - 1), (x1, y1 - j - 1, z1 - i)]
            yield "right", (-1, 0, 0), (u + i, v + d + j), [
                (x0, y1 - j, z0 + i), (x0, y1 - j, z0 + i + 1), (x0, y1 - j - 1, z0 + i + 1), (x0, y1 - j - 1, z0 + i)]
    for i in range(w):
        for j in range(d):
            yield "top", (0, 1, 0), (u + d + i, v + j), [
                (x0 + i, y1, z0 + j), (x0 + i + 1, y1, z0 + j), (x0 + i + 1, y1, z0 + j + 1), (x0 + i, y1, z0 + j + 1)]


def render(img, yaw_deg, scale, ox, oy):
    yaw, pitch = math.radians(yaw_deg), math.radians(18)
    view = (math.sin(yaw) * math.cos(pitch), math.sin(pitch), math.cos(yaw) * math.cos(pitch))
    right = (math.cos(yaw), 0.0, -math.sin(yaw))
    up = (view[1] * right[2] - view[2] * right[1], view[2] * right[0] - view[0] * right[2],
          view[0] * right[1] - view[1] * right[0])
    dot = lambda a, b: a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
    quads = []
    for base, over, x0, y0, z0, w, h, d in MODEL:
        layers = [(base, x0, y0, z0, w, h, d)]
        e = 0.5 if base is not HEAD else 0.5
        layers.append((over, x0 - e, y0 - e, z0 - e, w + 2 * e, h + 2 * e, d + 2 * e))
        for li, (part, bx, by, bz, bw, bh, bd) in enumerate(layers):
            sx, sy, sz = bw / w, bh / h, bd / d
            for face, n, (tx, ty), corners in box_texels(part, 0, 0, 0, w, h, d):
                if dot(n, view) <= 0:
                    continue
                c = TEX.getpixel((tx, ty))
                if c[3] < 128:
                    continue
                pts = [(bx + p[0] * sx, by + p[1] * sy, bz + p[2] * sz) for p in corners]
                cx = sum(p[0] for p in pts) / 4
                cy = sum(p[1] for p in pts) / 4
                cz = sum(p[2] for p in pts) / 4
                depth = dot((cx, cy, cz), view)
                col = shade(c, FACE_LIGHT[face])
                scr = [(ox + dot(p, right) * scale, oy - dot(p, up) * scale) for p in pts]
                quads.append((depth, scr, col))
    quads.sort(key=lambda q: q[0])
    draw = ImageDraw.Draw(img)
    for _, scr, col in quads:
        draw.polygon(scr, fill=col, outline=col)


def preview(skins, path):
    global TEX
    scale = 9
    cell_w, cell_h = 360, 560
    img = Image.new("RGB", (cell_w * len(skins), cell_h), (36, 44, 40))
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 24)
    except OSError:
        font = ImageFont.load_default()
    for i, (title, skin) in enumerate(skins):
        TEX = skin.img
        cx = i * cell_w
        render(img, 30, scale, cx + 110, 330)   # front
        render(img, 210, scale, cx + 255, 330)  # back
        tw = draw.textlength(title, font=font)
        draw.text((cx + (cell_w - tw) / 2, 358), title, fill=(236, 236, 228), font=font)
        # the flat 64x64 texture under the name
        flat = skin.img.resize((128, 128), Image.NEAREST)
        draw.rectangle((cx + 116, 404, cx + 243, 531), fill=(54, 64, 58))
        img.paste(flat, (cx + 116, 404), flat)
    img.save(path)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    made = [("Horník", "miner", miner()), ("Rytíř", "knight", knight()), ("Robot", "robot", robot())]
    for title, name, skin in made:
        print(skin.save(name))
    p = os.path.join(ROOT, "docs", "img", "skins_preview.png")
    preview([(t, s) for t, _, s in made], p)
    print(p)

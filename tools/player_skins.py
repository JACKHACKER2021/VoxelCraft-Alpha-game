#!/usr/bin/env python3
"""Draws the Horník (miner), Rytíř (knight), Robot, Zahradnice, Astronautka and the two VIP player skins.

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


# --------------------------------------------------------------- VIP ---

def vip_face(skin, skin_dk, hair, eyes, brow, mouth):
    def head(face, x, y, w, h):
        if face == "top":
            return hair
        if face == "bottom":
            return skin_dk
        if face == "back":
            return hair if y < 6 else skin_dk
        if face in ("right", "left"):
            if y < 2 or (y < 4 and ((face == "right" and x < 6) or (face == "left" and x > 1))):
                return hair
            return skin
        if y == 0 or (y == 1 and x in (0, 7)):
            return hair
        if y == 3 and x in (1, 2, 5, 6):
            return brow
        if y == 4:
            return {1: (240, 240, 236), 2: eyes, 5: eyes, 6: (240, 240, 236)}.get(x, skin)
        if y == 5 and x in (3, 4):
            return skin_dk
        if y == 6 and x in (2, 3, 4, 5):
            return mouth(x)
        return skin
    return head


def vip_gold():
    s = Skin(88)
    skin, skin_dk = (220, 170, 128), (190, 140, 100)
    gold, gold_lt, gold_dk = (238, 192, 52), (255, 230, 120), (176, 128, 24)
    red, red_dk = (170, 24, 40), (120, 14, 28)
    ermine, spot = (246, 244, 238), (26, 26, 30)
    pants, boot = (92, 62, 24), (60, 36, 22)

    s.paint(HEAD, vip_face(skin, skin_dk, (86, 54, 24), (40, 90, 180), (70, 44, 20),
                           lambda x: (160, 96, 84) if x in (3, 4) else skin))

    def crown(face, x, y, w, h):
        if face in ("top", "bottom"):
            return None
        if y > 2:
            return None
        if y == 2:
            return gold_dk  # band
        if y == 1:
            if face == "front" and x in (3, 4):
                return red  # ruby
            if face in ("right", "left", "back") and x in (3, 4) and w == 8 or face in ("right", "left") and x in (3, 4):
                return (40, 110, 210)  # sapphires
            return gold
        return gold_lt if x % 2 == 0 else None  # spikes

    s.paint(HAT, crown, grain=4)

    def body(face, x, y, w, h):
        if face == "top":
            return ermine if (x + y) % 3 else spot
        if face == "bottom":
            return pants
        if y < 2:
            return spot if (x * 3 + y) % 5 == 0 else ermine  # ermine collar
        if face == "back":
            if x in (0, 7):
                return gold  # cape edge
            return red if y < 11 else red_dk  # royal cape
        if y == 9:
            return gold_lt if face == "front" and x in (3, 4) else gold_dk  # belt
        if y >= 10:
            return pants
        if face == "front":
            if x == y - 1 or x == y - 2:
                return red  # sash
            if x in (3, 4) and y in (3, 5, 7) and not (x == y - 1 or x == y - 2):
                return gold_lt  # buttons
            return gold if x not in (0, 7) else gold_dk
        return gold_dk if x in (0, 3) else gold

    s.paint(BODY, body, grain=4)

    def arm(face, x, y, w, h):
        if face == "top":
            return ermine
        if face == "bottom":
            return skin_dk
        if y < 2:
            return ermine if (x + y) % 4 else spot
        if y < 8:
            return gold if y != 7 else gold_dk
        if y == 8:
            return ermine  # cuff
        if y == 10 and face == "front" and x == 1:
            return red  # signet ring
        return skin

    s.paint(R_ARM, arm, grain=4)
    s.paint(L_ARM, arm, grain=4)

    def leg(face, x, y, w, h):
        if face == "top":
            return pants
        if face == "bottom":
            return (30, 20, 14)
        if y < 7:
            return pants if not (face in ("right", "left") and x == 1) else gold_dk  # stripe
        if y == 7:
            return gold  # boot top
        if y == 11:
            return (30, 20, 14)
        if face == "front" and y == 9 and x in (1, 2):
            return gold_lt  # buckle
        return boot

    s.paint(R_LEG, leg, grain=4)
    s.paint(L_LEG, leg, grain=4)
    return s


def vip_tux():
    s = Skin(99)
    skin, skin_dk = (232, 192, 160), (204, 162, 130)
    black, satin, black_lt = (26, 26, 32), (52, 52, 62), (40, 40, 48)
    white, white_dk = (242, 242, 240), (210, 210, 212)
    gold, gold_dk = (238, 192, 52), (180, 134, 30)

    s.paint(HEAD, vip_face(skin, skin_dk, (34, 28, 24), (50, 56, 70), (34, 28, 24),
                           lambda x: (34, 28, 24)))  # waxed moustache

    def top_hat(face, x, y, w, h):
        if face == "top":
            return black_lt if x in (0, 7) or y in (0, 7) else black
        if face == "bottom":
            return None
        if y < 2:
            return black if y == 0 else black_lt
        if y == 2:
            return gold  # golden hat band
        if face == "front" and y == 4 and x == 5:
            return gold  # monocle rim
        if face == "front" and y == 5 and x == 6:
            return gold_dk  # monocle chain
        return None

    s.paint(HAT, top_hat, grain=3)

    def body(face, x, y, w, h):
        if face == "top":
            return white if x in (3, 4) else black
        if face == "bottom":
            return black
        if face == "front":
            if y == 0 and x in (2, 5):
                return gold  # bow tie
            if y == 0 and x in (3, 4):
                return gold_dk
            if x in (3, 4) and y < 7:
                if y in (2, 4) and x == 3:
                    return gold_dk  # studs
                return white if y < 6 else white_dk
            if x in (2, 5) and y < 6:
                return satin  # lapels
            if y == 1 and x in (1, 6):
                return satin
            if x == 6 and y == 3:
                return gold  # pocket square
            if x in (3, 4) and y == 8:
                return gold_dk  # button
            if y == 9:
                return black_lt  # cummerbund
            return black
        if face == "back" and y == 9:
            return black_lt
        return black

    s.paint(BODY, body, grain=3)

    def arm(face, x, y, w, h):
        if face == "top":
            return black
        if face == "bottom":
            return skin_dk
        if y < 9:
            return black
        if y == 9:
            if face == "front" and x == 1:
                return gold  # cufflink
            return white
        return skin

    s.paint(R_ARM, arm, grain=3)
    s.paint(L_ARM, arm, grain=3)

    def leg(face, x, y, w, h):
        if face == "top":
            return black
        if face == "bottom":
            return (14, 14, 16)
        if y < 10:
            if face in ("right", "left") and x == 1:
                return satin  # trouser stripe
            return black
        if face == "front" and y == 10 and x == 1:
            return (120, 120, 130)  # polished shine
        return (16, 16, 20)  # patent shoes

    s.paint(R_LEG, leg, grain=3)
    s.paint(L_LEG, leg, grain=3)
    return s


# --------------------------------------------------- Zahradnice, Astronautka ---

def gardener():
    s = Skin(33)
    skin, skin_dk = (240, 200, 170), (214, 172, 142)
    hair, hair_dk = (204, 110, 40), (164, 82, 26)
    freckle = (210, 150, 110)
    check_a, check_b = (232, 226, 206), (196, 84, 70)  # checked shirt
    denim, denim_dk, denim_lt = (70, 128, 62), (50, 98, 44), (98, 156, 86)
    glove, glove_dk = (236, 132, 150), (200, 100, 120)  # pink garden gloves
    boot, boot_dk = (236, 196, 40), (196, 156, 24)
    dirt = (110, 78, 50)

    def head(face, x, y, w, h):
        if face == "top":
            return hair
        if face == "bottom":
            return skin_dk
        if face == "back":
            if x in (3, 4) and y >= 2:
                return hair_dk if y % 2 else hair  # braid
            return hair
        if face in ("right", "left"):
            if y < 3 or (y < 6 and ((face == "right" and x < 4) or (face == "left" and x > 3))):
                return hair
            return skin
        if y == 0 or (y == 1 and x not in (2, 3, 4, 5)) or (y in (2, 3) and x in (0, 7)):
            return hair  # fringe and hair at the sides
        if y == 3 and x in (1, 2, 5, 6):
            return (170, 90, 40)
        if y == 4:
            return {1: (242, 242, 238), 2: (60, 130, 70), 5: (60, 130, 70), 6: (242, 242, 238)}.get(x, skin)
        if y == 5:
            if x in (1, 6):
                return (236, 160, 150)  # rosy cheeks
            if x in (2, 5):
                return freckle
            if x in (3, 4):
                return skin_dk
        if y == 6 and x in (3, 4):
            return (196, 96, 90)
        return skin

    s.paint(HEAD, head)

    straw, straw_dk, straw_lt = (226, 192, 110), (186, 150, 76), (244, 216, 140)

    def hat(face, x, y, w, h):
        if face == "top":
            return straw_lt if (x + y) % 3 == 0 else straw
        if face == "bottom":
            return None
        if y == 0:
            return straw_dk if (x % 2) else straw
        if y == 1:
            if face == "right" and x in (5, 6):
                return (220, 60, 80)  # flower on the band
            return (60, 130, 70)  # green ribbon
        if y == 2:
            return straw_dk  # brim
        return None

    s.paint(HAT, hat, grain=5)

    def body(face, x, y, w, h):
        checked = check_b if (x // 2 + y // 2) % 2 else check_a
        if face == "top":
            return check_a
        if face == "bottom":
            return denim_dk
        if face == "front":
            if y < 4:
                if x in (1, 6):
                    return denim  # straps
                if y == 3 and 2 <= x <= 5:
                    return denim  # bib top
                return checked
            if y == 4 and x in (1, 6):
                return (220, 200, 90)  # brass buttons
            if 5 <= y <= 7 and 2 <= x <= 5:
                if y == 5:
                    return denim_lt  # pocket edge
                if x == 5 and y == 6:
                    return (80, 160, 60)  # seed packet in the pocket
                return denim_dk
            if y == 9 and x in (0, 7):
                return dirt
            return denim
        if face == "back":
            if y < 4:
                if x == y + 1 or x == 6 - y:
                    return denim  # crossed straps
                return checked
            return denim
        if y < 4:
            return checked
        return denim if x not in (0, 3) else denim_dk

    s.paint(BODY, body)

    def arm(face, x, y, w, h):
        checked = check_b if (x // 2 + y // 2) % 2 else check_a
        if face == "top":
            return check_a
        if face == "bottom":
            return glove_dk
        if y < 4:
            return checked
        if y == 4:
            return check_a  # rolled sleeve
        if y < 8:
            return skin
        if y == 8:
            return glove_dk
        if face == "front" and y == 10 and x == 2:
            return dirt
        return glove

    s.paint(R_ARM, arm)
    s.paint(L_ARM, arm)

    def leg(face, x, y, w, h):
        if face == "top":
            return denim
        if face == "bottom":
            return (60, 50, 40)
        if y >= 7:
            if y == 11:
                return (60, 50, 40)
            if face == "front" and y == 8 and x == 1:
                return dirt
            return boot if y > 7 else boot_dk  # rubber boots
        if face == "front" and y in (3, 4) and x in (1, 2):
            return denim_lt if y == 3 else denim  # knee patch
        if s.rng.random() < 0.03:
            return dirt
        return denim

    s.paint(R_LEG, leg)
    s.paint(L_LEG, leg)
    return s


def astronaut():
    s = Skin(66)
    suit, suit_lt, suit_dk = (232, 234, 238), (250, 250, 252), (186, 190, 200)
    grey, grey_dk = (128, 134, 146), (82, 88, 100)
    orange = (240, 124, 36)
    blue = (52, 110, 210)
    glass, glass_lt, glass_dk = (60, 96, 150), (150, 196, 240), (34, 56, 96)
    skin, hair = (226, 180, 146), (92, 56, 34)

    def tinted(c):
        return mix(c, glass, 0.25)

    def head(face, x, y, w, h):
        if face == "top":
            return suit_lt if x in (3, 4) else suit
        if face == "bottom":
            return grey
        if face == "front":
            if 1 <= x <= 6 and 2 <= y <= 5:
                # the face behind the tinted visor
                if (x, y) in ((5, 2), (6, 2), (6, 3)):
                    return glass_lt  # reflection
                if y == 2:
                    return tinted(hair)
                if y == 3:
                    return tinted({1: (245, 245, 245), 2: (40, 70, 150), 5: (40, 70, 150), 6: (245, 245, 245)}.get(x, skin))
                if y == 5 and x in (3, 4):
                    return tinted((200, 110, 110))
                return tinted(skin)
            if x in (0, 7) or y in (1, 6):
                return suit_dk  # visor frame
            if y == 7:
                return grey  # neck ring
            if y == 0:
                return suit
        if face in ("right", "left"):
            if x in (3, 4) and y in (3, 4):
                return grey_dk if (x + y) % 2 else grey  # helmet light
            if y == 7:
                return grey
        if face == "back":
            if y == 7:
                return grey
            if y in (3, 4) and 2 <= x <= 5:
                return suit_dk
        return suit if y > 0 else suit_lt

    s.paint(HEAD, head, grain=2)

    def body(face, x, y, w, h):
        if face == "top":
            return grey if face == "top" and 2 <= x <= 5 else suit
        if face == "bottom":
            return suit_dk
        if face == "front":
            if 2 <= y <= 5 and 2 <= x <= 5:
                # control panel
                if y in (2, 5) or x in (2, 5):
                    return grey_dk
                return {(3, 3): (230, 60, 50), (4, 3): (70, 220, 90), (3, 4): (240, 210, 60), (4, 4): blue}[(x, y)]
            if y == 1 and x in (0, 1):
                return blue  # mission patch: a little cube
            if y == 1 and x == 6:
                return orange
            if y == 8:
                return grey  # belt
            if y in (9, 10) and x in (0, 7):
                return orange  # hoses
            return suit if x not in (0, 7) else suit_dk
        if face == "back":
            if 0 <= y <= 8 and 1 <= x <= 6:
                # life support backpack
                if y in (0, 8) or x in (1, 6):
                    return grey_dk
                if y == 2 and x in (2, 3):
                    return (70, 220, 90)
                if y == 6 and 2 <= x <= 5:
                    return orange
                return grey
            if y == 8:
                return grey
        if y == 8:
            return grey
        return suit if x not in (0, 3) else suit_dk

    s.paint(BODY, body, grain=2)

    def arm(face, x, y, w, h):
        if face == "top":
            return suit
        if face == "bottom":
            return grey_dk
        if y == 3:
            return orange  # shoulder stripe
        if y == 6:
            return suit_dk  # elbow fold
        if y == 9:
            return grey  # wrist ring
        if y >= 10:
            return grey_dk if face == "front" and y == 11 else grey  # gloves
        if face == "left" and w == 4 and y == 7 and x in (1, 2):
            return glass_dk  # wrist pad
        return suit

    s.paint(R_ARM, arm, grain=2)
    s.paint(L_ARM, arm, grain=2)

    def leg(face, x, y, w, h):
        if face == "top":
            return suit
        if face == "bottom":
            return grey_dk
        if y in (4, 5):
            return grey if face == "front" else suit_dk  # knee pads
        if y >= 9:
            if y == 11:
                return grey_dk
            if face == "front" and y == 9:
                return orange
            return grey  # moon boots
        if face in ("right", "left") and x == 1 and y < 4:
            return blue  # stripe
        return suit if not (face in ("front", "back") and x in (0, w - 1)) else mix(suit, suit_dk, 0.5)

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
    made = [("Horník", "miner", miner()), ("Rytíř", "knight", knight()), ("Robot", "robot", robot()),
            ("Zlatý VIP", "vip_gold", vip_gold()), ("VIP Gentleman", "vip_tux", vip_tux()),
            ("Zahradnice", "gardener", gardener()), ("Astronautka", "astronaut", astronaut())]
    for title, name, skin in made:
        print(skin.save(name))
    p = os.path.join(ROOT, "docs", "img", "skins_preview.png")
    preview([(t, s) for t, _, s in made[:3]], p)
    preview([(t, s) for t, _, s in made[3:5]], os.path.join(ROOT, "docs", "img", "skins_vip_preview.png"))
    preview([(t, s) for t, _, s in made[5:]], os.path.join(ROOT, "docs", "img", "skins_gardener_astronaut_preview.png"))
    print(p)

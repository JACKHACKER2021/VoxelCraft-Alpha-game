#!/usr/bin/env python3
"""VoxelCraft trailer: composes 1280x720 frames from game footage, then ffmpeg encodes them."""
import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = sys.argv[1]  # VoxelCraft repo
OUT = sys.argv[2]   # frames dir
SHOTS = os.path.join(ROOT, "build/snapshots")
FLY = os.path.join(ROOT, "build/video")
W, H, FPS = 1280, 720, 24
os.makedirs(OUT, exist_ok=True)

BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def font(size, bold=True):
    return ImageFont.truetype(BOLD if bold else REG, size)


atlas = Image.open(os.path.join(FLY, "atlas.png")).convert("RGBA")


def tile(index, size):
    x, y = (index % 16) * 16, (index // 16) * 16
    return atlas.crop((x, y, x + 16, y + 16)).resize((size, size), Image.NEAREST)


# Tile indices from Blocks.h (T_COUNT order).
T = {}
names = """T_STONE T_GRASS_TOP T_GRASS_SIDE T_DIRT T_COBBLE T_PLANKS T_BEDROCK
T_WATER T_SAND T_GRAVEL T_LOG_SIDE T_LOG_TOP T_LEAVES T_GLASS T_BRICKS
T_LAVA T_OBSIDIAN T_NETHERRACK T_SOUL_SAND T_GLOWSTONE T_PORTAL T_NETHER_BRICK""".split()
for i, n in enumerate(names):
    T[n] = i


def tile_index(name):
    # Parses Blocks.h to find any tile's index.
    import re
    src = open(os.path.join(ROOT, "app/src/main/cpp/engine/Blocks.h")).read()
    body = src[src.index("enum Tile"):src.index("T_COUNT")]
    body = re.sub(r"//.*", "", body)
    idx, table = 0, {}
    for tok in re.findall(r"(T_\w+)(\s*=\s*([^,]+))?", body):
        n, _, expr = tok
        if expr:
            expr = expr.strip()
            m = re.match(r"(T_\w+)\s*\+\s*(\d+)", expr)
            idx = table[m.group(1)] + int(m.group(2)) if m else int(expr)
        table[n] = idx
        idx += 1
    return table[name]


def ease(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def cover(img, zoom=1.0, cx=0.5, cy=0.5):
    """Scales an image to cover the frame (with Ken Burns zoom around cx, cy)."""
    iw, ih = img.size
    s = max(W / iw, H / ih) * zoom
    nw, nh = int(iw * s), int(ih * s)
    big = img.resize((nw, nh), Image.LANCZOS)
    x = int((nw - W) * cx)
    y = int((nh - H) * cy)
    return big.crop((x, y, x + W, y + H))


def fit(img, zoom=1.0, bg=(24, 20, 18)):
    """Fits a UI screenshot inside the frame on a dark background, slightly zooming."""
    iw, ih = img.size
    s = min(W / iw, H / ih) * zoom
    nw, nh = int(iw * s), int(ih * s)
    frame = Image.new("RGB", (W, H), bg)
    frame.paste(img.resize((nw, nh), Image.LANCZOS), ((W - nw) // 2, (H - nh) // 2))
    return frame


def text(draw, xy, s, size, fill=(255, 255, 255), anchor="mm", alpha=1.0, bold=True, shadow=True):
    f = font(size, bold)
    a = int(255 * max(0.0, min(1.0, alpha)))
    if shadow:
        for dx, dy in ((4, 4), (3, 3)):
            draw.text((xy[0] + dx, xy[1] + dy), s, font=f, fill=(0, 0, 0, int(a * 0.7)), anchor=anchor)
    draw.text(xy, s, font=f, fill=fill + (a,), anchor=anchor, stroke_width=2, stroke_fill=(30, 20, 10, a))


def caption(frame, title, sub, t, dur):
    """A banner at the bottom that slides in and fades out."""
    a = min(ease(t / 0.5), ease((dur - t) / 0.5))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    d.rectangle((0, H - 150, W, H), fill=(0, 0, 0, int(140 * a)))
    off = int((1 - ease(t / 0.6)) * 40)
    text(d, (W // 2 + off, H - 100), title, 46, (255, 236, 160), alpha=a)
    if sub:
        text(d, (W // 2 - off, H - 50), sub, 26, (235, 235, 235), alpha=a, bold=False)
    frame = frame.convert("RGBA")
    frame.alpha_composite(over)
    return frame.convert("RGB")


def fade(frame, amount):
    if amount <= 0:
        return frame
    return Image.blend(frame, Image.new("RGB", (W, H), (0, 0, 0)), min(1.0, amount))


frames = []


def add(fn, seconds):
    n = int(seconds * FPS)
    for i in range(n):
        frames.append((fn, i / FPS, seconds))


def shot(name):
    return Image.open(os.path.join(SHOTS, name + ".png")).convert("RGB")


def fly(i):
    i = max(0, min(479, i))
    return Image.open(os.path.join(FLY, "fly_%04d.png" % i)).convert("RGB")


# --- 1. Voxel Softworks presents
def intro(t, d):
    f = Image.new("RGB", (W, H), (0, 0, 0))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dr = ImageDraw.Draw(over)
    a = min(ease(t / 0.8), ease((d - t) / 0.6))
    # A small cube of blocks drawn from our own textures.
    g = tile(tile_index("T_GRASS_SIDE"), 72)
    for k in range(3):
        over.paste(g, (W // 2 - 108 + k * 72, 200), g)
    over.putalpha(over.getchannel("A").point(lambda v: int(v * a)))
    dr = ImageDraw.Draw(over)
    text(dr, (W // 2, 360), "VOXEL SOFTWORKS", 64, (255, 255, 255), alpha=a)
    text(dr, (W // 2, 430), "uvádí", 32, (200, 200, 200), alpha=a, bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return f.convert("RGB")


add(intro, 3.2)


# --- 2. The title screen
title = shot("30_title_cs")


def title_card(t, d):
    f = cover(title, 1.0 + 0.08 * t / d, 0.5, 0.35)
    return fade(f, 1 - ease(t / 0.6) + ease((t - d + 0.4) / 0.4))


add(title_card, 3.5)


# --- 3. Endless worlds (fly-over)
def flight(start, title_s, sub):
    def fn(t, d):
        f = fly(start + int(t * FPS)).resize((W, H), Image.NEAREST)
        f = caption(f, title_s, sub, t, d)
        return fade(f, 1 - ease(t / 0.4))
    return fn


add(flight(0, "Nekonečné světy", "Každý svět je jiný – prozkoumej ho celý"), 7)


# --- 4. Biomes and structures (Ken Burns over our renders)
def still(name, title_s, sub, zoom_from=1.0, zoom_to=1.12, cx=0.5, cy=0.5, ui=False):
    img = shot(name)

    def fn(t, d):
        z = zoom_from + (zoom_to - zoom_from) * t / d
        f = fit(img, z) if ui else cover(img, z, cx, cy)
        if title_s:
            f = caption(f, title_s, sub, t, d)
        return fade(f, 1 - ease(t / 0.3) + ease((t - d + 0.3) / 0.3))
    return fn


add(still("world_14_biome_snowy_taiga", "Zasněžené lesy", "Jehličnaté lesy, sníh a led"), 2.6)
add(still("world_14_biome_mountains", "Vysoké hory", "Vrcholky z tvrdého kamene"), 2.6)
add(still("world_14_biome_desert", "Pouště s kaktusy", None), 2.2)
add(still("world_15_shipwreck", "Struktury plné pokladů", "Potopené lodě, zříceniny, kobky a doly"), 2.4)
add(still("world_15_dungeon", "Struktury plné pokladů", "Potopené lodě, zříceniny, kobky a doly"), 1.8)
add(still("world_15_mineshaft", "Struktury plné pokladů", "Potopené lodě, zříceniny, kobky a doly"), 1.8)
add(still("world_16_third_person_steel", "Bronz, ocel a rubín", "Nové rudy, nástroje a brnění"), 2.6)
add(still("world_18_third_person_boat", "Pluj po mořích", None), 2.2)
add(still("33_worlds_lan", "Hraj s přáteli", "Otevři svět v síti LAN a stavějte spolu", 0.95, 1.02, ui=True), 2.8)
add(still("35_lan_client", "Hraj s přáteli", "Otevři svět v síti LAN a stavějte spolu", 0.95, 1.02, ui=True), 2.2)
add(still("03d_achievements", "Úspěchy a mince", "Splň cíle a utrať mince v obchodě", 0.95, 1.02, ui=True), 2.6)
add(still("10b_chat", "Chat a příkazy", "/tp, /give, /gamemode a další", 0.95, 1.02, ui=True), 2.4)
add(flight(240, "Tvůj svět, tvoje pravidla", None), 5)


# --- 5. Coming soon: the Crimson Void (teaser with the real new textures)
eye = tile(tile_index("T_EYE_LOG"), 300)
teaser_tiles = ["T_VOID_GRASS_SIDE", "T_NETHER_GRASS_SIDE", "T_RED_SAND", "T_VOID_STONE_BRICKS", "T_PUMPKIN_FACE",
                "T_VOID_LEAVES", "T_VOID_PORTAL", "T_NETHER_BRICK"]


def teaser(t, d):
    f = Image.new("RGB", (W, H), (12, 4, 18))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    # Floating blocks in the dark.
    for k, name in enumerate(teaser_tiles):
        size = 90
        ang = t * 0.4 + k * (2 * math.pi / len(teaser_tiles))
        x = int(W / 2 + math.cos(ang) * 470 - size / 2)
        y = int(H / 2 + math.sin(ang) * 230 - size / 2)
        tl = tile(tile_index(name), size)
        over.paste(tl, (x, y), tl)
    # The eye opens in the middle.
    open_ = ease((t - 0.5) / 1.5)
    if open_ > 0:
        e = eye.copy()
        h = max(2, int(300 * open_))
        e = e.resize((300, h), Image.NEAREST)
        glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        gd.ellipse((W / 2 - 230, H / 2 - 230, W / 2 + 230, H / 2 + 230), fill=(160, 20, 60, int(90 * open_)))
        glow = glow.filter(ImageFilter.GaussianBlur(40))
        over.alpha_composite(glow)
        over.paste(e, (W // 2 - 150, H // 2 - h // 2), e)
    d_ = ImageDraw.Draw(over)
    a1 = ease((t - 1.8) / 0.6)
    text(d_, (W // 2, 95), "PŘIPRAVUJEME", 34, (220, 160, 255), alpha=a1)
    a2 = ease((t - 2.4) / 0.6)
    text(d_, (W // 2, H - 120), "CRIMSON VOID", 72, (255, 60, 70), alpha=a2)
    a3 = ease((t - 3.4) / 0.6)
    text(d_, (W // 2, H - 55), "Nový Nether • pevnosti • nová dimenze • bytosti • boss", 26, (235, 220, 255),
         alpha=a3, bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return fade(f.convert("RGB"), 1 - ease(t / 0.5) + ease((t - d + 0.5) / 0.5))


add(teaser, 7.5)


# --- 6. Welcome
def welcome(t, d):
    f = cover(fly(470), 1.15 - 0.05 * t / d)
    f = f.filter(ImageFilter.GaussianBlur(3))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 110))
    dr = ImageDraw.Draw(over)
    a = ease(t / 0.8)
    text(dr, (W // 2, 230), "Vítej ve hře", 44, (255, 255, 255), alpha=a)
    text(dr, (W // 2, 320), "VOXELCRAFT", 110, (255, 214, 120), alpha=ease((t - 0.4) / 0.8))
    text(dr, (W // 2, 430), "Postav. Prozkoumej. Přežij.", 36, (235, 235, 235), alpha=ease((t - 1.2) / 0.8), bold=False)
    text(dr, (W // 2, 560), "Hoppy_Hopsy21 aneb Voxel Softworks", 28, (200, 230, 170), alpha=ease((t - 2.0) / 0.8))
    text(dr, (W // 2, 605), "Android 16 • verze 1.0.0", 24, (190, 190, 190), alpha=ease((t - 2.4) / 0.8), bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return fade(f.convert("RGB"), 1 - ease(t / 0.5) + ease((t - d + 1.0) / 1.0))


add(welcome, 6.5)

print("frames:", len(frames), "seconds:", len(frames) / FPS)
for i, (fn, t, d) in enumerate(frames):
    fn(t, d).save(os.path.join(OUT, "t_%05d.png" % i), compress_level=1)
    if i % 120 == 0:
        print("frame", i)

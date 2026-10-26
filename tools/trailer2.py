#!/usr/bin/env python3
"""VoxelCraft story trailer (English, narrated).

Inputs: build/trailer/<scene>/f_*.png (tests/trailer_scenes.cpp), build/snapshots/*.png
(the UI and world snapshots), build/trailer/vo/*.wav (Piper narration, see README),
the game's own music. Output: dist/VoxelCraft-trailer-en.mp4
"""
import math
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
FRAMES = os.path.join(ROOT, "build/trailer/frames")
SCENES = os.path.join(ROOT, "build/trailer")
SHOTS = os.path.join(ROOT, "build/snapshots")
VO = os.path.join(ROOT, "build/trailer/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-trailer-en.mp4")
W, H, FPS = 1280, 720, 24
BAR = 54          # letterbox bars
XFADE = 0.5       # crossfade between segments (seconds)
os.makedirs(FRAMES, exist_ok=True)

BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
_fonts = {}


def font(size, bold=True):
    key = (size, bold)
    if key not in _fonts:
        _fonts[key] = ImageFont.truetype(BOLD if bold else REG, size)
    return _fonts[key]


def ease(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


atlas = Image.open(os.path.join(ROOT, "build/video/atlas.png")).convert("RGBA")


def tile_index(name):
    src = open(os.path.join(ROOT, "app/src/main/cpp/engine/Blocks.h")).read()
    body = re.sub(r"//.*", "", src[src.index("enum Tile"):src.index("T_COUNT")])
    idx, table = 0, {}
    for n, _, expr in re.findall(r"(T_\w+)(\s*=\s*([^,]+))?", body):
        if expr:
            m = re.match(r"(T_\w+)\s*\+\s*(\d+)", expr.strip())
            idx = table[m.group(1)] + int(m.group(2)) if m else int(expr)
        table[n] = idx
        idx += 1
    return table[name]


def tile(name, size):
    i = tile_index(name)
    x, y = (i % 16) * 16, (i // 16) * 16
    return atlas.crop((x, y, x + 16, y + 16)).resize((size, size), Image.NEAREST)


_cache = {}


def load(path, size=None):
    key = (path, size)
    if key not in _cache:
        if len(_cache) > 64:
            _cache.clear()
        img = Image.open(path).convert("RGB")
        if size:
            img = img.resize(size, Image.LANCZOS)
        _cache[key] = img
    return _cache[key]


def cover(img, zoom=1.0, cx=0.5, cy=0.5):
    iw, ih = img.size
    s = max(W / iw, H / ih) * zoom
    nw, nh = int(iw * s), int(ih * s)
    big = img.resize((nw, nh), Image.LANCZOS)
    x, y = int((nw - W) * cx), int((nh - H) * cy)
    return big.crop((x, y, x + W, y + H))


def fit(img, zoom=1.0):
    """A UI screenshot on a softly blurred copy of itself."""
    bg = cover(img, 1.25).filter(ImageFilter.GaussianBlur(18))
    bg = Image.blend(bg, Image.new("RGB", (W, H), (10, 10, 14)), 0.45)
    iw, ih = img.size
    s = min(W / iw, (H - 2 * BAR - 30) / ih) * zoom
    nw, nh = int(iw * s), int(ih * s)
    shadow = Image.new("RGBA", (nw + 40, nh + 40), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rectangle((20, 20, nw + 20, nh + 20), fill=(0, 0, 0, 150))
    shadow = shadow.filter(ImageFilter.GaussianBlur(12))
    bg = bg.convert("RGBA")
    bg.alpha_composite(shadow, ((W - nw) // 2 - 14, (H - nh) // 2 - 6))
    bg = bg.convert("RGB")
    bg.paste(img.resize((nw, nh), Image.LANCZOS), ((W - nw) // 2, (H - nh) // 2))
    return bg


def put_text(d, xy, s, size, fill, alpha=1.0, anchor="la", bold=True, spacing=0):
    a = int(255 * max(0.0, min(1.0, alpha)))
    if a <= 0:
        return
    f = font(size, bold)
    if spacing:
        s = (" " * spacing).join(s) if False else s
    d.text((xy[0] + 3, xy[1] + 3), s, font=f, fill=(0, 0, 0, int(a * 0.6)), anchor=anchor)
    d.text(xy, s, font=f, fill=tuple(fill) + (a,), anchor=anchor)


def lower_third(frame, title, sub, t, dur, accent=(255, 196, 64)):
    """Bottom-left caption with an accent bar that grows in."""
    if not title:
        return frame
    a = min(ease((t - 0.25) / 0.45), ease((dur - t) / 0.45))
    if a <= 0:
        return frame
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    x, y = 70, H - BAR - 112
    grow = ease((t - 0.25) / 0.6)
    tw = font(40).getlength(title)
    sw = font(22, False).getlength(sub) if sub else 0
    width = max(tw, sw) + 40
    d.rectangle((x - 20, y - 12, x - 20 + width * grow, y + 82), fill=(0, 0, 0, int(120 * a)))
    d.rectangle((x - 20, y - 12, x - 14, y + 82), fill=accent + (int(255 * a),))
    slide = int((1 - grow) * -30)
    put_text(d, (x + slide, y), title, 40, (255, 255, 255), a)
    if sub:
        put_text(d, (x + slide, y + 50), sub, 22, (225, 225, 225), a, bold=False)
    frame = frame.convert("RGBA")
    frame.alpha_composite(over)
    return frame.convert("RGB")


def letterbox(frame):
    d = ImageDraw.Draw(frame)
    d.rectangle((0, 0, W, BAR), fill=(0, 0, 0))
    d.rectangle((0, H - BAR, W, H), fill=(0, 0, 0))
    return frame


# ---------------------------------------------------------------- segment kinds

class Seg:
    def __init__(self, dur, draw, title=None, sub=None, vo=None, vo_at=0.4, accent=(255, 196, 64), bars=True):
        self.dur, self.draw, self.title, self.sub, self.vo, self.vo_at = dur, draw, title, sub, vo, vo_at
        self.accent, self.bars = accent, bars

    def frame(self, t):
        f = self.draw(t, self.dur)
        f = lower_third(f, self.title, self.sub, t, self.dur, self.accent)
        return letterbox(f) if self.bars else f


def clip(scene, start=0, count=None, zoom=(1.0, 1.0)):
    d = os.path.join(SCENES, scene)
    files = sorted(os.listdir(d))
    files = files[start:start + count] if count else files[start:]
    n = len(files)

    def draw(t, dur):
        i = min(n - 1, int(t * FPS))
        img = load(os.path.join(d, files[i]), (W, H))
        z = zoom[0] + (zoom[1] - zoom[0]) * (t / dur)
        return cover(img, z) if z != 1.0 else img.copy()
    return draw, n / FPS


def still(name, zoom=(1.0, 1.1), cx=0.5, cy=0.5):
    img = load(os.path.join(SHOTS, name + ".png"))

    def draw(t, dur):
        return cover(img, zoom[0] + (zoom[1] - zoom[0]) * t / dur, cx, cy)
    return draw


def ui(name):
    img = load(os.path.join(SHOTS, name + ".png"))

    def draw(t, dur):
        return fit(img, 0.96 + 0.04 * t / dur)
    return draw


def intro(t, dur):
    f = Image.new("RGB", (W, H), (0, 0, 0))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    a = min(ease((t - 0.3) / 0.9), ease((dur - t) / 0.7))
    # Three blocks drop into place one after another.
    for k, name in enumerate(["T_GRASS_SIDE", "T_STONE", "T_PLANKS"]):
        drop = ease((t - 0.2 - k * 0.25) / 0.5)
        if drop <= 0:
            continue
        tl = tile(name, 64)
        tl.putalpha(tl.getchannel("A").point(lambda v: int(v * a)))
        over.alpha_composite(tl, (W // 2 - 100 + k * 68, int(250 - (1 - drop) * 120)))
    d = ImageDraw.Draw(over)
    put_text(d, (W // 2, 360), "VOXEL SOFTWORKS", 58, (255, 255, 255), a, anchor="mm")
    put_text(d, (W // 2, 420), "presents", 28, (190, 190, 190), ease((t - 1.0) / 0.8) * a, anchor="mm", bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return f.convert("RGB")


def soon_card(t, dur):
    f = Image.new("RGB", (W, H), (8, 2, 12))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = 160 + 40 * math.sin(t * 3)
    gd.ellipse((W / 2 - r, H / 2 - r, W / 2 + r, H / 2 + r), fill=(150, 20, 70, int(120 * ease(t / 0.8))))
    over.alpha_composite(glow.filter(ImageFilter.GaussianBlur(50)))
    d = ImageDraw.Draw(over)
    put_text(d, (W // 2, H // 2 - 30), "COMING SOON", 62, (255, 220, 240), ease((t - 0.2) / 0.6), anchor="mm")
    put_text(d, (W // 2, H // 2 + 40), "UPDATE 1.1", 30, (210, 150, 255), ease((t - 0.7) / 0.6), anchor="mm")
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return f.convert("RGB")


def end_card(t, dur):
    bg = load(os.path.join(SCENES, "03_build", sorted(os.listdir(os.path.join(SCENES, "03_build")))[-1]), (W, H))
    f = cover(bg, 1.12 - 0.06 * t / dur).filter(ImageFilter.GaussianBlur(4))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 120))
    d = ImageDraw.Draw(over)
    put_text(d, (W // 2, 250), "VOXELCRAFT", 120, (255, 214, 120), ease((t - 0.3) / 0.9), anchor="mm")
    put_text(d, (W // 2, 350), "BUILD.   EXPLORE.   SURVIVE.", 34, (255, 255, 255), ease((t - 1.6) / 0.8), anchor="mm")
    put_text(d, (W // 2, 470), "A game by Hoppy_Hopsy21  •  Voxel Softworks", 26, (200, 235, 170), ease((t - 3.0) / 0.8),
             anchor="mm")
    put_text(d, (W // 2, 515), "For Android  •  Version 1.0", 22, (190, 190, 190), ease((t - 3.5) / 0.8), anchor="mm",
             bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    f = f.convert("RGB")
    return Image.blend(f, Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.2) / 1.2))


# ---------------------------------------------------------------- the edit

segs = []


def add(dur, draw, **kw):
    segs.append(Seg(dur, draw, **kw))


add(4.2, intro, vo="v01", vo_at=1.2, bars=False)
add(4.6, still("30_title_en", (1.0, 1.1), 0.5, 0.35), vo="v02", vo_at=0.6)
d, n = clip("01_arrival")
add(n, d, title="AN ENDLESS WORLD", sub="Every world is unique, and every world is yours", vo="v03", vo_at=1.0)
d, n = clip("02_chop")
add(n, d, title="GATHER", sub="Chop trees and collect materials", vo="v04", vo_at=0.3)
add(3.6, ui("12_crafting_table"), title="CRAFT", sub="Workbench, furnace and dozens of recipes")
d, n = clip("03_build")
add(n, d, title="BUILD", sub="Anything you can imagine, block by block", vo="v05", vo_at=1.2)
d, n = clip("04_mine")
add(n, d, title="MINE", sub="Bronze, steel and rare ruby ores", vo="v06", vo_at=0.3)
add(3.4, still("world_16_third_person_steel", (1.05, 1.18)), title="GEAR UP", sub="Tools and armor in five materials")
add(2.8, ui("11_survival_inventory"), title="GEAR UP", sub="Tools and armor in five materials")
for k, (scene, title, sub) in enumerate([
        ("06_0_snowy_taiga", "SNOWY PINE FORESTS", "Snow, ice and spruce trees"),
        ("06_1_mountains", "HIGH MOUNTAINS", "Peaks of hardened stone"),
        ("06_2_desert", "PLAINS AND DESERTS", "Cacti, sand and wide meadows"),
        ("06_3_ice_plains", "ICE PLAINS", "Frozen lakes as far as you can see")]):
    d, n = clip(scene)
    add(n, d, title=title, sub=sub, vo="v07" if k == 0 else None, vo_at=1.0, accent=(120, 200, 255))
for name in ["world_15_shipwreck", "world_15_ruin", "world_15_dungeon", "world_15_mineshaft"]:
    add(2.7, still(name, (1.02, 1.16)), title="DISCOVER", sub="Shipwrecks  •  ruins  •  dungeons  •  mineshafts",
        vo="v08" if name.endswith("shipwreck") else None, vo_at=0.4)
add(3.6, still("world_18_third_person_boat", (1.0, 1.15)), title="SAIL", sub="Boats for the open sea", vo="v09", vo_at=0.4)
d, n = clip("05_friends")
add(n, d, title="PLAY TOGETHER", sub="Local multiplayer over Wi-Fi", vo="v10", vo_at=0.5, accent=(140, 230, 120))
add(3.0, ui("33_worlds_lan"), title="PLAY TOGETHER", sub="Find games on your network in the world list", accent=(140, 230, 120))
add(3.0, ui("35_lan_client"), title="PLAY TOGETHER", sub="See your friends, their skins and name tags", accent=(140, 230, 120))
add(3.4, ui("23b_chat_commands"), title="CHAT & COMMANDS", sub="/tp   /give   /gamemode   /time", accent=(140, 230, 120))
add(3.4, ui("03d_achievements"), title="ACHIEVEMENTS", sub="Complete goals and earn coins", vo="v11", vo_at=0.3)
add(3.4, ui("16_store"), title="THE STORE", sub="Spend your coins on skins and worlds")
add(2.6, soon_card, vo="v12", vo_at=0.4, bars=False)
d, n = clip("07_fortress")
add(n, d, title="NETHER FORTRESSES", sub="Halls full of loot and a giant void portal", accent=(255, 90, 90))
d, n = clip("08_void_forest")
add(n, d, title="THE CRIMSON VOID", sub="A purple forest of eye trees", accent=(200, 120, 255))
d, n = clip("09_arena")
add(n, d, title="THE FINAL ARENA", sub="Something is waiting for you", accent=(255, 120, 40))
add(9.5, end_card, vo="v13", vo_at=1.0, bars=False)

# Timeline with crossfades.
starts, t = [], 0.0
for i, s in enumerate(segs):
    starts.append(t)
    t += s.dur - (XFADE if i < len(segs) - 1 else 0)
total = t
nframes = int(total * FPS)
print("segments:", len(segs), "length: %.1f s" % total)

for fi in range(nframes):
    gt = fi / FPS
    active = [i for i, s in enumerate(segs) if starts[i] <= gt < starts[i] + s.dur]
    if not active:
        active = [len(segs) - 1]
    if len(active) == 1:
        i = active[0]
        img = segs[i].frame(gt - starts[i])
    else:
        a, b = active[0], active[-1]
        fa = segs[a].frame(gt - starts[a])
        fb = segs[b].frame(gt - starts[b])
        img = Image.blend(fa, fb, ease((gt - starts[b]) / XFADE))
    img.save(os.path.join(FRAMES, "t_%05d.png" % fi), compress_level=1)
    if fi % 240 == 0:
        print("frame", fi, "/", nframes, flush=True)

# Narration: each line at its place, mixed over the music, which ducks under the voice.
inputs, filters, labels = [], [], []
k = 0
for i, s in enumerate(segs):
    if not s.vo:
        continue
    path = os.path.join(VO, s.vo + ".wav")
    delay = int((starts[i] + s.vo_at) * 1000)
    inputs += ["-i", path]
    filters.append("[%d:a]aresample=48000,adelay=%d|%d,volume=1.6[vo%d]" % (k + 2, delay, delay, k))
    labels.append("[vo%d]" % k)
    k += 1
music = os.path.join(ROOT, "app/src/main/assets/music/overworld.mp3")
fc = ";".join(filters)
fc += ";%samix=inputs=%d:normalize=0[voice]" % ("".join(labels), k)
fc += ";[voice]asplit=2[voice1][voice2]"
fc += ";[1:a]aresample=48000,volume=0.55,afade=t=in:st=0:d=2,afade=t=out:st=%.2f:d=4[mus]" % (total - 4)
fc += ";[mus][voice1]sidechaincompress=threshold=0.03:ratio=6:attack=20:release=400[ducked]"
fc += ";[ducked][voice2]amix=inputs=2:normalize=0,atrim=0:%.2f,alimiter=limit=0.89,volume=0.92[aout]" % total
cmd = ["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", os.path.join(FRAMES, "t_%05d.png"),
       "-i", music] + inputs + [
    "-filter_complex", fc, "-map", "0:v", "-map", "[aout]", "-c:v", "libx264", "-crf", "19", "-preset", "slow",
    "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", OUT]
subprocess.run(cmd, check=True)
print("wrote", OUT)

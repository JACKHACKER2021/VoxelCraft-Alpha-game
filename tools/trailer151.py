#!/usr/bin/env python3
"""VoxelCraft 1.5.1, the fixed "Forgotten Powerstone": the update trailer (1920x1080, narrated).

Inputs: build/film/<shot>/f_*.jpg (real gameplay filmed by tests/film/trailer_film.cpp),
build/trailer151/vo/*.wav (Piper narration, lines in build/trailer151/lines.txt), the game's own
music (menu theme, Overworld song, biome pieces). Output: dist/VoxelCraft-1.5.1-trailer-en.mp4
"""
import math
import os
import subprocess
import sys
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
FILM = os.path.join(ROOT, "build/film")
VO = os.path.join(ROOT, "build/trailer151/vo")
MUSIC = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build/trailer6/music")
OUT = os.path.join(ROOT, "dist/VoxelCraft-1.5.1-trailer-en.mp4")
W, H, FPS = 1920, 1080, 24
GW, GH = 1920, 864          # the game picture (20:9), letterboxed
BAR = (H - GH) // 2
XFADE = 0.45

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


def vo_len(name):
    with wave.open(os.path.join(VO, name + ".wav")) as w:
        return w.getnframes() / w.getframerate()


# ---------------------------------------------------------------- footage

_frames = {}


def frames_of(shot):
    if shot not in _frames:
        d = os.path.join(FILM, shot)
        _frames[shot] = [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.endswith(".jpg")]
    return _frames[shot]


_cache = {}


def game_frame(shot, i):
    fs = frames_of(shot)
    path = fs[max(0, min(len(fs) - 1, i))]
    if path not in _cache:
        if len(_cache) > 48:
            _cache.clear()
        _cache[path] = Image.open(path).convert("RGB").resize((GW, GH), Image.BICUBIC)
    return _cache[path]


def letterboxed(img):
    f = Image.new("RGB", (W, H), (0, 0, 0))
    f.paste(img, (0, BAR))
    return f


class Clip:
    """Seconds a..b of a filmed shot; played slower (up to 1.35x) or held when the group needs more time."""

    def __init__(self, shot, a=0.0, b=None):
        self.shot, self.a = shot, a
        n = len(frames_of(shot))
        self.b = b if b is not None else n / FPS
        self.natural = self.b - self.a
        self.dur = self.natural

    def frame(self, t):
        speed = max(1 / 1.35, self.natural / self.dur)
        i = int((self.a + t * speed) * FPS)
        return letterboxed(game_frame(self.shot, i))


class Card:
    def __init__(self, draw, dur):
        self.draw, self.natural, self.dur = draw, dur, dur

    def frame(self, t):
        return self.draw(t, self.dur)


def put(d, xy, s, size, fill, alpha=1.0, anchor="la", bold=True):
    a = int(255 * max(0.0, min(1.0, alpha)))
    if a <= 0:
        return
    f = font(size, bold)
    d.text((xy[0] + 3, xy[1] + 3), s, font=f, fill=(0, 0, 0, int(a * 0.65)), anchor=anchor)
    d.text(xy, s, font=f, fill=tuple(fill) + (a,), anchor=anchor)


def blurred(shot, t, frac=0.5):
    fs = frames_of(shot)
    img = game_frame(shot, int(len(fs) * frac + t * FPS * 0.6))
    bg = img.resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(10))
    return Image.blend(bg, Image.new("RGB", (W, H), (6, 6, 10)), 0.62)


def text_card(shot, lines, frac=0.3):
    """Lines of (text, size, colour, bold, delay) over a blurred, darkened gameplay shot."""
    def draw(t, dur):
        f = blurred(shot, t, frac).convert("RGBA")
        over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(over)
        total = sum(l[1] * 1.45 for l in lines)
        y = H / 2 - total / 2
        out = ease((dur - t) / 0.5)
        for text, size, col, bold, delay in lines:
            a = min(ease((t - delay) / 0.6), out)
            put(d, (W // 2, int(y + size * 0.7 + (1 - a) * 18)), text, size, col, a, anchor="mm", bold=bold)
            y += size * 1.45
        f.alpha_composite(over)
        return f.convert("RGB")
    return draw


GOLD, WHITE, DIM, PURPLE, RED, GREEN = (255, 210, 110), (255, 255, 255), (200, 200, 210), (205, 150, 255), (255, 120, 100), (160, 235, 130)


def block_tile(path, size):
    return None


def intro(t, dur):
    f = Image.new("RGB", (W, H), (0, 0, 0))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.3) / 1.0), ease((dur - t) / 0.8))
    # Three blocks, like the title's pixel logo, drop into place.
    cols = [((106, 170, 64), (134, 96, 67)), ((128, 128, 128), (100, 100, 100)), ((176, 136, 82), (150, 112, 64))]
    for k, (top, side) in enumerate(cols):
        drop = ease((t - 0.2 - k * 0.22) / 0.5)
        if drop <= 0:
            continue
        x, y = W // 2 - 140 + k * 100, int(380 - (1 - drop) * 160)
        d.rectangle((x, y, x + 84, y + 84), fill=side + (int(255 * a),))
        d.rectangle((x, y, x + 84, y + 22), fill=top + (int(255 * a),))
    put(d, (W // 2, 560), "VOXELSOFTWORKS", 84, WHITE, a, anchor="mm")
    put(d, (W // 2, 640), "presents", 36, DIM, min(ease((t - 1.1) / 0.8), a), anchor="mm", bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return f.convert("RGB")


def logo_card(t, dur):
    f = blurred("s01_title", t, 0.2).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    s = 1 + 0.04 * t / dur
    put(d, (W // 2, 470), "VOXELCRAFT", int(190 * s), GOLD, a, anchor="mm")
    put(d, (W // 2, 620), "VERSION 0.1  →  1.2.0", 40, WHITE, min(a, ease((t - 1.0) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


GAMES = ["Pixel Gun 3D & Pixel Gun games", "Wild West Craft & MultiCraft", "Craftsman", "Minecraft: Java Edition",
         "Mini Block Craft", "Minecraft: Bedrock Edition", "Hytale", "Crafting and Building", "Survivalcraft 2", "Luanti",
         "Build Block Craft", "PlanetCraft", "Minetest"]


def engine_card(t, dur):
    f = blurred("s15_creative", t, 0.6).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    out = ease((dur - t) / 0.5)
    put(d, (W // 2, 150), "OUR OWN ENGINE", 76, GOLD, min(ease(t / 0.6), out), anchor="mm")
    put(d, (W // 2, 230), "VoxelCraft contains no decompiled or borrowed engine code from:", 34, WHITE,
        min(ease((t - 0.6) / 0.6), out), anchor="mm", bold=False)
    for i, g in enumerate(GAMES):
        col, row = i % 2, i // 2
        a = min(ease((t - 1.2 - i * 0.18) / 0.4), out)
        x = W // 2 - 420 + col * 560 if i < 12 else W // 2 - 140
        put(d, (x, 300 + row * 58), "✕  " + g, 32, (235, 235, 235), a, bold=False)
    a = min(ease((t - 4.0) / 0.6), out)
    put(d, (W // 2, 720), "Its native library is its own:  libvoxelcraft.so", 36, GREEN, a, anchor="mm")
    put(d, (W // 2, 780), "C++  ·  HPP  ·  Android SDK & NDK  ·  Android Gradle", 30, DIM, a, anchor="mm", bold=False)
    f.alpha_composite(over)
    return f.convert("RGB")


def disclaimer_card(t, dur):
    return text_card("s19_4_ocean", [
        ("AN INDEPENDENT PROJECT", 64, GOLD, True, 0.0),
        ("", 20, WHITE, False, 0),
        ("VoxelCraft is not affiliated with Mojang, Microsoft", 36, WHITE, False, 0.5),
        ("or the makers of any game named in this video.", 36, WHITE, False, 0.7),
        ("", 20, WHITE, False, 0),
        ("Menus inspired by the classic Bedrock style  ·  every pixel drawn by our own engine", 28, DIM, False, 1.3),
    ], 0.5)(t, dur)


def end_card(t, dur):
    f = blurred("s16_structures", t, 0.95).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 330), "VOXELCRAFT", 170, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 470), "MADE WITH LOVE", 52, WHITE, ease((t - 1.2) / 0.7), anchor="mm")
    put(d, (W // 2, 530), "by and for people with autism", 36, DIM, ease((t - 1.6) / 0.7), anchor="mm", bold=False)
    put(d, (W // 2, 660), "By Hoppy_Hopsy21 and VoxelSoftworks", 44, GREEN, ease((t - 3.0) / 0.8), anchor="mm")
    put(d, (W // 2, 720), "Built together with Claude AI", 30, DIM, ease((t - 3.4) / 0.8), anchor="mm", bold=False)
    put(d, (W // 2, 830), "Android  ·  Version 1.2.0  ·  Coming to Google Play", 30, (220, 220, 220), ease((t - 4.0) / 0.8),
        anchor="mm", bold=False)
    f.alpha_composite(over)
    f = f.convert("RGB")
    return Image.blend(f, Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.6) / 1.6))


# ---------------------------------------------------------------- the edit

class Group:
    def __init__(self, vo, items, title=None, sub=None, badge=None, chapter=None, accent=GOLD, pad=1.2, music=None):
        self.vo, self.items, self.title, self.sub, self.badge = vo, items, title, sub, badge
        self.chapter, self.accent, self.music = chapter, accent, music
        natural = sum(i.natural for i in items)
        need = (vo_len(vo) + pad) if vo else 0
        self.dur = max(natural, need)
        if self.dur > natural:
            scale = self.dur / natural
            for i in self.items:
                i.dur = i.natural * scale

    def frame(self, t):
        acc = 0
        for i in self.items:
            if t < acc + i.dur or i is self.items[-1]:
                img = i.frame(min(t - acc, i.dur - 1e-3))
                break
            acc += i.dur
        if not any(isinstance(i, Card) for i in self.items):
            img = self.captions(img, t)
        return img

    def captions(self, img, t):
        over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(over)
        fade = min(ease((t - 0.2) / 0.5), ease((self.dur - t) / 0.4))
        if self.badge:
            put(d, (48, BAR // 2), self.badge, 34, self.accent, fade, anchor="lm")
        if self.title:
            put(d, (48, H - BAR // 2 - 14), self.title, 38, WHITE, fade, anchor="lm")
            if self.sub:
                tw = font(38).getlength(self.title)
                put(d, (48 + tw + 26, H - BAR // 2 - 12), self.sub, 28, DIM, fade, anchor="lm", bold=False)
        put(d, (W - 48, BAR // 2), "VOXELCRAFT", 30, (255, 255, 255), 0.55 * fade, anchor="rm")
        if self.chapter:
            a = min(ease((t - 0.1) / 0.5), ease((2.6 - t) / 0.5))
            if a > 0:
                band = Image.new("RGBA", (W, 220), (0, 0, 0, int(150 * a)))
                over.alpha_composite(band, (0, H // 2 - 110))
                put(d, (W // 2, H // 2 - 22), self.chapter[0], 78, self.accent, a, anchor="mm")
                put(d, (W // 2, H // 2 + 50), self.chapter[1], 34, WHITE, a, anchor="mm", bold=False)
        img = img.convert("RGBA")
        img.alpha_composite(over)
        return img.convert("RGB")


C = Clip
G = Group
OW, NE, VD, ST = (255, 210, 110), (255, 120, 90), (205, 150, 255), (140, 230, 120)

def update_card(t, dur):
    f = blurred("g02_village", t, 0.8).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 400), "VOXELCRAFT 1.5.1", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 530), "FORGOTTEN POWERSTONE", 64, (120, 230, 250), min(a, ease((t - 0.9) / 0.6)), anchor="mm")
    put(d, (W // 2, 610), "THE FIXED EDITION OF 1.5.0", 40, WHITE, min(a, ease((t - 1.6) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def fixes_card(t, dur):
    return text_card("g05_crimson_desert", [
        ("WHAT'S FIXED IN 1.5.1", 64, GOLD, True, 0.0),
        ("", 16, WHITE, False, 0),
        ("Wasteland, villages and the Husking moved to the Cornerstone Dimension", 34, WHITE, False, 0.5),
        ("Trees no longer show seams  ·  new netherperk and void trunks", 34, WHITE, False, 0.9),
        ("Pickaxe and axe remodelled  ·  items held right in third person", 34, WHITE, False, 1.3),
        ("Eye Logs no longer craft into void planks", 34, WHITE, False, 1.7),
        ("Your blocks, recipes, inventories and worlds stay as they were", 30, GREEN, False, 2.3),
    ], 0.5)(t, dur)


def end151(t, dur):
    f = blurred("g06_eyes", t, 0.6).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 360), "VOXELCRAFT 1.5.1", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 490), "FORGOTTEN POWERSTONE  ·  FIXED", 54, (120, 230, 250), ease((t - 1.0) / 0.7), anchor="mm")
    put(d, (W // 2, 620), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 40, GREEN, ease((t - 2.0) / 0.8), anchor="mm")
    put(d, (W // 2, 690), "Android  ·  Next: 1.6.0 Caves & Ores", 30, (220, 220, 220), ease((t - 2.6) / 0.8), anchor="mm", bold=False)
    f.alpha_composite(over)
    f = f.convert("RGB")
    return Image.blend(f, Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.4) / 1.4))


C = Clip
G = Group
B = "v1.5.1"
PS, LAND, MOB, CS, BUILD, VOIDC = (120, 230, 250), (200, 200, 110), (255, 130, 70), (255, 110, 90), (230, 200, 90), (205, 150, 255)
groups = [
    G("q01", [C("s00_splash", 0.2, 3.6)], music="nether"),
    G("q02", [Card(update_card, 5.6)]),
    G(None, [Card(fixes_card, 7.5)]),
    G("q03", [C("g01_wasteland")], "Powerstone Wasteland", "now in the Cornerstone Dimension", B,
      ("THE WASTELAND", "back where it belongs"), LAND),
    G("q04", [C("g02_village")], "Forgotten villages", "on netherrack foundations, lamps still lit", B, accent=PS),
    G("q05", [C("g03_archaeology")], "Archaeology", "the brusher, debris gravel, eight pages of lore", B, accent=BUILD),
    G("q06", [C("g04_husking")], "The Husking", "only in the wasteland", B, accent=MOB),
    G("q07", [C("f09_fortress")], "Fortresses", "and the way to the Crimson Void", B, accent=CS),
    G("q08", [C("g05_crimson_desert")], "Crimson Desert", "dunes, rock spires, crystals, buried ruins", B,
      ("CRIMSON DESERT", "rebuilt from the ground up"), VOIDC, music="forest"),
    G("q09", [C("g06_eyes")], "Eye Log and Void Brick Eye", "they blink and look around", B, accent=VOIDC),
    G("q10", [C("t_tools", 0.0, 9.0)], "New tool models", "pickaxe, axe, shovel and hoe", B,
      ("POLISH", "tools, trees and textures"), BUILD),
    G("q11", [C("g07_trees", 0.0, 1.6), C("f11_nature", 0.0, 3.0)], "Seamless bark", "new netherperk and void trunks", B, accent=LAND),
    G("q12", [C("g08_ores")], "Experimental Features", "Ore Redesign  ·  1.6.0 Preview, per world", B,
      ("EXPERIMENTS", "a first look at 1.6.0"), PS, music="overworld"),
    G("q13", [Card(end151, 8.0)], pad=3.0),
]

starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (XFADE if i < len(groups) - 1 else 0)
total = t
print("groups:", len(groups), "length: %.1f s" % total, flush=True)
for g, s in zip(groups, starts):
    if g.music:
        print("  music %-10s at %6.1f s" % (g.music, s))


def frame_at(gt):
    active = [i for i, g in enumerate(groups) if starts[i] <= gt < starts[i] + g.dur] or [len(groups) - 1]
    if len(active) == 1:
        i = active[0]
        return groups[i].frame(gt - starts[i])
    a, b = active[0], active[-1]
    fa = groups[a].frame(gt - starts[a])
    fb = groups[b].frame(gt - starts[b])
    return Image.blend(fa, fb, ease((gt - starts[b]) / XFADE))


# ---------------------------------------------------------------- audio

def audio_track(path):
    mus_inputs, filters, labels = [], [], []
    # Music: each chapter's piece from where its group starts to where the next piece starts.
    cues = [(starts[i], g.music) for i, g in enumerate(groups) if g.music]
    files = {"menu": "menu_theme.mp3", "overworld": "overworld.mp3", "nether": "biome_nether.mp3",
             "overworld2": "biome_plains.mp3", "forest": "biome_forest.mp3", "menu2": "menu_theme.mp3"}
    offsets = {"menu2": 40.0}
    k = 0
    for j, (st, name) in enumerate(cues):
        end = cues[j + 1][0] + 1.5 if j + 1 < len(cues) else total
        dur = end - st
        mus_inputs += ["-stream_loop", "-1", "-i", os.path.join(MUSIC, files[name])]
        off = offsets.get(name, 0.0)
        d = int(st * 1000)
        filters.append("[%d:a]aresample=48000,atrim=%.2f:%.2f,asetpts=PTS-STARTPTS,afade=t=in:st=0:d=1.2,"
                       "afade=t=out:st=%.2f:d=2,adelay=%d|%d[m%d]" % (k, off, off + dur, max(0.1, dur - 2), d, d, k))
        labels.append("[m%d]" % k)
        k += 1
    nm = k
    vo_inputs, vlabels = [], []
    for i, g in enumerate(groups):
        if not g.vo:
            continue
        vo_inputs += ["-i", os.path.join(VO, g.vo + ".wav")]
        d = int((starts[i] + 0.35) * 1000)
        filters.append("[%d:a]aresample=48000,adelay=%d|%d,volume=1.7[v%d]" % (k, d, d, k))
        vlabels.append("[v%d]" % k)
        k += 1
    for name, at in boom_cues():
        vo_inputs += ["-i", name]
        d = int(at * 1000)
        filters.append("[%d:a]aresample=48000,adelay=%d|%d,volume=0.9[v%d]" % (k, d, d, k))
        vlabels.append("[v%d]" % k)
        k += 1
    fc = ";".join(filters)
    fc += ";%samix=inputs=%d:normalize=0,volume=0.5[mus]" % ("".join(labels), nm)
    fc += ";%samix=inputs=%d:normalize=0[voice]" % ("".join(vlabels), len(vlabels))
    fc += ";[voice]asplit=2[voice1][voice2]"
    fc += ";[mus][voice1]sidechaincompress=threshold=0.03:ratio=6:attack=20:release=450[ducked]"
    fc += ";[ducked][voice2]amix=inputs=2:normalize=0,apad,atrim=0:%.2f,alimiter=limit=0.9[aout]" % total
    cmd = ["ffmpeg", "-y", "-loglevel", "error"] + mus_inputs + vo_inputs + [
        "-filter_complex", fc, "-map", "[aout]", "-c:a", "pcm_s16le", path]
    subprocess.run(cmd, check=True)


def boom_cues():
    """Explosions: a synthesized boom where TNT goes off in the footage."""
    path = os.path.join(ROOT, "build/trailer6/boom.wav")
    if not os.path.exists(path):
        sr = 48000
        n = int(sr * 2.2)
        rng = np.random.default_rng(7)
        noise = rng.standard_normal(n)
        # Low-passed noise with a fast attack and a long tail, plus a thump.
        lp = np.zeros(n)
        a = 0.04
        for i in range(1, n):
            lp[i] = lp[i - 1] + a * (noise[i] - lp[i - 1])
        tt = np.arange(n) / sr
        env = np.minimum(1, tt / 0.01) * np.exp(-tt * 2.2)
        thump = np.sin(2 * np.pi * 48 * tt * np.exp(-tt * 0.8)) * np.exp(-tt * 5)
        sig = lp * env * 6 + thump * 0.8
        sig = np.clip(sig / np.max(np.abs(sig)) * 0.9, -1, 1)
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(sr)
            w.writeframes((sig * 32767).astype(np.int16).tobytes())
    cues = []
    for i, g in enumerate(groups):
        acc = 0
        for item in g.items:
            if isinstance(item, Clip):
                for shot, at in ():
                    if item.shot == shot and item.a <= at < item.b:
                        speed = max(1 / 1.35, item.natural / item.dur)
                        cues.append((path, starts[i] + acc + (at - item.a) / speed))
            acc += item.dur
    return cues


if __name__ == "__main__":
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer151/mix.wav")
    audio_track(wav)
    print("audio mixed", flush=True)
    enc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H),
                            "-r", str(FPS), "-i", "-", "-i", wav, "-map", "0:v", "-map", "1:a", "-c:v", "libx264", "-crf", "20",
                            "-preset", "medium", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest",
                            "-movflags", "+faststart", OUT], stdin=subprocess.PIPE)
    nframes = int(total * FPS)
    preview = len(sys.argv) > 3 and sys.argv[3] == "preview"
    for fi in range(nframes):
        img = frame_at(fi / FPS)
        if preview and fi % (FPS * 6) == 0:
            img.resize((640, 360)).save(os.path.join(ROOT, "build/trailer151/p_%04d.jpg" % (fi // FPS)))
        enc.stdin.write(img.tobytes())
        if fi % (FPS * 20) == 0:
            print("frame", fi, "/", nframes, flush=True)
    enc.stdin.close()
    enc.wait()
    print("wrote", OUT)

#!/usr/bin/env python3
"""VoxelCraft: the 6-minute English trailer (1920x1080, narrated).

Inputs: build/film/<shot>/f_*.jpg (real gameplay filmed by tests/film/trailer_film.cpp),
build/trailer6/vo/*.wav (Piper narration, lines in build/trailer6/lines.txt), the game's own
music (menu theme, Overworld song, biome pieces). Output: dist/VoxelCraft-trailer-6min-en.mp4
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
VO = os.path.join(ROOT, "build/trailer6/vo")
MUSIC = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build/trailer6/music")
OUT = os.path.join(ROOT, "dist/VoxelCraft-trailer-6min-en.mp4")
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
groups = [
    # Intro and the story
    G("n01", [Card(intro, 4.2)], music="menu"),
    G("n02", [Card(text_card("s03_walk", [("A STORY", 40, DIM, False, 0.2), ("OF A DREAM", 96, GOLD, True, 0.5)]), 4.4)]),
    G("n03", [Card(text_card("s14_third_person", [("HOPPY_HOPSY21", 96, GOLD, True, 0.3), ("creator  ·  autistic  ·  dreamer", 40, WHITE, False, 1.0),
                                                   ("", 30, WHITE, False, 0), ("The dream: a voxel game on an engine of my own", 34, DIM, False, 2.6)], 0.4), 8.5)]),
    G("n04", [Card(text_card("s34_night_monsters", [("THE WALL", 96, RED, True, 0.3), ("Plan A: take a game apart and remake it.", 38, WHITE, False, 1.2),
                                                     ("Technophobia and autism made that road impossible.", 38, WHITE, False, 3.0)], 0.3), 8.3)]),
    G("n05", [Card(text_card("s16_structures", [("A NEW ROAD", 96, GREEN, True, 0.3), ("No PC. Just a phone, an idea, and Claude AI.", 40, WHITE, False, 1.4),
                                                 ("SDK  ·  NDK  ·  C++  ·  HPP  ·  Android Gradle", 34, DIM, False, 3.2),
                                                 ("Designed by Hoppy_Hopsy21, built together step by step", 34, DIM, False, 5.6)], 0.9), 11.8)]),
    G("n06", [Card(logo_card, 4.0)]),
    # 0.1 - 0.4
    G("n07", [C("s01_title", 0, 2.4), C("s02_new_world", 1.0, 6.8)], "A brand new engine", "endless worlds, chunk by chunk",
      "v0.1 – 0.4", ("THE BEGINNING", "Versions 0.1 – 0.4"), OW, music="overworld"),
    G("n08", [C("s03_walk", 0, 3.0), C("s04_chop", 0, 6.0)], "Gather", "chop trees, collect what falls", "v0.1 – 0.4"),
    G("n09", [C("s05_inventory", 0, 5.9), C("s06_crafting_table", 2.2, 7.3)], "Craft", "inventory, crafting table, tools", "v0.1 – 0.4"),
    G("n10", [C("s07_furnace", 1.4, 6.6)], "Smelt", "furnaces and fuel", "v0.1 – 0.4"),
    G("n11", [C("s08_build", 0, 4.2), C("s09_mine", 0, 4.5)], "Build & mine", "coal, iron, bronze, steel, ruby", "v0.1 – 0.4"),
    # 0.5 - 0.9
    G("n12", [C("s10_settings", 0.6, 6.6), C("s11_packs", 1.2, 4.7)], "Your world, your rules", "biomes, recipes and mod packs in JSON",
      "v0.5", ("MAKE IT YOURS", "Versions 0.5 – 0.9"), OW),
    G("n13", [C("s12_store", 0.3, 5.5), C("s13_skins", 0.6, 3.7), C("s14_third_person", 0, 4.0)], "The store", "coins earned by playing — never real money", "v0.6"),
    G("n14", [C("s15_creative", 6.2, 10.8), C("s16_structures", 0.3, 7.0)], "Creative & VCStructures", "fly, build, place whole buildings", "v0.7 – 0.9"),
    # 1.0 and its updates
    G("n15", [C("s17_languages", 0, 6.5)], "Five languages", "English · Čeština · Español · Русский · Deutsch",
      "v1.0", ("VERSION 1.0", "and the updates that followed"), OW),
    G("n16", [C("s18_doors_chest", 0.3, 6.9)], "Doors & chests", "a place to call home", "v1.0 builds"),
    G("n17", [C("s19_0_snowy_taiga", 0.2, 2.0), C("s19_1_desert", 0, 1.9), C("s19_2_mountains", 1.2, 3.2), C("s19_3_ice_plains", 0.4, 2.2),
              C("s19_4_ocean", 0, 2.0)], "New biomes", "snowy forests, deserts, mountains, ice, sea", "v1.0 builds"),
    G("n18", [C("s20_ruin", 0, 1.6), C("s21_shipwreck", 0, 1.6), C("s23_mineshaft", 1.4, 3.2)], "Structures", "ruins, shipwrecks, mineshafts", "v1.0 builds"),
    G("n19", [C("s24_boat", 1.0, 4.6)], "Boats", None, "v1.0 builds"),
    G("n20", [C("s25_chat", 0.4, 5.6), C("s26_lan", 6.2, 10.2)], "Chat & LAN multiplayer", "commands, and friends on your Wi-Fi", "v1.0 builds"),
    # 1.1
    G("n21", [C("s27_nether_portal", 0, 3.0), C("s27_nether_portal", 12.2, 14.4)], "Portals", "light it with flint and steel",
      "v1.1 Crimson Void", ("THE CRIMSON VOID", "Update 1.1"), NE, music="nether"),
    G("n22", [C("s28_nether_meadow", 1.5, 6.0)], "The Nether", "meadows, lava and fortresses", "v1.1 Crimson Void", accent=NE),
    G("n23", [C("s29_fortress_void_portal", 0, 4.6)], "Fortress portal", "the void flint and steel", "v1.1 Crimson Void", accent=NE),
    G("n24", [C("s30_void_forest", 0, 4.0), C("s31_crimson_desert", 0, 3.0)], "The Crimson Void", "eye trees and red sand", "v1.1 Crimson Void", accent=VD),
    G("n25", [C("s32_boss", 0, 10.8)], "The Crimson Man", "the boss of the arena", "v1.1 Crimson Void", accent=VD),
    G("n26", [C("s33_animals", 0, 4.0)], "Animals", "pigs and sheep: meat and wool", "v1.1 Crimson Void", accent=NE, music="overworld2"),
    G("n27", [C("s34_night_monsters", 1.0, 8.0)], "Night", "pumpkin slimes and forgotten men", "v1.1 Crimson Void", accent=NE),
    G("n28", [C("s35_bed", 0.8, 6.1)], "Beds", "sleep through the night", "v1.1 Crimson Void", accent=NE),
    G("n29", [C("s36_sky", 0, 6.5), C("s37_0_rain", 0, 3.0)], "Sky & weather", "morning, day, evening, night, rain", "v1.1 Crimson Void", accent=NE),
    # 1.2
    G("n30", [C("s39_powerstone", 0.4, 7.8)], "Powerstone", "cables, switches, lamps, iron doors",
      "v1.2", ("POWER AND TNT", "Update 1.2"), ST, music="forest"),
    G("n31", [C("s40_tnt", 0, 7.0)], "TNT", "light it… and run", "v1.2", accent=ST),
    G("n32", [C("s41_bucket", 0, 3.6), C("s42_saplings", 0, 6.7)], "Buckets & saplings", "a tree in twenty seconds", "v1.2", accent=ST),
    G("n33", [C("s43_eggs_swords", 0, 5.4)], "Spawn eggs & swords", "six materials", "v1.2", accent=ST),
    G("n34", [C("s44_void_tower", 0, 6.8), C("s45_temple", 0, 2.0), C("s47_watchtower", 0, 2.0)], "New structures",
      "void towers, the arena compass, temples, watchtowers", "v1.2", accent=ST),
    # the store
    G("n35", [C("s48_vip", 0, 5.0)], "VIP", "3000 earned coins", "v1.2 store", ("THE NEW STORE", "Update 1.2"), GOLD),
    G("n36", [C("s49_rewards_wheel", 0.5, 12.0)], "Daily rewards & wheel of fortune", "coins, skins, VIP, codes", "v1.2 store"),
    G("n37", [C("s50_code", 0.4, 4.2)], "Anniversary code", "unlocks the first-birthday pack", "v1.2 store"),
    G("n38", [C("s51_winter", 0, 6.8)], "Winter add-on", "snow everywhere, presents", "v1.2 add-ons", accent=(150, 210, 255)),
    G("n39", [C("s52_halloween", 0, 5.0), C("s53_nuke", 0, 1.4), C("s53_nuke", 7.6, 10.5)], "Halloween add-on",
      "red sky, fog, zombie hordes, nuke TNT", "v1.2 add-ons", accent=(255, 140, 60)),
    G("n40", [C("s54_anniversary", 0, 5.3)], "Anniversary pack", "cake and confetti", "v1.2 add-ons", accent=(255, 150, 220)),
    # a quick montage of everything
    G(None, [C("s30_void_forest", 4.0, 6.0), C("s32_boss", 6.0, 8.4), C("s40_tnt", 4.3, 6.3), C("s19_4_ocean", 1.6, 3.6),
             C("s51_winter", 4.2, 6.2), C("s26_lan", 8.0, 10.2), C("s36_sky", 6.0, 8.6), C("s16_structures", 4.4, 7.0)],
      "One world", "built from scratch", "VoxelCraft 1.2.0", accent=GOLD),
    # the engine, the future, the end
    G("n41", [Card(engine_card, 11.0)], music="menu2"),
    G("n42", [Card(text_card("s19_2_mountains", [("libvoxelcraft.so", 88, GREEN, True, 0.4),
                                                  ("Many mobile games run on a native library.", 38, WHITE, False, 1.4),
                                                  ("VoxelCraft runs on its own. Made here, not taken.", 38, WHITE, False, 4.4)], 0.5), 9.8)]),
    G("n43", [Card(disclaimer_card, 8.6)]),
    G("n44", [Card(text_card("s26_lan", [("WHAT'S NEXT", 88, GOLD, True, 0.3), ("Google Play release", 44, WHITE, False, 1.6),
                                          ("Java mods for players who could never play them", 40, WHITE, False, 4.0)], 0.9), 8.9)]),
    G("n45", [Card(text_card("s36_sky", [("MADE WITH LOVE", 96, GOLD, True, 0.3), ("by and for people with autism", 44, WHITE, False, 1.2)], 0.2), 4.6)]),
    G("n46", [Card(end_card, 10.5)], pad=6.0),
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
                for shot, at in (("s40_tnt", 4.75), ("s53_nuke", 8.3)):
                    if item.shot == shot and item.a <= at < item.b:
                        speed = max(1 / 1.35, item.natural / item.dur)
                        cues.append((path, starts[i] + acc + (at - item.a) / speed))
            acc += item.dur
    return cues


if __name__ == "__main__":
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer6/mix.wav")
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
            img.resize((640, 360)).save(os.path.join(ROOT, "build/trailer6/p_%04d.jpg" % (fi // FPS)))
        enc.stdin.write(img.tobytes())
        if fi % (FPS * 20) == 0:
            print("frame", fi, "/", nframes, flush=True)
    enc.stdin.close()
    enc.wait()
    print("wrote", OUT)

#!/usr/bin/env python3
"""VoxelCraft 1.7.0 preview: a short narrated first look (1920x1080).

Reuses the editing kit of tools/trailer155.py. Footage: build/film17/<shot>/f_*.jpg (the new
menu, the boss and his heads, the Celestial portal, the concept islands), narration
build/trailer17/vo/*.wav. Output: dist/VoxelCraft-1.7.0-preview-en.mp4
"""
import os
import subprocess
import sys

sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.FILM = os.path.join(ROOT, "build/film17")
k.VO = os.path.join(ROOT, "build/trailer17/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-1.7.0-preview-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
SKY, CRIMSON = (170, 215, 255), (255, 110, 110)
B = "1.7.0 preview"


def open_card(t, dur):
    f = k.blurred("q01_reach_fly", t, 0.2).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 420), "VOXELCRAFT 1.7.0", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 550), "A FIRST LOOK", 64, SKY, min(a, ease((t - 0.9) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def concept_card(t, dur):
    return k.text_card("q01_reach_fly", [
        ("EARLY CONCEPT", 64, GOLD, True, 0.0),
        ("", 16, WHITE, False, 0),
        ("Built by hand in a flat world to show the idea", 36, WHITE, False, 0.5),
        ("Islands, trees, chains and ruins can still change", 36, DIM, False, 1.0),
    ], 0.5)(t, dur)


def end_card(t, dur):
    f = k.blurred("q01_reach_fly", t, 0.85).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 360), "VOXELCRAFT 1.7.0", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 490), "THE CELESTIAL REACH", 62, SKY, ease((t - 1.0) / 0.7), anchor="mm")
    put(d, (W // 2, 600), "COMING SOON", 46, WHITE, ease((t - 1.8) / 0.7), anchor="mm")
    put(d, (W // 2, 700), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 36, GREEN, ease((t - 2.5) / 0.8), anchor="mm")
    f.alpha_composite(over)
    f = f.convert("RGB")
    return Image.blend(f, Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.4) / 1.4))


groups = [
    G("t01", [C("s01_title", 0.5, 4.5)], music="menu"),
    G("t02", [Card(open_card, 4.5)]),
    G("t03", [C("s01_title", 2.0)], "New menus", "rounded buttons and icons", B, ("NEW LOOK", "the menus rebuilt"), GOLD),
    G("t04", [C("q03_boss_heads")], "The Crimson Man", "freezes, shakes, explodes  ·  twelve heads", B, ("THE KEY", "twelve heads"), CRIMSON,
      music="nether"),
    G("t05", [C("p01_celestial_portal")], "The Celestial portal", "one head on each of the twelve frame stones", B, accent=SKY),
    G("t06", [C("q01_reach_fly")], "The Celestial Reach", "floating islands of soul grass and stone", B, ("A NEW DIMENSION", "high above"),
      SKY, music="overworld2"),
    G("t07", [C("q02_reach_walk")], "New trees and ruins", "sky leaves  ·  glowing fruit  ·  a lost kingdom", B, accent=SKY),
    G("t08", [Card(concept_card, 5.0)]),
    G("t09", [Card(end_card, 7.0)], pad=3.0),
]

k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)

if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "build/trailer17"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer17/mix.wav")
    k.audio_track(wav)
    print("audio mixed", flush=True)
    enc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H),
                            "-r", str(k.FPS), "-i", "-", "-i", wav, "-map", "0:v", "-map", "1:a", "-c:v", "libx264", "-crf", "20",
                            "-preset", "veryfast", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest",
                            "-movflags", "+faststart", OUT], stdin=subprocess.PIPE)
    n = int(k.total * k.FPS)
    for fi in range(n):
        enc.stdin.write(k.frame_at(fi / k.FPS).tobytes())
    enc.stdin.close()
    enc.wait()
    print("wrote", OUT)

#!/usr/bin/env python3
"""VoxelCraft 1.5.7 build 2: the update trailer (1920x1080, narrated).

Reuses the editing kit of tools/trailer155.py (clips, cards, captions, the audio mix) with the
1.5.7 footage: build/film/j*_*/f_*.jpg, narration build/trailer157/vo/*.wav.
Output: dist/VoxelCraft-1.5.7-trailer-en.mp4
"""
import os
import subprocess
import sys

sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.VO = os.path.join(ROOT, "build/trailer157/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-1.5.7-trailer-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
FIRE, STONE, CRIMSON, POWER, PAPER = (255, 170, 80), (200, 210, 225), (255, 110, 110), (110, 220, 255), (240, 220, 170)
B = "v1.5.7"


def update_card(t, dur):
    f = k.blurred("j05_caves", t, 0.4).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 420), "VOXELCRAFT 1.5.7", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 550), "BUILD 2", 70, (255, 235, 170), min(a, ease((t - 0.9) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def features_card(t, dur):
    return k.text_card("j01_torches", [
        ("ALSO IN 1.5.7", 60, GOLD, True, 0.0),
        ("", 16, WHITE, False, 0),
        ("Bigger Nether fortress  ·  explorer maps (experiment)", 36, WHITE, False, 0.5),
        ("Redesigned ores  ·  easier powerstone lamp", 36, WHITE, False, 0.9),
        ("New Graphics settings tab  ·  your shadow on the ground", 34, DIM, False, 1.3),
        ("Battery saver  ·  smaller app  ·  compact world saves", 34, GREEN, False, 1.8),
    ], 0.5)(t, dur)


def end157(t, dur):
    f = k.blurred("j03_flowing", t, 0.8).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 360), "VOXELCRAFT 1.5.7", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 490), "BUILD 2", 58, (255, 235, 170), ease((t - 1.0) / 0.7), anchor="mm")
    put(d, (W // 2, 620), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 40, GREEN, ease((t - 2.0) / 0.8), anchor="mm")
    put(d, (W // 2, 690), "Android  ·  Next: the Powerstone Apple", 30, (220, 220, 220), ease((t - 2.6) / 0.8), anchor="mm", bold=False)
    f.alpha_composite(over)
    f = f.convert("RGB")
    return Image.blend(f, Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.4) / 1.4))


groups = [
    G("r01", [C("s00_splash", 0.2, 3.6)], music="overworld"),
    G("r02", [Card(update_card, 4.6)]),
    G("r03", [C("j01_torches")], "Torches", "in your hand, in your off hand, on the wall", B, ("TORCHES", "light your way"), FIRE),
    G("r04", [C("j02_shield")], "Shield", "crouch to raise it", B, accent=STONE),
    G("r05", [C("j03_flowing")], "Flowing water and lava", "experiment", B, ("FLOWING LIQUIDS", "an experimental feature"), POWER),
    G("r06", [C("j04_falling")], "Falling sand and gravel", None, B, accent=PAPER),
    G("r07", [C("j05_caves")], "Bigger caves", "with openings to the surface", B, ("CAVES", "deeper and wider"), FIRE),
    G("r08", [C("j06_crimson_mineshaft")], "Crimson mineshaft", "the Cornerstone Dimension", B, accent=CRIMSON, music="nether"),
    G("r09", [C("j07_hogman"), C("k01_fortress", 0.0, 3.0)], "The Hogman", "pay him in powerstone", B,
      ("THE HOGMAN", "friend or foe?"), CRIMSON),
    G("r10", [C("j08_powerstone")], "Powerstone furnace and cables", "up walls, across ceilings", B, accent=POWER),
    G("r11", [C("j09_world_map")], "World map", "zoom, pan and name places", B, ("THE WORLD MAP", "your whole world"), PAPER, music="forest"),
    G("r12", [C("j10_chat"), C("j11_book")], "Chat and book and quill", None, B, accent=PAPER),
    G("r13", [C("j12_credits", 0.0, 5.0)], "Credits and trailers", "tap VoxelSoftworks on the title", B, accent=GOLD),
    G("r14", [Card(features_card, 6.0), Card(end157, 7.0)], pad=3.0),
]

k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)

if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "build/trailer157"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer157/mix.wav")
    k.audio_track(wav)
    print("audio mixed", flush=True)
    enc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H),
                            "-r", str(k.FPS), "-i", "-", "-i", wav, "-map", "0:v", "-map", "1:a", "-c:v", "libx264", "-crf", "20",
                            "-preset", "medium", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest",
                            "-movflags", "+faststart", OUT], stdin=subprocess.PIPE)
    n = int(k.total * k.FPS)
    for fi in range(n):
        img = k.frame_at(fi / k.FPS)
        if fi % (k.FPS * 6) == 0:
            img.resize((640, 360)).save(os.path.join(ROOT, "build/trailer157/p_%04d.jpg" % (fi // k.FPS)))
        enc.stdin.write(img.tobytes())
    enc.stdin.close()
    enc.wait()
    print("wrote", OUT)

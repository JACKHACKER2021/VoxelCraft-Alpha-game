#!/usr/bin/env python3
"""VoxelCraft 1.6.0 Caves and Tunes: the update trailer (1920x1080, narrated).

Reuses the editing kit of tools/trailer155.py (clips, cards, captions, the audio mix) with the
1.5.7 footage: build/film/j*_*/f_*.jpg, narration build/trailer160/vo/*.wav.
Output: dist/VoxelCraft-1.6.0-trailer-en.mp4
"""
import os
import subprocess
import sys

sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.VO = os.path.join(ROOT, "build/trailer160/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-1.6.0-trailer-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
FIRE, STONE, CRIMSON, POWER, PAPER = (255, 170, 80), (200, 210, 225), (255, 110, 110), (110, 220, 255), (240, 220, 170)
B = "v1.6.0"
CAVE, MOSS, LAVAC = (150, 200, 255), (140, 220, 120), (255, 150, 70)


def update_card(t, dur):
    f = k.blurred("m06_shaft", t, 0.7).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 420), "VOXELCRAFT 1.6.0", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 550), "CAVES AND TUNES", 70, (190, 230, 255), min(a, ease((t - 0.9) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def features_card(t, dur):
    return k.text_card("m09_blocks", [
        ("ALSO IN 1.6.0", 60, GOLD, True, 0.0),
        ("", 16, WHITE, False, 0),
        ("Deepstone bricks  ·  rotten planks  ·  moss  ·  vines  ·  new gravel", 34, WHITE, False, 0.5),
        ("Hardium pickaxe  ·  ores showing in cave walls", 34, WHITE, False, 0.9),
        ("New songs: Crimson Void, Cornerstone Dimension, caves", 34, DIM, False, 1.3),
        ("Celestial Reach portal frames  ·  coming in 1.7.0", 34, GREEN, False, 1.8),
    ], 0.5)(t, dur)


def end160(t, dur):
    f = k.blurred("m01_mossy_cave", t, 0.6).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 360), "VOXELCRAFT 1.6.0", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 490), "CAVES AND TUNES", 58, (190, 230, 255), ease((t - 1.0) / 0.7), anchor="mm")
    put(d, (W // 2, 620), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 40, GREEN, ease((t - 2.0) / 0.8), anchor="mm")
    put(d, (W // 2, 690), "Android  ·  Next: 1.7.0 and the Celestial Reach", 30, (220, 220, 220), ease((t - 2.6) / 0.8), anchor="mm", bold=False)
    f.alpha_composite(over)
    f = f.convert("RGB")
    return Image.blend(f, Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.4) / 1.4))


groups = [
    G("r01", [C("s00_splash", 0.2, 3.6)], music="overworld"),
    G("r02", [Card(update_card, 4.6)]),
    G("r03", [C("m06_shaft")], "Bigger caves", "shafts open into deep halls", B, ("CAVES", "bigger and deeper"), CAVE, music="forest"),
    G("r04", [C("m01_mossy_cave")], "Mossy caves", "vines to climb, glowing fungus", B, accent=MOSS),
    G("r05", [C("m02_lava_lake")], "Lava pools and lakes", None, B, accent=LAVAC),
    G("r06", [C("m03_tar_lake")], "Tar lakes", "and the tar monster", B, accent=DIM),
    G("r07", [C("m04_hardium")], "Deepstone and hardium", "deeper and harder to dig", B, ("DEEPSTONE", "the deep layer"), CAVE),
    G("r08", [C("m05_dungeon")], "Cave dungeons", "spawners and treasure", B, accent=CRIMSON),
    G("r09", [C("m07_boss")], "The Crimson Man", "break the eyes that heal him", B, ("BOSS FIGHT", "a harder encounter"), CRIMSON, music="nether"),
    G("r10", [C("m08_forgotten")], "The Forgotten Overworld", "a new Crimson Void biome", B, accent=PAPER),
    G("r11", [C("m09_blocks"), Card(features_card, 5.0)], "New blocks", None, B, accent=GOLD, music="overworld2"),
    G("r12", [Card(end160, 7.0)], pad=3.0),
]

k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)

if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "build/trailer160"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer160/mix.wav")
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
            img.resize((640, 360)).save(os.path.join(ROOT, "build/trailer160/p_%04d.jpg" % (fi // k.FPS)))
        enc.stdin.write(img.tobytes())
    enc.stdin.close()
    enc.wait()
    print("wrote", OUT)

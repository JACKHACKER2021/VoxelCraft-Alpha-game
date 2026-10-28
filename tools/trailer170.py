#!/usr/bin/env python3
"""VoxelCraft 1.7.0 the Celestial Reach: the update trailer (1920x1080, narrated), with a
teaser of 1.8.0 Concrete and Old World at the end.

Reuses the editing kit of tools/trailer155.py. Footage: build/film17/<shot>/f_*.jpg, narration
build/trailer170/vo/*.wav, the 1.8.0 preview pictures build/trailer170/teaser180_*.jpg.
Output: dist/VoxelCraft-1.7.0-trailer-en.mp4
"""
import os
import subprocess
import sys

sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.FILM = os.path.join(ROOT, "build/film17")
k.VO = os.path.join(ROOT, "build/trailer170/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-1.7.0-trailer-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
SKY, CRIMSON, ICE, CAVE = (170, 215, 255), (255, 110, 110), (120, 220, 240), (150, 200, 255)
B = "v1.7.0"


def update_card(t, dur):
    f = k.blurred("y01_reach_fly", t, 0.3).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 420), "VOXELCRAFT 1.7.0", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 550), "THE CELESTIAL REACH", 70, SKY, min(a, ease((t - 0.9) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def out_now(t, dur):
    f = k.blurred("y01_reach_fly", t, 0.85).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 380), "VOXELCRAFT 1.7.0", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 510), "THE CELESTIAL REACH", 62, SKY, ease((t - 0.9) / 0.7), anchor="mm")
    put(d, (W // 2, 620), "OUT NOW", 50, WHITE, ease((t - 1.6) / 0.7), anchor="mm")
    put(d, (W // 2, 720), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 36, GREEN, ease((t - 2.2) / 0.8), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


_teaser = None


def teaser(t, dur):
    """1.8.0 Concrete and Old World: the creator's two preview pictures, then the title."""
    global _teaser
    if _teaser is None:
        _teaser = [Image.open(os.path.join(ROOT, "build/trailer170", n)).convert("RGB") for n in ("teaser180_a.jpg", "teaser180_b.jpg")]
    f = Image.new("RGBA", (W, H), (8, 8, 12, 255))
    half = dur / 2
    i = 0 if t < half else 1
    lt = t - i * half
    im = _teaser[i]
    sc = min((W - 160) / im.width, (H - 260) / im.height) * (1 + 0.04 * lt / half)
    r = im.resize((int(im.width * sc), int(im.height * sc)))
    a = min(ease(lt / 0.6), ease((half - lt) / 0.5))
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    layer.paste(r, (W // 2 - r.width // 2, H // 2 - 50 - r.height // 2))
    layer.putalpha(layer.split()[3].point(lambda v: int(v * a)))
    f.alpha_composite(layer)
    d = ImageDraw.Draw(f)
    put(d, (W // 2, H - 110), "NEXT: VOXELCRAFT 1.8.0", 56, GOLD, ease((t - 0.4) / 0.7) * ease((dur - t) / 0.6), anchor="mm")
    put(d, (W // 2, H - 50), "Concrete & Old World  ·  preview pictures, not final", 34, WHITE,
        ease((t - 0.9) / 0.7) * ease((dur - t) / 0.6), anchor="mm", bold=False)
    return Image.blend(f.convert("RGB"), Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.2) / 1.2))


groups = [
    G("u01", [C("s01_title", 0.5, 4.0)], music="menu"),
    G("u02", [Card(update_card, 4.6)]),
    G("u03", [C("y04_ui"), C("s01_title", 2.0)], "A new look", "rounded menus  ·  health and hunger bars", B, ("NEW LOOK", "every menu rebuilt"), GOLD),
    G("u04", [C("y05_creative")], "Creative inventory", "armour and off hand beside you", B, accent=GOLD),
    G("u05", [C("x02_hardium")], "Hardium set", "sword  ·  axe  ·  shovel  ·  armour", B, ("HARDIUM", "the strongest gear"), ICE, music="caves"),
    G("u06", [C("x01_gen_check")], "Deepstone", "a 40-block deep layer", B, accent=CAVE),
    G("u07", [C("y03_blocks"), C("x05_crimson_log"), C("x03_ice")], "New blocks", "crimson and deepstone stairs and slabs  ·  solid ice", B, accent=GOLD),
    G("u08", [C("p02_mob_deaths"), C("q03_boss_heads", 2.5)], "The Crimson Man", "a death worth watching", B, ("THE BOSS", "falls"), CRIMSON,
      music="nether"),
    G("u09", [C("q03_boss_heads", 8.5), C("p01_celestial_portal")], "Twelve heads", "one on each stone of the frame", B, accent=SKY),
    G("u10", [C("y01_reach_fly")], "The Celestial Reach", "floating islands over a white sky", B, ("A NEW DIMENSION", "above the clouds"), SKY,
      music="celestial"),
    G("u11", [C("y02_reach_walk")], "Nothing below", "a fall is a fall", B, accent=SKY),
    G("u12", [C("y01_reach_fly", 5.0)], "New songs", "Celestial Exploration  ·  Echoes of Caves", B, accent=SKY),
    G("u13", [Card(out_now, 6.0)], pad=2.0),
    G("u14", [Card(teaser, 10.0)], pad=2.0),
]

k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)

if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "build/trailer170"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer170/mix.wav")
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

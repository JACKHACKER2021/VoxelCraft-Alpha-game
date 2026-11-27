#!/usr/bin/env python3
"""VoxelCraft 3.0.0: the trailer of the road from 2.0.0 to 3.0.0 (1920x1080, narrated by Piper's Joe).

Built with the editing kit of tools/trailer155.py, like the 1.9.0 trailer: every version as a chapter with
its badge, the green menu, the add-on studio and the Creator, the new creatures, the VoxelSoftworks seat,
and the end: 2.0.0 blown up, 3.0.0 with fireworks.
Footage: <film dir>/<shot>/f_*.png (tests/film/trailer_film.cpp: a21_import, b07_train, b13_drill,
c05_corner_biomes, c06_frost, c11_open, z01_*, z06_*, z07_start, z11_collar, z12_bag*, t00_intro,
t30_*, t31_*). Narration: build/trailer300/vo/w*.wav (piper --model en_US-joe-medium, lines.txt).
Run: python3 tools/trailer300.py <repo root> <music dir> <film dir>
Output: trailers/t14_3.0.0.mp4
"""
import os
import subprocess
import sys

FILM_DIR = sys.argv[3] if len(sys.argv) > 3 else "build/film30"
sys.argv = sys.argv[:1] + sys.argv[1:3]
# The kit of the 1.5.5 trailer, without its own list of groups (it would look for that trailer's footage).
import types  # noqa: E402
_kit = os.path.join(os.path.dirname(os.path.abspath(__file__)), "trailer155.py")
_lines = open(_kit, encoding="utf-8").read().split("\n")
_a = next(i for i, l in enumerate(_lines) if l.startswith("groups = ["))
_b = next(i for i, l in enumerate(_lines) if l.startswith('def frame_at')) - 1
k = types.ModuleType("trailer155")
k.__file__ = _kit
exec(compile("\n".join(_lines[:_a] + [""] * (_b + 1 - _a) + _lines[_b + 1:]), _kit, "exec"), k.__dict__)

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.FILM = FILM_DIR
k.VO = os.path.join(ROOT, "build/trailer300/vo")
OUT = os.path.join(ROOT, "trailers/t14_3.0.0.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
SKY, ORANGE, CRIMSON, VIOLET = (170, 215, 255), (255, 160, 70), (255, 110, 110), (190, 140, 255)


def frames_of(shot):  # our film writes PNG frames
    if shot not in k._frames:
        d = os.path.join(k.FILM, shot)
        k._frames[shot] = [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.endswith((".png", ".jpg"))]
    return k._frames[shot]


k.frames_of = frames_of


def road_card(t, dur):
    f = k.blurred("t30_hq", t, 0.4).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 400), "VOXELCRAFT", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 540), "2.0.0  >  3.0.0", 84, WHITE, min(a, ease((t - 0.8) / 0.6)), anchor="mm")
    put(d, (W // 2, 640), "THE ROAD TO 3.0.0", 46, DIM, min(a, ease((t - 1.4) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def out_now(t, dur):
    f = k.blurred("t30_finale", t, 0.9).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 360), "VOXELCRAFT 3.0.0", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 490), "NO IRON  ·  THE CORNERSTONE WHOLLY ITS OWN", 50, WHITE, ease((t - 0.9) / 0.7), anchor="mm")
    put(d, (W // 2, 600), "OUT NOW", 56, GREEN, ease((t - 1.6) / 0.7), anchor="mm")
    put(d, (W // 2, 700), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 36, DIM, ease((t - 2.2) / 0.8), anchor="mm")
    f.alpha_composite(over)
    return Image.blend(f.convert("RGB"), Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.2) / 1.2))


groups = [
    G(None, [C("t00_intro", 0.2, 6.2)], music="menu"),
    G("w01", [Card(road_card, 5.0)], music="overworld"),
    G("w02", [C("a21_import")], "Bedrock world import", ".mcworld files open as VoxelCraft worlds", "v2.0.0", ("2.0.0", "the whole story"), GOLD),
    G("w03", [C("b07_train", 0, 5), C("b13_drill", 0, 4)], "Trains and the drill car", "3D rails, crossings, the Bronze update", "v2.1.0",
      ("2.1.0", "trains & bronze"), ORANGE),
    G("w04", [C("c05_corner_biomes"), C("c06_frost")], "Cornerstone biomes and frost", "Gilded Quarry, Prismatide Coast, thin ice", "v2.2.0",
      ("2.2.0", "cornerstone & frost"), CRIMSON),
    G("w05", [C("c11_open")], "The subway", "sliding doors and night lights", "v2.3.0", ("2.3.0", "the subway"), SKY),
    G("w06", [C("z01_loop"), C("z01_hub"), C("z01_tunnel"), C("z01_terminus")], "Stations and the underground", "platforms, a subway network, the lost city",
      "v2.4.0 - 2.5.0", ("2.4.0 - 2.5.0", "metro & underground"), SKY, music="caves"),
    G("w07", [C("z06_golem_idle"), C("z06_fox_idle"), C("z07_start")], "Cornerstone creatures and winter", "golem, fox, snow on every leaf", "v2.6.0",
      ("2.6.0", "cornerstone & holidays"), CRIMSON, music="overworld"),
    G("w08", [C("z12_bag2"), C("z11_collar"), C("t31_menu", 4.6)], "Backpacks, the collar, the green menu", "six tiers, worn on your back", "v2.7.0",
      ("2.7.0", "backpacks & add-ons"), GREEN),
    G("w09", [C("t31_addons")], "The add-on studio", "add-ons  ·  behavior  ·  resource  ·  store", "v2.7.0", accent=GREEN),
    G("w10", [C("t31_lineup")], "The VC language", "every creature is a GLB model", "v2.8.0", ("2.8.0", "the VC language"), VIOLET, music="celestial"),
    G("w11", [C("t30_mobs")], "New creatures", "pig, sheep, chicken, cow and ghost", "v2.8.0", accent=VIOLET),
    G("w12", [C("t31_creator")], "The Creator", "blocks  ·  items  ·  mobs  ·  models  ·  recipes  ·  shaders", "v2.8.0", accent=VIOLET),
    G("w13", [C("t30_hq")], "The VoxelSoftworks seat", "the HQ and the developer's house", "v3.0.0", ("3.0.0", "no iron, the cornerstone its own"), GOLD,
      music="overworld"),
    G("w14", [C("t30_finale", 0, 11.0)], "Goodbye, 2.0.0", "", "v3.0.0", accent=GOLD),
    G("w15", [C("t30_finale", 11.0)], "Welcome to 3.0.0", "", "v3.0.0", accent=GOLD),
    G(None, [Card(out_now, 6.0)], pad=2.0),
]
k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)


_kit_boom = k.boom_cues


def boom_cues():
    """The explosions of 2.0.0 in the finale (seconds into the shot: the fuse, then the blasts along it)."""
    k_cues = []
    orig = _kit_boom
    path = os.path.join(ROOT, "build/trailer6/boom.wav")
    if not os.path.exists(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        k.groups, saved = [], k.groups
        orig()
        k.groups = saved
    for i, g in enumerate(groups):
        if g.vo == "w14":
            item = g.items[0]
            speed = max(1 / 1.35, item.natural / item.dur)
            for at in (6.4, 7.0, 7.6, 8.2, 8.8, 9.4):
                k_cues.append((path, starts[i] + at / speed))
    return k_cues


k.boom_cues = boom_cues

if __name__ == "__main__":
    wav = os.path.join(ROOT, "build/trailer300/mix.wav")
    k.audio_track(wav)
    print("audio mixed", flush=True)
    enc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H),
                            "-r", str(k.FPS), "-i", "-", "-i", wav, "-map", "0:v", "-map", "1:a", "-c:v", "libx264", "-crf", "21",
                            "-preset", "veryfast", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest",
                            "-movflags", "+faststart", OUT], stdin=subprocess.PIPE)
    n = int(k.total * k.FPS)
    for fi in range(n):
        enc.stdin.write(k.frame_at(fi / k.FPS).tobytes())
    enc.stdin.close()
    enc.wait()
    print("wrote", OUT)

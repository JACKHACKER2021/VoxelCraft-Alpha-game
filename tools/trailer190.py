#!/usr/bin/env python3
"""VoxelCraft 1.9.0 Celestial Reach & Seasons: the update trailer (1920x1080, narrated).

It opens the way the game does (falling blocks, "VoxelSoftworks presents", the version screen),
shows every new thing of the update and ends with a look ahead to 1.9.0.
Reuses the editing kit of tools/trailer155.py. Footage: build/film17/v*/f_*.jpg (tests/film,
the v00..v13 shots), narration build/trailer190/vo/w*.wav (Piper).
Output: dist/VoxelCraft-1.9.0-trailer-en.mp4
"""
import os
import subprocess
import sys

sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.FILM = os.path.join(ROOT, "build/film17")
k.VO = os.path.join(ROOT, "build/trailer190/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-1.9.0-trailer-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
GRAY, ORANGE, SKY, CRIMSON, VIOLET = (200, 200, 200), (255, 160, 70), (170, 215, 255), (255, 110, 110), (190, 140, 255)
B = "v1.9.0"


_logo = None
_edge = None


def logo_intro(t, dur):
    """The new VoxelSoftworks logo, put together: the voxel cube assembles from its pieces flying in,
    the name is revealed from left to right, then "presents"."""
    global _logo
    if _logo is None:
        im = Image.open(os.path.join(ROOT, "build/trailer180/logo.jpg")).convert("RGB")
        sc = 1500 / im.width
        _logo = im.resize((int(im.width * sc), int(im.height * sc)), Image.LANCZOS)
        # Drop the picture's dark grey background (and its faint grid) to pure black.
        _logo = _logo.point(lambda v: 0 if v < 46 else int((v - 46) * 255 / 209))
        # A soft edge, so the picture's lighter background melts into the black frame.
        global _edge
        _edge = Image.new("L", _logo.size, 0)
        ed = ImageDraw.Draw(_edge)
        for i in range(140):
            ed.rectangle((i, i, _logo.width - 1 - i, _logo.height - 1 - i), fill=int(255 * (i / 139) ** 1.5))
    lw, lh = _logo.size
    ox, oy = (W - lw) // 2, (H - lh) // 2 - 40
    f = Image.new("RGB", (W, H), (0, 0, 0))
    fade = ease((dur - t) / 0.7)
    cube_x1 = int(lw * 0.40)  # the cube and its flying bits are the left 40 % of the picture
    # The cube: tiles of 30 px fly in from scattered places, the farther ones a little later.
    n = 30
    import random
    rnd = random.Random(7)
    for ty in range(0, lh, n):
        for tx in range(0, cube_x1, n):
            delay = rnd.uniform(0.0, 0.9)
            k = ease((t - 0.2 - delay) / 0.8)
            if k <= 0:
                continue
            piece = _logo.crop((tx, ty, min(tx + n, cube_x1), min(ty + n, lh)))
            sx, sy = rnd.uniform(-500, 500), rnd.uniform(-420, 420)
            x = int(ox + tx + (1 - k) * sx)
            y = int(oy + ty + (1 - k) * sy)
            mask = _edge.crop((tx, ty, tx + piece.width, ty + piece.height)).point(lambda v: int(v * k * fade))
            f.paste(piece, (x, y), mask)
    # The name: a left-to-right wipe once the cube stands.
    w = ease((t - 1.6) / 0.9)
    if w > 0:
        cut = cube_x1 + int((lw - cube_x1) * w)
        part = _logo.crop((cube_x1, 0, cut, lh))
        f.paste(part, (ox + cube_x1, oy), _edge.crop((cube_x1, 0, cut, lh)).point(lambda v: int(v * fade)))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, oy + lh + 40), "presents", 44, DIM, min(ease((t - 2.8) / 0.8), fade), anchor="mm", bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return f.convert("RGB")


def update_card(t, dur):
    f = k.blurred("b01_reach", t, 0.3).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 420), "VOXELCRAFT 1.9.0", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 550), "CELESTIAL REACH & SEASONS", 70, WHITE, min(a, ease((t - 0.9) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def out_now(t, dur):
    f = k.blurred("t01_autumn", t, 0.85).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 380), "VOXELCRAFT 1.9.0", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 510), "CELESTIAL REACH & SEASONS", 62, WHITE, ease((t - 0.9) / 0.7), anchor="mm")
    put(d, (W // 2, 620), "OUT NOW", 50, GREEN, ease((t - 1.6) / 0.7), anchor="mm")
    put(d, (W // 2, 720), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 36, DIM, ease((t - 2.2) / 0.8), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def next_card(t, dur):
    f = k.blurred("t05_newyear", t, 0.8).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.7), ease((dur - t) / 0.8))
    put(d, (W // 2, 400), "NEXT: VOXELCRAFT 2.0.0", 90, SKY, a, anchor="mm")
    put(d, (W // 2, 500), "TRY IT NOW IN THE EXPERIMENTS", 56, WHITE, min(a, ease((t - 0.7) / 0.6)), anchor="mm")
    put(d, (W // 2, 600), "thunderstorms  ·  shooting stars  ·  animal breeding", 40, DIM, min(a, ease((t - 1.3) / 0.6)), anchor="mm", bold=False)
    f.alpha_composite(over)
    return Image.blend(f.convert("RGB"), Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.2) / 1.2))


groups = [
    G(None, [Card(logo_intro, 5.5)]),
    G(None, [C("t00_intro", 4.6, 10.6)]),
    G("w01", [Card(update_card, 4.6)], music="menu"),
    G("w02", [C("b01_reach")], "Celestial Reach", "white and blue stone  ·  crystals  ·  bridge remnants", B, ("NEW", "the sky islands"), SKY, music="celestial"),
    G("w03", [C("c01_sky_animals", 0, 2.5)], "Low gravity", "45 % in the Reach", B, accent=SKY),
    G("w04", [C("b02_reach_under")], "Crystal spikes", "under every island", B, accent=SKY),
    G("w05", [C("b05_celestium")], "Celestium", "pickaxe, sword, axe, shovel and chestplate", B, ("CELESTIUM", "new ore"), GOLD),
    G("w06", [C("c01_sky_animals", 2.0)], "Celestial pigs and sheep", "on the sky islands", B, accent=SKY),
    G("w07", [C("b03_campfire")], "Fire and the campfire", "light any block with flint and steel", B, ("FIRE", "it spreads"), ORANGE, music="overworld"),
    G("w08", [C("c02_water")], "Flowing water and lava", "in every world, with sloping surfaces", B, accent=SKY),
    G("w09", [C("t06_falling")], "Smooth falling blocks", "sand, gravel and concrete", B, accent=GOLD),
    G("w10", [C("t01_autumn"), C("b04_leaves", 0, 2.5)], "Autumn and Halloween", "1 October - 30 November", B, ("SEASONS", "by your calendar"), ORANGE, music="forest"),
    G("w11", [C("t03_christmas"), C("t04_christmas_plains", 0, 3)], "Christmas", "20 - 29 December", B, accent=WHITE),
    G("w12", [C("t05_newyear")], "New Year's Eve", "30 December - 2 January", B, accent=VIOLET),
    G("w13", [C("t07_flat_desert", 0, 2), C("t08_flat_snow", 0, 2), C("t09_flat_nether", 0, 2), C("t10_flat_void", 0, 2)], "Flat world looks",
      "classic, desert, snow, stone, nether, void", B, accent=GOLD, music="menu2"),
    G("w14", [C("c03_howto")], "How to craft?", "every recipe, the furnace and the cutters", B, ("NEW", "in the inventory"), GREEN),
    G(None, [Card(out_now, 6.0)], pad=2.0),
    G(None, [Card(next_card, 5.0)], pad=1.0),
]
k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)

if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "build/trailer190"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer190/mix.wav")
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

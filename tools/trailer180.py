#!/usr/bin/env python3
"""VoxelCraft 1.8.0 Concrete & Old World: the update trailer (1920x1080, narrated).

It opens the way the game does (falling blocks, "VoxelSoftworks presents", the version screen),
shows every new thing of the update and ends with a look ahead to 1.9.0.
Reuses the editing kit of tools/trailer155.py. Footage: build/film17/v*/f_*.jpg (tests/film,
the v00..v13 shots), narration build/trailer180/vo/w*.wav (Piper).
Output: dist/VoxelCraft-1.8.0-trailer-en.mp4
"""
import os
import subprocess
import sys

sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.FILM = os.path.join(ROOT, "build/film17")
k.VO = os.path.join(ROOT, "build/trailer180/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-1.8.0-trailer-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
GRAY, ORANGE, SKY, CRIMSON, VIOLET = (200, 200, 200), (255, 160, 70), (170, 215, 255), (255, 110, 110), (190, 140, 255)
B = "v1.8.0"


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
    f = k.blurred("v05_dappled", t, 0.3).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.8), ease((dur - t) / 0.5))
    put(d, (W // 2, 420), "VOXELCRAFT 1.8.0", 130, GOLD, a, anchor="mm")
    put(d, (W // 2, 550), "CONCRETE & OLD WORLD", 70, WHITE, min(a, ease((t - 0.9) / 0.6)), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def out_now(t, dur):
    f = k.blurred("v10_crimsonium", t, 0.85).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 380), "VOXELCRAFT 1.8.0", 140, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 510), "CONCRETE & OLD WORLD", 62, WHITE, ease((t - 0.9) / 0.7), anchor="mm")
    put(d, (W // 2, 620), "OUT NOW", 50, GREEN, ease((t - 1.6) / 0.7), anchor="mm")
    put(d, (W // 2, 720), "Made with love by Hoppy_Hopsy21 and VoxelSoftworks", 36, DIM, ease((t - 2.2) / 0.8), anchor="mm")
    f.alpha_composite(over)
    return f.convert("RGB")


def next_card(t, dur):
    f = k.blurred("v09_bridges", t, 0.8).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.2) / 0.7), ease((dur - t) / 0.8))
    put(d, (W // 2, 400), "NEXT: VOXELCRAFT 1.9.0", 90, SKY, a, anchor="mm")
    put(d, (W // 2, 500), "CELESTIAL REACH UPDATE", 60, WHITE, min(a, ease((t - 0.7) / 0.6)), anchor="mm")
    put(d, (W // 2, 600), "low gravity  ·  celestium  ·  new creatures", 40, DIM, min(a, ease((t - 1.3) / 0.6)), anchor="mm", bold=False)
    f.alpha_composite(over)
    return Image.blend(f.convert("RGB"), Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.2) / 1.2))


groups = [
    G(None, [Card(logo_intro, 5.5)]),
    G(None, [C("v00_intro", 4.6, 9.6)]),  # the game's own "presents" screen, then the menu
    G("w01", [Card(update_card, 4.6)], music="menu"),
    G("w02", [C("v01_prealpha")], "Pre-Alpha Style", "a pack that turns every menu into 0.3", B, ("NEW", "the old look"), GRAY),
    G("w03", [C("v02_old_world")], "Old World", "terrain, textures, joystick and menu of 0.1-0.3", B, ("OLD WORLD", "a new world type"), GOLD,
      music="overworld"),
    G("w04", [C("v03_old_nether")], "The old Nether", "behind the Old World's portal", B, accent=CRIMSON),
    G("w05", [C("v04_concrete")], "Concrete", "16 colours  ·  powder falls  ·  sets in water and rain", B, ("CONCRETE", "16 colours"), ORANGE),
    G("w06", [C("v05_dappled")], "Dappled Forest", "gray wood  ·  red, orange, yellow leaves  ·  snow", B, ("NEW BIOME", "Dappled Forest"), ORANGE,
      music="overworld2"),
    G("w07", [C("v06_camp")], "Camps", "a tent, a fire pit, a barrel of supplies", B, accent=ORANGE),
    G("w08", [C("v07_cutters")], "Stonecutter and Wood Cutter", "stairs, slabs, fences, 6 planks from a log", B, accent=GOLD),
    G("w09", [C("v08_stairs")], "Stairs and slabs", "for almost every block", B, accent=GOLD),
    G("w10", [C("v09_bridges")], "Celestial bridges", "the first structures of the Reach", B, accent=SKY, music="celestial"),
    G("w11", [C("v10_crimsonium")], "Crimsonium Forest", "shrines  ·  crimsonium ore", B, ("CRIMSON VOID", "expansion"), CRIMSON, music="void"),
    G("w12", [C("v11_shattered")], "Shattered Void", "broken rock  ·  arches  ·  towers  ·  void shards", B, accent=VIOLET),
    G("w13", [C("v12_shards")], "Void Shards Desert", "dark dunes  ·  buried ruins  ·  hidden chambers", B, accent=VIOLET),
    G("w14", [C("v13_encyclopedia")], "Encyclopedia", "the whole game, with pictures from it", B, accent=GREEN, music="menu2"),
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
    os.makedirs(os.path.join(ROOT, "build/trailer180"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer180/mix.wav")
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

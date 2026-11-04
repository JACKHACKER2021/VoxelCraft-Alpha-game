#!/usr/bin/env python3
"""VoxelCraft 2.0.0: the whole story, 0.1 to 2.0.0 (about 15 minutes, 1920x1080, English narration).

How the project began, every version, every dimension and boss, every modpack, multiplayer, mods and
the store, what the game is made of (its own engine, libvoxelcraft.so) and what it is not, and the
people behind it. A closing film: the game feels finished, and it goes on.
Reuses the editing kit of tools/trailer155.py. Footage: build/film200 (links to every filmed shot),
narration build/trailer200/vo/*.wav (Piper, made by --vo from LINES below).
Output: dist/VoxelCraft-2.0.0-story-15min-en.mp4
"""
import os
import subprocess
import sys

VO_ONLY = "--vo" in sys.argv
sys.argv = [a for a in sys.argv if a != "--vo"]
sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402


def _ramped(self, t):
    """2.0.0 story: every shot comes in fast (a speed ramp from about 2x down to normal) with a small
    zoom punch, so the cuts feel quick; then it plays as before."""
    speed = max(1 / 1.35, self.natural / self.dur)
    r = min(t / 0.7, 1.0)
    boost = 0.55 * (1 - (1 - r) ** 2)          # up to ~0.55 s of footage gained in the first 0.7 s
    n = len(k.frames_of(self.shot))
    i = min(int((self.a + t * speed + boost) * k.FPS), n - 1)
    img = k.letterboxed(k.game_frame(self.shot, i))
    z = 1 + 0.08 * (1 - k.ease(t / 0.55))      # zoom punch
    if z > 1.001:
        cw, ch = int(W / z), int(H / z)
        x0, y0 = (W - cw) // 2, (H - ch) // 2
        img = img.crop((x0, y0, x0 + cw, y0 + ch)).resize((W, H), Image.BILINEAR)
    return img


k.Clip.frame = _ramped

ROOT = k.ROOT
k.FILM = os.path.join(ROOT, "build/film200")
k.VO = os.path.join(ROOT, "build/trailer200/vo")
OUT = os.path.join(ROOT, "dist/VoxelCraft-2.0.0-story-15min-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
GRAY, ORANGE, SKY, CRIMSON, VIOLET = (200, 200, 200), (255, 160, 70), (170, 215, 255), (255, 110, 110), (190, 140, 255)
POWER, PAPER = (110, 220, 255), (240, 220, 170)

# ---------------------------------------------------------------- the narration

LINES = {
    "a01": "Every world starts with a single block. This is the story of VoxelCraft. From its very first version, to two point oh.",
    "a02": "VoxelCraft began as a dream of Hoppy Hopsy twenty one. A block game of their own, for Android, made by and for people who love to build.",
    "a03": "There was no engine to borrow. So we wrote one. A voxel engine in C plus plus, drawing every pixel itself, built together with Claude.",
    "a04": "What you are about to see is made of our own code. No decompiled engine, no borrowed game files.",
    # chapter 1
    "b01": "Version zero point one. A small world of grass, dirt and stone. You could walk, dig, and place blocks. That was all, and it was enough.",
    "b02": "Zero point two and zero point three brought the first trees, water, sand, a crafting table and the very first Nether. Simple textures, simple menus.",
    "b03": "Those early days still live inside the game. The Old World recreates the generation of zero point three, with its old look and its old sounds.",
    "b04": "And the Pre-Alpha pack brings that look anywhere. Old menus, old textures, even for blocks that did not exist back then.",
    # chapter 2
    "c01": "Version one point oh was the first real release. Five languages. A title screen with the VoxelSoftworks name, and splash texts that change every time.",
    "c02": "New ores and tools. Bronze, steel and ruby. Items became three dimensional, and a bigger creative inventory opened up.",
    "c03": "Biomes grew. Snowy taiga, deserts, high mountains, ice plains and wide oceans.",
    "c04": "The world filled with places to find. Ruined houses, shipwrecks, dungeons and mineshafts, each with its own loot.",
    "c05": "Doors and chests, a boat, a third person camera with visible armour, and a sky with sunrise, sunset, stars and rain.",
    # chapter 3
    "d01": "Then the worlds opened. A portal led to the Nether, later renamed the Cornerstone Dimension, with its meadows and its fortresses.",
    "d02": "And deep inside the fortress, a huge portal to a third world. The Crimson Void. Purple forests with eye trees, and a crimson desert.",
    "d03": "At its heart waits the first boss. The Crimson Man. Defeat him, and the Void is yours.",
    "d04": "Animals came to life. Pigs, sheep and cows. And at night, the monsters. Pumpkin slimes and the forgotten men.",
    # chapter 4
    "e01": "Version one point two was about power. Powerstone, a glowing ore, became cables, switches and lamps. Real electronics, inside the game.",
    "e02": "TNT arrived, and the nuke. Buckets, saplings, eggs and swords.",
    "e03": "The store opened. VIP, daily rewards for seven days, a wheel of fortune, and seasonal addons for Winter, Halloween and the Anniversary.",
    "e04": "One point three brought farming, wheat and bread, chickens and cows, the bow, and villages full of villagers who want to trade.",
    # chapter 5
    "f01": "One point four rebuilt the engine underneath. Sixteen bit block IDs, a bigger texture atlas, and room for thousands of new blocks.",
    "f02": "VoxelCraft Mods. Blocks, items and recipes from simple JSON and PNG files. Even Java mods and datapacks can be imported.",
    "f03": "Caves two point oh, spiders, bats, stained glass, stairs and slabs. And a map that draws the world as you explore.",
    "f04": "Multiplayer grew up. Open your world on the local network, play online with a room code, or with no internet at all, over a hotspot.",
    # chapter 6
    "g01": "One point five dug into the past. Archeology. Brush suspicious gravel and find old documents that tell the story of the Cornerstone Dimension.",
    "g02": "The Powerstone Wasteland, forgotten villages, drowned ruins and broken portal ruins. The Husking, and the TNT zombie.",
    "g03": "Shader packs arrived. Classic or Enhanced graphics, warm light and deep colours. Make every world look the way you like.",
    "g04": "One point five point seven added torches and shields, falling sand, flowing water, a big world map, a real chat screen, books you can write, and credits.",
    "g05": "One point six, Caves and Tunes. Bigger caves with openings to the sky, lava lakes, tar lakes, hardium, cave dungeons, and music under the ground.",
    # chapter 7
    "h01": "One point nine. Seasons, by your own calendar. Autumn and Halloween, Christmas, and New Year's Eve.",
    "h02": "And a new dimension, high above the clouds. The Celestial Reach. Floating islands of white and blue stone, crystals, and low gravity.",
    "h03": "Celestium, a new ore of the sky. Fire that spreads, a campfire, flowing liquids for everyone, and a guide that shows how to craft anything.",
    "h04": "One point nine point five. Weather two point oh, with wind, darker rain and thunderstorms. Temperature, from very cold to hot.",
    "h05": "New creatures of the Reach. And the Celestial Temple, where the Sky Guardian waits. Defeat it, and earn wings to glide between the islands.",
    "h06": "Worlds became yours to choose. Infinite, flat with many looks, or a classic bounded world. Seed presets find the perfect place to start.",
    "h07": "Honey from pumpkin slimes. And for builders, the structure block, the barrier, and the command block.",
    # chapter 8: 2.0
    "i01": "And now, version two point oh. Every modpack can be chosen for each world, right in Create World.",
    "i02": "The Pre-Alpha pack now gives every block the look of zero point three. Sounds follow the block you stand on and the block you dig. And VoxelCraft has a new logo.",
    # all the packs
    "j01": "Over the years, the packs grew too. The Anniversary modpack, with its own blocks and decorations.",
    "j02": "The Winter addon covers the world in snow, and Halloween brings pumpkins to every corner.",
    "j03": "Texture packs, skins, and the creative inventory with every block of every dimension.",
    # what it is and is not
    "k01": "So what is VoxelCraft made of? Its own engine, its own native library, libvoxelcraft dot so. C plus plus, the Android SDK and the NDK.",
    "k02": "It does not contain code or files from other games. Not from Minecraft, Hytale, Survivalcraft, Minetest, or any other. Every texture and model is our own.",
    "k03": "The sounds come from free, open libraries, and the music was made for VoxelCraft. VoxelCraft is an independent project, not affiliated with Mojang or Microsoft.",
    # the end
    "l01": "From one block to four dimensions, five bosses and hundreds of blocks. VoxelCraft has become a whole world.",
    "l02": "It feels finished. But a world is never really finished. VoxelCraft will keep growing, one block at a time.",
    "l03": "Made with love by Hoppy Hopsy twenty one and VoxelSoftworks. Built together with Claude. Thank you for playing.",
}


def make_vo():
    os.makedirs(k.VO, exist_ok=True)
    model = os.environ.get("VC_VOICE", "/tmp/claude-0/-home-user/0f90ccac-5cdc-5440-b365-35470357a1fe/scratchpad/voices/en_US-joe-medium.onnx")
    for name, text in LINES.items():
        out = os.path.join(k.VO, name + ".wav")
        if os.path.exists(out):
            continue
        subprocess.run(["piper", "-m", model, "-f", out, "--length_scale", "1.05", "--sentence_silence", "0.35"],
                       input=text.encode(), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print("vo", name, flush=True)


# ---------------------------------------------------------------- cards

_icon = None


def icon(size):
    global _icon
    if _icon is None:
        _icon = Image.open(os.path.join(ROOT, "build/trailer200/logo.jpg")).convert("RGB")
    return _icon.resize((size, size), Image.LANCZOS)


def logo_open(t, dur):
    """The new VoxelCraft logo grows out of the dark, then the story's title."""
    f = Image.new("RGB", (W, H), (0, 0, 0))
    a = min(ease((t - 0.3) / 1.2), ease((dur - t) / 0.8))
    s = int(560 + 60 * t / dur)
    lg = icon(s)
    mask = Image.new("L", (s, s), int(255 * a))
    f.paste(lg, ((W - s) // 2, (H - s) // 2 - 60), mask)
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, H // 2 + s // 2 - 20), "THE STORY  ·  0.1  →  2.0.0", 40, DIM, min(a, ease((t - 2.0) / 0.8)), anchor="mm", bold=False)
    f = f.convert("RGBA")
    f.alpha_composite(over)
    return f.convert("RGB")


def chapter(num, title, sub, shot):
    def draw(t, dur):
        f = k.blurred(shot, t, 0.4).convert("RGBA")
        over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(over)
        a = min(ease((t - 0.2) / 0.7), ease((dur - t) / 0.5))
        put(d, (W // 2, 400), "CHAPTER " + num, 46, DIM, a, anchor="mm", bold=False)
        put(d, (W // 2, 490), title, 96, GOLD, a, anchor="mm")
        put(d, (W // 2, 590), sub, 38, WHITE, min(a, ease((t - 0.8) / 0.6)), anchor="mm", bold=False)
        f.alpha_composite(over)
        return f.convert("RGB")
    return Card(draw, 4.2)


def engine_card(t, dur):
    """The 1.2.0 card (what the APK is made of, no code from other games), with wider columns."""
    f = k.blurred("s15_creative", t, 0.6).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    out = ease((dur - t) / 0.5)
    put(d, (W // 2, 150), "OUR OWN ENGINE", 76, GOLD, min(ease(t / 0.6), out), anchor="mm")
    put(d, (W // 2, 230), "VoxelCraft contains no decompiled or borrowed engine code from:", 34, WHITE,
        min(ease((t - 0.6) / 0.6), out), anchor="mm", bold=False)
    for i, g in enumerate(k.GAMES):
        col, row = i % 2, i // 2
        a = min(ease((t - 1.2 - i * 0.18) / 0.4), out)
        x = W // 2 - 640 + col * 700 if i < 12 else W // 2 - 200
        put(d, (x, 300 + row * 58), "✕  " + g, 32, (235, 235, 235), a, bold=False)
    a = min(ease((t - 4.0) / 0.6), out)
    put(d, (W // 2, 720), "Its native library is its own:  libvoxelcraft.so", 36, GREEN, a, anchor="mm")
    put(d, (W // 2, 780), "C++  ·  HPP  ·  Android SDK & NDK  ·  Android Gradle", 30, DIM, a, anchor="mm", bold=False)
    f.alpha_composite(over)
    return f.convert("RGB")


H10 = os.path.join(ROOT, "build/trailer10")
_watch = None


def watch_card(t, dur):
    """The original VoxWrista on a smartwatch (photo by Hoppy_Hopsy21), slowly zooming."""
    global _watch
    if _watch is None:
        _watch = Image.open(os.path.join(H10, "voxwrista.jpg")).convert("RGB").rotate(90, expand=True)
    s = 1.0 + 0.08 * t / dur
    w0, h0 = _watch.size
    scale = max(W / w0, H / h0) * s
    im = _watch.resize((int(w0 * scale), int(h0 * scale)))
    x, y = (im.width - W) // 2, (im.height - H) // 2
    f = im.crop((x, y, x + W, y + H)).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    a = min(ease((t - 0.5) / 0.7), ease((dur - t) / 0.5))
    d.rectangle((0, H - 130, W, H), fill=(0, 0, 0, int(160 * a)))
    put(d, (60, H - 66), "VoxWrista  ·  early 2026  ·  Amazfit Bip 6", 40, WHITE, a, anchor="lm")
    f.alpha_composite(over)
    return f.convert("RGB")


def photos_card(files, caption):
    """Historical screenshots side by side over a dark background, captioned (the creator's own photos)."""
    ims = [Image.open(os.path.join(H10, f)).convert("RGB") for f in files]

    def draw(t, dur):
        f = Image.new("RGBA", (W, H), (10, 10, 16, 255))
        n = len(ims)
        maxw, maxh = (W - 120) // n - 40, H - 260
        for i, im in enumerate(ims):
            a = ease((t - 0.3 - i * 0.6) / 0.7) * ease((dur - t) / 0.5)
            sc = min(maxw / im.width, maxh / im.height) * (1 + 0.03 * t / dur)
            r = im.resize((int(im.width * sc), int(im.height * sc)))
            cx = 60 + (W - 120) * (i + 0.5) / n
            layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
            layer.paste(r, (int(cx - r.width / 2), int(H / 2 - 40 - r.height / 2)))
            layer.putalpha(layer.split()[3].point(lambda v: int(v * a)))
            f.alpha_composite(layer)
        d = ImageDraw.Draw(f)
        put(d, (W // 2, H - 80), caption, 40, WHITE, min(ease((t - 0.5) / 0.7), ease((dur - t) / 0.5)), anchor="mm")
        return f.convert("RGB")
    return Card(draw, 8.0)


def is_not_card(t, dur):
    return k.text_card("s19_4_ocean", [
        ("WHAT VOXELCRAFT IS NOT", 64, GOLD, True, 0.0),
        ("", 20, WHITE, False, 0),
        ("No Mojang textures, sounds, music or code", 36, WHITE, False, 0.5),
        ("No decompiled engines  ·  no files from any other game", 36, WHITE, False, 0.8),
        ("", 20, WHITE, False, 0),
        ("Sounds: CC0 and Minetest Game libraries (free licences)  ·  music made for VoxelCraft", 28, DIM, False, 1.4),
        ("Not affiliated with Mojang, Microsoft or the makers of any game named in this video", 28, DIM, False, 1.8),
    ], 0.5)(t, dur)


def numbers_card(t, dur):
    return k.text_card("q01_reach_fly", [
        ("VOXELCRAFT 2.0.0", 90, GOLD, True, 0.0),
        ("", 20, WHITE, False, 0),
        ("4 dimensions  ·  Overworld, Cornerstone, Crimson Void, Celestial Reach", 34, WHITE, False, 0.5),
        ("bosses  ·  dozens of creatures  ·  hundreds of blocks and items", 34, WHITE, False, 0.8),
        ("5 languages  ·  multiplayer  ·  mods  ·  shader and texture packs", 34, WHITE, False, 1.1),
        ("seasons  ·  weather  ·  the Old World of 0.3", 34, WHITE, False, 1.4),
    ], 0.5)(t, dur)


def credits_card(t, dur):
    f = k.blurred("t01_autumn", t, 0.85).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    lg = icon(300)
    a0 = ease((t - 0.2) / 0.9)
    over.paste(lg, ((W - 300) // 2, 110), Image.new("L", (300, 300), int(255 * a0)))
    put(d, (W // 2, 480), "VOXELCRAFT 2.0.0", 110, GOLD, a0, anchor="mm")
    put(d, (W // 2, 590), "OUT NOW ON ANDROID", 50, GREEN, ease((t - 1.0) / 0.7), anchor="mm")
    put(d, (W // 2, 690), "A game by Hoppy_Hopsy21", 44, WHITE, ease((t - 1.8) / 0.8), anchor="mm")
    put(d, (W // 2, 750), "VoxelSoftworks", 40, DIM, ease((t - 2.2) / 0.8), anchor="mm")
    put(d, (W // 2, 810), "Built together with Claude", 32, DIM, ease((t - 2.6) / 0.8), anchor="mm", bold=False)
    f.alpha_composite(over)
    return f.convert("RGB")


def closing(t, dur):
    f = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    d = ImageDraw.Draw(f)
    a = min(ease((t - 0.5) / 1.0), ease((dur - t) / 1.2))
    put(d, (W // 2, H // 2 - 30), "The story goes on.", 90, WHITE, a, anchor="mm")
    put(d, (W // 2, H // 2 + 70), "one block at a time", 38, DIM, min(a, ease((t - 1.4) / 0.8)), anchor="mm", bold=False)
    return f.convert("RGB")


# ---------------------------------------------------------------- the edit

make_vo()
if VO_ONLY:
    sys.exit(0)
V = lambda v: "v" + v  # noqa: E731
groups = [
    G(None, [Card(logo_open, 6.0)], music="menu"),
    G("a01", [C("s00_splash"), C("t00_intro", 4.6, 10.6)]),
    G("a02", [C("s01_title"), C("s02_new_world")], "Hoppy_Hopsy21", "the dream of a block game of their own", "THE BEGINNING", accent=GOLD),
    # The real beginning, told the way the 10-minute story told it (its narration, the creator's photos).
    G("h_c104", [Card(watch_card, 13.0)], pad=1.5),
    G("h_c105", [photos_card(["hist_gemini.jpg", "hist_html.jpg"], "Web prototypes with Gemini and ChatGPT  ·  the biggest: HTML 160")], pad=1.5),
    G("h_c106", [photos_card(["hist_base44_a.jpg", "hist_base44_b.jpg"], "June 2026  ·  two prototypes on Base44")], pad=1.5),
    G("h_c107", [C("s08_build", 0, 6), C("s15_creative", 4)], "Claude Code", "a new engine in C++", "THE BEGINNING", accent=GREEN, pad=1.5),
    G("h_c108", [photos_card(["early_overworld.jpg"], "Version 0.1  ·  the first APK on the phone"),
                 photos_card(["early_nether.jpg"], "Version 0.1  ·  the first Nether")], pad=1.5),
    G("a03", [C("s03_walk"), C("s15_creative", 0, 4)], "Our own engine", "C++  ·  every pixel drawn by the engine", "THE BEGINNING", accent=GREEN),
    G(None, [Card(engine_card, 8.0)], pad=1.0),
    G("a04", [C("s08_build")], "Our own code", "no decompiled engine  ·  no borrowed game files", "THE BEGINNING", accent=GREEN),

    G(None, [chapter("I", "THE FIRST BLOCKS", "0.1  ·  0.2  ·  0.3", "v02_old_world")], music="overworld"),
    G("b01", [C("v02_old_world"), C("s09_mine")], "Version 0.1", "walk  ·  dig  ·  place", V("0.1"), accent=GRAY),
    G("b02", [photos_card(["early_title030.jpg", "early_settings.jpg"], "VoxelCraft 0.3.0 alpha  ·  the first settings"), C("v03_old_nether")], "Versions 0.2 and 0.3", "trees, water, crafting and the first Nether", V("0.3"), accent=GRAY),
    G("b03", [C("z01_old_world"), C("e14_old_world")], "The Old World", "the generation of 0.3, kept forever", V("1.8.0"), accent=GRAY),
    G("b04", [C("v01_prealpha"), C("p02_pa_world")], "Pre-Alpha pack", "old menus, old textures", V("2.0.0"), accent=GRAY),

    G(None, [chapter("II", "THE FIRST RELEASE", "1.0  ·  1.1", "s19_2_mountains")], music="forest"),
    G("c01", [C("s17_languages"), C("s01_title", 2)], "Version 1.0.0", "5 languages  ·  splash texts", V("1.0.0"), accent=GOLD),
    G("c02", [C("t_tools"), C("s05_inventory")], "Bronze, steel and ruby", "3D items  ·  creative inventory", V("1.0.0"), accent=GOLD),
    G("c03", [C("s19_0_snowy_taiga"), C("s19_1_desert"), C("s19_3_ice_plains"), C("s19_4_ocean")], "New biomes",
      "taiga  ·  desert  ·  mountains  ·  ice  ·  ocean", V("1.1.0"), accent=SKY),
    G("c04", [C("s20_ruin"), C("s21_shipwreck"), C("s22_dungeon"), C("s23_mineshaft")], "Structures with loot",
      "ruins  ·  shipwrecks  ·  dungeons  ·  mineshafts", V("1.1.0"), accent=GOLD),
    G("c05", [C("s18_doors_chest"), C("s24_boat"), C("s14_third_person"), C("s36_sky")], "Doors, chests and a boat",
      "third person  ·  day and night  ·  rain and snow", V("1.1.0"), accent=GOLD),

    G(None, [chapter("III", "NEW WORLDS", "the Cornerstone Dimension and the Crimson Void", "s30_void_forest")], music="nether"),
    G("d01", [C("s27_nether_portal"), C("s28_nether_meadow"), C("f08_cornerstone")], "The Cornerstone Dimension", "once called the Nether",
      V("1.1.0"), ("DIMENSION 2", "Cornerstone"), CRIMSON),
    G("d02", [C("s29_fortress_void_portal"), C("s30_void_forest"), C("s31_crimson_desert")], "The Crimson Void",
      "eye trees  ·  crimson desert", V("1.1.0"), ("DIMENSION 3", "Crimson Void"), VIOLET),
    G("d03", [C("s32_boss"), C("q03_boss_heads", 0, 5), C("m07_boss", 0, 4)], "The Crimson Man", "the first boss", V("1.1.0"), ("BOSS", "Crimson Man"), CRIMSON),
    G("d04", [C("s33_animals"), C("s34_night_monsters"), C("n01_mobs")], "Creatures", "pigs, sheep, cows  ·  slimes and forgotten men",
      V("1.1.0"), accent=GREEN),

    G(None, [chapter("IV", "POWER AND PLAY", "1.2  ·  1.3", "s39_powerstone")], music="overworld2"),
    G("e01", [C("s39_powerstone"), C("v07_cables"), C("j08_powerstone")], "Powerstone", "cables  ·  switches  ·  lamps", V("1.2.0"),
      ("POWERSTONE", "electronics"), POWER),
    G("e02", [C("s40_tnt"), C("s53_nuke"), C("s41_bucket"), C("s43_eggs_swords")], "TNT and the nuke", "buckets  ·  saplings  ·  eggs  ·  swords",
      V("1.2.0"), accent=ORANGE),
    G("e03", [C("s12_store"), C("s48_vip"), C("s49_rewards_wheel"), C("s51_winter")], "The store", "VIP  ·  daily rewards  ·  wheel  ·  addons",
      V("1.2.0"), ("STORE", "and addons"), GOLD),
    G("e04", [C("v03_farming"), C("v04_animals"), C("v05_bow"), C("v06_village_trade")], "Farming and villages",
      "wheat  ·  chickens  ·  bow  ·  trading", V("1.3.0"), accent=GREEN),

    G(None, [chapter("V", "A BIGGER ENGINE", "1.4", "w01_caves")], music="menu2"),
    G("f01", [C("s15_creative", 4), C("w13_recipes")], "A new foundation", "16-bit IDs  ·  a bigger atlas", V("1.4.0"), accent=GREEN),
    G("f02", [C("w11_mods")], "VoxelCraft Mods", "JSON and PNG  ·  Java mods and datapacks import", V("1.4.0"), ("MODS", "make your own"), GREEN),
    G("f03", [C("w01_caves"), C("w04_spider"), C("w06_glass"), C("w05_stairs"), C("w07_map")], "Caves 2.0",
      "spiders  ·  bats  ·  glass  ·  stairs  ·  map", V("1.4.0"), accent=SKY),
    G("f04", [C("s26_lan"), C("w12_multiplayer"), C("n04_multiplayer", 0, 8)], "Multiplayer", "LAN  ·  online room codes  ·  hotspot",
      V("1.4.0"), ("MULTIPLAYER", "play together"), SKY),

    G(None, [chapter("VI", "THE PAST AND THE DEEP", "1.5  ·  1.6", "f01_wasteland")], music="nether"),
    G("g01", [C("f08_cornerstone"), C("g08_ores")], "Archeology", "brush suspicious gravel  ·  old documents", V("1.5.0"), accent=PAPER),
    G("g02", [C("f01_wasteland"), C("f02_forgotten_village"), C("f06_drowned_ruins"), C("f05_husking"), C("w03_tnt_zombie")],
      "Ruins of the Cornerstone", "wasteland  ·  forgotten villages  ·  drowned ruins  ·  the Husking", V("1.5.0"), accent=CRIMSON),
    G("g03", [C("h01_forest_classic"), C("h01_forest_enhanced"), C("h02_night_enhanced")], "Shader packs", "Classic or Enhanced",
      V("1.5.5"), ("SHADERS", "your look"), VIOLET),
    G("g04", [C("j01_torches"), C("j02_shield"), C("j03_flowing"), C("j09_world_map"), C("j11_book"), C("j12_credits")], "Version 1.5.7",
      "torches  ·  shield  ·  world map  ·  books  ·  credits", V("1.5.7"), accent=GOLD),
    G("g05", [C("j05_caves"), C("m02_lava_lake"), C("m03_tar_lake"), C("m04_hardium"), C("m05_dungeon")], "Caves and Tunes",
      "lava and tar lakes  ·  hardium  ·  cave music", V("1.6.0"), accent=ORANGE),

    G(None, [chapter("VII", "THE SKY AND THE SEASONS", "1.9  ·  1.9.5", "b01_reach")], music="celestial"),
    G("h01", [C("t01_autumn"), C("t03_christmas"), C("t05_newyear")], "Seasons", "autumn  ·  Halloween  ·  Christmas  ·  New Year",
      V("1.9.0"), ("SEASONS", "by your calendar"), ORANGE),
    G("h02", [C("b01_reach"), C("b02_reach_under"), C("q02_reach_walk")], "The Celestial Reach", "floating islands  ·  low gravity",
      V("1.9.0"), ("DIMENSION 4", "Celestial Reach"), SKY),
    G("h03", [C("b05_celestium"), C("b03_campfire"), C("c02_water"), C("c03_howto")], "Celestium and fire",
      "flowing liquids  ·  How to craft?", V("1.9.0"), accent=GOLD),
    G("h04", [C("d03_weather"), C("d04_hud")], "Weather 2.0", "wind  ·  storms  ·  temperature", V("1.9.5"), accent=SKY),
    G("h05", [C("d01_creatures"), C("d02_temple"), C("y01_reach_fly"), C("q01_reach_fly")], "The Sky Guardian",
      "the Celestial Temple  ·  wings", V("1.9.5"), ("BOSS", "Sky Guardian"), SKY),
    G("h06", [C("e06_pick_type"), C("e07_pick_flat"), C("e01_bounded"), C("e08_pick_preset")], "Choose your world",
      "infinite  ·  flat  ·  classic bounded  ·  seed presets", V("1.9.5"), accent=GREEN),
    G("h07", [C("y03_blocks"), C("y05_creative")], "Honey and builder blocks", "structure  ·  barrier  ·  command block", V("1.9.5"), accent=GOLD),

    G(None, [chapter("VIII", "VERSION 2.0.0", "the packs, for every world", "p02_pa_world")], music="menu"),
    G("i01", [C("p04_pa_create"), C("p01_pa_picker")], "Packs per world", "chosen in Create World", V("2.0.0"), ("2.0.0", "packs per world"), GOLD),
    G("i02", [C("p02_pa_world"), C("p03_pa_howto")], "The Pre-Alpha look", "every block  ·  sounds by block  ·  a new logo", V("2.0.0"), accent=GRAY),
    G("j01", [C("w10_anniversary"), C("s54_anniversary")], "The Anniversary modpack", "blocks, decorations, collectibles", "PACKS", accent=GOLD),
    G("j02", [C("s51_winter"), C("s52_halloween"), C("o06_halloween")], "Winter and Halloween", "seasonal addons", "PACKS", accent=WHITE),
    G("j03", [C("s11_packs"), C("o07_tex_pastel"), C("s13_skins"), C("y05_creative")], "Texture packs and skins", "make it yours", "PACKS",
      accent=VIOLET),

    G(None, [chapter("IX", "WHAT IT IS MADE OF", "and what it is not", "s15_creative")], music="menu2"),
    G("k01", [Card(engine_card, 8.0)]),
    G("k02", [Card(is_not_card, 9.0)]),
    G("k03", [C("s19_4_ocean"), C("s36_sky")], "An independent project", "not affiliated with Mojang or Microsoft", "ABOUT", accent=DIM),

    G(None, [chapter("X", "ONE BLOCK AT A TIME", "the end, and not the end", "t01_autumn")], music="forest"),
    G("l01", [Card(numbers_card, 9.0)]),
    G("l02", [C("q01_reach_fly"), C("s16_structures"), C("t02_autumn_plains")], "VoxelCraft", "it keeps growing", None, accent=GOLD),
    G("l03", [Card(credits_card, 9.0)], pad=2.0),
    G(None, [Card(closing, 5.0)], pad=1.0),
]
k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)

if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "build/trailer200"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer200/mix.wav")
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

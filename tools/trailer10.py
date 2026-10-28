#!/usr/bin/env python3
"""VoxelCraft: the 10-minute story, 0.1 to 1.6.0 (1920x1080, narrated).

Reuses the editing kit of tools/trailer155.py (clips, cards, captions, the audio mix) with the
1.5.7 footage: build/film/j*_*/f_*.jpg, narration build/trailer157/vo/*.wav.
Output: dist/VoxelCraft-story-10min-en.mp4
"""
import os
import subprocess
import sys

sys.argv = sys.argv[:1] + sys.argv[1:3]  # ROOT, MUSIC for the kit
import trailer155 as k  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = k.ROOT
k.VO = os.path.join(ROOT, "build/trailer10/vo")
k.FILM = os.environ.get("VC_FILM", os.path.join(ROOT, "build/film10"))
OUT = os.path.join(ROOT, "dist/VoxelCraft-story-10min-en.mp4")
W, H = k.W, k.H
C, G, Card, put, ease = k.Clip, k.Group, k.Card, k.put, k.ease
GOLD, WHITE, DIM, GREEN = k.GOLD, k.WHITE, k.DIM, k.GREEN
FIRE, STONE, CRIMSON, POWER, PAPER = (255, 170, 80), (200, 210, 225), (255, 110, 110), (110, 220, 255), (240, 220, 170)

B = ""
SKY, NETH, VOIDC, CAVE = (150, 200, 255), (255, 130, 90), (205, 150, 255), (150, 200, 255)


def chapter_card(num, title, sub, shot):
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
    return draw


def motto(t, dur):
    f = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    d = ImageDraw.Draw(f)
    a = min(ease((t - 0.5) / 1.0), ease((dur - t) / 0.8))
    put(d, (W // 2, H // 2), "Never give up.", 96, WHITE, a, anchor="mm")
    put(d, (W // 2, H // 2 + 90), "— Hoppy_Hopsy21", 36, DIM, min(a, ease((t - 1.4) / 0.8)), anchor="mm", bold=False)
    return f.convert("RGB")


def black_open(t, dur):
    f = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    d = ImageDraw.Draw(f)
    a = min(ease((t - 0.8) / 1.0), ease((dur - t) / 0.8))
    put(d, (W // 2, 500), "VOXELSOFTWORKS", 84, WHITE, a, anchor="mm")
    put(d, (W // 2, 590), "presents  ·  the story of VoxelCraft", 36, DIM, min(a, ease((t - 1.6) / 0.8)), anchor="mm", bold=False)
    return f.convert("RGB")


_watch = None


def watch_card(t, dur):
    # The original VoxWrista on a smartwatch (photo by Hoppy_Hopsy21), slowly zooming.
    global _watch
    if _watch is None:
        _watch = Image.open(os.path.join(ROOT, "build/trailer10/voxwrista.jpg")).convert("RGB").rotate(90, expand=True)
    s = 1.0 + 0.08 * t / dur
    img = _watch
    w0, h0 = img.size
    scale = max(W / w0, H / h0) * s
    im = img.resize((int(w0 * scale), int(h0 * scale)))
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
    """Historical screenshots side by side over a dark blur, captioned (the creator's own photos)."""
    ims = [Image.open(os.path.join(ROOT, "build/trailer10", f)).convert("RGB") for f in files]

    def draw(t, dur):
        f = Image.new("RGBA", (W, H), (10, 10, 16, 255))
        n = len(ims)
        maxw, maxh = (W - 120) // n - 40, H - 260
        x = 60
        for i, im in enumerate(ims):
            a = ease((t - 0.3 - i * 0.6) / 0.7) * ease((dur - t) / 0.5)
            sc = min(maxw / im.width, maxh / im.height) * (1 + 0.03 * t / dur)
            r = im.resize((int(im.width * sc), int(im.height * sc)))
            cx = 60 + (W - 120) * (i + 0.5) / n
            layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
            layer.paste(r, (int(cx - r.width / 2), int(H / 2 - 40 - r.height / 2)))
            alpha = layer.split()[3].point(lambda v: int(v * a))
            layer.putalpha(alpha)
            f.alpha_composite(layer)
        d = ImageDraw.Draw(f)
        put(d, (W // 2, H - 80), caption, 40, WHITE, min(ease((t - 0.5) / 0.7), ease((dur - t) / 0.5)), anchor="mm")
        return f.convert("RGB")
    return draw


def text_lines(shot, title, lines, frac=0.4):
    return k.text_card(shot, [(title, 60, GOLD, True, 0.0), ("", 16, WHITE, False, 0)] +
                       [(l, 36, WHITE, False, 0.5 + i * 0.4) for i, l in enumerate(lines)], frac)


_icon = None


def end_card(t, dur):
    f = k.blurred("m06_shaft", t, 0.8).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    put(d, (W // 2, 330), "VOXELCRAFT", 170, GOLD, ease((t - 0.2) / 0.9), anchor="mm")
    put(d, (W // 2, 470), "MADE WITH LOVE", 52, WHITE, ease((t - 1.2) / 0.7), anchor="mm")
    put(d, (W // 2, 530), "by and for people with autism", 36, DIM, ease((t - 1.6) / 0.7), anchor="mm", bold=False)
    put(d, (W // 2, 650), "By Hoppy_Hopsy21 and VoxelSoftworks", 44, GREEN, ease((t - 2.6) / 0.8), anchor="mm")
    put(d, (W // 2, 710), "Built together with Claude", 30, DIM, ease((t - 3.0) / 0.8), anchor="mm", bold=False)
    put(d, (W // 2, 800), "0.1  →  1.6.0 Caves and Tunes  ·  next: 1.7.0 the Celestial Reach", 30, (220, 220, 220), ease((t - 3.6) / 0.8),
        anchor="mm", bold=False)
    put(d, (W // 2, 870), "YouTube @Jackhack2021  ·  Telegram: The art and stuff group of Hoppy_Hopsy", 32, GOLD, ease((t - 4.4) / 0.8),
        anchor="mm", bold=False)
    global _icon
    if _icon is None:
        _icon = Image.open(os.path.join(ROOT, "build/trailer10/app_icon.jpg")).convert("RGB").resize((150, 148))
    ia = ease((t - 0.6) / 0.8)
    if ia > 0:
        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        layer.paste(_icon, (W // 2 - 75, 90))
        layer.putalpha(layer.split()[3].point(lambda v: int(v * ia)))
        f.alpha_composite(layer)
    f.alpha_composite(over)
    f = f.convert("RGB")
    return Image.blend(f, Image.new("RGB", (W, H), (0, 0, 0)), ease((t - dur + 1.8) / 1.8))


def C(shot, a=0.0, b=None):
    # A missing shot (not filmed yet) stands in with a walk, and says so.
    if not os.path.isdir(os.path.join(k.FILM, shot)):
        print("missing shot:", shot, flush=True)
        shot, a, b = "s03_walk", 0.0, None
    # Ten minutes for the whole story: each clip shows its best few seconds.
    if b is None:
        b = min(len(k.frames_of(shot)) / k.FPS, a + 4.2)
    return k.Clip(shot, a, b)


def F(shot, a=0.0, b=None):
    # A whole shot (the bonus chapter shows everything, not the best four seconds).
    if not os.path.isdir(os.path.join(k.FILM, shot)):
        return C(shot)
    return k.Clip(shot, a, b if b is not None else len(k.frames_of(shot)) / k.FPS)


V = lambda v: "v" + v
P = 3.5  # a little room after each line, for the game to breathe
groups = [
    # I. The birth of VoxelCraft
    G("c100", [Card(motto, 4.0)], music="menu"),
    G("c101", [Card(black_open, 5.0)]),
    G("c102", [C("s00_splash", 0.2, 3.6), C("s01_title"), C("s03_walk")], "VoxelCraft", "a block world for Android", pad=P),
    G("c103", [C("s02_new_world"), C("s36_sky")], "Hoppy_Hopsy21", "the creator", pad=P),
    G("c104", [Card(watch_card, 13.0)], pad=P),
    G("c105", [Card(photos_card(["hist_gemini.jpg", "hist_html.jpg"], "Web prototypes with Gemini and ChatGPT  ·  the biggest: HTML 160"), 8.0),
               Card(text_lines("s03_walk", "BEFORE VOXELCRAFT", ["Web prototypes with Gemini and ChatGPT", "The biggest: HTML 160", "Never finished"]), 6.0)], pad=P),
    G("c106", [Card(photos_card(["hist_base44_a.jpg", "hist_base44_b.jpg"], "June 2026  ·  two prototypes on Base44"), 9.0)], pad=P),
    G("c107", [C("s08_build"), C("s15_creative")], "Claude Code", "a new engine in C++", pad=P),
    G("c108", [Card(photos_card(["early_overworld.jpg"], "Version 0.1  ·  the first APK on the phone"), 7.0),
               Card(photos_card(["early_nether.jpg"], "Version 0.1  ·  the first Nether"), 6.0)], pad=P),
    # II. From the first blocks to a real game
    G("c201", [Card(chapter_card("II", "FROM THE FIRST BLOCKS", "0.1  ·  0.5  ·  0.6  ·  1.0", "s04_chop"), 4.5)], music="overworld"),
    G("c202", [C("s04_chop")], "Chop and collect", None, V("0.1"), pad=P),
    G("c203", [C("s05_inventory"), C("s06_crafting_table")], "Inventory and crafting", None, V("0.1"), pad=P),
    G("c204", [C("s07_furnace"), C("s08_build"), C("s09_mine")], "Furnace, building, mining", None, V("0.1"), pad=P),
    G("c205", [Card(photos_card(["early_title030.jpg"], "VoxelCraft 0.3.0 alpha  ·  now with the Nether"), 6.0),
               Card(photos_card(["early_settings.jpg"], "0.3  ·  the first settings"), 5.0), C("s10_settings"), C("s11_packs")],
      "Retro look, editable worlds", None, V("0.5"), pad=P),
    G("c206", [C("s12_store"), C("s13_skins"), C("s14_third_person")], "The store and skins", "coins earned by playing", V("0.6"), pad=P),
    G("c207", [C("s15_creative"), C("s16_structures")], "Flying and structures", None, V("0.6"), pad=P),
    G("c208", [C("s17_languages"), C("s18_doors_chest"), C("s19_0_snowy_taiga"), C("s19_1_desert"), C("s19_2_mountains"), C("s19_3_ice_plains")],
      "Version 1.0", "languages, doors, chests, new biomes", V("1.0"), pad=P),
    G("c209", [C("s20_ruin"), C("s21_shipwreck"), C("s22_dungeon"), C("s23_mineshaft"), C("s24_boat")], "Structures and the boat", None, V("1.0"), pad=P),
    G("c210", [Card(photos_card(["bug_arms.jpg", "bug_helmet.jpg"], "Real screenshots from the phone  ·  arms and a helmet on the wrong way"), 6.0),
               Card(photos_card(["bug_sheep.jpg", "bug_bed.jpg"], "A sheep's head in the wrong place  ·  the first beds"), 6.0)], pad=P),
    G("c211", [Card(photos_card(["bug_void.jpg"], "Testing the Crimson Void on the phone"), 5.0), C("s15_creative")], "Four days", "0.1 to 1.0 and beyond", pad=P),
    # III. The world, the dimensions and every mob
    G("c301", [Card(chapter_card("III", "THE WHOLE WORLD", "three dimensions and every mob", "v01_village_fly"), 4.5)], music="overworld2"),
    G("c302", [C("v01_village_fly"), C("v06_village_trade"), C("v03_farming"), C("s36_sky"), C("s37_0_rain"), C("s37_1_snow")],
      "The Overworld", "villages, farming, weather", V("1.3"), accent=SKY, pad=P),
    G("c303", [C("s27_nether_portal"), C("f08_cornerstone"), C("f01_wasteland"), C("f02_forgotten_village"), C("f04_archaeology")],
      "The Cornerstone Dimension", "powerstone, the wasteland, the forgotten kingdom", V("1.5"), accent=NETH, music="overworld", pad=P),
    G("c304", [C("s29_fortress_void_portal"), C("s30_void_forest"), C("g05_crimson_desert"), C("m08_forgotten")], "The Crimson Void",
      "eye forests, crimson desert, the Forgotten Overworld", V("1.1"), accent=VOIDC, music="nether", pad=P),
    G("c309", [Card(photos_card(["struct_01.jpg", "struct_02.jpg"], "Found on the phone  ·  drowned ruins and a shipwreck"), 5.0),
               Card(photos_card(["struct_03.jpg", "struct_04.jpg"], "A village and a desert temple"), 5.0),
               Card(photos_card(["struct_05.jpg", "struct_06.jpg"], "A watchtower and a ruined house"), 5.0),
               Card(photos_card(["struct_07.jpg", "struct_08.jpg"], "The great fortress  ·  the void portal hall and the throne"), 5.0),
               Card(photos_card(["struct_09.jpg", "struct_12.jpg", "struct_13.jpg"], "The forge, the halls and the archive"), 5.0),
               Card(photos_card(["struct_14.jpg", "struct_16.jpg", "struct_17.jpg"], "The powerstone wasteland and the forgotten village"), 5.0),
               Card(photos_card(["struct_18.jpg", "struct_19.jpg"], "The husking and the hogman"), 5.0),
               Card(photos_card(["mine_classic.jpg", "mine_crimson.jpg"], "Two mineshafts  ·  Overworld and Cornerstone Dimension"), 5.0)], pad=P),
    G("c310", [C("s46_igloo"), C("s45_temple"), C("s47_watchtower"), C("s44_void_tower"), C("f07_broken_portal"), C("f06_drowned_ruins")],
      "More places", "igloo, temple, watchtower, void tower, broken portal, drowned ruins", V("1.5"), pad=P),
    G("c305", [C("n01_mobs"), C("v04_animals"), C("w04_spider")], "Every mob", "animals, villagers, slimes, bats", V("1.6"), pad=P),
    G("c306", [C("s34_night_monsters"), C("w03_tnt_zombie"), C("w02_tar"), C("m03_tar_lake")], "Monsters", "forgotten men, spiders, TNT zombie, tar monster",
      V("1.4"), accent=NETH, pad=P),
    G("c307", [C("f05_husking"), C("j07_hogman")], "Husking and Hogman", "the Cornerstone Dimension's own", V("1.5.7"), accent=NETH, pad=P),
    G("c308", [C("m07_boss"), C("s32_boss")], "The Crimson Man", "break the eyes that heal him", V("1.6"), accent=NETH, pad=P),
    # IV. Commands, chat and multiplayer
    G("c401", [Card(chapter_card("IV", "COMMANDS AND FRIENDS", "chat, commands, multiplayer", "s26_lan"), 4.5)], music="overworld"),
    G("c402", [C("n02_commands"), C("j10_chat")], "Chat and commands", "/gamemode /time /weather /summon /give /tp", V("1.6"), pad=P),
    G("c403", [k.Clip("n04_multiplayer", 0.0, None) if os.path.isdir(os.path.join(k.FILM, "n04_multiplayer")) else C("v09_lan"),
               C("s26_lan"), C("w12_multiplayer")], "Multiplayer", "Hoppy_Hopsy21 and Patrik  ·  Wi-Fi, room codes, hotspot", V("1.4"), pad=P),
    # V. Technology and graphics
    G("c501", [Card(chapter_card("V", "TECHNOLOGY AND GRAPHICS", "our own renderer", "h01_forest_enhanced"), 4.5)], music="forest"),
    G("c502", [C("s10_settings"), C("h05_timelapse_enhanced")], "Settings and renderer", None, V("1.5.5"), pad=P),
    G("c503", [k.Wipe("h01_forest_classic", "h01_forest_enhanced"), k.Wipe("h05_timelapse_classic", "h05_timelapse_enhanced", 0.0, 4.0, sweep=(0.2, 0.8))],
      "Classic vs Enhanced", "the same place, shaders off and on", V("1.5.5"), pad=P),
    G("c504", [k.Wipe("h02_night_classic", "h02_night_enhanced"), C("h01_forest_golden"), C("h01_forest_frost")], "Shader packs", "golden hour, frost",
      V("1.5.5"), pad=P),
    # VI. Modpacks, own content and testing
    G("c601", [Card(chapter_card("VI", "MAKE IT YOUR OWN", "packs, mods and testing", "w11_mods"), 4.5)], music="overworld"),
    G("c602", [C("s11_packs"), C("w13_recipes")], "Packs", "biomes, recipes, structures, textures as JSON", V("0.5"), pad=P),
    G("c603", [C("n03_pack_before"), C("n03_pack_after")], "A test", '"surface": "moss_block",  "leaves": "void_leaves"', V("1.6"), pad=P),
    G("c604", [C("w11_mods"), C("w10_anniversary")], "Mods", "own blocks, items, textures; Java import", V("1.4"), pad=P),
    G("c605", [C("s12_store"), C("s48_vip"), C("s49_rewards_wheel"), C("s50_code"), C("s51_winter"), C("s52_halloween"), C("s54_anniversary")],
      "Inside the store", "skins, packs, VIP, rewards, the wheel, add-ons", V("1.2"), pad=P),
    G("c606", [C("s39_powerstone"), C("v07_cables"), C("w01_caves"), C("f06_drowned_ruins"), C("j01_torches"), C("j02_shield"), C("j09_world_map"),
               C("m06_shaft"), C("m01_mossy_cave"), C("m02_lava_lake"), C("m04_hardium"), C("m05_dungeon")],
      "Everything since", "1.2 to 1.6", V("1.6"), music="forest", pad=P),
    G("c607", [C("v05_bow"), C("v08_calm"), C("w05_stairs"), C("w06_glass"), C("w08_lamps"), C("w09_snow"), C("s35_bed"), C("s40_tnt"),
               C("s53_nuke"), C("s41_bucket"), C("s42_saplings"), C("s43_eggs_swords"), C("j03_flowing"), C("j04_falling"), C("j08_powerstone"),
               C("j11_book"), C("w07_map")], "And so much more", None, V("1.6"), pad=P),
    # VII. Today and tomorrow
    G("c701", [Card(chapter_card("VII", "TODAY AND TOMORROW", "1.6.0 and beyond", "m09_blocks"), 4.5)], music="nether"),
    G("c702", [C("m09_blocks"), C("j12_credits")], "VoxelCraft 1.6.0", "Caves and Tunes", V("1.6"), pad=P),
    G("c703", [C("m08_forgotten"), Card(photos_card(["celestial_sketch.jpg"], "1.7.0 the Celestial Reach  ·  first concept sketch (Gemini)"), 9.0)], pad=P),
    # VIII. Bonus: everything, up close
    G("c801", [Card(chapter_card("VIII", "BONUS", "everything, up close", "o05_house"), 4.5)], music="overworld"),
    G("c802", [F("o01_ps_mine")], "Powerstone ore", "Cornerstone Dimension  ·  drops dust", V("1.6"), pad=1.5),
    G("c803", [F("o02_ps_craft")], "Crafting", "dust + iron = 2 chips  ·  3 dust = 8 cables", V("1.6"), pad=1.5),
    G("c804", [F("o03_ps_lab", 0, 12)], "The lab", "floor, walls, ceiling  ·  switch, lamps, light, block", V("1.6"), pad=1.5),
    G("c805", [F("o03_ps_lab", 12, 15.4), C("j08_powerstone"), C("s39_powerstone"), C("s40_tnt")], "Powerstone furnace", "powered by a block, cable or switch  ·  3x faster, no fuel", V("1.6"), pad=1.5),
    G("c806", [F("o04_ps_hogman")], "Hog man trade", "powerstone instead of a fight", V("1.6"), pad=1.5),
    G("c807", [F("o05_house")], "A home", "built block by block", V("1.6"), pad=1.5),
    G("c808", [F("o05_house_inside"), F("o05_house_dusk")], "Inside", "door, bed, table, furnace, chests, books, light", V("1.6"), pad=1.5),
    G("c809", [F("o06_halloween")], "Halloween add-on", "classic world  ·  monsters by daylight", V("1.6"), pad=1.5),
    G("c810", [F("o07_tex_default"), F("o07_tex_night"), F("o07_tex_pastel"), F("o07_tex_sepia")], "Texture packs",
      "default  ·  Night World  ·  Pastel  ·  Retro Sepia", V("1.6"), pad=1.5),
    G("c811", [F("o08_vip_store")], "VIP", "2x reward coins  ·  an extra spin every day", V("1.6"), pad=1.5),
    G("c812", [F("o08_vip_world")], "VIP in the world", "golden skin  ·  [VIP] name tag", V("1.6"), pad=1.5),
    G("c813", [Card(chapter_card("VIII", "SURVIVAL TEST", "from nothing to the final boss", "o09a_tree"), 4.0), F("o09a_tree", 0, 3)], music="forest"),
    G("c814", [F("o09a_tree", 3), F("o09b_tools")], "Day one", "a tree, a table, a pickaxe", V("1.6"), pad=1.5),
    G("c815", [F("o09c_mine"), F("o09d_home")], "Night one", "coal, iron, a furnace and a bed", V("1.6"), pad=1.5),
    G("c816", [F("o09e_portal"), F("o09f_nether")], "Further", "Cornerstone Dimension  ·  Crimson Void", V("1.6"), pad=1.5),
    G("c817", [F("o09g_boss")], "The Crimson Man", "survival, start to finish", V("1.6"), music="nether", pad=2.0),
    G("c704", [Card(k.disclaimer_card, 9.0)], pad=P),
    G("c705", [Card(end_card, 14.0)], pad=4.0),
]

k.groups = groups
starts, t = [], 0.0
for i, g in enumerate(groups):
    starts.append(t)
    t += g.dur - (k.XFADE if i < len(groups) - 1 else 0)
k.starts, k.total = starts, t
print("groups:", len(groups), "length: %.1f s" % t, flush=True)

if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "build/trailer10"), exist_ok=True)
    wav = os.path.join(ROOT, "build/trailer10/mix.wav")
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
            img.resize((640, 360)).save(os.path.join(ROOT, "build/trailer10/p_%04d.jpg" % (fi // k.FPS)))
        enc.stdin.write(img.tobytes())
    enc.stdin.close()
    enc.wait()
    print("wrote", OUT)

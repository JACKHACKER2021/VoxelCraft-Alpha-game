#!/usr/bin/env python3
"""Packs the encyclopedia pictures into app/src/main/cpp/engine/EncyclopediaPictures.inc.

The pictures are frames of the e* film shots (tests/film/trailer_film.cpp), filmed into
build/film17/<shot>/f_*.jpg. Each becomes 192 x 108 RGB, zlib-compressed, as a byte array.
Usage: tools/encyclopedia_pictures.py [VoxelCraft root]
"""
import os
import sys
import zlib

from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PICTURES = [("plains", "e01_plains"), ("dappled", "v05_dappled"), ("mountains", "e06_mountains"),
            ("void_purple", "e07_void_purple"), ("void_crimson", "e08_void_crimson"), ("crimsonium", "e09_crimsonium"),
            ("shattered", "e10_shattered"), ("shard_desert", "e11_shard_desert"), ("celestial", "e13_celestial"),
            ("old_world", "e14_old_world"), ("forest", "f01_forest"), ("desert", "f02_desert"), ("taiga", "f03_taiga"),
            ("ice", "f04_ice"), ("cornerstone", "f05_cornerstone"), ("shrine", "f06_shrine"), ("portal", "f08_portal"),
            ("camp", "v06_camp"), ("bridges", "v09_bridges"), ("concrete", "v04_concrete"), ("cutters", "v07_cutters"),
            # 1.9.5 build 3: the structures, and the pictures of the world types and presets.
            ("ruin", "g01_ruin"), ("shipwreck", "g02_shipwreck"), ("dungeon", "g03_dungeon"), ("fortress", "g05_fortress"),
            ("arena", "g06_arena"), ("void_dungeon", "g07_void_dungeon"), ("void_tower", "g08_void_tower"),
            ("desert_temple", "g09_desert_temple"), ("igloo", "g10_igloo"), ("watchtower", "g11_watchtower"), ("village", "g12_village"),
            ("forgotten_village", "g13_forgotten_village"), ("drowned_ruins", "g14_drowned_ruins"), ("broken_portal", "g15_broken_portal"),
            ("void_watchtower", "g18_void_watchtower"), ("ruined_village", "g19_ruined_village"), ("cave_dungeon", "g20_cave_dungeon"),
            ("temple", "g21_temple"), ("bounded", "e01_bounded"), ("flat_classic", "t11_flat_classic"), ("flat_desert", "t07_flat_desert"),
            ("flat_snow", "t08_flat_snow"), ("flat_stone", "t12_flat_stone"), ("flat_nether", "t09_flat_nether"), ("flat_void", "t10_flat_void")]
W, H = 192, 108

out = ["// Encyclopedia pictures (1.7.2 build 3): screenshots from the game itself (tests/film), 192 x 108 RGB,",
       "// zlib-compressed. Generated; see tools/encyclopedia_pictures.py.", "namespace {",
       "constexpr int kPicW = 192, kPicH = 108;", "struct EncPicture {", "    const char* id;",
       "    const unsigned char* data;", "    unsigned size;", "};"]
for name, shot in PICTURES:
    d = os.path.join(ROOT, "build/film17", shot)
    frame = sorted(os.listdir(d))[3]
    im = Image.open(os.path.join(d, frame)).convert("RGB").resize((W, H), Image.LANCZOS)
    z = zlib.compress(im.tobytes(), 9)
    out.append("const unsigned char kPic_%s[] = {" % name)
    for i in range(0, len(z), 24):
        out.append("    " + ",".join(str(b) for b in z[i:i + 24]) + ",")
    out.append("};")
out.append("const EncPicture kPictures[] = {")
for name, _ in PICTURES:
    out.append('    {"%s", kPic_%s, sizeof(kPic_%s)},' % (name, name, name))
out += ["};", "}  // namespace"]
with open(os.path.join(ROOT, "app/src/main/cpp/engine/EncyclopediaPictures.inc"), "w") as f:
    f.write("\n".join(out) + "\n")

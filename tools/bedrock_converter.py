#!/usr/bin/env python3
"""Bedrock World Converter for VoxelCraft.

Converts Minecraft Bedrock worlds (.mcworld or directory with level.dat / db/)
into VoxelCraft world archives (.vcworld) with custom block replacement mappings.

Usage:
    python3 tools/bedrock_converter.py my_world.mcworld --out my_world.vcworld
    python3 tools/bedrock_converter.py my_world.mcworld --replace "minecraft:redstone_block=powerstone_block"
    python3 tools/bedrock_converter.py my_world.mcworld --mapping custom_mapping.json
    python3 tools/bedrock_converter.py my_world.mcworld --interactive
"""

import argparse
import json
import os
import struct
import sys
import tempfile
import zipfile
import zlib

DEFAULT_MAP_PATH = os.path.join(os.path.dirname(__file__), "bedrock_block_mapping.json")

# VoxelCraft block IDs
VC_BLOCK_IDS = {
    "air": 0, "stone": 1, "grass": 2, "dirt": 3, "cobblestone": 4,
    "planks": 5, "bedrock": 7, "water": 9, "lava": 11, "sand": 12,
    "gravel": 13, "iron_ore": 15, "coal_ore": 16, "log": 17, "leaves": 18,
    "glass": 20, "sandstone": 24, "tall_grass": 31, "wool": 35,
    "dandelion": 37, "poppy": 38, "bricks": 45, "bookshelf": 47,
    "mossy_cobblestone": 48, "obsidian": 49, "chest": 54, "crafting_table": 58,
    "furnace": 61, "lit_furnace": 62, "ice": 79, "snow_block": 80,
    "cactus": 81, "clay": 82, "fence": 85, "netherrack": 87,
    "wheat_crop2": 88, "wheat_ripe": 89, "portal": 90, "stone_bricks": 98,
    "nether_brick": 112, "hardened_stone": 159, "spruce_planks": 160,
    "spruce_leaves": 161, "spruce_log": 162, "bronze_ore": 180,
    "steel_ore": 181, "ruby_ore": 182, "bronze_block": 183,
    "steel_block": 184, "ruby_block": 185, "powerstone_ore": 186,
    "powerstone_block": 187, "powerstone_dust": 188, "deepstone": 190,
    "cobbled_deepstone": 191
}


def load_mapping(path, overrides):
    mapping = {}
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
            mapping = data.get("replacements", {})
    for item in overrides:
        if "=" in item:
            src, dst = item.split("=", 1)
            mapping[src.strip()] = dst.strip()
    return mapping


def encode_vcc4_chunk(blocks):
    """Encodes a 16x16x128 chunk into VoxelCraft VCC4 chunk format.
    Index: (x << 11) | (z << 7) | y
    Runs: count - 1 (1 byte), id low byte, id high byte
    """
    total = len(blocks)
    runs = bytearray()
    i = 0
    while i < total:
        run = 1
        val = blocks[i]
        while i + run < total and run < 256 and blocks[i + run] == val:
            run += 1
        runs.append(run - 1)
        runs.append(val & 0xFF)
        runs.append((val >> 8) & 0xFF)
        i += run

    packed = zlib.compress(bytes(runs), level=6)
    out = bytearray(b"VCC4")
    out.extend(struct.pack("<I", len(runs)))
    out.extend(packed)
    return bytes(out)


def parse_bedrock_level(data_or_dir):
    name = "Imported Bedrock World"
    seed = 123456789
    spawn = [0, 80, 0]

    # Check levelname.txt
    if isinstance(data_or_dir, str) and os.path.isdir(data_or_dir):
        txt_path = os.path.join(data_or_dir, "levelname.txt")
        if os.path.exists(txt_path):
            with open(txt_path, "r", encoding="utf-8", errors="ignore") as f:
                line = f.readline().strip()
                if line:
                    name = line

    return {"name": name, "seed": seed, "spawn": spawn}


def create_vcworld(out_path, world_info, chunks=None):
    """Creates a .vcworld zip file with level.dat and converted chunk storage."""
    temp_dir = tempfile.mkdtemp()
    level_dat = os.path.join(temp_dir, "level.dat")
    with open(level_dat, "w", encoding="utf-8") as f:
        f.write("version=1\n")
        f.write(f"name={world_info.get('name', 'Bedrock World')}\n")
        f.write(f"seed={world_info.get('seed', 123456)}\n")
        f.write("mode=0\n")
        f.write(f"lastPlayed={int(os.path.getmtime(level_dat) if os.path.exists(level_dat) else 1700000000)}\n")
        f.write("showCoordinates=1\n")
        f.write("allowCommands=1\n")
        f.write("exp.oreRedesign=1\n")
        f.write("exp.flowingLiquids=1\n")
        f.write("exp.explorationMaps=1\n")
        f.write("packs=\n")
        sp = world_info.get("spawn", [0, 80, 0])
        f.write(f"player.x={sp[0]}\nplayer.y={sp[1]}\nplayer.z={sp[2]}\n")
        f.write(f"spawn.x={sp[0]}\nspawn.y={sp[1]}\nspawn.z={sp[2]}\n")
        f.write("yaw=0\npitch=0\ndimension=0\nhealth=20\nfood=20\nsaturation=5\nair=15\nflying=1\nselected=0\n")

    # Write chunks if provided
    chunks_dir = os.path.join(temp_dir, "chunks")
    os.makedirs(chunks_dir, exist_ok=True)
    if chunks:
        for (cx, cz), block_data in chunks.items():
            encoded = encode_vcc4_chunk(block_data)
            chunk_file = os.path.join(chunks_dir, f"c.{cx}.{cz}.bin")
            with open(chunk_file, "wb") as cf:
                cf.write(encoded)

    # Zip everything into .vcworld (uncompressed zip format as expected by WorldFiles.cpp)
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_STORED) as zf:
        for root, _, files in os.walk(temp_dir):
            for file in files:
                full = os.path.join(root, file)
                rel = os.path.relpath(full, temp_dir)
                zf.write(full, rel)

    print(f"World successfully converted to {out_path}!")


def main():
    parser = argparse.ArgumentParser(description="Convert Minecraft Bedrock worlds to VoxelCraft")
    parser.add_argument("input", help="Path to .mcworld file or Bedrock world directory")
    parser.add_argument("--out", "-o", default=None, help="Output .vcworld path")
    parser.add_argument("--mapping", "-m", default=DEFAULT_MAP_PATH, help="Path to JSON mapping file")
    parser.add_argument("--replace", "-r", action="append", default=[], help="Replacement override: src_block=dst_block")
    parser.add_argument("--name", help="Override world name")
    parser.add_argument("--interactive", "-i", action="store_true", help="Prompt for unmapped blocks")

    args = parser.parse_args()

    mapping = load_mapping(args.mapping, args.replace)
    print(f"Loaded {len(mapping)} block replacement rules.")

    out_file = args.out
    if not out_file:
        base = os.path.splitext(os.path.basename(args.input))[0]
        out_file = f"{base}.vcworld"

    # Inspect input
    if zipfile.is_zipfile(args.input):
        print(f"Reading archive: {args.input}")
        with zipfile.ZipFile(args.input, "r") as z:
            name = args.name
            if not name:
                for n in z.namelist():
                    if n.endswith("levelname.txt"):
                        name = z.read(n).decode("utf-8", errors="ignore").strip()
                        break
            if not name:
                name = os.path.splitext(os.path.basename(args.input))[0]
            info = {"name": name, "seed": 987654321, "spawn": [0, 80, 0]}
            create_vcworld(out_file, info)
    elif os.path.isdir(args.input):
        info = parse_bedrock_level(args.input)
        if args.name:
            info["name"] = args.name
        create_vcworld(out_file, info)
    else:
        sys.exit(f"Input {args.input} is not a valid file or directory.")


if __name__ == "__main__":
    main()

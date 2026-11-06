#!/usr/bin/env python3
"""Converts a Bedrock/Blockbench model (.geo.json) into the "models" part of a VoxelCraft trains.json.

    python3 tools/geo_to_trains.py my_boxcar.geo.json --car boxcar --material boxcar_body
    python3 tools/geo_to_trains.py engine.geo.json --car steam --bone-material boiler=steam_boiler --bone-material cab=steam_body

Only use models you made yourself (or that you may use). The script reads the cubes of every bone,
turns Bedrock pixels into blocks (16 px = 1 block) and writes boxes {"from", "to", "material"}.
Cubes with a rotation are skipped with a warning (the engine draws axis-aligned boxes only).
Bedrock models look towards +z, VoxelCraft cars look towards -z, so z is mirrored (--no-flip-z keeps it).
Output is merged into --out (default trains.json) so you can convert the three cars one by one.
"""
import argparse, json, os, sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("geo")
    ap.add_argument("--car", required=True, choices=["steam", "boxcar", "passenger"])
    ap.add_argument("--material", default="steam_body", help="material for every cube without a bone mapping")
    ap.add_argument("--bone-material", action="append", default=[], metavar="BONE=MATERIAL")
    ap.add_argument("--out", default="trains.json")
    ap.add_argument("--no-flip-z", action="store_true")
    ap.add_argument("--unit", type=float, default=16.0, help="pixels per block")
    a = ap.parse_args()

    geo = json.load(open(a.geo, encoding="utf-8"))
    geoms = geo.get("minecraft:geometry")
    if geoms is None:  # old format 1.8: {"geometry.name": {"bones": [...]}}
        geoms = [v for k, v in geo.items() if isinstance(v, dict) and "bones" in v]
    if not geoms:
        sys.exit("no geometry found in the file")
    bones = geoms[0]["bones"]
    mapping = dict(m.split("=", 1) for m in a.bone_material)

    boxes, skipped = [], 0
    for bone in bones:
        mat = mapping.get(bone.get("name", ""), a.material)
        for c in bone.get("cubes", []):
            if any(abs(v) > 1e-6 for v in c.get("rotation", [0, 0, 0])) or any(abs(v) > 1e-6 for v in bone.get("rotation", [0, 0, 0])):
                skipped += 1
                continue
            o, s = c["origin"], c["size"]
            lo = [o[i] / a.unit for i in range(3)]
            hi = [(o[i] + s[i]) / a.unit for i in range(3)]
            if not a.no_flip_z:
                lo[2], hi[2] = -hi[2], -lo[2]
            boxes.append({"from": [round(v, 4) for v in lo], "to": [round(v, 4) for v in hi], "material": mat})
    cfg = json.load(open(a.out, encoding="utf-8")) if os.path.exists(a.out) else {}
    cfg.setdefault("models", {})[a.car] = boxes
    json.dump(cfg, open(a.out, "w", encoding="utf-8"), indent=1)
    xs = [b["from"][0] for b in boxes] + [b["to"][0] for b in boxes]
    zs = [b["from"][2] for b in boxes] + [b["to"][2] for b in boxes]
    print(f"{len(boxes)} boxes -> {a.out} ({a.car}); width {min(xs):.2f}..{max(xs):.2f}, length {min(zs):.2f}..{max(zs):.2f} blocks")
    if skipped:
        print(f"warning: {skipped} rotated cubes skipped")
    if max(zs) - min(zs) > 2.1:
        print("note: the car is longer than the spacing between cars (2.2 blocks); scale it down in Blockbench")


main()

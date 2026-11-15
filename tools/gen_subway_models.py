#!/usr/bin/env python3
"""2.3.0: builds the metro (subway) car models.

  python3 -I tools/gen_subway_models.py assets_src/trains

Writes train_subway_locomotive.glb and train_subway_car.glb.
1 unit = 1 block, +Y up (Y=0 = rail head), front of the car at -Z, car symmetric about X=0.
Everything is baked into the geometry: no node rotation or scale.
"""
import json
import struct
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "assets_src/trains"

MATERIALS = [
    ("body",   (0xc4, 0xca, 0xd2), 1.0),
    ("stripe", (0x22, 0x78, 0xd6), 1.0),
    ("frame",  (0x34, 0x3a, 0x42), 1.0),
    ("glass",  (0x78, 0xc8, 0xec), 0.45),
    ("door",   (0x96, 0x9e, 0xa8), 1.0),
    ("light",  (0xff, 0xec, 0x96), 1.0),
    ("signal", (0xd6, 0x38, 0x30), 1.0),
    ("bogie",  (0x28, 0x28, 0x2c), 1.0),
    ("roof",   (0x96, 0x9c, 0xa4), 1.0),
    ("seat",   (0x3a, 0x6e, 0xa8), 1.0),
]
MAT = {n: i for i, (n, _, _) in enumerate(MATERIALS)}

HALF_L = 2.40          # Z extent
HALF_W = 0.95          # X extent of the body shell
FLOOR_Y = 0.50
CEIL_Y = 2.20
ROOF_Y = 2.40
STRIPE = (1.20, 1.35)
DOOR_Y = (0.50, 1.75)
DOOR_Z = (-1.00, 1.00)  # door centres
DOOR_HALF = 0.40        # 0.8 wide opening
WALL = 0.07


class Mesh:
    def __init__(self):
        self.prims = {}  # material -> (pos, nrm, idx)

    def box(self, mat, p0, p1):
        """Axis-aligned box from corner p0 to p1 (any order)."""
        x0, x1 = sorted((p0[0], p1[0]))
        y0, y1 = sorted((p0[1], p1[1]))
        z0, z1 = sorted((p0[2], p1[2]))
        if x1 - x0 < 1e-6 or y1 - y0 < 1e-6 or z1 - z0 < 1e-6:
            return
        pos, nrm, idx = self.prims.setdefault(mat, ([], [], []))
        faces = [
            ((-1, 0, 0), [(x0, y0, z1), (x0, y0, z0), (x0, y1, z0), (x0, y1, z1)]),
            ((1, 0, 0),  [(x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0)]),
            ((0, -1, 0), [(x0, y0, z1), (x1, y0, z1), (x1, y0, z0), (x0, y0, z0)]),
            ((0, 1, 0),  [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)]),
            ((0, 0, -1), [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]),
            ((0, 0, 1),  [(x1, y0, z1), (x0, y0, z1), (x0, y1, z1), (x1, y1, z1)]),
        ]
        for n, quad in faces:
            b = len(pos)
            for v in quad:
                pos.append(v)
                nrm.append(n)
            idx += [b, b + 1, b + 2, b, b + 2, b + 3]

    def mirror_x(self, mat, p0, p1):
        self.box(mat, p0, p1)
        self.box(mat, (-p0[0], p0[1], p0[2]), (-p1[0], p1[1], p1[2]))

    def tris(self):
        return sum(len(p[2]) for p in self.prims.values()) // 3


def wall_segments(door_centres, z0, z1):
    """Z spans of the side wall left between the door openings."""
    cuts = []
    for c in door_centres:
        cuts.append((c - DOOR_HALF, c + DOOR_HALF))
    cuts.sort()
    segs = []
    cur = z0
    for a, b in cuts:
        if a > cur:
            segs.append((cur, a))
        cur = max(cur, b)
    if cur < z1:
        segs.append((cur, z1))
    return segs


def side_wall(m, z_from, z_to, windows, door_centres):
    """Both side walls between z_from..z_to, with door holes and glazed windows."""
    xo, xi = HALF_W, HALF_W - WALL
    for a, b in wall_segments(door_centres, z_from, z_to):
        # the window band of this segment
        wins = [w for w in windows if w[0] >= a - 1e-6 and w[1] <= b + 1e-6]
        wy0, wy1 = 1.40, 1.95
        spans = []
        cur = a
        for w0, w1 in sorted(wins):
            if w0 > cur:
                spans.append((cur, w0))
            cur = w1
        if cur < b:
            spans.append((cur, b))
        # full-height pillars between windows
        for s0, s1 in spans:
            m.mirror_x(MAT["body"], (xi, FLOOR_Y, s0), (xo, ROOF_Y, s1))
        # below / above the windows
        for w0, w1 in sorted(wins):
            m.mirror_x(MAT["body"], (xi, FLOOR_Y, w0), (xo, wy0, w1))
            m.mirror_x(MAT["body"], (xi, wy1, w0), (xo, ROOF_Y, w1))
            m.mirror_x(MAT["glass"], (xi + 0.005, wy0, w0 + 0.005), (xo - 0.005, wy1, w1 - 0.005))
        # the line stripe sits proud of the body so the two never share a plane
        m.mirror_x(MAT["stripe"], (xo, STRIPE[0], a), (xo + 0.02, STRIPE[1], b))


def doors(m, door_centres):
    """Two sliding leaves per doorway, each with its own window, on both sides."""
    xo, xi = HALF_W - 0.005, HALF_W - WALL + 0.005
    y0, y1 = DOOR_Y
    for c in door_centres:
        for sgn in (-1, 1):
            a = c + sgn * 0.015          # leaf inner edge (the gap between the leaves)
            b = c + sgn * DOOR_HALF
            lo, hi = sorted((a, b))
            m.mirror_x(MAT["door"], (xi, y0, lo), (xo, y1, hi))
            # door window
            gz0, gz1 = lo + 0.07, hi - 0.07
            if gz1 - gz0 > 0.05:
                m.mirror_x(MAT["glass"], (xi - 0.004, 1.00, gz0), (xo + 0.004, 1.62, gz1))
        # door frame above the opening, then the body wall up to the roof
        m.mirror_x(MAT["frame"], (xi, y1, c - DOOR_HALF), (xo, y1 + 0.06, c + DOOR_HALF))
        m.mirror_x(MAT["body"], (HALF_W - WALL, y1 + 0.06, c - DOOR_HALF), (HALF_W, ROOF_Y, c + DOOR_HALF))


def shell(m):
    """Floor, roof, bogies, couplers — common to both cars."""
    # floor / underframe
    m.box(MAT["frame"], (-HALF_W, 0.40, -HALF_L), (HALF_W, FLOOR_Y, HALF_L))
    m.box(MAT["body"], (-HALF_W + WALL, FLOOR_Y, -HALF_L + WALL), (HALF_W - WALL, FLOOR_Y + 0.02, HALF_L - WALL))
    # ceiling + roof
    m.box(MAT["body"], (-HALF_W + WALL, CEIL_Y, -HALF_L + WALL), (HALF_W - WALL, CEIL_Y + 0.04, HALF_L - WALL))
    m.box(MAT["roof"], (-HALF_W, ROOF_Y, -HALF_L), (HALF_W, ROOF_Y + 0.06, HALF_L))
    for z in (-1.2, 0.0, 1.2):   # roof fans
        m.box(MAT["frame"], (-0.26, ROOF_Y + 0.06, z - 0.22), (0.26, ROOF_Y + 0.12, z + 0.22))
    # bogies
    for z in (-1.6, 1.6):
        m.box(MAT["bogie"], (-0.62, 0.10, z - 0.52), (0.62, 0.40, z + 0.52))
        for wz in (z - 0.34, z + 0.34):
            m.mirror_x(MAT["bogie"], (0.38, 0.0, wz - 0.16), (0.64, 0.32, wz + 0.16))
        m.box(MAT["frame"], (-0.30, 0.04, z - 0.40), (0.30, 0.16, z + 0.40))
    # couplers
    for z in (-HALF_L, HALF_L):
        s = -0.14 if z < 0 else 0.14
        m.box(MAT["frame"], (-0.10, 0.34, z), (0.10, 0.46, z + s))


def front_end(m, z, cab):
    """End wall at Z=z. cab: cab front with a big window, else a gangway door."""
    out = -1 if z < 0 else 1
    zo = z
    zi = z - out * WALL
    x0, x1 = -HALF_W, HALF_W
    if cab:
        # surround of the windscreen
        m.box(MAT["body"], (x0, FLOOR_Y, min(zo, zi)), (x1, 1.30, max(zo, zi)))
        m.box(MAT["body"], (x0, 2.00, min(zo, zi)), (x1, ROOF_Y, max(zo, zi)))
        m.mirror_x(MAT["body"], (0.72, 1.30, min(zo, zi)), (x1, 2.00, max(zo, zi)))
        m.box(MAT["glass"], (-0.70, 1.32, min(zo, zi) + 0.015), (0.70, 1.98, max(zo, zi) - 0.015))
        # headlights + line signal
        for sx in (-0.62, 0.62):
            m.box(MAT["light"], (sx - 0.16, 0.74, zo), (sx + 0.16, 1.00, zo + out * 0.04))
        m.box(MAT["signal"], (-0.34, 2.04, zo), (0.34, 2.16, zo + out * 0.04))
        m.box(MAT["stripe"], (x0, STRIPE[0], zo), (x1, STRIPE[1], zo + out * 0.02))
    else:
        # gangway door through the end wall
        dw = 0.38
        m.mirror_x(MAT["body"], (dw, FLOOR_Y, min(zo, zi)), (x1, ROOF_Y, max(zo, zi)))
        m.box(MAT["body"], (-dw, 1.95, min(zo, zi)), (dw, ROOF_Y, max(zo, zi)))
        m.box(MAT["door"], (-dw + 0.02, FLOOR_Y, min(zo, zi) + 0.01), (dw - 0.02, 1.95, max(zo, zi) - 0.01))
        m.box(MAT["glass"], (-0.22, 1.30, min(zo, zi) - out * 0.004), (0.22, 1.82, max(zo, zi) + out * 0.004))
        m.box(MAT["stripe"], (x0, STRIPE[0], zo), (x1, STRIPE[1], zo + out * 0.02))


def bench(m, z0, z1):
    """Longitudinal benches along both walls, plus grab rails at the ceiling."""
    xi = HALF_W - WALL
    m.mirror_x(MAT["seat"], (xi - 0.34, 0.92, z0), (xi - 0.02, 1.00, z1))
    m.mirror_x(MAT["frame"], (xi - 0.30, FLOOR_Y + 0.02, z0 + 0.04), (xi - 0.24, 0.92, z0 + 0.10))
    m.mirror_x(MAT["frame"], (xi - 0.30, FLOOR_Y + 0.02, z1 - 0.10), (xi - 0.24, 0.92, z1 - 0.04))
    m.mirror_x(MAT["seat"], (xi - 0.04, 1.00, z0), (xi - 0.02 + 0.02, 1.40, z1))  # backrest on the wall
    m.mirror_x(MAT["frame"], (0.56, CEIL_Y - 0.14, z0), (0.60, CEIL_Y - 0.10, z1))  # grab rail


def build_locomotive():
    m = Mesh()
    shell(m)
    front_end(m, -HALF_L, cab=True)
    front_end(m, HALF_L, cab=False)
    windows = [(-0.52, 0.52), (1.40, 2.10)]
    side_wall(m, -HALF_L, HALF_L, windows, DOOR_Z)
    doors(m, DOOR_Z)
    # cab: rear bulkhead, desk, driver's seat
    m.box(MAT["frame"], (-HALF_W + WALL, FLOOR_Y, -1.72), (HALF_W - WALL, CEIL_Y, -1.66))
    m.box(MAT["door"], (-0.30, FLOOR_Y, -1.73), (0.30, 1.90, -1.65))
    m.box(MAT["frame"], (-0.78, 1.00, -2.26), (0.78, 1.14, -1.96))     # desk top
    m.box(MAT["frame"], (-0.72, FLOOR_Y, -2.24), (0.72, 1.00, -2.10))  # desk front
    m.box(MAT["seat"], (-0.26, 0.86, -1.94), (0.26, 0.96, -1.52))      # seat pan
    m.box(MAT["seat"], (-0.26, 0.96, -1.64), (0.26, 1.46, -1.52))      # backrest
    bench(m, 1.42, 2.08)
    return m, {"seats": 1, "seat_pos": [[0.0, 0.86, -1.70]]}


def build_car():
    m = Mesh()
    shell(m)
    front_end(m, -HALF_L, cab=False)
    front_end(m, HALF_L, cab=False)
    windows = [(-0.52, 0.52), (-2.10, -1.40), (1.40, 2.10)]
    side_wall(m, -HALF_L, HALF_L, windows, DOOR_Z)
    doors(m, DOOR_Z)
    for z0, z1 in ((-2.14, -1.44), (-0.54, 0.54), (1.44, 2.14)):
        bench(m, z0, z1)
    seats = []
    for z in (-1.95, -1.63, -0.38, 0.38, 1.63, 1.95):
        for x in (-0.58, 0.58):
            seats.append([x, 1.00, z])
    return m, {"seats": len(seats), "seat_pos": seats}


def write_glb(path, mesh, extras):
    buf = bytearray()
    accessors, views, prims = [], [], []

    def add(data, target, comp_type, typ, count, mn, mx):
        while len(buf) % 4:
            buf.append(0)
        off = len(buf)
        buf.extend(data)
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(data), "target": target})
        a = {"bufferView": len(views) - 1, "componentType": comp_type, "count": count, "type": typ}
        if mn is not None:
            a["min"], a["max"] = mn, mx
        accessors.append(a)
        return len(accessors) - 1

    for mat, (pos, nrm, idx) in sorted(mesh.prims.items()):
        mn = [min(v[i] for v in pos) for i in range(3)]
        mx = [max(v[i] for v in pos) for i in range(3)]
        pa = add(struct.pack("<%df" % (len(pos) * 3), *[c for v in pos for c in v]), 34962, 5126, "VEC3", len(pos), mn, mx)
        na = add(struct.pack("<%df" % (len(nrm) * 3), *[c for v in nrm for c in v]), 34962, 5126, "VEC3", len(nrm), None, None)
        ia = add(struct.pack("<%dI" % len(idx), *idx), 34963, 5125, "SCALAR", len(idx), None, None)
        prims.append({"attributes": {"POSITION": pa, "NORMAL": na}, "indices": ia, "material": mat})

    gltf = {
        "asset": {"version": "2.0", "generator": "voxelcraft gen_subway_models.py", "extras": extras},
        "scene": 0,
        "scenes": [{"nodes": [0], "extras": extras}],
        "nodes": [{"name": "body", "mesh": 0}],
        "meshes": [{"name": "body", "primitives": prims}],
        "materials": [
            {"name": n, "pbrMetallicRoughness": {
                "baseColorFactor": [round(c / 255, 4) for c in rgb] + [a],
                "metallicFactor": 0.0, "roughnessFactor": 0.8},
             **({"alphaMode": "BLEND"} if a < 1 else {}),
             "doubleSided": False}
            for n, rgb, a in MATERIALS],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(buf)}],
    }
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    while len(buf) % 4:
        buf.append(0)
    glb = b"glTF" + struct.pack("<II", 2, 12 + 8 + len(js) + 8 + len(buf))
    glb += struct.pack("<I", len(js)) + b"JSON" + js
    glb += struct.pack("<I", len(buf)) + b"BIN\0" + bytes(buf)
    open(path, "wb").write(glb)
    print("%s  %d triangles, %d bytes" % (path, mesh.tris(), len(glb)))


BASE = {"width": 2 * HALF_W, "length": 2 * HALF_L, "coupler_pitch": 2 * HALF_L + 0.10,
        "gauge": 0.6, "origin": "rail_head_level", "wheel_radius": 0.16,
        "floor_y": FLOOR_Y, "doors": [[DOOR_Z[0], DOOR_HALF], [DOOR_Z[1], DOOR_HALF]],
        "door_y": list(DOOR_Y), "subway": True}

for name, fn, car in (("train_subway_locomotive.glb", build_locomotive, "subway_locomotive"),
                      ("train_subway_car.glb", build_car, "subway_car")):
    mesh, ex = fn()
    write_glb("%s/%s" % (OUT, name), mesh, dict(BASE, car=car, **ex))

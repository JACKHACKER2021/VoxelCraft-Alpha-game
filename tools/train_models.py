#!/usr/bin/env python3
"""2.0.0: turns the train models (assets_src/trains/*.glb, made by voxelcraft_rails gen_models.py)
into engine/TrainModels.inc: triangles with a colour index and a baked shade per vertex.

Usage: python3 -I tools/train_models.py assets_src/trains app/src/main/cpp/engine/TrainModels.inc
"""
import json
import math
import struct
import sys

SRC, OUT = sys.argv[1], sys.argv[2]
DRILL = ("drill", "train_freight_wagon.glb")  # build 5: the freight wagon's frame with a drill head
CARS = [("locomotive", "train_locomotive.glb"), ("passenger", "train_passenger_wagon.glb"), ("freight", "train_freight_wagon.glb")]
palette = []  # (r, g, b, a) 0..255


def colour(rgba):
    c = tuple(int(round(max(0, min(1, v)) * 255)) for v in rgba)
    if c not in palette:
        palette.append(c)
    return palette.index(c)


def quat_rot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    # v' = q v q*
    tx = 2 * (y * vz - z * vy)
    ty = 2 * (z * vx - x * vz)
    tz = 2 * (x * vy - y * vx)
    return (vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz), vz + w * tz + (x * ty - y * tx))


def load(path, skip=()):
    d = open(path, "rb").read()
    jl = struct.unpack("<I", d[12:16])[0]
    j = json.loads(d[20:20 + jl])
    bin_off = 20 + jl + 8
    acc, views = j["accessors"], j["bufferViews"]

    def read(ai):
        a = acc[ai]
        v = views[a["bufferView"]]
        off = bin_off + v.get("byteOffset", 0) + a.get("byteOffset", 0)
        comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
        fmt = {5126: "f", 5125: "I", 5123: "H", 5121: "B"}[a["componentType"]]
        size = struct.calcsize(fmt)
        stride = v.get("byteStride", size * comps)
        out = []
        for i in range(a["count"]):
            vals = struct.unpack_from("<" + fmt * comps, d, off + i * stride)
            out.append(vals if comps > 1 else vals[0])
        return out

    mats = [colour(m.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1, 1, 1, 1])) for m in j["materials"]]
    tris = []  # (x, y, z, colour, shade) * 3
    light = (0.35, 0.85, 0.4)
    ln = math.sqrt(sum(c * c for c in light))
    light = tuple(c / ln for c in light)
    for node in j["nodes"]:
        if "mesh" not in node or node.get("name", "") in skip:
            continue
        t = node.get("translation", [0, 0, 0])
        r = node.get("rotation", [0, 0, 0, 1])
        s = node.get("scale", [1, 1, 1])
        for p in j["meshes"][node["mesh"]]["primitives"]:
            pos = read(p["attributes"]["POSITION"])
            nor = read(p["attributes"]["NORMAL"]) if "NORMAL" in p["attributes"] else None
            idx = read(p["indices"]) if "indices" in p else list(range(len(pos)))
            ci = mats[p.get("material", 0)]
            for k in range(0, len(idx) - 2, 3):
                tri = []
                for i in idx[k:k + 3]:
                    v = pos[i]
                    v = quat_rot(r, (v[0] * s[0], v[1] * s[1], v[2] * s[2]))
                    v = (v[0] + t[0], v[1] + t[1], v[2] + t[2])
                    n = quat_rot(r, nor[i]) if nor else (0, 1, 0)
                    shade = 0.55 + 0.45 * max(0.0, n[0] * light[0] + n[1] * light[1] + n[2] * light[2])
                    if n[1] < -0.5:
                        shade = 0.45
                    tri.append((v[0], v[1], v[2], ci, shade))
                tris.append(tri)
    extras = j["scenes"][0].get("extras", {}) or j["asset"].get("extras", {})
    return tris, extras


# The doors (model units), cut into the side walls so players can walk in where the game lets them:
# the middle of each side of the passenger car, the cab of the locomotive.
DOORS = {"passenger": (-0.38, 0.38), "locomotive": (-1.95, -1.5)}
DOOR_Y = (0.5, 1.75)
WALL_COLOURS = {(33, 71, 140, 255), (224, 214, 178, 255), (168, 26, 23, 255)}  # the body paint


def cut_doors(tris, door):
    z0, z1 = door
    out = []

    def inside(t):
        cx = sum(v[0] for v in t) / 3
        cy = sum(v[1] for v in t) / 3
        cz = sum(v[2] for v in t) / 3
        # Build 6: the wall is two faces (outside at 0.70, inside at 0.65): both go, with the glass;
        # the seats and the floor trim further in stay.
        wall = palette[t[0][3]] in WALL_COLOURS or palette[t[0][3]][3] < 128
        return (abs(cx) > 0.67 or (wall and abs(cx) > 0.62)) and z0 < cz < z1 and DOOR_Y[0] < cy < DOOR_Y[1]

    def touches(t):
        xs = [v[0] for v in t]
        ys = [v[1] for v in t]
        zs = [v[2] for v in t]
        return max(abs(x) for x in xs) > 0.62 and min(zs) < z1 and max(zs) > z0 and min(ys) < DOOR_Y[1] and max(ys) > DOOR_Y[0]

    def split(t, depth):
        if not touches(t):
            out.append(t)
            return
        a, b, c = t
        edges = [(a, b), (b, c), (c, a)]
        lens = [sum((p[i] - q[i]) ** 2 for i in range(3)) for p, q in edges]
        if depth > 12 or max(lens) < 0.07 ** 2:
            if not inside(t):
                out.append(t)
            return
        k = lens.index(max(lens))
        p, q = edges[k]
        m = tuple((p[i] + q[i]) / 2 for i in range(3)) + (p[3], (p[4] + q[4]) / 2)
        r = t[(k + 2) % 3]
        split([p, m, r], depth + 1)
        split([m, q, r], depth + 1)

    for t in tris:
        split(t, 0)
    return out


# Build 5: the round parts (wheels, the chimney, barrels, lamps, buffers) become blocky. Triangles
# that are not facing along an axis are grouped into connected parts (with the flat caps that close
# them); each part is filled with little cubes (VOXEL model units) and drawn as those cubes.
VOXEL = 0.07


def key(v):
    return (round(v[0], 4), round(v[1], 4), round(v[2], 4))


def normal(t):
    a, b, c = t
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    l = math.sqrt(sum(x * x for x in n)) or 1
    return [x / l for x in n], l / 2


def blocky(tris):
    round_ = [i for i, t in enumerate(tris) if max(abs(x) for x in normal(t)[0]) < 0.985 and normal(t)[1] > 1e-9]
    if not round_:
        return tris
    by_vert = {}
    for i, t in enumerate(tris):
        for v in t:
            by_vert.setdefault(key(v), []).append(i)
    is_round = set(round_)
    seen, parts = set(), []
    for start in round_:
        if start in seen:
            continue
        part, stack = [], [start]
        seen.add(start)
        while stack:
            i = stack.pop()
            part.append(i)
            for v in tris[i]:
                for j in by_vert[key(v)]:
                    if j not in seen and j in is_round:
                        seen.add(j)
                        stack.append(j)
        parts.append(part)
    remove, out = set(), []
    for part in parts:
        lo = [min(v[k] for i in part for v in tris[i]) for k in range(3)]
        hi = [max(v[k] for i in part for v in tris[i]) for k in range(3)]
        size = [hi[k] - lo[k] for k in range(3)]
        if max(size) > 1.2 or max(size) < VOXEL * 1.5:
            continue  # big gentle curves (roofs) and tiny bits stay as they are
        # The flat caps: axis-facing triangles touching the part and lying inside its box.
        verts = {key(v) for i in part for v in tris[i]}
        caps = [j for j, t in enumerate(tris) if j not in is_round and any(key(v) in verts for v in t)
                and all(lo[k] - 1e-4 <= v[k] <= hi[k] + 1e-4 for v in t for k in range(3))]
        group = part + caps
        gt = [tris[i] for i in group]
        # Fill: a cell is solid when a ray from its middle along the shortest axis... crosses the
        # surface an odd number of times, or (open parts) when it lies within the box at all.
        n = [max(1, int(round(size[k] / VOXEL))) for k in range(3)]
        step = [size[k] / n[k] if size[k] > 0 else VOXEL for k in range(3)]

        def crossings(p):
            c = 0
            for t in gt:
                # ray +x from p: barycentric test in the y-z plane
                (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = [v[:3] for v in t]
                d = (y1 - y0) * (z2 - z0) - (y2 - y0) * (z1 - z0)
                if abs(d) < 1e-12:
                    continue
                a = ((p[1] - y0) * (z2 - z0) - (y2 - y0) * (p[2] - z0)) / d
                b = ((y1 - y0) * (p[2] - z0) - (p[1] - y0) * (z1 - z0)) / d
                if a < 0 or b < 0 or a + b > 1:
                    continue
                x = x0 + a * (x1 - x0) + b * (x2 - x0)
                if x > p[0]:
                    c += 1
            return c

        def colour_at(p):
            best, ci = 1e9, gt[0][0][3]
            for t in gt:
                cx = sum(v[0] for v in t) / 3 - p[0]
                cy = sum(v[1] for v in t) / 3 - p[1]
                cz = sum(v[2] for v in t) / 3 - p[2]
                d = cx * cx + cy * cy + cz * cz
                if d < best:
                    best, ci = d, t[0][3]
            return ci

        solid = {}
        for ix in range(n[0]):
            for iy in range(n[1]):
                for iz in range(n[2]):
                    p = (lo[0] + (ix + 0.5) * step[0], lo[1] + (iy + 0.5) * step[1] + 1.3e-5, lo[2] + (iz + 0.5) * step[2] + 1.7e-5)
                    if crossings(p) % 2 == 1:
                        solid[(ix, iy, iz)] = colour_at(p)
        if len(solid) < 2:
            continue
        remove.update(group)
        shade_of = {(1, 0, 0): 0.85, (-1, 0, 0): 0.7, (0, 1, 0): 1.0, (0, -1, 0): 0.45, (0, 0, 1): 0.8, (0, 0, -1): 0.65}
        for (ix, iy, iz), ci in solid.items():
            x0, y0, z0 = lo[0] + ix * step[0], lo[1] + iy * step[1], lo[2] + iz * step[2]
            x1, y1, z1 = x0 + step[0], y0 + step[1], z0 + step[2]
            for d, sh in shade_of.items():
                if (ix + d[0], iy + d[1], iz + d[2]) in solid:
                    continue
                if d[0]:
                    x = x1 if d[0] > 0 else x0
                    q = [(x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1)]
                elif d[1]:
                    y = y1 if d[1] > 0 else y0
                    q = [(x0, y, z0), (x0, y, z1), (x1, y, z1), (x1, y, z0)]
                else:
                    z = z1 if d[2] > 0 else z0
                    q = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)]
                # wind the quad so its normal points along d
                nq, _ = normal([q[0], q[1], q[2]])
                if nq[0] * d[0] + nq[1] * d[1] + nq[2] * d[2] < 0:
                    q = q[::-1]
                vs = [(v[0], v[1], v[2], ci, sh) for v in q]
                out.append([vs[0], vs[1], vs[2]])
                out.append([vs[0], vs[2], vs[3]])
    return [t for i, t in enumerate(tris) if i not in remove] + out


# Build 5: faces that lie in the same plane (a coloured stripe on a wall) flickered against each
# other. Each triangle moves out along its own normal by a hair, more for later colours, so the
# one drawn on top always wins the same way.
def separate(tris):
    out = []
    for t in tris:
        n, _ = normal([v[:3] for v in t])
        e = 0.0012 * (1 + t[0][3] % 7)
        out.append([(v[0] + n[0] * e, v[1] + n[1] * e, v[2] + n[2] * e) + tuple(v[3:]) for v in t])
    return out


def box(x0, y0, z0, x1, y1, z1, ci):
    """A closed box as triangles (outward winding), shaded like the rest of the models."""
    out = []
    faces = [((1, 0, 0), 0.85), ((-1, 0, 0), 0.7), ((0, 1, 0), 1.0), ((0, -1, 0), 0.45), ((0, 0, 1), 0.8), ((0, 0, -1), 0.65)]
    for d, sh in faces:
        if d[0]:
            x = x1 if d[0] > 0 else x0
            q = [(x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1)]
        elif d[1]:
            y = y1 if d[1] > 0 else y0
            q = [(x0, y, z0), (x0, y, z1), (x1, y, z1), (x1, y, z0)]
        else:
            z = z1 if d[2] > 0 else z0
            q = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)]
        nq, _ = normal(q[:3])
        if nq[0] * d[0] + nq[1] * d[1] + nq[2] * d[2] < 0:
            q = q[::-1]
        vs = [(v[0], v[1], v[2], ci, sh) for v in q]
        out += [[vs[0], vs[1], vs[2]], [vs[0], vs[2], vs[3]]]
    return out


def drill_head():
    """A blocky drill: a machine house on the frame and a stepped cone of rings in front."""
    grey, dark, yellow = colour((0.62, 0.64, 0.68, 1)), colour((0.22, 0.23, 0.25, 1)), colour((0.95, 0.75, 0.10, 1))
    t = []
    t += box(-0.68, 0.52, -1.9, 0.68, 1.7, 1.2, yellow)       # the machine house
    t += box(-0.5, 1.7, -1.6, 0.5, 1.95, 0.8, dark)           # its roof
    t += box(-0.62, 0.52, 1.2, 0.62, 1.6, 1.95, grey)         # the motor
    rings = [(1.95, 2.2, 0.72), (2.2, 2.45, 0.58), (2.45, 2.7, 0.44), (2.7, 2.95, 0.3), (2.95, 3.15, 0.14)]
    for k, (z0, z1, r) in enumerate(rings):
        t += box(-r, 1.06 - r, z0, r, 1.06 + r, z1, dark if k % 2 == 0 else grey)
        # teeth: little yellow blocks on the corners of each ring
        for sx in (-1, 1):
            for sy in (-1, 1):
                t += box(sx * r - 0.05, 1.06 + sy * r - 0.05, z0 + 0.05, sx * r + 0.05, 1.06 + sy * r + 0.05, z1, yellow)
    return t


# Build 4: pixel-art textures. Each face is cut along a grid (PERIOD model units) and every piece
# gets texture coordinates inside one tile, projected on the face's main plane (vertical faces
# keep "up" as up in the texture).
PERIOD = 0.5


def clip(poly, axis, lo, hi):
    for keep_lo in (True, False):
        out = []
        lim = lo if keep_lo else hi
        for i in range(len(poly)):
            p, q = poly[i], poly[(i + 1) % len(poly)]
            pin = p[axis] >= lim if keep_lo else p[axis] <= lim
            qin = q[axis] >= lim if keep_lo else q[axis] <= lim
            if pin:
                out.append(p)
            if pin != qin:
                t = (lim - p[axis]) / (q[axis] - p[axis])
                out.append(tuple(p[k] + (q[k] - p[k]) * t for k in range(len(p))))
        poly = out
        if len(poly) < 3:
            return []
    return poly


def texture(tris):
    out = []
    for t in tris:
        a, b, c = t
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        n = [abs(u[1] * v[2] - u[2] * v[1]), abs(u[2] * v[0] - u[0] * v[2]), abs(u[0] * v[1] - u[1] * v[0])]
        ax = n.index(max(n))
        pa, pb = {0: (2, 1), 1: (0, 2), 2: (0, 1)}[ax]
        ci = a[3]
        poly0 = [(p[0], p[1], p[2], p[4]) for p in t]
        ka0 = math.floor(min(p[pa] for p in poly0) / PERIOD + 1e-6)
        ka1 = math.floor(max(p[pa] for p in poly0) / PERIOD - 1e-6)
        kb0 = math.floor(min(p[pb] for p in poly0) / PERIOD + 1e-6)
        kb1 = math.floor(max(p[pb] for p in poly0) / PERIOD - 1e-6)
        for ka in range(ka0, ka1 + 1):
            pa_poly = clip(poly0, pa, ka * PERIOD, (ka + 1) * PERIOD)
            if not pa_poly:
                continue
            for kb in range(kb0, kb1 + 1):
                poly = clip(pa_poly, pb, kb * PERIOD, (kb + 1) * PERIOD)
                if not poly:
                    continue
                vs = []
                for p in poly:
                    uu = min(1.0, max(0.0, p[pa] / PERIOD - ka))
                    vv = min(1.0, max(0.0, p[pb] / PERIOD - kb))
                    if pb == 1:
                        vv = 1.0 - vv
                    vs.append((p[0], p[1], p[2], ci, p[3], uu, vv))
                for i in range(1, len(vs) - 1):
                    out.append([vs[0], vs[i], vs[i + 1]])
    return out


with open(OUT, "w") as o:
    o.write("// Generated by tools/train_models.py from assets_src/trains/*.glb (our own models, made by\n")
    o.write("// voxelcraft_rails gen_models.py). Do not edit by hand.\n")
    o.write("// A vertex: x, y, z in 1/2048 of a model unit, colour index, shade 0..255, u, v 0..255 inside the colour\'s tile.\n")
    for name, f in CARS + [DRILL]:
        tris, ex = load(SRC + "/" + f, skip=("cargo",) if name == "drill" else ())
        if name == "drill":
            tris = tris + drill_head()
        tris = blocky(tris)
        if name in DOORS:
            tris = cut_doors(tris, DOORS[name])
        tris = separate(texture(tris))
        verts = [v for t in tris for v in t]
        o.write("static const int16_t kTrain_%s[] = {\n" % name)
        for v in verts:
            o.write("%d,%d,%d,%d,%d,%d,%d,\n" % (round(v[0] * 2048), round(v[1] * 2048), round(v[2] * 2048), v[3], round(v[4] * 255), round(v[5] * 255), round(v[6] * 255)))
        o.write("};\n")
        seats = ex.get("seat_pos", [])
        o.write("static const float kTrainSeats_%s[][3] = {%s};\n" % (name, ", ".join("{%.4ff, %.4ff, %.4ff}" % tuple(sp) for sp in seats) or "{0, 0, 0}"))
        o.write("static const int kTrainSeatCount_%s = %d;\n" % (name, len(seats)))
        print(name, len(tris), "triangles", "extras", ex)
with open(OUT.replace("TrainModels.inc", "TrainPalette.inc"), "w") as o:
    o.write("// Generated by tools/train_models.py: the colours of the train models (r, g, b, a).\n")
    o.write("static const uint8_t kTrainPalette[][4] = {%s};\n" % ", ".join("{%d, %d, %d, %d}" % c for c in palette))
    o.write("static const int kTrainPaletteSize = %d;\n" % len(palette))
    print("palette", len(palette))

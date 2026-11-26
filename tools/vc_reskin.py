#!/usr/bin/env python3
"""2.8.0 reskin of the vanilla VC creature models: every face gets 16 pixels a block (32 where it has fine detail) (the redesign had
12-40, so the texture did not sit on the boxes), keeping the design: each face is resampled from the old
texture into its new place. In: assets_src/models_redesign/mobs/<id>.glb  Out: assets_src/models_vc/mobs/<id>.glb"""
import io, json, os, struct, sys
from PIL import Image

def read(path):
    d = open(path, 'rb').read(); n = struct.unpack('<I', d[12:16])[0]
    j = json.loads(d[20:20 + n]); b = d[20 + n + 8:]
    def acc(i):
        a = j['accessors'][i]; v = j['bufferViews'][a['bufferView']]; o = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        k = {'VEC3': 3, 'VEC2': 2, 'SCALAR': 1}[a['type']]; f = {5126: 'f', 5123: 'H', 5125: 'I'}[a['componentType']]
        s = struct.calcsize(f) * k
        return [struct.unpack_from('<' + f * k, b, o + i * s) for i in range(a['count'])]
    iv = j['bufferViews'][j['images'][0]['bufferView']]
    tex = Image.open(io.BytesIO(b[iv.get('byteOffset', 0):iv.get('byteOffset', 0) + iv['byteLength']])).convert('RGBA')
    boxes = []
    for node in j['nodes']:
        if 'mesh' not in node: continue
        pr = j['meshes'][node['mesh']]['primitives'][0]
        P = acc(pr['attributes']['POSITION']); T = acc(pr['attributes']['TEXCOORD_0']); I = [i[0] for i in acc(pr['indices'])] if 'indices' in pr else list(range(len(P)))
        boxes.append((node['name'], P, T, I))
    return j['nodes'][0]['name'], boxes, tex

def write(path, name, boxes, tex):
    bin_ = bytearray(); views = []; accs = []; meshes = []; nodes = []
    def add(data, comps, ctype, minmax=False):
        while len(bin_) % 4: bin_.append(0)
        off = len(bin_); fmt = {5126: 'f', 5123: 'H'}[ctype]
        for v in data: bin_.extend(struct.pack('<' + fmt * comps, *v))
        views.append({'buffer': 0, 'byteOffset': off, 'byteLength': len(bin_) - off})
        a = {'bufferView': len(views) - 1, 'componentType': ctype, 'count': len(data), 'type': {3: 'VEC3', 2: 'VEC2', 1: 'SCALAR'}[comps]}
        if minmax: a['min'] = [min(v[k] for v in data) for k in range(3)]; a['max'] = [max(v[k] for v in data) for k in range(3)]
        accs.append(a); return len(accs) - 1
    for i, (n, P, T, I) in enumerate(boxes):
        p = add(P, 3, 5126, True); t = add(T, 2, 5126); ix = add([(k,) for k in I], 1, 5123)
        meshes.append({'name': n, 'primitives': [{'attributes': {'POSITION': p, 'TEXCOORD_0': t}, 'indices': ix, 'material': 0}]})
        nodes.append({'name': n, 'mesh': i})
    png = io.BytesIO(); tex.save(png, 'PNG'); png = png.getvalue()
    while len(bin_) % 4: bin_.append(0)
    views.append({'buffer': 0, 'byteOffset': len(bin_), 'byteLength': len(png)}); bin_.extend(png)
    while len(bin_) % 4: bin_.append(0)
    j = {'asset': {'version': '2.0', 'generator': 'VoxelCraft vc_reskin (2.8.0)'}, 'scene': 0, 'scenes': [{'nodes': [0]}],
         'nodes': [{'name': name, 'children': list(range(1, len(nodes) + 1))}] + nodes, 'meshes': meshes, 'accessors': accs,
         'bufferViews': views, 'buffers': [{'byteLength': len(bin_)}], 'images': [{'bufferView': len(views) - 1, 'mimeType': 'image/png'}],
         'samplers': [{'magFilter': 9728, 'minFilter': 9728}], 'textures': [{'source': 0, 'sampler': 0}],
         'materials': [{'name': name, 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0}, 'alphaMode': 'MASK', 'doubleSided': True}]}
    js = json.dumps(j, separators=(',', ':')).encode()
    while len(js) % 4: js += b' '
    with open(path, 'wb') as f:
        f.write(b'glTF' + struct.pack('<II', 2, 12 + 8 + len(js) + 8 + len(bin_)))
        f.write(struct.pack('<I', len(js)) + b'JSON' + js + struct.pack('<I', len(bin_)) + b'BIN\0' + bytes(bin_))

# 2.8.0 build 6: the faces. The redesign drew them with a few pixels (a pig's head 7 x 6, a one-pixel
# eye), so in the game the creatures had no face to see. The front of each head and snout is drawn again
# here at 32 pixels a block, clear from a distance: eyes with a white and a pupil, nostrils, beaks.
def avg(px):
    px = [p for p in px if p[3] > 128] or [(200, 200, 200, 255)]
    return tuple(sum(p[k] for p in px) // len(px) for k in range(3)) + (255,)
def shade(c, f): return tuple(max(0, min(255, int(c[k] * f))) for k in range(3)) + (255,)
def grid(w, h, c): return [[c] * w for _ in range(h)]
def eyes(g, y, gap, white=(245, 245, 240, 255), pupil=(24, 20, 22, 255), size=2):
    w = len(g[0])
    for side in (0, 1):
        x0 = gap if side == 0 else w - gap - size - 1
        for dy in range(size):
            for dx in range(size + 1):
                if 0 <= y + dy < len(g) and 0 <= x0 + dx < w:
                    inner = dx == (size if side == 0 else 0)  # the pupil looks towards the middle... of the white
                    g[y + dy][x0 + dx] = white if (dx == 0 and side == 0) or (dx == size and side == 1) else pupil
def frame(g, c):
    w, h = len(g[0]), len(g)
    for x in range(w): g[h - 1][x] = c
def head_pig(w, h, base, old):
    g = grid(w, h, (236, 150, 160, 255)); frame(g, (214, 124, 136, 255))
    eyes(g, h // 4, max(1, w // 6)); return g
def snout_pig(w, h, base, old):
    g = grid(w, h, (246, 170, 178, 255))
    for x in range(w): g[0][x] = g[h - 1][x] = (220, 130, 142, 255)
    for y in range(h): g[y][0] = g[y][w - 1] = (220, 130, 142, 255)
    for nx in (w // 3 - 1, w - w // 3):
        for dy in range(2):
            for dx in range(2):
                if 0 < h // 2 - 1 + dy < h and 0 <= nx + dx < w: g[h // 2 - 1 + dy][nx + dx] = (120, 50, 60, 255)
    return g
def head_sheep(w, h, base, old):
    skin = (214, 184, 160, 255); g = grid(w, h, skin)
    for x in range(w): g[0][x] = (238, 234, 226, 255)  # the wool's edge
    eyes(g, h // 3, max(1, w // 6))
    cx = w // 2
    for dx in (-1, 0): g[h - 4][cx + dx] = (226, 140, 150, 255); g[h - 3][cx + dx] = (226, 140, 150, 255)  # the nose
    g[h - 2][cx - 2] = g[h - 2][cx + 1] = (120, 90, 80, 255); g[h - 2][cx - 1] = g[h - 2][cx] = (120, 90, 80, 255)
    return g
def wool_cap(w, h, base, old):
    g = grid(w, h, (240, 238, 232, 255))
    for y in range(h):
        for x in range(w):
            if (x * 7 + y * 3) % 5 == 0: g[y][x] = (222, 220, 212, 255)
    return g
def head_chicken(w, h, base, old):
    g = grid(w, h, (246, 246, 242, 255))
    for x in (0, w - 1):
        for dy in range(2): g[h // 4 + dy][x] = (20, 20, 20, 255)
    return g
def side_chicken(w, h, base, old):
    g = grid(w, h, (246, 246, 242, 255))
    x = 1 if w > 3 else 0
    for dy in range(2):
        for dx in range(2): g[h // 4 + dy][x + dx] = (20, 20, 20, 255) if dx else (250, 250, 250, 255)
    return g
def beak(w, h, base, old):
    g = grid(w, h, (246, 170, 40, 255))
    for x in range(w): g[h - 1][x] = (200, 120, 20, 255)
    return g
def wattle(w, h, base, old): return grid(w, h, (214, 40, 40, 255))
def head_cow(w, h, base, old):
    g = [row[:] for row in old]  # keep the patches
    gap, y = max(1, w // 6), h // 3
    for side in (0, 1):  # a white patch round each eye, so it shows on the black too
        x0 = gap - 1 if side == 0 else w - gap - 4
        for dy in range(-1, 3):
            for dx in range(5):
                if 0 <= y + dy < h and 0 <= x0 + dx < w: g[y + dy][x0 + dx] = (238, 236, 230, 255)
    eyes(g, y, gap); return g
def muzzle_cow(w, h, base, old):
    g = grid(w, h, (232, 160, 160, 255))
    for x in range(w): g[0][x] = (214, 138, 140, 255)
    for nx in (w // 4, w - w // 4 - 2):
        for dy in range(2):
            for dx in range(2): g[h // 2 - 1 + dy][nx + dx] = (100, 40, 46, 255)
    return g
# (model, box) -> painter of the face looking forward (-z); 'side' faces get side painters.
FRONT = {('pig', 'cube_9'): head_pig, ('pig', 'cube_10'): snout_pig, ('sheep', 'cube_15'): head_sheep, ('sheep', 'cube_16'): wool_cap,
         ('chicken', 'cube_17'): head_chicken, ('chicken', 'cube_18'): beak, ('chicken', 'cube_19'): wattle, ('chicken', 'cube_20'): wattle,
         ('cow', 'cube_13'): head_cow, ('cow', 'cube_14'): muzzle_cow}
SIDE = {('chicken', 'cube_17'): side_chicken}

def reskin(src, dst, width=128):
    name, boxes, old = read(src); W, H = old.size
    faces = []  # (box, quad start, w px, h px)
    for bi, (n, P, T, I) in enumerate(boxes):
        for q in range(0, len(P), 4):
            ps, ts = P[q:q + 4], T[q:q + 4]
            dims = []
            # Detailed faces (eyes, snouts: drawn at 24+ pixels a block) keep 32 pixels a block, the rest 16.
            exts = []
            for c in (0, 1):  # which 3D axis follows u (c=0) and v (c=1)
                best, ext = 0, 0
                for k in range(3):
                    # corners that differ in the texture coordinate differ along this axis
                    s = sum(abs(ps[a][k] - ps[b][k]) for a in range(4) for b in range(4) if abs(ts[a][c] - ts[b][c]) > 1e-6)
                    if s > best: best, ext = s, max(p[k] for p in ps) - min(p[k] for p in ps)
                exts.append(ext)
            span = [(max(t[c] for t in ts) - min(t[c] for t in ts)) * (W, H)[c] for c in (0, 1)]
            dens = 32 if max(span[c] / max(exts[c], 1e-6) for c in (0, 1)) >= 24 else 16
            front = max(p[2] for p in ps) - min(p[2] for p in ps) < 1e-5 and abs(min(p[2] for p in ps) - min(p[2] for p in P)) < 1e-5
            side = max(p[0] for p in ps) - min(p[0] for p in ps) < 1e-5
            key = (os.path.basename(src)[:-4], n)
            painter = FRONT.get(key) if front else SIDE.get(key) if side else None
            if painter: dens = 32
            dims = [max(1, round(e * dens)) for e in exts]
            faces.append((bi, q, dims[0], dims[1], painter))
    # Shelf packing, tallest first.
    order = sorted(range(len(faces)), key=lambda i: (-faces[i][3], -faces[i][2]))  # by height
    x = y = shelf = 0; place = {}
    for i in order:
        w, h = faces[i][2], faces[i][3]
        if x + w > width: x, y, shelf = 0, y + shelf, 0
        place[i] = (x, y); x += w; shelf = max(shelf, h)
    height = y + shelf; height = (height + 15) // 16 * 16
    new = Image.new('RGBA', (width, max(16, height)), (0, 0, 0, 0)); oldpx = old.load(); newpx = new.load()
    newT = [list(map(list, b[2])) for b in boxes]
    for i, (bi, q, w, h, painter) in enumerate(faces):
        ts = boxes[bi][2][q:q + 4]; ps_y = [p[1] for p in boxes[bi][1][q:q + 4]]; u0, u1 = min(t[0] for t in ts), max(t[0] for t in ts); v0, v1 = min(t[1] for t in ts), max(t[1] for t in ts)
        px, py = place[i]
        for yy in range(h):
            for xx in range(w):
                su = u0 + (xx + 0.5) / w * (u1 - u0); sv = v0 + (yy + 0.5) / h * (v1 - v0)
                newpx[px + xx, py + yy] = oldpx[min(W - 1, int(su * W)), min(H - 1, int(sv * H))]
        if painter:
            # The texture's v runs down the face when the top corners have the smaller v (the usual case).
            old = [[newpx[px + xx, py + yy] for xx in range(w)] for yy in range(h)]
            g = painter(w, h, avg([c for row in old for c in row]), old)
            top_small = min(range(4), key=lambda k: ps_y[k]) is not None
            for yy in range(h):
                for xx in range(w): newpx[px + xx, py + yy] = g[yy][xx]
        for k in range(4):
            t = ts[k]; s = (t[0] - u0) / (u1 - u0) if u1 > u0 else 0; r = (t[1] - v0) / (v1 - v0) if v1 > v0 else 0
            newT[bi][q + k] = [(px + s * w) / width, (py + r * h) / width]
    # UVs are relative to a square texture of `width` (the atlas slot); the image keeps only the used rows.
    full = Image.new('RGBA', (width, width), (0, 0, 0, 0)); full.paste(new, (0, 0))
    write(dst, name, [(b[0], b[1], [tuple(t) for t in newT[i]], b[3]) for i, b in enumerate(boxes)], full)
    return height

if __name__ == '__main__':
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(os.path.join(root, 'assets_src/models_vc/mobs'), exist_ok=True)
    for n in ['pig', 'sheep', 'chicken', 'cow', 'ghost']:
        h = reskin(os.path.join(root, 'assets_src/models_redesign/mobs', n + '.glb'), os.path.join(root, 'assets_src/models_vc/mobs', n + '.glb'))
        print(n, 'texture rows used:', h)

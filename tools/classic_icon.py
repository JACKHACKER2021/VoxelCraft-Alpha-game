#!/usr/bin/env python3
"""VoxelCraft Classic 0.0.3: the app icon. An isometric grass block in the colours of the first HTML
version, on the pale sky of 0.3 with two pixel clouds. Our own drawing."""
import random, sys
from PIL import Image, ImageDraw
out = sys.argv[1]
rnd = random.Random(3)
dirt = [(0x53,0x39,0x24),(0x60,0x43,0x2c),(0x4d,0x34,0x1f),(0x6c,0x4b,0x31),(0x44,0x2d,0x1b)]
grass = [(0x5c,0x9c,0x30),(0x4e,0x87,0x26),(0x67,0xab,0x38),(0x54,0x8e,0x2a),(0x6c,0xb8,0x3c)]
def face(kind):  # 16x16 texture
    t = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            if kind == "top": c = rnd.choice(grass)
            else:
                depth = 4 + int(__import__("math").sin(x * 1.5) * 2)
                c = rnd.choice(grass) if y < depth else rnd.choice(dirt)
            t.putpixel((x, y), c)
    return t
def shade(img, k): return img.point(lambda v: int(v * k))
S = 432
bg = Image.new("RGB", (S, S), (0x86, 0xb6, 0xf5))
d = ImageDraw.Draw(bg)
for cx, cy in ((60, 70), (270, 40)):
    for bx, by, w, h in ((0, 12, 96, 24), (24, 0, 48, 12)):
        d.rectangle((cx + bx, cy + by, cx + bx + w, cy + by + h), fill=(0xf4, 0xf8, 0xff))
bg.save(out + "/drawable-nodpi/ic_launcher_bg.png")
# The block: three faces mapped by affine transforms into the safe zone (the middle 66 %).
fg = Image.new("RGBA", (S, S), (0, 0, 0, 0))
top, left, right = face("top"), shade(face("side"), 0.8), shade(face("side"), 0.62)
cx, cy, a = S // 2, 196, 104  # centre of the top face, half width
def paste(tex, quad):
    # quad: (x0,y0) top-left, (x1,y1) top-right, (x3,y3) bottom-left of the face; nearest-neighbour fill
    (x0, y0), (x1, y1), (x3, y3) = quad
    for v in range(160):
        for u in range(160):
            px = x0 + (x1 - x0) * u / 160 + (x3 - x0) * v / 160
            py = y0 + (y1 - y0) * u / 160 + (y3 - y0) * v / 160
            c = tex.getpixel((u * 16 // 160, v * 16 // 160))
            fg.putpixel((int(px), int(py)), c + (255,))
            fg.putpixel((int(px) + 1, int(py)), c + (255,))
paste(top, ((cx, cy - a // 2 - 30), (cx + a, cy - 30), (cx - a, cy - 30)))
paste(left, ((cx - a, cy - 30), (cx, cy + a // 2 - 30), (cx - a, cy - 30 + a + 16)))
paste(right, ((cx, cy + a // 2 - 30), (cx + a, cy - 30), (cx, cy + a // 2 - 30 + a + 16)))
fg.save(out + "/drawable-nodpi/ic_launcher_fg.png")
full = bg.convert("RGBA"); full.alpha_composite(fg)
full.resize((192, 192), Image.LANCZOS).save(out + "/mipmap-xxxhdpi/ic_launcher.png")

#!/usr/bin/env python3
"""VoxelCraft 2.3.0: a blueprint for the subway locomotive and car the creator will draw: side, front and top
views on a pixel grid (1 block = 16 px), sizes, a palette and the parts. Our own design."""
import sys
from PIL import Image, ImageDraw, ImageFont
OUT = sys.argv[1]
P = 6  # screen pixels per texture pixel
pal = {"body": (196, 202, 210), "stripe": (34, 120, 214), "dark": (52, 58, 66), "window": (120, 200, 236),
       "door": (150, 158, 168), "light": (255, 236, 150), "red": (214, 56, 48), "bogie": (40, 40, 44), "roof": (150, 156, 164)}
W, H = 1800, 1250
img = Image.new("RGB", (W, H), (22, 30, 44))
d = ImageDraw.Draw(img)
try: font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18); big = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 30)
except OSError: font = big = ImageFont.load_default()
def grid(x0, y0, w, h):
    for i in range(w + 1): d.line((x0 + i * P, y0, x0 + i * P, y0 + h * P), fill=(36, 48, 66) if i % 16 else (70, 92, 120))
    for j in range(h + 1): d.line((x0, y0 + j * P, x0 + w * P, y0 + j * P), fill=(36, 48, 66) if j % 16 else (70, 92, 120))
def px(x0, y0, x, y, w, h, c):
    d.rectangle((x0 + x * P, y0 + y * P, x0 + (x + w) * P - 1, y0 + (y + h) * P - 1), fill=pal[c])
def side(x0, y0, loco):
    L = 64  # 4 blocks long
    grid(x0, y0, L, 40)
    px(x0, y0, 0, 4, L, 28, "body"); px(x0, y0, 0, 2, L, 2, "roof")
    px(x0, y0, 0, 22, L, 3, "stripe")
    for k in range(4):  # windows
        wx = 6 + k * 15
        if loco and k == 0: continue
        px(x0, y0, wx, 9, 8, 8, "window")
    for dx in (20, 44) if not loco else (30, 46):  # sliding doors, two leaves
        px(x0, y0, dx, 7, 10, 24, "door"); px(x0, y0, dx + 1, 9, 3, 9, "window"); px(x0, y0, dx + 6, 9, 3, 9, "window"); px(x0, y0, dx + 5, 7, 1, 24, "dark")
    if loco:
        px(x0, y0, 0, 6, 10, 12, "window"); px(x0, y0, 0, 26, 3, 3, "light"); px(x0, y0, 1, 4, 8, 1, "dark")
    px(x0, y0, 2, 32, L - 4, 3, "dark")
    for bx in (6, L - 14):
        px(x0, y0, bx, 34, 8, 4, "bogie"); px(x0, y0, bx + 1, 37, 2, 2, "dark"); px(x0, y0, bx + 5, 37, 2, 2, "dark")
def front(x0, y0, loco):
    grid(x0, y0, 24, 40)
    px(x0, y0, 0, 4, 24, 28, "body"); px(x0, y0, 0, 2, 24, 2, "roof"); px(x0, y0, 0, 22, 24, 3, "stripe")
    if loco:
        px(x0, y0, 3, 7, 18, 11, "window"); px(x0, y0, 3, 26, 4, 3, "light"); px(x0, y0, 17, 26, 4, 3, "light")
        px(x0, y0, 8, 3, 8, 1, "red"); px(x0, y0, 9, 19, 6, 2, "dark")
    else:
        px(x0, y0, 7, 8, 10, 22, "door"); px(x0, y0, 9, 10, 6, 7, "window")
    px(x0, y0, 2, 32, 20, 3, "dark"); px(x0, y0, 4, 34, 16, 4, "bogie")
def top(x0, y0):
    grid(x0, y0, 64, 24)
    px(x0, y0, 0, 0, 64, 24, "roof"); px(x0, y0, 0, 0, 64, 2, "body"); px(x0, y0, 0, 22, 64, 2, "body")
    for k in range(3): px(x0, y0, 12 + k * 18, 8, 8, 8, "dark")  # roof vents
d.text((30, 18), "VoxelCraft 2.3.0 – METRO: předloha pro kreslení (1 blok = 16 px)", font=big, fill=(240, 244, 250))
d.text((30, 64), "LOKOMOTIVA METRA", font=big, fill=(120, 200, 236))
side(30, 110, True); d.text((30, 360), "Bok: 4 bloky dlouhá (64 px), 2,5 bloku vysoká (40 px)\nkabina vpředu, dvoje posuvné dveře", font=font, fill=(220, 226, 236))
front(460, 110, True); d.text((640, 220), "Čelo:\n1,5 bloku\n(24 px)", font=font, fill=(220, 226, 236))
d.text((30, 410), "VAGON METRA", font=big, fill=(120, 200, 236))
side(30, 456, False); d.text((30, 706), "Bok: 4 bloky, 4 okna,\ndvoje posuvné dveře (otevírají se do stran)", font=font, fill=(220, 226, 236))
front(460, 456, False); d.text((640, 566), "Čelo vagonu:\nprůchozí\ndveře", font=font, fill=(220, 226, 236))
d.text((30, 760), "STŘECHA (oba vozy)", font=big, fill=(120, 200, 236))
top(30, 806); d.text((30, 960), "Shora: 4 × 1,5 bloku, tři větráky", font=font, fill=(220, 226, 236))
x, y = 900, 120
d.text((x, 70), "PALETA", font=big, fill=(120, 200, 236))
names = {"body": "plech vozu", "stripe": "modrý pruh linky", "dark": "rám / spáry", "window": "sklo", "door": "posuvné dveře",
         "light": "světla", "red": "návěst linky", "bogie": "podvozek", "roof": "střecha"}
for k, c in pal.items():
    d.rectangle((x, y, x + 40, y + 40), fill=c, outline=(240, 240, 240)); d.text((x + 52, y + 8), f"{names[k]}  #{c[0]:02x}{c[1]:02x}{c[2]:02x}", font=font, fill=(230, 234, 240)); y += 52
notes = ["CO NAKRESLIT:", "• textura boku 64×40, čela 24×40, střechy 64×24 (pro oba vozy)", "• lokomotiva má kabinu s čelním oknem a světly,",
         "  vagon má na čele průchozí dveře", "• dveře: dvě křídla, každé s oknem (otevírají se do stran)", "• pruh linky můžeš udělat v jiné barvě",
         "VE HŘE:", "• metro je rychlejší než běžný vlak", "• dveře se otevřou ve stanici a zavřou se před odjezdem", "• stejná šířka jako dnešní vagony (1,5 bloku)"]
y += 20
for n in notes: d.text((x, y), n, font=font, fill=(240, 220, 140) if n.endswith(":") else (226, 230, 238)); y += 28
img.save(OUT)

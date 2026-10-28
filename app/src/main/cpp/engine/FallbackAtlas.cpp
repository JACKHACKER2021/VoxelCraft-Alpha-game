#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Blocks.h"
#include "Noise.h"
#include "TexturePack.h"

namespace vc {

#include "CaveTextures.inc"
#include "StoneTexture.inc"

namespace {

#include "ItemSprites.inc"

struct Rgb {
    int r, g, b;
    Rgb scaled(float f) const { return {(int)(r * f), (int)(g * f), (int)(b * f)}; }
    Rgb plus(int d) const { return {r + d, g + d, b + d}; }
};

// Material colours of tools, ingots and armour.
const Rgb kWood{168, 122, 66}, kStone{138, 138, 138}, kIron{190, 192, 200}, kBronze{208, 128, 52},
    kSteel{150, 166, 184}, kRuby{214, 34, 64}, kHandle{120, 84, 44};

class Canvas {
public:
    explicit Canvas(int size) : size_(size), img_(size * size * 4, 0) {}
    std::vector<uint8_t>& image() { return img_; }

    void put(int tile, int px, int py, Rgb c, int a = 255) {
        if (px < 0 || py < 0 || px > 15 || py > 15) return;
        uint8_t* p = at(tile, px, py);
        p[0] = clamp8(c.r);
        p[1] = clamp8(c.g);
        p[2] = clamp8(c.b);
        p[3] = (uint8_t)a;
    }
    void clear(int tile, int px, int py) { put(tile, px, py, {0, 0, 0}, 0); }
    bool filled(int tile, int px, int py) {
        return px >= 0 && py >= 0 && px < 16 && py < 16 && at(tile, px, py)[3] != 0;
    }

    // An item sprite from ASCII art: the shape is filled, lit from the top left, and
    // gets a dark outline around it, so it reads crisply at any size.
    void sprite(int tile, const char* const rows[16], Rgb material, Rgb accent = {255, 255, 255}) {
        int code[16][16];
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) code[y][x] = rows[y][x] == '.' ? 0 : rows[y][x] - '0';
        auto in = [&](int x, int y) { return x >= 0 && y >= 0 && x < 16 && y < 16 && code[y][x] != 0; };
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                if (!code[y][x]) {
                    bool ring = in(x - 1, y) || in(x + 1, y) || in(x, y - 1) || in(x, y + 1);
                    if (ring) put(tile, x, y, {28, 22, 20});
                    else clear(tile, x, y);
                    continue;
                }
                Rgb base = code[y][x] == 2 ? kHandle : code[y][x] == 3 ? accent : material;
                int grain = (int)(hash3(x, y, tile, 77) % 12) - 6;
                float light = 1.0f;
                if (!in(x - 1, y) || !in(x, y - 1)) light = 1.28f;        // lit edge
                else if (!in(x + 1, y) || !in(x, y + 1)) light = 0.68f;   // shaded edge
                put(tile, x, y, base.scaled(light).plus(grain));
            }
    }

private:
    static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
    uint8_t* at(int tile, int px, int py) {
        int x = (tile % kAtlasCols) * 16 + px, y = (tile / kAtlasCols) * 16 + py;
        return &img_[(y * size_ + x) * 4];
    }
    int size_;
    std::vector<uint8_t> img_;
};

// Smooth-ish value noise in 0..1 for soft patches on block faces.
float patch(int x, int y, int seed, int cell) {
    auto v = [&](int cx, int cy) { return (hash3(cx, cy, seed, 91) % 1000) / 1000.0f; };
    int cx = x / cell, cy = y / cell;
    float fx = (x % cell + 0.5f) / cell, fy = (y % cell + 0.5f) / cell;
    float a = v(cx, cy) + (v(cx + 1, cy) - v(cx, cy)) * fx;
    float b = v(cx, cy + 1) + (v(cx + 1, cy + 1) - v(cx, cy + 1)) * fx;
    return a + (b - a) * fy;
}

// Seamless bark of the trees (defined further down).
Rgb barkPixel(int px, int py, Rgb base, int seed);
Rgb netherperkBark(int px, int py);
Rgb voidBark(int px, int py);

// Stone: grey with soft patches, darker cracks and a few light flecks.
Rgb stonePixel(int px, int py, int seed) {
    int n = (int)(hash3(px, py, seed, 1234) % 20) - 10;
    int v = 118 + (int)(patch(px, py, seed, 5) * 26) + n;
    if (hash3(px, py, seed, 7) % 23 == 0) v -= 38;  // crack pixel
    if (hash3(px, py, seed, 8) % 31 == 0) v += 30;  // fleck
    return {v, v, v + 2};
}

// Cobblestone-like cells: rounded stones with dark gaps; `tint` colours the stones.
Rgb cobblePixel(int px, int py, int seed, Rgb tint, bool* gap = nullptr) {
    // Nearest of a few jittered points per 5x5 cell (cheap Voronoi).
    float best = 1e9f, second = 1e9f;
    int bestId = 0;
    for (int cy = -1; cy <= 4; ++cy)
        for (int cx = -1; cx <= 4; ++cx) {
            uint32_t h = hash3((cx + 4) % 4, (cy + 4) % 4, seed, 55);
            float sx = cx * 5 + 1 + h % 3, sy = cy * 5 + 1 + (h >> 4) % 3;
            float d = (px - sx) * (px - sx) + (py - sy) * (py - sy);
            if (d < best) { second = best; best = d; bestId = (int)(h % 7); }
            else if (d < second) second = d;
        }
    bool edge = std::sqrt(second) - std::sqrt(best) < 1.1f;
    if (gap) *gap = edge;
    int n = (int)(hash3(px, py, seed, 4) % 14) - 7;
    if (edge) return Rgb{tint.r, tint.g, tint.b}.scaled(0.48f).plus(n / 2);
    float shade = 0.82f + bestId * 0.04f;
    return tint.scaled(shade).plus(n);
}

// Leaves (1.4.0): clumps of small leaves, each lit from the top left with a dark rim,
// and holes between some clumps so the sky shows through the crown. Needles add thin
// diagonal streaks for the conifers.
void leafPixel(Canvas& a, int tile, int px, int py, Rgb base, int seed, bool needles) {
    float best = 1e9f, second = 1e9f;
    float bx = 0, by = 0;
    int bestId = 0;
    for (int cy = -1; cy <= 4; ++cy)
        for (int cx = -1; cx <= 4; ++cx) {
            uint32_t h = hash3((cx + 4) % 4, (cy + 4) % 4, seed, 31);
            float sx = cx * 4 + 1 + h % 3, sy = cy * 4 + 1 + (h >> 4) % 3;
            float d = (px - sx) * (px - sx) + (py - sy) * (py - sy);
            if (d < best) { second = best; best = d; bestId = (int)(h % 5); bx = sx; by = sy; }
            else if (d < second) second = d;
        }
    const bool rim = std::sqrt(second) - std::sqrt(best) < 1.0f;
    const int n = (int)(hash3(px, py, seed, 5) % 16) - 8;
    if (rim && hash3((int)bx, (int)by, seed, 6) % 3 == 0) {
        a.clear(tile, px, py);  // a gap between the clumps
        return;
    }
    if (rim) {
        a.put(tile, px, py, base.scaled(0.52f).plus(n / 3));
        return;
    }
    float f = 0.82f + bestId * 0.06f;
    if (px - bx + py - by < -0.5f) f += 0.16f;      // lit side of the clump
    else if (px - bx + py - by > 1.5f) f -= 0.1f;   // shaded side
    if (needles && (px + py * 2) % 4 == 0) f -= 0.14f;
    if (!needles && hash3(px, py, seed, 7) % 11 == 0) f += 0.2f;  // a shiny leaf
    a.put(tile, px, py, base.scaled(f).plus(n / 2));
}

// Ore: stone with clusters of coloured crystals that have a light corner.
void ore(Canvas& a, int tile, Rgb color, int seed) {
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            uint32_t h = hash3(px / 3, py / 3, seed, 21);
            bool cluster = h % 4 == 0;
            bool speck = cluster && hash3(px, py, seed, 3) % 4 != 0;
            if (!speck) a.put(tile, px, py, stonePixel(px, py, seed));
            else {
                bool hi = (px % 3 == 0) && (py % 3 == 0);
                a.put(tile, px, py, color.scaled(hi ? 1.25f : 0.95f).plus((int)(hash3(px, py, 2, seed) % 16) - 8));
            }
        }
}

// Storage block: a bevelled metal plate with corner rivets.
void metalBlock(Canvas& a, int tile, Rgb color) {
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            int n = (int)(hash3(px, py, tile, 5) % 10) - 5;
            float f = 1.0f;
            if (px == 0 || py == 0) f = 1.3f;
            else if (px == 15 || py == 15) f = 0.6f;
            else if (px == 1 || py == 1) f = 1.12f;
            else if (px == 14 || py == 14) f = 0.78f;
            else if ((px - py == 0 || px - py == 1) && px > 3 && px < 12) f = 1.15f;  // shine
            bool rivet = (px == 3 || px == 12) && (py == 3 || py == 12);
            a.put(tile, px, py, color.scaled(rivet ? 0.62f : f).plus(n));
        }
}

const Rgb kWoolColors[16] = {{234, 236, 236}, {240, 118, 19},  {189, 68, 179},  {58, 175, 217},
                             {248, 198, 39},  {112, 185, 25},  {237, 141, 172}, {62, 68, 71},
                             {142, 142, 134}, {21, 137, 145},  {121, 42, 172},  {53, 57, 157},
                             {114, 71, 40},   {84, 109, 27},   {160, 39, 34},   {20, 21, 25}};

// 1.1.0: the new Nether, the Crimson Void, mobs, beds and the sky.
const char* const kSprite_meat[16] = {
    "................",
    "................",
    "........1111....",
    "......11111111..",
    ".....1111111111.",
    "....11113111111.",
    "...111131111111.",
    "...111111111111.",
    "..1111111111111.",
    "..111111111111..",
    "..11111111111...",
    ".2211111111.....",
    "222.1111........",
    "22..............",
    "................",
    "................",
};
const char* const kSprite_bed[16] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "33311111111111..",
    "333111111111111.",
    "333111111111111.",
    "2222222222222222",
    "2222222222222222",
    "2..............2",
    "2..............2",
    "................",
    "................",
    "................",
    "................",
};

void drawUpdate11(Canvas& a, int tile) {
    if (tile == T_RAW_PORK) { a.sprite(tile, kSprite_meat, {236, 140, 150}, {255, 230, 230}); return; }
    if (tile == T_COOKED_PORK) { a.sprite(tile, kSprite_meat, {176, 104, 60}, {226, 186, 140}); return; }
    if (tile == T_RAW_MUTTON) { a.sprite(tile, kSprite_meat, {204, 60, 64}, {250, 220, 220}); return; }
    if (tile == T_COOKED_MUTTON) { a.sprite(tile, kSprite_meat, {130, 74, 46}, {210, 170, 120}); return; }
    if (tile == T_BED_ITEM) { a.sprite(tile, kSprite_bed, {190, 40, 40}, {236, 236, 236}); return; }
    const Rgb voidStone{58, 40, 78}, voidGrass{132, 62, 196}, netherGrass{30, 128, 112}, netherrack{116, 50, 50};
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            int n = (int)(hash3(px, py, tile, 4321) % 24) - 12;
            auto voidStonePixel = [&](int seed) {
                // Like stone, but violet with soul-like pale swirls.
                float f = 0.8f + patch(px, py, seed, 4) * 0.4f;
                Rgb c = voidStone.scaled(f).plus(n / 2);
                if (std::sin(px * 0.9f + py * 0.5f + patch(px, py, seed + 1, 5) * 6) > 0.92f) c = Rgb{150, 120, 190}.plus(n / 3);
                return c;
            };
            switch (tile) {
                case T_NETHER_GRASS_TOP: {
                    float f = 0.8f + patch(px, py, 51, 4) * 0.35f;
                    Rgb c = netherGrass.scaled(f).plus(n / 2);
                    if (hash3(px, py, 52, 5) % 9 == 0) c = Rgb{190, 40, 60}.plus(n / 2);  // crimson specks
                    a.put(tile, px, py, c);
                    break;
                }
                case T_NETHER_GRASS_SIDE: {
                    int edge = 3 + (int)(hash3(px, 0, 53, 7) % 4);
                    if (py < edge) a.put(tile, px, py, netherGrass.scaled(py == edge - 1 ? 0.75f : 1.0f).plus(n / 2));
                    else a.put(tile, px, py, netherrack.scaled(0.8f + patch(px, py, 13, 3) * 0.4f).plus(n));
                    break;
                }
                case T_NETHER_SPROUTS: {
                    int blade = px % 2 == 0 ? (int)(hash3(px, 0, 54, 9) % 7) + 3 : -1;
                    if (blade >= 0 && 15 - py <= blade) a.put(tile, px, py, netherGrass.scaled(15 - py == blade ? 1.3f : 1.0f).plus(n));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_VOID_STONE: a.put(tile, px, py, voidStonePixel(60)); break;
                case T_VOID_GRASS_TOP: {
                    float f = 0.8f + patch(px, py, 61, 4) * 0.35f;
                    if (hash3(px, py, 62, 3) % 7 == 0) f += 0.2f;
                    a.put(tile, px, py, voidGrass.scaled(f).plus(n / 2));
                    break;
                }
                case T_VOID_GRASS_SIDE: {
                    int edge = 3 + (int)(hash3(px, 0, 63, 7) % 3);
                    if (py < edge) a.put(tile, px, py, voidGrass.scaled(py == edge - 1 ? 0.75f : 1.0f).plus(n / 2));
                    else a.put(tile, px, py, voidStonePixel(60));
                    break;
                }
                case T_VOID_LOG_SIDE: a.put(tile, px, py, voidBark(px, py)); break;
                case T_EYE_LOG: break;  // drawn whole below (it is animated)
                case T_VOID_LOG_TOP: {
                    const float dx = px - 7.5f, dy = py - 7.5f, r = std::sqrt(dx * dx + dy * dy);
                    if (r > 7.0f || std::max(std::fabs(dx), std::fabs(dy)) > 6.6f) a.put(tile, px, py, voidBark(px, py).scaled(0.9f));
                    else a.put(tile, px, py, Rgb{140, 90, 170}.scaled((int)r % 2 ? 0.8f : 1.0f).plus(n / 3));
                    break;
                }
                case T_VOID_LEAVES: leafPixel(a, tile, px, py, {150, 58, 200}, 65, false); break;
                case T_RED_SAND: {
                    float ripple = ((py + (int)(patch(px, 0, 67, 6) * 3)) % 5 == 0) ? 0.9f : 1.0f;
                    a.put(tile, px, py, Rgb{186, 52, 34}.scaled(ripple * (0.92f + patch(px, py, 68, 3) * 0.16f)).plus(n / 2));
                    break;
                }
                case T_VOID_PORTAL: {
                    int w = (int)(40 * std::sin(px * 0.8f + std::cos(py * 0.6f) * 2.5f));
                    a.put(tile, px, py, {40 + w / 2 + n, 10, 70 + w + n}, 200);
                    break;
                }
                case T_VOID_STONE_BRICKS: {
                    int row = py / 8, off = (row % 2) * 8;
                    bool gap = py % 8 == 7 || (px + off) % 16 == 15;
                    bool hi = py % 8 == 0 || (px + off) % 16 == 0;
                    if (gap) a.put(tile, px, py, Rgb{26, 16, 36}.plus(n / 3));
                    else a.put(tile, px, py, voidStonePixel(69).scaled(hi ? 1.2f : 1.0f));
                    break;
                }
                case T_PUMPKIN_SIDE:
                case T_PUMPKIN_FACE: {
                    bool rib = px % 4 == 0;
                    Rgb c = Rgb{214, 118, 22}.scaled(rib ? 0.78f : 1.0f).plus(n / 2);
                    if (tile == T_PUMPKIN_FACE) {
                        bool eyes = py >= 4 && py <= 6 && ((px >= 3 && px <= 5) || (px >= 10 && px <= 12));
                        bool mouth = (py == 10 && px >= 3 && px <= 12) || (py == 11 && px >= 4 && px <= 11 && px % 3 != 0);
                        if (eyes || mouth) c = Rgb{255, 220, 70};  // glowing carved face
                    }
                    a.put(tile, px, py, c);
                    break;
                }
                case T_PUMPKIN_TOP: {
                    int d = std::abs(px - 7) + std::abs(py - 7);
                    if (d <= 1) a.put(tile, px, py, {90, 70, 30});
                    else a.put(tile, px, py, Rgb{200, 108, 20}.scaled(px % 4 == 0 || py % 4 == 0 ? 0.82f : 1.0f).plus(n / 2));
                    break;
                }
                case T_CRIMSON_ROOTS: {
                    int root = px % 3 == 1 ? (int)(hash3(px, 0, 70, 9) % 9) + 4 : -1;
                    bool curl = root >= 0 && 15 - py <= root && (py % 4 != 0 || px % 2 == 0);
                    if (curl) a.put(tile, px, py, Rgb{150, 20, 30}.scaled(15 - py == root ? 1.3f : 1.0f).plus(n));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_BED_TOP_HEAD:
                case T_BED_TOP_FOOT: {
                    bool pillow = tile == T_BED_TOP_HEAD && py >= 2 && py <= 6 && px >= 2 && px <= 13;
                    bool rim = px == 0 || px == 15;
                    Rgb c = Rgb{180, 36, 36}.scaled(rim ? 0.7f : ((px + py) % 4 == 0 ? 0.92f : 1.0f)).plus(n / 3);
                    if (pillow) c = Rgb{236, 236, 236}.plus(n / 4);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_BED_SIDE: {
                    if (py < 7) a.clear(tile, px, py);
                    else if (py < 11) a.put(tile, px, py, Rgb{180, 36, 36}.scaled(py == 7 ? 1.15f : 1.0f).plus(n / 3));
                    else if (py < 13 || px <= 1 || px >= 14) a.put(tile, px, py, Rgb{150, 110, 64}.plus(n / 3));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_VOID_FLINT_AND_STEEL: {
                    int dx = px - 6, dy = py - 6, d2 = dx * dx + dy * dy;
                    bool ring = d2 >= 9 && d2 <= 22 && !(px > 6 && py > 6);
                    bool flint = px >= 8 && py >= 8 && px + py <= 26 && px + py >= 17;
                    if (ring) a.put(tile, px, py, {170, 120, 230});
                    else if (flint) a.put(tile, px, py, Rgb{70, 30, 100}.plus(n));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_PIG_SKIN: a.put(tile, px, py, Rgb{236, 158, 160}.scaled(0.92f + patch(px, py, 71, 4) * 0.14f).plus(n / 3)); break;
                case T_PIG_FACE: {
                    Rgb c = Rgb{236, 158, 160}.plus(n / 3);
                    bool eye = py >= 4 && py <= 5 && (px == 3 || px == 4 || px == 11 || px == 12);
                    bool snout = px >= 5 && px <= 10 && py >= 8 && py <= 12;
                    bool nostril = snout && py == 10 && (px == 6 || px == 9);
                    if (eye) c = px == 3 || px == 12 ? Rgb{250, 250, 250} : Rgb{20, 20, 20};
                    else if (nostril) c = {120, 60, 60};
                    else if (snout) c = Rgb{246, 178, 180}.plus(n / 4);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_SHEEP_WOOL: {
                    float f = 0.9f + patch(px, py, 72, 3) * 0.18f;
                    a.put(tile, px, py, Rgb{236, 236, 230}.scaled(f).plus(n / 3));
                    break;
                }
                case T_SHEEP_SKIN: a.put(tile, px, py, Rgb{214, 196, 180}.scaled(0.92f + patch(px, py, 79, 3) * 0.14f).plus(n / 3)); break;
                case T_SHEEP_FACE: {
                    Rgb c = Rgb{214, 196, 180}.plus(n / 3);
                    bool wool = py < 3;
                    bool eye = py >= 6 && py <= 7 && (px == 3 || px == 4 || px == 11 || px == 12);
                    bool nose = py >= 11 && py <= 12 && px >= 6 && px <= 9;
                    if (wool) c = Rgb{236, 236, 230}.plus(n / 3);
                    else if (eye) c = px == 3 || px == 12 ? Rgb{250, 250, 250} : Rgb{30, 30, 30};
                    else if (nose) c = {200, 150, 150};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_FORGOTTEN_FACE: {
                    Rgb c = Rgb{96, 112, 96}.scaled(0.9f + patch(px, py, 73, 3) * 0.2f).plus(n / 2);  // grey-green skin
                    bool eye = py >= 6 && py <= 7 && ((px >= 3 && px <= 5) || (px >= 10 && px <= 12));
                    bool mouth = py == 11 && px >= 5 && px <= 10;
                    if (eye) c = Rgb{20, 18, 24};
                    if (eye && (px == 4 || px == 11)) c = {210, 230, 255};  // pale glowing pupils
                    if (mouth) c = {40, 30, 30};
                    if (py < 2) c = Rgb{40, 34, 30}.plus(n / 3);  // hair
                    a.put(tile, px, py, c);
                    break;
                }
                case T_FORGOTTEN_SHIRT: {
                    bool torn = hash3(px, py, 74, 5) % 11 == 0;
                    a.put(tile, px, py, (torn ? Rgb{96, 112, 96} : Rgb{70, 70, 92}).scaled(0.9f + patch(px, py, 75, 4) * 0.2f).plus(n / 2));
                    break;
                }
                case T_FORGOTTEN_SKIN: a.put(tile, px, py, Rgb{96, 112, 96}.scaled(0.88f + patch(px, py, 76, 3) * 0.22f).plus(n / 2)); break;
                case T_CRIMSON_BODY: {
                    // Black stone-like hide with glowing red cracks.
                    Rgb c = Rgb{22, 18, 22}.scaled(0.8f + patch(px, py, 77, 4) * 0.5f).plus(n / 3);
                    if (std::abs(std::sin(px * 0.7f + py * 1.3f) + std::cos(py * 0.4f - px * 0.3f)) < 0.12f) c = Rgb{220, 30, 30}.plus(n / 3);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_CRIMSON_FACE: {
                    Rgb c = Rgb{22, 18, 22}.scaled(0.8f + patch(px, py, 78, 4) * 0.5f).plus(n / 3);
                    bool eye = py >= 5 && py <= 7 && ((px >= 2 && px <= 5) || (px >= 10 && px <= 13));
                    bool mouth = py >= 11 && py <= 12 && px >= 4 && px <= 11;
                    if (eye) c = Rgb{255, 40 + n, 30};
                    else if (mouth) c = {120, 10, 10};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_SUN: {
                    float d = std::max(std::fabs(px - 7.5f), std::fabs(py - 7.5f));
                    if (d < 4.5f) a.put(tile, px, py, {255, 250, 200});
                    else if (d < 7.5f) a.put(tile, px, py, {255, 220, 90}, (int)(220 - (d - 4.5f) * 55));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_MOON: {
                    float d = std::max(std::fabs(px - 7.5f), std::fabs(py - 7.5f));
                    bool crater = (px - 5) * (px - 5) + (py - 6) * (py - 6) <= 2 || (px - 10) * (px - 10) + (py - 10) * (py - 10) <= 1;
                    if (d < 5.5f) a.put(tile, px, py, crater ? Rgb{170, 176, 196} : Rgb{226, 230, 240});
                    else a.clear(tile, px, py);
                    break;
                }
                case T_CLOUD: a.put(tile, px, py, {255, 255, 255}, 200); break;
                case T_RAIN:
                    if (px % 8 == 3 && (py + px * 3) % 16 < 6) a.put(tile, px, py, {176, 196, 236}, 150);
                    else a.clear(tile, px, py);
                    break;
                case T_SNOWFLAKE: {
                    int dx = std::abs(px - 7), dy = std::abs(py - 7);
                    if ((dx == 0 && dy <= 4) || (dy == 0 && dx <= 4) || (dx == dy && dx <= 3)) a.put(tile, px, py, {250, 252, 255});
                    else a.clear(tile, px, py);
                    break;
                }
                case T_STAR: {
                    int dx = std::abs(px - 7), dy = std::abs(py - 7);
                    if (dx + dy <= 2) a.put(tile, px, py, {255, 255, 240});
                    else a.clear(tile, px, py);
                    break;
                }
                default: break;
            }
        }
}

// 1.2.0: TNT, powerstone, new woods, saplings, buckets, swords, eggs...
const char* const kSprite_sword[16] = {
    "..............33",
    ".............333",
    "............313.",
    "...........313..",
    "..........313...",
    ".........313....",
    "........313.....",
    ".......313......",
    "..2...313.......",
    "...2.313........",
    "....222.........",
    "....222.........",
    "...2..2.........",
    "..22............",
    ".22.............",
    "................",
};
const char* const kSprite_bucket[16] = {
    "................",
    "................",
    "...1111111111...",
    "..11........11..",
    "..1333333333.1..",
    "..1333333333.1..",
    "..1.33333333.1..",
    "...1........1...",
    "...1........1...",
    "...1........1...",
    "....1......1....",
    "....1......1....",
    ".....111111.....",
    "................",
    "................",
    "................",
};
const char* const kSprite_egg[16] = {
    "................",
    "......1111......",
    ".....111111.....",
    "....11131111....",
    "....11111111....",
    "...1113111111...",
    "...1111111311...",
    "...1111111111...",
    "...1131111111...",
    "...1111113111...",
    "...1111111111...",
    "....11111111....",
    "....11311111....",
    ".....111111.....",
    "................",
    "................",
};
const char* const kSprite_dust[16] = {
    "................",
    "................",
    "................",
    "......3.........",
    "........1...3...",
    "....1..111......",
    "...3..11111.....",
    ".....1111111....",
    "....111131111...",
    "...11111111111..",
    "..1111111113111.",
    "................",
    "................",
    "................",
    "................",
    "................",
};
const char* const kSprite_chip[16] = {
    "................",
    "...3..3..3..3...",
    "...3..3..3..3...",
    "..111111111111..",
    "..111111111111..",
    "..112222222211..",
    "..112222222211..",
    "..112233332211..",
    "..112233332211..",
    "..112222222211..",
    "..112222222211..",
    "..111111111111..",
    "..111111111111..",
    "...3..3..3..3...",
    "...3..3..3..3...",
    "................",
};
const char* const kSprite_cane[16] = {
    "................",
    ".....11333......",
    "....13...31.....",
    "...31.....33....",
    "...3.......1....",
    "...........3....",
    "..........13....",
    "..........31....",
    ".........13.....",
    ".........31.....",
    "........13......",
    "........31......",
    ".......13.......",
    ".......31.......",
    "................",
    "................",
};
const char* const kSprite_compass[16] = {
    "................",
    ".....111111.....",
    "...1122222211...",
    "..122222222221..",
    "..122223222221..",
    ".12222333222221.",
    ".12222333222221.",
    ".12222232222221.",
    ".12222222222221.",
    ".12222222222221.",
    "..122222222221..",
    "..122222222221..",
    "...1122222211...",
    ".....111111.....",
    "................",
    "................",
};
const char* const kSprite_sapling[16] = {
    "................",
    "................",
    "......1..1......",
    ".....111111.....",
    "....11111111....",
    "...111.11.111...",
    "....11.22.11....",
    "......122.......",
    ".......2..1.....",
    ".......2.11.....",
    ".....1.2111.....",
    ".....1122.......",
    "......12........",
    ".......2........",
    ".......2........",
    "................",
};

const char* const kSprite_bow[16] = {
    "................",
    "................",
    "......22222223..",
    "....22......3...",
    "...22......3....",
    "...2......3.....",
    "..2.......3.....",
    "..2......3......",
    "..2.....3.......",
    "..2....3........",
    "..2..33.........",
    "..2.3...........",
    "..23............",
    "..3.............",
    "................",
    "................",
};
const char* const kSprite_bow_pulled[16] = {
    "................",
    "................",
    "......22222223..",
    "....22.......3..",
    "...21.......3...",
    "...2.1......3...",
    "..2...1.....3...",
    "..2....1...3....",
    "..2.....1..3....",
    "..2......13.....",
    "..2......31.....",
    "..2....33.......",
    "..2.333.........",
    "..33............",
    "................",
    "................",
};
const char* const kSprite_arrow[16] = {
    "................",
    "................",
    "............11..",
    "...........111..",
    "...........21...",
    "..........2.....",
    ".........2......",
    "........2.......",
    ".......2........",
    "......2.........",
    ".....2..........",
    "..3.2...........",
    "..32............",
    ".3333...........",
    "..3.............",
    "................",
};
const char* const kSprite_feather[16] = {
    "................",
    "..........11....",
    "..........11....",
    ".........11.211.",
    "........11.2.11.",
    ".......11.2.11..",
    "......11.2.11...",
    ".....11.2.11....",
    "....11..211.....",
    ".......211......",
    "......211.......",
    ".....2..........",
    "....2...........",
    "...2............",
    "................",
    "................",
};
const char* const kSprite_wheat[16] = {
    "................",
    "................",
    "................",
    "................",
    "...33.33.33.33..",
    "...33.33.33.33..",
    "...33.33.33.33..",
    "...1..1..1..1...",
    "....1.1..1..1...",
    "....1.1..1..1...",
    "....1..1.1.1....",
    "....12222222....",
    ".....1.1.1.1....",
    ".....1.1.1.1....",
    ".....1.1.1.1....",
    "................",
};
const char* const kSprite_bread[16] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "....11111111....",
    "..111111111111..",
    ".11131131131111.",
    ".11113113113111.",
    ".11111111111111.",
    ".11111111111111.",
    "..111111111111..",
    "....11111111....",
    "................",
    "................",
    "................",
};
const char* const kSprite_drumstick[16] = {
    "................",
    "......1.........",
    "...1111111......",
    "..111111111.....",
    "..111111111.....",
    "..111111111.....",
    ".11111111111....",
    "..111111111.....",
    "..111111111.....",
    "..111111131.....",
    "...11111113.....",
    "......1....3....",
    "............3...",
    ".............33.",
    ".............33.",
    "................",
};
const char* const kSprite_beef[16] = {
    "................",
    "................",
    "................",
    "......11111.....",
    "....111111111...",
    "...11111111111..",
    "...13311111111..",
    "..111133111111..",
    "..111111331111..",
    "...11311113311..",
    "...11133311111..",
    "....111113111...",
    "......11111.....",
    "................",
    "................",
    "................",
};

const char* const kSprite_string[16] = {
    "................",
    "............11..",
    "...........1..1.",
    "..........1...1.",
    ".........1...1..",
    "........1...1...",
    ".......1...1....",
    "......1...1.....",
    ".....1...1......",
    "....1...1.......",
    "...1...1........",
    "..1...1.........",
    ".1...1..........",
    ".1..1...........",
    "..11............",
    "................",
};

const char* const kSprite_map[16] = {
    "................",
    "..111111111111..",
    "..133333333331..",
    "..131113311131..",
    "..133313333331..",
    "..133311113331..",
    "..133333313331..",
    "..131133313311..",
    "..133313333331..",
    "..133331111331..",
    "..131133333331..",
    "..133313311131..",
    "..133333333331..",
    "..111111111111..",
    "................",
    "................",
};

const char* const kSprite_ball[16] = {
    "................",
    "................",
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "...1113111111...",
    "..111331111111..",
    "..111111111111..",
    "..111111111111..",
    "...1111111111...",
    "...1111111111...",
    "....11111111....",
    "......1111......",
    "................",
    "................",
};

const char* const kSprite_token[16] = {
    "................",
    ".....111111.....",
    "...1133333311...",
    "..133333333331..",
    "..133331333331..",
    ".13333113333331.",
    ".13333313333331.",
    ".13333313333331.",
    ".13333313333331.",
    ".13333313333331.",
    ".13333111333331.",
    "..133333333331..",
    "..133333333331..",
    "...1133333311...",
    ".....111111.....",
    "................",
};

const char* const kSprite_medal[16] = {
    "....22....22....",
    "....222..222....",
    ".....222222.....",
    "......2222......",
    "......2222......",
    ".....111111.....",
    "...1133333311...",
    "..133331333331..",
    "..133311333331..",
    "..133331333331..",
    "..133331333331..",
    "..133311133331..",
    "...1133333311...",
    ".....111111.....",
    "................",
    "................",
};

const char* const kSprite_popper[16] = {
    "..........3..3..",
    "...........3....",
    ".........3...3..",
    "..........111...",
    ".........11111..",
    "........111111..",
    ".......111111...",
    "......111111....",
    ".....111111.....",
    "....111111......",
    "...111111.......",
    "...11111........",
    "..2111..........",
    ".222............",
    ".22.............",
    "................",
};

const char* const kSprite_hoe[16] = {
    "................",
    "........33......",
    ".......1133.....",
    "........1133....",
    ".........113....",
    ".........212....",
    "........222.....",
    ".......222......",
    "......222.......",
    ".....222........",
    "....222.........",
    "...222..........",
    "..222...........",
    ".222............",
    ".22.............",
    "................",
};

void drawUpdate13(Canvas& a, int tile);
void drawUpdate14(Canvas& a, int tile);
void drawUpdate15(Canvas& a, int tile);
void drawUpdate157(Canvas& a, int tile);
Rgb barkPixel(int px, int py, Rgb base, int seed);

void drawUpdate12(Canvas& a, int tile) {
    if (tile >= T_FIRST_SWORD && tile < T_FIRST_SWORD + 6) {
        const Rgb colors[6] = {kWood, kStone, kIron, kBronze, kSteel, kRuby};
        a.sprite(tile, kSprite_sword, colors[tile - T_FIRST_SWORD], {255, 255, 255});
        return;
    }
    switch (tile) {
        case T_BUCKET: a.sprite(tile, kSprite_bucket, kIron, kIron); return;
        case T_WATER_BUCKET: a.sprite(tile, kSprite_bucket, kIron, {50, 100, 230}); return;
        case T_LAVA_BUCKET: a.sprite(tile, kSprite_bucket, kIron, {240, 120, 20}); return;
        case T_COMPASS: a.sprite(tile, kSprite_compass, {90, 40, 110}, {230, 30, 50}); return;
        case T_EGG_PIG: a.sprite(tile, kSprite_egg, {236, 158, 160}, {200, 90, 100}); return;
        case T_EGG_SHEEP: a.sprite(tile, kSprite_egg, {234, 234, 228}, {214, 196, 180}); return;
        case T_EGG_SLIME: a.sprite(tile, kSprite_egg, {214, 118, 22}, {255, 220, 70}); return;
        case T_EGG_FORGOTTEN: a.sprite(tile, kSprite_egg, {96, 112, 96}, {70, 70, 92}); return;
        case T_POWERSTONE_DUST: a.sprite(tile, kSprite_dust, {40, 200, 230}, {200, 255, 255}); return;
        case T_POWERSTONE_CHIP: a.sprite(tile, kSprite_chip, {40, 120, 60}, {230, 210, 90}); return;
        case T_CANDY_CANE: a.sprite(tile, kSprite_cane, {240, 240, 240}, {220, 30, 40}); return;
        case T_OAK_SAPLING: a.sprite(tile, kSprite_sapling, {70, 140, 50}); return;
        case T_PINE_SAPLING: a.sprite(tile, kSprite_sapling, {40, 92, 60}); return;
        case T_VOID_SAPLING: a.sprite(tile, kSprite_sapling, {150, 60, 200}); return;
        case T_NETHERPERK_SAPLING: a.sprite(tile, kSprite_sapling, {230, 90, 40}); return;
        default: break;
    }
    const Rgb power{40, 210, 235};
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            int n = (int)(hash3(px, py, tile, 8765) % 24) - 12;
            switch (tile) {
                case T_WORLD_END: {
                    // Pitch black with faint cracks of light: the very bottom of the world.
                    bool crack = std::fabs(std::sin(px * 0.9f + py * 0.4f) + std::cos(py * 0.7f - px * 0.2f)) < 0.08f;
                    a.put(tile, px, py, crack ? Rgb{90, 70, 140} : Rgb{8 + n / 4, 6, 12 + n / 4});
                    break;
                }
                case T_TNT_SIDE:
                case T_NUKE_SIDE: {
                    bool band = py >= 5 && py <= 10;
                    bool letters = band && py >= 6 && py <= 9 && (px % 5 == 1 || (py == 6 && px % 5 != 4));
                    Rgb body = tile == T_NUKE_SIDE ? Rgb{60, 160, 40} : Rgb{200, 40, 30};
                    if (letters) a.put(tile, px, py, {30, 30, 30});
                    else if (band) a.put(tile, px, py, Rgb{236, 230, 220}.plus(n / 4));
                    else a.put(tile, px, py, body.scaled(px % 4 == 0 ? 0.8f : 1.0f).plus(n / 3));
                    break;
                }
                case T_TNT_TOP:
                case T_TNT_BOTTOM: {
                    int dx = px - 7, dy = py - 7;
                    bool fuse = tile == T_TNT_TOP && dx * dx + dy * dy <= 2;
                    a.put(tile, px, py, fuse ? Rgb{60, 50, 40} : Rgb{200, 40, 30}.scaled((px + py) % 4 == 0 ? 0.8f : 1.0f).plus(n / 3));
                    break;
                }
                case T_NETHERPERK_LOG_SIDE: a.put(tile, px, py, netherperkBark(px, py)); break;
                case T_NETHERPERK_LOG_TOP: {
                    const float dx = px - 7.5f, dy = py - 7.5f, r = std::sqrt(dx * dx + dy * dy);
                    if (r > 7.0f || std::max(std::fabs(dx), std::fabs(dy)) > 6.6f) a.put(tile, px, py, netherperkBark(px, py).scaled(0.9f));
                    else a.put(tile, px, py, Rgb{230, 120, 60}.scaled((int)r % 2 ? 0.8f : 1.0f).plus(n / 3));
                    break;
                }
                case T_NETHERPERK_CAP: {
                    float f = 0.75f + patch(px, py, 82, 3) * 0.4f;
                    Rgb c = Rgb{230, 80, 30}.scaled(f).plus(n / 2);
                    if (hash3(px, py, 83, 5) % 9 == 0) c = {255, 220, 90};  // glowing spots
                    a.put(tile, px, py, c);
                    break;
                }
                case T_NETHERPERK_PLANKS:
                case T_VOID_PLANKS: {
                    int row = py / 4;
                    bool seam = py % 4 == 3 || (px == (row * 6 + 4) % 16 && py % 4 != 3);
                    Rgb base = tile == T_VOID_PLANKS ? Rgb{96, 60, 130} : Rgb{150, 54, 44};
                    a.put(tile, px, py, base.scaled(seam ? 0.6f : 0.95f + ((px + row * 2) % 5) * 0.02f).plus(n / 3));
                    break;
                }
                case T_POWERSTONE_ORE: {
                    // Cornerstone with glowing cyan crystals.
                    uint32_t h = hash3(px / 3, py / 3, 84, 21);
                    bool speck = h % 4 == 0 && hash3(px, py, 85, 3) % 3 != 0;
                    if (speck) a.put(tile, px, py, power.scaled(px % 3 == 0 ? 1.2f : 0.9f).plus(n / 3));
                    else a.put(tile, px, py, Rgb{116, 50, 50}.scaled(0.8f + patch(px, py, 13, 3) * 0.4f).plus(n));
                    break;
                }
                case T_POWERSTONE_BLOCK: {
                    bool edge = px == 0 || py == 0 || px == 15 || py == 15;
                    bool circuit = (px % 5 == 2 && py > 2 && py < 13) || (py % 5 == 2 && px > 2 && px < 13);
                    a.put(tile, px, py, edge ? Rgb{20, 90, 110} : circuit ? Rgb{200, 255, 255} : power.scaled(0.85f).plus(n / 3));
                    break;
                }
                case T_CABLE:
                case T_CABLE_ON: {
                    bool line = std::abs(px - 7) <= 1 || std::abs(py - 7) <= 1;
                    Rgb c = tile == T_CABLE_ON ? Rgb{80, 240, 255} : Rgb{20, 80, 96};
                    if (line) a.put(tile, px, py, c.plus(n / 4));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_LAMP:
                case T_LAMP_ON: {
                    bool frame = px == 0 || py == 0 || px == 15 || py == 15 || px == 7 || py == 7;
                    Rgb glass = tile == T_LAMP_ON ? Rgb{170, 250, 255} : Rgb{40, 70, 80};
                    a.put(tile, px, py, frame ? Rgb{60, 60, 66}.plus(n / 4) : glass.scaled(0.9f + patch(px, py, 86, 3) * 0.2f).plus(n / 4));
                    break;
                }
                case T_SWITCH:
                case T_SWITCH_ON: {
                    // Seen from above: a stone plate with a lever pointing up or down.
                    bool lever = px >= 7 && px <= 8 && (tile == T_SWITCH_ON ? (py >= 2 && py <= 8) : (py >= 7 && py <= 13));
                    bool knob = px >= 6 && px <= 9 && (tile == T_SWITCH_ON ? (py >= 1 && py <= 3) : (py >= 12 && py <= 14));
                    if (knob) a.put(tile, px, py, tile == T_SWITCH_ON ? power : Rgb{120, 30, 30});
                    else if (lever) a.put(tile, px, py, {120, 90, 50});
                    else a.put(tile, px, py, cobblePixel(px, py, 3, {130, 130, 130}));
                    break;
                }
                case T_POWERSTONE_LIGHT: {
                    bool bright = hash3(px / 2, py / 2, 87, 4) % 3 != 0;
                    a.put(tile, px, py, bright ? Rgb{180 + n, 250, 255} : Rgb{40 + n, 160 + n, 190});
                    break;
                }
                case T_GIFT_SIDE:
                case T_GIFT_TOP: {
                    bool ribbon = std::abs(px - 7) <= 1 || (tile == T_GIFT_TOP && std::abs(py - 7) <= 1);
                    a.put(tile, px, py, ribbon ? Rgb{240, 200, 40}.plus(n / 4) : Rgb{200, 30, 40}.scaled((px + py) % 6 == 0 ? 0.85f : 1.0f).plus(n / 4));
                    break;
                }
                case T_CAKE_TOP: {
                    bool cherry = (px - 7) * (px - 7) + (py - 7) * (py - 7) <= 2;
                    bool sprinkle = hash3(px, py, 88, 7) % 11 == 0;
                    a.put(tile, px, py, cherry ? Rgb{220, 20, 40} : sprinkle ? Rgb{80, 160, 240} : Rgb{250, 244, 236}.plus(n / 4));
                    break;
                }
                case T_CAKE_SIDE: {
                    bool icing = py < 4 + (px % 3 == 0 ? 1 : 0);
                    bool layer = py == 9 || py == 10;
                    a.put(tile, px, py, icing ? Rgb{250, 244, 236} : layer ? Rgb{220, 60, 90} : Rgb{200, 150, 90}.plus(n / 3));
                    break;
                }
                case T_ZOMBIE_FACE: {
                    Rgb c = Rgb{70, 130, 60}.scaled(0.9f + patch(px, py, 89, 3) * 0.2f).plus(n / 2);
                    bool eye = py >= 6 && py <= 7 && ((px >= 3 && px <= 5) || (px >= 10 && px <= 12));
                    bool mouth = py >= 11 && py <= 12 && px >= 4 && px <= 11;
                    if (eye) c = {10, 10, 10};
                    if (eye && (px == 4 || px == 11)) c = {255, 60, 30};  // burning eyes
                    if (mouth) c = (px % 2) ? Rgb{30, 20, 20} : Rgb{220, 220, 200};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_CONFETTI: {
                    static const Rgb kColors[5] = {{240, 60, 60}, {60, 200, 80}, {60, 120, 240}, {250, 210, 40}, {220, 80, 220}};
                    if (px >= 4 && px <= 11 && py >= 6 && py <= 9) a.put(tile, px, py, kColors[(px / 2) % 5]);
                    else a.clear(tile, px, py);
                    break;
                }
                default: break;
            }
        }
}

void drawUpdate13(Canvas& a, int tile) {
    const Rgb meatRaw{230, 120, 120}, meatCooked{160, 90, 50}, bone{240, 236, 224};
    switch (tile) {
        case T_HOE: a.sprite(tile, kSprite_hoe, kIron); return;
        case T_BOW: a.sprite(tile, kSprite_bow, kWood, {230, 230, 230}); return;
        case T_BOW_PULLED: a.sprite(tile, kSprite_bow_pulled, kIron, {230, 230, 230}); return;
        case T_ARROW: a.sprite(tile, kSprite_arrow, {170, 170, 175}, {240, 240, 240}); return;
        case T_FEATHER: a.sprite(tile, kSprite_feather, {245, 245, 245}); return;
        case T_WHEAT_ITEM: a.sprite(tile, kSprite_wheat, {214, 176, 60}, {236, 206, 90}); return;
        case T_BREAD: a.sprite(tile, kSprite_bread, {196, 132, 60}, {236, 196, 120}); return;
        case T_RAW_CHICKEN: a.sprite(tile, kSprite_drumstick, {240, 186, 170}, bone); return;
        case T_COOKED_CHICKEN: a.sprite(tile, kSprite_drumstick, {200, 130, 60}, bone); return;
        case T_RAW_BEEF: a.sprite(tile, kSprite_beef, meatRaw.scaled(0.9f), {250, 230, 220}); return;
        case T_STEAK: a.sprite(tile, kSprite_beef, meatCooked, {110, 60, 30}); return;
        case T_MILK_BUCKET: a.sprite(tile, kSprite_bucket, kIron, {250, 250, 250}); return;
        case T_EGG_CHICKEN: a.sprite(tile, kSprite_egg, {240, 240, 236}, {220, 40, 40}); return;
        case T_EGG_COW: a.sprite(tile, kSprite_egg, {70, 54, 44}, {230, 230, 230}); return;
        case T_EGG_VILLAGER: a.sprite(tile, kSprite_egg, {120, 84, 60}, {80, 180, 90}); return;
        default: break;
    }
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            int n = (int)(hash3(px, py, tile, 9131) % 24) - 12;
            switch (tile) {
                case T_WHEAT0:
                case T_WHEAT1:
                case T_WHEAT2:
                case T_WHEAT3: {
                    // Rows of stalks that grow taller and turn golden; ripe wheat has full ears.
                    const int stage = tile - T_WHEAT0;
                    const int height = 4 + stage * 4;
                    const bool stalk = px % 3 == 1 && py >= 16 - height - (int)(hash3(px, 0, tile, 3) % 2);
                    const bool ear = stage == 3 && px % 3 != 2 && py >= 16 - height - 1 && py < 16 - height + 5;
                    const Rgb green{70, 150, 40}, gold{220, 180, 60};
                    const Rgb col = stage == 3 ? gold : stage == 2 ? Rgb{150, 170, 50} : green;
                    if (ear) a.put(tile, px, py, Rgb{236, 200, 80}.plus(n / 2));
                    else if (stalk) a.put(tile, px, py, col.scaled(py > 12 ? 0.8f : 1.0f).plus(n / 3));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_CHICKEN_BODY:
                    a.put(tile, px, py, Rgb{246, 246, 240}.scaled(0.92f + patch(px, py, 91, 3) * 0.1f).plus(n / 4));
                    break;
                case T_CHICKEN_FACE: {
                    Rgb c = Rgb{246, 246, 240}.plus(n / 4);
                    if (py >= 5 && py <= 6 && (px == 4 || px == 11)) c = {20, 20, 20};       // eyes
                    if (py >= 8 && py <= 10 && px >= 6 && px <= 9) c = {240, 170, 40};      // beak
                    if (py >= 11 && py <= 13 && px >= 7 && px <= 8) c = {210, 30, 30};      // wattle
                    a.put(tile, px, py, c);
                    break;
                }
                case T_COW_SKIN: {
                    bool patchDark = patch(px, py, 92, 5) > 0.55f;
                    a.put(tile, px, py, (patchDark ? Rgb{50, 40, 34} : Rgb{236, 232, 226}).plus(n / 3));
                    break;
                }
                case T_COW_FACE: {
                    Rgb c = (patch(px, py, 93, 4) > 0.6f ? Rgb{50, 40, 34} : Rgb{236, 232, 226}).plus(n / 3);
                    if (py >= 5 && py <= 6 && (px == 3 || px == 12)) c = {15, 15, 15};
                    if (py >= 10 && px >= 4 && px <= 11) c = Rgb{220, 160, 150}.plus(n / 4);  // muzzle
                    if (py >= 12 && py <= 13 && (px == 6 || px == 9)) c = {90, 50, 50};
                    if (py <= 1 && (px <= 2 || px >= 13)) c = {220, 214, 200};             // horns
                    a.put(tile, px, py, c);
                    break;
                }
                case T_VILLAGER_SKIN: a.put(tile, px, py, Rgb{190, 138, 100}.plus(n / 4)); break;
                case T_VILLAGER_FACE: {
                    Rgb c = Rgb{190, 138, 100}.plus(n / 4);
                    if (py <= 2) c = Rgb{80, 54, 36}.plus(n / 3);                               // hair
                    if (py >= 6 && py <= 7 && (px == 4 || px == 11)) c = {40, 90, 40};          // green eyes
                    if (py >= 6 && py <= 7 && (px == 3 || px == 12)) c = {240, 240, 240};
                    if (py >= 5 && py <= 5 && px >= 3 && px <= 12 && px != 7 && px != 8) c = {70, 46, 30};  // brows
                    if (px >= 7 && px <= 8 && py >= 7 && py <= 12) c = Rgb{170, 112, 80};       // a big nose
                    if (py == 13 && px >= 5 && px <= 10) c = {110, 60, 50};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_VILLAGER_ROBE:
                case T_VILLAGER_SMITH:
                case T_VILLAGER_FARMER: {
                    Rgb robe = tile == T_VILLAGER_SMITH ? Rgb{60, 60, 66} : tile == T_VILLAGER_FARMER ? Rgb{90, 130, 60} : Rgb{120, 84, 56};
                    bool belt = py == 8 || py == 9;
                    bool apron = tile == T_VILLAGER_SMITH && px >= 4 && px <= 11 && py >= 4;
                    Rgb c = belt ? Rgb{70, 44, 30} : apron ? Rgb{40, 30, 26} : robe.scaled(px % 4 == 0 ? 0.88f : 1.0f);
                    a.put(tile, px, py, c.plus(n / 3));
                    break;
                }
                case T_CABLE_LINE:
                case T_CABLE_LINE_ON: {
                    Rgb c = tile == T_CABLE_LINE_ON ? Rgb{80, 240, 255} : Rgb{30, 96, 110};
                    a.put(tile, px, py, c.scaled((px + py) % 3 == 0 ? 0.8f : 1.0f).plus(n / 4));
                    break;
                }
                default: a.put(tile, px, py, {255, 0, 255}); break;
            }
        }
}

}  // namespace

std::vector<uint8_t> buildFallbackAtlas(int& sizeOut) {
    // Procedural textures of our own. Real textures can come from a texture pack the
    // player installs (see TexturePack.h); these are what VoxelCraft looks like without one.
    const int size = 16 * kAtlasCols;  // 16 px tiles
    sizeOut = size;
    Canvas a(size);
    const Rgb grassTop{92, 158, 52}, dirt{134, 96, 66}, plank{170, 132, 80};

    for (int tile = 0; tile < T_COUNT; ++tile) {
        if (tile >= T_TORCH) {
            drawUpdate157(a, tile);
            continue;
        }
        if (tile >= T_POWERSTONE_BRICKS) {
            drawUpdate15(a, tile);
            continue;
        }
        if (tile >= T_TAR) {
            drawUpdate14(a, tile);
            continue;
        }
        if (tile >= T_WHEAT0) {
            drawUpdate13(a, tile);
            continue;
        }
        if (tile >= T_WORLD_END) {
            drawUpdate12(a, tile);
            continue;
        }
        if (tile >= T_NETHER_GRASS_TOP) {
            drawUpdate11(a, tile);
            continue;
        }
        if (tile >= T_WOOL && tile < T_WOOL + 16) {
            // Wool: soft woven rows.
            Rgb c = kWoolColors[tile - T_WOOL];
            for (int py = 0; py < 16; ++py)
                for (int px = 0; px < 16; ++px) {
                    float f = ((px + (py / 2) * 2) % 4 < 2 ? 1.02f : 0.96f) * (0.95f + patch(px, py, tile, 4) * 0.1f);
                    a.put(tile, px, py, c.scaled(f).plus((int)(hash3(px, py, tile, 9) % 8) - 4));
                }
            continue;
        }
        if (tile >= T_FIRST_NEW_TOOL && tile < T_FIRST_NEW_TOOL + 9) {
            int mat = (tile - T_FIRST_NEW_TOOL) / 3, kind = (tile - T_FIRST_NEW_TOOL) % 3;
            const Rgb colors[3] = {kBronze, kSteel, kRuby};
            const char* const* shapes[3] = {kSprite_pickaxe, kSprite_axe, kSprite_shovel};
            a.sprite(tile, shapes[kind], colors[mat]);
            continue;
        }
        if (tile >= T_FIRST_ARMOR && tile < T_FIRST_ARMOR + 20) {
            int mat = (tile - T_FIRST_ARMOR) / 4, piece = (tile - T_FIRST_ARMOR) % 4;
            const Rgb colors[5] = {kWood, kStone, kBronze, kSteel, kRuby};
            const char* const* shapes[4] = {kSprite_helmet, kSprite_chestplate, kSprite_leggings, kSprite_boots};
            a.sprite(tile, shapes[piece], colors[mat]);
            continue;
        }
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                int n = (int)(hash3(px, py, tile, 1234) % 24) - 12;  // per-pixel grain
                switch (tile) {
                    case T_STONE: a.put(tile, px, py, stonePixel(px, py, 1)); break;
                    case T_GRASS_TOP: {
                        float f = 0.86f + patch(px, py, 2, 4) * 0.22f;
                        if (hash3(px, py, 3, 3) % 7 == 0) f += 0.14f;  // bright blade tips
                        a.put(tile, px, py, grassTop.scaled(f).plus(n / 2));
                        break;
                    }
                    case T_DIRT: {
                        float f = 0.88f + patch(px, py, 4, 4) * 0.2f;
                        Rgb c = dirt.scaled(f).plus(n);
                        if (hash3(px, py, 5, 6) % 19 == 0) c = {150, 140, 128};  // pebble
                        a.put(tile, px, py, c);
                        break;
                    }
                    case T_GRASS_SIDE: {
                        int edge = 3 + (int)(hash3(px, 0, 99, 7) % 3);
                        if (py < edge) a.put(tile, px, py, grassTop.scaled(py == edge - 1 ? 0.78f : 1.0f).plus(n / 2));
                        else {
                            Rgb c = dirt.scaled(0.88f + patch(px, py, 4, 4) * 0.2f).plus(n);
                            if (hash3(px, py, 5, 6) % 19 == 0) c = {150, 140, 128};
                            a.put(tile, px, py, c);
                        }
                        break;
                    }
                    case T_COBBLE: a.put(tile, px, py, cobblePixel(px, py, 1, {136, 136, 136})); break;
                    case T_MOSSY_COBBLE: {
                        bool gap;
                        Rgb c = cobblePixel(px, py, 1, {136, 136, 136}, &gap);
                        if (patch(px, py, 12, 4) > 0.5f && !gap) c = Rgb{86, 130, 54}.plus(n);
                        a.put(tile, px, py, c);
                        break;
                    }
                    case T_PLANKS: {
                        int row = py / 4;
                        bool seam = py % 4 == 3 || (px == (row * 7 + 3) % 16 && py % 4 != 3);
                        float f = seam ? 0.66f : (0.92f + ((px * 3 + row * 5) % 7) * 0.025f);
                        if (!seam && (px + row * 4) % 9 == 0) f -= 0.08f;  // grain line
                        a.put(tile, px, py, plank.scaled(f).plus(n / 3));
                        break;
                    }
                    case T_BEDROCK: {
                        int v = 40 + (int)(patch(px, py, 6, 3) * 70) + n;
                        a.put(tile, px, py, {v, v, v});
                        break;
                    }
                    case T_WATER: {
                        float w = std::sin((px + py * 0.6f) * 0.7f) * 0.5f + 0.5f;
                        a.put(tile, px, py, Rgb{40, 82, 214}.scaled(0.9f + w * 0.2f).plus(n / 3), 180);
                        break;
                    }
                    case T_SAND: {
                        float ripple = ((py + (int)(patch(px, 0, 8, 6) * 3)) % 5 == 0) ? 0.94f : 1.0f;
                        a.put(tile, px, py, Rgb{220, 208, 162}.scaled(ripple).plus(n / 2));
                        break;
                    }
                    case T_GRAVEL: {
                        uint32_t h = hash3(px / 2, py / 2, 9, 9);
                        int v = 100 + (int)(h % 60) + n / 2;
                        a.put(tile, px, py, {v + 4, v, v - 4});
                        break;
                    }
                    case T_LOG_SIDE: a.put(tile, px, py, barkPixel(px, py, {110, 82, 50}, 4)); break;
                    case T_LOG_TOP: {
                        // 1.5.0: round growth rings inside a ring of bark, with a crack to the middle.
                        const float dx = px - 7.5f, dy = py - 7.5f, r = std::sqrt(dx * dx + dy * dy);
                        if (r > 7.0f || std::max(std::fabs(dx), std::fabs(dy)) > 6.6f) a.put(tile, px, py, barkPixel(px, py, {110, 82, 50}, 4).scaled(0.9f));
                        else {
                            const bool ring = (int)(r + patch(px, py, 41, 4) * 0.8f) % 2 == 0;
                            const bool crack = px == 8 && py < 7 && py > 2;
                            a.put(tile, px, py, Rgb{190, 152, 96}.scaled(crack ? 0.6f : ring ? 0.84f : 1.0f).plus(n / 3));
                        }
                        break;
                    }
                    case T_LEAVES: leafPixel(a, tile, px, py, {62, 132, 40}, 77, false); break;
                    case T_GLASS: {
                        bool edge = px == 0 || py == 0 || px == 15 || py == 15;
                        bool shine = (px == py + 3 && px < 8) || (px == py + 5 && px < 7);
                        if (edge) a.put(tile, px, py, {200, 222, 232});
                        else if (shine) a.put(tile, px, py, {240, 250, 255}, 220);
                        else a.clear(tile, px, py);
                        break;
                    }
                    case T_BRICKS: {
                        int row = py / 4, off = (row % 2) * 4;
                        bool mortar = py % 4 == 3 || (px + off) % 8 == 7;
                        bool top = py % 4 == 0;
                        if (mortar) a.put(tile, px, py, Rgb{178, 170, 160}.plus(n / 3));
                        else a.put(tile, px, py, Rgb{152, 72, 56}.scaled(top ? 1.15f : 1.0f).plus(n));
                        break;
                    }
                    case T_STONE_BRICKS: {
                        int row = py / 8, off = (row % 2) * 8;
                        bool gap = py % 8 == 7 || (px + off) % 16 == 15;
                        bool hi = py % 8 == 0 || (px + off) % 16 == 0;
                        if (gap) a.put(tile, px, py, Rgb{70, 70, 72}.plus(n / 3));
                        else a.put(tile, px, py, stonePixel(px, py, 14).scaled(hi ? 1.15f : 1.0f));
                        break;
                    }
                    case T_LAVA: {
                        float w = std::sin((px + py * 0.5f) * 0.8f) * 0.5f + 0.5f;
                        float c = patch(px, py, 11, 4);
                        a.put(tile, px, py, {225 + n / 2, 70 + (int)(w * 70 + c * 50) + n, 15 + (int)(c * 30)});
                        break;
                    }
                    case T_OBSIDIAN: {
                        bool fleck = hash3(px, py, 5, 11) % 9 == 0;
                        if (fleck) a.put(tile, px, py, Rgb{88, 56, 128}.plus(n));
                        else a.put(tile, px, py, Rgb{22, 16, 36}.plus((int)(patch(px, py, 5, 4) * 16) + n / 4));
                        break;
                    }
                    case T_NETHERRACK: {
                        float f = 0.8f + patch(px, py, 13, 3) * 0.4f;
                        a.put(tile, px, py, Rgb{116, 50, 50}.scaled(f).plus(n));
                        break;
                    }
                    case T_FARMLAND: {
                        // Tilled earth: dark furrows with lighter ridges.
                        bool ridge = py % 4 == 1 || py % 4 == 2;
                        a.put(tile, px, py, Rgb{96, 62, 40}.scaled(ridge ? 1.15f : 0.75f).plus(n / 2));
                        break;
                    }
                    case T_HAY_TOP: {
                        int d = std::max(std::abs(px * 2 - 15), std::abs(py * 2 - 15)) / 2;
                        bool band = d == 3 || d == 6;
                        a.put(tile, px, py, (band ? Rgb{150, 110, 30} : Rgb{214, 176, 60}).plus(n / 2));
                        break;
                    }
                    case T_PORTAL: {
                        int w = (int)(30 * std::sin(px * 0.9f + std::cos(py * 0.7f) * 2));
                        a.put(tile, px, py, {120 + w + n, 30 + n / 2, 210 + w / 2 + n}, 190);
                        break;
                    }
                    case T_NETHER_BRICK: {
                        bool mortar = py % 4 == 3 || ((px + (py / 4 % 2) * 4) % 8 == 7);
                        if (mortar) a.put(tile, px, py, {28, 12, 16});
                        else a.put(tile, px, py, Rgb{64, 30, 36}.scaled(py % 4 == 0 ? 1.25f : 1.0f).plus(n / 2));
                        break;
                    }
                    case T_HAY_SIDE: {
                        bool band = py == 3 || py == 4 || py == 11 || py == 12;
                        float straw = (px % 3 == 0 ? 0.85f : 1.0f) * (0.95f + patch(px, py, 14, 2) * 0.1f);
                        a.put(tile, px, py, (band ? Rgb{150, 40, 30} : Rgb{214, 176, 60}.scaled(straw)).plus(n / 2));
                        break;
                    }
                    case T_SANDSTONE_TOP: a.put(tile, px, py, Rgb{218, 206, 158}.plus(n / 3)); break;
                    case T_SANDSTONE_SIDE: {
                        float f = py < 3 ? 1.06f : (py % 5 == 4 ? 0.88f : 1.0f);
                        a.put(tile, px, py, Rgb{216, 202, 152}.scaled(f).plus(n / 3));
                        break;
                    }
                    case T_BOOKSHELF: {
                        bool board = py < 2 || py > 13 || py == 7 || py == 8;
                        if (board) a.put(tile, px, py, plank.scaled(py == 7 || py == 1 ? 0.7f : 1.0f).plus(n / 3));
                        else {
                            static const Rgb spines[] = {{140, 40, 40}, {40, 70, 140}, {60, 110, 50}, {150, 120, 50}, {90, 50, 110}};
                            int book = (px + (py > 8 ? 2 : 0)) / 2;
                            Rgb c = spines[hash3(book, py > 8, 3, 3) % 5];
                            bool gap = px % 2 == 1 && hash3(book, py > 8, 4, 4) % 3 == 0;
                            a.put(tile, px, py, gap ? Rgb{40, 30, 20} : c.scaled(px % 2 ? 0.8f : 1.0f).plus(n / 3));
                        }
                        break;
                    }
                    case T_SNOW: a.put(tile, px, py, Rgb{240, 248, 252}.plus(n / 4 - (int)(patch(px, py, 15, 4) * 10))); break;
                    case T_CLAY: a.put(tile, px, py, Rgb{158, 164, 178}.scaled(0.92f + patch(px, py, 16, 4) * 0.14f).plus(n / 3)); break;
                    case T_COAL_ORE: ore(a, tile, {34, 34, 38}, 21); break;
                    case T_IRON_ORE: ore(a, tile, {214, 172, 140}, 22); break;
                    case T_BRONZE_ORE: ore(a, tile, kBronze, 23); break;
                    case T_STEEL_ORE: ore(a, tile, {176, 196, 220}, 24); break;
                    case T_RUBY_ORE: ore(a, tile, kRuby, 25); break;
                    case T_BRONZE_BLOCK: metalBlock(a, tile, kBronze); break;
                    case T_STEEL_BLOCK: metalBlock(a, tile, kSteel); break;
                    case T_RUBY_BLOCK: metalBlock(a, tile, kRuby); break;
                    case T_CRAFTING_TOP: {
                        bool line = px == 0 || py == 0 || px == 15 || py == 15 || px == 5 || px == 10 || py == 5 || py == 10;
                        a.put(tile, px, py, plank.scaled(line ? 0.62f : 1.0f).plus(n / 3));
                        break;
                    }
                    case T_CRAFTING_SIDE:
                    case T_CRAFTING_FRONT: {
                        float f = (py % 4 == 3) ? 0.72f : 1.0f;
                        if (py < 3) f = 0.6f;  // dark band under the top
                        bool tool = tile == T_CRAFTING_FRONT &&
                                    ((px >= 3 && px <= 5 && py >= 5 && py <= 13) || (py == 5 && px >= 2 && px <= 6) ||
                                     (px + py == 20 && px >= 8 && px <= 13));
                        if (tool) a.put(tile, px, py, Rgb{78, 66, 58}.plus(n / 3));
                        else a.put(tile, px, py, Rgb{160, 120, 70}.scaled(f).plus(n / 3));
                        break;
                    }
                    case T_FURNACE_TOP: a.put(tile, px, py, stonePixel(px, py, 30).scaled(0.92f)); break;
                    case T_FURNACE_SIDE:
                    case T_FURNACE_FRONT:
                    case T_FURNACE_FRONT_LIT: {
                        bool edge = px == 0 || py == 0 || px == 15 || py == 15;
                        bool hole = tile != T_FURNACE_SIDE && px >= 4 && px <= 11 && py >= 8 && py <= 13;
                        bool slot = tile != T_FURNACE_SIDE && px >= 4 && px <= 11 && py == 4;
                        if (hole && tile == T_FURNACE_FRONT_LIT)
                            a.put(tile, px, py, {255, 150 + n * 2 - (13 - py) * 12, 30 + n});
                        else if (hole || slot) a.put(tile, px, py, {24, 22, 22});
                        else a.put(tile, px, py, stonePixel(px, py, 31).scaled(edge ? 0.75f : 1.0f));
                        break;
                    }
                    case T_STICK:
                        if (px + py == 15 && px >= 3 && px <= 12) a.put(tile, px, py, {136, 96, 52});
                        else if (px + py == 16 && px >= 4 && px <= 12) a.put(tile, px, py, {84, 58, 30});
                        else if ((px + py == 14 && px >= 3 && px <= 11) || (px + py == 17 && px >= 5 && px <= 12))
                            a.put(tile, px, py, {40, 28, 18});
                        else a.clear(tile, px, py);
                        break;
                    case T_COAL: {
                        int dx = px - 8, dy = py - 8;
                        int d = dx * dx + dy * dy * 2;
                        if (d <= 34 + (int)(hash3(px, py, 9, 9) % 8))
                            a.put(tile, px, py, Rgb{40, 40, 46}.scaled(dx + dy < -3 ? 1.6f : 1.0f).plus(n / 2));
                        else if (d <= 48) a.put(tile, px, py, {12, 12, 14});
                        else a.clear(tile, px, py);
                        break;
                    }
                    case T_FLINT:
                        if (std::abs(px - 8) + std::abs(py - 9) <= 5 - (py < 6 ? 1 : 0))
                            a.put(tile, px, py, Rgb{58, 58, 64}.scaled(px < 8 && py < 9 ? 1.4f : 1.0f).plus(n / 2));
                        else if (std::abs(px - 8) + std::abs(py - 9) <= 6) a.put(tile, px, py, {20, 20, 22});
                        else a.clear(tile, px, py);
                        break;
                    case T_TALL_GRASS: {
                        // Blades of different heights growing from the bottom, leaning a little,
                        // darker at the root and lighter towards the tips.
                        a.clear(tile, px, py);
                        for (int k = 0; k < 6; ++k) {
                            const uint32_t h = hash3(k, 0, 31, 9);
                            const int root = 1 + k * 3 - (k > 2 ? 1 : 0), height = 6 + (int)(h % 9);
                            const int up = 15 - py;
                            if (up > height) continue;
                            const int lean = (int)((h >> 5) % 3) - 1;
                            if (px != root + (up * lean) / 6) continue;
                            const float t = (float)up / height;
                            a.put(tile, px, py, Rgb{68, 136, 44}.scaled(0.75f + t * 0.5f).plus(n / 2));
                        }
                        break;
                    }
                    case T_DANDELION:
                    case T_POPPY: {
                        bool stem = px == 7 && py >= 8;
                        bool leaf = py == 11 && (px == 5 || px == 6 || px == 8 || px == 9);
                        int dx = px - 7, dy = py - 5;
                        bool petal = dx * dx + dy * dy <= 6;
                        bool middle = dx * dx + dy * dy <= 1;
                        if (middle) a.put(tile, px, py, {tile == T_POPPY ? 30 : 230, tile == T_POPPY ? 25 : 150, 20});
                        else if (petal) a.put(tile, px, py, {tile == T_POPPY ? 210 + n / 2 : 250, tile == T_POPPY ? 30 : 220 + n / 2, 25});
                        else if (stem || leaf) a.put(tile, px, py, {60 + n, 140 + n, 40});
                        else a.clear(tile, px, py);
                        break;
                    }
                    case T_APPLE: {
                        int dx = px - 8, dy = py - 9;
                        bool body = dx * dx + dy * dy <= 30;
                        bool rim = !body && dx * dx + dy * dy <= 40;
                        bool stem = px == 8 && py >= 1 && py <= 3;
                        bool leaf = py == 2 && (px == 9 || px == 10);
                        if (stem) a.put(tile, px, py, {90, 60, 30});
                        else if (leaf) a.put(tile, px, py, {60, 150, 40});
                        else if (body) a.put(tile, px, py, Rgb{204, 32, 32}.scaled(dx < -1 && dy < -1 ? 1.25f : dx > 2 ? 0.75f : 1.0f).plus(n / 3));
                        else if (rim && py > 3) a.put(tile, px, py, {70, 10, 10});
                        else a.clear(tile, px, py);
                        break;
                    }
                    case T_FLINT_AND_STEEL: {
                        // Item icon: a grey steel ring and a dark flint, transparent background.
                        int dx = px - 6, dy = py - 6;
                        int d2 = dx * dx + dy * dy;
                        bool ring = d2 >= 9 && d2 <= 22 && !(px > 6 && py > 6);
                        bool flint = px >= 8 && py >= 8 && px + py <= 26 && px + py >= 17;
                        if (ring) a.put(tile, px, py, {196, 196, 206});
                        else if (flint) a.put(tile, px, py, Rgb{60, 60, 66}.plus(n));
                        else a.clear(tile, px, py);
                        break;
                    }
                    case T_SPRUCE_LOG_SIDE: a.put(tile, px, py, barkPixel(px, py, {66, 46, 28}, 6)); break;
                    case T_SPRUCE_LOG_TOP: {
                        const float dx = px - 7.5f, dy = py - 7.5f, r = std::sqrt(dx * dx + dy * dy);
                        if (r > 7.0f || std::max(std::fabs(dx), std::fabs(dy)) > 6.6f) a.put(tile, px, py, barkPixel(px, py, {66, 46, 28}, 6).scaled(0.9f));
                        else {
                            const bool ring = (int)(r + patch(px, py, 43, 4) * 0.8f) % 2 == 0;
                            a.put(tile, px, py, Rgb{156, 116, 70}.scaled(ring ? 0.82f : 1.0f).plus(n / 3));
                        }
                        break;
                    }
                    case T_SPRUCE_LEAVES: leafPixel(a, tile, px, py, {40, 90, 58}, 79, true); break;
                    case T_SPRUCE_PLANKS: {
                        int row = py / 4;
                        bool seam = py % 4 == 3 || (px == (row * 5 + 6) % 16 && py % 4 != 3);
                        a.put(tile, px, py, Rgb{114, 84, 50}.scaled(seam ? 0.62f : 0.95f + ((px + row * 3) % 5) * 0.02f).plus(n / 3));
                        break;
                    }
                    case T_ICE: {
                        bool streak = (px + py) % 7 == 0 || (px - py + 16) % 11 == 0;
                        a.put(tile, px, py, Rgb{150, 192, 242}.scaled(streak ? 1.12f : 1.0f).plus(n / 4));  // 1.7.0: solid ice
                        break;
                    }
                    case T_CACTUS_SIDE: {
                        bool rib = px % 4 == 1;
                        bool spine = (px % 4 == 3) && hash3(px, py, 18, 2) % 4 == 0;
                        if (spine) a.put(tile, px, py, {222, 220, 170});
                        else a.put(tile, px, py, Rgb{58, 128, 40}.scaled(rib ? 0.75f : 1.0f).plus(n / 2));
                        break;
                    }
                    case T_CACTUS_TOP: {
                        int d = std::max(std::abs(px * 2 - 15), std::abs(py * 2 - 15)) / 2;
                        a.put(tile, px, py, Rgb{70, 140, 48}.scaled(d >= 6 ? 0.8f : d % 3 == 0 ? 0.9f : 1.05f).plus(n / 2));
                        break;
                    }
                    case T_HARDENED_STONE: {
                        Rgb c = stonePixel(px, py, 40).scaled(0.72f);
                        if ((px * 3 + py * 5) % 13 == 0) c = c.scaled(0.75f);  // tight, dense cracks
                        a.put(tile, px, py, Rgb{c.r - 4, c.g - 2, c.b + 8});
                        break;
                    }
                    case T_CHEST_TOP:
                    case T_CHEST_SIDE:
                    case T_CHEST_FRONT: {
                        bool rim = px == 0 || px == 15 || py == 0 || py == 15;
                        bool lidLine = tile != T_CHEST_TOP && py == 5;
                        bool latch = tile == T_CHEST_FRONT && px >= 7 && px <= 8 && py >= 4 && py <= 7;
                        if (latch) a.put(tile, px, py, {196, 196, 204});
                        else if (rim || lidLine) a.put(tile, px, py, Rgb{70, 46, 22}.plus(n / 3));
                        else a.put(tile, px, py, Rgb{164, 116, 56}.scaled(py % 4 == 2 ? 0.88f : 1.0f).plus(n / 3));
                        break;
                    }
                    case T_OAK_DOOR_LOWER:
                    case T_OAK_DOOR_UPPER: {
                        bool frame = px <= 1 || px >= 14 || py == 0 || py == 15;
                        bool window = tile == T_OAK_DOOR_UPPER && px >= 3 && px <= 12 && py >= 3 && py <= 10 && px != 7 && px != 8 && py != 6;
                        bool handle = tile == T_OAK_DOOR_LOWER && px == 12 && (py == 1 || py == 2);
                        if (window) a.clear(tile, px, py);
                        else if (handle) a.put(tile, px, py, {60, 60, 64});
                        else a.put(tile, px, py, plank.scaled(frame ? 0.7f : (px % 4 == 0 ? 0.85f : 1.0f)).plus(n / 3));
                        break;
                    }
                    case T_IRON_DOOR_LOWER:
                    case T_IRON_DOOR_UPPER: {
                        bool frame = px <= 1 || px >= 14 || py == 0 || py == 15;
                        bool window = tile == T_IRON_DOOR_UPPER && py >= 3 && py <= 9 && (px == 4 || px == 7 || px == 10);
                        bool rivet = (px == 3 || px == 12) && (py == 3 || py == 12);
                        if (window) a.clear(tile, px, py);
                        else a.put(tile, px, py, Rgb{200, 200, 206}.scaled(frame ? 0.78f : rivet ? 0.6f : 1.0f).plus(n / 4));
                        break;
                    }
                    case T_TRAPDOOR: {
                        bool hole = (px % 5 >= 2 && px % 5 <= 3) && (py % 5 >= 2 && py % 5 <= 3) && px < 15 && py < 15;
                        bool frame = px == 0 || px == 15 || py == 0 || py == 15;
                        if (hole) a.clear(tile, px, py);
                        else a.put(tile, px, py, plank.scaled(frame ? 0.68f : 0.95f).plus(n / 3));
                        break;
                    }
                    default: break;
                }
            }
        // Items drawn from sprites (after the per-pixel pass for simplicity).
        switch (tile) {
            case T_WOOD_PICKAXE: case T_STONE_PICKAXE: case T_IRON_PICKAXE:
            case T_WOOD_AXE: case T_STONE_AXE: case T_IRON_AXE:
            case T_WOOD_SHOVEL: case T_STONE_SHOVEL: case T_IRON_SHOVEL: {
                int kind = (tile - T_WOOD_PICKAXE) / 3, mat = (tile - T_WOOD_PICKAXE) % 3;
                const Rgb colors[3] = {kWood, kStone, kIron};
                const char* const* shapes[3] = {kSprite_pickaxe, kSprite_axe, kSprite_shovel};
                a.sprite(tile, shapes[kind], colors[mat]);
                break;
            }
            case T_IRON_INGOT: a.sprite(tile, kSprite_ingot, kIron); break;
            case T_BRONZE_INGOT: a.sprite(tile, kSprite_ingot, kBronze); break;
            case T_STEEL_INGOT: a.sprite(tile, kSprite_ingot, kSteel); break;
            case T_RUBY: a.sprite(tile, kSprite_gem, kRuby, {255, 170, 186}); break;
            case T_OAK_DOOR_ITEM: a.sprite(tile, kSprite_door, kWood, {60, 60, 64}); break;
            case T_IRON_DOOR_ITEM: a.sprite(tile, kSprite_door, kIron, {90, 90, 96}); break;
            case T_BOAT: a.sprite(tile, kSprite_boat, kWood); break;
            default: break;
        }
    }
    // The animated eyes, in their first frame.
    for (int kind = 0; kind < 2; ++kind) {
        uint8_t eye[16 * 16 * 4];
        drawEyeTile(eye, kind, 0);
        const int tile = kind == 0 ? T_EYE_LOG : T_VOID_BRICK_EYE;
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const uint8_t* p = eye + (py * 16 + px) * 4;
                a.put(tile, px, py, {p[0], p[1], p[2]}, p[3]);
            }
    }
    // 1.6.0 Caves and Tunes: the user's cave textures (and new looks for gravel, tar blocks,
    // dripstone, debris gravel and the powerstone dust and chip).
    {
        static const int kCaveTiles[] = {T_DEEPSTONE, T_HARDIUM_ORE, T_MOSSY_DEEPSTONE, T_MOSS_BLOCK, T_FUNGAL_DEEPSTONE,
                                         T_DEEPSTONE_BRICKS, T_CRACKED_DEEPSTONE_BRICKS, T_MOSSY_DEEPSTONE_BRICKS, T_GRAVEL,
                                         T_ROTTEN_PLANKS, T_VINES, T_TAR_BLOCK, T_DRIPSTONE, T_DEBRIS_GRAVEL, T_POWERSTONE_DUST,
                                         T_POWERSTONE_CHIP};
        for (int i = 0; i < (int)(sizeof(kCaveTiles) / sizeof(kCaveTiles[0])); ++i)
            for (int py = 0; py < 16; ++py)
                for (int px = 0; px < 16; ++px) {
                    const uint8_t* p = kCaveTextures[i] + (py * 16 + px) * 4;
                    if (p[3] < 128) a.clear(kCaveTiles[i], px, py);
                    else a.put(kCaveTiles[i], px, py, {p[0], p[1], p[2]});
                }
        // And the user's new stone.
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const uint8_t* p = kStoneTexture + (py * 16 + px) * 3;
                a.put(T_STONE, px, py, {p[0], p[1], p[2]});
            }
        a.sprite(T_HARDIUM, kSprite_gem, {70, 200, 225}, {225, 250, 255});
        a.sprite(T_HARDIUM_PICKAXE, kSprite_pickaxe, {80, 205, 230});
        // 1.7.0: the rest of the hardium set, in the same icy cyan.
        a.sprite(T_HARDIUM_SWORD, kSprite_sword, {80, 205, 230}, {255, 255, 255});
        a.sprite(T_HARDIUM_AXE, kSprite_axe, {80, 205, 230});
        a.sprite(T_HARDIUM_SHOVEL, kSprite_shovel, {80, 205, 230});
        {
            const char* const* shapes[4] = {kSprite_helmet, kSprite_chestplate, kSprite_leggings, kSprite_boots};
            for (int k = 0; k < 4; ++k) a.sprite(T_HARDIUM_ARMOR + k, shapes[k], {80, 205, 230});
        }
        // 1.7.0 the Celestial Reach: pale, sunlit blocks with gold and soul-cyan flecks.
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const int n = (int)(hash3(px, py, 1556, 5) % 14) - 7;
                const bool soul = hash3(px, py, 1560, 7) % 21 == 0;
                const Rgb soulC{150, 240, 240}, gold{232, 200, 110};
                // Log: pale pearly bark (light, like birch) in soft vertical bands, short dark lenticel
                // marks across it, a thin gold vein winding up, and a lit left edge on each band.
                {
                    const int band = px / 4, inBand = px % 4;
                    const float shade = 0.9f + 0.06f * (float)((band * 7 + 3) % 4) + (inBand == 0 ? 0.08f : 0.0f) - (inBand == 3 ? 0.07f : 0.0f);
                    Rgb bark = Rgb{226, 228, 238}.scaled(shade).plus(n / 2);
                    const uint32_t mh = hash3(band, py / 2, 1558, 5);
                    if (mh % 5 == 0 && inBand >= 1 && inBand <= 2 && py % 2 == 0) bark = Rgb{120, 120, 140};  // lenticels
                    const int vein = (int)(7 + std::sin(py * 0.55f) * 3.2f);
                    if (px == vein) bark = gold;
                    if (std::abs(px - vein) == 1 && hash3(px, py, 1559, 2) % 3 == 0) bark = gold.scaled(0.75f);
                    a.put(T_CELESTIAL_LOG_SIDE, px, py, bark);
                }
                const float rr = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                a.put(T_CELESTIAL_LOG_TOP, px, py, rr > 7 ? Rgb{205, 205, 218}.plus(n) : ((int)rr % 2 ? Rgb{236, 226, 196} : Rgb{222, 206, 160}).plus(n / 2));
                // Planks: cream boards with gold nail heads.
                const bool board = py % 4 == 3;
                const bool nail = py % 4 == 1 && (px == 2 || px == 13);
                a.put(T_CELESTIAL_PLANKS, px, py, nail ? gold : board ? Rgb{196, 186, 160}.plus(n / 2) : Rgb{232, 222, 196}.plus(n / 2));
                // Stone and bricks: white-blue stone; bricks with gold mortar.
                const Rgb stone = Rgb{206, 214, 232}.plus(n);
                a.put(T_CELESTIAL_STONE, px, py, soul ? soulC : stone);
                const bool mortar = py % 8 == 7 || (px + (py / 8) * 4) % 8 == 7;
                a.put(T_CELESTIAL_STONE_BRICKS, px, py, mortar ? gold.scaled(0.8f) : stone.scaled(1.04f));
                // Soul grass of the sky: pale mint with cyan souls; dirt is light sandy grey.
                const Rgb grass = Rgb{150, 222, 190}.plus(n), dirt = Rgb{186, 176, 162}.plus(n);
                a.put(T_CELESTIAL_GRASS_TOP, px, py, soul ? soulC : grass);
                const int fringe = 3 + (int)(hash3(px, 0, 1561, 9) % 3);
                a.put(T_CELESTIAL_GRASS_SIDE, px, py, py < fringe ? grass : soul ? soulC : dirt);
                a.put(T_CELESTIAL_DIRT, px, py, soul ? soulC : dirt);
                leafPixel(a, T_CELESTIAL_LEAVES, px, py, {120, 200, 235}, 81, false);
            }
        // 1.8.0 Concrete & Old World.
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const int n = (int)(hash3(px, py, 1563, 5) % 12) - 6;
                // Dusty concrete: muted grey-beige, rough speckles of grit and a few pale dust patches.
                const uint32_t g = hash3(px, py, 1563, 11);
                Rgb dusty = Rgb{150, 146, 138}.plus(n);
                if (g % 9 == 0) dusty = dusty.scaled(0.82f);                           // grit
                if (hash3(px / 3, py / 3, 1563, 3) % 5 == 0) dusty = dusty.scaled(1.08f);  // dust patches
                a.put(T_DUSTY_CONCRETE, px, py, dusty);
                // Hardened concrete: even, cool grey, barely any noise, one faint form-board line.
                Rgb hard = Rgb{128, 132, 136}.plus(n / 3);
                if (py == 7 && hash3(px, 0, 1564, 2) % 4 != 0) hard = hard.scaled(0.94f);
                a.put(T_HARDENED_CONCRETE, px, py, hard);
                // Gray wood of the Dappled Forest: ash-grey bark with dark furrows; rings on top.
                const int furrow = (px * 5 + (int)(hash3(px, py / 3, 1565, 7) % 3)) % 6;
                Rgb bark = Rgb{118, 116, 112}.plus(n);
                if (furrow == 0) bark = Rgb{78, 76, 74}.plus(n / 2);
                if (furrow == 1) bark = bark.scaled(1.1f);
                a.put(T_GRAY_LOG_SIDE, px, py, bark);
                const float rr = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                a.put(T_GRAY_LOG_TOP, px, py, rr > 7 ? Rgb{96, 94, 90}.plus(n) : ((int)rr % 2 ? Rgb{176, 170, 160} : Rgb{150, 144, 134}).plus(n / 2));
                // Gray planks, redone: four boards with staggered end joints, a lit top edge and a dark
                // seam under each, fine grain lines along the board and nail holes at the joints.
                const int row = py / 4, inRow = py % 4;
                const int joint = (row * 7 + 3) % 16;
                Rgb plank = Rgb{164, 160, 152}.scaled(0.94f + 0.04f * (float)(row % 3));
                if (hash3(px / 2, py, 1566, 4) % 4 == 0) plank = plank.scaled(0.95f);  // grain
                if (inRow == 0) plank = plank.scaled(1.08f);
                if (inRow == 3 || px == joint) plank = Rgb{92, 90, 86};
                if (inRow == 1 && (px == (joint + 2) % 16 || px == (joint + 14) % 16)) plank = Rgb{70, 68, 66};
                a.put(T_GRAY_PLANKS, px, py, plank.plus(n / 3));
                // 1.7.2 build 2: the cutter (stone body, a steel saw blade across the top) and the barrel
                // (oak staves bound by two iron hoops; the lid shows the boards and a dark bung).
                const Rgb stoneC = Rgb{124, 124, 124}.plus(n), wood = Rgb{150, 108, 62}.plus(n / 2);
                const bool blade = py >= 7 && py <= 8 && px >= 1 && px <= 14, tooth = py == 6 && px % 2 == 0 && px >= 2 && px <= 13;
                a.put(T_CUTTER_TOP, px, py, blade || tooth ? Rgb{200, 204, 212} : (px == 0 || px == 15 || py == 0 || py == 15) ? stoneC.scaled(0.75f) : Rgb{96, 72, 44}.plus(n / 2));
                a.put(T_CUTTER_SIDE, px, py, py < 4 ? Rgb{120, 88, 52}.plus(n / 2) : (py == 4 ? stoneC.scaled(0.7f) : stoneC));
                const bool hoop = py == 3 || py == 12, stave = px % 4 == 0;
                a.put(T_BARREL_SIDE, px, py, hoop ? Rgb{70, 70, 76}.plus(n / 3) : stave ? wood.scaled(0.72f) : wood);
                const float br = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                a.put(T_BARREL_TOP, px, py, br > 7.2f ? Rgb{70, 70, 76} : br < 1.6f ? Rgb{50, 34, 20} : (py % 4 == 0 ? wood.scaled(0.75f) : wood.scaled(1.08f)));
                // The wood cutter: a plank bench with a round saw (darker blade, teeth all round) on top.
                const float sr = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                const Rgb bench = Rgb{168, 126, 76}.plus(n / 2);
                a.put(T_WOOD_CUTTER_TOP, px, py, sr < 1.3f ? Rgb{60, 60, 66} : sr < 5.2f ? Rgb{186, 190, 198}.plus(n / 3) :
                                                 (sr < 6.4f && (px + py) % 2 == 0) ? Rgb{150, 154, 162} : (py % 4 == 3 ? bench.scaled(0.75f) : bench));
                a.put(T_WOOD_CUTTER_SIDE, px, py, py < 3 ? Rgb{150, 154, 162}.plus(n / 3) : (px < 2 || px > 13) ? bench.scaled(0.7f) : (py % 4 == 3 ? bench.scaled(0.8f) : bench));
                // Autumn leaves.
                leafPixel(a, T_RED_LEAVES, px, py, {186, 46, 38}, 91, false);
                leafPixel(a, T_ORANGE_LEAVES, px, py, {222, 120, 36}, 92, false);
                leafPixel(a, T_YELLOW_LEAVES, px, py, {226, 190, 52}, 93, false);
            }
        // 1.7.2 build 3: concrete in the 16 wool colours: the powder grainy with pale and dark
        // specks, the hard concrete flat and even; dyes as little piles of colour.
        for (int c = 0; c < 16; ++c) {
            const Rgb base = kWoolColors[c];
            for (int py = 0; py < 16; ++py)
                for (int px = 0; px < 16; ++px) {
                    const uint32_t g = hash3(px, py, 1930 + c, 5);
                    const int n = (int)(g % 10) - 5;
                    Rgb powder = base.scaled(0.95f).plus(n * 2);
                    if (g % 7 == 0) powder = powder.scaled(1.12f);
                    else if (g % 11 == 0) powder = powder.scaled(0.8f);
                    a.put(T_CONCRETE_POWDER + c, px, py, powder);
                    a.put(T_CONCRETE + c, px, py, base.scaled(0.88f).plus(n / 3));
                }
            a.sprite(T_DYE + c, kSprite_dust, base, base.scaled(1.15f));
        }
        // 1.7.2 build 3: the Crimson Void Expansion.
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const int n = (int)(hash3(px, py, 1978, 5) % 14) - 7;
                const uint32_t g = hash3(px, py, 1981, 9);
                // Crimsonium wood: deep red bark in twisting strands; rings of bright red inside.
                const int strand = (px * 3 + (int)(std::sin(py * 0.7f + px) * 2)) % 5;
                a.put(T_CRIMSONIUM_LOG_SIDE, px, py, strand == 0 ? Rgb{70, 14, 20}.plus(n / 2) : Rgb{128, 26, 34}.plus(n));
                const float rr = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                a.put(T_CRIMSONIUM_LOG_TOP, px, py, rr > 7 ? Rgb{90, 18, 26}.plus(n) : ((int)rr % 2 ? Rgb{196, 52, 58} : Rgb{160, 36, 44}).plus(n / 2));
                const bool seam = py % 4 == 3 || (px == ((py / 4) * 5 + 3) % 16 && py % 4 != 3);
                a.put(T_CRIMSONIUM_PLANKS, px, py, seam ? Rgb{92, 20, 28} : Rgb{168, 44, 52}.scaled(0.94f + 0.04f * ((py / 4) % 3)).plus(n / 3));
                leafPixel(a, T_CRIMSONIUM_LEAVES, px, py, {200, 30, 42}, 95, false);
                // Shattered stone: dark grey-violet rock split by bright cracks.
                const bool crack = std::abs((px * 7 + py * 3) % 13 - 6) == 0 && g % 3 != 0;
                const Rgb sh = Rgb{62, 56, 72}.plus(n);
                a.put(T_SHATTERED_STONE, px, py, crack ? Rgb{150, 110, 190} : sh);
                const bool mortar = py % 8 == 7 || (px + (py / 8) * 4) % 8 == 7;
                a.put(T_SHATTERED_BRICKS, px, py, mortar ? Rgb{36, 32, 42} : sh.scaled(1.15f));
                // Void shards: violet crystal with light facets.
                const int facet = (px + py) % 6, facet2 = (px - py + 16) % 7;
                a.put(T_VOID_SHARD_BLOCK, px, py, facet == 0 || facet2 == 0 ? Rgb{230, 200, 255} : Rgb{150, 80, 220}.plus(n));
                // Dark sand and sandstone.
                const Rgb ds = Rgb{58, 46, 62}.plus(n);
                a.put(T_DARK_SAND, px, py, g % 13 == 0 ? Rgb{160, 110, 220} : ds);
                a.put(T_DARK_SANDSTONE, px, py, py < 3 ? ds.scaled(1.2f) : py % 5 == 4 ? ds.scaled(0.75f) : ds.scaled(1.05f));
                // Crimsonium ore: void stone with blood-red crystals.
                const bool gem = hash3(px / 2, py / 2, 1987, 4) % 5 == 0;
                a.put(T_CRIMSONIUM_ORE, px, py, gem ? Rgb{235, 40, 60}.plus(n) : Rgb{60, 40, 70}.plus(n));
                // Shard cluster (a plant): three violet spikes, see-through around.
                const bool spike = (std::abs(px - 4) <= (15 - py) / 5 && py > 6) || (std::abs(px - 8) <= (15 - py) / 4 && py > 2) ||
                                   (std::abs(px - 12) <= (15 - py) / 5 && py > 7);
                if (spike) a.put(T_SHARD_CLUSTER, px, py, (px + py) % 4 == 0 ? Rgb{230, 200, 255} : Rgb{160, 90, 230}.plus(n));
                else a.clear(T_SHARD_CLUSTER, px, py);
            }
        a.sprite(T_VOID_SHARD, kSprite_gem, {150, 80, 220}, {235, 210, 255});
        a.sprite(T_CRIMSONIUM, kSprite_gem, {200, 30, 50}, {255, 170, 170});
        // 1.7.2 build 2: saplings of the new trees.
        a.sprite(T_GRAY_SAPLING, kSprite_sapling, {214, 110, 40});
        a.sprite(T_CELESTIAL_SAPLING, kSprite_sapling, {120, 200, 235});
        // The metal of worn hardium armour: icy plates with bright seams.
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const int n = (int)(hash3(px, py, 1624, 3) % 16) - 8;
                const bool seam = px % 8 == 0 || py % 8 == 0;
                a.put(T_HARDIUM_PLATE, px, py, seam ? Rgb{190, 245, 255} : Rgb{70, 175, 205}.plus(n));
            }
        // The Forgotten Overworld: grass, dirt and stone of a lost world, pale and with ghostly
        // cyan specks of souls in them.
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const int n = (int)(hash3(px, py, 1549, 5) % 14) - 7;
                const bool soul = hash3(px, py, 1550, 7) % 23 == 0;
                const Rgb soulC{150, 235, 235};
                const Rgb grass = Rgb{96, 128, 112}.plus(n), dirt = Rgb{92, 82, 78}.plus(n), stone = Rgb{112, 116, 128}.plus(n);
                a.put(T_SOULED_GRASS_TOP, px, py, soul ? soulC : grass);
                const int fringe = 3 + (int)(hash3(px, 0, 1551, 9) % 3);
                a.put(T_SOULED_GRASS_SIDE, px, py, py < fringe ? grass : soul ? soulC : dirt);
                a.put(T_SOULED_DIRT, px, py, soul ? soulC : dirt);
                const bool crack = (px * 3 + py * 5) % 17 == 0;
                a.put(T_SOULED_STONE, px, py, soul ? soulC : crack ? stone.scaled(0.7f) : stone);
                // The portal frame: void stone with a gold rim and a pale star socket on top.
                const Rgb vs = Rgb{58, 40, 82}.plus(n);
                const bool rim = px == 0 || py == 0 || px == 15 || py == 15;
                const int dx = std::abs(px * 2 - 15), dy = std::abs(py * 2 - 15);
                const bool star = (dx <= 2 && dy <= 9) || (dy <= 2 && dx <= 9) || (dx + dy <= 9);
                a.put(T_CELESTIAL_FRAME_TOP, px, py, rim ? Rgb{214, 180, 90} : star ? Rgb{235, 240, 255} : vs);
                a.put(T_CELESTIAL_FRAME_SIDE, px, py, py < 3 ? Rgb{214, 180, 90}.scaled(py == 2 ? 0.75f : 1.0f) : vs);
                // 1.7.0 preview: a frame block holding a head (the gold rim round the Crimson Man's
                // face), the portal (a starry sky over pale clouds) and the head as an item.
                const bool face = px >= 3 && px <= 12 && py >= 3 && py <= 12;
                Rgb fc = Rgb{22, 18, 22}.plus(n / 3);
                if (py >= 5 && py <= 6 && ((px >= 4 && px <= 6) || (px >= 9 && px <= 11))) fc = {255, 50, 35};
                else if (py == 10 && px >= 5 && px <= 10) fc = {130, 12, 12};
                a.put(T_CELESTIAL_FRAME_FULL, px, py, rim ? Rgb{214, 180, 90} : face ? fc : vs);
                const uint32_t h = hash3(px, py, 1555, 3);
                const float sky = py / 15.0f;
                Rgb pc = Rgb{(int)(40 + sky * 90), (int)(60 + sky * 120), (int)(150 + sky * 90)}.plus(n / 2);
                if (h % 19 == 0) pc = {245, 245, 255};
                a.put(T_CELESTIAL_PORTAL, px, py, pc);
                Rgb hc = Rgb{22, 18, 22}.scaled(0.8f + patch(px, py, 78, 4) * 0.5f).plus(n / 3);
                if (py >= 5 && py <= 7 && ((px >= 2 && px <= 5) || (px >= 10 && px <= 13))) hc = Rgb{255, 40 + n, 30};
                else if (py >= 11 && py <= 12 && px >= 4 && px <= 11) hc = {120, 10, 10};
                if (px < 1 || px > 14 || py < 1 || py > 14) a.clear(T_CRIMSON_HEAD, px, py);
                else a.put(T_CRIMSON_HEAD, px, py, hc);
            }
        a.sprite(T_CELESTIAL_COMPASS, kSprite_compass, {70, 60, 110}, {235, 240, 255});
        // The spawner: a dark iron cage with a faint glow inside.
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const bool bar = px % 5 == 0 || py % 5 == 0 || px == 15 || py == 15;
                if (bar) a.put(T_SPAWNER, px, py, Rgb{52, 58, 70}.scaled((px + py) % 3 == 0 ? 1.25f : 1.0f));
                else a.put(T_SPAWNER, px, py, Rgb{30, 12, 40}.plus((int)(hash3(px, py, 1548, 3) % 20)));
            }
    }
    // 1.5.7: the redesigned ores (once the 1.6.0 Preview experiment) are the standard look.
    for (int i = 0; i < kOreRedesignCount; ++i) {
        uint8_t ore[16 * 16 * 4];
        oreRedesignTile(i, ore);
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const uint8_t* p = ore + (py * 16 + px) * 4;
                a.put(kOreRedesignTiles[i], px, py, {p[0], p[1], p[2]});
            }
    }
    return a.image();
}

namespace {

void drawUpdate14(Canvas& a, int tile) {
    switch (tile) {
        case T_STRING: a.sprite(tile, kSprite_string, {236, 236, 230}); return;
        case T_MAP: a.sprite(tile, kSprite_map, {120, 90, 56}, {226, 210, 160}); return;
        case T_TAR_BALL: a.sprite(tile, kSprite_ball, {30, 26, 30}, {120, 110, 130}); return;
        case T_EGG_SPIDER: a.sprite(tile, kSprite_egg, {52, 44, 40}, {190, 20, 20}); return;
        case T_EGG_BAT: a.sprite(tile, kSprite_egg, {70, 56, 44}, {30, 26, 24}); return;
        case T_EGG_TNT_ZOMBIE: a.sprite(tile, kSprite_egg, {96, 130, 80}, {210, 50, 40}); return;
        case T_EGG_TAR_MONSTER: a.sprite(tile, kSprite_egg, {24, 22, 26}, {170, 120, 220}); return;
        case T_ANNIV_TOKEN: a.sprite(tile, kSprite_token, {150, 110, 20}, {246, 206, 70}); return;
        case T_ANNIV_MEDAL: a.sprite(tile, kSprite_medal, {150, 110, 20}, {246, 206, 70}); return;
        case T_PARTY_POPPER: a.sprite(tile, kSprite_popper, {200, 60, 160}, {250, 220, 60}); return;
        default: break;
    }
    if (tile == T_ANNIV_LOGO) {
        // The VoxelCraft emblem: "VC" on deep green, a gold frame and a little gold "1".
        static const char* const kLogo[16] = {
            "GGGGGGGGGGGGGGGG",
            "G..............G",
            "G.W...W..WWWW..G",
            "G.W...W.W......G",
            "G.W...W.W......G",
            "G..W.W..W......G",
            "G..W.W..W......G",
            "G...W....WWWW..G",
            "G..............G",
            "G.....GGGG.....G",
            "G....GG..GG..Y.G",
            "G....G....G.YY.G",
            "G....GG..GG..Y.G",
            "G.....GGGG...Y.G",
            "G............Y.G",
            "GGGGGGGGGGGGGGGG",
        };
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const int n = (int)(hash3(px, py, tile, 5) % 10) - 5;
                const char c = kLogo[py][px];
                Rgb col = c == 'G' ? Rgb{236, 186, 60} : c == 'W' ? Rgb{250, 250, 244} : c == 'Y' ? Rgb{255, 214, 80}
                                                                                   : Rgb{30, 96, 70}.scaled(0.9f + patch(px, py, 501, 4) * 0.2f);
                if (c == 'G' && (px + py) % 5 == 0) col = col.plus(30);  // glints on the frame
                a.put(tile, px, py, col.plus(n / 2));
            }
        return;
    }
    if (tile >= T_STAINED_GLASS && tile < T_STAINED_GLASS + 16) {
        // A coloured pane: a light frame in the colour, tinted see-through glass inside.
        const Rgb c = kWoolColors[tile - T_STAINED_GLASS];
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                bool edge = px == 0 || py == 0 || px == 15 || py == 15;
                bool shine = (px == py + 3 && px < 8) || (px == py + 5 && px < 7);
                if (edge) a.put(tile, px, py, c.scaled(0.8f).plus(30));
                else if (shine) a.put(tile, px, py, c.plus(70), 200);
                else a.put(tile, px, py, c, 110);
            }
        return;
    }
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            int n = (int)(hash3(px, py, tile, 1414) % 24) - 12;
            switch (tile) {
                case T_TAR: {
                    // Thick black liquid with slow oily swirls.
                    float sw = patch(px, py, 141, 4);
                    Rgb c = Rgb{22, 20, 26}.plus((int)(sw * 18) + n / 4);
                    if (sw > 0.78f) c = Rgb{70, 60, 96}.plus(n / 3);  // purple sheen
                    a.put(tile, px, py, c);
                    break;
                }
                case T_TAR_BLOCK: {
                    bool crack = hash3(px, py, 142, 3) % 13 == 0 || (px + py * 3) % 11 == 0;
                    a.put(tile, px, py, (crack ? Rgb{12, 10, 14} : Rgb{40, 36, 44}).plus((int)(patch(px, py, 143, 3) * 14) + n / 3));
                    break;
                }
                case T_DRIPSTONE: {
                    // Brown stone in wavy vertical streaks.
                    int streak = (int)(std::sin(px * 1.3f + std::sin(py * 0.6f) * 1.5f) * 10);
                    a.put(tile, px, py, Rgb{132, 104, 86}.plus(streak + n / 2));
                    break;
                }
                case T_DRIPSTONE_UP:
                case T_DRIPSTONE_DOWN: {
                    // A spike, wide at its base and pointed at the tip.
                    const int fromBase = tile == T_DRIPSTONE_UP ? 15 - py : py;
                    const float half = std::max(0.5f, 4.5f - fromBase * 0.28f);
                    if (std::fabs(px - 7.5f) <= half) a.put(tile, px, py, Rgb{140, 110, 90}.scaled(px < 8 ? 1.1f : 0.85f).plus(n / 2));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_SPIDER_BODY:
                    a.put(tile, px, py, Rgb{48, 40, 36}.scaled(0.85f + patch(px, py, 145, 3) * 0.3f).plus(n / 3));
                    break;
                case T_SPIDER_FACE: {
                    Rgb c = Rgb{48, 40, 36}.plus(n / 3);
                    if ((py == 5 || py == 6) && (px == 3 || px == 4 || px == 11 || px == 12)) c = {220, 20, 20};  // big eyes
                    if (py == 8 && (px == 6 || px == 9)) c = {200, 30, 30};                                       // small eyes
                    if (py >= 11 && py <= 13 && (px == 5 || px == 10)) c = {20, 16, 14};                         // fangs
                    a.put(tile, px, py, c);
                    break;
                }
                case T_BAT_BODY: a.put(tile, px, py, Rgb{78, 62, 48}.plus(n / 3)); break;
                case T_BAT_FACE: {
                    Rgb c = Rgb{78, 62, 48}.plus(n / 3);
                    if (py == 7 && (px == 5 || px == 10)) c = {10, 10, 10};
                    if (py <= 3 && (px <= 3 || px >= 12)) c = {60, 46, 36};  // ears
                    a.put(tile, px, py, c);
                    break;
                }
                case T_BAT_WING: {
                    bool bone = px % 5 == 0 || py == 2;
                    a.put(tile, px, py, (bone ? Rgb{40, 32, 26} : Rgb{66, 50, 40}).plus(n / 4));
                    break;
                }
                case T_TAR_SKIN: {
                    float sw = patch(px, py, 146, 3);
                    Rgb c = Rgb{20, 18, 24}.plus((int)(sw * 20) + n / 4);
                    if (sw > 0.8f) c = Rgb{90, 70, 130}.plus(n / 3);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_TAR_FACE: {
                    Rgb c = Rgb{20, 18, 24}.plus(n / 4);
                    if (py >= 5 && py <= 7 && ((px >= 3 && px <= 5) || (px >= 10 && px <= 12))) c = {200, 140, 255};  // glowing eyes
                    if (py >= 10 && py <= 13 && px >= 4 && px <= 11 && (py != 10 || px % 2)) c = {6, 4, 8};          // dripping mouth
                    a.put(tile, px, py, c);
                    break;
                }
                case T_TNT_ZOMBIE_SHIRT: {
                    // A vest of TNT sticks strapped over a torn shirt.
                    bool stick = py >= 4 && py <= 12 && px % 4 != 3;
                    bool band = py == 7 || py == 8;
                    Rgb c = stick ? (band ? Rgb{230, 230, 220} : Rgb{200, 40, 32}) : Rgb{70, 100, 130};
                    a.put(tile, px, py, c.plus(n / 3));
                    break;
                }
                case T_ANNIV_LOGO_SIDE: {
                    // Gold with a green inlay, like the top of a trophy.
                    const bool inlay = px >= 4 && px <= 11 && py >= 4 && py <= 11;
                    const bool edge = px == 0 || py == 0 || px == 15 || py == 15;
                    a.put(tile, px, py, (inlay ? Rgb{30, 96, 70} : Rgb{236, 186, 60}.scaled(edge ? 0.8f : 1.0f)).plus(n / 2));
                    break;
                }
                case T_CONFETTI_BLOCK: {
                    // White paper heaped with bits of every wool colour.
                    const uint32_t h = hash3(px / 2, py / 2, 502, 7);
                    Rgb c = Rgb{240, 238, 232}.plus(n / 3);
                    if (h % 3 == 0) c = kWoolColors[1 + h / 3 % 15].scaled(1.05f);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_BALLOON:
                case T_BALLOON + 1:
                case T_BALLOON + 2: {
                    static const Rgb kBalloon[3] = {{220, 40, 50}, {50, 110, 220}, {240, 200, 40}};
                    const Rgb c = kBalloon[tile - T_BALLOON];
                    const float dx = px - 5.5f, dy = py - 4.5f;
                    float f = 0.85f + (py < 8 ? 0.1f : -0.1f);
                    if (dx * dx + dy * dy < 4) f = 1.45f;  // the shine
                    a.put(tile, px, py, c.scaled(f).plus(n / 3));
                    break;
                }
                case T_PARTY_LANTERN: {
                    const bool frame = px <= 1 || px >= 14 || py <= 1 || py >= 14 || px == 7 || px == 8;
                    a.put(tile, px, py, frame ? Rgb{196, 150, 50}.plus(n / 2) : Rgb{255, 214, 120}.scaled(0.9f + patch(px, py, 503, 3) * 0.2f));
                    break;
                }
                case T_PARTY_LANTERN_TOP: a.put(tile, px, py, Rgb{196, 150, 50}.scaled((px + py) % 4 == 0 ? 1.15f : 1.0f).plus(n / 2)); break;
                case T_FESTIVE_BRICKS: {
                    // Red bricks with gold mortar, every other row a little lighter.
                    const int row = py / 4, off = (row % 2) * 4;
                    const bool mortar = py % 4 == 3 || (px + off) % 8 == 7;
                    if (mortar) a.put(tile, px, py, Rgb{232, 186, 70}.plus(n / 3));
                    else a.put(tile, px, py, Rgb{170, 50, 56}.scaled(row % 2 ? 1.08f : 0.96f).scaled(py % 4 == 0 ? 1.12f : 1.0f).plus(n));
                    break;
                }
                case T_PAINTED_PLANKS:
                case T_PAINTED_PLANKS + 1:
                case T_PAINTED_PLANKS + 2:
                case T_PAINTED_PLANKS + 3: {
                    static const Rgb kPaint[4] = {{196, 64, 56}, {64, 104, 196}, {226, 192, 70}, {84, 160, 70}};
                    const int row = py / 4;
                    const bool seam = py % 4 == 3 || (px == (row * 6 + 4) % 16 && py % 4 != 3);
                    const float grain = ((px + row * 3) % 5) * 0.025f;
                    a.put(tile, px, py, kPaint[tile - T_PAINTED_PLANKS].scaled(seam ? 0.6f : 0.92f + grain).plus(n / 3));
                    break;
                }
                default: a.put(tile, px, py, {255, 0, 255}); break;
            }
        }
}

// Value noise whose cells wrap around the 16 px tile, so a texture joins its neighbours
// seamlessly (above, below and beside); `cell` must divide 16.
float tilePatch(int x, int y, int seed, int cell) {
    const int m = 16 / cell;
    auto v = [&](int cx, int cy) { return (hash3((cx % m + m) % m, (cy % m + m) % m, seed, 91) % 1000) / 1000.0f; };
    const int cx = x / cell, cy = y / cell;
    const float fx = (x % cell + 0.5f) / cell, fy = (y % cell + 0.5f) / cell;
    const float a = v(cx, cy) + (v(cx + 1, cy) - v(cx, cy)) * fx;
    const float b = v(cx, cy + 1) + (v(cx + 1, cy + 1) - v(cx, cy + 1)) * fx;
    return a + (b - a) * fy;
}

// 1.5.0 bark, made seamless in 1.5.1: vertical plates between wavy dark furrows, lit on one side
// like the leaf clumps, a short crack on some plates and now and then a knot. Every wave repeats
// exactly every 16 px, so stacked logs join into one smooth trunk.
Rgb barkPixel(int px, int py, Rgb base, int seed) {
    const float k = 2 * 3.14159265f / 16;
    const float wobble = std::sin(py * k + seed) * 0.8f + std::sin(py * 2 * k + seed * 1.3f) * 0.35f;
    float x = px + wobble;
    x -= std::floor(x / 16.0f) * 16.0f;
    const int col = (int)(x / 4.0f);
    const float fx = x - col * 4.0f;
    const uint32_t h = hash3(col, 0, seed, 17);
    const int n = (int)(hash3(px, py, seed, 18) % 14) - 7;
    if (fx < 0.95f) return base.scaled(0.48f).plus(n / 3);                      // furrow
    if (py == (int)(h % 16) && fx > 1.2f && fx < 3.4f) return base.scaled(0.6f);  // crack across the plate
    const float kx = px - (float)(h % 11 + 3), ky = py - (float)((h >> 5) % 11 + 3);
    if ((h >> 10) % 3 == 0 && kx * kx + ky * ky * 0.6f < 2.2f) return base.scaled(kx * kx + ky * ky * 0.6f < 0.8f ? 0.5f : 0.72f);  // knot
    float f = fx < 1.8f ? 1.17f : fx > 3.3f ? 0.84f : 1.0f;
    f *= 0.94f + tilePatch(px, py, seed + 1, 4) * 0.12f;
    if (hash3(px, py / 4, seed, 19) % 7 == 0) f -= 0.08f;  // fine streaks
    return base.scaled(f).plus(n / 2);
}

// 1.5.1 Netherperk stem: red fibres twisting up the trunk, glowing orange where the sap shows.
Rgb netherperkBark(int px, int py) {
    const float x = px + py * 0.5f;  // half a pixel per row: after 16 rows the twist lines up again
    const int strand = ((int)std::floor(x) % 4 + 4) % 4;
    const int n = (int)(hash3(px, py, 81, 18) % 12) - 6;
    const Rgb base{128, 34, 30};
    if (strand == 0) return base.scaled(0.55f).plus(n / 3);
    if (strand == 2 && tilePatch(px, py, 85, 4) > 0.8f) return Rgb{255, 150, 50}.plus(n / 2);  // sap
    return base.scaled((strand == 1 ? 1.15f : 0.95f) * (0.92f + tilePatch(px, py, 84, 4) * 0.16f)).plus(n / 2);
}

// 1.5.1 Void bark: dark violet scales in offset rows, a glowing vein through some of them.
Rgb voidBark(int px, int py) {
    const int row = py / 4, off = (row % 2) * 2, sx = (px + off) % 16;
    const int n = (int)(hash3(px, py, 64, 18) % 12) - 6;
    const bool edge = sx % 4 == 0 || py % 4 == 0;
    const uint32_t cell = hash3(sx / 4, row, 64, 23);
    if (edge) return Rgb{24, 12, 34}.plus(n / 4);
    if (cell % 5 == 0 && sx % 4 == 2) return Rgb{170, 110, 230}.plus(n / 3);  // a vein of void light
    const float lit = (sx % 4 == 1 || py % 4 == 1) ? 1.25f : (sx % 4 == 3 || py % 4 == 3) ? 0.8f : 1.0f;
    return Rgb{58, 32, 80}.scaled(lit * (0.9f + (cell % 3) * 0.08f)).plus(n / 2);
}

const char* const kSprite_brusher[16] = {
    "................",
    "..........3333..",
    ".........333333.",
    "........3333333.",
    "........333333..",
    ".......11113....",
    "......1111......",
    ".....2111.......",
    "....222.........",
    "...222..........",
    "..222...........",
    ".222............",
    "222.............",
    "22..............",
    "................",
    "................",
};
const char* const kSprite_document[16] = {
    "................",
    "..111111111111..",
    "..133333333331..",
    "..132222222231..",
    "..133333333331..",
    "..132222222331..",
    "..133333333331..",
    "..132222222231..",
    "..133333333331..",
    "..132222223331..",
    "..133333333331..",
    "..132222222231..",
    "..133333333311..",
    "..13333333311...",
    "..1111111111....",
    "................",
};
const char* const kSprite_coin[16] = {
    "................",
    "................",
    ".....111111.....",
    "....13333331....",
    "...1331111331...",
    "..133133331331..",
    "..131331133131..",
    "..131311113131..",
    "..131311113131..",
    "..131331133131..",
    "..133133331331..",
    "...1331111331...",
    "....13333331....",
    ".....111111.....",
    "................",
    "................",
};
const char* const kSprite_relic[16] = {
    "................",
    ".......11.......",
    "......1331......",
    ".....133331.....",
    "....13333331....",
    "...1333113331...",
    "..133311113331..",
    ".13331111113331.",
    "..133311113331..",
    "...1333113331...",
    "....13333331....",
    ".....133331.....",
    "......1331......",
    ".......11.......",
    "................",
    "................",
};
const char* const kSprite_shard[16] = {
    "................",
    "................",
    "....111.........",
    "...11111........",
    "...1133111......",
    "..11333311......",
    "..1113331111....",
    "..11111331111...",
    "...1111133111...",
    "...11111111111..",
    "....1111111111..",
    ".....111111.....",
    "......111.......",
    "................",
    "................",
    "................",
};
const char* const kSprite_egg15[16] = {
    "................",
    "......1111......",
    ".....111111.....",
    "....11133111....",
    "....11133111....",
    "...1111111111...",
    "...1131111111...",
    "...1111111311...",
    "..111111111111..",
    "..111133111111..",
    "..111133111111..",
    "..111111111311..",
    "...1111111111...",
    "....11111111....",
    "......1111......",
    "................",
};

void drawUpdate15(Canvas& a, int tile) {
    switch (tile) {
        case T_BRUSHER: a.sprite(tile, kSprite_brusher, {200, 140, 70}, {236, 220, 190}); return;
        case T_FORTRESS_COMPASS: a.sprite(tile, kSprite_compass, {60, 24, 30}, {40, 220, 240}); return;
        case T_DOCUMENT: a.sprite(tile, kSprite_document, {150, 116, 70}, {226, 208, 160}); return;
        case T_ANCIENT_COIN: a.sprite(tile, kSprite_coin, {150, 110, 30}, {236, 196, 80}); return;
        case T_POWERSTONE_RELIC: a.sprite(tile, kSprite_relic, {30, 110, 130}, {120, 240, 255}); return;
        case T_POTTERY_SHARD: a.sprite(tile, kSprite_shard, {170, 90, 60}, {230, 200, 140}); return;
        case T_EGG_HUSKING: a.sprite(tile, kSprite_egg15, {150, 132, 100}, {230, 110, 30}); return;
        default: break;
    }
    const Rgb power{40, 200, 230}, psStone{82, 92, 98}, netherBrick{64, 30, 36};
    // Powerstone bricks: blue-grey stone bricks with thin cyan veins (they do not glow).
    auto psBrick = [&](int px, int py, int seed) {
        const int row = py / 8, off = (row % 2) * 8;
        const bool gap = py % 8 == 7 || (px + off) % 16 == 15;
        const bool hi = py % 8 == 0 || (px + off) % 16 == 0;
        const int n = (int)(hash3(px, py, seed, 1515) % 16) - 8;
        if (gap) return Rgb{34, 40, 46}.plus(n / 3);
        const float f = (hi ? 1.18f : 1.0f) * (0.9f + patch(px, py, seed, 4) * 0.2f);
        Rgb c = psStone.scaled(f).plus(n / 2);
        if (std::fabs(std::sin(px * 0.8f + py * 0.35f + patch(px, py, seed + 3, 5) * 5)) < 0.08f) c = power.scaled(0.7f).plus(n / 3);
        return c;
    };
    auto netherBrickPixel = [&](int px, int py, int n) {
        const bool mortar = py % 4 == 3 || ((px + (py / 4 % 2) * 4) % 8 == 7);
        return mortar ? Rgb{28, 12, 16} : netherBrick.scaled(py % 4 == 0 ? 1.25f : 1.0f).plus(n / 2);
    };
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            const int n = (int)(hash3(px, py, tile, 1515) % 24) - 12;
            switch (tile) {
                case T_POWERSTONE_BRICKS: a.put(tile, px, py, psBrick(px, py, 151)); break;
                case T_CRACKED_POWERSTONE_BRICKS: {
                    // The same bricks, split by dark cracks and with chipped corners.
                    Rgb c = psBrick(px, py, 151);
                    const float crack = std::fabs(px - 4 - py * 0.6f - std::sin(py * 1.3f) * 1.2f);
                    const float crack2 = std::fabs(py - 11 + px * 0.5f - std::sin(px * 1.1f));
                    if (crack < 0.6f || (crack2 < 0.55f && px > 6)) c = Rgb{22, 26, 30};
                    else if (hash3(px / 2, py / 2, 152, 3) % 9 == 0) c = c.scaled(0.78f);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_CHISELED_POWERSTONE_BRICKS: {
                    // A carved panel: a frame around the old powerstone sign (a diamond with a dot).
                    const bool frame = px <= 1 || px >= 14 || py <= 1 || py >= 14;
                    const int d = std::abs(px * 2 - 15) + std::abs(py * 2 - 15);
                    Rgb c = psStone.scaled(0.95f + patch(px, py, 153, 4) * 0.12f).plus(n / 2);
                    if (frame) c = psStone.scaled(px == 0 || py == 0 ? 1.25f : 0.8f).plus(n / 3);
                    else if (d >= 10 && d <= 12) c = Rgb{30, 36, 42};
                    else if (d <= 3) c = power.scaled(0.75f);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_POWERSTONE_GLASS: {
                    // A cyan-tinted pane with a frame of powerstone bricks at the corners.
                    const bool edge = px == 0 || py == 0 || px == 15 || py == 15;
                    const bool corner = (px <= 2 || px >= 13) && (py <= 2 || py >= 13);
                    const bool shine = (px == py + 3 && px < 8) || (px == py + 5 && px < 7);
                    if (corner) a.put(tile, px, py, psStone.scaled(1.1f).plus(n / 3));
                    else if (edge) a.put(tile, px, py, Rgb{90, 200, 220}.plus(n / 4));
                    else if (shine) a.put(tile, px, py, Rgb{220, 255, 255}, 210);
                    else a.put(tile, px, py, Rgb{60, 170, 200}, 90);
                    break;
                }
                case T_DEBRIS_GRAVEL: {
                    // Gravel, but with bits of old brick and a pale shard poking out.
                    const uint32_t h = hash3(px / 2, py / 2, 154, 9);
                    int v = 100 + (int)(h % 60) + n / 2;
                    Rgb c{v + 6, v, v - 6};
                    if (h % 11 == 0) c = psStone.scaled(1.1f);
                    if (h % 13 == 1) c = Rgb{168, 100, 70}.plus(n / 3);            // pottery
                    if ((px >= 9 && px <= 11 && py >= 5 && py <= 6) || (px == 10 && py == 7)) c = Rgb{226, 220, 200};  // a bone
                    a.put(tile, px, py, c);
                    break;
                }
                case T_SKULL:
                case T_SKULL_FACE: {
                    Rgb c = Rgb{214, 208, 190}.scaled(0.9f + patch(px, py, 155, 3) * 0.15f).plus(n / 3);
                    if (tile == T_SKULL_FACE) {
                        const bool eye = py >= 5 && py <= 8 && ((px >= 3 && px <= 6) || (px >= 9 && px <= 12));
                        const bool nose = py >= 9 && py <= 10 && px >= 7 && px <= 8;
                        const bool teeth = py >= 12 && py <= 13 && px >= 4 && px <= 11 && px % 2 == 0;
                        if (eye || nose) c = Rgb{30, 26, 24};
                        if (teeth) c = Rgb{120, 112, 100};
                    } else if (hash3(px, py, 156, 4) % 17 == 0) {
                        c = c.scaled(0.75f);  // cracks
                    }
                    a.put(tile, px, py, c);
                    break;
                }
                case T_WASTELAND_GRASS_TOP: {
                    // 1.5.1: the wasteland of the Cornerstone Dimension: rusty dry grass, cornerstone
                    // showing through in bare red patches, and a few powerstone specks.
                    const float f = 0.85f + patch(px, py, 157, 4) * 0.25f;
                    Rgb c = Rgb{150, 104, 58}.scaled(f).plus(n / 2);
                    if (hash3(px, py, 166, 4) % 5 == 0) c = Rgb{190, 150, 80}.scaled(f).plus(n / 2);  // pale blades
                    if (patch(px, py, 158, 5) > 0.68f) c = Rgb{116, 50, 50}.scaled(0.8f + patch(px, py, 13, 3) * 0.4f).plus(n);  // cornerstone
                    if (hash3(px, py, 167, 3) % 61 == 0) c = power.scaled(0.9f);
                    a.put(tile, px, py, c);
                    break;
                }
                case T_WASTELAND_GRASS_SIDE: {
                    const int edge = 2 + (int)(hash3(px, 0, 160, 7) % 3);
                    if (py < edge) a.put(tile, px, py, Rgb{150, 104, 58}.scaled(py == edge - 1 ? 0.78f : 1.0f).plus(n / 2));
                    else a.put(tile, px, py, Rgb{116, 50, 50}.scaled(0.8f + patch(px, py, 13, 3) * 0.4f).plus(n));
                    break;
                }
                case T_DRY_GRASS:
                case T_FERN: {
                    // Thin blades (dry grass) or a fern with leaflets along a bent stem.
                    if (tile == T_DRY_GRASS) {
                        const int blade = px % 2 == 1 ? (int)(hash3(px, 0, 161, 9) % 9) + 4 : -1;
                        if (blade >= 0 && 15 - py <= blade) a.put(tile, px, py, Rgb{190, 170, 96}.scaled(15 - py == blade ? 1.2f : 0.95f).plus(n / 2));
                        else a.clear(tile, px, py);
                    } else {
                        const float stem = 7.5f + (15 - py) * 0.12f;
                        const float leaf = std::fabs(px - stem);
                        const bool on = py >= 2 && (leaf < 0.8f || (leaf < 5.0f - (15 - py) * 0.18f && (py + (int)leaf) % 3 != 0));
                        if (on) a.put(tile, px, py, Rgb{60, 130, 50}.scaled(leaf < 0.8f ? 0.8f : 1.0f + (py % 3) * 0.06f).plus(n / 2));
                        else a.clear(tile, px, py);
                    }
                    break;
                }
                case T_GRASSER_FLOWER: {
                    // The Grasser of the Overworld: a teal stem with spiky leaves and a cyan bloom.
                    const float dx = px - 7.5f, dy = py - 4.5f;
                    const bool bloom = dx * dx + dy * dy < 9.5f && (std::fabs(dx) < 1 || std::fabs(dy) < 1 || dx * dx + dy * dy < 4.5f);
                    const bool stem = (px == 7 || px == 8) && py > 6;
                    const bool leaves = (py == 11 && px >= 4 && px <= 6) || (py == 10 && px == 3) || (py == 12 && px >= 9 && px <= 11) || (py == 11 && px == 12);
                    if (bloom) a.put(tile, px, py, (dx * dx + dy * dy < 1.5f ? Rgb{250, 240, 120} : power.scaled(1.1f)).plus(n / 3));
                    else if (stem || leaves) a.put(tile, px, py, Rgb{30, 128, 112}.plus(n / 2));
                    else a.clear(tile, px, py);
                    break;
                }
                case T_CRACKED_NETHER_BRICKS: {
                    Rgb c = netherBrickPixel(px, py, n);
                    const float crack = std::fabs(px - 3 - py * 0.7f - std::sin(py * 1.4f));
                    if (crack < 0.6f || (std::fabs(py - 9 + px * 0.4f) < 0.5f && px > 8)) c = Rgb{18, 8, 10};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_CHISELED_NETHER_BRICKS: {
                    // A carved block: a frame and a skull-like mask in the middle.
                    const bool frame = px <= 1 || px >= 14 || py <= 1 || py >= 14;
                    Rgb c = netherBrick.scaled(1.05f).plus(n / 2);
                    if (frame) c = netherBrick.scaled(px == 0 || py == 0 ? 1.35f : 0.7f);
                    else if (py >= 5 && py <= 7 && (px == 5 || px == 6 || px == 9 || px == 10)) c = {28, 12, 16};
                    else if (py == 10 && px >= 5 && px <= 10) c = {28, 12, 16};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_HUSKING_SKIN: a.put(tile, px, py, Rgb{170, 150, 112}.scaled(0.86f + patch(px, py, 162, 3) * 0.24f).plus(n / 3)); break;
                case T_HUSKING_FACE: {
                    // A dried, hollow face: dark sunken eyes with ember dots and a wide open mouth.
                    Rgb c = Rgb{170, 150, 112}.scaled(0.86f + patch(px, py, 162, 3) * 0.24f).plus(n / 3);
                    if (py >= 4 && py <= 7 && ((px >= 3 && px <= 6) || (px >= 9 && px <= 12))) c = {34, 22, 16};
                    if (py == 6 && (px == 5 || px == 10)) c = {255, 140, 30};
                    if (py >= 10 && py <= 13 && px >= 5 && px <= 10) c = py == 10 || py == 13 ? Rgb{90, 50, 30} : Rgb{60, 20, 10};
                    if (py >= 11 && py <= 12 && px >= 6 && px <= 9) c = {240, 120, 30};  // the fire inside
                    a.put(tile, px, py, c);
                    break;
                }
                case T_HUSKING_ROBE: {
                    // Torn grey-brown cloth with a faded cyan band of the old kingdom.
                    Rgb c = Rgb{96, 82, 66}.scaled(0.86f + patch(px, py, 163, 4) * 0.24f).plus(n / 2);
                    if (py == 5 || py == 6) c = power.scaled(0.45f).plus(n / 3);
                    if (py >= 13 && hash3(px, 0, 164, 3) % 3 == 0) c = c.scaled(0.6f);  // ragged hem
                    a.put(tile, px, py, c);
                    break;
                }
                case T_VOID_BRICK_EYE: break;  // drawn whole (it is animated)
                case T_CRIMSON_STONE: {
                    // Layered crimson rock: wind-worn bands with dark cracks and pale grains.
                    const float band = std::sin(py * 0.9f + tilePatch(px, py, 171, 4) * 3) * 0.08f;
                    Rgb c = Rgb{142, 46, 40}.scaled(0.9f + band + tilePatch(px, py, 172, 4) * 0.18f).plus(n / 2);
                    if (hash3(px, py, 173, 3) % 19 == 0) c = Rgb{70, 20, 22};
                    if (hash3(px, py, 174, 3) % 23 == 0) c = Rgb{200, 110, 90};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_FIREBALL: {
                    const float dx = px - 7.5f, dy = py - 7.5f, d = std::sqrt(dx * dx + dy * dy) + patch(px, py, 165, 3) * 2;
                    a.put(tile, px, py, d < 3.5f ? Rgb{255, 240, 140} : d < 6 ? Rgb{255, 150, 30} : Rgb{200, 50, 10});
                    break;
                }
                default: a.put(tile, px, py, {255, 0, 255}); break;
            }
        }
}

// ---------------------------------------------------------------- 1.5.7

const char* const kSprite_shield[16] = {
    "................",
    "..111111111111..",
    "..133333113331..",
    "..131111111131..",
    "..131111111131..",
    "..131111111131..",
    "..133333333331..",
    "..131111111131..",
    "..131111111131..",
    "...1311111131...",
    "...1311111131...",
    "....13111131....",
    ".....131131.....",
    "......1331......",
    ".......11.......",
    "................",
};

const char* const kSprite_explorer_map[16] = {
    "................",
    "..111111111111..",
    "..133333333331..",
    "..131113311131..",
    "..133313333331..",
    "..133311113331..",
    "..133333313331..",
    "..131133313311..",
    "..133313333331..",
    "..133331111331..",
    "..131133333331..",
    "..133313311131..",
    "..133333333331..",
    "..111111111111..",
    "................",
    "................",
};

void drawUpdate157(Canvas& a, int tile) {
    switch (tile) {
        case T_SHIELD: {
            a.sprite(tile, kSprite_shield, {150, 104, 60}, {196, 200, 210});
            // An iron boss in the middle.
            for (int y = 6; y <= 8; ++y)
                for (int x = 7; x <= 8; ++x) a.put(tile, x, y, y == 6 ? Rgb{230, 232, 240} : Rgb{170, 172, 184});
            return;
        }
        case T_EXPLORER_MAP: {
            a.sprite(tile, kSprite_explorer_map, {104, 84, 60}, {222, 206, 156});
            // A red cross marks the mineshaft.
            for (int k = 0; k < 4; ++k) a.put(tile, 9 + k, 8 + k, {200, 30, 30}), a.put(tile, 12 - k, 8 + k, {200, 30, 30});
            return;
        }
        case T_WRITABLE_BOOK:
        case T_WRITTEN_BOOK: {
            // A leather book (brown, or green once signed) with pale pages and, unsigned, a quill.
            const Rgb cover = tile == T_WRITTEN_BOOK ? Rgb{60, 120, 70} : Rgb{120, 72, 40};
            for (int y = 3; y <= 13; ++y)
                for (int x = 3; x <= 11; ++x) a.put(tile, x, y, x == 3 ? cover.scaled(0.7f) : y == 13 || x == 11 ? Rgb{236, 228, 200} : cover);
            for (int x = 5; x <= 9; ++x) a.put(tile, x, 6, cover.scaled(1.3f));
            if (tile == T_WRITABLE_BOOK)
                for (int k = 0; k < 7; ++k) a.put(tile, 13 - k, 2 + k, k < 2 ? Rgb{30, 30, 30} : Rgb{245, 245, 245});
            return;
        }
        case T_EGG_HOGMAN: a.sprite(tile, kSprite_egg15, {226, 150, 140}, {60, 200, 220}); return;
        default: break;
    }
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            const int n = (int)(hash3(px, py, tile, 157) % 9) - 4;
            switch (tile) {
                case T_TORCH: {
                    // A stick in the middle columns (7..8) with a burning top: the shapes cut it out.
                    Rgb c{0, 0, 0};
                    bool on = false;
                    if (px >= 7 && px <= 8 && py >= 6) c = Rgb{120, 84, 46}.scaled(px == 7 ? 1.1f : 0.85f), on = true;
                    if (px >= 7 && px <= 8 && py >= 6 && py <= 7) c = py == 6 ? Rgb{255, 240, 150} : Rgb{255, 170, 40}, on = true;
                    if (px >= 6 && px <= 9 && py >= 4 && py <= 5) c = Rgb{255, 200, 70}, on = true;  // the flame above
                    if (on) a.put(tile, px, py, c);
                    else a.clear(tile, px, py);
                    break;
                }
                case T_CRIMSON_PLANKS: {
                    const int row = py / 4;
                    const bool seam = py % 4 == 3 || (px == (row * 5 + 3) % 16 && py % 4 != 3);
                    a.put(tile, px, py, Rgb{164, 40, 52}.scaled(seam ? 0.55f : 0.92f + ((px + row * 3) % 5) * 0.025f).plus(n / 3));
                    break;
                }
                case T_CRIMSON_LOG_SIDE: {
                    // 1.7.0: real bark. Vertical plates of uneven width split by dark furrows, a lit
                    // left edge on each plate, short cross cracks, a knot, and beads of glowing sap
                    // that run down the furrows.
                    static const int kEdges[] = {0, 3, 5, 9, 12, 16};
                    int plate = 0;
                    while (px >= kEdges[plate + 1]) ++plate;
                    const int left = kEdges[plate], right = kEdges[plate + 1] - 1;
                    const int shift = (int)(hash3(plate, 0, 178, 3) % 5);  // plates break at different heights
                    const bool furrow = px == right && right != 15;
                    const bool crack = (py + shift) % 7 == 0 && !furrow;
                    const bool lit = px == left && !furrow;
                    const float kx = px - 10.5f, ky = py - 5.5f;
                    const bool knot = kx * kx + ky * ky * 1.6f < 3.2f, knotRing = !knot && kx * kx + ky * ky * 1.6f < 6.0f;
                    Rgb c = Rgb{112, 30, 40}.scaled(0.9f + tilePatch(px, py, 176, 4) * 0.14f).plus(n / 3);
                    if (lit) c = c.scaled(1.22f);
                    if (crack) c = c.scaled(0.6f);
                    if (furrow) c = Rgb{46, 10, 16};
                    if (knotRing) c = Rgb{70, 16, 24};
                    if (knot) c = Rgb{150, 46, 52}.plus(n / 4);
                    // Sap: a bright bead in a furrow with a fading trail below it.
                    if (furrow) {
                        const int bead = (int)(hash3(px, 0, 177, 3) % 16);
                        const int below = (py - bead + 16) % 16;
                        if (below == 0) c = Rgb{255, 96, 70};
                        else if (below <= 2) c = Rgb{190, 50, 44}.scaled(1.0f - below * 0.2f);
                    }
                    a.put(tile, px, py, c);
                    break;
                }
                case T_CRIMSON_LOG_TOP: {
                    // Bark round the edge, then growth rings, a few radial cracks and a glowing heart.
                    const float dx = px - 7.5f, dy = py - 7.5f, d = std::sqrt(dx * dx + dy * dy);
                    const float ang = std::atan2(dy, dx);
                    Rgb c;
                    if (d > 6.6f) c = Rgb{96, 24, 34}.scaled(0.85f + tilePatch(px, py, 179, 3) * 0.2f);
                    else if (d > 5.9f) c = Rgb{60, 14, 20};
                    else c = ((int)(d * 1.25f) % 2 ? Rgb{176, 54, 58} : Rgb{146, 38, 48});
                    if (d < 6 && d > 1.5f && std::fabs(std::sin(ang * 2.5f + 0.7f)) < 0.07f) c = Rgb{80, 18, 26};  // cracks
                    if (d < 1.3f) c = Rgb{255, 110, 80};  // the glowing heart
                    a.put(tile, px, py, c.plus(n / 4));
                    break;
                }
                case T_PS_FURNACE_FRONT:
                case T_PS_FURNACE_FRONT_ON: {
                    // Blue-grey powerstone masonry with a cyan-rimmed mouth that glows when it works.
                    const bool edge = px == 0 || py == 0 || px == 15 || py == 15;
                    const bool mouth = px >= 4 && px <= 11 && py >= 7 && py <= 13;
                    const bool rim = !mouth && px >= 3 && px <= 12 && py >= 6 && py <= 14;
                    Rgb c = Rgb{86, 96, 104}.scaled(edge ? 0.7f : 0.95f + tilePatch(px, py, 180, 4) * 0.12f).plus(n / 3);
                    if (rim) c = Rgb{40, 200, 230}.scaled(tile == T_PS_FURNACE_FRONT_ON ? 1.0f : 0.5f);
                    if (mouth) c = tile == T_PS_FURNACE_FRONT_ON ? Rgb{120 + (13 - py) * 12, 230, 255} : Rgb{20, 24, 30};
                    if (py == 3 && px >= 4 && px <= 11) c = {20, 24, 30};
                    a.put(tile, px, py, c);
                    break;
                }
                case T_HOGMAN_SKIN: a.put(tile, px, py, Rgb{226, 150, 140}.scaled(0.88f + patch(px, py, 178, 3) * 0.2f).plus(n / 3)); break;
                case T_HOGMAN_FACE: {
                    // A broad snout with nostrils, small angry eyes and two tusks.
                    Rgb c = Rgb{226, 150, 140}.scaled(0.88f + patch(px, py, 178, 3) * 0.2f).plus(n / 3);
                    if (py >= 4 && py <= 5 && ((px >= 3 && px <= 5) || (px >= 10 && px <= 12))) c = py == 4 ? Rgb{40, 20, 20} : Rgb{250, 250, 250};
                    if (py == 5 && (px == 4 || px == 11)) c = {20, 10, 10};
                    if (py >= 8 && py <= 11 && px >= 4 && px <= 11) c = Rgb{240, 170, 160};
                    if (py >= 9 && py <= 10 && (px == 6 || px == 9)) c = {120, 50, 50};
                    if (py >= 12 && py <= 14 && (px == 3 || px == 12)) c = {245, 240, 220};  // tusks
                    a.put(tile, px, py, c);
                    break;
                }
                case T_HOGMAN_TUNIC: {
                    // A leather tunic with a powerstone-blue belt and gold studs.
                    Rgb c = Rgb{110, 70, 40}.scaled(0.86f + patch(px, py, 179, 4) * 0.22f).plus(n / 2);
                    if (py == 9 || py == 10) c = Rgb{40, 170, 200}.scaled(0.8f).plus(n / 3);
                    if ((py == 3 || py == 13) && px % 4 == 1) c = {230, 190, 60};
                    a.put(tile, px, py, c);
                    break;
                }
                default: a.put(tile, px, py, {255, 0, 255}); break;
            }
        }
}

}  // namespace

void drawEyeTile(uint8_t out[16 * 16 * 4], int kind, float time) {
    // Where the eye looks: a slow wander with quick glances; it blinks every few seconds.
    const float lookX = std::sin(time * 0.9f) * 1.6f + (std::fmod(time, 5.3f) < 0.6f ? 1.2f : 0);
    const float lookY = std::sin(time * 0.6f + 1.3f) * 0.8f;
    const float blinkPhase = std::fmod(time + kind * 1.7f, kind == 0 ? 4.1f : 3.3f);
    const float lid = blinkPhase < 0.18f ? 1.0f - std::fabs(blinkPhase - 0.09f) / 0.09f : 0;  // 0 open .. 1 shut
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            const int n = (int)(hash3(px, py, 600 + kind, 4321) % 24) - 12;
            Rgb c;
            if (kind == 0) {
                c = voidBark(px, py);
            } else {
                // Void stone bricks around the eye.
                const int row = py / 8, off = (row % 2) * 8;
                const bool gap = py % 8 == 7 || (px + off) % 16 == 15;
                c = gap ? Rgb{26, 16, 36}.plus(n / 3) : Rgb{58, 40, 78}.scaled((py % 8 == 0 || (px + off) % 16 == 0 ? 1.2f : 1.0f) * (0.85f + patch(px, py, 69, 4) * 0.3f)).plus(n / 2);
            }
            const float dx = (px - 7.5f) / 6.5f, dy = (py - 7.5f) / (kind == 0 ? 3.6f : 4.2f);
            const float d = dx * dx + dy * dy;
            const float half = (kind == 0 ? 3.6f : 4.2f) * (1 - lid);  // how far the lids are open
            const bool inside = d <= 1 && std::fabs(py - 7.5f) <= half + 0.01f;
            if (inside) {
                const float ix = px - 7.5f - lookX, iy = py - 7.5f - lookY, id = ix * ix + iy * iy;
                if (kind == 0) {
                    c = Rgb{236, 226, 214}.plus(n / 4);                                  // white
                    if (id <= 9) c = Rgb{200, 30, 50}.scaled(id <= 4 ? 1.2f : 0.9f);       // iris
                    if (id <= 1.5f) c = {14, 6, 10};                                      // pupil
                    if (std::fabs(ix + 1) < 0.6f && std::fabs(iy + 1) < 0.6f) c = {255, 255, 255};  // glint
                } else {
                    // A glowing violet eye whose iris pulses and whose slit narrows and widens.
                    const float pulse = 0.85f + 0.25f * std::sin(time * 3.0f);
                    c = Rgb{40, 10, 60}.plus(n / 4);
                    if (id <= 12) c = Rgb{150, 70, 230}.scaled(pulse * (id <= 5 ? 1.25f : 1.0f));
                    const float slit = 0.6f + 0.5f * (0.5f + 0.5f * std::sin(time * 1.3f));
                    if (std::fabs(ix) <= slit && std::fabs(iy) <= 3.2f) c = {10, 0, 16};
                    if (id <= 12 && id > 9) c = Rgb{80, 220, 240}.scaled(pulse);  // a cyan ring
                }
            } else if (d <= 1.35f) {
                c = kind == 0 ? Rgb{24, 10, 30} : Rgb{20, 8, 30};  // eyelid
            }
            uint8_t* o = out + (py * 16 + px) * 4;
            auto cl = [](int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
            o[0] = cl(c.r), o[1] = cl(c.g), o[2] = cl(c.b), o[3] = 255;
        }
}

#include "OreRedesign.inc"

const int kOreRedesignTiles[kOreRedesignCount] = {T_COAL_ORE, T_IRON_ORE, T_BRONZE_ORE, T_STEEL_ORE, T_RUBY_ORE, T_POWERSTONE_ORE};

void oreRedesignTile(int index, uint8_t out[16 * 16 * 4]) {
    for (int i = 0; i < 16 * 16; ++i) {
        for (int k = 0; k < 3; ++k) out[i * 4 + k] = kOreRedesign[index][i * 3 + k];
        out[i * 4 + 3] = 255;
    }
}

// 1.8.0 Old World: the procedural textures of VoxelCraft 0.2-0.3 (FallbackAtlas.cpp at commit
// feed712), for the blocks that existed back then. Drawn over a finished atlas of any tile size.
void applyLegacyTiles(std::vector<uint8_t>& rgba, int size) {
    const int t = size / kAtlasCols;
    auto put = [&](int tile, int px, int py, int r, int g, int b, int a = 255) {
        auto c8 = [](int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
        for (int sy = py * t / 16; sy < (py + 1) * t / 16; ++sy)
            for (int sx = px * t / 16; sx < (px + 1) * t / 16; ++sx) {
                uint8_t* p = &rgba[((size_t)((tile / kAtlasCols) * t + sy) * size + (tile % kAtlasCols) * t + sx) * 4];
                p[0] = c8(r), p[1] = c8(g), p[2] = c8(b), p[3] = (uint8_t)a;
            }
    };
    const int tiles[] = {T_STONE, T_GRASS_TOP, T_DIRT, T_GRASS_SIDE, T_COBBLE, T_PLANKS, T_BEDROCK, T_WATER, T_SAND, T_GRAVEL,
                         T_LOG_SIDE, T_LOG_TOP, T_LEAVES, T_GLASS, T_BRICKS, T_LAVA, T_OBSIDIAN, T_NETHERRACK, T_PORTAL,
                         T_NETHER_BRICK, T_COAL_ORE, T_IRON_ORE, T_FLINT_AND_STEEL};
    for (const int tile : tiles)
        for (int py = 0; py < 16; ++py)
            for (int px = 0; px < 16; ++px) {
                const int n = (int)(hash3(px, py, tile, 1234) % 32) - 16;
                switch (tile) {
                    case T_STONE: put(tile, px, py, 125 + n, 125 + n, 125 + n); break;
                    case T_GRASS_TOP: put(tile, px, py, 95 + n, 159 + n, 53 + n / 2); break;
                    case T_DIRT: put(tile, px, py, 134 + n, 96 + n, 67 + n); break;
                    case T_GRASS_SIDE:
                        if (py < 3 + (int)(hash3(px, 0, 99, 7) % 3)) put(tile, px, py, 95 + n, 159 + n, 53 + n / 2);
                        else put(tile, px, py, 134 + n, 96 + n, 67 + n);
                        break;
                    case T_COBBLE: {
                        const bool mortar = (px % 5 == 0) != (py % 4 == 0) && (hash3(px / 5, py / 4, 3, 5) & 1);
                        const int v = mortar ? 85 : 130;
                        put(tile, px, py, v + n, v + n, v + n);
                        break;
                    }
                    case T_PLANKS: {
                        const int v = (py % 4 == 3) ? -40 : 0;
                        put(tile, px, py, 168 + n / 2 + v, 133 + n / 2 + v, 82 + n / 2 + v);
                        break;
                    }
                    case T_BEDROCK: put(tile, px, py, 60 + n * 3, 60 + n * 3, 60 + n * 3); break;
                    case T_WATER: put(tile, px, py, 40 + n / 2, 80 + n / 2, 220 + n / 2, 180); break;
                    case T_SAND: put(tile, px, py, 219 + n / 2, 207 + n / 2, 163 + n / 2); break;
                    case T_GRAVEL: put(tile, px, py, 135 + n * 2, 127 + n * 2, 125 + n * 2); break;
                    case T_LOG_SIDE: {
                        const int v = (px % 4 == 0) ? -30 : 0;
                        put(tile, px, py, 104 + n / 2 + v, 83 + n / 2 + v, 50 + n / 2 + v);
                        break;
                    }
                    case T_LOG_TOP: {
                        const int d = std::max(std::abs(px - 8), std::abs(py - 8));
                        if (d >= 7) put(tile, px, py, 104 + n, 83 + n, 50 + n);
                        else put(tile, px, py, 180 + (d % 2) * 20 + n / 2, 145 + (d % 2) * 15 + n / 2, 90 + n / 2);
                        break;
                    }
                    case T_LEAVES:
                        if (hash3(px, py, 77, 3) % 5 == 0) put(tile, px, py, 0, 0, 0, 0);
                        else put(tile, px, py, 60 + n, 130 + n, 40 + n);
                        break;
                    case T_GLASS: {
                        const bool edge = px == 0 || py == 0 || px == 15 || py == 15 || (px == py + 3 && px < 8);
                        if (edge) put(tile, px, py, 220, 240, 250);
                        else put(tile, px, py, 0, 0, 0, 0);
                        break;
                    }
                    case T_BRICKS: {
                        const bool mortar = py % 4 == 3 || ((px + (py / 4 % 2) * 4) % 8 == 7);
                        if (mortar) put(tile, px, py, 170 + n / 2, 165 + n / 2, 160 + n / 2);
                        else put(tile, px, py, 150 + n, 70 + n / 2, 55 + n / 2);
                        break;
                    }
                    case T_LAVA: {
                        const int w = (int)(8 * std::sin((px + py * 0.5f) * 0.8f));
                        put(tile, px, py, 230 + n / 2, 100 + w * 4 + n, 20 + n / 2);
                        break;
                    }
                    case T_OBSIDIAN: {
                        if (hash3(px, py, 5, 11) % 9 == 0) put(tile, px, py, 70 + n, 45 + n, 100 + n);
                        else put(tile, px, py, 22 + n / 4, 16 + n / 4, 34 + n / 4);
                        break;
                    }
                    case T_NETHERRACK: put(tile, px, py, 112 + n * 2, 48 + n, 48 + n); break;
                    case T_PORTAL: {
                        const int w = (int)(30 * std::sin(px * 0.9f + std::cos(py * 0.7f) * 2));
                        put(tile, px, py, 120 + w + n, 30 + n / 2, 210 + w / 2 + n, 190);
                        break;
                    }
                    case T_NETHER_BRICK: {
                        const bool mortar = py % 4 == 3 || ((px + (py / 4 % 2) * 4) % 8 == 7);
                        if (mortar) put(tile, px, py, 25, 12, 15);
                        else put(tile, px, py, 60 + n / 2, 28 + n / 3, 34 + n / 3);
                        break;
                    }
                    case T_COAL_ORE:
                    case T_IRON_ORE: {
                        const bool speck = hash3(px / 2, py / 2, tile, 21) % 5 == 0 && hash3(px, py, tile, 3) % 3 != 0;
                        if (!speck) put(tile, px, py, 125 + n, 125 + n, 125 + n);
                        else if (tile == T_COAL_ORE) put(tile, px, py, 30 + n / 2, 30 + n / 2, 30 + n / 2);
                        else put(tile, px, py, 216 + n / 2, 175 + n / 2, 147 + n / 2);
                        break;
                    }
                    case T_FLINT_AND_STEEL: {
                        const int dx = px - 6, dy = py - 6, d2 = dx * dx + dy * dy;
                        const bool ring = d2 >= 9 && d2 <= 22 && !(px > 6 && py > 6);
                        const bool flint = px >= 8 && py >= 8 && px + py <= 26 && px + py >= 17;
                        if (ring) put(tile, px, py, 190, 190, 200);
                        else if (flint) put(tile, px, py, 60 + n, 60 + n, 66 + n);
                        else put(tile, px, py, 0, 0, 0, 0);
                        break;
                    }
                }
            }
}

}  // namespace vc

#include "Font.h"

#include <algorithm>
#include <cstring>

#include "third_party/font8x8/font8x8_basic.h"

namespace vc {

namespace {

enum Accent { Acute, Caron, Ring, Apostrophe, Diaeresis, Tilde };

struct Composite {
    uint32_t cp;
    char base;
    Accent accent;
};

// Czech letters built from a base glyph and an accent.
const Composite kComposites[] = {
    {0xC1, 'A', Acute}, {0x10C, 'C', Caron}, {0x10E, 'D', Caron}, {0xC9, 'E', Acute}, {0x11A, 'E', Caron},
    {0xCD, 'I', Acute}, {0x147, 'N', Caron}, {0xD3, 'O', Acute}, {0x158, 'R', Caron}, {0x160, 'S', Caron},
    {0x164, 'T', Caron}, {0xDA, 'U', Acute}, {0x16E, 'U', Ring}, {0xDD, 'Y', Acute}, {0x17D, 'Z', Caron},
    {0xE1, 'a', Acute}, {0x10D, 'c', Caron}, {0x10F, 'd', Apostrophe}, {0xE9, 'e', Acute}, {0x11B, 'e', Caron},
    {0xED, 'i', Acute}, {0x148, 'n', Caron}, {0xF3, 'o', Acute}, {0x159, 'r', Caron}, {0x161, 's', Caron},
    {0x165, 't', Apostrophe}, {0xFA, 'u', Acute}, {0x16F, 'u', Ring}, {0xFD, 'y', Acute}, {0x17E, 'z', Caron},
    // German and Spanish (Ё and ё of Russian share Ë and ë).
    {0xC4, 'A', Diaeresis}, {0xD6, 'O', Diaeresis}, {0xDC, 'U', Diaeresis}, {0xCB, 'E', Diaeresis},
    {0xE4, 'a', Diaeresis}, {0xF6, 'o', Diaeresis}, {0xFC, 'u', Diaeresis}, {0xEB, 'e', Diaeresis},
    {0xD1, 'N', Tilde}, {0xF1, 'n', Tilde},
};
constexpr int kFirstComposite = 128;

// A few punctuation marks used in Czech text.
struct Symbol {
    uint32_t cp;
    uint8_t rows[8];
};
const Symbol kSymbols[] = {
    {0xB7, {0, 0, 0, 0x0C, 0x0C, 0, 0, 0}},             // middle dot
    {0xB0, {0x0C, 0x12, 0x12, 0x0C, 0, 0, 0, 0}},       // degree
    {0x201E, {0, 0, 0, 0, 0, 0x1B, 0x1B, 0x12}},        // „
    {0x201C, {0x12, 0x1B, 0x1B, 0, 0, 0, 0, 0}},        // “
    {0x201D, {0x1B, 0x1B, 0x12, 0, 0, 0, 0, 0}},        // ”
    {0xD7, {0, 0, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0}},    // multiplication sign
};
constexpr int kFirstSymbol = 170;

// Icons, 8 rows each, bit 0 = leftmost pixel.
const uint8_t kHeartFull[8] = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0, 0};
const uint8_t kHeartHalf[8] = {0x06, 0x0F, 0x0F, 0x0E, 0x0C, 0x08, 0, 0};
const uint8_t kHeartEmpty[8] = {0x36, 0x49, 0x41, 0x22, 0x14, 0x08, 0, 0};
const uint8_t kFoodFull[8] = {0x30, 0x78, 0x7C, 0x3C, 0x1A, 0x05, 0x02, 0};
const uint8_t kFoodHalf[8] = {0x30, 0x78, 0x78, 0x38, 0x18, 0x00, 0x00, 0};
const uint8_t kFoodEmpty[8] = {0x30, 0x48, 0x44, 0x24, 0x1A, 0x05, 0x02, 0};
const uint8_t kBubble[8] = {0x1C, 0x22, 0x45, 0x41, 0x41, 0x22, 0x1C, 0};
const uint8_t kArrowUp[8] = {0x08, 0x1C, 0x3E, 0x7F, 0x1C, 0x1C, 0x1C, 0};
const uint8_t kArrowDown[8] = {0x1C, 0x1C, 0x1C, 0x7F, 0x3E, 0x1C, 0x08, 0};
const uint8_t kCoin[8] = {0x1C, 0x3E, 0x7B, 0x7D, 0x7B, 0x3E, 0x1C, 0};
const uint8_t kStar[8] = {0x08, 0x08, 0x7F, 0x3E, 0x1C, 0x36, 0x22, 0};
const uint8_t kArrowLeft[8] = {0x08, 0x0C, 0x7E, 0x7F, 0x7E, 0x0C, 0x08, 0};
const uint8_t kArrowRight[8] = {0x08, 0x18, 0x3F, 0x7F, 0x3F, 0x18, 0x08, 0};
#include "FontGlyphs.inc"

// Cyrillic letters that look exactly like Latin ones reuse those glyphs.
uint32_t latinTwin(uint32_t cp) {
    switch (cp) {
        case 0x0410: return 'A'; case 0x0412: return 'B'; case 0x0415: return 'E'; case 0x041A: return 'K';
        case 0x041C: return 'M'; case 0x041D: return 'H'; case 0x041E: return 'O'; case 0x0420: return 'P';
        case 0x0421: return 'C'; case 0x0422: return 'T'; case 0x0425: return 'X'; case 0x0430: return 'a';
        case 0x0435: return 'e'; case 0x043E: return 'o'; case 0x0440: return 'p'; case 0x0441: return 'c';
        case 0x0443: return 'y'; case 0x0445: return 'x'; case 0x0401: return 0xCB; case 0x0451: return 0xEB;
        default: return 0;
    }
}

// Glyph slots of the drawn letters: the free cells of the 16x16 texture.
int drawnSlot(int index) {
    static const int kFree[] = {1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,
                                18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  127, 174, 175,
                                176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
                                193, 194, 195, 196, 197, 198, 199};
    static_assert(sizeof kFree / sizeof kFree[0] >= sizeof kDrawn / sizeof kDrawn[0], "not enough glyph slots");
    return kFree[index];
}

const uint8_t kArmorFull[8] = {0x36, 0x7F, 0x7F, 0x3E, 0x3E, 0x3E, 0x3E, 0};
const uint8_t kArmorHalf[8] = {0x06, 0x0F, 0x0F, 0x0E, 0x0E, 0x0E, 0x0E, 0};
const uint8_t kArmorEmpty[8] = {0x36, 0x49, 0x41, 0x22, 0x22, 0x22, 0x3E, 0};
const uint8_t kMapPin[8] = {0x1C, 0x3E, 0x36, 0x3E, 0x1C, 0x08, 0x08, 0};
const uint8_t kHome[8] = {0x08, 0x1C, 0x3E, 0x7F, 0x22, 0x2A, 0x2A, 0};
const uint8_t kSkull[8] = {0x1C, 0x3E, 0x6B, 0x7F, 0x3E, 0x2A, 0x00, 0};
const uint8_t kHouse[8] = {0x08, 0x14, 0x22, 0x7F, 0x41, 0x55, 0x7F, 0};
const uint8_t kSolid[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int lowestSetBit(uint8_t v) { int i = 0; while (i < 8 && !(v >> i & 1)) ++i; return i; }
int highestSetBit(uint8_t v) { int i = 7; while (i >= 0 && !(v >> i & 1)) --i; return i; }

}  // namespace

Font::Font() {
    for (int c = 32; c < 127; ++c) setGlyph(c, reinterpret_cast<const uint8_t*>(font8x8_basic[c]));
    widths_[' '] = 3;

    for (int i = 0; i < (int)(sizeof kComposites / sizeof kComposites[0]); ++i) {
        const Composite& comp = kComposites[i];
        int g = kFirstComposite + i;
        uint8_t rows[8];
        std::memcpy(rows, font8x8_basic[(int)comp.base], 8);
        bool upper = comp.base >= 'A' && comp.base <= 'Z';
        if (comp.base == 'i') rows[0] = 0;  // dotless i under the accent
        setGlyph(g, rows);

        int minCol = 8, maxCol = -1;
        for (int r = 0; r < 8; ++r)
            if (rows[r]) {
                minCol = std::min(minCol, lowestSetBit(rows[r]));
                maxCol = std::max(maxCol, highestSetBit(rows[r]));
            }
        int center = (minCol + maxCol) / 2;
        // Accent shapes, 5 columns wide with their centre on column 2.
        uint8_t acc[3] = {0, 0, 0};
        int accRows = 2;
        switch (comp.accent) {
            case Acute: acc[0] = 0x18; acc[1] = 0x0C; break;
            case Caron: acc[0] = 0x11; acc[1] = 0x0E; break;
            case Ring:
                if (upper) { acc[0] = 0x0E; acc[1] = 0x0E; }
                else { acc[0] = 0x0E; acc[1] = 0x0A; acc[2] = 0x0E; accRows = 3; }
                break;
            case Apostrophe: break;
            case Diaeresis: acc[upper ? 0 : 1] = 0x1B; break;
            case Tilde: acc[0] = 0x16; acc[1] = 0x0D; break;
        }
        // Lowercase letters have two free rows above them, uppercase ones use the headroom.
        int top = upper ? 0 : (accRows == 3 ? 1 : 2);
        if (comp.accent == Apostrophe) {
            int col = maxCol + 2;
            bits_[g][2] |= (uint8_t)(1 << col);
            bits_[g][3] |= (uint8_t)(1 << (col - 1));
            widths_[g] = (uint8_t)(col + 1);
            continue;
        }
        int shift = center - 2;
        for (int r = 0; r < accRows; ++r)
            bits_[g][top + r] |= (uint8_t)(shift >= 0 ? acc[r] << shift : acc[r] >> -shift);
    }

    for (int i = 0; i < (int)(sizeof kSymbols / sizeof kSymbols[0]); ++i) setGlyph(kFirstSymbol + i, kSymbols[i].rows);
    for (int i = 0; i < (int)(sizeof kDrawn / sizeof kDrawn[0]); ++i) setGlyph(drawnSlot(i), kDrawn[i].rows);

    setGlyph(HeartFull, kHeartFull);
    setGlyph(HeartHalf, kHeartHalf);
    setGlyph(HeartEmpty, kHeartEmpty);
    setGlyph(FoodFull, kFoodFull);
    setGlyph(FoodHalf, kFoodHalf);
    setGlyph(FoodEmpty, kFoodEmpty);
    setGlyph(Bubble, kBubble);
    setGlyph(ArrowUp, kArrowUp);
    setGlyph(ArrowDown, kArrowDown);
    setGlyph(Coin, kCoin);
    setGlyph(Star, kStar);
    setGlyph(ArrowLeft, kArrowLeft);
    setGlyph(ArrowRight, kArrowRight);
    setGlyph(ArmorFull, kArmorFull);
    setGlyph(ArmorHalf, kArmorHalf);
    setGlyph(ArmorEmpty, kArmorEmpty);
    setGlyph(MapPin, kMapPin);
    setGlyph(Home, kHome);
    setGlyph(Skull, kSkull);
    setGlyph(House, kHouse);
    setGlyph(Solid, kSolid, 0);
    for (int r = 0; r < kCellH; ++r) bits_[Solid][r] = 0xFF;
    widths_[Solid] = 8;

    rgba_.assign(kTexW * kTexH * 4, 0);
    for (int g = 0; g < 256; ++g) {
        int ox = (g % 16) * kCellW, oy = (g / 16) * kCellH;
        for (int r = 0; r < kCellH; ++r)
            for (int c = 0; c < kCellW; ++c) {
                uint8_t* p = &rgba_[((oy + r) * kTexW + ox + c) * 4];
                p[0] = p[1] = p[2] = 255;
                p[3] = (bits_[g][r] >> c & 1) ? 255 : 0;
            }
    }
}

void Font::setGlyph(int index, const uint8_t rows[8], int topRow) {
    int maxCol = -1;
    for (int r = 0; r < 8 && topRow + r < kCellH; ++r) {
        bits_[index][topRow + r] = rows[r];
        if (rows[r]) maxCol = std::max(maxCol, highestSetBit(rows[r]));
    }
    widths_[index] = (uint8_t)(maxCol + 1);
}

int Font::glyphFor(uint32_t cp) const {
    if (cp >= 32 && cp < 127) return (int)cp;
    for (int i = 0; i < (int)(sizeof kComposites / sizeof kComposites[0]); ++i)
        if (kComposites[i].cp == cp) return kFirstComposite + i;
    for (int i = 0; i < (int)(sizeof kSymbols / sizeof kSymbols[0]); ++i)
        if (kSymbols[i].cp == cp) return kFirstSymbol + i;
    for (int i = 0; i < (int)(sizeof kDrawn / sizeof kDrawn[0]); ++i)
        if (kDrawn[i].cp == cp) return drawnSlot(i);
    if (uint32_t twin = latinTwin(cp)) return glyphFor(twin);
    return '?';
}

void Font::uv(int glyph, float& u0, float& v0, float& u1, float& v1) const {
    u0 = (glyph % 16) * kCellW / (float)kTexW;
    v0 = (glyph / 16) * kCellH / (float)kTexH;
    u1 = u0 + kCellW / (float)kTexW;
    v1 = v0 + kCellH / (float)kTexH;
}

uint32_t Font::nextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = s[i++];
    if (c < 0x80) return c;
    int extra = (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : 0;
    uint32_t cp = c & (0x3F >> extra);
    for (int k = 0; k < extra && i < s.size(); ++k) cp = (cp << 6) | (s[i++] & 0x3F);
    return cp;
}

int Font::measure(const std::string& utf8) const {
    int w = 0;
    for (size_t i = 0; i < utf8.size();) w += advance(glyphFor(nextCodepoint(utf8, i)));
    return w > 0 ? w - 1 : 0;
}

}  // namespace vc

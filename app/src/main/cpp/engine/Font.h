#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vc {

// GUI texture: an 8x8 bitmap font (ASCII + Czech letters) plus HUD icons.
// Every glyph cell is 8x10 pixels; the two extra rows on top hold accents.
class Font {
public:
    static constexpr int kCellW = 8, kCellH = 10;
    // 2.2.0: under the glyphs, 96 rows hold the nine 32 x 32 HUD buttons (rows of four).
    static constexpr int kGlyphRows = 16 * kCellH;
    static constexpr int kTexW = 16 * kCellW, kTexH = kGlyphRows + 96;
    enum HudButton { HudUp, HudDown, HudLeft, HudRight, HudJump, HudSneak, HudCamera, HudPause, HudChat, kHudButtonCount };
    static void hudButtonUv(int button, float& u0, float& v0, float& u1, float& v1) {
        u0 = (button % 4) * 32 / (float)kTexW, v0 = (kGlyphRows + (button / 4) * 32) / (float)kTexH;
        u1 = u0 + 32 / (float)kTexW, v1 = v0 + 32 / (float)kTexH;
    }

    // Icon glyphs (not reachable from text).
    enum Icon { HeartFull = 200, HeartHalf, HeartEmpty, FoodFull, FoodHalf, FoodEmpty, Bubble, ArrowUp, ArrowDown, Solid, Coin, Star, ArrowLeft, ArrowRight,
                ArmorFull, ArmorHalf, ArmorEmpty, MapPin, Home, Skull, House,
                // 1.7.0 preview: the menu button icons
                Sword, Gear, Bag, Person, Cube, ExitDoor };

    Font();
    // 2.2.0 preview: the "Tiny Caps" letters (Silkscreen, from the project's own font pack) instead
    // of the classic 8x8 ones. Rebuilds the texture; the caller uploads rgba() again.
    void setTinyCaps(bool on);
    bool tinyCaps() const { return tiny_; }
    float sizeFactor() const { return tiny_ ? 1.4f : 1.0f; }  // Tiny Caps are drawn as tall as the classic letters
    const std::vector<uint8_t>& rgba() const { return rgba_; }

    // Glyph index for a Unicode code point ('?' when unknown).
    int glyphFor(uint32_t cp) const;
    int advance(int glyph) const { return widths_[glyph] + 1; }
    void uv(int glyph, float& u0, float& v0, float& u1, float& v1) const;

    // Width in font pixels (before scaling).
    int measure(const std::string& utf8) const;

    // 2.2.0: the colour of a "§" code (0-9, a-f as in Minecraft); false for other characters.
    static bool codeColor(char code, float& r, float& g, float& b);
    // Decodes the next code point and advances `i`.
    static uint32_t nextCodepoint(const std::string& s, size_t& i);

private:
    void setGlyph(int index, const uint8_t rows[8], int topRow = 2);
    void build();
    bool tiny_ = false;
    std::vector<uint8_t> rgba_;
    uint8_t widths_[256] = {};
    uint8_t bits_[256][kCellH] = {};  // row bitmaps, bit 0 = leftmost pixel
};

}  // namespace vc

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
    static constexpr int kTexW = 16 * kCellW, kTexH = 16 * kCellH;

    // Icon glyphs (not reachable from text).
    enum Icon { HeartFull = 200, HeartHalf, HeartEmpty, FoodFull, FoodHalf, FoodEmpty, Bubble, ArrowUp, ArrowDown, Solid, Coin, Star, ArrowLeft, ArrowRight,
                ArmorFull, ArmorHalf, ArmorEmpty, MapPin, Home, Skull, House,
                // 1.7.0 preview: the menu button icons
                Sword, Gear, Bag, Person, Cube, ExitDoor };

    Font();
    const std::vector<uint8_t>& rgba() const { return rgba_; }

    // Glyph index for a Unicode code point ('?' when unknown).
    int glyphFor(uint32_t cp) const;
    int advance(int glyph) const { return widths_[glyph] + 1; }
    void uv(int glyph, float& u0, float& v0, float& u1, float& v1) const;

    // Width in font pixels (before scaling).
    int measure(const std::string& utf8) const;

    // Decodes the next code point and advances `i`.
    static uint32_t nextCodepoint(const std::string& s, size_t& i);

private:
    void setGlyph(int index, const uint8_t rows[8], int topRow = 2);
    std::vector<uint8_t> rgba_;
    uint8_t widths_[256] = {};
    uint8_t bits_[256][kCellH] = {};  // row bitmaps, bit 0 = leftmost pixel
};

}  // namespace vc

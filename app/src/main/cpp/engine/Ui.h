#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Blocks.h"
#include "Font.h"
#include "Lang.h"

namespace vc {

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
};

enum UiTex { TexNone = 0, TexAtlas = 1, TexGui = 2, TexSkins = 3, TexMap = 4 };

// Colours of the retro menu theme (dark wood panels, moss green highlights).
namespace theme {
constexpr Color kBackdrop{0.07f, 0.06f, 0.05f, 0.82f};
constexpr Color kPanel{0.165f, 0.145f, 0.125f, 1};
constexpr Color kPanelInner{0.21f, 0.19f, 0.165f, 1};
constexpr Color kBorder{0.43f, 0.4f, 0.35f, 1};
constexpr Color kBorderDark{0.1f, 0.09f, 0.08f, 1};
constexpr Color kButton{0.235f, 0.21f, 0.185f, 1};
constexpr Color kButtonDown{0.3f, 0.27f, 0.235f, 1};
constexpr Color kGreen{0.165f, 0.31f, 0.04f, 1};
constexpr Color kGreenDown{0.22f, 0.4f, 0.07f, 1};
constexpr Color kGreenBorder{0.42f, 0.56f, 0.23f, 1};
constexpr Color kAccent{0.36f, 0.5f, 0.17f, 1};
constexpr Color kText{0.79f, 0.72f, 0.61f, 1};
constexpr Color kTextBright{0.9f, 0.85f, 0.72f, 1};
constexpr Color kTextDim{0.6f, 0.55f, 0.47f, 1};
constexpr Color kTitle{0.76f, 0.66f, 0.53f, 1};
constexpr Color kSeparator{0.29f, 0.27f, 0.24f, 1};
constexpr Color kField{0.08f, 0.07f, 0.055f, 1};
constexpr Color kCode{0.5f, 0.75f, 0.42f, 1};
constexpr Color kError{0.9f, 0.4f, 0.32f, 1};
constexpr Color kGold{0.85f, 0.65f, 0.25f, 1};
}  // namespace theme

struct UiVert {
    float x, y, u, v, r, g, b, a;
};

// Triangles to draw, split into runs that share a texture (kept in order).
struct UiList {
    std::vector<UiVert> verts;
    std::vector<std::pair<int, int>> runs;  // (UiTex, vertex count)
};

// Small immediate-mode GUI working in virtual "GUI pixels" (the screen size
// divided by the GUI scale), so the 8px font stays crisp at any resolution.
class Ui {
public:
    explicit Ui(const Font& font) : font_(font) {}

    // Called when a button, checkbox or dropdown is activated (for the click sound).
    std::function<void()> onClick;

    void begin(float width, float height);
    void endFrame();
    float width() const { return w_; }
    float height() const { return h_; }
    const UiList& list() const { return list_; }

    // Input, already in GUI pixels.
    void touchDown(int id, float x, float y);
    void touchMove(int id, float x, float y);
    void touchUp(int id, float x, float y);
    void cancelTouches();

    // Drawing
    void fill(float x, float y, float w, float h, Color c);
    void image(UiTex tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color c = {});
    void quad(UiTex tex, const float pts[4][2], const float uvs[4][2], Color c);
    void tile(float x, float y, float size, int tile, Color c = {});
    void item(float x, float y, float size, ItemId id);  // blocks drawn as little 3D cubes
    void icon(float x, float y, int glyph, Color c, float scale = 1);
    // maxWidth > 0 cuts the text off at that width.
    float text(float x, float y, const std::string& s, Color c = {}, float scale = 1, bool shadow = true,
               float maxWidth = 0);
    void textCentered(float cx, float y, const std::string& s, Color c = {}, float scale = 1);
    float textWidth(const std::string& s, float scale = 1) const { return font_.measure(tr(s)) * scale; }
    // Tests: when set, every drawn text (after translation) is appended here.
    static std::vector<std::string>* textLog;
    void bevel(float x, float y, float w, float h, Color face, Color light, Color dark);
    void panel(float x, float y, float w, float h);  // light grey inventory window
    void slotFrame(float x, float y, float size, bool highlight);

    // Retro theme pieces
    void frame(float x, float y, float w, float h, Color face, Color border);
    void separator(float x, float y, float w) { fill(x, y, w, 1, theme::kSeparator); }

    // Widgets. `primary` draws the green variant (main action, selected tab).
    bool button(float x, float y, float w, float h, const std::string& label, bool enabled = true, bool primary = false);
    // 1.7.0 preview UI: a rounded, raised menu button with an optional icon glyph.
    enum class Style { Normal, Primary, Danger };
    bool menuButton(float x, float y, float w, float h, const std::string& label, Style style, int iconGlyph = -1, bool enabled = true);
    // A rounded dark glass panel with a light rim (the menus' new frame).
    void roundPanel(float x, float y, float w, float h);
    // A filled rectangle with rounded corners of radius r (0..4 GUI pixels).
    void roundRect(float x, float y, float w, float h, int r, Color c);
    // In-game touch button (D-pad, jump, pause...): dark glass panel with a block-style bevel.
    void hudButton(float x, float y, float w, float h, bool pressed);
    void hudGlyph(float x, float y, float w, float h, int glyph, float scale = 2);
    bool checkbox(float x, float y, bool& value);                  // 12x12
    bool themedSlider(float x, float y, float w, float& value);   // 10 tall, value in [0, 1]
    bool dropdown(float x, float y, float w, float h, const std::string& value);  // tapped = cycle the value
    bool textBox(float x, float y, float w, float h, const std::string& value, const std::string& placeholder);
    // `value` in [0, 1]; returns true while it changes.
    bool slider(float x, float y, float w, float h, const std::string& label, float& value);
    // A finished tap (finger lifted without dragging) inside the rectangle; consumes it.
    bool tapped(float x, float y, float w, float h);
    bool held(float x, float y, float w, float h) const;
    // Vertical drag distance this frame for fingers that started inside.
    float dragY(float x, float y, float w, float h) const;
    float dragX(float x, float y, float w, float h) const;  // 1.5.7: the same sideways
    // For a dialog over a screen: hold this frame's taps back while the screen under it is
    // drawn, then give them back for the dialog's own buttons.
    void holdTaps(bool hold) {
        if (hold) heldTaps_.swap(taps_);
        else taps_.swap(heldTaps_), heldTaps_.clear();
    }

private:
    struct Pointer {
        int id = -1;
        float x = 0, y = 0, startX = 0, startY = 0, frameDy = 0, frameDx = 0;
        bool moved = false;
    };
    struct Tap {
        float sx, sy, x, y;
        bool consumed = false;
    };
    static bool inside(float px, float py, float x, float y, float w, float h) {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    void addTri(UiTex tex, const UiVert& a, const UiVert& b, const UiVert& c);
    float drawText(float x, float y, const std::string& s, Color c, float scale, float maxWidth);

    const Font& font_;
    float w_ = 1, h_ = 1;
    UiList list_;
    Pointer pointers_[10];
    std::vector<Tap> taps_, heldTaps_;
};

}  // namespace vc

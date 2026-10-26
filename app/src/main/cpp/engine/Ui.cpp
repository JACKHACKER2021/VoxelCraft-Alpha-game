#include "Ui.h"

#include "Lang.h"

#include <algorithm>
#include <cmath>

#include "BlockShapes.h"
#include "Blocks.h"
#include "Items.h"

namespace vc {

namespace {
constexpr float kAtlasTile = kTileUv;
constexpr float kDragThreshold = 6;
}  // namespace

void Ui::begin(float width, float height) {
    w_ = width;
    h_ = height;
    list_.verts.clear();
    list_.runs.clear();
}

void Ui::endFrame() {
    taps_.clear();
    for (Pointer& p : pointers_) p.frameDy = 0;
}

void Ui::touchDown(int id, float x, float y) {
    for (Pointer& p : pointers_)
        if (p.id < 0) {
            p = {id, x, y, x, y, 0, false};
            return;
        }
}

void Ui::touchMove(int id, float x, float y) {
    for (Pointer& p : pointers_)
        if (p.id == id) {
            p.frameDy += y - p.y;
            p.x = x;
            p.y = y;
            if (std::hypot(x - p.startX, y - p.startY) > kDragThreshold) p.moved = true;
        }
}

void Ui::touchUp(int id, float x, float y) {
    for (Pointer& p : pointers_)
        if (p.id == id) {
            if (!p.moved) taps_.push_back({p.startX, p.startY, x, y});
            p.id = -1;
        }
}

void Ui::cancelTouches() {
    for (Pointer& p : pointers_) p.id = -1;
    taps_.clear();
}

void Ui::addTri(UiTex tex, const UiVert& a, const UiVert& b, const UiVert& c) {
    if (list_.runs.empty() || list_.runs.back().first != tex) list_.runs.push_back({tex, 0});
    list_.verts.insert(list_.verts.end(), {a, b, c});
    list_.runs.back().second += 3;
}

void Ui::quad(UiTex tex, const float p[4][2], const float uv[4][2], Color c) {
    UiVert v[4];
    for (int i = 0; i < 4; ++i) v[i] = {p[i][0], p[i][1], uv[i][0], uv[i][1], c.r, c.g, c.b, c.a};
    addTri(tex, v[0], v[1], v[2]);
    addTri(tex, v[0], v[2], v[3]);
}

void Ui::image(UiTex tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color c) {
    const float p[4][2] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    quad(tex, p, uv, c);
}

void Ui::fill(float x, float y, float w, float h, Color c) { image(TexNone, x, y, w, h, 0, 0, 0, 0, c); }

void Ui::tile(float x, float y, float size, int t, Color c) {
    float u0 = (t % kAtlasCols) * kAtlasTile, v0 = (t / kAtlasCols) * kAtlasTile;
    image(TexAtlas, x, y, size, size, u0, v0, u0 + kAtlasTile, v0 + kAtlasTile, c);
}

void Ui::item(float x, float y, float size, ItemId id) {
    if (id == AIR) return;
    if (isItemOnly(id) || isLiquid(id) || isPlant(id) || isLeaves(id) || id == GLASS || id == POWERSTONE_GLASS || isDoor(id)) {
        // Flat icon, like items and see-through blocks in the original inventory.
        float pad = size * 0.1f;
        tile(x + pad, y + pad, size - 2 * pad, tileFor(id, F_SOUTH));
        return;
    }
    // Isometric boxes (a cube, or the shape of stairs and slabs): top, south and east faces
    // with the classic shading, the part of the tile matching each face.
    const float cx = x + size / 2, w = size * 0.9f, y0 = y + (size - w) / 2;
    auto P = [&](float bx, float by, float bz, float out[2]) {
        out[0] = cx + (bx - bz) * w / 2;
        out[1] = y0 + (bx + bz) * w / 4 + (1 - by) * w / 2;
    };
    auto face = [&](int tile, const float pts[4][3], const float st[4][2], Color c) {
        const float u0 = (tile % kAtlasCols) * kAtlasTile, v0 = (tile / kAtlasCols) * kAtlasTile;
        float xy[4][2], uv[4][2];
        for (int i = 0; i < 4; ++i) {
            P(pts[i][0], pts[i][1], pts[i][2], xy[i]);
            uv[i][0] = u0 + st[i][0] * kAtlasTile;
            uv[i][1] = v0 + st[i][1] * kAtlasTile;
        }
        quad(TexAtlas, xy, uv, c);
    };
    Box boxes[kItemBoxes];
    const int n = itemBoxes(id, boxes);
    for (int i = 0; i < n; ++i) {
        const Box& b = boxes[i];
        const float top[4][3] = {{b.x0, b.y1, b.z0}, {b.x1, b.y1, b.z0}, {b.x1, b.y1, b.z1}, {b.x0, b.y1, b.z1}};
        const float topSt[4][2] = {{b.x0, b.z0}, {b.x1, b.z0}, {b.x1, b.z1}, {b.x0, b.z1}};
        const float south[4][3] = {{b.x0, b.y1, b.z1}, {b.x1, b.y1, b.z1}, {b.x1, b.y0, b.z1}, {b.x0, b.y0, b.z1}};
        const float southSt[4][2] = {{b.x0, 1 - b.y1}, {b.x1, 1 - b.y1}, {b.x1, 1 - b.y0}, {b.x0, 1 - b.y0}};
        const float east[4][3] = {{b.x1, b.y1, b.z1}, {b.x1, b.y1, b.z0}, {b.x1, b.y0, b.z0}, {b.x1, b.y0, b.z1}};
        const float eastSt[4][2] = {{1 - b.z1, 1 - b.y1}, {1 - b.z0, 1 - b.y1}, {1 - b.z0, 1 - b.y0}, {1 - b.z1, 1 - b.y0}};
        face(tileFor(id, F_TOP), top, topSt, {1, 1, 1, 1});
        face(tileFor(id, F_SOUTH), south, southSt, {0.8f, 0.8f, 0.8f, 1});
        face(tileFor(id, F_EAST), east, eastSt, {0.6f, 0.6f, 0.6f, 1});
    }
}

void Ui::icon(float x, float y, int glyph, Color c, float scale) {
    float u0, v0, u1, v1;
    font_.uv(glyph, u0, v0, u1, v1);
    // Icons sit in rows 2..9 of the cell, like letters.
    image(TexGui, x, y - 2 * scale, Font::kCellW * scale, Font::kCellH * scale, u0, v0, u1, v1, c);
}

std::vector<std::string>* Ui::textLog = nullptr;

float Ui::text(float x, float y, const std::string& source, Color c, float scale, bool shadow, float maxWidth) {
    // Every text on screen goes through the translation table.
    const std::string s = tr(source);
    if (textLog) textLog->push_back(s);
    if (shadow) {
        Color sc{c.r * 0.25f, c.g * 0.25f, c.b * 0.25f, c.a};
        drawText(x + scale, y + scale, s, sc, scale, maxWidth);
    }
    return drawText(x, y, s, c, scale, maxWidth);
}

float Ui::drawText(float x, float y, const std::string& s, Color c, float scale, float maxWidth) {
    float start = x;
    for (size_t i = 0; i < s.size();) {
        int g = font_.glyphFor(Font::nextCodepoint(s, i));
        if (maxWidth > 0 && x + font_.advance(g) * scale - start > maxWidth) break;
        float u0, v0, u1, v1;
        font_.uv(g, u0, v0, u1, v1);
        // y is the top of the letters; the cell starts two rows higher for accents.
        image(TexGui, x, y - 2 * scale, Font::kCellW * scale, Font::kCellH * scale, u0, v0, u1, v1, c);
        x += font_.advance(g) * scale;
    }
    return x - start;
}

void Ui::textCentered(float cx, float y, const std::string& s, Color c, float scale) {
    text(cx - textWidth(s, scale) / 2, y, s, c, scale);
}

void Ui::bevel(float x, float y, float w, float h, Color face, Color light, Color dark) {
    fill(x, y, w, h, dark);
    fill(x, y, w - 1, h - 1, light);
    fill(x + 1, y + 1, w - 2, h - 2, face);
}

void Ui::panel(float x, float y, float w, float h) {
    fill(x, y, w, h, {0, 0, 0, 1});
    bevel(x + 1, y + 1, w - 2, h - 2, {0.776f, 0.776f, 0.776f, 1}, {1, 1, 1, 1}, {0.333f, 0.333f, 0.333f, 1});
}

void Ui::slotFrame(float x, float y, float size, bool highlight) {
    // Sunken slot: dark top-left edge, light bottom-right edge.
    fill(x, y, size, size, {1, 1, 1, 1});
    fill(x, y, size - 1, size - 1, {0.216f, 0.216f, 0.216f, 1});
    Color face = highlight ? Color{0.6f, 0.75f, 0.95f, 1} : Color{0.545f, 0.545f, 0.545f, 1};
    fill(x + 1, y + 1, size - 2, size - 2, face);
}

bool Ui::tapped(float x, float y, float w, float h) {
    for (Tap& t : taps_)
        if (!t.consumed && inside(t.sx, t.sy, x, y, w, h) && inside(t.x, t.y, x, y, w, h)) {
            t.consumed = true;
            return true;
        }
    return false;
}

bool Ui::held(float x, float y, float w, float h) const {
    for (const Pointer& p : pointers_)
        if (p.id >= 0 && inside(p.startX, p.startY, x, y, w, h) && inside(p.x, p.y, x, y, w, h)) return true;
    return false;
}

float Ui::dragY(float x, float y, float w, float h) const {
    float d = 0;
    for (const Pointer& p : pointers_)
        if (p.id >= 0 && p.moved && inside(p.startX, p.startY, x, y, w, h)) d += p.frameDy;
    return d;
}

void Ui::frame(float x, float y, float w, float h, Color face, Color border) {
    fill(x, y, w, h, border);
    fill(x + 1, y + 1, w - 2, h - 2, face);
}

bool Ui::button(float x, float y, float w, float h, const std::string& label, bool enabled, bool primary) {
    using namespace theme;
    bool down = enabled && held(x, y, w, h);
    Color face = primary ? (down ? kGreenDown : kGreen) : (down ? kButtonDown : kButton);
    Color border = primary ? kGreenBorder : kBorder;
    if (!enabled) face = {0.18f, 0.165f, 0.145f, 1};
    // Block-style button: a 1px drop shadow, a frame and an inner bevel (light top-left,
    // dark bottom-right). Pressed buttons sink by a pixel and lose the shadow.
    float dy = down ? 1 : 0;
    if (!down) fill(x + 1, y + h, w - 1, 1, kBorderDark);
    frame(x, y + dy, w, h, face, border);
    if (enabled) {
        Color light{1, 1, 1, down ? 0.04f : 0.1f}, dark{0, 0, 0, down ? 0.08f : 0.22f};
        fill(x + 1, y + dy + 1, w - 2, 1, light);
        fill(x + 1, y + dy + 1, 1, h - 2, light);
        fill(x + 1, y + dy + h - 2, w - 2, 1, dark);
        fill(x + w - 2, y + dy + 1, 1, h - 2, dark);
    }
    Color tc = !enabled ? kTextDim : primary ? kTextBright : kText;
    float tw = textWidth(label);
    text(x + (w - tw) / 2, y + dy + (h - 8) / 2, label, tc, 1, false, w - 4);
    bool hit = enabled && tapped(x, y, w, h);
    if (hit && onClick) onClick();
    return hit;
}

void Ui::hudButton(float x, float y, float w, float h, bool pressed) {
    using namespace theme;
    Color face = pressed ? Color{0.2f, 0.36f, 0.06f, 0.82f} : Color{0.1f, 0.09f, 0.08f, 0.55f};
    Color border = pressed ? kGreenBorder : Color{kBorder.r, kBorder.g, kBorder.b, 0.85f};
    fill(x, y, w, h, border);
    fill(x + 1, y + 1, w - 2, h - 2, face);
    // Bevel like the edge of a block: light top-left, dark bottom-right (swapped when pressed).
    Color light{1, 1, 1, pressed ? 0.05f : 0.14f}, dark{0, 0, 0, pressed ? 0.1f : 0.3f};
    fill(x + 1, y + 1, w - 2, 1, pressed ? dark : light);
    fill(x + 1, y + 1, 1, h - 2, pressed ? dark : light);
    fill(x + 1, y + h - 2, w - 2, 1, pressed ? light : dark);
    fill(x + w - 2, y + 1, 1, h - 2, pressed ? light : dark);
}

void Ui::hudGlyph(float x, float y, float w, float h, int glyph, float scale) {
    icon(std::floor(x + (w - 7 * scale) / 2), std::floor(y + (h - 7 * scale) / 2), glyph, theme::kTextBright, scale);
}

bool Ui::checkbox(float x, float y, bool& value) {
    using namespace theme;
    // A generous invisible margin makes the small box easy to hit.
    bool hit = tapped(x - 6, y - 6, 24, 24);
    if (hit) value = !value;
    if (hit && onClick) onClick();
    frame(x, y, 12, 12, value ? kAccent : kPanel, value ? kGreenBorder : kBorder);
    if (value) {
        // Check mark
        const Color m = {0.12f, 0.16f, 0.06f, 1};
        fill(x + 2, y + 6, 2, 2, m);
        fill(x + 4, y + 8, 2, 2, m);
        fill(x + 6, y + 6, 2, 2, m);
        fill(x + 8, y + 4, 2, 2, m);
        fill(x + 9, y + 2, 2, 2, m);
    }
    return hit;
}

bool Ui::themedSlider(float x, float y, float w, float& value) {
    using namespace theme;
    bool changed = false;
    for (const Pointer& p : pointers_)
        if (p.id >= 0 && inside(p.startX, p.startY, x - 6, y - 6, w + 12, 22)) {
            float v = std::clamp((p.x - x) / w, 0.0f, 1.0f);
            if (v != value) { value = v; changed = true; }
        }
    frame(x, y + 3, w, 4, kField, kSeparator);
    fill(x + 1, y + 4, (w - 2) * value, 2, kAccent);
    float kx = x + value * (w - 8);
    frame(kx, y, 8, 10, kAccent, kGreenBorder);
    return changed;
}

bool Ui::dropdown(float x, float y, float w, float h, const std::string& value) {
    using namespace theme;
    bool down = held(x, y, w, h);
    frame(x, y, w, h, down ? kButtonDown : kPanelInner, kBorder);
    text(x + 5, y + (h - 8) / 2, value, kText, 1, false, w - 16);
    // Chevron
    float cx = x + w - 9, cy = y + h / 2 - 1;
    fill(cx, cy, 5, 1, kTextDim);
    fill(cx + 1, cy + 1, 3, 1, kTextDim);
    fill(cx + 2, cy + 2, 1, 1, kTextDim);
    bool hit = tapped(x, y, w, h);
    if (hit && onClick) onClick();
    return hit;
}

bool Ui::textBox(float x, float y, float w, float h, const std::string& value, const std::string& placeholder) {
    using namespace theme;
    bool down = held(x, y, w, h);
    frame(x, y, w, h, kField, down ? kTextBright : kBorder);
    if (value.empty()) text(x + 5, y + (h - 8) / 2, placeholder, kTextDim, 1, false, w - 10);
    else text(x + 5, y + (h - 8) / 2, value, kTextBright, 1, false, w - 10);
    return tapped(x, y, w, h);
}

bool Ui::slider(float x, float y, float w, float h, const std::string& label, float& value) {
    bool changed = false;
    for (const Pointer& p : pointers_)
        if (p.id >= 0 && inside(p.startX, p.startY, x, y, w, h)) {
            float v = std::clamp((p.x - x - 4) / (w - 8), 0.0f, 1.0f);
            if (v != value) { value = v; changed = true; }
        }
    fill(x, y, w, h, {0, 0, 0, 1});
    fill(x + 1, y + 1, w - 2, h - 2, {0.3f, 0.3f, 0.3f, 1});
    float kx = x + 1 + value * (w - 10);
    button(kx, y, 8, h, "", true);
    textCentered(x + w / 2, y + (h - 8) / 2, label, {0.88f, 0.88f, 0.88f, 1});
    return changed;
}

}  // namespace vc

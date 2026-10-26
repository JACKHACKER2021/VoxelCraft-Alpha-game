#include "Skin.h"
#include "Lang.h"

#include <algorithm>
#include <cstring>

#include "Noise.h"
#include "third_party/stb/stb_image.h"  // implementation lives in TexturePack.cpp

namespace vc {

bool Skin::fromRgba(const uint8_t* rgba, int w, int h, Skin& out) {
    if (w != kSize || (h != kSize && h != kSize / 2)) return false;
    out.rgba.assign((size_t)kSize * kSize * 4, 0);
    std::memcpy(out.rgba.data(), rgba, (size_t)w * h * 4);
    if (h == kSize / 2) {
        // Legacy 64x32 skins have one arm and one leg; reuse them for the other side.
        auto copy = [&](int sx, int sy, int dx, int dy) {
            for (int y = 0; y < 16; ++y)
                std::memcpy(&out.rgba[((dy + y) * kSize + dx) * 4], &rgba[((sy + y) * w + sx) * 4], 16 * 4);
        };
        copy(0, 16, 16, 48);   // leg
        copy(40, 16, 32, 48);  // arm
    }
    return true;
}

bool Skin::loadPng(const std::string& path, Skin& out, std::string& error) {
    int w = 0, h = 0, n = 0;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) {
        error = "soubor nejde přečíst jako PNG";
        return false;
    }
    bool ok = fromRgba(data, w, h, out);
    stbi_image_free(data);
    if (!ok) error = trf("skin musí mít 64×64 nebo 64×32 pixelů (má {0}×{1})", {std::to_string(w), std::to_string(h)});
    return ok;
}

namespace {

struct Rgb {
    int r, g, b;
};

class Painter {
public:
    explicit Painter(Skin& s, uint32_t seed) : s_(s), seed_(seed) { s_.rgba.assign((size_t)Skin::kSize * Skin::kSize * 4, 0); }

    void px(int x, int y, Rgb c, int a = 255) {
        if (x < 0 || y < 0 || x >= Skin::kSize || y >= Skin::kSize) return;
        // A little grain so flat colours look like pixel art, not plastic.
        int n = (int)(hash3(x, y, 0, seed_) % 13) - 6;
        uint8_t* p = &s_.rgba[(y * Skin::kSize + x) * 4];
        p[0] = (uint8_t)std::clamp(c.r + n, 0, 255);
        p[1] = (uint8_t)std::clamp(c.g + n, 0, 255);
        p[2] = (uint8_t)std::clamp(c.b + n, 0, 255);
        p[3] = (uint8_t)a;
    }
    void clear(int x, int y, int w, int h) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) std::memset(&s_.rgba[((y + j) * Skin::kSize + x + i) * 4], 0, 4);
    }
    void rect(int x, int y, int w, int h, Rgb c) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) px(x + i, y + j, c);
    }
    // Paints all six faces of a part in one colour.
    void box(const SkinPart& p, Rgb c) {
        rect(p.u + p.d, p.v, p.w * 2, p.d, c);       // top + bottom
        rect(p.u, p.v + p.d, 2 * (p.d + p.w), p.h, c);  // sides, front, back
    }
    // Colours a horizontal band (rows y0..y1 of the part's side faces) all around a part.
    void band(const SkinPart& p, int y0, int y1, Rgb c) { rect(p.u, p.v + p.d + y0, 2 * (p.d + p.w), y1 - y0, c); }
    void top(const SkinPart& p, Rgb c) { rect(p.u + p.d, p.v, p.w, p.d, c); }
    // Front face pixel of a part.
    void front(const SkinPart& p, int x, int y, Rgb c) { px(p.u + p.d + x, p.v + p.d + y, c); }

private:
    Skin& s_;
    uint32_t seed_;
};

Skin make(const char* id, const char* name, uint32_t seed, void (*paint)(Painter&)) {
    Skin s;
    s.id = id;
    s.name = name;
    Painter p(s, seed);
    paint(p);
    return s;
}

using namespace skinparts;

void face(Painter& p, Rgb skin, Rgb hair, Rgb eyes) {
    p.box(kHead, skin);
    p.top(kHead, hair);
    p.band(kHead, 0, 2, hair);
    p.front(kHead, 1, 4, {255, 255, 255});
    p.front(kHead, 2, 4, eyes);
    p.front(kHead, 5, 4, eyes);
    p.front(kHead, 6, 4, {255, 255, 255});
    p.front(kHead, 3, 6, {150, 90, 80});
    p.front(kHead, 4, 6, {150, 90, 80});
}

void explorer(Painter& p) {
    const Rgb skin{214, 165, 122}, shirt{51, 153, 158}, pants{61, 69, 158}, shoes{90, 90, 90};
    face(p, skin, {84, 51, 26}, {64, 77, 178});
    p.box(kBody, shirt);
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, shirt);
        p.band(a, 8, 12, skin);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, pants);
        p.band(l, 10, 12, shoes);
    }
}

void miner(Painter& p) {
    const Rgb skin{198, 140, 100}, vest{230, 120, 30}, shirt{70, 70, 80}, pants{50, 50, 60};
    face(p, skin, {40, 30, 20}, {60, 50, 40});
    p.box(kHat, {235, 190, 30});  // helmet
    p.clear(kHat.u, kHat.v + kHat.d + 3, 2 * (kHat.d + kHat.w), kHat.h - 3);  // only the helmet's top part
    p.px(kHat.u + kHat.d + 3, kHat.v + kHat.d + 1, {255, 250, 200});           // head lamp
    p.px(kHat.u + kHat.d + 4, kHat.v + kHat.d + 1, {255, 250, 200});
    p.box(kBody, shirt);
    p.rect(kBody.u + kBody.d, kBody.v + kBody.d, 8, 12, vest);
    p.rect(kBody.u + kBody.d + 3, kBody.v + kBody.d, 2, 12, shirt);
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, shirt);
        p.band(a, 10, 12, skin);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, pants);
        p.band(l, 9, 12, {60, 40, 25});
    }
}

void gardener(Painter& p) {
    const Rgb skin{240, 200, 170}, overalls{70, 130, 60}, shirt{240, 230, 210};
    face(p, skin, {200, 120, 40}, {60, 120, 60});
    p.box(kBody, shirt);
    p.rect(kBody.u + kBody.d, kBody.v + kBody.d + 4, 8, 8, overalls);
    p.front(kBody, 1, 0, overalls);
    p.front(kBody, 6, 0, overalls);
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, skin);
        p.band(a, 0, 4, shirt);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, overalls);
        p.band(l, 10, 12, {120, 80, 40});
    }
}

void knight(Painter& p) {
    const Rgb steel{165, 170, 178}, dark{95, 100, 110}, red{170, 30, 35};
    p.box(kHead, steel);
    p.band(kHead, 3, 4, {20, 20, 25});  // visor slit
    p.box(kBody, steel);
    p.rect(kBody.u + kBody.d + 2, kBody.v + kBody.d + 2, 4, 8, red);  // tabard
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, steel);
        p.band(a, 4, 5, dark);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, dark);
        p.band(l, 0, 6, steel);
    }
}

void robot(Painter& p) {
    const Rgb metal{120, 130, 140}, panel{80, 88, 100}, glow{60, 230, 240};
    p.box(kHead, metal);
    p.front(kHead, 2, 3, glow);
    p.front(kHead, 5, 3, glow);
    for (int x = 2; x < 6; ++x) p.front(kHead, x, 6, panel);
    p.box(kBody, metal);
    p.rect(kBody.u + kBody.d + 2, kBody.v + kBody.d + 3, 4, 4, panel);
    p.front(kBody, 3, 4, glow);
    p.front(kBody, 4, 5, glow);
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, panel);
        p.band(a, 4, 5, metal);
        p.band(a, 8, 9, metal);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, panel);
        p.band(l, 6, 7, metal);
    }
}

void astronaut(Painter& p) {
    const Rgb suit{235, 235, 240}, visor{40, 60, 110}, accent{230, 120, 40};
    p.box(kHead, suit);
    p.rect(kHead.u + kHead.d + 1, kHead.v + kHead.d + 2, 6, 4, visor);
    p.front(kHead, 5, 2, {150, 180, 230});
    p.box(kBody, suit);
    p.rect(kBody.u + kBody.d + 1, kBody.v + kBody.d + 2, 3, 2, accent);
    p.rect(kBody.u + kBody.d + 5, kBody.v + kBody.d + 2, 2, 3, {60, 120, 200});
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, suit);
        p.band(a, 10, 12, accent);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, suit);
        p.band(l, 10, 12, {120, 120, 130});
    }
}

void netherWalker(Painter& p) {
    const Rgb skin{120, 40, 30}, cloth{35, 20, 25}, ember{255, 140, 30};
    face(p, skin, {25, 15, 15}, ember);
    p.box(kBody, cloth);
    for (int y = 0; y < 12; y += 3) p.front(kBody, (y * 5) % 8, y, ember);
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, cloth);
        p.band(a, 9, 12, skin);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, cloth);
        p.band(l, 11, 12, ember);
    }
}

void vipGold(Painter& p) {
    const Rgb skin{214, 165, 122}, gold{235, 190, 50}, dark{170, 120, 20}, gem{200, 30, 60};
    face(p, skin, {60, 40, 20}, {40, 90, 180});
    p.box(kHat, gold);  // a crown
    p.clear(kHat.u, kHat.v + kHat.d + 2, 2 * (kHat.d + kHat.w), kHat.h - 2);
    p.px(kHat.u + kHat.d + 3, kHat.v + kHat.d + 1, gem);
    p.px(kHat.u + kHat.d + 4, kHat.v + kHat.d + 1, gem);
    p.box(kBody, gold);
    p.rect(kBody.u + kBody.d + 3, kBody.v + kBody.d, 2, 12, dark);
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, gold);
        p.band(a, 9, 12, skin);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) {
        p.box(l, dark);
        p.band(l, 10, 12, {40, 30, 20});
    }
}

void vipTux(Painter& p) {
    const Rgb skin{230, 190, 160}, black{25, 25, 30}, white{240, 240, 240}, gold{235, 190, 50};
    face(p, skin, {30, 25, 20}, {50, 50, 60});
    p.box(kBody, black);
    p.rect(kBody.u + kBody.d + 2, kBody.v + kBody.d, 4, 7, white);
    p.rect(kBody.u + kBody.d + 3, kBody.v + kBody.d, 2, 1, gold);  // a golden bow tie
    p.rect(kBody.u + kBody.d + 3, kBody.v + kBody.d + 2, 2, 4, gold);
    for (const SkinPart& a : {kRightArm, kLeftArm}) {
        p.box(a, black);
        p.band(a, 10, 11, white);
        p.band(a, 11, 12, skin);
    }
    for (const SkinPart& l : {kRightLeg, kLeftLeg}) p.box(l, black);
}

}  // namespace

namespace {
#include "DeveloperSkin.inc"
}  // namespace

Skin developerSkin() {
    Skin s;
    s.id = "developer";
    s.name = "Hoppy_Hopsy21";
    s.rgba.assign(kDeveloperSkin, kDeveloperSkin + sizeof kDeveloperSkin);
    s.developerOnly = true;
    return s;
}

static Skin vip(Skin s) {
    s.vipOnly = true;
    return s;
}

std::vector<Skin> builtinSkins() {
    return {
        make("explorer", "Průzkumník", 1, explorer),
        make("miner", "Horník", 2, miner),
        make("gardener", "Zahradnice", 3, gardener),
        make("knight", "Rytíř", 4, knight),
        make("robot", "Robot V-01", 5, robot),
        make("astronaut", "Astronautka", 6, astronaut),
        make("nether_walker", "Poutník Netheru", 7, netherWalker),
        vip(make("vip_gold", "Zlatý VIP", 8, vipGold)),
        vip(make("vip_tux", "VIP Gentleman", 9, vipTux)),
    };
}

}  // namespace vc

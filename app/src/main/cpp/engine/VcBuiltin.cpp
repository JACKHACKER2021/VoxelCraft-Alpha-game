#include "VcBuiltin.h"

#include <cmath>
#include <string>

#include "third_party/stb/stb_image.h"  // implementation lives in TexturePack.cpp

namespace vc {

namespace {

#include "VcBuiltinModels.inc"

struct Slot {
    MobType type;
    const char* id;
    int col, row, rows;  // atlas tiles: 8 columns wide (128 pixels), `rows` high
};
constexpr Slot kSlots[] = {{MobType::Cow, "cow", 0, 0, 4},     {MobType::Sheep, "sheep", 8, 0, 4}, {MobType::Ghost, "ghost", 16, 0, 4},
                           {MobType::Pig, "pig", 24, 0, 4},    {MobType::Chicken, "chicken", 0, 4, 2}};

struct Loaded {
    VcModel model;
    std::vector<uint8_t> rgba;  // the texture, w x h
    int w = 0, h = 0;
    bool ok = false;
};

const Loaded& loaded(size_t slot) {
    static Loaded all[sizeof(kSlots) / sizeof(kSlots[0])];
    static bool done = false;
    if (!done) {
        done = true;
        for (size_t i = 0; i < sizeof(kSlots) / sizeof(kSlots[0]); ++i)
            for (const BuiltinGlb& g : kBuiltinGlbs) {
                if (std::string(g.id) != kSlots[i].id) continue;
                std::vector<uint8_t> png;
                std::string err;
                Loaded& l = all[i];
                if (!parseVcGlb(g.data, g.size, l.model, &png, err) || png.empty()) break;
                int n = 0;
                if (stbi_uc* px = stbi_load_from_memory(png.data(), (int)png.size(), &l.w, &l.h, &n, 4)) {
                    l.rgba.assign(px, px + (size_t)l.w * l.h * 4);
                    stbi_image_free(px);
                    guessVcLegs(l.model);
                    // Only the used rows of the texture have room in the atlas.
                    l.ok = l.w == 128 && vcUsedTextureHeight(l.model) * l.h <= kSlots[i].rows * 16 + 0.5f;
                }
            }
    }
    return all[slot];
}

}  // namespace

const VcModel* builtinMobModel(MobType type, VcTexRect& tex) {
    if (!kFeatures280) return nullptr;
    for (size_t i = 0; i < sizeof(kSlots) / sizeof(kSlots[0]); ++i) {
        if (kSlots[i].type != type) continue;
        const Loaded& l = loaded(i);
        if (!l.ok) return nullptr;
        const float t = 1.0f / kAtlasCols;
        tex.u0 = kSlots[i].col * t, tex.v0 = (kVcBuiltinRow + kSlots[i].row) * t;
        tex.u1 = tex.u0 + 8 * t, tex.v1 = tex.v0 + 8 * t * l.h / 128.0f;
        return &l.model;
    }
    return nullptr;
}

void blitBuiltinModelTextures(std::vector<uint8_t>& rgba, int size) {
    if (!kFeatures280) return;
    const int tile = size / kAtlasCols;
    for (size_t i = 0; i < sizeof(kSlots) / sizeof(kSlots[0]); ++i) {
        const Loaded& l = loaded(i);
        if (!l.ok) continue;
        const int x0 = kSlots[i].col * tile, y0 = (kVcBuiltinRow + kSlots[i].row) * tile, w = 8 * tile, h = kSlots[i].rows * tile;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const int sx = x * 128 / w, sy = y * 16 / tile;
                if (sy >= l.h) continue;
                const uint8_t* s = &l.rgba[((size_t)sy * l.w + sx) * 4];
                std::copy(s, s + 4, &rgba[((size_t)(y0 + y) * size + x0 + x) * 4]);
            }
    }
}

}  // namespace vc

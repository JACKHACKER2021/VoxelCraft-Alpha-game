#include "ItemModel.h"

#include "TexturePack.h"

namespace vc {

namespace {

std::vector<uint8_t> g_atlas;
int g_size = 0;

bool opaque(int tile, int px, int py) {
    if (px < 0 || py < 0 || px > 15 || py > 15) return false;
    const int ts = g_size / kAtlasCols;
    // Sample the middle of the pixel at any atlas resolution (16 px or HD packs).
    int x = (tile % kAtlasCols) * ts + px * ts / 16 + ts / 32, y = (tile / kAtlasCols) * ts + py * ts / 16 + ts / 32;
    return g_atlas[(y * g_size + x) * 4 + 3] > 127;
}

}  // namespace

void setItemModelAtlas(const std::vector<uint8_t>& rgba, int size) {
    g_atlas = rgba;
    g_size = size;
}

void appendItemModel(int tile, float light, float depth, const std::function<Vec3(const Vec3&)>& map,
                     std::vector<Vertex>& out) {
    if (g_atlas.empty()) g_atlas = buildFallbackAtlas(g_size);
    const float tileUv = kTileUv, pixUv = tileUv / 16, h = depth / 2;
    const float u0 = (tile % kAtlasCols) * tileUv, v0 = (tile / kAtlasCols) * tileUv;
    auto quad = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, float ua, float va, float ub, float vb, float shade) {
        Vec3 ma = map(a), mb = map(b), mc = map(c), md = map(d);
        Vertex A{ma.x, ma.y, ma.z, ua, vb, shade * light}, B{mb.x, mb.y, mb.z, ub, vb, shade * light};
        Vertex C{mc.x, mc.y, mc.z, ub, va, shade * light}, D{md.x, md.y, md.z, ua, va, shade * light};
        out.insert(out.end(), {A, B, C, A, C, D});
    };
    // Front and back: the whole sprite, transparent pixels are cut by the alpha test.
    quad({-0.5f, -0.5f, h}, {0.5f, -0.5f, h}, {0.5f, 0.5f, h}, {-0.5f, 0.5f, h}, u0, v0, u0 + tileUv, v0 + tileUv, 1.0f);
    quad({0.5f, -0.5f, -h}, {-0.5f, -0.5f, -h}, {-0.5f, 0.5f, -h}, {0.5f, 0.5f, -h}, u0 + tileUv, v0, u0, v0 + tileUv, 0.7f);
    // Side walls where an opaque pixel meets a transparent one, textured with that pixel.
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            if (!opaque(tile, px, py)) continue;
            const float x0 = px / 16.0f - 0.5f, x1 = x0 + 1 / 16.0f;
            const float y1 = 0.5f - py / 16.0f, y0 = y1 - 1 / 16.0f;  // sprite rows go down
            const float pu = u0 + (px + 0.25f) * pixUv, pv = v0 + (py + 0.25f) * pixUv;
            const float pu1 = pu + pixUv * 0.5f, pv1 = pv + pixUv * 0.5f;
            if (!opaque(tile, px, py - 1)) quad({x0, y1, h}, {x1, y1, h}, {x1, y1, -h}, {x0, y1, -h}, pu, pv, pu1, pv1, 1.0f);
            if (!opaque(tile, px, py + 1)) quad({x0, y0, -h}, {x1, y0, -h}, {x1, y0, h}, {x0, y0, h}, pu, pv, pu1, pv1, 0.5f);
            if (!opaque(tile, px - 1, py)) quad({x0, y0, -h}, {x0, y0, h}, {x0, y1, h}, {x0, y1, -h}, pu, pv, pu1, pv1, 0.75f);
            if (!opaque(tile, px + 1, py)) quad({x1, y0, h}, {x1, y0, -h}, {x1, y1, -h}, {x1, y1, h}, pu, pv, pu1, pv1, 0.75f);
        }
}

}  // namespace vc

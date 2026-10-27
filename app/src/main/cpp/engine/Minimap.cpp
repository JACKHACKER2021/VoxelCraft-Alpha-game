#include "Minimap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

#include "Blocks.h"
#include "Math.h"

namespace vc {

namespace {

std::vector<uint32_t> g_palette;  // 0xRRGGBB per atlas tile
bool g_hasPalette = false;

uint32_t colorOf(ItemId id) {
    if (!g_hasPalette) return 0x808080;
    // Liquids as they look in the world, not their grey source textures.
    if (id == WATER) return 0x3F62C8;
    if (id == LAVA) return 0xE06010;
    const int tile = tileFor(id, F_TOP);
    return tile >= 0 && tile < (int)g_palette.size() ? g_palette[(size_t)tile] : 0x808080;
}

bool isOre(ItemId id) {
    return id == COAL_ORE || id == IRON_ORE || id == BRONZE_ORE || id == STEEL_ORE || id == RUBY_ORE;
}

}  // namespace

void Minimap::setPalette(const std::vector<uint8_t>& atlas, int size) {
    const int ts = size / kAtlasCols;
    g_palette.assign(T_COUNT, 0);
    for (int tile = 0; tile < T_COUNT; ++tile) {
        long r = 0, g = 0, b = 0, n = 0;
        for (int y = 0; y < ts; ++y)
            for (int x = 0; x < ts; ++x) {
                const uint8_t* p = &atlas[(((tile / kAtlasCols) * ts + y) * size + (tile % kAtlasCols) * ts + x) * 4];
                if (p[3] < 128) continue;
                r += p[0];
                g += p[1];
                b += p[2];
                ++n;
            }
        if (n) g_palette[(size_t)tile] = (uint32_t)(r / n) << 16 | (uint32_t)(g / n) << 8 | (uint32_t)(b / n);
    }
    g_hasPalette = true;
}

bool Minimap::update(const World& world, float px, float py, float pz, float dt, bool showOres) {
    timer_ += dt;
    const int cx = (int)std::floor(px) - kSize / 2, cz = (int)std::floor(pz) - kSize / 2;
    const bool moved = std::abs(cx - originX_) >= 2 || std::abs(cz - originZ_) >= 2;
    if (!moved && timer_ < 1.0f && showOres == lastOres_) return false;
    timer_ = 0;
    lastOres_ = showOres;
    originX_ = cx;
    originZ_ = cz;
    rebuild(world, (int)std::floor(py), showOres);
    return true;
}

void Minimap::rebuild(const World& world, int playerY, bool showOres) {
    const bool nether = world.dimension() == Dimension::Nether;
    std::vector<int> heights(kSize * kSize, -1);
    for (int z = 0; z < kSize; ++z)
        for (int x = 0; x < kSize; ++x) {
            const int wx = originX_ + x, wz = originZ_ + z;
            uint8_t* out = &rgba_[(z * kSize + x) * 4];
            const Chunk* c = world.getChunk(floorDiv(wx, kChunkSize), floorDiv(wz, kChunkSize));
            if (!c) {
                out[0] = out[1] = out[2] = 20;
                out[3] = 160;
                continue;
            }
            const int lx = floorMod(wx, kChunkSize), lz = floorMod(wz, kChunkSize);
            // The top visible block: in the Nether below the player (the roof hides everything).
            int y = nether ? std::min(playerY + 1, kChunkHeight - 1)
                           : std::min(kChunkHeight - 1, c->heightmap[lx * kChunkSize + lz] + 24);
            while (y > 0 && (c->get(lx, y, lz) == AIR || (nether && y > playerY + 1))) --y;
            ItemId id = c->get(lx, y, lz);
            int depth = 0;
            if (id == WATER)
                while (y - depth > 0 && c->get(lx, y - depth, lz) == WATER) ++depth;
            heights[z * kSize + x] = y;
            uint32_t col = colorOf(id);
            float r = (col >> 16 & 255), g = (col >> 8 & 255), b = (col & 255);
            if (depth > 1) {
                float f = std::max(0.55f, 1.0f - depth * 0.04f);  // deeper water is darker
                r *= f;
                g *= f;
                b *= f;
            }
            if (showOres) {
                // Experiment: ores within 16 blocks above and below the player shine through.
                for (int oy = std::max(1, playerY - 16); oy <= std::min(kChunkHeight - 1, playerY + 16); ++oy) {
                    ItemId o = c->get(lx, oy, lz);
                    if (!isOre(o)) continue;
                    uint32_t oc = o == COAL_ORE ? 0x202020 : o == IRON_ORE ? 0xD8B090 : o == BRONZE_ORE ? 0xE08030
                                : o == STEEL_ORE ? 0xB8D0F0 : o == RUBY_ORE ? 0xFF2050 : 0xF0F0F0;
                    r = oc >> 16 & 255;
                    g = oc >> 8 & 255;
                    b = oc & 255;
                    break;
                }
            }
            out[0] = (uint8_t)std::clamp(r, 0.0f, 255.0f);
            out[1] = (uint8_t)std::clamp(g, 0.0f, 255.0f);
            out[2] = (uint8_t)std::clamp(b, 0.0f, 255.0f);
            out[3] = 255;
        }
    // Relief: lighter where the ground rises towards the south, darker where it falls.
    for (int z = 1; z < kSize; ++z)
        for (int x = 0; x < kSize; ++x) {
            int h = heights[z * kSize + x], north = heights[(z - 1) * kSize + x];
            if (h < 0 || north < 0) continue;
            float f = h > north ? 1.15f : h < north ? 0.82f : 1.0f;
            uint8_t* out = &rgba_[(z * kSize + x) * 4];
            for (int k = 0; k < 3; ++k) out[k] = (uint8_t)std::min(255.0f, out[k] * f);
        }
}

namespace {
const uint8_t kParchment[3] = {226, 210, 166};
}

PaperMaps::Sheet& PaperMaps::at(int dim, int ax, int az) {
    for (Sheet& s : sheets_)
        if (s.dim == dim && s.ax == ax && s.az == az) return s;
    Sheet s{dim, ax, az, std::vector<uint8_t>((size_t)kSize * kSize * 4), 0};
    for (size_t i = 0; i < s.rgba.size(); i += 4) {
        std::copy(kParchment, kParchment + 3, &s.rgba[i]);
        s.rgba[i + 3] = 255;
    }
    sheets_.push_back(std::move(s));
    return sheets_.back();
}

void PaperMaps::explore(int dim, const Minimap& minimap, float px, float pz, float radius) {
    const std::vector<uint8_t>& src = minimap.rgba();
    for (int z = 0; z < Minimap::kSize; ++z)
        for (int x = 0; x < Minimap::kSize; ++x) {
            const int wx = minimap.originX() + x, wz = minimap.originZ() + z;
            const float dx = wx + 0.5f - px, dz = wz + 0.5f - pz;
            if (dx * dx + dz * dz > radius * radius) continue;
            const uint8_t* c = &src[((size_t)z * Minimap::kSize + x) * 4];
            if (c[3] < 255) continue;  // not loaded
            Sheet& s = at(dim, floorDiv(wx, kSize), floorDiv(wz, kSize));
            uint8_t* o = &s.rgba[((size_t)floorMod(wz, kSize) * kSize + floorMod(wx, kSize)) * 4];
            if (o[3] == 255 && o[0] == kParchment[0] && o[1] == kParchment[1] && o[2] == kParchment[2]) ++s.drawn;
            // Inked on paper: the colours lean a little towards the parchment.
            for (int k = 0; k < 3; ++k) o[k] = (uint8_t)((c[k] * 3 + kParchment[k]) / 4);
            o[3] = 255;
        }
}

const PaperMaps::Sheet* PaperMaps::find(int dim, int ax, int az) const {
    for (const Sheet& s : sheets_)
        if (s.dim == dim && s.ax == ax && s.az == az) return &s;
    return nullptr;
}

void PaperMaps::view(int dim, int x, int z, std::vector<uint8_t>& rgba, int& originX, int& originZ) const {
    originX = x - kSize / 2;
    originZ = z - kSize / 2;
    rgba.assign((size_t)kSize * kSize * 4, 255);
    for (int vz = 0; vz < kSize; ++vz)
        for (int vx = 0; vx < kSize; ++vx) {
            const int wx = originX + vx, wz = originZ + vz;
            const Sheet* s = find(dim, floorDiv(wx, kSize), floorDiv(wz, kSize));
            uint8_t* o = &rgba[((size_t)vz * kSize + vx) * 4];
            if (!s) {
                std::copy(kParchment, kParchment + 3, o);
                continue;
            }
            const uint8_t* c = &s->rgba[((size_t)floorMod(wz, kSize) * kSize + floorMod(wx, kSize)) * 4];
            std::copy(c, c + 3, o);
        }
}

void PaperMaps::render(int dim, float cx, float cz, float scale, int size, std::vector<uint8_t>& rgba) const {
    rgba.assign((size_t)size * size * 4, 255);
    const Sheet* last = nullptr;
    int lastAx = 1 << 30, lastAz = 1 << 30;
    for (int vz = 0; vz < size; ++vz)
        for (int vx = 0; vx < size; ++vx) {
            const int wx = (int)std::floor(cx + (vx - size / 2 + 0.5f) * scale), wz = (int)std::floor(cz + (vz - size / 2 + 0.5f) * scale);
            const int ax = floorDiv(wx, kSize), az = floorDiv(wz, kSize);
            if (ax != lastAx || az != lastAz) last = find(dim, ax, az), lastAx = ax, lastAz = az;
            uint8_t* o = &rgba[((size_t)vz * size + vx) * 4];
            const uint8_t* c = last ? &last->rgba[((size_t)floorMod(wz, kSize) * kSize + floorMod(wx, kSize)) * 4] : nullptr;
            if (!c || (c[0] == kParchment[0] && c[1] == kParchment[1] && c[2] == kParchment[2]))
                std::copy(kParchment, kParchment + 3, o);
            else
                std::copy(c, c + 3, o);
        }
}

int PaperMaps::explored(int dim, int x, int z) const {
    const Sheet* s = find(dim, floorDiv(x, kSize), floorDiv(z, kSize));
    return s ? s->drawn : 0;
}

void PaperMaps::save(const std::string& folder) const {
    std::ofstream out(folder + "/maps.dat", std::ios::binary);
    out << "VCMAP1 " << sheets_.size() << "\n";
    for (const Sheet& s : sheets_) {
        out << s.dim << " " << s.ax << " " << s.az << " " << s.drawn << "\n";
        for (size_t i = 0; i < s.rgba.size(); i += 4) out.write((const char*)&s.rgba[i], 3);
    }
}

void PaperMaps::load(const std::string& folder) {
    sheets_.clear();
    std::ifstream in(folder + "/maps.dat", std::ios::binary);
    std::string magic;
    size_t n = 0;
    if (!(in >> magic >> n) || magic != "VCMAP1" || n > 4096) return;
    for (size_t k = 0; k < n; ++k) {
        Sheet s{0, 0, 0, std::vector<uint8_t>((size_t)kSize * kSize * 4, 255), 0};
        if (!(in >> s.dim >> s.ax >> s.az >> s.drawn)) return;
        in.get();
        for (size_t i = 0; i < s.rgba.size(); i += 4)
            if (!in.read((char*)&s.rgba[i], 3)) return;
        sheets_.push_back(std::move(s));
    }
}

}  // namespace vc

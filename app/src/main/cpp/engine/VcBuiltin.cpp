#include "VcBuiltin.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

#include "Lang.h"
#include "third_party/stb/stb_image.h"  // implementation lives in TexturePack.cpp

namespace vc {

bool gVcLegacyMobs = false;

namespace {

constexpr const char* kIds[(int)MobType::Count] = {"pig", "sheep", "pumpkin_slime", "forgotten_man", "crimson_man", "chicken", "cow", "villager", "spider",
                                                  "bat", "tnt_zombie", "tar_monster", "husking", "hogman", "celestling", "reach_stalker", "sky_drifter",
                                                  "sky_guardian", "cornerstone_golem", "moss_lurker", "sporeling", "ember_moth", "ash_fox", "fox", "ghost", "snowman"};

std::vector<std::string> gDirs;
VcModel gModels[(int)MobType::Count][2];
bool gHave[(int)MobType::Count][2] = {};
struct Blit { int x, y, w, h; std::vector<uint8_t> rgba; };  // in 16-pixel atlas pixels
std::vector<Blit> gBlits;

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return !out.empty();
}

// Room for new texture pieces: rows kVcBuiltinRow .. + kVcBuiltinRows, filled shelf by shelf.
struct Shelf {
    int x = 0, y = 0, h = 0;
    bool take(int w, int hh, int& ox, int& oy) {
        if (w > kAtlasCols) return false;
        if (x + w > kAtlasCols) x = 0, y += h, h = 0;
        if (y + hh > kVcBuiltinRows) return false;
        ox = x, oy = kVcBuiltinRow + y, x += w, h = std::max(h, hh);
        return true;
    }
};

}  // namespace

const char* vcMobModelId(MobType type) { return (int)type >= 0 && (int)type < (int)MobType::Count ? kIds[(int)type] : ""; }

void setVcModelDirs(const std::vector<std::string>& dirs) { gDirs = dirs; }

int loadVcMobModels(const std::vector<uint8_t>& atlas, int size, std::vector<std::string>* warnings) {
    gBlits.clear();
    for (auto& h : gHave) h[0] = h[1] = false;
    if (!kFeatures280) return 0;
    const int t = size / kAtlasCols;  // 16 for the built-in atlas
    // Every tile of the atlas by its pixels, to share the tiles a model already finds there.
    std::map<std::string, int> tileByPixels;
    for (int tile = kAtlasCols * kAtlasCols - 1; tile >= 0; --tile) {
        std::string px((size_t)t * t * 4, '\0');
        for (int y = 0; y < t; ++y)
            std::memcpy(&px[(size_t)y * t * 4], &atlas[((size_t)((tile / kAtlasCols) * t + y) * size + (tile % kAtlasCols) * t) * 4], (size_t)t * 4);
        if (px.find_first_not_of('\0') != std::string::npos) tileByPixels[px] = tile;
    }
    Shelf shelf;
    int count = 0;
    for (int type = 0; type < (int)MobType::Count; ++type)
        for (int sit = 0; sit < 2; ++sit) {
            const std::string file = std::string("models/mobs/") + kIds[type] + (sit ? "_sit" : "") + ".glb";
            std::vector<uint8_t> data;
            std::string from;
            for (const std::string& d : gDirs)
                if (readFile(d + "/" + file, data)) { from = d + "/" + file; break; }
            if (data.empty()) continue;
            VcModel m;
            std::vector<uint8_t> png;
            std::string err;
            int w = 0, h = 0, n = 0;
            stbi_uc* px = nullptr;
            if (!parseVcGlb(data.data(), data.size(), m, &png, err) || png.empty() ||
                !(px = stbi_load_from_memory(png.data(), (int)png.size(), &w, &h, &n, 4))) {
                if (warnings) warnings->push_back(trf("Model {0} nejde načíst: {1}", {from, err.empty() ? "PNG" : err}));
                continue;
            }
            std::vector<uint8_t> tex(px, px + (size_t)w * h * 4);
            stbi_image_free(px);
            bool animated = false;
            for (const VcPart& p : m.parts) animated |= p.swing != 0 || p.flap != 0;
            if (!animated && !m.animated) guessVcLegs(m);
            // Does every triangle stay inside one 16-pixel tile of its texture? Then tile by tile (sharing
            // the atlas's own tiles), else the used part of the texture as one block.
            bool tiled = w % 16 == 0 && h % 16 == 0;
            for (const VcPart& p : m.parts)
                for (size_t i = 0; tiled && i + 15 <= p.tris.size(); i += 15) {
                    const float cu = (p.tris[i + 3] + p.tris[i + 8] + p.tris[i + 13]) / 3 * w / 16, cv = (p.tris[i + 4] + p.tris[i + 9] + p.tris[i + 14]) / 3 * h / 16;
                    for (int k = 0; k < 3; ++k) {
                        const float u = p.tris[i + k * 5 + 3] * w / 16, v = p.tris[i + k * 5 + 4] * h / 16;
                        if (u < std::floor(cu) - 1e-3f || u > std::floor(cu) + 1 + 1e-3f || v < std::floor(cv) - 1e-3f || v > std::floor(cv) + 1 + 1e-3f) tiled = false;
                    }
                }
            bool ok = true;
            if (tiled) {
                std::map<int, int> place;  // source tile -> atlas tile
                for (VcPart& p : m.parts)
                    for (size_t i = 0; ok && i + 15 <= p.tris.size(); i += 15) {
                        const int su = (int)((p.tris[i + 3] + p.tris[i + 8] + p.tris[i + 13]) / 3 * w / 16), sv = (int)((p.tris[i + 4] + p.tris[i + 9] + p.tris[i + 14]) / 3 * h / 16);
                        const int src = sv * (w / 16) + su;
                        if (!place.count(src)) {
                            std::string pxs(16 * 16 * 4, '\0');
                            for (int y = 0; y < 16; ++y) std::memcpy(&pxs[(size_t)y * 64], &tex[((size_t)(sv * 16 + y) * w + su * 16) * 4], 64);
                            auto it = t == 16 ? tileByPixels.find(pxs) : tileByPixels.end();
                            if (it != tileByPixels.end()) {
                                place[src] = it->second;
                            } else {
                                int ox, oy;
                                if (!shelf.take(1, 1, ox, oy)) { ok = false; break; }
                                gBlits.push_back({ox * 16, oy * 16, 16, 16, std::vector<uint8_t>(pxs.begin(), pxs.end())});
                                place[src] = oy * kAtlasCols + ox;
                                tileByPixels[pxs] = place[src];
                            }
                        }
                        const int at = place[src];
                        for (int k = 0; k < 3; ++k) {
                            float& u = p.tris[i + k * 5 + 3];
                            float& v = p.tris[i + k * 5 + 4];
                            u = ((at % kAtlasCols) + (u * w / 16 - su)) / kAtlasCols;
                            v = ((at / kAtlasCols) + (v * h / 16 - sv)) / kAtlasCols;
                        }
                    }
            } else {
                const int used = std::max(1, (int)std::ceil(vcUsedTextureHeight(m) * h / 16.0f - 1e-3f)), tw = (w + 15) / 16;
                int ox, oy;
                ok = shelf.take(tw, used, ox, oy);
                if (ok) {
                    Blit b{ox * 16, oy * 16, tw * 16, used * 16, std::vector<uint8_t>((size_t)tw * 16 * used * 16 * 4, 0)};
                    for (int y = 0; y < std::min(h, used * 16); ++y) std::memcpy(&b.rgba[(size_t)y * tw * 16 * 4], &tex[(size_t)y * w * 4], (size_t)w * 4);
                    gBlits.push_back(std::move(b));
                    for (VcPart& p : m.parts)
                        for (size_t i = 0; i + 5 <= p.tris.size(); i += 5)
                            p.tris[i + 3] = (ox + p.tris[i + 3] * w / 16) / kAtlasCols, p.tris[i + 4] = (oy + p.tris[i + 4] * h / 16) / kAtlasCols;
                }
            }
            if (!ok) {
                if (warnings) warnings->push_back(trf("Textura modelu {0} chybí nebo se nevejde", {from}));
                continue;
            }
            m.texW = m.texH = 1;
            gModels[type][sit] = std::move(m);
            gHave[type][sit] = true;
            ++count;
        }
    return count;
}

void blitVcMobTextures(std::vector<uint8_t>& rgba, int size) {
    const int t = size / kAtlasCols;
    for (const Blit& b : gBlits)
        for (int y = 0; y < b.h * t / 16; ++y)
            for (int x = 0; x < b.w * t / 16; ++x) {
                const uint8_t* s = &b.rgba[((size_t)(y * 16 / t) * b.w + x * 16 / t) * 4];
                std::copy(s, s + 4, &rgba[((size_t)(b.y * t / 16 + y) * size + b.x * t / 16 + x) * 4]);
            }
}

const VcModel* builtinMobModel(MobType type, bool sitting) {
    if (!kFeatures280 || gVcLegacyMobs || (int)type < 0 || (int)type >= (int)MobType::Count) return nullptr;
    if (sitting && gHave[(int)type][1]) return &gModels[(int)type][1];
    return gHave[(int)type][0] ? &gModels[(int)type][0] : nullptr;
}

}  // namespace vc

// 2.8.0: exports the models the engine builds today (every creature, standing and sitting, and the player
// in every built-in skin) as GLB files: the reference ("the standard") for rebuilding them in the VC language.
// Every box of a model is its own node (cube_0, cube_1, ...) so editors such as Blockbench show the parts;
// the texture (the terrain atlas for creatures, the 64 x 64 skin for the player) is packed into the file.
// Build: g++ -std=c++17 -Iapp/src/main/cpp tools/export_models.cpp <engine objects> -lz -o export_models
// Run:   ./export_models <out dir>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "engine/Mesher.h"
#include "engine/Mobs.h"
#include "engine/PlayerModel.h"
#include "engine/Png.h"
#include "engine/Skin.h"
#include "engine/TexturePack.h"

using namespace vc;
namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> pngBytes(int w, int h, const std::vector<uint8_t>& rgba) {
    const std::string tmp = (fs::temp_directory_path() / "vc_export_tex.png").string();
    writePng(tmp, w, h, rgba);
    std::ifstream in(tmp, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

// A GLB with one node per box (36 vertices), positions in blocks (16 model pixels = 1), y up.
void writeGlb(const fs::path& file, const std::string& name, const std::vector<Vertex>& verts, const std::vector<uint8_t>& png, float scale) {
    std::vector<uint8_t> bin;
    auto pad4 = [&](std::vector<uint8_t>& b, uint8_t fill) { while (b.size() % 4) b.push_back(fill); };
    struct View { size_t offset, length; };
    std::vector<View> views;
    std::ostringstream nodes, meshes, accessors;
    const size_t perBox = 36, boxes = (verts.size() + perBox - 1) / perBox;
    for (size_t b = 0; b < boxes; ++b) {
        const size_t first = b * perBox, count = std::min(perBox, verts.size() - first);
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        const size_t posOff = bin.size();
        for (size_t i = 0; i < count; ++i) {
            const Vertex& v = verts[first + i];
            const float p[3] = {v.x * scale, v.y * scale, v.z * scale};
            for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], p[k]), hi[k] = std::max(hi[k], p[k]);
            const uint8_t* q = reinterpret_cast<const uint8_t*>(p);
            bin.insert(bin.end(), q, q + 12);
        }
        views.push_back({posOff, count * 12});
        const size_t uvOff = bin.size();
        for (size_t i = 0; i < count; ++i) {
            const float t[2] = {verts[first + i].u, verts[first + i].v};
            const uint8_t* q = reinterpret_cast<const uint8_t*>(t);
            bin.insert(bin.end(), q, q + 8);
        }
        views.push_back({uvOff, count * 8});
        const size_t a = b * 2;
        accessors << (b ? "," : "") << "{\"bufferView\":" << a << ",\"componentType\":5126,\"count\":" << count << ",\"type\":\"VEC3\",\"min\":[" << lo[0] << ","
                  << lo[1] << "," << lo[2] << "],\"max\":[" << hi[0] << "," << hi[1] << "," << hi[2] << "]},{\"bufferView\":" << a + 1
                  << ",\"componentType\":5126,\"count\":" << count << ",\"type\":\"VEC2\"}";
        meshes << (b ? "," : "") << "{\"name\":\"cube_" << b << "\",\"primitives\":[{\"attributes\":{\"POSITION\":" << a << ",\"TEXCOORD_0\":" << a + 1
               << "},\"material\":0}]}";
        nodes << ",{\"name\":\"cube_" << b << "\",\"mesh\":" << b << "}";
    }
    pad4(bin, 0);
    const size_t imgOff = bin.size();
    bin.insert(bin.end(), png.begin(), png.end());
    views.push_back({imgOff, png.size()});
    pad4(bin, 0);
    std::ostringstream j;
    j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"VoxelCraft export_models (2.8.0)\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"name\":\"" << name
      << "\",\"children\":[";
    for (size_t b = 0; b < boxes; ++b) j << (b ? "," : "") << b + 1;
    j << "]}" << nodes.str() << "],\"meshes\":[" << meshes.str() << "],\"accessors\":[" << accessors.str() << "],\"bufferViews\":[";
    for (size_t i = 0; i < views.size(); ++i) j << (i ? "," : "") << "{\"buffer\":0,\"byteOffset\":" << views[i].offset << ",\"byteLength\":" << views[i].length << "}";
    j << "],\"buffers\":[{\"byteLength\":" << bin.size() << "}],\"images\":[{\"bufferView\":" << views.size() - 1
      << ",\"mimeType\":\"image/png\"}],\"samplers\":[{\"magFilter\":9728,\"minFilter\":9728}],\"textures\":[{\"source\":0,\"sampler\":0}],"
      << "\"materials\":[{\"name\":\"" << name << "\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"metallicFactor\":0},\"alphaMode\":\"MASK\",\"doubleSided\":true}]}";
    std::string json = j.str();
    while (json.size() % 4) json += ' ';
    std::ofstream out(file, std::ios::binary);
    auto u32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    out.write("glTF", 4), u32(2), u32((uint32_t)(12 + 8 + json.size() + 8 + bin.size()));
    u32((uint32_t)json.size()), out.write("JSON", 4), out.write(json.data(), (std::streamsize)json.size());
    u32((uint32_t)bin.size()), out.write("BIN\0", 4), out.write(reinterpret_cast<const char*>(bin.data()), (std::streamsize)bin.size());
}

std::string fileName(const std::string& s) {
    std::string o;
    for (unsigned char c : s) o += std::isalnum(c) ? (char)std::tolower(c) : '_';
    return o;
}

// Only the atlas tiles a model uses, packed in a small square sheet; the UVs are moved onto it.
std::vector<uint8_t> compactTexture(std::vector<Vertex>& v, const std::vector<uint8_t>& atlas, int size) {
    const int t = size / kAtlasCols;
    std::vector<int> tiles, tileOf(v.size() / 3);
    for (size_t tri = 0; tri < v.size() / 3; ++tri) {
        const float u = (v[tri * 3].u + v[tri * 3 + 1].u + v[tri * 3 + 2].u) / 3, w = (v[tri * 3].v + v[tri * 3 + 1].v + v[tri * 3 + 2].v) / 3;
        const int tile = (int)(w * kAtlasCols) * kAtlasCols + (int)(u * kAtlasCols);
        auto it = std::find(tiles.begin(), tiles.end(), tile);
        tileOf[tri] = (int)(it - tiles.begin());
        if (it == tiles.end()) tiles.push_back(tile);
    }
    int cols = 1;
    while (cols * cols < (int)tiles.size()) ++cols;
    const int sheet = cols * t;
    std::vector<uint8_t> rgba((size_t)sheet * sheet * 4, 0);
    for (size_t i = 0; i < tiles.size(); ++i) {
        const int sx = (tiles[i] % kAtlasCols) * t, sy = (tiles[i] / kAtlasCols) * t, dx = (int)(i % cols) * t, dy = (int)(i / cols) * t;
        for (int y = 0; y < t; ++y) std::memcpy(&rgba[((size_t)(dy + y) * sheet + dx) * 4], &atlas[((size_t)(sy + y) * size + sx) * 4], (size_t)t * 4);
    }
    for (size_t k = 0; k < v.size(); ++k) {
        const int i = tileOf[k / 3], tile = tiles[(size_t)i];
        const float lu = v[k].u * kAtlasCols - (tile % kAtlasCols), lv = v[k].v * kAtlasCols - (tile / kAtlasCols);
        v[k].u = ((i % cols) + lu) / cols, v[k].v = ((i / cols) + lv) / cols;
    }
    return pngBytes(sheet, sheet, rgba);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    const fs::path out = argv[1];
    fs::create_directories(out / "mobs"), fs::create_directories(out / "player");
    int size = 0;
    const std::vector<uint8_t> atlas = buildFallbackAtlas(size);
    static const char* const kIds[(int)MobType::Count] = {"pig", "sheep", "pumpkin_slime", "forgotten_man", "crimson_man", "chicken", "cow", "villager", "spider",
                                                          "bat", "tnt_zombie", "tar_monster", "husking", "hogman", "celestling", "reach_stalker", "sky_drifter",
                                                          "sky_guardian", "cornerstone_golem", "moss_lurker", "sporeling", "ember_moth", "ash_fox", "fox", "ghost", "snowman"};
    int files = 0;
    for (int t = 0; t < (int)MobType::Count; ++t)
        for (int sit = 0; sit < 2; ++sit) {
            MobSystem ms;
            ms.spawn((MobType)t, 0, Vec3(0, 0, 0));
            if (ms.mobs().empty()) continue;
            Mob& m = ms.mobs().back();
            m.yaw = 0, m.pos = Vec3(0, 0, 0);
            if (sit) m.sitting = true, m.sit = 1;
            std::vector<Vertex> v;
            ms.buildVertices(0, 0, v);
            if (v.empty()) continue;
            if (sit) {
                std::vector<Vertex> stand;
                MobSystem ms2;
                ms2.spawn((MobType)t, 0, Vec3(0, 0, 0));
                ms2.mobs().back().yaw = 0;
                ms2.buildVertices(0, 0, stand);
                bool same = stand.size() == v.size();
                for (size_t i = 0; same && i < v.size(); ++i) same = std::abs(stand[i].y - v[i].y) < 1e-4f;
                if (same) continue;  // this creature does not sit
            }
            const std::vector<uint8_t> tex = compactTexture(v, atlas, size);
            writeGlb(out / "mobs" / (std::string(kIds[t]) + (sit ? "_sit" : "") + ".glb"), kIds[t], v, tex, 1.0f);
            ++files;
        }
    for (const Skin& s : builtinSkins()) {
        PlayerPose pose;
        pose.slim = s.slim;
        const std::vector<Vertex> v = buildPlayerModel(pose);
        writeGlb(out / "player" / (fileName(s.id.empty() ? s.name : s.id) + ".glb"), s.name, v, pngBytes(Skin::kSize, Skin::kSize, s.rgba), 1.0f);
        ++files;
    }
    std::printf("%d models\n", files);
    return 0;
}

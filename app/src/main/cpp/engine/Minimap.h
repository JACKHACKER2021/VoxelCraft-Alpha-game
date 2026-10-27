#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "World.h"

namespace vc {

// A place shown on the minimap.
struct MapMarker {
    enum Kind { Spawn, Death, Structure } kind;
    int x, z;
};

// Top-down map of the blocks around the player (north up), coloured from the
// textures in use and shaded by height like classic paper maps. It is rebuilt
// when the player has moved or every second, from loaded chunks only.
class Minimap {
public:
    static constexpr int kSize = 64;  // blocks across = pixels across

    // Average colour of each block's top texture (call when the atlas changes).
    static void setPalette(const std::vector<uint8_t>& atlas, int size);

    // Returns true when the image was rebuilt (upload it again).
    bool update(const World& world, float px, float py, float pz, float dt, bool showOres);
    const std::vector<uint8_t>& rgba() const { return rgba_; }
    int originX() const { return originX_; }  // world x of pixel column 0
    int originZ() const { return originZ_; }
    void invalidate() { timer_ = 1e9f; }

private:
    void rebuild(const World& world, int py, bool showOres);
    std::vector<uint8_t> rgba_ = std::vector<uint8_t>(kSize * kSize * 4, 0);
    int originX_ = 0, originZ_ = 0;
    float timer_ = 1e9f;
    bool lastOres_ = false;
};

// The map item (1.4.0): paper sheets of 128 x 128 blocks that fill in where the player
// walks while carrying a map. Unexplored parts stay blank parchment.
class PaperMaps {
public:
    static constexpr int kSize = 128;
    // Copies what the minimap shows within `radius` blocks of the player onto the sheets.
    void explore(int dim, const Minimap& minimap, float px, float pz, float radius);
    // A kSize x kSize view centred on (x, z), put together from the sheets around it.
    void view(int dim, int x, int z, std::vector<uint8_t>& rgba, int& originX, int& originZ) const;
    int explored(int dim, int x, int z) const;  // pixels drawn on that sheet
    // 1.5.7, the world map: a size x size picture centred on (cx, cz) with `scale` blocks per
    // pixel (1 = close up, 8 = far out); unexplored land is parchment.
    void render(int dim, float cx, float cz, float scale, int size, std::vector<uint8_t>& rgba) const;
    void save(const std::string& folder) const;
    void load(const std::string& folder);

private:
    struct Sheet {
        int dim, ax, az;
        std::vector<uint8_t> rgba;
        int drawn = 0;
    };
    Sheet& at(int dim, int ax, int az);
    const Sheet* find(int dim, int ax, int az) const;
    std::vector<Sheet> sheets_;
};

}  // namespace vc

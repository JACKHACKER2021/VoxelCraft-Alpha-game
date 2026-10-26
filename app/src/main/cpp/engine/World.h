#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Blocks.h"
#include "GenConfig.h"
#include "Noise.h"

namespace vc {
struct Structure;
}

namespace vc {

// Same dimensions as the original Pocket Edition chunks.
constexpr int kChunkSize = 16;
constexpr int kChunkHeight = 128;
constexpr int kCaveLakeLevel = 11;  // cavern floors up to this height hold water or tar
constexpr int kSeaLevel = 62;
constexpr int kNetherLavaLevel = 31;

// The Crimson Void (Void) is VoxelCraft's own dimension, reached through a void portal in the Nether.
enum class Dimension { Overworld, Nether, Void };

// One changed block, as sent between LAN players.
struct BlockEdit {
    int x, y, z;
    ItemId id;
};

struct Chunk {
    int cx, cz;
    ItemId blocks[kChunkSize * kChunkSize * kChunkHeight] = {};
    uint8_t heightmap[kChunkSize * kChunkSize] = {};  // first y above the highest opaque block
    bool dirty = true;                                // mesh needs rebuilding
    bool modified = false;                            // differs from the generator, must be saved
    mutable int8_t lights = -1;                       // holds glowing blocks? -1: not known yet (Mesher)

    Chunk(int cx, int cz) : cx(cx), cz(cz) {}

    // PE layout: x-major, then z, then y.
    static int index(int x, int y, int z) { return (x << 11) | (z << 7) | y; }
    ItemId get(int x, int y, int z) const { return blocks[index(x, y, z)]; }
    void set(int x, int y, int z, ItemId id) {
        blocks[index(x, y, z)] = id;
        lights = -1;
    }
    void recalcHeight(int x, int z);
    void recalcHeightmap();
};

class World {
public:
    World(uint32_t seed, Dimension dim, const GenConfig& config = GenConfig::defaults());
    Dimension dimension() const { return dim_; }
    const GenConfig& config() const { return config_; }
    int seaLevel() const { return config_.overworld.seaLevel; }
    int lavaLevel() const { return config_.nether.lavaLevel; }
    // Biome at a column (overworld only, nullptr elsewhere).
    const BiomeDef* biomeAt(int x, int z) const;

    ItemId getBlock(int x, int y, int z) const;
    // Returns false if the target chunk is not loaded.
    bool setBlock(int x, int y, int z, ItemId id);
    // Like setBlock, but loads (or generates) the chunk first.
    bool setBlockLoading(int x, int y, int z, ItemId id);
    // When set, every setBlock that changes a block is appended here (LAN games).
    std::vector<BlockEdit>* editLog = nullptr;
    // Highest y that is still lit by the sky in this column.
    int skyHeight(int x, int z) const;

    Chunk* getChunk(int cx, int cz) const;
    Chunk* generateChunk(int cx, int cz);
    // Generates at most `budget` missing chunks around the given chunk, nearest first.
    int ensureChunksAround(int cx, int cz, int radius, int budget);
    void unloadFarChunks(int cx, int cz, int radius);
    const std::unordered_map<int64_t, std::unique_ptr<Chunk>>& chunks() const { return chunks_; }

    int surfaceHeight(int x, int z) const;  // overworld generator height (top block), for spawning

    // 1.1.0: the Nether's grassy meadows and the three lands of the Crimson Void.
    enum class VoidBiome { Purple, Crimson, Lava };
    VoidBiome voidBiomeAt(int x, int z) const;
    int voidHeight(int x, int z) const;
    bool netherGrassAt(int x, int z) const;
    // 1.5.1: the Powerstone Wasteland of the Cornerstone Dimension, and its forgotten villages
    // (the lamp column on the plaza; cy is the feet height there).
    bool wastelandAt(int x, int z) const;
    bool forgottenVillageIn(int rx, int rz, int& cx, int& cy, int& cz) const;
    static constexpr int kForgottenRegion = 192;
    // Name of the place for the HUD and /pos (any dimension), a Czech key of LangTable.inc.
    std::string placeName(int x, int z) const;
    // The nearest fortress (Nether) or boss arena (Crimson Void) to (x, z): its centre and floor height.
    bool nearestStructure(int x, int z, int& outX, int& outY, int& outZ) const;
    static constexpr int kFortressRegion = 176, kArenaRegion = 640, kVoidLavaLevel = 57, kVillageRegion = 384;
    // The village of an overworld region, if it has one: the well in the middle (feet height).
    bool villageIn(int rx, int rz, int& cx, int& cy, int& cz) const;

    // Persistence: modified chunks are stored as files in this directory and
    // loaded instead of generated. Empty = nothing is saved (menu background).
    void setStorageDir(const std::string& dir) { storageDir_ = dir; }
    void saveModifiedChunks();
    uint32_t seed() const { return seed_; }

private:
    static int64_t key(int cx, int cz) { return (int64_t)(((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz); }
    void markDirty(int cx, int cz);
    void generateOverworld(Chunk& c);
    void generateNether(Chunk& c);
    void generateVoid(Chunk& c);
    struct Column {
        int height = 0;
        const BiomeDef* biome = nullptr;
        float temperature = 0.5f;  // climate at this column (cold water freezes)
    };
    Column column(int x, int z) const;
    // Generated structures (ruins, shipwrecks, dungeons, mineshafts) overlapping this chunk.
    void stampStructures(Chunk& c);
    void stampBlueprint(Chunk& c, const Structure& s, int ox, int oy, int oz, bool pillars);
    const Structure& blueprint(int kind, int rx, int rz, uint32_t h);
    bool fortressIn(int rx, int rz, int& ox, int& oy, int& oz, uint32_t& h) const;
    void arenaCentre(int rx, int rz, int& x, int& z) const;
    float arenaInfluence(int x, int z) const;  // 1 at an arena, 0 far away
    void placePine(Chunk& c, int x, int y, int z, uint32_t h, ItemId trunkBlock, ItemId leafBlock);
    void placeTree(Chunk& c, int x, int y, int z, uint32_t h, ItemId trunk, ItemId leaves);
    void placeOres(Chunk& c, const OreDef& ore, uint32_t salt);
    std::string chunkPath(int cx, int cz) const;
    bool loadChunk(Chunk& c) const;
    void saveChunk(Chunk& c) const;

    uint32_t seed_;
    Dimension dim_;
    GenConfig config_;
    Noise terrain_, detail_, caves_, temperature_, humidity_;
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks_;
    std::unordered_map<uint64_t, std::shared_ptr<Structure>> blueprints_;  // generated structures, by region
    std::string storageDir_;
};

}  // namespace vc

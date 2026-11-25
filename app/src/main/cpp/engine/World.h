#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <array>
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
constexpr int kChunkHeight = 192;
// 2.2.0: the generators still build worlds 128 blocks high; the Overworld of the preview is then lifted by
// kDeepShift blocks, and the room below it is filled with the deep layers (deepstone, abyssal rock).
constexpr int kGenHeight = 128;
constexpr int kDeepShift = 64;
constexpr int kCaveLakeLevel = 11;  // cavern floors up to this height hold water or tar
constexpr int kSeaLevel = 62;
constexpr int kNetherLavaLevel = 31;

// The Crimson Void (Void) is VoxelCraft's own dimension, reached through a void portal in the Nether.
enum class Dimension { Overworld, Nether, Void, Celestial };  // 1.7.0: the Celestial Reach

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
    static int index(int x, int y, int z) { return ((x << 4) | z) * kChunkHeight + y; }
    ItemId get(int x, int y, int z) const { return blocks[index(x, y, z)]; }
    void set(int x, int y, int z, ItemId id) {
        blocks[index(x, y, z)] = id;
        lights = -1;
    }
    void recalcHeight(int x, int z);
    void recalcHeightmap();
};

// 1.9.5 build 2: seed presets: a seed whose spawn lies in a taiga, by a village, on ice...
constexpr int kSeedPresets = 8;
const char* seedPresetName(int preset);  // Czech, for tr()
// Finds the nearest place of the seed's world that fits the preset (a taiga, a village...), searched
// outwards from the middle; false when there is none within a few thousand blocks.
bool findPresetSpot(int preset, const GenConfig& config, uint32_t seed, int& outX, int& outZ);

// 2.5.0 preview build 4: structure seeds: a fixed seed for each structure, with its own generation
// pinned and the spawn right by the structure. tests/core_test.cpp checks every entry, so an update
// that would move one of them fails the tests instead of quietly changing the world.
struct StructureSeed {
    const char* name;  // Czech, for tr()
    const char* pic;   // a picture of EncyclopediaPictures.inc
    const char* seed;  // the seed text, as typed into the menu
    int generation;    // the generator generation it was found with (kept, whatever is newest later)
    int type;          // WorldStructure, or 100 = Bronze Halls, 101 = lost city, 102 = subway station
    int x, z;          // the structure's middle (for the subway: its entrance shaft)
};
const StructureSeed* structureSeeds(int& count);  // only the ones this edition has

// 2.5.0 preview build 5: developer-only test maps (flat worlds laid out from fixed coordinates).
constexpr int kDevMaps = 6;
const char* devMapName(int map);              // Czech, for tr(); 0 = none
int devMapSpawn(int map, int& x, int& z);     // the spawn of the map; returns the ground's height
// The developer structures (they stand in the test world of structures and the metro lab).
Structure devHeadquarters();
Structure devHoppyTower();
Structure devVersionArchive();
Structure devTestTrack();
Structure devGlitch();

class World {
public:
    World(uint32_t seed, Dimension dim, const GenConfig& config = GenConfig::defaults());
    Dimension dimension() const { return dim_; }
    const GenConfig& config() const { return config_; }
    int seaLevel() const { return config_.overworld.seaLevel + yShift(); }
    // 2.2.0: how far the Overworld is lifted above the generator's heights (0 elsewhere and in the main edition).
    int yShift() const { return deep_ ? kDeepShift : 0; }
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
    // 1.5.7: when set, the position of every changed block is appended here (falling blocks, flowing liquids).
    std::vector<std::array<int, 3>>* changeLog = nullptr;
    // 1.9.0 build 3: how far each flowing liquid cell is from its source (Session's map), so the
    // mesher can lower and slope the surface; 0 = a source or a cell not in the map.
    const std::unordered_map<uint64_t, uint8_t>* flowLevels = nullptr;
    int liquidLevel(int x, int y, int z) const {
        if (!flowLevels) return 0;
        auto it = flowLevels->find(liquidCellKey(x, y, z));
        return it == flowLevels->end() ? 0 : it->second;
    }
    static uint64_t liquidCellKey(int x, int y, int z) {
        return ((uint64_t)((uint32_t)x & 0xFFFFFFFu) << 36) | ((uint64_t)((uint32_t)z & 0xFFFFFFFu) << 8) | (uint64_t)(y & 0xFF);
    }
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
    // 1.6.0: the Forgotten Overworld (generation 4). 1.7.2 build 3 (generation 5): the Crimson Void
    // Expansion: the Crimsonium Forest, the Shattered Void and the Void Shards Desert.
    enum class VoidBiome { Purple, Crimson, Lava, Forgotten, CrimsoniumForest, Shattered, ShardDesert };
    VoidBiome voidBiomeAt(int x, int z) const;
    int voidHeight(int x, int z) const;
    bool netherGrassAt(int x, int z) const;
    // 1.5.1: the Powerstone Wasteland of the Cornerstone Dimension, and its forgotten villages
    // (the lamp column on the plaza; cy is the feet height there).
    bool wastelandAt(int x, int z) const;
    // 2.2.0 preview: the new biomes of the Cornerstone Dimension (0 none, 1 Gilded Quarry,
    // 2 Prismatide Coast, 3 Overgrown Monolith). The wasteland wins where both would be.
    int cornerBiomeAt(int x, int z) const;
    bool forgottenVillageIn(int rx, int rz, int& cx, int& cy, int& cz) const;
    static constexpr int kForgottenRegion = 192;
    // Name of the place for the HUD and /pos (any dimension), a Czech key of LangTable.inc.
    std::string placeName(int x, int z) const;
    // The nearest fortress (Nether) or boss arena (Crimson Void) to (x, z): its centre and floor height.
    bool nearestStructure(int x, int z, int& outX, int& outY, int& outZ) const;
    // 1.7.0 preview: the nearest Celestial portal frame (Crimson Void, Forgotten Overworld).
    bool nearestCelestialFrame(int x, int z, int& outX, int& outZ) const;
    static constexpr int kCelestialRegion = 240;
    void celestialSpot(int rx, int rz, int& px, int& pz) const;  // where a region's frame would lie
    // 1.9.5: the Celestial Temple of a region of the Reach (one near the arrival island), and its floor height.
    static constexpr int kTempleRegion = 320, kTempleY = 92, kTempleRadius = 15;
    void templeCentre(int rx, int rz, int& tx, int& tz) const;
    void stampCelestialTemple(Chunk& c);
    void generateBounded(Chunk& c);  // 1.9.5 build 2
    bool insideBounds(int x, int z) const;  // a bounded world's square (always true for endless worlds)
    // 1.5.7: the middle of the nearest mineshaft (Overworld: the wooden one; Cornerstone
    // Dimension: the crimson one), for explorer maps. False when none is within ~1000 blocks.
    bool nearestMineshaft(int x, int z, int& outX, int& outY, int& outZ);
    static constexpr int kFortressRegion = 176, kArenaRegion = 640, kVoidLavaLevel = 57, kVillageRegion = 384;
    // The village of an overworld region, if it has one: the well in the middle (feet height).
    bool villageIn(int rx, int rz, int& cx, int& cy, int& cz) const;
    bool ruinedStationIn(int rx, int rz, int& cx, int& cz) const;  // 2.4.0: the ruined station of a region (for the map)
    static constexpr int kStationRegion = 272;

    // Persistence: modified chunks are stored as files in this directory and
    // loaded instead of generated. Empty = nothing is saved (menu background).
    void setStorageDir(const std::string& dir) { storageDir_ = dir; }
    void saveModifiedChunks();  // also writes the region files
    uint32_t seed() const { return seed_; }

private:
    static int64_t key(int cx, int cz) { return (int64_t)(((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz); }
    void markDirty(int cx, int cz);
    void generateOverworld(Chunk& c);
    void generateNether(Chunk& c);
    void generateVoid(Chunk& c);
    void generateCelestial(Chunk& c);

public:
    // 1.7.0: the height of the top of the floating island at x, z, or -1 over the open sky.
    int celestialTop(int x, int z) const;
    // 1.8.0: bridges between the sky islands, and the generator of worlds made as "Old World".
    void stampCelestialBridges(Chunk& c);
    void generateOldWorld(Chunk& c);
    void generateOldNether(Chunk& c);

private:
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
    // 1.5.7: where the crimson mineshaft of a region of the Cornerstone Dimension starts (corner).
    static constexpr int kCrimsonShaftRegion = 224;
    bool crimsonShaftIn(int rx, int rz, int& ox, int& oy, int& oz) const;
    void arenaCentre(int rx, int rz, int& x, int& z) const;
    float arenaInfluence(int x, int z) const;  // 1 at an arena, 0 far away
    void placePine(Chunk& c, int x, int y, int z, uint32_t h, ItemId trunkBlock, ItemId leafBlock);
    void placeTree(Chunk& c, int x, int y, int z, uint32_t h, ItemId trunk, ItemId leaves);
    ItemId forgottenBlock(int wx, int y, int wz, int h) const;
    int forgottenTop(int wx, int wz, int h) const;
    void decorateCaves(Chunk& c, const Column cols[kChunkSize][kChunkSize]);
    void placeOres(Chunk& c, const OreDef& ore, uint32_t salt);
    std::string chunkPath(int cx, int cz) const;
    bool loadChunk(Chunk& c) const;
    void saveChunk(Chunk& c) const;
    // 1.5.7: chunks live in region files of 32 x 32 chunks ("r.<x>.<z>.vcr"), compressed, instead
    // of a small file each: far less space on the phone. Old chunk files are read and moved over.
    struct Region {
        std::vector<std::vector<uint8_t>> blobs = std::vector<std::vector<uint8_t>>(1024);
        bool dirty = false;
    };
    Region& region(int rx, int rz) const;
    std::string regionPath(int rx, int rz) const;
    void flushRegions() const;
    mutable std::unordered_map<uint64_t, Region> regions_;
    mutable std::vector<std::string> legacyFiles_;  // chunk files now kept in a region

    uint32_t seed_;
    Dimension dim_;
    bool deep_ = false;  // 2.2.0: the lifted Overworld with the deep layers below
    void deepenOverworld(Chunk& c);
    void liftColumns(Chunk& c) const;
    void stampBronzeHalls(Chunk& c);
    ItemId bronzeHallsBlock(int lx, int ly, int lz, uint32_t seed) const;
public:
    bool bronzeHallsIn(int rx, int rz, int& ox, int& oz) const;  // 2.3.0: the Bronze Halls of a region
    // 2.5.0: the subway network (see World.cpp): lines of regions, a station in every region a line passes.
    struct MetroStation {
        enum Kind { Stop, Depot, Terminus, Hub };
        bool ok = false, row = true, loopWest = false, loopEast = false;
        int line = 0, region = 0, kind = Stop, centre = 0, across = 0, length = 40, y = 0;
    };
    static constexpr int kMetroRegion = 448, kMetroY = 34, kMetroLowY = 18;
    int metroLineCoord(bool row, int line, bool& exists) const;
    bool metroActive(bool row, int line, int region, bool* first, bool* last) const;
    MetroStation metroStation(bool row, int line, int region) const;
    ItemId metroLineBlock(bool row, int line, int a, int b, int y) const;
    bool nearestMetroStation(int x, int z, int& outX, int& outZ, int& kind) const;  // its entrance shaft
    std::string metroStationName(int x, int z) const;  // the name of the station there (made up from the place)
    void stampMetro(Chunk& c);
    // 2.5.0: the lost city at the end of some lines: a ramp down from a terminus' loop into a huge cave in
    // the abyssal rock with streets, houses, lamps and a treasure on the square. `x0, z0`: its corner.
    bool lostCityOf(int line, int region, int& rampX, int& x0, int& z0) const;
    void stampLostCity(Chunk& c);
    // 2.5.0: where the generator puts a structure of kind `ki` in a region (shared with the map), and
    // the structures around a place (their kind, a WorldStructure, and their middle) for the map.
    struct FoundStructure {
        int type, x, z;
    };
    bool structurePlacement(int ki, int rx, int rz, int& ox, int& oy, int& oz, const Structure*& sp);
    static bool structureOnSurface(int type);
    std::vector<FoundStructure> structuresNear(int x, int z, int radius);
    // 2.5.0: developer maps: the pieces of the map in a chunk, the text of a board of the map.
    void stampDevMap(Chunk& c);
    // page: which page of a board with several (the HQ journal); pages and mark (a distance mark of the
    // test track, in metres, else -1) come back when asked for.
    std::string devSignText(int x, int y, int z, int page = 0, int* pages = nullptr, int* mark = nullptr) const;
    const BiomeDef* devBiomeAt(int x, int z) const;
    static constexpr int kCitySize = 90, kCityFloor = 8, kCityHeight = 24;
private:
    GenConfig config_;
    Noise terrain_, detail_, caves_, temperature_, humidity_;
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks_;
    std::unordered_map<uint64_t, std::shared_ptr<Structure>> blueprints_;  // generated structures, by region
    std::unordered_map<uint64_t, std::array<int, 4>> mineshafts_;          // region -> found?, x, y, z
    std::string storageDir_;
};

}  // namespace vc

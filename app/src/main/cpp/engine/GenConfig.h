#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Json.h"
#include "Blocks.h"

namespace vc {

// World generation settings, loaded from JSON files in packs:
//   biomes/<id>.json          one biome each
//   dimensions/overworld.json sea level, caves, ores
//   dimensions/nether.json    lava sea, blocks, glowstone, fog
// A copy is stored with every world (generator.json) so editing packs never
// changes the terrain of worlds that already exist.

struct BiomeDef {
    std::string id, name;
    float temperature = 0.5f, humidity = 0.5f;  // where it sits on the climate map, 0..1
    float height = 66, variation = 8;          // terrain base height and hilliness
    ItemId surface = 0, filler = 0, beach = 0, underwater = 0;
    int fillerDepth = 3;
    float treeDensity = 0;                     // chance per column
    ItemId trunk = 0, leaves = 0;
    float grassDensity = 0, flowerDensity = 0;  // tall grass and flowers, chance per grass column
    // 1.0.0 update
    int treeShape = 0;          // 0 round oak crown, 1 pine (cone)
    bool freeze = false;        // the sea surface turns to ice
    float cactusDensity = 0;    // cacti on sand, chance per column
    ItemId peakBlock = 0;      // above peakHeight the ground is this block (hardened stone in high mountains)
    int peakHeight = 128;
    int snowHeight = 128;       // above this the top block is snow
};

struct OreDef {
    ItemId block = 0;
    int veins = 0, size = 0, minY = 5, maxY = 64;
};

struct OverworldDef {
    int seaLevel = 62;
    bool caves = true;
    float caveSize = 0.06f;    // bigger = wider caves
    bool caverns = true;       // 1.4.0: big caverns with lakes, tar pools and dripstone
    bool structures = true;    // ruins, shipwrecks, dungeons, mineshafts
    float biomeScale = 320;    // blocks across a typical biome
    float sky[3] = {0.53f, 0.74f, 1.0f};
    std::vector<OreDef> ores;
};

struct NetherDef {
    int lavaLevel = 31;
    ItemId fill = 0, liquid = 0, floor = 0, shore = 0, ore = 0, ceiling = 0;
    float floorAmount = 0.25f;   // how much of the floor is covered by `floor`
    float oreChance = 1 / 120.0f;
    int glowstoneClusters = 2;   // at most per chunk
    float caveScale = 32;
    float openness = 0.05f;      // bigger = more open caverns
    float fog[3] = {0.22f, 0.04f, 0.03f};
};

struct GenConfig;
// Worlds made before 1.0 get the bronze, steel and ruby ores they lack.
void addMissingOres(GenConfig& config);
// Worlds made before 1.2 get powerstone and cornerstone grass instead of quartz and soul sand.
void upgradeNether(GenConfig& config);
// Default pack files of older versions: replaced by the new defaults if the player never changed them.
const std::vector<std::pair<std::string, std::string>>& upgradableDefaults();
// Default pack files that were dropped (path, old content): removed if the player never changed them.
const std::vector<std::pair<std::string, std::string>>& retiredDefaults();
// Worlds made with 1.5.0 lose the Overworld wasteland biome (it lives in the Cornerstone Dimension now).
void retireBiomes(GenConfig& config);

struct GenConfig {
    OverworldDef overworld;
    NetherDef nether;
    std::vector<BiomeDef> biomes;
    bool flat = false;  // superflat world type
    // Generator generations, so worlds keep the terrain they were made with: 1 = before 1.5.1,
    // 2 = the 1.5.1 Crimson Desert (dunes, rock formations, minerals, buried ruins).
    int crimsonDesert = 2;
    // 1.5.7: bigger caves with openings to the surface, crimson mineshafts and the larger
    // fortress. Worlds from before keep their old generator (1).
    // 1.5.9 (3): Crimson Void mineshafts and watchtowers, ruined villages in the Overworld.
    int generation = 3;

    // The built-in defaults (the same content as the default pack files).
    static const GenConfig& defaults();
    // From a merged document {"overworld": {...}, "nether": {...}, "biomes": [...]}.
    static GenConfig fromJson(const Json& doc, std::vector<std::string>* warnings = nullptr);
    Json toJson() const;
};

// The files of the built-in "VoxelCraft Default" pack, path -> content.
const std::vector<std::pair<std::string, std::string>>& defaultPackFiles();

}  // namespace vc

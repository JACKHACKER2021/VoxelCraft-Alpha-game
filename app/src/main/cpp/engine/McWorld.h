#pragma once
#include <map>
#include <string>
#include <vector>

#include "Items.h"

namespace vc {

// 2.0.0: turns a Minecraft Bedrock world (.mcworld) into a new VoxelCraft world.
//
// A .mcworld is a zip of the world folder; its chunks live in a LevelDB database (db/*.ldb,
// db/*.log). We read that database ourselves (no LevelDB library), decode the sub-chunks
// (palette format, versions 1, 8 and 9) and write each block, mapped to a VoxelCraft block, into
// a new world: the Overworld into the Overworld, the Nether into the Cornerstone Dimension and the
// End into the Crimson Void. Only blocks come over: no entities, chests' contents or redstone state.
// VoxelCraft worlds are 128 blocks high: a 1.18+ world (y -64..319) is shifted up by 56, so
// y -56..71 fits (higher mountains are cut off); older worlds keep y 0..127.
struct McWorldReport {
    std::string name;      // the world's name (levelname.txt or level.dat)
    int chunks = 0;        // columns written, all dimensions
    int subchunks = 0;     // sub-chunks decoded
    long long blocks = 0;  // non-air blocks written
    long long cutOff = 0;  // blocks above or below what fits
    std::map<std::string, long long> replaced;  // Bedrock names we had no exact match for -> count
    int yOffset = 0;
};

// Maps a Bedrock block name (without "minecraft:") to a VoxelCraft block. Unknown solid blocks
// become stone, unknown plants and small things air; `exact` says whether it was a real match.
ItemId mapMcWorldBlock(const std::string& name, bool& exact);

// Reads the .mcworld at `file` and creates the new world under `worldsRoot`. `folder` is the new
// world's folder. Errors are in Czech (shown with tr()).
bool importMcWorld(const std::string& file, const std::string& worldsRoot, std::string& folder, McWorldReport& report,
                   std::string& error);

// For tests: the database part on its own, from LevelDB files already in memory (name -> bytes).
bool readLevelDb(const std::map<std::string, std::vector<uint8_t>>& files, std::map<std::string, std::vector<uint8_t>>& out,
                 std::string& error);

}  // namespace vc

#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "World.h"

namespace vc {

// VCStructures: building blocks of VoxelCraft worlds (houses, towers, ruins).
//
// A structure is a box of sx * sy * sz cells holding VoxelCraft block ids.
// kVoid cells leave the world untouched (like Bedrock's structure void),
// AIR cells clear what is there.
//
// .vcstructure (binary, little endian, version 1):
//   "VCST" u16 version  u16 sx sy sz
//   u16 nameLength  name (UTF-8)
//   u16 paletteSize  then for each: u8 length + block key ("stone", "planks", "void", ...)
//   runs of (u16 count, u16 paletteIndex) over the cells in (y, z, x) order, x fastest
// Keys instead of numeric ids keep files valid if block ids ever change.
struct Structure {
    static constexpr ItemId kVoid = 0xFFFF;  // not a block id in VoxelCraft

    std::string name;
    int sx = 0, sy = 0, sz = 0;
    std::vector<ItemId> cells;  // index = (y * sz + z) * sx + x

    bool valid() const { return sx > 0 && sy > 0 && sz > 0 && cells.size() == (size_t)sx * sy * sz; }
    ItemId at(int x, int y, int z) const { return cells[((size_t)y * sz + z) * sx + x]; }
    ItemId& at(int x, int y, int z) { return cells[((size_t)y * sz + z) * sx + x]; }
    int blockCount() const;  // cells that are not void

    // Quarter turns clockwise seen from above (0-3), and mirroring along X.
    Structure rotated(int quarterTurns) const;
    Structure mirroredX() const;
};

struct ImportReport {
    int cells = 0, placedBlocks = 0, voidCells = 0;
    std::map<std::string, int> unknown;  // Bedrock block name -> cell count (become void)
    int ignoredStates = 0;               // palette entries whose states we cannot express
    int secondLayer = 0;                 // waterlogged/secondary layer cells (ignored)
    int entities = 0;                    // entities in the file (not imported)
};

std::vector<uint8_t> writeVcStructure(const Structure& s);
bool readVcStructure(const std::vector<uint8_t>& data, Structure& out, std::string& error);

// Imports a Minecraft Bedrock .mcstructure (little-endian NBT). Blocks VoxelCraft
// does not have become void and are listed in the report.
bool importMcStructure(const std::vector<uint8_t>& data, Structure& out, ImportReport& report, std::string& error);

// Writes a structure as a Bedrock .mcstructure (VoxelCraft blocks under their Bedrock names).
std::vector<uint8_t> exportMcStructure(const Structure& s);

// Copies a box of the world (all of it must be loaded; air becomes AIR, nothing is void).
bool captureStructure(const World& world, int x0, int y0, int z0, int sx, int sy, int sz, Structure& out,
                      std::string& error);

// Maps a Bedrock block name ("minecraft:oak_planks") to a VoxelCraft block, AIR for air,
// Structure::kVoid for structure_void and for blocks we do not have (known = false).
ItemId mapBedrockBlock(const std::string& name, bool& known);

struct BlockChange {
    int x, y, z;
    ItemId before, after;
};

// Places the structure with its minimum corner at (ox, oy, oz). Either the whole
// structure is placed or nothing is: every chunk it touches must be loaded and it
// must fit between y = 1 and the top of the world. Bedrock is never replaced.
// The previous blocks are returned in `undo` so the placement can be reverted.
bool placeStructure(World& world, const Structure& s, int ox, int oy, int oz, std::vector<BlockChange>& undo,
                    std::string& error);
void undoPlacement(World& world, const std::vector<BlockChange>& undo);

// Structures shipped with the game, so the Structures screen is never empty.
std::vector<Structure> builtinStructures();

bool loadStructureFile(const std::string& path, Structure& out, ImportReport& report, std::string& error);

}  // namespace vc

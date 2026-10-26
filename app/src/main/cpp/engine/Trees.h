#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "Blocks.h"

namespace vc {

class World;

// Growing saplings into trees, and leaves that fall when no trunk holds them.
enum class TreeKind { Oak, Pine, Void, Netherperk };
TreeKind treeForSapling(ItemId sapling);
// The block a sapling needs under it (grass or dirt, void grass, cornerstone grass).
bool saplingCanGrowOn(ItemId sapling, ItemId ground);
// Replaces the sapling at (x, y, z) with a whole tree. False if there is no room.
bool growTree(World& world, int x, int y, int z, TreeKind kind, uint32_t seed);
// True if a log is within 4 steps of the leaves at (x, y, z), going through leaves.
bool leafSupported(const World& world, int x, int y, int z);

// Powerstone: power blocks and switches feed cables; cables light lamps, ignite TNT and
// open iron doors. Call after any change at (x, y, z); TNT that got power is listed in `ignite`.
void updatePower(World& world, int x, int y, int z, std::vector<std::array<int, 3>>& ignite);
bool isPowerBlock(ItemId id);  // anything the power system cares about

}  // namespace vc

#pragma once
#include <cstdint>

#include "Blocks.h"

namespace vc {

class World;

// Axis-aligned box inside a block cell, in block units (0..1, fences reach 1.5 high).
struct Box {
    float x0, y0, z0, x1, y1, z1;
};

// Door and trapdoor state packed into the block id (see Blocks.h).
struct DoorState {
    int facing;  // 0 north, 1 east, 2 south, 3 west
    bool open, upper;
};
DoorState doorState(ItemId id);           // doors and trapdoors (upper = sits at the top for trapdoors)
ItemId doorId(ItemId base, DoorState s);  // base: OAK_DOOR, IRON_DOOR or OAK_TRAPDOOR

// The boxes a shaped block is made of (isShaped). Fences look at their neighbours.
// `collision` gives the boxes the player bumps into (fences are 1.5 blocks high),
// otherwise the boxes that are drawn and targeted. Returns how many were written.
int blockBoxes(const World& world, int x, int y, int z, ItemId id, bool collision, Box out[10]);

// The shape of a block as an item (icons, the hand, items on the ground): stairs, slabs, fences
// and a few small blocks keep their shape, everything else is one full cube. Returns how many
// boxes were written (at most kItemBoxes).
constexpr int kItemBoxes = 4;
int itemBoxes(ItemId id, Box out[kItemBoxes]);

// Whether a fence connects towards a neighbour block.
bool fenceConnects(ItemId neighbour);
// Whether a power cable runs towards a neighbour block (cables and powerstone parts).
bool cableConnects(ItemId neighbour);

}  // namespace vc

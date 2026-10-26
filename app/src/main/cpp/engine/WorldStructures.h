#pragma once
#include <cstdint>

#include "Structure.h"

namespace vc {

// Structures that generate in the world, each with a loot chest:
// a ruined house on land, a shipwreck on the sea floor, a stone brick dungeon
// and a mineshaft (long abandoned corridors) underground. In the Nether stands a
// fortress of nether bricks with a huge void stone portal frame in its hall, and the
// lava lands of the Crimson Void hold the boss arena.
enum class WorldStructure {
    Ruin, Shipwreck, Dungeon, Mineshaft, Fortress, BossArena,
    // 1.2.0: Crimson Void dungeons and towers, desert temples, igloos and watchtowers.
    VoidDungeon, VoidTower, DesertTemple, Igloo, Watchtower,
    // 1.3.0: villages with houses, a well, fields of wheat and villagers.
    Village,
    // 1.5.0: the forgotten powerstone kingdom: abandoned villages in the wasteland, ruins
    // under lakes and seas, and broken portals that lead the way to a Nether fortress.
    ForgottenVillage, DrownedRuins, BrokenPortal,
    Count
};

// Where the arena's centre lies inside its blueprint (the boss appears above it).
constexpr int kArenaRadius = 20;
// Villages: a round blueprint this wide, its centre (the well) in the middle.
constexpr int kVillageSize = 45;
// 1.5.0 blueprint sizes (round village, square ruins).
constexpr int kForgottenVillageSize = 41, kDrownedRuinsSize = 19, kBrokenPortalSize = 13;

// The blueprint of one instance; `seed` varies the shape (missing walls, corridor layout).
Structure buildWorldStructure(WorldStructure kind, uint32_t seed);

// 1.4.0 Anniversary modpack: a party plaza with "VOXELCRAFT" in big letters, the logo on a
// pedestal, a cake, balloons, lanterns and benches. The chest stands at kShowcaseChest.
constexpr int kShowcaseW = 43, kShowcaseD = 15;
constexpr int kShowcaseChest[3] = {27, 1, 9};
Structure anniversaryShowcase();

}  // namespace vc

#pragma once
#include <array>
#include <cstdint>

#include "Items.h"

namespace vc {

constexpr int kChestSize = 27;
using ChestContents = std::array<ItemStack, kChestSize>;

// What a structure chest holds the first time it is opened. `chestBlock` is one of
// LOOT_CHEST_RUIN, _SHIP, _DUNGEON or _MINE; `seed` makes the same chest always give
// the same loot (world seed and position).
ChestContents generateLoot(ItemId chestBlock, uint32_t seed);

// 1.5.0: what the brusher uncovers in a block of debris gravel (skeleton heads, coins, shards,
// pages of the old documents, relics, powerstone dust or flint). `seed`: world seed and position.
ItemStack archaeologyFind(uint32_t seed);

}  // namespace vc

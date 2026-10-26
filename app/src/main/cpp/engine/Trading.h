#pragma once
#include <vector>

#include "Items.h"
#include "Mobs.h"

namespace vc {

// Villagers trade goods for rubies and rubies for goods; what they offer depends on
// their profession (farmers, smiths and travelling traders).
struct Trade {
    ItemStack cost, result;
};
const std::vector<Trade>& tradesFor(Profession profession);
const char* professionName(Profession profession);  // Czech key

int countInInventory(const Inventory& inv, ItemId id);
// Pays the cost and adds the result when the player has enough and there is room. False otherwise.
bool makeTrade(Inventory& inv, const Trade& trade);

}  // namespace vc

#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Items.h"
#include "Json.h"
#include "Blocks.h"

namespace vc {

struct CraftResult {
    ItemId id = AIR;
    int count = 0;
    bool empty() const { return id == AIR; }
};

struct ShapedRecipe {
    int w = 0, h = 0;
    std::vector<ItemId> cells;  // w*h, row-major, AIR = must be empty
    CraftResult result;
};

struct ShapelessRecipe {
    std::vector<ItemId> ingredients;  // sorted
    CraftResult result;
};

// All crafting and furnace rules, loaded from recipes/*.json in the enabled
// packs (see GenConfig.cpp for the default files and their format).
struct RecipeBook {
    std::vector<ShapedRecipe> shaped;
    std::vector<ShapelessRecipe> shapeless;
    std::map<ItemId, ItemId> smelting;
    std::map<ItemId, float> fuels;

    // Adds the content of a crafting.json / smelting.json style document.
    void addJson(const Json& doc, std::vector<std::string>* warnings = nullptr);
    static const RecipeBook& defaults();
};

// The rules in use (defaults until packs are loaded).
const RecipeBook& recipes();
void setRecipes(RecipeBook book);

// Matches a square crafting grid (2x2 from the inventory, 3x3 on a crafting
// table), row-major. Shaped recipes may sit anywhere in the grid and may be
// mirrored; shapeless recipes only care about the ingredients.
CraftResult matchRecipe(const ItemStack* grid, int size);

// Furnace rules.
ItemId smeltResult(ItemId id);   // AIR if it cannot be smelted
float fuelSeconds(ItemId id);     // 0 if it does not burn

constexpr float kSmeltSeconds = 10;

struct Furnace {
    ItemStack input, fuel, output;
    float burnLeft = 0, burnTotal = 0;  // seconds of fuel left / of the current fuel item
    float cook = 0;                     // seconds spent on the current item

    bool burning() const { return burnLeft > 0; }
    bool canSmelt() const;
    // Advances time. Returns true when the furnace lit up or went out.
    bool update(float dt);
    bool empty() const { return input.empty() && fuel.empty() && output.empty(); }
};

}  // namespace vc

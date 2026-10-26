#include "Loot.h"

#include <vector>

namespace vc {

namespace {

struct LootEntry {
    ItemId id;
    int weight, minCount, maxCount;
};

// Loot tables: a few rolls of weighted entries per chest.
const std::vector<LootEntry>& table(ItemId chest, int& rollsMin, int& rollsMax) {
    static const std::vector<LootEntry> kRuin = {
        {APPLE, 10, 1, 3},      {STICK, 10, 2, 6},         {COAL, 8, 2, 5},         {IRON_INGOT, 5, 1, 3},
        {BRONZE_INGOT, 4, 1, 2}, {WOOD_PICKAXE, 3, 1, 1},  {STONE_AXE, 3, 1, 1},    {WOOL, 4, 1, 3},
        {OAK_DOOR_ITEM, 2, 1, 1}, {FLINT_AND_STEEL, 2, 1, 1}, {armorItem(0, 0), 2, 1, 1}, {armorItem(1, 3), 2, 1, 1},
    };
    static const std::vector<LootEntry> kShip = {
        {IRON_INGOT, 8, 1, 4},  {BRONZE_INGOT, 8, 1, 4}, {COAL, 8, 2, 8},           {PLANKS, 8, 4, 12},
        {APPLE, 6, 1, 4},       {STEEL_INGOT, 3, 1, 2},  {RUBY, 2, 1, 2},           {BOAT, 2, 1, 1},
        {armorItem(2, 1), 2, 1, 1}, {armorItem(2, 0), 2, 1, 1}, {CHEST, 1, 1, 1},
    };
    static const std::vector<LootEntry> kDungeon = {
        {IRON_INGOT, 8, 2, 5},  {STEEL_INGOT, 5, 1, 3},  {RUBY, 3, 1, 3},           {APPLE, 6, 1, 3},
        {FLINT_AND_STEEL, 3, 1, 1}, {armorItem(3, 0), 2, 1, 1}, {armorItem(3, 2), 2, 1, 1}, {armorItem(2, 1), 3, 1, 1},
        {STEEL_PICKAXE, 2, 1, 1}, {POWERSTONE_LIGHT, 4, 1, 4},  {OBSIDIAN, 3, 1, 3},
    };
    static const std::vector<LootEntry> kMine = {
        {COAL, 10, 3, 9},       {IRON_ORE, 6, 1, 5},     {BRONZE_INGOT, 6, 1, 4},  {STEEL_INGOT, 3, 1, 3},
        {RUBY, 2, 1, 2},        {STONE_PICKAXE, 3, 1, 1}, {IRON_PICKAXE, 2, 1, 1}, {BRONZE_SHOVEL, 2, 1, 1},
        {FENCE, 4, 2, 6},       {PLANKS, 5, 3, 10},      {APPLE, 4, 1, 3},
    };
    // The Nether fortress: lots of loot, the best metals and the key to the Crimson Void.
    static const std::vector<LootEntry> kFortress = {
        {RUBY, 8, 1, 4},          {STEEL_INGOT, 8, 2, 5},     {BRONZE_INGOT, 6, 2, 6},   {IRON_INGOT, 6, 2, 6},
        {POWERSTONE_LIGHT, 6, 2, 8},     {OBSIDIAN, 5, 2, 6},        {NETHER_BRICK, 5, 4, 12},  {COOKED_PORK, 6, 2, 5},
        {RUBY_BLOCK, 2, 1, 1},    {STEEL_BLOCK, 2, 1, 2},     {RUBY_PICKAXE, 2, 1, 1},   {STEEL_AXE, 2, 1, 1},
        {armorItem(4, 1), 2, 1, 1}, {armorItem(4, 0), 2, 1, 1}, {armorItem(3, 2), 3, 1, 1}, {armorItem(3, 3), 3, 1, 1},
        {VOID_FLINT_AND_STEEL, 3, 1, 1}, {FLINT_AND_STEEL, 3, 1, 1}, {BED_ITEM, 2, 1, 1},
    };
    // 1.2.0: the Crimson Void (with the arena compass), desert temples, igloos and watchtowers.
    static const std::vector<LootEntry> kVoid = {
        {RUBY, 8, 2, 6},         {STEEL_INGOT, 6, 2, 5},   {VOID_STONE_BRICKS, 5, 4, 12}, {EYE_LOG, 4, 1, 3},
        {RUBY_SWORD, 2, 1, 1},   {STEEL_SWORD, 3, 1, 1},   {armorItem(4, 2), 2, 1, 1},    {armorItem(4, 3), 2, 1, 1},
        {POWERSTONE_DUST, 5, 3, 9}, {TNT, 3, 1, 3},        {COOKED_MUTTON, 5, 2, 5},      {BOSS_COMPASS, 3, 1, 1},
    };
    static const std::vector<LootEntry> kTemple = {
        {RUBY, 5, 1, 3},         {BRONZE_INGOT, 8, 2, 6},  {SANDSTONE, 6, 4, 12},         {TNT, 5, 1, 4},
        {IRON_SWORD, 3, 1, 1},   {BRONZE_SWORD, 3, 1, 1},  {APPLE, 6, 1, 4},              {POWERSTONE_DUST, 5, 2, 6},
        {armorItem(2, 0), 2, 1, 1}, {BUCKET, 3, 1, 1},
    };
    static const std::vector<LootEntry> kIgloo = {
        {COOKED_PORK, 8, 2, 5},  {COOKED_MUTTON, 8, 2, 5}, {APPLE, 6, 2, 5},              {SNOW_BLOCK, 5, 2, 8},
        {ICE, 4, 1, 4},          {PINE_SAPLING, 5, 1, 3},  {WOOL, 4, 2, 5},               {IRON_INGOT, 3, 1, 3},
        {CANDY_CANE, 3, 1, 3},
    };
    static const std::vector<LootEntry> kTower = {
        {IRON_INGOT, 6, 1, 4},   {STONE_SWORD, 4, 1, 1},   {IRON_SWORD, 3, 1, 1},         {armorItem(1, 1), 3, 1, 1},
        {armorItem(2, 3), 3, 1, 1}, {COOKED_PORK, 6, 1, 4}, {OAK_SAPLING, 5, 1, 3},       {BUCKET, 2, 1, 1},
        {SPRUCE_PLANKS, 5, 4, 10},
    };
    // 1.5.0: the forgotten kingdom. Relics, coins and pages of its story; the portal ruins
    // always hold the compass to a fortress (added below).
    static const std::vector<LootEntry> kForgotten = {
        {POWERSTONE_DUST, 8, 2, 6}, {ANCIENT_COIN, 8, 1, 4},      {POTTERY_SHARD, 6, 1, 3},   {POWERSTONE_BRICKS, 5, 4, 10},
        {POWERSTONE_LAMP, 3, 1, 2}, {BRUSHER, 3, 1, 1},          {POWERSTONE_RELIC, 2, 1, 1}, {BREAD, 5, 1, 3},
        {IRON_INGOT, 4, 1, 3},      {SKELETON_HEAD, 2, 1, 1},    {POWERSTONE_CHIP, 3, 1, 2},
    };
    static const std::vector<LootEntry> kDrowned = {
        {ANCIENT_COIN, 8, 2, 6},    {IRON_INGOT, 6, 1, 4},       {BRONZE_INGOT, 6, 1, 4},    {POTTERY_SHARD, 6, 1, 3},
        {SKELETON_HEAD, 3, 1, 1},   {RUBY, 3, 1, 2},             {POWERSTONE_RELIC, 2, 1, 1}, {BRUSHER, 2, 1, 1},
        {POWERSTONE_DUST, 4, 1, 4},
    };
    static const std::vector<LootEntry> kPortalRuin = {
        {OBSIDIAN, 8, 1, 4},        {FLINT_AND_STEEL, 4, 1, 1},  {POWERSTONE_DUST, 6, 2, 5}, {ANCIENT_COIN, 5, 1, 3},
        {IRON_INGOT, 5, 1, 3},      {NETHER_BRICK, 4, 2, 6},     {COOKED_PORK, 4, 1, 3},     {CRACKED_POWERSTONE_BRICKS, 4, 2, 6},
    };
    switch (chest) {
        case LOOT_CHEST_FORGOTTEN: rollsMin = 4; rollsMax = 8; return kForgotten;
        case LOOT_CHEST_DROWNED: rollsMin = 4; rollsMax = 7; return kDrowned;
        case LOOT_CHEST_PORTAL: rollsMin = 4; rollsMax = 7; return kPortalRuin;
        case LOOT_CHEST_VOID: rollsMin = 5; rollsMax = 9; return kVoid;
        case LOOT_CHEST_TEMPLE: rollsMin = 4; rollsMax = 8; return kTemple;
        case LOOT_CHEST_IGLOO: rollsMin = 3; rollsMax = 6; return kIgloo;
        case LOOT_CHEST_TOWER: rollsMin = 3; rollsMax = 6; return kTower;
        case LOOT_CHEST_FORTRESS: rollsMin = 6; rollsMax = 11; return kFortress;
        case LOOT_CHEST_SHIP: rollsMin = 4; rollsMax = 8; return kShip;
        case LOOT_CHEST_DUNGEON: rollsMin = 3; rollsMax = 7; return kDungeon;
        case LOOT_CHEST_MINE: rollsMin = 3; rollsMax = 6; return kMine;
        default: rollsMin = 3; rollsMax = 6; return kRuin;
    }
}

}  // namespace

ChestContents generateLoot(ItemId chestBlock, uint32_t seed) {
    ChestContents out{};
    uint32_t r = seed * 2654435761u + 0x9E3779B9u;
    auto rnd = [&](int n) {
        r ^= r << 13;
        r ^= r >> 17;
        r ^= r << 5;
        return (int)(r % (uint32_t)n);
    };
    int rollsMin, rollsMax;
    const std::vector<LootEntry>& t = table(chestBlock, rollsMin, rollsMax);
    int total = 0;
    for (const LootEntry& e : t) total += e.weight;
    const int rolls = rollsMin + rnd(rollsMax - rollsMin + 1);
    for (int i = 0; i < rolls; ++i) {
        int pick = rnd(total);
        const LootEntry* e = &t[0];
        for (const LootEntry& c : t) {
            if (pick < c.weight) {
                e = &c;
                break;
            }
            pick -= c.weight;
        }
        int count = std::min(e->minCount + rnd(e->maxCount - e->minCount + 1), maxStack(e->id));
        if (e->id == VOID_FLINT_AND_STEEL || e->id == BOSS_COMPASS || e->id == BRUSHER) count = 1;
        // Scattered over the chest like in the classic game; never on top of other loot.
        for (int tries = 0; tries < 40; ++tries) {
            ItemStack& slot = out[rnd(kChestSize)];
            if (slot.empty()) {
                slot = {e->id, (uint8_t)count, 0};
                break;
            }
        }
    }
    // Every other fortress chest holds the void flint and steel for sure.
    if (chestBlock == LOOT_CHEST_FORTRESS && rnd(2) == 0) {
        bool has = false;
        for (const ItemStack& st : out) has |= st.id == VOID_FLINT_AND_STEEL;
        for (int tries = 0; tries < 60 && !has; ++tries) {
            ItemStack& slot = out[rnd(kChestSize)];
            if (slot.empty()) {
                slot = {VOID_FLINT_AND_STEEL, 1, 0};
                has = true;
            }
        }
    }
    // Void chests point the way to the arena often.
    if (chestBlock == LOOT_CHEST_VOID && rnd(5) < 2) {
        bool has = false;
        for (const ItemStack& st : out) has |= st.id == BOSS_COMPASS;
        for (int tries = 0; tries < 60 && !has; ++tries) {
            ItemStack& slot = out[rnd(kChestSize)];
            if (slot.empty()) {
                slot = {BOSS_COMPASS, 1, 0};
                has = true;
            }
        }
    }
    // The story of the forgotten kingdom: a page in every chest of its ruins (and in some
    // fortress chests), each kind of place holding its own part of the tale.
    auto put = [&](ItemId id) {
        for (int tries = 0; tries < 60; ++tries) {
            ItemStack& slot = out[rnd(kChestSize)];
            if (slot.empty()) {
                slot = {id, 1, 0};
                return;
            }
        }
    };
    if (chestBlock == LOOT_CHEST_FORGOTTEN) put((ItemId)(OLD_DOCUMENT + rnd(4)));          // pages I-IV
    if (chestBlock == LOOT_CHEST_PORTAL) put((ItemId)(OLD_DOCUMENT + 3 + rnd(3)));         // IV-VI
    if (chestBlock == LOOT_CHEST_DROWNED) put((ItemId)(OLD_DOCUMENT + 5 + rnd(3)));        // VI-VIII
    if (chestBlock == LOOT_CHEST_FORTRESS && rnd(3) == 0) put((ItemId)(OLD_DOCUMENT + 5 + rnd(2)));
    // Broken portal ruins: the compass that finds a Nether fortress, always.
    if (chestBlock == LOOT_CHEST_PORTAL) put(FORTRESS_COMPASS);
    return out;
}

ItemStack archaeologyFind(uint32_t seed) {
    uint32_t r = seed * 2654435761u + 0x51ED2Bu;
    r ^= r << 13;
    r ^= r >> 17;
    r ^= r << 5;
    const int roll = (int)(r % 100), extra = (int)((r >> 8) % 8);
    if (roll < 10) return {SKELETON_HEAD, 1, 0};
    if (roll < 35) return {ANCIENT_COIN, (uint8_t)(1 + extra % 3), 0};
    if (roll < 55) return {POTTERY_SHARD, 1, 0};
    if (roll < 70) return {(ItemId)(OLD_DOCUMENT + extra), 1, 0};
    if (roll < 75) return {POWERSTONE_RELIC, 1, 0};
    if (roll < 90) return {POWERSTONE_DUST, (uint8_t)(1 + extra % 4), 0};
    return {FLINT, 1, 0};
}

}  // namespace vc

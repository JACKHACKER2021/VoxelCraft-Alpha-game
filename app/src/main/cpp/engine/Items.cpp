#include "Items.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "Mods.h"

namespace vc {

// The creative catalog, grouped by tab (see kCreativeTabs).
const ItemId kCreativeItems[] = {
    // Building
    STONE, COBBLESTONE, MOSSY_COBBLESTONE, STONE_BRICKS, BRICKS, PLANKS, LOG, SANDSTONE, GLASS, BOOKSHELF,
    OBSIDIAN, BRONZE_BLOCK, STEEL_BLOCK, RUBY_BLOCK, CRAFTING_TABLE, FURNACE, BEDROCK,
    SPRUCE_PLANKS, SPRUCE_LOG, HARDENED_STONE, CHEST, FENCE, OAK_TRAPDOOR, OAK_DOOR_ITEM, IRON_DOOR_ITEM,
    WOOL, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, VOID_STONE_BRICKS, BED_ITEM,
    VOID_PLANKS, NETHERPERK_PLANKS, TNT, POWERSTONE_BLOCK, POWER_CABLE, POWERSTONE_LAMP, SWITCH, POWERSTONE_LIGHT, HAY_BALE,
    1000, 1008, 1016, 1024, 1032, 1040, 1048, 1056, 1064, 1072, 1080, 1088,
    1100, 1103, 1106, 1109, 1112, 1115, 1118, 1121, 1124, 1127, 1130, 1133,
    1210, 1211, 1212, 1213, 1214, 1215, 1216, 1217, 1218, 1219, 1220, 1221, 1222, 1223, 1224, 1225, TAR_BLOCK,
    POWERSTONE_BRICKS, CRACKED_POWERSTONE_BRICKS, CHISELED_POWERSTONE_BRICKS, POWERSTONE_GLASS, STAIRS_EXTRA, 1136, STAIRS_EXTRA2, STAIRS_EXTRA2 + 8, STAIRS_EXTRA2 + 16, STAIRS_EXTRA2 + 24,
    SLAB_EXTRA, SLAB_EXTRA + 3, SLAB_EXTRA + 6, SLAB_EXTRA + 9,
    CRACKED_NETHER_BRICKS, CHISELED_NETHER_BRICKS, SPRUCE_FENCE, VOID_FENCE, NETHERPERK_FENCE, NETHER_BRICK_FENCE,
    POWERSTONE_FENCE, SKELETON_HEAD, VOID_BRICK_EYE, TORCH, CRIMSON_PLANKS, POWERSTONE_FURNACE,
    DEEPSTONE_BRICKS, CRACKED_DEEPSTONE_BRICKS, MOSSY_DEEPSTONE_BRICKS, ROTTEN_PLANKS, MOB_SPAWNER, CELESTIAL_FRAME,
    CELESTIAL_PLANKS, CELESTIAL_STONE_BRICKS,
    DUSTY_CONCRETE, HARDENED_CONCRETE, GRAY_PLANKS, STAIRS_EXTRA3, STAIRS_EXTRA3 + 8, STAIRS_EXTRA3 + 16, STAIRS_EXTRA3 + 24,
    STAIRS_EXTRA3 + 32, SLAB_EXTRA3, SLAB_EXTRA3 + 3, SLAB_EXTRA3 + 6, SLAB_EXTRA3 + 9, SLAB_EXTRA3 + 12,
    CUTTER, WOOD_CUTTER, BARREL,
    CRIMSONIUM_PLANKS, SHATTERED_BRICKS, DARK_SANDSTONE, STAIRS_EXTRA5 + 0, STAIRS_EXTRA5 + 8, STAIRS_EXTRA5 + 16, STAIRS_EXTRA5 + 24, SLAB_EXTRA5 + 0, SLAB_EXTRA5 + 3, SLAB_EXTRA5 + 6, SLAB_EXTRA5 + 9,
    CONCRETE_POWDER + 0, CONCRETE_POWDER + 1, CONCRETE_POWDER + 2, CONCRETE_POWDER + 3, CONCRETE_POWDER + 4, CONCRETE_POWDER + 5, CONCRETE_POWDER + 6, CONCRETE_POWDER + 7, CONCRETE_POWDER + 8, CONCRETE_POWDER + 9, CONCRETE_POWDER + 10, CONCRETE_POWDER + 11, CONCRETE_POWDER + 12, CONCRETE_POWDER + 13, CONCRETE_POWDER + 14, CONCRETE_POWDER + 15,
    CONCRETE + 0, CONCRETE + 1, CONCRETE + 2, CONCRETE + 3, CONCRETE + 4, CONCRETE + 5, CONCRETE + 6, CONCRETE + 7, CONCRETE + 8, CONCRETE + 9, CONCRETE + 10, CONCRETE + 11, CONCRETE + 12, CONCRETE + 13, CONCRETE + 14, CONCRETE + 15,
    CARPET + 0, CARPET + 1, CARPET + 2, CARPET + 3, CARPET + 4, CARPET + 5, CARPET + 6, CARPET + 7, CARPET + 8, CARPET + 9, CARPET + 10, CARPET + 11, CARPET + 12, CARPET + 13, CARPET + 14, CARPET + 15,
    STAIRS_EXTRA4 + 0, STAIRS_EXTRA4 + 8, STAIRS_EXTRA4 + 16, STAIRS_EXTRA4 + 24, STAIRS_EXTRA4 + 32, STAIRS_EXTRA4 + 40, STAIRS_EXTRA4 + 48, STAIRS_EXTRA4 + 56, STAIRS_EXTRA4 + 64, STAIRS_EXTRA4 + 72, STAIRS_EXTRA4 + 80, STAIRS_EXTRA4 + 88, STAIRS_EXTRA4 + 96, STAIRS_EXTRA4 + 104, STAIRS_EXTRA4 + 112,
    SLAB_EXTRA4 + 0, SLAB_EXTRA4 + 3, SLAB_EXTRA4 + 6, SLAB_EXTRA4 + 9, SLAB_EXTRA4 + 12, SLAB_EXTRA4 + 15, SLAB_EXTRA4 + 18, SLAB_EXTRA4 + 21, SLAB_EXTRA4 + 24, SLAB_EXTRA4 + 27, SLAB_EXTRA4 + 30, SLAB_EXTRA4 + 33, SLAB_EXTRA4 + 36, SLAB_EXTRA4 + 39, SLAB_EXTRA4 + 42,
    // Nature
    GRASS, DIRT, SAND, GRAVEL, CLAY, SNOW_BLOCK, ICE, CACTUS, LEAVES, SPRUCE_LEAVES, TALL_GRASS, DANDELION, POPPY, WATER, LAVA,
    COAL_ORE, IRON_ORE, BRONZE_ORE, STEEL_ORE, RUBY_ORE, NETHERRACK, NETHER_BRICK,
    NETHER_GRASS, NETHER_SPROUTS, VOID_STONE, VOID_GRASS, VOID_LOG, EYE_LOG, VOID_LEAVES, RED_SAND, CRIMSON_ROOTS, PUMPKIN,
    NETHERPERK_LOG, NETHERPERK_CAP, OAK_SAPLING, PINE_SAPLING, VOID_SAPLING, NETHERPERK_SAPLING, POWERSTONE_ORE, WORLD_END,
    FARMLAND, WHEAT_SEEDS, TAR, DRIPSTONE_BLOCK, DRIPSTONE_UP, SNOW_LAYER,
    WASTELAND_GRASS, DRY_GRASS, GRASSER_FLOWER, FERN, DEBRIS_GRAVEL, CRIMSON_STONE, CRIMSON_LOG,
    DEEPSTONE, HARDIUM_ORE, MOSSY_DEEPSTONE, MOSS_BLOCK, FUNGAL_DEEPSTONE, VINES, SOULED_GRASS, SOULED_DIRT, SOULED_STONE,
    CELESTIAL_GRASS, CELESTIAL_DIRT, CELESTIAL_STONE, CELESTIAL_LOG, CELESTIAL_LEAVES,
    GRAY_LOG, RED_LEAVES, ORANGE_LEAVES, YELLOW_LEAVES, GRAY_SAPLING, CELESTIAL_SAPLING,
    CRIMSONIUM_LOG, CRIMSONIUM_LEAVES, SHATTERED_STONE, VOID_SHARD_BLOCK, DARK_SAND, SHARD_CLUSTER, CRIMSONIUM_ORE,
    // Tools
    WOOD_PICKAXE, WOOD_AXE, WOOD_SHOVEL, STONE_PICKAXE, STONE_AXE, STONE_SHOVEL, IRON_PICKAXE, IRON_AXE, IRON_SHOVEL,
    BRONZE_PICKAXE, BRONZE_AXE, BRONZE_SHOVEL, STEEL_PICKAXE, STEEL_AXE, STEEL_SHOVEL, RUBY_PICKAXE, RUBY_AXE, RUBY_SHOVEL,
    FLINT_AND_STEEL, VOID_FLINT_AND_STEEL,
    WOOD_SWORD, STONE_SWORD, IRON_SWORD, BRONZE_SWORD, STEEL_SWORD, RUBY_SWORD, BUCKET, WATER_BUCKET, LAVA_BUCKET, BOSS_COMPASS,
    HOE, BOW, ARROW, MAP, BRUSHER, FORTRESS_COMPASS, SHIELD, EXPLORER_MAP, WRITABLE_BOOK, HARDIUM_PICKAXE, CELESTIAL_COMPASS, HARDIUM_SWORD, HARDIUM_AXE, HARDIUM_SHOVEL,
    // Armour
    228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247,
    HARDIUM_ARMOR, HARDIUM_ARMOR + 1, HARDIUM_ARMOR + 2, HARDIUM_ARMOR + 3,
    // Materials
    VOID_SHARD, CRIMSONIUM, DYE + 0, DYE + 1, DYE + 2, DYE + 3, DYE + 4, DYE + 5, DYE + 6, DYE + 7, DYE + 8, DYE + 9, DYE + 10, DYE + 11, DYE + 12, DYE + 13, DYE + 14, DYE + 15,
    STICK, COAL, IRON_INGOT, BRONZE_INGOT, STEEL_INGOT, RUBY, FLINT, APPLE, BOAT,
    RAW_PORK, COOKED_PORK, RAW_MUTTON, COOKED_MUTTON,
    POWERSTONE_DUST, POWERSTONE_CHIP, EGG_PIG, EGG_SHEEP, EGG_SLIME, EGG_FORGOTTEN,
    FEATHER, WHEAT, BREAD, RAW_CHICKEN, COOKED_CHICKEN, RAW_BEEF, STEAK, MILK_BUCKET, EGG_CHICKEN, EGG_COW, EGG_VILLAGER,
    STRING, TAR_BALL, EGG_SPIDER, EGG_BAT, EGG_TNT_ZOMBIE, EGG_TAR_MONSTER, CRIMSON_HEAD,
    OLD_DOCUMENT, OLD_DOCUMENT + 1, OLD_DOCUMENT + 2, OLD_DOCUMENT + 3, OLD_DOCUMENT + 4, OLD_DOCUMENT + 5, OLD_DOCUMENT + 6,
    OLD_DOCUMENT + 7, ANCIENT_COIN, POWERSTONE_RELIC, POTTERY_SHARD, EGG_HUSKING, EGG_HOGMAN, HARDIUM,
};
const int kCreativeItemCount = sizeof(kCreativeItems) / sizeof(kCreativeItems[0]);
static_assert(sizeof(kCreativeItems) / sizeof(kCreativeItems[0]) == 232 + 80 + 44 + 24 + 69, "the creative tabs cover the whole catalog");
const CreativeTab kCreativeTabs[] = {
    {"Stavební", 0, 232}, {"Příroda", 232, 80}, {"Nástroje", 312, 44}, {"Brnění", 356, 24}, {"Suroviny", 380, 69},
};
const int kCreativeTabCount = sizeof(kCreativeTabs) / sizeof(kCreativeTabs[0]);

namespace {
bool keyHas(ItemId id, const char* part) { return std::strstr(itemKey(id), part) != nullptr; }
bool keyEnds(ItemId id, const char* tail) {
    const std::string k = itemKey(id), t = tail;
    return k.size() >= t.size() && k.compare(k.size() - t.size(), t.size(), t) == 0;
}

int categoryOf(ItemId id, int oldTab) {
    if (isSpawnEgg(id)) return CatEggs;
    if (isArmor(id) || id == SHIELD) return id == SHIELD ? CatTools : CatArmor;
    if (id == MAP || id == EXPLORER_MAP || id == FORTRESS_COMPASS || id == BOSS_COMPASS || id == WRITABLE_BOOK ||
        id == WRITTEN_BOOK || isDocument(id) || id == ANCIENT_COIN || id == POWERSTONE_RELIC || id == POTTERY_SHARD ||
        id == WORLD_END || id == BEDROCK)
        return CatMaps;
    if (isStainedGlass(id) || id == GLASS || id == POWERSTONE_GLASS) return CatGlass;
    if (isTorch(id) || id == POWERSTONE_LIGHT || id == POWERSTONE_LAMP || isLamp(id)) return CatLighting;
    if (isCable(id) || isSwitch(id) || isWallSwitch(id) || isTnt(id) || isPowerstoneFurnace(id) || id == POWERSTONE_BLOCK ||
        id == POWERSTONE_DUST || id == POWERSTONE_CHIP || (keyHas(id, "powerstone") && !keyEnds(id, "_ore")))
        return CatPowerstone;
    if (oldTab == 2 || toolDurability(id) > 0 || isSword(id)) return CatTools;
    if (isFood(id)) return CatMaterials;
    if (isLog(id)) return CatBuilding;  // 1.7.2 build 2: every log next to oak and spruce, not only those two
    if (oldTab == 1) return CatNature;
    if (isFence(id) || isDoor(id) || isTrapdoor(id) || isBed(id) || isChest(id) || isSkeletonHead(id) || id == BOOKSHELF ||
        id == CRAFTING_TABLE || id == FURNACE || id == HAY_BALE || id == WOOL || (id >= 164 && id <= 178) || id == OAK_DOOR_ITEM ||
        id == IRON_DOOR_ITEM || id == BED_ITEM)
        return CatDecoration;
    if (oldTab == 0) return CatBuilding;
    return CatMaterials;
}
}  // namespace

const char* creativeCategoryName(int category) {
    static const char* const names[CatCount] = {"Stavební bloky", "Sklo", "Osvětlení", "Příroda", "Dekorace",
                                                "Powerstone a mechanismy", "Nástroje a zbraně", "Brnění", "Mapy a zvláštní předměty",
                                                "Jídlo a suroviny", "Vejce"};
    return category >= 0 && category < CatCount ? names[category] : "";
}

ItemId creativeCategoryIcon(int category) {
    static const ItemId icons[CatCount] = {STONE_BRICKS, GLASS, TORCH, GRASS, BOOKSHELF, POWERSTONE_LAMP, IRON_PICKAXE,
                                           (ItemId)(armorItem(2, 1)), MAP, BREAD, EGG_PIG};
    return category >= 0 && category < CatCount ? icons[category] : AIR;
}

const std::vector<ItemId>& creativeCategoryItems(int category) {
    static std::vector<ItemId> lists[CatCount];
    static bool built = false;
    if (!built) {
        built = true;
        for (int t = 0; t < kCreativeTabCount; ++t)
            for (int i = kCreativeTabs[t].first; i < kCreativeTabs[t].first + kCreativeTabs[t].count; ++i)
                lists[categoryOf(kCreativeItems[i], t)].push_back(kCreativeItems[i]);
    }
    static const std::vector<ItemId> none;
    return category >= 0 && category < CatCount ? lists[category] : none;
}

const char* creativeGroup(ItemId id) {
    if (isStainedGlass(id)) return "Barevné sklo";
    if (isStairs(id)) return "Schody";
    if (isSlab(id)) return "Desky";
    if (isFence(id)) return "Ploty";
    if (id == WOOL || (id >= 164 && id <= 178)) return "Vlna";
    if (isDocument(id)) return "Staré dokumenty";
    if (isSapling(id)) return "Sazenice";
    if (isLeaves(id)) return "Listí";
    if (isLog(id) && !keyHas(id, "eye")) return "Kmeny";
    if (keyEnds(id, "planks")) return "Prkna";
    if (keyEnds(id, "_ore")) return "Rudy";
    if (isCable(id)) return "Kabely";
    if (isSword(id)) return "Meče";
    if (keyEnds(id, "_pickaxe")) return "Krumpáče";
    if (keyEnds(id, "_axe")) return "Sekery";
    if (keyEnds(id, "_shovel")) return "Lopaty";
    if (isPaintedPlanks(id)) return "Malovaná prkna";
    return nullptr;
}

namespace {
struct KeyEntry {
    ItemId id;
    const char* key;
};
const KeyEntry kKeys[] = {
    {AIR, "air"}, {STONE, "stone"}, {GRASS, "grass"}, {DIRT, "dirt"}, {COBBLESTONE, "cobblestone"},
    {PLANKS, "planks"}, {BEDROCK, "bedrock"}, {WATER, "water"}, {LAVA, "lava"}, {SAND, "sand"},
    {GRAVEL, "gravel"}, {TALL_GRASS, "tall_grass"}, {DANDELION, "dandelion"}, {POPPY, "poppy"}, {IRON_ORE, "iron_ore"}, {COAL_ORE, "coal_ore"}, {LOG, "log"}, {LEAVES, "leaves"},
    {GLASS, "glass"}, {BRICKS, "bricks"}, {OBSIDIAN, "obsidian"}, {CRAFTING_TABLE, "crafting_table"},
    {FURNACE, "furnace"}, {LIT_FURNACE, "lit_furnace"}, {NETHERRACK, "netherrack"},
    {PORTAL, "portal"}, {NETHER_BRICK, "nether_brick"},
    {STICK, "stick"}, {COAL, "coal"}, {IRON_INGOT, "iron_ingot"}, {FLINT, "flint"},
    {WOOD_PICKAXE, "wooden_pickaxe"}, {STONE_PICKAXE, "stone_pickaxe"}, {IRON_PICKAXE, "iron_pickaxe"},
    {WOOD_AXE, "wooden_axe"}, {STONE_AXE, "stone_axe"}, {IRON_AXE, "iron_axe"},
    {WOOD_SHOVEL, "wooden_shovel"}, {STONE_SHOVEL, "stone_shovel"}, {IRON_SHOVEL, "iron_shovel"},
    {APPLE, "apple"}, {FLINT_AND_STEEL, "flint_and_steel"},
    {SANDSTONE, "sandstone"}, {BOOKSHELF, "bookshelf"}, {MOSSY_COBBLESTONE, "mossy_cobblestone"},
    {SNOW_BLOCK, "snow"}, {CLAY, "clay"}, {STONE_BRICKS, "stone_bricks"},
    {WOOL, "white_wool"}, {164, "orange_wool"}, {165, "magenta_wool"}, {166, "light_blue_wool"}, {167, "yellow_wool"},
    {168, "lime_wool"}, {169, "pink_wool"}, {170, "gray_wool"}, {171, "light_gray_wool"}, {172, "cyan_wool"},
    {173, "purple_wool"}, {174, "blue_wool"}, {175, "brown_wool"}, {176, "green_wool"}, {177, "red_wool"},
    {178, "black_wool"},
    {BRONZE_ORE, "bronze_ore"}, {STEEL_ORE, "steel_ore"}, {RUBY_ORE, "ruby_ore"},
    {BRONZE_BLOCK, "bronze_block"}, {STEEL_BLOCK, "steel_block"}, {RUBY_BLOCK, "ruby_block"},
    {BRONZE_INGOT, "bronze_ingot"}, {STEEL_INGOT, "steel_ingot"}, {RUBY, "ruby"},
    {BRONZE_PICKAXE, "bronze_pickaxe"}, {BRONZE_AXE, "bronze_axe"}, {BRONZE_SHOVEL, "bronze_shovel"},
    {STEEL_PICKAXE, "steel_pickaxe"}, {STEEL_AXE, "steel_axe"}, {STEEL_SHOVEL, "steel_shovel"},
    {RUBY_PICKAXE, "ruby_pickaxe"}, {RUBY_AXE, "ruby_axe"}, {RUBY_SHOVEL, "ruby_shovel"},
    {228, "wooden_helmet"}, {229, "wooden_chestplate"}, {230, "wooden_leggings"}, {231, "wooden_boots"},
    {232, "stone_helmet"}, {233, "stone_chestplate"}, {234, "stone_leggings"}, {235, "stone_boots"},
    {236, "bronze_helmet"}, {237, "bronze_chestplate"}, {238, "bronze_leggings"}, {239, "bronze_boots"},
    {240, "steel_helmet"}, {241, "steel_chestplate"}, {242, "steel_leggings"}, {243, "steel_boots"},
    {244, "ruby_helmet"}, {245, "ruby_chestplate"}, {246, "ruby_leggings"}, {247, "ruby_boots"},
    {SPRUCE_LOG, "spruce_log"}, {SPRUCE_LEAVES, "spruce_leaves"}, {SPRUCE_PLANKS, "spruce_planks"}, {ICE, "ice"},
    {CACTUS, "cactus"}, {HARDENED_STONE, "hardened_stone"}, {CHEST, "chest"}, {FENCE, "fence"},
    {OAK_TRAPDOOR, "trapdoor"}, {OAK_DOOR_ITEM, "oak_door"}, {IRON_DOOR_ITEM, "iron_door"}, {BOAT, "boat"},
    {NETHER_GRASS, "nether_grass"}, {NETHER_SPROUTS, "nether_sprouts"}, {VOID_STONE, "void_stone"},
    {VOID_GRASS, "void_grass"}, {VOID_LOG, "void_log"}, {EYE_LOG, "eye_log"}, {VOID_LEAVES, "void_leaves"},
    {RED_SAND, "red_sand"}, {VOID_PORTAL, "void_portal"}, {VOID_STONE_BRICKS, "void_stone_bricks"}, {PUMPKIN, "pumpkin"},
    {CRIMSON_ROOTS, "crimson_roots"}, {VOID_FLINT_AND_STEEL, "void_flint_and_steel"}, {RAW_PORK, "porkchop"},
    {COOKED_PORK, "cooked_porkchop"}, {RAW_MUTTON, "mutton"}, {COOKED_MUTTON, "cooked_mutton"}, {BED_ITEM, "bed"},
    {WORLD_END, "world_end"}, {TNT, "tnt"}, {NUKE_TNT, "nuke_tnt"}, {NETHERPERK_LOG, "netherperk_log"},
    {NETHERPERK_CAP, "netherperk_cap"}, {NETHERPERK_PLANKS, "netherperk_planks"}, {VOID_PLANKS, "void_planks"},
    {OAK_SAPLING, "oak_sapling"}, {PINE_SAPLING, "pine_sapling"}, {VOID_SAPLING, "void_sapling"},
    {NETHERPERK_SAPLING, "netherperk_sapling"}, {POWERSTONE_ORE, "powerstone_ore"}, {POWERSTONE_BLOCK, "powerstone_block"},
    {POWER_CABLE, "power_cable"}, {POWERSTONE_LAMP, "powerstone_lamp"}, {SWITCH, "switch"}, {POWERSTONE_LIGHT, "powerstone_light"},
    {GIFT_BOX, "gift_box"}, {CAKE, "cake"}, {POWERSTONE_DUST, "powerstone_dust"}, {POWERSTONE_CHIP, "powerstone_chip"},
    {CANDY_CANE, "candy_cane"}, {BUCKET, "bucket"}, {WATER_BUCKET, "water_bucket"}, {LAVA_BUCKET, "lava_bucket"},
    {BOSS_COMPASS, "boss_compass"}, {WOOD_SWORD, "wooden_sword"}, {STONE_SWORD, "stone_sword"}, {IRON_SWORD, "iron_sword"},
    {BRONZE_SWORD, "bronze_sword"}, {STEEL_SWORD, "steel_sword"}, {RUBY_SWORD, "ruby_sword"}, {EGG_PIG, "pig_spawn_egg"},
    {EGG_SHEEP, "sheep_spawn_egg"}, {EGG_SLIME, "pumpkin_slime_spawn_egg"}, {EGG_FORGOTTEN, "forgotten_man_spawn_egg"},
    {FARMLAND, "farmland"}, {WHEAT_SEEDS, "wheat_seeds"}, {WHEAT_CROP1, "wheat_crop_1"}, {WHEAT_CROP2, "wheat_crop_2"},
    {WHEAT_RIPE, "wheat_crop_ripe"}, {HAY_BALE, "hay_block"}, {HOE, "hoe"}, {BOW, "bow"}, {ARROW, "arrow"},
    {FEATHER, "feather"}, {WHEAT, "wheat"}, {BREAD, "bread"}, {RAW_CHICKEN, "chicken"}, {COOKED_CHICKEN, "cooked_chicken"},
    {RAW_BEEF, "beef"}, {STEAK, "cooked_beef"}, {MILK_BUCKET, "milk_bucket"}, {EGG_CHICKEN, "chicken_spawn_egg"},
    {EGG_COW, "cow_spawn_egg"}, {EGG_VILLAGER, "villager_spawn_egg"},
    {TAR, "tar"}, {TAR_BLOCK, "tar_block"}, {DRIPSTONE_BLOCK, "dripstone_block"}, {DRIPSTONE_UP, "pointed_dripstone"},
    {DRIPSTONE_DOWN, "pointed_dripstone_down"}, {SNOW_LAYER, "snow_layer"}, {STRING, "string"}, {MAP, "map"}, {TAR_BALL, "tar_ball"},
    {EGG_SPIDER, "spider_spawn_egg"}, {EGG_BAT, "bat_spawn_egg"}, {EGG_TNT_ZOMBIE, "tnt_zombie_spawn_egg"},
    {EGG_TAR_MONSTER, "tar_monster_spawn_egg"},
    {ANNIV_LOGO, "voxelcraft_logo"}, {CONFETTI_BLOCK, "confetti_block"}, {BALLOON, "red_balloon"}, {BALLOON + 1, "blue_balloon"},
    {BALLOON + 2, "yellow_balloon"}, {PARTY_LANTERN, "party_lantern"}, {FESTIVE_BRICKS, "festive_bricks"},
    {PAINTED_PLANKS, "red_painted_planks"}, {PAINTED_PLANKS + 1, "blue_painted_planks"}, {PAINTED_PLANKS + 2, "yellow_painted_planks"},
    {PAINTED_PLANKS + 3, "green_painted_planks"}, {ANNIV_TOKEN, "anniversary_token"}, {ANNIV_MEDAL, "anniversary_medal"},
    {PARTY_POPPER, "party_popper"},
    {POWERSTONE_BRICKS, "powerstone_bricks"}, {CRACKED_POWERSTONE_BRICKS, "cracked_powerstone_bricks"},
    {CHISELED_POWERSTONE_BRICKS, "chiseled_powerstone_bricks"}, {POWERSTONE_GLASS, "powerstone_glass"},
    {DEBRIS_GRAVEL, "debris_gravel"}, {SKELETON_HEAD, "skeleton_head"}, {WASTELAND_GRASS, "wasteland_grass"},
    {DRY_GRASS, "dry_grass"}, {GRASSER_FLOWER, "grasser_flower"}, {FERN, "fern"},
    {CRACKED_NETHER_BRICKS, "cracked_nether_bricks"}, {CHISELED_NETHER_BRICKS, "chiseled_nether_bricks"},
    {SPRUCE_FENCE, "spruce_fence"}, {VOID_FENCE, "void_fence"}, {NETHERPERK_FENCE, "netherperk_fence"},
    {NETHER_BRICK_FENCE, "nether_brick_fence"}, {POWERSTONE_FENCE, "powerstone_fence"},
    {BRUSHER, "brusher"}, {FORTRESS_COMPASS, "fortress_compass"}, {ANCIENT_COIN, "ancient_coin"},
    {POWERSTONE_RELIC, "powerstone_relic"}, {POTTERY_SHARD, "pottery_shard"}, {EGG_HUSKING, "husking_spawn_egg"}, {VOID_BRICK_EYE, "void_brick_eye"}, {CRIMSON_STONE, "crimson_stone"},
    {TORCH, "torch"}, {CRIMSON_PLANKS, "crimson_planks"}, {CRIMSON_LOG, "crimson_log"}, {SHIELD, "shield"}, {EXPLORER_MAP, "explorer_map"}, {WRITABLE_BOOK, "writable_book"}, {DEEPSTONE, "deepstone"}, {HARDIUM_ORE, "hardium_ore"}, {MOSSY_DEEPSTONE, "mossy_deepstone"}, {MOSS_BLOCK, "moss_block"}, {FUNGAL_DEEPSTONE, "fungal_deepstone"}, {DEEPSTONE_BRICKS, "deepstone_bricks"}, {CRACKED_DEEPSTONE_BRICKS, "cracked_deepstone_bricks"}, {MOSSY_DEEPSTONE_BRICKS, "mossy_deepstone_bricks"}, {ROTTEN_PLANKS, "rotten_planks"}, {VINES, "vines"}, {HARDIUM, "hardium"}, {HARDIUM_PICKAXE, "hardium_pickaxe"}, {HARDIUM_SWORD, "hardium_sword"}, {HARDIUM_AXE, "hardium_axe"}, {HARDIUM_SHOVEL, "hardium_shovel"}, {HARDIUM_ARMOR, "hardium_helmet"}, {HARDIUM_ARMOR + 1, "hardium_chestplate"}, {HARDIUM_ARMOR + 2, "hardium_leggings"}, {HARDIUM_ARMOR + 3, "hardium_boots"}, {MOB_SPAWNER, "spawner"}, {SOULED_GRASS, "souled_grass"}, {SOULED_DIRT, "souled_dirt"}, {SOULED_STONE, "souled_stone"}, {CELESTIAL_FRAME, "celestial_portal_frame"}, {CELESTIAL_FRAME_FULL, "celestial_portal_frame_filled"}, {CELESTIAL_PORTAL, "celestial_portal"}, {CRIMSON_HEAD, "crimson_head"}, {CELESTIAL_COMPASS, "celestial_compass"}, {CELESTIAL_LOG, "celestial_log"}, {CELESTIAL_PLANKS, "celestial_planks"}, {CELESTIAL_STONE_BRICKS, "celestial_stone_bricks"}, {CELESTIAL_LEAVES, "celestial_leaves"}, {CELESTIAL_GRASS, "celestial_grass"}, {CELESTIAL_DIRT, "celestial_dirt"}, {CELESTIAL_STONE, "celestial_stone"}, {DUSTY_CONCRETE, "dusty_concrete"}, {HARDENED_CONCRETE, "hardened_concrete"}, {GRAY_LOG, "gray_log"}, {GRAY_PLANKS, "gray_planks"}, {RED_LEAVES, "red_leaves"}, {ORANGE_LEAVES, "orange_leaves"}, {YELLOW_LEAVES, "yellow_leaves"}, {CUTTER, "stonecutter"}, {CRIMSONIUM_LOG, "crimsonium_log"}, {CRIMSONIUM_PLANKS, "crimsonium_planks"}, {CRIMSONIUM_LEAVES, "crimsonium_leaves"}, {SHATTERED_STONE, "shattered_stone"}, {SHATTERED_BRICKS, "shattered_bricks"}, {VOID_SHARD_BLOCK, "void_shard_block"}, {DARK_SAND, "dark_sand"}, {DARK_SANDSTONE, "dark_sandstone"}, {SHARD_CLUSTER, "shard_cluster"}, {CRIMSONIUM_ORE, "crimsonium_ore"}, {VOID_SHARD, "void_shard"}, {CRIMSONIUM, "crimsonium"}, {LOOT_CHEST_SHRINE, "shrine_chest"}, {LOOT_CHEST_CHAMBER, "chamber_chest"}, {LOOT_BARREL_CAMP, "camp_barrel"}, {WOOD_CUTTER, "wood_cutter"}, {GRAY_SAPLING, "gray_sapling"}, {CELESTIAL_SAPLING, "celestial_sapling"}, {BARREL, "barrel"}, {WRITTEN_BOOK, "written_book"},
    {EGG_HOGMAN, "hogman_spawn_egg"}, {POWERSTONE_FURNACE, "powerstone_furnace"}, {POWERSTONE_FURNACE_ON, "lit_powerstone_furnace"},
    {CEILING_CABLE, "ceiling_power_cable"}, {CEILING_CABLE_ON, "lit_ceiling_power_cable"},
};
}  // namespace

// Stairs and slabs, by material (see shapeMaterialBlock).
const char* const kStairsKeys[kShapeMaterials] = {"oak_stairs", "spruce_stairs", "void_stairs", "netherperk_stairs", "cobblestone_stairs", "stone_stairs", "stone_brick_stairs", "mossy_cobblestone_stairs", "brick_stairs", "sandstone_stairs", "nether_brick_stairs", "void_brick_stairs", "powerstone_stairs", "crimson_stairs", "deepstone_stairs", "deepstone_brick_stairs", "mossy_deepstone_brick_stairs", "dusty_concrete_stairs", "hardened_concrete_stairs", "gray_stairs", "celestial_stairs", "celestial_brick_stairs", "hardened_stone_stairs", "void_stone_stairs", "netherrack_stairs", "obsidian_stairs", "crimson_stone_stairs", "souled_stone_stairs", "celestial_stone_stairs", "dripstone_stairs", "tar_stairs", "cracked_powerstone_brick_stairs", "cracked_nether_brick_stairs", "cracked_deepstone_brick_stairs", "rotten_stairs", "mossy_deepstone_stairs", "chiseled_powerstone_brick_stairs", "crimsonium_stairs", "shattered_stone_stairs", "shattered_brick_stairs", "dark_sandstone_stairs"};
const char* const kSlabKeys[kShapeMaterials] = {"oak_slab", "spruce_slab", "void_slab", "netherperk_slab", "cobblestone_slab", "stone_slab", "stone_brick_slab", "mossy_cobblestone_slab", "brick_slab", "sandstone_slab", "nether_brick_slab", "void_brick_slab", "powerstone_slab", "crimson_slab", "deepstone_slab", "deepstone_brick_slab", "mossy_deepstone_brick_slab", "dusty_concrete_slab", "hardened_concrete_slab", "gray_slab", "celestial_slab", "celestial_brick_slab", "hardened_stone_slab", "void_stone_slab", "netherrack_slab", "obsidian_slab", "crimson_stone_slab", "souled_stone_slab", "celestial_stone_slab", "dripstone_slab", "tar_slab", "cracked_powerstone_brick_slab", "cracked_nether_brick_slab", "cracked_deepstone_brick_slab", "rotten_slab", "mossy_deepstone_slab", "chiseled_powerstone_brick_slab", "crimsonium_slab", "shattered_stone_slab", "shattered_brick_slab", "dark_sandstone_slab"};
const char* const kStairsNames[kShapeMaterials] = {"Dubové schody", "Smrkové schody", "Prázdnotné schody", "Netherperkové schody", "Schody z dlažebního kamene", "Kamenné schody", "Schody z kamenných cihel", "Schody z mechového kamene", "Cihlové schody", "Pískovcové schody", "Schody z netherových cihel", "Schody z prázdnotných cihel", "Powerstonové schody", "Karmínové schody", "Deepstonové schody", "Schody z deepstonových cihel", "Schody z mechových deepstonových cihel", "Schody z prašného betonu", "Schody z tvrzeného betonu", "Šedé dřevěné schody", "Nebeské schody", "Schody z nebeských cihel", "Schody z tvrzeného kamene", "Schody z prázdnotného kamene", "Netherrackové schody", "Obsidiánové schody", "Schody z karmínového kamene", "Schody z duševního kamene", "Schody z nebeského kamene", "Krápníkové schody", "Schody z tvrdého dehtu", "Schody z popraskaných powerstonových cihel", "Schody z popraskaných netherových cihel", "Schody z popraskaných deepstonových cihel", "Schody z trouchnivých prken", "Schody z mechového deepstonu", "Schody z vytesaných powerstonových cihel", "Crimsoniové schody", "Schody z roztříštěného kamene", "Schody z cihel roztříštěného kamene", "Schody z temného pískovce"};
const char* const kSlabNames[kShapeMaterials] = {"Dubová deska", "Smrková deska", "Prázdnotná deska", "Netherperková deska", "Deska z dlažebního kamene", "Kamenná deska", "Deska z kamenných cihel", "Deska z mechového kamene", "Cihlová deska", "Pískovcová deska", "Deska z netherových cihel", "Deska z prázdnotných cihel", "Powerstonová deska", "Karmínová deska", "Deepstonová deska", "Deska z deepstonových cihel", "Deska z mechových deepstonových cihel", "Deska z prašného betonu", "Deska z tvrzeného betonu", "Šedá dřevěná deska", "Nebeská deska", "Deska z nebeských cihel", "Deska z tvrzeného kamene", "Deska z prázdnotného kamene", "Netherracková deska", "Obsidiánová deska", "Deska z karmínového kamene", "Deska z duševního kamene", "Deska z nebeského kamene", "Krápníková deska", "Deska z tvrdého dehtu", "Deska z popraskaných powerstonových cihel", "Deska z popraskaných netherových cihel", "Deska z popraskaných deepstonových cihel", "Deska z trouchnivých prken", "Deska z mechového deepstonu", "Deska z vytesaných powerstonových cihel", "Crimsoniová deska", "Deska z roztříštěného kamene", "Deska z cihel roztříštěného kamene", "Deska z temného pískovce"};

const char* const kGlassNames[16] = {"Bílé sklo", "Oranžové sklo", "Purpurové sklo", "Světle modré sklo", "Žluté sklo", "Limetkové sklo", "Růžové sklo", "Šedé sklo", "Světle šedé sklo", "Azurové sklo", "Fialové sklo", "Modré sklo", "Hnědé sklo", "Zelené sklo", "Červené sklo", "Černé sklo"};
const char* const kConcretePowderKeys[16] = {"white_concrete_powder", "orange_concrete_powder", "magenta_concrete_powder", "light_blue_concrete_powder", "yellow_concrete_powder", "lime_concrete_powder", "pink_concrete_powder", "gray_concrete_powder", "light_gray_concrete_powder", "cyan_concrete_powder", "purple_concrete_powder", "blue_concrete_powder", "brown_concrete_powder", "green_concrete_powder", "red_concrete_powder", "black_concrete_powder"};
const char* const kConcretePowderNames[16] = {"Bílý betonový prášek", "Oranžový betonový prášek", "Purpurový betonový prášek", "Světle modrý betonový prášek", "Žlutý betonový prášek", "Limetkový betonový prášek", "Růžový betonový prášek", "Šedý betonový prášek", "Světle šedý betonový prášek", "Azurový betonový prášek", "Fialový betonový prášek", "Modrý betonový prášek", "Hnědý betonový prášek", "Zelený betonový prášek", "Červený betonový prášek", "Černý betonový prášek"};
const char* const kConcreteKeys[16] = {"white_concrete", "orange_concrete", "magenta_concrete", "light_blue_concrete", "yellow_concrete", "lime_concrete", "pink_concrete", "gray_concrete", "light_gray_concrete", "cyan_concrete", "purple_concrete", "blue_concrete", "brown_concrete", "green_concrete", "red_concrete", "black_concrete"};
const char* const kConcreteNames[16] = {"Bílý beton", "Oranžový beton", "Purpurový beton", "Světle modrý beton", "Žlutý beton", "Limetkový beton", "Růžový beton", "Šedý beton", "Světle šedý beton", "Azurový beton", "Fialový beton", "Modrý beton", "Hnědý beton", "Zelený beton", "Červený beton", "Černý beton"};
const char* const kCarpetKeys[16] = {"white_carpet", "orange_carpet", "magenta_carpet", "light_blue_carpet", "yellow_carpet", "lime_carpet", "pink_carpet", "gray_carpet", "light_gray_carpet", "cyan_carpet", "purple_carpet", "blue_carpet", "brown_carpet", "green_carpet", "red_carpet", "black_carpet"};
const char* const kCarpetNames[16] = {"Bílý koberec", "Oranžový koberec", "Purpurový koberec", "Světle modrý koberec", "Žlutý koberec", "Limetkový koberec", "Růžový koberec", "Šedý koberec", "Světle šedý koberec", "Azurový koberec", "Fialový koberec", "Modrý koberec", "Hnědý koberec", "Zelený koberec", "Červený koberec", "Černý koberec"};
const char* const kDyeKeys[16] = {"white_dye", "orange_dye", "magenta_dye", "light_blue_dye", "yellow_dye", "lime_dye", "pink_dye", "gray_dye", "light_gray_dye", "cyan_dye", "purple_dye", "blue_dye", "brown_dye", "green_dye", "red_dye", "black_dye"};
const char* const kDyeNames[16] = {"Bílé barvivo", "Oranžové barvivo", "Purpurové barvivo", "Světle modré barvivo", "Žluté barvivo", "Limetkové barvivo", "Růžové barvivo", "Šedé barvivo", "Světle šedé barvivo", "Azurové barvivo", "Fialové barvivo", "Modré barvivo", "Hnědé barvivo", "Zelené barvivo", "Červené barvivo", "Černé barvivo"};
const char* const kGlassKeys[16] = {"white_stained_glass", "orange_stained_glass", "magenta_stained_glass", "light_blue_stained_glass", "yellow_stained_glass", "lime_stained_glass", "pink_stained_glass", "gray_stained_glass", "light_gray_stained_glass", "cyan_stained_glass", "purple_stained_glass", "blue_stained_glass", "brown_stained_glass", "green_stained_glass", "red_stained_glass", "black_stained_glass"};

const char* const kDocumentKeys[kDocumentPages] = {"old_document_1", "old_document_2", "old_document_3", "old_document_4", "old_document_5", "old_document_6", "old_document_7", "old_document_8"};
const char* const kDocumentNames[kDocumentPages] = {"Starý dokument I", "Starý dokument II", "Starý dokument III", "Starý dokument IV", "Starý dokument V", "Starý dokument VI", "Starý dokument VII", "Starý dokument VIII"};

const char* itemKey(ItemId id) {
    if (id >= kModIdBase) {
        const char* k = ModRegistry::get().key(id);
        return k ? k : "air";
    }
    if (isStainedGlass(id)) return kGlassKeys[id - STAINED_GLASS];
    if (id >= CONCRETE_POWDER && id < CONCRETE_POWDER + 16) return kConcretePowderKeys[id - CONCRETE_POWDER];
    if (isColorConcrete(id)) return kConcreteKeys[id - CONCRETE];
    if (isCarpet(id)) return kCarpetKeys[id - CARPET];
    if (isDye(id)) return kDyeKeys[id - DYE];
    if (isStairs(id)) return kStairsKeys[stairsMaterial(id)];
    if (isDocument(id)) return kDocumentKeys[id - OLD_DOCUMENT];
    if (isSkeletonHead(id)) return "skeleton_head";
    if (isTorch(id)) return "torch";
    if (isSlab(id)) return kSlabKeys[slabMaterial(id)];
    for (const KeyEntry& e : kKeys)
        if (e.id == id) return e.key;
    return "air";
}

ItemId itemFromKey(const std::string& key) {
    // Accept an optional "minecraft:"-style namespace and a few common aliases.
    std::string k = key;
    // Mod content is named with its own namespace, "mymod:ruby_bricks".
    if (k.find(':') != std::string::npos && k.rfind("minecraft:", 0) != 0)
        if (ItemId mod = ModRegistry::get().find(k); mod != AIR) return mod;
    if (size_t colon = k.find(':'); colon != std::string::npos) k = k.substr(colon + 1);
    if (k == "oak_log") k = "log";
    if (k == "oak_planks") k = "planks";
    if (k == "oak_leaves") k = "leaves";
    if (k == "grass_block") k = "grass";
    if (k == "tallgrass" || k == "short_grass") k = "tall_grass";
    if (k == "yellow_flower") k = "dandelion";
    if (k == "red_flower") k = "poppy";
    if (k == "wool") k = "white_wool";
    if (k == "stonebrick") k = "stone_bricks";
    if (k == "snow_block") k = "snow";
    if (k == "clay_block") k = "clay";
    if (k == "oak_fence" || k == "fence") k = "fence";
    if (k == "oak_trapdoor" || k == "wooden_trapdoor") k = "trapdoor";
    if (k == "wooden_door") k = "oak_door";
    if (k == "oak_boat") k = "boat";
    if (k == "red_bed" || k == "white_bed") k = "bed";
    if (k == "raw_porkchop") k = "porkchop";
    if (k == "spruce_sapling") k = "pine_sapling";
    if (k == "cornerstone") k = "netherrack";
    if (k == "cornerstone_grass") k = "nether_grass";
    if (k == "grasser") k = "nether_sprouts";
    if (k == "raw_mutton") k = "mutton";
    // Removed in 1.2.0: old packs still name them.
    if (k == "soul_sand") k = "nether_grass";
    if (k == "glowstone") k = "powerstone_light";
    if (k == "quartz_ore" || k == "nether_quartz_ore") k = "powerstone_ore";
    if (k == "seeds" || k == "wheat_crop") k = "wheat_seeds";
    if (k == "hay_bale" || k == "hay") k = "hay_block";
    if (k == "steak") k = "cooked_beef";
    if (k == "iron_hoe" || k == "wooden_hoe" || k == "stone_hoe") k = "hoe";
    if (k == "stone_brick_slab" || k == "stone_bricks_slab") k = "stone_brick_slab";
    if (k == "nether_bricks_stairs") k = "nether_brick_stairs";
    if (k == "dripstone" || k == "pointed_dripstone") k = "pointed_dripstone";
    if (k == "filled_map" || k == "empty_map") k = "map";
    if (k == "suspicious_gravel") k = "debris_gravel";
    if (k == "skeleton_skull") k = "skeleton_head";
    if (k == "brush") k = "brusher";
    if (k == "short_dry_grass" || k == "tall_dry_grass") k = "dry_grass";
    if (k == "old_document") k = "old_document_1";
    for (int c = 0; c < 16; ++c)
        if (k == kGlassKeys[c]) return (ItemId)(STAINED_GLASS + c);
    for (int c = 0; c < 16; ++c) {
        if (k == kConcretePowderKeys[c]) return (ItemId)(CONCRETE_POWDER + c);
        if (k == kConcreteKeys[c]) return (ItemId)(CONCRETE + c);
        if (k == kCarpetKeys[c]) return (ItemId)(CARPET + c);
        if (k == kDyeKeys[c]) return (ItemId)(DYE + c);
    }
    for (int p = 0; p < kDocumentPages; ++p)
        if (k == kDocumentKeys[p]) return (ItemId)(OLD_DOCUMENT + p);
    for (int m = 0; m < kShapeMaterials; ++m) {
        if (k == kStairsKeys[m]) return stairsId(m, 0, false);
        if (k == kSlabKeys[m]) return slabId(m, 0);
    }
    for (const KeyEntry& e : kKeys)
        if (k == e.key) return e.id;
    return AIR;
}

const char* itemName(ItemId id) {
    if (id >= kModIdBase) {
        const char* n = ModRegistry::get().name(id);
        return n ? n : "";
    }
    if (isStairs(id)) return kStairsNames[stairsMaterial(id)];
    if (isDocument(id)) return kDocumentNames[id - OLD_DOCUMENT];
    if (isSkeletonHead(id)) return "Lebka kostlivce";
    if (isSlab(id)) return kSlabNames[slabMaterial(id)];
    if (isStainedGlass(id)) return kGlassNames[id - STAINED_GLASS];
    if (id >= CONCRETE_POWDER && id < CONCRETE_POWDER + 16) return kConcretePowderNames[id - CONCRETE_POWDER];
    if (isColorConcrete(id)) return kConcreteNames[id - CONCRETE];
    if (isCarpet(id)) return kCarpetNames[id - CARPET];
    if (isDye(id)) return kDyeNames[id - DYE];
    if (isWallCable(id) || isCeilingCable(id)) return "Napájecí kabel";
    if (isWallSwitch(id)) return "Vypínač";
    switch (id) {
        case TAR: return "Dehet";
        case TAR_BLOCK: return "Ztuhlý dehet";
        case DRIPSTONE_BLOCK: return "Krápníkový blok";
        case DRIPSTONE_UP: case DRIPSTONE_DOWN: return "Krápník";
        case SNOW_LAYER: return "Sněhová pokrývka";
        case STRING: return "Provázek";
        case MAP: return "Mapa";
        case TAR_BALL: return "Dehtová koule";
        case EGG_SPIDER: return "Vejce: pavouk";
        case EGG_BAT: return "Vejce: netopýr";
        case EGG_TNT_ZOMBIE: return "Vejce: TNT zombie";
        case EGG_TAR_MONSTER: return "Vejce: dehtová příšera";
        case ANNIV_LOGO: return "Logo VoxelCraftu";
        case CONFETTI_BLOCK: return "Blok konfet";
        case BALLOON: return "Červený balónek";
        case BALLOON + 1: return "Modrý balónek";
        case BALLOON + 2: return "Žlutý balónek";
        case PARTY_LANTERN: return "Slavnostní lucerna";
        case FESTIVE_BRICKS: return "Slavnostní cihly";
        case PAINTED_PLANKS: return "Červená prkna";
        case PAINTED_PLANKS + 1: return "Modrá prkna";
        case PAINTED_PLANKS + 2: return "Žlutá prkna";
        case PAINTED_PLANKS + 3: return "Zelená prkna";
        case ANNIV_TOKEN: return "Výroční žeton";
        case ANNIV_MEDAL: return "Výroční medaile";
        case PARTY_POPPER: return "Vystřelovací konfety";
        case POWERSTONE_BRICKS: return "Powerstonové cihly";
        case CRACKED_POWERSTONE_BRICKS: return "Popraskané powerstonové cihly";
        case CHISELED_POWERSTONE_BRICKS: return "Zdobené powerstonové cihly";
        case POWERSTONE_GLASS: return "Powerstonové sklo";
        case DEBRIS_GRAVEL: return "Štěrk s nálezy";
        case WASTELAND_GRASS: return "Pustá tráva";
        case DRY_GRASS: return "Suchá tráva";
        case GRASSER_FLOWER: return "Květ Grasser";
        case FERN: return "Kapradí";
        case CRACKED_NETHER_BRICKS: return "Popraskané netherové cihly";
        case CHISELED_NETHER_BRICKS: return "Zdobené netherové cihly";
        case LOOT_CHEST_FORGOTTEN: case LOOT_CHEST_DROWNED: case LOOT_CHEST_PORTAL: return "Truhla";
        case SPRUCE_FENCE: return "Smrkový plot";
        case VOID_FENCE: return "Prázdnotný plot";
        case NETHERPERK_FENCE: return "Netherperkový plot";
        case NETHER_BRICK_FENCE: return "Plot z netherových cihel";
        case POWERSTONE_FENCE: return "Powerstonový plot";
        case BRUSHER: return "Štětec";
        case FORTRESS_COMPASS: return "Kompas k pevnosti";
        case ANCIENT_COIN: return "Starověká mince";
        case POWERSTONE_RELIC: return "Powerstonová relikvie";
        case POTTERY_SHARD: return "Střep";
        case EGG_HUSKING: return "Vejce: Husking";
        case VOID_BRICK_EYE: return "Oko v prázdnotných cihlách";
        case CRIMSON_STONE: return "Karmínový kámen";
        case TORCH: return "Pochodeň";
        case CRIMSON_PLANKS: return "Karmínová prkna";
        case CRIMSON_LOG: return "Karmínový kmen";
        case SHIELD: return "Štít";
        case EXPLORER_MAP: return "Průzkumná mapa";
        case WRITABLE_BOOK: return "Kniha a brk";
        case DEEPSTONE: return "Hlubinný kámen";
        case HARDIUM_ORE: return "Hardiová ruda";
        case MOSSY_DEEPSTONE: return "Mechový hlubinný kámen";
        case MOSS_BLOCK: return "Blok mechu";
        case FUNGAL_DEEPSTONE: return "Houbový hlubinný kámen";
        case DEEPSTONE_BRICKS: return "Cihly z hlubinného kamene";
        case CRACKED_DEEPSTONE_BRICKS: return "Popraskané cihly z hlubinného kamene";
        case MOSSY_DEEPSTONE_BRICKS: return "Mechové cihly z hlubinného kamene";
        case ROTTEN_PLANKS: return "Shnilá prkna";
        case VINES: return "Liány";
        case HARDIUM: return "Hardium";
        case HARDIUM_PICKAXE: return "Hardiový krumpáč";
        case HARDIUM_SWORD: return "Hardiový meč";
        case HARDIUM_AXE: return "Hardiová sekera";
        case HARDIUM_SHOVEL: return "Hardiová lopata";
        case MOB_SPAWNER: return "Líheň příšer";
        case SOULED_GRASS: return "Duševní tráva";
        case SOULED_DIRT: return "Duševní hlína";
        case SOULED_STONE: return "Duševní kámen";
        case CELESTIAL_FRAME: return "Rám nebeského portálu";
        case CELESTIAL_FRAME_FULL: return "Rám nebeského portálu";
        case CELESTIAL_PORTAL: return "Nebeský portál";
        case CELESTIAL_LOG: return "Nebeský kmen";
        case CELESTIAL_PLANKS: return "Nebeská prkna";
        case CELESTIAL_STONE_BRICKS: return "Nebeské kamenné cihly";
        case CELESTIAL_LEAVES: return "Nebeské listí";
        case CELESTIAL_GRASS: return "Nebeská duševní tráva";
        case CELESTIAL_DIRT: return "Nebeská duševní hlína";
        case CELESTIAL_STONE: return "Nebeský duševní kámen";
        case DUSTY_CONCRETE: return "Prašný beton";
        case HARDENED_CONCRETE: return "Tvrzený beton";
        case GRAY_LOG: return "Šedý kmen";
        case GRAY_PLANKS: return "Šedá prkna";
        case RED_LEAVES: return "Červené listí";
        case ORANGE_LEAVES: return "Oranžové listí";
        case YELLOW_LEAVES: return "Žluté listí";
        case CUTTER: return "Kamenořez";
        case CRIMSONIUM_LOG: return "Crimsoniový kmen";
        case CRIMSONIUM_PLANKS: return "Crimsoniová prkna";
        case CRIMSONIUM_LEAVES: return "Crimsoniové listí";
        case SHATTERED_STONE: return "Roztříštěný kámen";
        case SHATTERED_BRICKS: return "Cihly z roztříštěného kamene";
        case VOID_SHARD_BLOCK: return "Blok prázdnotných střepů";
        case DARK_SAND: return "Temný písek";
        case DARK_SANDSTONE: return "Temný pískovec";
        case SHARD_CLUSTER: return "Shluk střepů";
        case CRIMSONIUM_ORE: return "Crimsoniová ruda";
        case VOID_SHARD: return "Prázdnotný střep";
        case CRIMSONIUM: return "Crimsonium";
        case LOOT_CHEST_SHRINE: return "Truhla svatyně";
        case LOOT_CHEST_CHAMBER: return "Truhla skryté komory";
        case LOOT_BARREL_CAMP: return "Barel z tábora";
        case WOOD_CUTTER: return "Dřevořez";
        case GRAY_SAPLING: return "Šedá sazenice";
        case CELESTIAL_SAPLING: return "Nebeská sazenice";
        case BARREL: return "Barel";
        case CRIMSON_HEAD: return "Hlava Karmínového muže";
        case CELESTIAL_COMPASS: return "Nebeský kompas";
        case WRITTEN_BOOK: return "Podepsaná kniha";
        case EGG_HOGMAN: return "Vejce: Prasák";
        case POWERSTONE_FURNACE: case POWERSTONE_FURNACE_ON: return "Powerstonová pec";
        default: if (isTorch(id)) return "Pochodeň"; break;
    }
    switch (id) {
        case STONE: return "Kámen";
        case GRASS: return "Travnatý blok";
        case TALL_GRASS: return "Vysoká tráva";
        case DANDELION: return "Pampeliška";
        case POPPY: return "Mák";
        case DIRT: return "Hlína";
        case COBBLESTONE: return "Dlažební kámen";
        case PLANKS: return "Dubová prkna";
        case BEDROCK: return "Podloží";
        case WATER: return "Voda";
        case LAVA: return "Láva";
        case SAND: return "Písek";
        case GRAVEL: return "Štěrk";
        case IRON_ORE: return "Železná ruda";
        case COAL_ORE: return "Uhelná ruda";
        case LOG: return "Dubový kmen";
        case LEAVES: return "Dubové listí";
        case GLASS: return "Sklo";
        case BRICKS: return "Cihly";
        case OBSIDIAN: return "Obsidián";
        case CRAFTING_TABLE: return "Pracovní stůl";
        case FURNACE: case LIT_FURNACE: return "Pec";
        case NETHERRACK: return "Úhelný kámen";
        case NETHER_BRICK: return "Netherové cihly";
        case FARMLAND: return "Ornice";
        case WHEAT_SEEDS: return "Semínka pšenice";
        case WHEAT_CROP1: case WHEAT_CROP2: return "Rostoucí pšenice";
        case WHEAT_RIPE: return "Zralá pšenice";
        case HAY_BALE: return "Balík sena";
        case HOE: return "Motyka";
        case BOW: return "Luk";
        case ARROW: return "Šíp";
        case FEATHER: return "Pírko";
        case WHEAT: return "Pšenice";
        case BREAD: return "Chléb";
        case RAW_CHICKEN: return "Syrové kuře";
        case COOKED_CHICKEN: return "Pečené kuře";
        case RAW_BEEF: return "Syrové hovězí";
        case STEAK: return "Steak";
        case MILK_BUCKET: return "Kbelík mléka";
        case EGG_CHICKEN: return "Vejce: slepice";
        case EGG_COW: return "Vejce: kráva";
        case EGG_VILLAGER: return "Vejce: vesničan";
        case STICK: return "Klacek";
        case COAL: return "Uhlí";
        case IRON_INGOT: return "Železný ingot";
        case FLINT: return "Pazourek";
        case WOOD_PICKAXE: return "Dřevěný krumpáč";
        case STONE_PICKAXE: return "Kamenný krumpáč";
        case IRON_PICKAXE: return "Železný krumpáč";
        case WOOD_AXE: return "Dřevěná sekera";
        case STONE_AXE: return "Kamenná sekera";
        case IRON_AXE: return "Železná sekera";
        case WOOD_SHOVEL: return "Dřevěná lopata";
        case STONE_SHOVEL: return "Kamenná lopata";
        case IRON_SHOVEL: return "Železná lopata";
        case APPLE: return "Jablko";
        case FLINT_AND_STEEL: return "Křesadlo";
        case SANDSTONE: return "Pískovec";
        case BOOKSHELF: return "Knihovna";
        case MOSSY_COBBLESTONE: return "Mechový dlažební kámen";
        case SNOW_BLOCK: return "Sněhový blok";
        case CLAY: return "Jíl";
        case STONE_BRICKS: return "Kamenné cihly";
        case BRONZE_ORE: return "Bronzová ruda";
        case STEEL_ORE: return "Ocelová ruda";
        case RUBY_ORE: return "Rubínová ruda";
        case BRONZE_BLOCK: return "Bronzový blok";
        case STEEL_BLOCK: return "Ocelový blok";
        case RUBY_BLOCK: return "Rubínový blok";
        case BRONZE_INGOT: return "Bronzový ingot";
        case STEEL_INGOT: return "Ocelový ingot";
        case RUBY: return "Rubín";
        case BRONZE_PICKAXE: return "Bronzový krumpáč";
        case BRONZE_AXE: return "Bronzová sekera";
        case BRONZE_SHOVEL: return "Bronzová lopata";
        case STEEL_PICKAXE: return "Ocelový krumpáč";
        case STEEL_AXE: return "Ocelová sekera";
        case STEEL_SHOVEL: return "Ocelová lopata";
        case RUBY_PICKAXE: return "Rubínový krumpáč";
        case RUBY_AXE: return "Rubínová sekera";
        case RUBY_SHOVEL: return "Rubínová lopata";
        case SPRUCE_LOG: return "Smrkový kmen";
        case SPRUCE_LEAVES: return "Smrkové jehličí";
        case SPRUCE_PLANKS: return "Smrková prkna";
        case ICE: return "Led";
        case CACTUS: return "Kaktus";
        case HARDENED_STONE: return "Tvrdý kámen";
        case CHEST: case LOOT_CHEST_RUIN: case LOOT_CHEST_SHIP: case LOOT_CHEST_DUNGEON: case LOOT_CHEST_MINE: return "Truhla";
        case FENCE: return "Plot";
        case OAK_DOOR_ITEM: return "Dubové dveře";
        case IRON_DOOR_ITEM: return "Železné dveře";
        case BOAT: return "Loďka";
        case NETHER_GRASS: return "Úhelná tráva";
        case NETHER_SPROUTS: return "Grasser";
        case VOID_STONE: return "Prázdnotný kámen";
        case VOID_GRASS: return "Fialová tráva";
        case VOID_LOG: return "Prázdnotný kmen";
        case EYE_LOG: return "Oční kmen";
        case VOID_LEAVES: return "Prázdnotné listí";
        case RED_SAND: return "Rudý písek";
        case VOID_PORTAL: return "Prázdnotný portál";
        case VOID_STONE_BRICKS: return "Prázdnotné cihly";
        case PUMPKIN: return "Dýně";
        case CRIMSON_ROOTS: return "Karmínové kořeny";
        case LOOT_CHEST_FORTRESS: return "Truhla";
        case VOID_FLINT_AND_STEEL: return "Prázdnotné křesadlo";
        case RAW_PORK: return "Syrové vepřové";
        case COOKED_PORK: return "Pečené vepřové";
        case RAW_MUTTON: return "Syrové skopové";
        case COOKED_MUTTON: return "Pečené skopové";
        case BED_ITEM: return "Postel";
        case WORLD_END: return "Konec světa";
        case TNT: return "TNT";
        case NUKE_TNT: return "Jaderné TNT";
        case NETHERPERK_LOG: return "Netherperkový kmen";
        case NETHERPERK_CAP: return "Netherperková koruna";
        case NETHERPERK_PLANKS: return "Netherperková prkna";
        case VOID_PLANKS: return "Prázdnotná prkna";
        case OAK_SAPLING: return "Dubová sazenice";
        case PINE_SAPLING: return "Borová sazenice";
        case VOID_SAPLING: return "Prázdnotná sazenice";
        case NETHERPERK_SAPLING: return "Netherperková sazenice";
        case POWERSTONE_ORE: return "Powerstonová ruda";
        case POWERSTONE_BLOCK: return "Powerstonový blok";
        case POWER_CABLE: case POWER_CABLE_ON: return "Napájecí kabel";
        case POWERSTONE_LAMP: case POWERSTONE_LAMP_ON: return "Powerstonová lampa";
        case SWITCH: case SWITCH_ON: return "Vypínač";
        case POWERSTONE_LIGHT: return "Powerstonové světlo";
        case GIFT_BOX: return "Dárek";
        case CAKE: return "Dort";
        case LOOT_CHEST_VOID: case LOOT_CHEST_TEMPLE: case LOOT_CHEST_IGLOO: case LOOT_CHEST_TOWER: return "Truhla";
        case POWERSTONE_DUST: return "Powerstonový prach";
        case POWERSTONE_CHIP: return "Powerstonový čip";
        case CANDY_CANE: return "Cukrová hůlka";
        case BUCKET: return "Kbelík";
        case WATER_BUCKET: return "Kbelík vody";
        case LAVA_BUCKET: return "Kbelík lávy";
        case BOSS_COMPASS: return "Kompas k aréně";
        case WOOD_SWORD: return "Dřevěný meč";
        case STONE_SWORD: return "Kamenný meč";
        case IRON_SWORD: return "Železný meč";
        case BRONZE_SWORD: return "Bronzový meč";
        case STEEL_SWORD: return "Ocelový meč";
        case RUBY_SWORD: return "Rubínový meč";
        case EGG_PIG: return "Vejce: prase";
        case EGG_SHEEP: return "Vejce: ovce";
        case EGG_SLIME: return "Vejce: dýňový sliz";
        case EGG_FORGOTTEN: return "Vejce: zapomenutý muž";
        default: break;
    }
    if (isDoor(id)) return id >= IRON_DOOR ? "Železné dveře" : "Dubové dveře";
    if (isTrapdoor(id)) return "Dubový poklop";
    if (isBed(id)) return "Postel";
    if (int c = woolColor(id); c >= 0) {
        static const char* kWool[16] = {"Bílá vlna", "Oranžová vlna", "Purpurová vlna", "Světle modrá vlna",
                                        "Žlutá vlna", "Limetková vlna", "Růžová vlna", "Šedá vlna",
                                        "Světle šedá vlna", "Azurová vlna", "Fialová vlna", "Modrá vlna",
                                        "Hnědá vlna", "Zelená vlna", "Červená vlna", "Černá vlna"};
        return kWool[c];
    }
    if (isArmor(id)) {
        static const char* kArmor[6][4] = {
            {"Dřevěná helma", "Dřevěný kyrys", "Dřevěné nohavice", "Dřevěné boty"},
            {"Kamenná helma", "Kamenný kyrys", "Kamenné nohavice", "Kamenné boty"},
            {"Bronzová helma", "Bronzový kyrys", "Bronzové nohavice", "Bronzové boty"},
            {"Ocelová helma", "Ocelový kyrys", "Ocelové nohavice", "Ocelové boty"},
            {"Rubínová helma", "Rubínový kyrys", "Rubínové nohavice", "Rubínové boty"},
            {"Hardiová helma", "Hardiový kyrys", "Hardiové nohavice", "Hardiové boty"},
        };
        return kArmor[armorMaterial(id)][armorPiece(id)];
    }
    switch (id) {
        default: return "";
    }
}

ToolKind toolKind(ItemId id) {
    switch (id) {
        case WOOD_PICKAXE: case STONE_PICKAXE: case IRON_PICKAXE: case HARDIUM_PICKAXE: return ToolKind::Pickaxe;
        case WOOD_AXE: case STONE_AXE: case IRON_AXE: case HARDIUM_AXE: return ToolKind::Axe;
        case WOOD_SHOVEL: case STONE_SHOVEL: case IRON_SHOVEL: case HARDIUM_SHOVEL: return ToolKind::Shovel;
        default:
            if (id >= BRONZE_PICKAXE && id <= RUBY_SHOVEL)
                return (id - BRONZE_PICKAXE) % 3 == 0 ? ToolKind::Pickaxe : (id - BRONZE_PICKAXE) % 3 == 1 ? ToolKind::Axe : ToolKind::Shovel;
            if (isSword(id)) return ToolKind::Sword;
            return ToolKind::None;
    }
}

int toolTier(ItemId id) {
    switch (id) {
        case WOOD_PICKAXE: case WOOD_AXE: case WOOD_SHOVEL: return 1;
        case STONE_PICKAXE: case STONE_AXE: case STONE_SHOVEL: return 2;
        case IRON_PICKAXE: case IRON_AXE: case IRON_SHOVEL: return 3;
        case BRONZE_PICKAXE: case BRONZE_AXE: case BRONZE_SHOVEL: return 3;  // like iron, but tougher
        case STEEL_PICKAXE: case STEEL_AXE: case STEEL_SHOVEL: return 4;
        case RUBY_PICKAXE: case RUBY_AXE: case RUBY_SHOVEL: return 5;
        case HARDIUM_PICKAXE: case HARDIUM_AXE: case HARDIUM_SHOVEL: case HARDIUM_SWORD: return 6;
        case WOOD_SWORD: return 1;
        case STONE_SWORD: return 2;
        case IRON_SWORD: case BRONZE_SWORD: return 3;
        case STEEL_SWORD: return 4;
        case RUBY_SWORD: return 5;
        default: return 0;
    }
}

int toolDurability(ItemId id) {
    if (isArmor(id)) {
        static const int kBase[6] = {5, 8, 12, 18, 33, 45}, kPiece[4] = {11, 16, 15, 13};
        return kBase[armorMaterial(id)] * kPiece[armorPiece(id)];
    }
    if ((id >= BRONZE_PICKAXE && id <= BRONZE_SHOVEL) || id == BRONZE_SWORD) return 320;
    if (id == BUCKET || id == WATER_BUCKET || id == LAVA_BUCKET || id == BOSS_COMPASS || id == FORTRESS_COMPASS) return 0;
    if (id == BRUSHER) return 64;
    if (id == SHIELD) return 336;
    switch (toolTier(id)) {
        case 1: return 59;
        case 2: return 131;
        case 3: return 250;
        case 4: return 700;
        case 5: return 1500;
        case 6: return 2400;
        default: return id == FLINT_AND_STEEL ? 64 : id == VOID_FLINT_AND_STEEL ? 16 : id == HOE ? 250 : id == BOW ? 384 : 0;
    }
}

int armorPiece(ItemId id) { return !isArmor(id) ? -1 : id >= HARDIUM_ARMOR ? id - HARDIUM_ARMOR : (id - FIRST_ARMOR) % 4; }
int armorMaterial(ItemId id) { return !isArmor(id) ? -1 : id >= HARDIUM_ARMOR ? 5 : (id - FIRST_ARMOR) / 4; }
ItemId armorItem(int material, int piece) { return (ItemId)(material == 5 ? HARDIUM_ARMOR + piece : FIRST_ARMOR + material * 4 + piece); }

int armorPoints(ItemId id) {
    if (!isArmor(id)) return 0;
    static const int kPoints[6][4] = {{1, 2, 2, 1}, {1, 3, 2, 1}, {2, 5, 4, 2}, {2, 6, 5, 2}, {3, 8, 6, 3}, {4, 9, 7, 4}};
    return kPoints[armorMaterial(id)][armorPiece(id)];
}

float armorReduction(const ItemStack armor[4]) {
    int points = 0;
    for (int i = 0; i < 4; ++i)
        if (!armor[i].empty()) points += armorPoints(armor[i].id);
    return std::min(0.8f, points * 0.04f);
}

int maxStack(ItemId id) {
    if (const ModItem* m = id >= kModIdBase ? ModRegistry::get().item(id) : nullptr) return m->stack;
    if (id == WATER_BUCKET || id == LAVA_BUCKET || id == BOSS_COMPASS || id == CAKE || id == MILK_BUCKET || id == MAP || id == ANNIV_MEDAL ||
        id == FORTRESS_COMPASS || id == EXPLORER_MAP || id == WRITABLE_BOOK || id == WRITTEN_BOOK || id == CELESTIAL_COMPASS)
        return 1;
    if (isSkeletonHead(id) || isDocument(id) || id == POWERSTONE_RELIC || id == CRIMSON_HEAD) return 16;
    if (id == BUCKET || isSpawnEgg(id)) return 16;
    return toolDurability(id) > 0 ? 1 : kMaxStack;
}

namespace {

float hardness(ItemId id) {
    if (const ModBlock* m = id >= kModIdBase ? ModRegistry::get().block(id) : nullptr) return m->hardness;
    switch (id) {
        case BEDROCK: case WATER: case LAVA: case PORTAL: case VOID_PORTAL: case AIR: case WORLD_END: return -1;
        case TNT: case NUKE_TNT: case POWER_CABLE: case POWER_CABLE_ON: case SWITCH: case SWITCH_ON: return 0.0f;
        case POWERSTONE_LAMP: case POWERSTONE_LAMP_ON: case POWERSTONE_LIGHT: return 0.3f;
        case POWERSTONE_ORE: return 3.0f;
        case POWERSTONE_BLOCK: return 5.0f;
        case GIFT_BOX: return 0.5f;
        case CAKE: return 0.5f;
        case NETHERPERK_CAP: return 0.3f;
        case TALL_GRASS: case DANDELION: case POPPY: case NETHER_SPROUTS: case CRIMSON_ROOTS: return 0.0f;
        case NETHER_GRASS: return 0.5f;
        case VOID_STONE: case VOID_GRASS: return 1.8f;
        case VOID_STONE_BRICKS: return 2.2f;
        case RED_SAND: return 0.5f;
        case PUMPKIN: return 1.0f;
        case EYE_LOG: return 2.5f;
        case LEAVES: return 0.2f;
        case GLASS: return 0.3f;
        case TAR: return -1;
        case TAR_BLOCK: return 1.2f;
        case DRIPSTONE_BLOCK: return 1.5f;
        case DRIPSTONE_UP: case DRIPSTONE_DOWN: return 0.4f;
        case SNOW_LAYER: return 0.1f;
        case FARMLAND: return 0.6f;
        case HAY_BALE: return 0.5f;
        case NETHERRACK: return 0.4f;
        case DIRT: case SAND: return 0.5f;
        case GRASS: case GRAVEL: return 0.6f;
        case STONE: return 1.5f;
        case COBBLESTONE: case PLANKS: case LOG: case BRICKS: case NETHER_BRICK: return 2.0f;
        case CRAFTING_TABLE: return 2.5f;
        case COAL_ORE: case IRON_ORE: return 3.0f;
        case FURNACE: case LIT_FURNACE: case POWERSTONE_FURNACE: case POWERSTONE_FURNACE_ON: return 3.5f;
        case OBSIDIAN: return 50.0f;
        case SNOW_BLOCK: return 0.2f;
        case CLAY: return 0.6f;
        case SANDSTONE: return 0.8f;
        case BOOKSHELF: case STONE_BRICKS: return 1.5f;
        case ICE: return 0.5f;
        case CACTUS: return 0.4f;
        case HARDENED_STONE: return 4.0f;  // like stone, but slower
        case FENCE: return 2.0f;
        case MOSSY_COBBLESTONE: return 2.0f;
        case POWERSTONE_BRICKS: case CRACKED_POWERSTONE_BRICKS: case CHISELED_POWERSTONE_BRICKS: case POWERSTONE_FENCE: return 2.0f;
        case POWERSTONE_GLASS: return 0.3f;
        case DEBRIS_GRAVEL: return 0.6f;
        case WASTELAND_GRASS: return 0.5f;
        case DRY_GRASS: case GRASSER_FLOWER: case FERN: return 0.0f;
        case CRACKED_NETHER_BRICKS: case CHISELED_NETHER_BRICKS: case NETHER_BRICK_FENCE: return 2.0f;
        case VOID_BRICK_EYE: return 3.0f;
        case CRIMSON_STONE: return 1.5f;
        case DEEPSTONE: case MOSSY_DEEPSTONE: case FUNGAL_DEEPSTONE: return 4.5f;  // twice as hard as stone
        case MOB_SPAWNER: return 5.0f;
        case SOULED_GRASS: case CELESTIAL_GRASS: return 0.6f;
        case CELESTIAL_DIRT: return 0.5f;
        case CELESTIAL_STONE: return 1.5f;
        case CELESTIAL_STONE_BRICKS: return 2.0f;
        case DUSTY_CONCRETE: return 1.6f;
        case HARDENED_CONCRETE: return 2.4f;
        case GRAY_PLANKS: return 2.0f;
        case CUTTER: return 3.5f;
        case BARREL: case WOOD_CUTTER: return 2.5f;
        case SOULED_DIRT: return 0.5f;
        case SOULED_STONE: return 1.5f;
        case CELESTIAL_FRAME: case CELESTIAL_FRAME_FULL: case CELESTIAL_PORTAL: return -1.0f;  // unbreakable, like the End's frames
        case MOSS_BLOCK: return 0.6f;
        case DEEPSTONE_BRICKS: case CRACKED_DEEPSTONE_BRICKS: case MOSSY_DEEPSTONE_BRICKS: return 4.0f;
        case HARDIUM_ORE: return 7.0f;
        case ROTTEN_PLANKS: return 1.0f;
        case VINES: return 0.2f;
        case BRONZE_ORE: case STEEL_ORE: case RUBY_ORE: return 3.0f;
        case BRONZE_BLOCK: case STEEL_BLOCK: case RUBY_BLOCK: return 5.0f;
        default:
            if (isStairs(id) || isSlab(id)) return hardness(shapeMaterial(id));
            if (isConcretePowder(id)) return 0.5f;
            if (id == DARK_SAND) return 0.5f;
            if (id == SHARD_CLUSTER) return 0.2f;
            if (id == SHATTERED_STONE || id == SHATTERED_BRICKS || id == DARK_SANDSTONE) return 1.8f;
            if (id == VOID_SHARD_BLOCK) return 1.5f;
            if (id == CRIMSONIUM_ORE) return 4.0f;
            if (id == CRIMSONIUM_PLANKS || id == CRIMSONIUM_LOG) return 2.0f;
            if (isColorConcrete(id)) return 1.8f;
            if (isCarpet(id)) return 0.1f;
            if (isStainedGlass(id)) return 0.3f;
            if (isWallCable(id) || isWallSwitch(id) || isCeilingCable(id)) return 0.0f;
            if (isBalloon(id) || id == CONFETTI_BLOCK) return 0.1f;
            if (id == PARTY_LANTERN) return 0.5f;
            if (id == ANNIV_LOGO || id == FESTIVE_BRICKS) return 2.0f;
            if (isPaintedPlanks(id)) return 2.0f;
            if (woolColor(id) >= 0) return 0.8f;
            if (isChest(id)) return 2.5f;
            if (isSkeletonHead(id)) return 1.0f;
            if (isFence(id)) return 2.0f;
            if (isDoor(id)) return id >= IRON_DOOR ? 5.0f : 3.0f;
            if (isTrapdoor(id)) return 3.0f;
            if (isBed(id)) return 0.2f;
            if (isSapling(id) || isCrop(id)) return 0.0f;
            if (id == VOID_PLANKS || id == NETHERPERK_PLANKS || id == CRIMSON_PLANKS) return 2.0f;
            if (isTorch(id)) return 0.0f;
            if (isLeaves(id)) return 0.2f;
            if (isLog(id) || id == SPRUCE_PLANKS) return 2.0f;
            return 1.0f;
    }
}

// Which tool speeds a block up, and the tier needed to get a drop at all.
ToolKind preferredTool(ItemId id, int& requiredTier) {
    requiredTier = 0;
    if (const ModBlock* m = id >= kModIdBase ? ModRegistry::get().block(id) : nullptr) {
        if (m->tool == "pickaxe") {
            requiredTier = 1;
            return ToolKind::Pickaxe;
        }
        return m->tool == "axe" ? ToolKind::Axe : m->tool == "shovel" ? ToolKind::Shovel : ToolKind::None;
    }
    if ((isConcretePowder(id) && id != DUSTY_CONCRETE) || id == DARK_SAND) return ToolKind::Shovel;
    if (id == CRIMSONIUM_LOG || id == CRIMSONIUM_PLANKS) return ToolKind::Axe;
    if (id == SHATTERED_STONE || id == SHATTERED_BRICKS || id == DARK_SANDSTONE || id == VOID_SHARD_BLOCK || id == CRIMSONIUM_ORE) {
        requiredTier = id == CRIMSONIUM_ORE ? 3 : 1;
        return ToolKind::Pickaxe;
    }
    if (isColorConcrete(id)) {
        requiredTier = 1;
        return ToolKind::Pickaxe;
    }
    switch (id) {
        case STONE: case COBBLESTONE: case BRICKS: case NETHER_BRICK: case NETHERRACK: case COAL_ORE:
        case FURNACE: case LIT_FURNACE: case POWERSTONE_FURNACE: case POWERSTONE_FURNACE_ON: case SANDSTONE: case STONE_BRICKS: case MOSSY_COBBLESTONE:
            requiredTier = 1;
            return ToolKind::Pickaxe;
        case BRONZE_ORE: case BRONZE_BLOCK:
            requiredTier = 2;  // a stone pickaxe
            return ToolKind::Pickaxe;
        case STEEL_ORE: case STEEL_BLOCK:
            requiredTier = 3;  // iron or bronze
            return ToolKind::Pickaxe;
        case RUBY_ORE: case RUBY_BLOCK:
            requiredTier = 4;  // steel
            return ToolKind::Pickaxe;
        case HARDIUM_ORE:
            requiredTier = 4;  // steel or better
            return ToolKind::Pickaxe;
        case DEEPSTONE: case MOSSY_DEEPSTONE: case FUNGAL_DEEPSTONE: case DEEPSTONE_BRICKS: case CRACKED_DEEPSTONE_BRICKS:
        case MOSSY_DEEPSTONE_BRICKS:
            requiredTier = 2;  // at least a stone pickaxe
            return ToolKind::Pickaxe;
        case MOB_SPAWNER:
            requiredTier = 1;
            return ToolKind::Pickaxe;
        case MOSS_BLOCK: case SOULED_GRASS: case SOULED_DIRT: case CELESTIAL_GRASS: case CELESTIAL_DIRT: return ToolKind::Shovel;
        case SOULED_STONE:
            requiredTier = 1;
            return ToolKind::Pickaxe;
        case ROTTEN_PLANKS: return ToolKind::Axe;
        case BOOKSHELF: case FENCE: case SPRUCE_LOG: case SPRUCE_PLANKS: return ToolKind::Axe;
        case HARDENED_STONE:
            requiredTier = 2;  // at least a stone pickaxe
            return ToolKind::Pickaxe;
        case SNOW_BLOCK: case CLAY: case RED_SAND: case SNOW_LAYER: return ToolKind::Shovel;
        case VOID_STONE: case VOID_GRASS: case VOID_STONE_BRICKS: case NETHER_GRASS: case WASTELAND_GRASS: case VOID_BRICK_EYE: case CRIMSON_STONE:
            requiredTier = 1;
            return ToolKind::Pickaxe;
        case PUMPKIN: case VOID_LOG: case EYE_LOG: case NETHERPERK_LOG: case VOID_PLANKS: case NETHERPERK_PLANKS:
        case CRIMSON_LOG: case CRIMSON_PLANKS: case GRAY_LOG: case GRAY_PLANKS: return ToolKind::Axe;
        case BARREL: case WOOD_CUTTER: return ToolKind::Axe;
        case CUTTER:
        case DUSTY_CONCRETE: case HARDENED_CONCRETE:
            requiredTier = 1;
            return ToolKind::Pickaxe;
        case POWERSTONE_ORE: case POWERSTONE_BLOCK:
            requiredTier = 2;  // a stone pickaxe
            return ToolKind::Pickaxe;
        case IRON_ORE:
            requiredTier = 2;
            return ToolKind::Pickaxe;
        case OBSIDIAN:
            requiredTier = 3;  // no diamonds yet, iron does the job
            return ToolKind::Pickaxe;
        case LOG: case PLANKS: case CRAFTING_TABLE: return ToolKind::Axe;
        case DRIPSTONE_BLOCK: case DRIPSTONE_UP: case DRIPSTONE_DOWN: case TAR_BLOCK: case FESTIVE_BRICKS: case ANNIV_LOGO:
            requiredTier = 1;
            return ToolKind::Pickaxe;
        case DIRT: case GRASS: case SAND: case GRAVEL: case FARMLAND: case DEBRIS_GRAVEL: return ToolKind::Shovel;
        case POWERSTONE_BRICKS: case CRACKED_POWERSTONE_BRICKS: case CHISELED_POWERSTONE_BRICKS: case POWERSTONE_FENCE:
        case CRACKED_NETHER_BRICKS: case CHISELED_NETHER_BRICKS: case NETHER_BRICK_FENCE:
            requiredTier = 1;
            return ToolKind::Pickaxe;
        case SPRUCE_FENCE: case VOID_FENCE: case NETHERPERK_FENCE: return ToolKind::Axe;
        default:
            if (isStairs(id) || isSlab(id)) return preferredTool(shapeMaterial(id), requiredTier);
            if (isPaintedPlanks(id)) return ToolKind::Axe;
            if (isChest(id) || (isDoor(id) && id < IRON_DOOR) || isTrapdoor(id)) return ToolKind::Axe;
            if (isDoor(id)) {  // iron doors need a pickaxe
                requiredTier = 1;
                return ToolKind::Pickaxe;
            }
            return ToolKind::None;
    }
}

}  // namespace

bool canHarvest(ItemId block, ItemId tool) {
    int tier = 0;
    ToolKind kind = preferredTool(block, tier);
    return tier == 0 || (toolKind(tool) == kind && toolTier(tool) >= tier);
}

float breakTime(ItemId block, ItemId tool) {
    float h = hardness(block);
    if (h < 0) return -1;
    int tier = 0;
    ToolKind kind = preferredTool(block, tier);
    // The classic formula: x1.5 when harvestable, x5 when not, divided by the tool speed.
    float t = h * (canHarvest(block, tool) ? 1.5f : 5.0f);
    if (kind != ToolKind::None && toolKind(tool) == kind) t /= 2.0f * toolTier(tool);
    return std::max(t, 0.05f);
}

ItemId dropFor(ItemId block, float roll, ItemId tool) {
    if (!canHarvest(block, tool)) return AIR;
    if (const ModBlock* m = block >= kModIdBase ? ModRegistry::get().block(block) : nullptr)
        return m->drop.empty() ? block : itemFromKey(m->drop);
    switch (block) {
        case GRASS: return DIRT;
        case STONE: return COBBLESTONE;
        case GLASS: case POWERSTONE_GLASS: return AIR;
        case DEBRIS_GRAVEL: return GRAVEL;     // the finds are lost without a brusher
        case WASTELAND_GRASS: return NETHERRACK;
        case DRY_GRASS: return roll < 0.08f ? (ItemId)WHEAT_SEEDS : (ItemId)AIR;
        case FERN: return AIR;
        case DRIPSTONE_DOWN: return DRIPSTONE_UP;  // one item for both ways
        case SNOW_LAYER: return AIR;  // it just melts away
        case LEAVES: return roll < 0.12f ? (ItemId)APPLE : roll < 0.2f ? (ItemId)OAK_SAPLING : (ItemId)AIR;
        case TALL_GRASS: return roll < 0.14f ? (ItemId)WHEAT_SEEDS : (ItemId)AIR;  // seeds in the grass
        case FARMLAND: return DIRT;
        case WHEAT_CROP1: case WHEAT_CROP2: return WHEAT_SEEDS;
        case WHEAT_RIPE: return WHEAT;  // and seeds, see Session::breakBlock
        case GRAVEL: return roll < 0.1f ? FLINT : GRAVEL;
        case COAL_ORE: return COAL;
        case HARDIUM_ORE: return HARDIUM;
        case MOB_SPAWNER: return AIR;
        case SOULED_GRASS: return SOULED_DIRT;
        case CELESTIAL_GRASS: return CELESTIAL_DIRT;
        case ROTTEN_PLANKS: return roll < 0.5f ? (ItemId)STICK : (ItemId)ROTTEN_PLANKS;  // they crumble
        case RUBY_ORE: return RUBY;
        case ICE: return AIR;  // melts
        case SPRUCE_LEAVES: return roll < 0.08f ? (ItemId)PINE_SAPLING : (ItemId)AIR;
        case VOID_LEAVES: return roll < 0.08f ? (ItemId)VOID_SAPLING : (ItemId)AIR;
        case NETHERPERK_CAP: return roll < 0.08f ? (ItemId)NETHERPERK_SAPLING : (ItemId)AIR;
        case RED_LEAVES: case ORANGE_LEAVES: case YELLOW_LEAVES: return roll < 0.08f ? (ItemId)GRAY_SAPLING : (ItemId)AIR;
        case CELESTIAL_LEAVES: return roll < 0.08f ? (ItemId)CELESTIAL_SAPLING : (ItemId)AIR;
        case CRIMSONIUM_LEAVES: return AIR;
        case VOID_SHARD_BLOCK: case SHARD_CLUSTER: return VOID_SHARD;
        case CRIMSONIUM_ORE: return CRIMSONIUM;
        case POWERSTONE_ORE: return POWERSTONE_DUST;
        case POWER_CABLE_ON: return POWER_CABLE;
        case POWERSTONE_LAMP_ON: return POWERSTONE_LAMP;
        case SWITCH_ON: return SWITCH;
        case CAKE: return AIR;
        case GIFT_BOX: return AIR;
        case NETHER_GRASS: return NETHERRACK;
        case VOID_GRASS: return VOID_STONE;
        case NETHER_SPROUTS: case CRIMSON_ROOTS: return AIR;
        case HARDENED_STONE: return HARDENED_STONE;
        case LIT_FURNACE: return FURNACE;
        case POWERSTONE_FURNACE_ON: return POWERSTONE_FURNACE;
        default:
            if (isChest(block)) return CHEST;
            if (isDoor(block)) return block >= IRON_DOOR ? IRON_DOOR_ITEM : OAK_DOOR_ITEM;  // either half gives the door
            if (isTrapdoor(block)) return OAK_TRAPDOOR;
            if (isBed(block)) return BED_ITEM;
            if (isStairs(block)) return stairsId(stairsMaterial(block), 0, false);
            if (isSkeletonHead(block)) return SKELETON_HEAD;  // a double slab gives two
            if (isSlab(block)) return slabId(slabMaterial(block), 0);
            if (isStainedGlass(block)) return AIR;  // glass breaks
            if (isWallCable(block) || isCeilingCable(block)) return POWER_CABLE;
            if (isWallSwitch(block)) return SWITCH;
            if (isTorch(block)) return TORCH;
            return isPlaceable(block) ? block : (ItemId)AIR;
    }
}

void moveOrSwap(ItemStack& a, ItemStack& b) {
    if (&a == &b) return;
    if (!a.empty() && a.id == b.id && maxStack(a.id) > 1) {
        int n = std::min<int>(a.count, maxStack(b.id) - b.count);
        b.count += n;
        a.count -= n;
        if (a.count == 0) a = {};
        return;
    }
    std::swap(a, b);
}

int transferItems(ItemStack& from, ItemStack& to, int count) {
    if (&from == &to || from.empty() || count <= 0) return 0;
    count = std::min<int>(count, from.count);
    if (to.empty()) {
        count = std::min(count, maxStack(from.id));
        to = {from.id, (uint8_t)count, from.damage};
    } else if (to.id == from.id && maxStack(to.id) > 1 && to.damage == from.damage) {
        count = std::min(count, maxStack(to.id) - (int)to.count);
        if (count <= 0) return 0;
        to.count = (uint8_t)(to.count + count);
    } else {
        if (count < from.count) return 0;  // a part cannot be swapped with something else
        std::swap(from, to);
        return to.count;
    }
    from.count = (uint8_t)(from.count - count);
    if (from.count == 0) from = {};
    return count;
}

int moveIntoRange(ItemStack& from, int count, ItemStack* begin, ItemStack* end) {
    count = std::min<int>(count, from.empty() ? 0 : from.count);
    int moved = 0;
    for (int pass = 0; pass < 2 && moved < count; ++pass)
        for (ItemStack* t = begin; t != end && moved < count; ++t) {
            if (t == &from || (pass == 0) == t->empty()) continue;  // pass 0: same item, pass 1: empty slots
            moved += transferItems(from, *t, count - moved);
        }
    return moved;
}

int Inventory::add(ItemId id, int count, uint16_t damage) {
    const int limit = maxStack(id);
    if (limit > 1)
        for (auto& s : slots) {
            if (count == 0) break;
            if (s.id == id && s.count > 0 && s.count < limit) {
                int n = std::min(count, limit - s.count);
                s.count += n;
                count -= n;
            }
        }
    for (auto& s : slots) {
        if (count == 0) break;
        if (s.empty()) {
            int n = std::min(count, limit);
            s = {id, (uint8_t)n, damage};
            count -= n;
        }
    }
    return count;
}

void Inventory::removeOne(int slot) {
    ItemStack& s = slots[slot];
    if (s.count > 0 && --s.count == 0) s = {};
}

}  // namespace vc

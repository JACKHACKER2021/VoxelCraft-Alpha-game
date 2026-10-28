#include "TexturePack.h"

#include <algorithm>
#include <cstdio>
#include <initializer_list>

#include "Blocks.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "third_party/stb/stb_image.h"
#pragma GCC diagnostic pop

namespace vc {

namespace {

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    bool valid() const { return w > 0 && h > 0; }
};

Image loadImage(const std::string& path) {
    Image img;
    int n = 0;
    stbi_uc* data = stbi_load(path.c_str(), &img.w, &img.h, &n, 4);
    if (!data) return {};
    img.rgba.assign(data, data + img.w * img.h * 4);
    stbi_image_free(data);
    return img;
}

struct TileSource {
    int terrainIndex;                  // slot in a classic terrain.png, -1 if it has none
    const char* folder;                // "blocks" or "items"
    std::initializer_list<const char*> names;  // Bedrock names first, then Java ones
    uint32_t tint;                     // applied to grayscale textures (grass, leaves, water)
};

// Indexed by Tile. Classic terrain.png slots follow the Java Beta / old PE layout.
const TileSource kSources[] = {
    /* T_STONE          */ {1, "blocks", {"stone"}, 0},
    /* T_GRASS_TOP      */ {0, "blocks", {"grass_carried", "grass_top", "grass_block_top"}, 0x79C05A},
    /* T_GRASS_SIDE     */ {3, "blocks", {"grass_side_carried", "grass_side", "grass_block_side"}, 0},
    /* T_DIRT           */ {2, "blocks", {"dirt"}, 0},
    /* T_COBBLE         */ {16, "blocks", {"cobblestone"}, 0},
    /* T_PLANKS         */ {4, "blocks", {"planks_oak", "oak_planks"}, 0},
    /* T_BEDROCK        */ {17, "blocks", {"bedrock"}, 0},
    /* T_WATER          */ {205, "blocks", {"water_still", "water_placeholder"}, 0x3F76E4},
    /* T_SAND           */ {18, "blocks", {"sand"}, 0},
    /* T_GRAVEL         */ {19, "blocks", {"gravel"}, 0},
    /* T_LOG_SIDE       */ {20, "blocks", {"log_oak", "oak_log"}, 0},
    /* T_LOG_TOP        */ {21, "blocks", {"log_oak_top", "oak_log_top"}, 0},
    /* T_LEAVES         */ {52, "blocks", {"leaves_oak_carried", "leaves_oak", "oak_leaves"}, 0x48B518},
    /* T_GLASS          */ {49, "blocks", {"glass"}, 0},
    /* T_BRICKS         */ {7, "blocks", {"brick", "bricks"}, 0},
    /* T_LAVA           */ {237, "blocks", {"lava_still", "lava_placeholder"}, 0},
    /* T_OBSIDIAN       */ {37, "blocks", {"obsidian"}, 0},
    /* T_NETHERRACK     */ {103, "blocks", {"netherrack"}, 0},
    /* T_FARMLAND       */ {-1, "blocks", {"farmland_dry", "farmland"}, 0},
    /* T_HAY_TOP        */ {-1, "blocks", {"hay_block_top"}, 0},
    /* T_PORTAL         */ {-1, "blocks", {"portal", "nether_portal"}, 0},
    /* T_NETHER_BRICK   */ {224, "blocks", {"nether_brick", "nether_bricks"}, 0},
    /* T_HAY_SIDE       */ {-1, "blocks", {"hay_block_side"}, 0},
    /* T_FLINT_AND_STEEL*/ {-1, "items", {"flint_and_steel"}, 0},
    /* T_APPLE          */ {-1, "items", {"apple"}, 0},
    /* T_COAL_ORE       */ {34, "blocks", {"coal_ore"}, 0},
    /* T_IRON_ORE       */ {33, "blocks", {"iron_ore"}, 0},
    /* T_CRAFTING_TOP   */ {43, "blocks", {"crafting_table_top"}, 0},
    /* T_CRAFTING_SIDE  */ {59, "blocks", {"crafting_table_side"}, 0},
    /* T_CRAFTING_FRONT */ {60, "blocks", {"crafting_table_front"}, 0},
    /* T_FURNACE_TOP    */ {62, "blocks", {"furnace_top"}, 0},
    /* T_FURNACE_SIDE   */ {45, "blocks", {"furnace_side"}, 0},
    /* T_FURNACE_FRONT  */ {44, "blocks", {"furnace_front_off", "furnace_front"}, 0},
    /* T_FURNACE_LIT    */ {61, "blocks", {"furnace_front_on"}, 0},
    /* T_STICK          */ {-1, "items", {"stick"}, 0},
    /* T_COAL           */ {-1, "items", {"coal"}, 0},
    /* T_IRON_INGOT     */ {-1, "items", {"iron_ingot"}, 0},
    /* T_FLINT          */ {-1, "items", {"flint"}, 0},
    /* T_WOOD_PICKAXE   */ {-1, "items", {"wood_pickaxe", "wooden_pickaxe"}, 0},
    /* T_STONE_PICKAXE  */ {-1, "items", {"stone_pickaxe"}, 0},
    /* T_IRON_PICKAXE   */ {-1, "items", {"iron_pickaxe"}, 0},
    /* T_WOOD_AXE       */ {-1, "items", {"wood_axe", "wooden_axe"}, 0},
    /* T_STONE_AXE      */ {-1, "items", {"stone_axe"}, 0},
    /* T_IRON_AXE       */ {-1, "items", {"iron_axe"}, 0},
    /* T_WOOD_SHOVEL    */ {-1, "items", {"wood_shovel", "wooden_shovel"}, 0},
    /* T_STONE_SHOVEL   */ {-1, "items", {"stone_shovel"}, 0},
    /* T_IRON_SHOVEL    */ {-1, "items", {"iron_shovel"}, 0},
    /* T_TALL_GRASS     */ {39, "blocks", {"tallgrass", "tall_grass", "short_grass"}, 0x79C05A},
    /* T_DANDELION      */ {13, "blocks", {"flower_dandelion", "dandelion"}, 0},
    /* T_POPPY          */ {12, "blocks", {"flower_rose", "poppy"}, 0},
    /* T_SANDSTONE_TOP  */ {176, "blocks", {"sandstone_top"}, 0},
    /* T_SANDSTONE_SIDE */ {192, "blocks", {"sandstone_normal", "sandstone"}, 0},
    /* T_BOOKSHELF      */ {35, "blocks", {"bookshelf"}, 0},
    /* T_MOSSY_COBBLE   */ {36, "blocks", {"cobblestone_mossy", "mossy_cobblestone"}, 0},
    /* T_SNOW           */ {66, "blocks", {"snow"}, 0},
    /* T_CLAY           */ {72, "blocks", {"clay"}, 0},
    /* T_STONE_BRICKS   */ {54, "blocks", {"stonebrick", "stone_bricks"}, 0},
    /* T_WOOL+0        */ {64, "blocks", {"wool_colored_white", "white_wool"}, 0},
    /* T_WOOL+1        */ {-1, "blocks", {"wool_colored_orange", "orange_wool"}, 0},
    /* T_WOOL+2        */ {-1, "blocks", {"wool_colored_magenta", "magenta_wool"}, 0},
    /* T_WOOL+3        */ {-1, "blocks", {"wool_colored_light_blue", "light_blue_wool"}, 0},
    /* T_WOOL+4        */ {-1, "blocks", {"wool_colored_yellow", "yellow_wool"}, 0},
    /* T_WOOL+5        */ {-1, "blocks", {"wool_colored_lime", "lime_wool"}, 0},
    /* T_WOOL+6        */ {-1, "blocks", {"wool_colored_pink", "pink_wool"}, 0},
    /* T_WOOL+7        */ {-1, "blocks", {"wool_colored_gray", "gray_wool"}, 0},
    /* T_WOOL+8        */ {-1, "blocks", {"wool_colored_silver", "light_gray_wool"}, 0},
    /* T_WOOL+9        */ {-1, "blocks", {"wool_colored_cyan", "cyan_wool"}, 0},
    /* T_WOOL+10       */ {-1, "blocks", {"wool_colored_purple", "purple_wool"}, 0},
    /* T_WOOL+11       */ {-1, "blocks", {"wool_colored_blue", "blue_wool"}, 0},
    /* T_WOOL+12       */ {-1, "blocks", {"wool_colored_brown", "brown_wool"}, 0},
    /* T_WOOL+13       */ {-1, "blocks", {"wool_colored_green", "green_wool"}, 0},
    /* T_WOOL+14       */ {-1, "blocks", {"wool_colored_red", "red_wool"}, 0},
    /* T_WOOL+15       */ {-1, "blocks", {"wool_colored_black", "black_wool"}, 0},
    /* T_BRONZE_ORE     */ {-1, "blocks", {"bronze_ore"}, 0},
    /* T_STEEL_ORE      */ {-1, "blocks", {"steel_ore"}, 0},
    /* T_RUBY_ORE       */ {-1, "blocks", {"ruby_ore"}, 0},
    /* T_BRONZE_BLOCK   */ {-1, "blocks", {"bronze_block"}, 0},
    /* T_STEEL_BLOCK    */ {-1, "blocks", {"steel_block"}, 0},
    /* T_RUBY_BLOCK     */ {-1, "blocks", {"ruby_block"}, 0},
    /* T_BRONZE_INGOT   */ {-1, "items", {"bronze_ingot"}, 0},
    /* T_STEEL_INGOT    */ {-1, "items", {"steel_ingot"}, 0},
    /* T_RUBY           */ {-1, "items", {"ruby"}, 0},
    /* new tool         */ {-1, "items", {"bronze_pickaxe"}, 0},
    /* new tool         */ {-1, "items", {"bronze_axe"}, 0},
    /* new tool         */ {-1, "items", {"bronze_shovel"}, 0},
    /* new tool         */ {-1, "items", {"steel_pickaxe"}, 0},
    /* new tool         */ {-1, "items", {"steel_axe"}, 0},
    /* new tool         */ {-1, "items", {"steel_shovel"}, 0},
    /* new tool         */ {-1, "items", {"ruby_pickaxe"}, 0},
    /* new tool         */ {-1, "items", {"ruby_axe"}, 0},
    /* new tool         */ {-1, "items", {"ruby_shovel"}, 0},
    /* armour           */ {-1, "items", {"wooden_helmet"}, 0},
    /* armour           */ {-1, "items", {"wooden_chestplate"}, 0},
    /* armour           */ {-1, "items", {"wooden_leggings"}, 0},
    /* armour           */ {-1, "items", {"wooden_boots"}, 0},
    /* armour           */ {-1, "items", {"stone_helmet"}, 0},
    /* armour           */ {-1, "items", {"stone_chestplate"}, 0},
    /* armour           */ {-1, "items", {"stone_leggings"}, 0},
    /* armour           */ {-1, "items", {"stone_boots"}, 0},
    /* armour           */ {-1, "items", {"bronze_helmet"}, 0},
    /* armour           */ {-1, "items", {"bronze_chestplate"}, 0},
    /* armour           */ {-1, "items", {"bronze_leggings"}, 0},
    /* armour           */ {-1, "items", {"bronze_boots"}, 0},
    /* armour           */ {-1, "items", {"steel_helmet"}, 0},
    /* armour           */ {-1, "items", {"steel_chestplate"}, 0},
    /* armour           */ {-1, "items", {"steel_leggings"}, 0},
    /* armour           */ {-1, "items", {"steel_boots"}, 0},
    /* armour           */ {-1, "items", {"ruby_helmet"}, 0},
    /* armour           */ {-1, "items", {"ruby_chestplate"}, 0},
    /* armour           */ {-1, "items", {"ruby_leggings"}, 0},
    /* armour           */ {-1, "items", {"ruby_boots"}, 0},
    /* T_SPRUCE_LOG_SIDE*/ {116, "blocks", {"log_spruce", "spruce_log"}, 0},
    /* T_SPRUCE_LOG_TOP */ {-1, "blocks", {"log_spruce_top", "spruce_log_top"}, 0},
    /* T_SPRUCE_LEAVES  */ {-1, "blocks", {"leaves_spruce_carried", "leaves_spruce", "spruce_leaves"}, 0x619961},
    /* T_SPRUCE_PLANKS  */ {-1, "blocks", {"planks_spruce", "spruce_planks"}, 0},
    /* T_ICE            */ {67, "blocks", {"ice"}, 0},
    /* T_CACTUS_SIDE    */ {70, "blocks", {"cactus_side"}, 0},
    /* T_CACTUS_TOP     */ {69, "blocks", {"cactus_top"}, 0},
    /* T_HARDENED_STONE */ {-1, "blocks", {"hardened_stone"}, 0},
    /* T_CHEST_TOP      */ {-1, "blocks", {"vc_chest_top"}, 0},
    /* T_CHEST_SIDE     */ {-1, "blocks", {"vc_chest_side"}, 0},
    /* T_CHEST_FRONT    */ {-1, "blocks", {"vc_chest_front"}, 0},
    /* T_OAK_DOOR_LOWER */ {97, "blocks", {"door_wood_lower", "oak_door_bottom"}, 0},
    /* T_OAK_DOOR_UPPER */ {81, "blocks", {"door_wood_upper", "oak_door_top"}, 0},
    /* T_IRON_DOOR_LOWER*/ {98, "blocks", {"door_iron_lower", "iron_door_bottom"}, 0},
    /* T_IRON_DOOR_UPPER*/ {82, "blocks", {"door_iron_upper", "iron_door_top"}, 0},
    /* T_TRAPDOOR       */ {84, "blocks", {"trapdoor", "oak_trapdoor"}, 0},
    /* T_OAK_DOOR_ITEM  */ {-1, "items", {"door_wood", "oak_door"}, 0},
    /* T_IRON_DOOR_ITEM */ {-1, "items", {"door_iron", "iron_door"}, 0},
    /* T_BOAT           */ {-1, "items", {"boat_oak", "oak_boat"}, 0},
    /* T_NETHER_GRASS_TOP*/ {-1, "blocks", {"vc_nether_grass_top"}, 0},
    /* T_NETHER_GRASS_SIDE*/ {-1, "blocks", {"vc_nether_grass_side"}, 0},
    /* T_NETHER_SPROUTS */ {-1, "blocks", {"nether_sprouts"}, 0},
    /* T_VOID_STONE     */ {-1, "blocks", {"vc_void_stone"}, 0},
    /* T_VOID_GRASS_TOP */ {-1, "blocks", {"vc_void_grass_top"}, 0},
    /* T_VOID_GRASS_SIDE*/ {-1, "blocks", {"vc_void_grass_side"}, 0},
    /* T_VOID_LOG_SIDE  */ {-1, "blocks", {"vc_void_log"}, 0},
    /* T_VOID_LOG_TOP   */ {-1, "blocks", {"vc_void_log_top"}, 0},
    /* T_EYE_LOG        */ {-1, "blocks", {"vc_eye_log"}, 0},
    /* T_VOID_LEAVES    */ {-1, "blocks", {"vc_void_leaves"}, 0},
    /* T_RED_SAND       */ {-1, "blocks", {"red_sand"}, 0},
    /* T_VOID_PORTAL    */ {-1, "blocks", {"vc_void_portal"}, 0},
    /* T_VOID_STONE_BRICKS*/ {-1, "blocks", {"vc_void_stone_bricks"}, 0},
    /* T_PUMPKIN_SIDE   */ {-1, "blocks", {"pumpkin_side"}, 0},
    /* T_PUMPKIN_TOP    */ {-1, "blocks", {"pumpkin_top"}, 0},
    /* T_PUMPKIN_FACE   */ {-1, "blocks", {"carved_pumpkin"}, 0},
    /* T_CRIMSON_ROOTS  */ {-1, "blocks", {"crimson_roots"}, 0},
    /* T_BED_TOP_HEAD   */ {-1, "blocks", {"vc_bed_head"}, 0},
    /* T_BED_TOP_FOOT   */ {-1, "blocks", {"vc_bed_foot"}, 0},
    /* T_BED_SIDE       */ {-1, "blocks", {"vc_bed_side"}, 0},
    /* T_VOID_FLINT_AND_STEEL*/ {-1, "items", {"vc_void_flint_and_steel"}, 0},
    /* T_RAW_PORK       */ {-1, "items", {"porkchop_raw"}, 0},
    /* T_COOKED_PORK    */ {-1, "items", {"porkchop_cooked"}, 0},
    /* T_RAW_MUTTON     */ {-1, "items", {"mutton_raw"}, 0},
    /* T_COOKED_MUTTON  */ {-1, "items", {"mutton_cooked"}, 0},
    /* T_BED_ITEM       */ {-1, "items", {"vc_bed"}, 0},
    /* T_PIG_SKIN       */ {-1, "mobs", {"vc_pig_skin"}, 0},
    /* T_PIG_FACE       */ {-1, "mobs", {"vc_pig_face"}, 0},
    /* T_SHEEP_WOOL     */ {-1, "mobs", {"vc_sheep_wool"}, 0},
    /* T_SHEEP_FACE     */ {-1, "mobs", {"vc_sheep_face"}, 0},
    /* T_FORGOTTEN_FACE */ {-1, "mobs", {"vc_forgotten_face"}, 0},
    /* T_FORGOTTEN_SHIRT*/ {-1, "mobs", {"vc_forgotten_shirt"}, 0},
    /* T_FORGOTTEN_SKIN */ {-1, "mobs", {"vc_forgotten_skin"}, 0},
    /* T_CRIMSON_BODY   */ {-1, "mobs", {"vc_crimson_body"}, 0},
    /* T_CRIMSON_FACE   */ {-1, "mobs", {"vc_crimson_face"}, 0},
    /* T_SUN            */ {-1, "environment", {"vc_sun"}, 0},
    /* T_MOON           */ {-1, "environment", {"vc_moon"}, 0},
    /* T_CLOUD          */ {-1, "environment", {"vc_cloud"}, 0},
    /* T_RAIN           */ {-1, "environment", {"vc_rain"}, 0},
    /* T_SNOWFLAKE      */ {-1, "environment", {"vc_snow"}, 0},
    /* T_STAR           */ {-1, "environment", {"vc_star"}, 0},
    /* T_SHEEP_SKIN     */ {-1, "mobs", {"vc_sheep_skin"}, 0},
    /* T_WORLD_END      */ {-1, "blocks", {"vc_world_end"}, 0},
    /* T_TNT_SIDE       */ {-1, "blocks", {"tnt_side"}, 0},
    /* T_TNT_TOP        */ {-1, "blocks", {"tnt_top"}, 0},
    /* T_TNT_BOTTOM     */ {-1, "blocks", {"tnt_bottom"}, 0},
    /* T_NUKE_SIDE      */ {-1, "blocks", {"vc_nuke_tnt_side"}, 0},
    /* T_NETHERPERK_LOG_SIDE*/ {-1, "blocks", {"vc_netherperk_log"}, 0},
    /* T_NETHERPERK_LOG_TOP*/ {-1, "blocks", {"vc_netherperk_log_top"}, 0},
    /* T_NETHERPERK_CAP */ {-1, "blocks", {"vc_netherperk_cap"}, 0},
    /* T_NETHERPERK_PLANKS*/ {-1, "blocks", {"vc_netherperk_planks"}, 0},
    /* T_VOID_PLANKS    */ {-1, "blocks", {"vc_void_planks"}, 0},
    /* T_OAK_SAPLING    */ {-1, "blocks", {"sapling_oak"}, 0},
    /* T_PINE_SAPLING   */ {-1, "blocks", {"sapling_spruce"}, 0},
    /* T_VOID_SAPLING   */ {-1, "blocks", {"vc_void_sapling"}, 0},
    /* T_NETHERPERK_SAPLING*/ {-1, "blocks", {"vc_netherperk_sapling"}, 0},
    /* T_POWERSTONE_ORE */ {-1, "blocks", {"vc_powerstone_ore"}, 0},
    /* T_POWERSTONE_BLOCK*/ {-1, "blocks", {"vc_powerstone_block"}, 0},
    /* T_CABLE          */ {-1, "blocks", {"vc_power_cable"}, 0},
    /* T_CABLE_ON       */ {-1, "blocks", {"vc_power_cable_on"}, 0},
    /* T_LAMP           */ {-1, "blocks", {"vc_powerstone_lamp"}, 0},
    /* T_LAMP_ON        */ {-1, "blocks", {"vc_powerstone_lamp_on"}, 0},
    /* T_SWITCH         */ {-1, "blocks", {"vc_switch"}, 0},
    /* T_SWITCH_ON      */ {-1, "blocks", {"vc_switch_on"}, 0},
    /* T_POWERSTONE_LIGHT*/ {-1, "blocks", {"vc_powerstone_light"}, 0},
    /* T_GIFT_SIDE      */ {-1, "blocks", {"vc_gift_side"}, 0},
    /* T_GIFT_TOP       */ {-1, "blocks", {"vc_gift_top"}, 0},
    /* T_CAKE_TOP       */ {-1, "blocks", {"cake_top"}, 0},
    /* T_CAKE_SIDE      */ {-1, "blocks", {"cake_side"}, 0},
    /* T_BUCKET         */ {-1, "items", {"bucket_empty"}, 0},
    /* T_WATER_BUCKET   */ {-1, "items", {"bucket_water"}, 0},
    /* T_LAVA_BUCKET    */ {-1, "items", {"bucket_lava"}, 0},
    /* T_COMPASS        */ {-1, "items", {"vc_boss_compass"}, 0},
    /* T_FIRST_SWORD    */ {-1, "items", {"wood_sword"}, 0},
    /* sword            */ {-1, "items", {"stone_sword"}, 0},
    /* sword            */ {-1, "items", {"iron_sword"}, 0},
    /* sword            */ {-1, "items", {"vc_bronze_sword"}, 0},
    /* sword            */ {-1, "items", {"vc_steel_sword"}, 0},
    /* sword            */ {-1, "items", {"vc_ruby_sword"}, 0},
    /* T_EGG_PIG        */ {-1, "items", {"spawn_egg_pig"}, 0},
    /* T_EGG_SHEEP      */ {-1, "items", {"spawn_egg_sheep"}, 0},
    /* T_EGG_SLIME      */ {-1, "items", {"vc_spawn_egg_slime"}, 0},
    /* T_EGG_FORGOTTEN  */ {-1, "items", {"vc_spawn_egg_forgotten"}, 0},
    /* T_POWERSTONE_DUST*/ {-1, "items", {"vc_powerstone_dust"}, 0},
    /* T_POWERSTONE_CHIP*/ {-1, "items", {"vc_powerstone_chip"}, 0},
    /* T_CANDY_CANE     */ {-1, "items", {"vc_candy_cane"}, 0},
    /* T_ZOMBIE_FACE    */ {-1, "mobs", {"vc_zombie_face"}, 0},
    /* T_CONFETTI       */ {-1, "environment", {"vc_confetti"}, 0},
    // 1.3.0
    /* T_WHEAT0         */ {-1, "blocks", {"wheat_stage_0", "wheat_stage0"}, 0},
    /* T_WHEAT1         */ {-1, "blocks", {"wheat_stage_3", "wheat_stage3"}, 0},
    /* T_WHEAT2         */ {-1, "blocks", {"wheat_stage_5", "wheat_stage5"}, 0},
    /* T_WHEAT3         */ {-1, "blocks", {"wheat_stage_7", "wheat_stage7"}, 0},
    /* T_HOE            */ {-1, "items", {"iron_hoe"}, 0},
    /* T_BOW            */ {-1, "items", {"bow_standby", "bow"}, 0},
    /* T_ARROW          */ {-1, "items", {"arrow"}, 0},
    /* T_FEATHER        */ {-1, "items", {"feather"}, 0},
    /* T_WHEAT_ITEM     */ {-1, "items", {"wheat"}, 0},
    /* T_BREAD          */ {-1, "items", {"bread"}, 0},
    /* T_RAW_CHICKEN    */ {-1, "items", {"chicken_raw", "chicken"}, 0},
    /* T_COOKED_CHICKEN */ {-1, "items", {"chicken_cooked", "cooked_chicken"}, 0},
    /* T_RAW_BEEF       */ {-1, "items", {"beef_raw", "beef"}, 0},
    /* T_STEAK          */ {-1, "items", {"beef_cooked", "cooked_beef"}, 0},
    /* T_MILK_BUCKET    */ {-1, "items", {"bucket_milk", "milk_bucket"}, 0},
    /* T_EGG_CHICKEN    */ {-1, "items", {"spawn_egg_chicken"}, 0},
    /* T_EGG_COW        */ {-1, "items", {"spawn_egg_cow"}, 0},
    /* T_EGG_VILLAGER   */ {-1, "items", {"spawn_egg_villager"}, 0},
    /* T_CHICKEN_BODY   */ {-1, "mobs", {"vc_chicken_body"}, 0},
    /* T_CHICKEN_FACE   */ {-1, "mobs", {"vc_chicken_face"}, 0},
    /* T_COW_SKIN       */ {-1, "mobs", {"vc_cow_skin"}, 0},
    /* T_COW_FACE       */ {-1, "mobs", {"vc_cow_face"}, 0},
    /* T_VILLAGER_FACE  */ {-1, "mobs", {"vc_villager_face"}, 0},
    /* T_VILLAGER_ROBE  */ {-1, "mobs", {"vc_villager_robe"}, 0},
    /* T_VILLAGER_SKIN  */ {-1, "mobs", {"vc_villager_skin"}, 0},
    /* T_CABLE_LINE     */ {-1, "blocks", {"vc_power_cable_line"}, 0},
    /* T_CABLE_LINE_ON  */ {-1, "blocks", {"vc_power_cable_line_on"}, 0},
    /* T_VILLAGER_SMITH */ {-1, "mobs", {"vc_villager_smith"}, 0},
    /* T_VILLAGER_FARMER*/ {-1, "mobs", {"vc_villager_farmer"}, 0},
    /* T_BOW_PULLED     */ {-1, "items", {"bow_pulling_2"}, 0},
    /* T_TAR            */ {-1, "blocks", {"vc_tar"}, 0},
    /* T_TAR_BLOCK      */ {-1, "blocks", {"vc_tar_block"}, 0},
    /* T_DRIPSTONE      */ {-1, "blocks", {"dripstone_block"}, 0},
    /* T_DRIPSTONE_UP   */ {-1, "blocks", {"pointed_dripstone_up_tip", "pointed_dripstone_up_frustum"}, 0},
    /* T_DRIPSTONE_DOWN */ {-1, "blocks", {"pointed_dripstone_down_tip", "pointed_dripstone_down_frustum"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_white", "white_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_orange", "orange_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_magenta", "magenta_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_light_blue", "light_blue_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_yellow", "yellow_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_lime", "lime_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_pink", "pink_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_gray", "gray_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_silver", "light_gray_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_cyan", "cyan_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_purple", "purple_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_blue", "blue_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_brown", "brown_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_green", "green_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_red", "red_stained_glass"}, 0},
    /* T_STAINED_GLASS  */ {-1, "blocks", {"glass_black", "black_stained_glass"}, 0},
    /* T_STRING         */ {-1, "items", {"string"}, 0},
    /* T_MAP            */ {-1, "items", {"map_filled", "filled_map"}, 0},
    /* T_TAR_BALL       */ {-1, "items", {"vc_tar_ball"}, 0},
    /* T_EGG_SPIDER     */ {-1, "items", {"spawn_egg_spider"}, 0},
    /* T_EGG_BAT        */ {-1, "items", {"spawn_egg_bat"}, 0},
    /* T_EGG_TNT_ZOMBIE */ {-1, "items", {"vc_spawn_egg_tnt_zombie"}, 0},
    /* T_EGG_TAR_MONSTER*/ {-1, "items", {"vc_spawn_egg_tar_monster"}, 0},
    /* T_SPIDER_BODY    */ {-1, "mobs", {"vc_spider_body"}, 0},
    /* T_SPIDER_FACE    */ {-1, "mobs", {"vc_spider_face"}, 0},
    /* T_BAT_BODY       */ {-1, "mobs", {"vc_bat_body"}, 0},
    /* T_BAT_FACE       */ {-1, "mobs", {"vc_bat_face"}, 0},
    /* T_BAT_WING       */ {-1, "mobs", {"vc_bat_wing"}, 0},
    /* T_TAR_SKIN       */ {-1, "mobs", {"vc_tar_skin"}, 0},
    /* T_TAR_FACE       */ {-1, "mobs", {"vc_tar_face"}, 0},
    /* T_TNT_ZOMBIE_SHIRT*/ {-1, "mobs", {"vc_tnt_zombie_shirt"}, 0},
    /* T_ANNIV_LOGO     */ {-1, "anniversary", {"vc_logo"}, 0},
    /* T_ANNIV_LOGO_SIDE*/ {-1, "anniversary", {"vc_logo_side"}, 0},
    /* T_CONFETTI_BLOCK */ {-1, "anniversary", {"vc_confetti_block"}, 0},
    /* T_BALLOON red    */ {-1, "anniversary", {"vc_balloon_red"}, 0},
    /* T_BALLOON blue   */ {-1, "anniversary", {"vc_balloon_blue"}, 0},
    /* T_BALLOON yellow */ {-1, "anniversary", {"vc_balloon_yellow"}, 0},
    /* T_PARTY_LANTERN  */ {-1, "anniversary", {"vc_party_lantern"}, 0},
    /* T_PARTY_LANTERN_TOP */ {-1, "anniversary", {"vc_party_lantern_top"}, 0},
    /* T_FESTIVE_BRICKS */ {-1, "anniversary", {"vc_festive_bricks"}, 0},
    /* T_PAINTED_PLANKS red    */ {-1, "anniversary", {"vc_painted_planks_red"}, 0},
    /* T_PAINTED_PLANKS blue   */ {-1, "anniversary", {"vc_painted_planks_blue"}, 0},
    /* T_PAINTED_PLANKS yellow */ {-1, "anniversary", {"vc_painted_planks_yellow"}, 0},
    /* T_PAINTED_PLANKS green  */ {-1, "anniversary", {"vc_painted_planks_green"}, 0},
    /* T_ANNIV_TOKEN    */ {-1, "anniversary", {"vc_anniversary_token"}, 0},
    /* T_ANNIV_MEDAL    */ {-1, "anniversary", {"vc_anniversary_medal"}, 0},
    /* T_PARTY_POPPER   */ {-1, "anniversary", {"vc_party_popper"}, 0},
    /* T_POWERSTONE_BRICKS */ {-1, "blocks", {"vc_powerstone_bricks"}, 0},
    /* T_CRACKED_POWERSTONE_BRICKS */ {-1, "blocks", {"vc_cracked_powerstone_bricks"}, 0},
    /* T_CHISELED_POWERSTONE_BRICKS */ {-1, "blocks", {"vc_chiseled_powerstone_bricks"}, 0},
    /* T_POWERSTONE_GLASS */ {-1, "blocks", {"vc_powerstone_glass"}, 0},
    /* T_DEBRIS_GRAVEL  */ {-1, "blocks", {"suspicious_gravel_0", "vc_debris_gravel"}, 0},
    /* T_SKULL          */ {-1, "blocks", {"vc_skeleton_head"}, 0},
    /* T_SKULL_FACE     */ {-1, "blocks", {"vc_skeleton_head_face"}, 0},
    /* T_WASTELAND_GRASS_TOP */ {-1, "blocks", {"vc_wasteland_grass_top"}, 0},
    /* T_WASTELAND_GRASS_SIDE */ {-1, "blocks", {"vc_wasteland_grass_side"}, 0},
    /* T_DRY_GRASS      */ {-1, "blocks", {"vc_dry_grass", "short_dry_grass"}, 0},
    /* T_GRASSER_FLOWER */ {-1, "blocks", {"vc_grasser_flower"}, 0},
    /* T_FERN           */ {-1, "blocks", {"fern_carried", "fern"}, 0x79C05A},
    /* T_CRACKED_NETHER_BRICKS */ {-1, "blocks", {"cracked_nether_bricks"}, 0},
    /* T_CHISELED_NETHER_BRICKS */ {-1, "blocks", {"chiseled_nether_bricks"}, 0},
    /* T_BRUSHER        */ {-1, "items", {"vc_brusher", "brush"}, 0},
    /* T_FORTRESS_COMPASS */ {-1, "items", {"vc_fortress_compass"}, 0},
    /* T_DOCUMENT       */ {-1, "items", {"vc_old_document"}, 0},
    /* T_ANCIENT_COIN   */ {-1, "items", {"vc_ancient_coin"}, 0},
    /* T_POWERSTONE_RELIC */ {-1, "items", {"vc_powerstone_relic"}, 0},
    /* T_POTTERY_SHARD  */ {-1, "items", {"vc_pottery_shard"}, 0},
    /* T_EGG_HUSKING    */ {-1, "items", {"vc_spawn_egg_husking"}, 0},
    /* T_HUSKING_SKIN   */ {-1, "mobs", {"vc_husking_skin"}, 0},
    /* T_HUSKING_FACE   */ {-1, "mobs", {"vc_husking_face"}, 0},
    /* T_HUSKING_ROBE   */ {-1, "mobs", {"vc_husking_robe"}, 0},
    /* T_FIREBALL       */ {-1, "mobs", {"vc_fireball"}, 0},
    /* T_VOID_BRICK_EYE */ {-1, "blocks", {"vc_void_brick_eye"}, 0},
    /* T_CRIMSON_STONE  */ {-1, "blocks", {"vc_crimson_stone"}, 0},
    /* T_TORCH          */ {-1, "blocks", {"vc_torch"}, 0},
    /* T_CRIMSON_PLANKS */ {-1, "blocks", {"vc_crimson_planks"}, 0},
    /* T_CRIMSON_LOG_SIDE */ {-1, "blocks", {"vc_crimson_log"}, 0},
    /* T_CRIMSON_LOG_TOP */ {-1, "blocks", {"vc_crimson_log_top"}, 0},
    /* T_SHIELD         */ {-1, "items", {"vc_shield"}, 0},
    /* T_EXPLORER_MAP   */ {-1, "items", {"vc_explorer_map"}, 0},
    /* T_EGG_HOGMAN     */ {-1, "items", {"vc_spawn_egg_hogman"}, 0},
    /* T_HOGMAN_SKIN    */ {-1, "mobs", {"vc_hogman_skin"}, 0},
    /* T_HOGMAN_FACE    */ {-1, "mobs", {"vc_hogman_face"}, 0},
    /* T_HOGMAN_TUNIC   */ {-1, "mobs", {"vc_hogman_tunic"}, 0},
    /* T_PS_FURNACE_FRONT */ {-1, "blocks", {"vc_powerstone_furnace_front"}, 0},
    /* T_PS_FURNACE_FRONT_ON */ {-1, "blocks", {"vc_powerstone_furnace_front_on"}, 0},
    /* T_WRITABLE_BOOK  */ {-1, "items", {"vc_writable_book"}, 0},
    /* T_WRITTEN_BOOK   */ {-1, "items", {"vc_written_book"}, 0},
    /* T_DEEPSTONE      */ {-1, "blocks", {"deepslate", "vc_deepstone"}, 0},
    /* T_HARDIUM_ORE    */ {-1, "blocks", {"vc_hardium_ore"}, 0},
    /* T_MOSSY_DEEPSTONE*/ {-1, "blocks", {"vc_mossy_deepstone"}, 0},
    /* T_MOSS_BLOCK     */ {-1, "blocks", {"moss_block", "vc_moss_block"}, 0},
    /* T_FUNGAL_DEEPSTONE*/ {-1, "blocks", {"vc_fungal_deepstone"}, 0},
    /* T_DEEPSTONE_BRICKS*/ {-1, "blocks", {"deepslate_bricks", "vc_deepstone_bricks"}, 0},
    /* T_CRACKED_DEEPSTONE_BRICKS*/ {-1, "blocks", {"cracked_deepslate_bricks", "vc_cracked_deepstone_bricks"}, 0},
    /* T_MOSSY_DEEPSTONE_BRICKS*/ {-1, "blocks", {"vc_mossy_deepstone_bricks"}, 0},
    /* T_ROTTEN_PLANKS  */ {-1, "blocks", {"vc_rotten_planks"}, 0},
    /* T_VINES          */ {-1, "blocks", {"vine", "vc_vines"}, 0},
    /* T_HARDIUM        */ {-1, "items", {"vc_hardium"}, 0},
    /* T_HARDIUM_PICKAXE*/ {-1, "items", {"vc_hardium_pickaxe"}, 0},
    /* T_SPAWNER        */ {-1, "blocks", {"spawner", "mob_spawner"}, 0},
    /* T_SOULED_GRASS_TOP*/ {-1, "blocks", {"vc_souled_grass_top"}, 0},
    /* T_SOULED_GRASS_SIDE*/ {-1, "blocks", {"vc_souled_grass_side"}, 0},
    /* T_SOULED_DIRT    */ {-1, "blocks", {"vc_souled_dirt"}, 0},
    /* T_SOULED_STONE   */ {-1, "blocks", {"vc_souled_stone"}, 0},
    /* T_CELESTIAL_FRAME_TOP*/ {-1, "blocks", {"vc_celestial_frame_top"}, 0},
    /* T_CELESTIAL_FRAME_SIDE*/ {-1, "blocks", {"vc_celestial_frame_side"}, 0},
    /* T_CELESTIAL_FRAME_FULL*/ {-1, "blocks", {"vc_celestial_frame_full"}, 0},
    /* T_CELESTIAL_PORTAL*/ {-1, "blocks", {"vc_celestial_portal"}, 0},
    /* T_CRIMSON_HEAD   */ {-1, "items", {"vc_crimson_head"}, 0},
    /* T_CELESTIAL_COMPASS*/ {-1, "items", {"vc_celestial_compass"}, 0},
    /* T_HARDIUM_SWORD  */ {-1, "items", {"vc_hardium_sword"}, 0},
    /* T_HARDIUM_AXE    */ {-1, "items", {"vc_hardium_axe"}, 0},
    /* T_HARDIUM_SHOVEL */ {-1, "items", {"vc_hardium_shovel"}, 0},
    /* T_HARDIUM_ARMOR  */ {-1, "items", {"vc_hardium_helmet"}, 0},
    /* +1               */ {-1, "items", {"vc_hardium_chestplate"}, 0},
    /* +2               */ {-1, "items", {"vc_hardium_leggings"}, 0},
    /* +3               */ {-1, "items", {"vc_hardium_boots"}, 0},
    /* T_HARDIUM_PLATE  */ {-1, "blocks", {"vc_hardium_plate"}, 0},
    /* T_CELESTIAL_LOG_SIDE*/ {-1, "blocks", {"vc_celestial_log"}, 0},
    /* T_CELESTIAL_LOG_TOP*/ {-1, "blocks", {"vc_celestial_log_top"}, 0},
    /* T_CELESTIAL_PLANKS*/ {-1, "blocks", {"vc_celestial_planks"}, 0},
    /* T_CELESTIAL_STONE_BRICKS*/ {-1, "blocks", {"vc_celestial_stone_bricks"}, 0},
    /* T_CELESTIAL_LEAVES*/ {-1, "blocks", {"vc_celestial_leaves"}, 0},
    /* T_CELESTIAL_GRASS_TOP*/ {-1, "blocks", {"vc_celestial_grass_top"}, 0},
    /* T_CELESTIAL_GRASS_SIDE*/ {-1, "blocks", {"vc_celestial_grass_side"}, 0},
    /* T_CELESTIAL_DIRT */ {-1, "blocks", {"vc_celestial_dirt"}, 0},
    /* T_CELESTIAL_STONE*/ {-1, "blocks", {"vc_celestial_stone"}, 0},
    /* T_DUSTY_CONCRETE */ {-1, "blocks", {"vc_dusty_concrete"}, 0},
    /* T_HARDENED_CONCRETE*/ {-1, "blocks", {"vc_hardened_concrete"}, 0},
    /* T_GRAY_LOG_SIDE  */ {-1, "blocks", {"vc_gray_log"}, 0},
    /* T_GRAY_LOG_TOP   */ {-1, "blocks", {"vc_gray_log_top"}, 0},
    /* T_GRAY_PLANKS    */ {-1, "blocks", {"vc_gray_planks"}, 0},
    /* T_RED_LEAVES     */ {-1, "blocks", {"vc_red_leaves"}, 0},
    /* T_ORANGE_LEAVES  */ {-1, "blocks", {"vc_orange_leaves"}, 0},
    /* T_YELLOW_LEAVES  */ {-1, "blocks", {"vc_yellow_leaves"}, 0},
    /* T_CUTTER_TOP     */ {-1, "blocks", {"stonecutter_top", "vc_cutter_top"}, 0},
    /* T_CUTTER_SIDE    */ {-1, "blocks", {"stonecutter_side", "vc_cutter_side"}, 0},
    /* T_BARREL_TOP     */ {-1, "blocks", {"barrel_top", "vc_barrel_top"}, 0},
    /* T_BARREL_SIDE    */ {-1, "blocks", {"barrel_side", "vc_barrel_side"}, 0},
    /* T_WOOD_CUTTER_TOP*/ {-1, "blocks", {"vc_wood_cutter_top"}, 0},
    /* T_WOOD_CUTTER_SIDE*/ {-1, "blocks", {"vc_wood_cutter_side"}, 0},
    /* T_GRASS_TOP_ORANGE*/ {-1, "blocks", {"vc_grass_top_orange"}, 0},
    /* T_GRASS_SIDE_ORANGE*/ {-1, "blocks", {"vc_grass_side_orange"}, 0},
    /* T_GRASS_TOP_DARK */ {-1, "blocks", {"vc_grass_top_dark"}, 0},
    /* T_GRASS_SIDE_DARK*/ {-1, "blocks", {"vc_grass_side_dark"}, 0},
    /* T_GRAY_SAPLING   */ {-1, "blocks", {"vc_gray_sapling"}, 0},
    /* T_CELESTIAL_SAPLING*/ {-1, "blocks", {"vc_celestial_sapling"}, 0},
    /* concrete powder */ {-1, "blocks", {"white_concrete_powder", "vc_white_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"orange_concrete_powder", "vc_orange_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"magenta_concrete_powder", "vc_magenta_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"light_blue_concrete_powder", "vc_light_blue_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"yellow_concrete_powder", "vc_yellow_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"lime_concrete_powder", "vc_lime_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"pink_concrete_powder", "vc_pink_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"gray_concrete_powder", "vc_gray_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"light_gray_concrete_powder", "vc_light_gray_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"cyan_concrete_powder", "vc_cyan_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"purple_concrete_powder", "vc_purple_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"blue_concrete_powder", "vc_blue_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"brown_concrete_powder", "vc_brown_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"green_concrete_powder", "vc_green_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"red_concrete_powder", "vc_red_concrete_powder"}, 0},
    /* concrete powder */ {-1, "blocks", {"black_concrete_powder", "vc_black_concrete_powder"}, 0},
    /* concrete        */ {-1, "blocks", {"white_concrete", "vc_white_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"orange_concrete", "vc_orange_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"magenta_concrete", "vc_magenta_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"light_blue_concrete", "vc_light_blue_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"yellow_concrete", "vc_yellow_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"lime_concrete", "vc_lime_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"pink_concrete", "vc_pink_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"gray_concrete", "vc_gray_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"light_gray_concrete", "vc_light_gray_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"cyan_concrete", "vc_cyan_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"purple_concrete", "vc_purple_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"blue_concrete", "vc_blue_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"brown_concrete", "vc_brown_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"green_concrete", "vc_green_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"red_concrete", "vc_red_concrete"}, 0},
    /* concrete        */ {-1, "blocks", {"black_concrete", "vc_black_concrete"}, 0},
    /* dye             */ {-1, "items", {"white_dye", "vc_white_dye"}, 0},
    /* dye             */ {-1, "items", {"orange_dye", "vc_orange_dye"}, 0},
    /* dye             */ {-1, "items", {"magenta_dye", "vc_magenta_dye"}, 0},
    /* dye             */ {-1, "items", {"light_blue_dye", "vc_light_blue_dye"}, 0},
    /* dye             */ {-1, "items", {"yellow_dye", "vc_yellow_dye"}, 0},
    /* dye             */ {-1, "items", {"lime_dye", "vc_lime_dye"}, 0},
    /* dye             */ {-1, "items", {"pink_dye", "vc_pink_dye"}, 0},
    /* dye             */ {-1, "items", {"gray_dye", "vc_gray_dye"}, 0},
    /* dye             */ {-1, "items", {"light_gray_dye", "vc_light_gray_dye"}, 0},
    /* dye             */ {-1, "items", {"cyan_dye", "vc_cyan_dye"}, 0},
    /* dye             */ {-1, "items", {"purple_dye", "vc_purple_dye"}, 0},
    /* dye             */ {-1, "items", {"blue_dye", "vc_blue_dye"}, 0},
    /* dye             */ {-1, "items", {"brown_dye", "vc_brown_dye"}, 0},
    /* dye             */ {-1, "items", {"green_dye", "vc_green_dye"}, 0},
    /* dye             */ {-1, "items", {"red_dye", "vc_red_dye"}, 0},
    /* dye             */ {-1, "items", {"black_dye", "vc_black_dye"}, 0},
    /* crimsonium_log   */ {-1, "blocks", {"vc_crimsonium_log"}, 0},
    /* crimsonium_log_t */ {-1, "blocks", {"vc_crimsonium_log_top"}, 0},
    /* crimsonium_plank */ {-1, "blocks", {"vc_crimsonium_planks"}, 0},
    /* crimsonium_leave */ {-1, "blocks", {"vc_crimsonium_leaves"}, 0},
    /* shattered_stone  */ {-1, "blocks", {"vc_shattered_stone"}, 0},
    /* shattered_bricks */ {-1, "blocks", {"vc_shattered_bricks"}, 0},
    /* void_shard_block */ {-1, "blocks", {"vc_void_shard_block"}, 0},
    /* dark_sand        */ {-1, "blocks", {"vc_dark_sand"}, 0},
    /* dark_sandstone   */ {-1, "blocks", {"vc_dark_sandstone"}, 0},
    /* shard_cluster    */ {-1, "blocks", {"vc_shard_cluster"}, 0},
    /* crimsonium_ore   */ {-1, "blocks", {"vc_crimsonium_ore"}, 0},
    /* void_shard       */ {-1, "items", {"vc_void_shard"}, 0},
    /* crimsonium       */ {-1, "items", {"vc_crimsonium"}, 0},
};
static_assert(sizeof(kSources) / sizeof(kSources[0]) == T_COUNT, "a texture source for every tile");

bool fileExists(const std::string& path) {
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        std::fclose(f);
        return true;
    }
    return false;
}

Image findTexture(const std::string& dir, const TileSource& src) {
    for (const char* name : src.names)
        for (const char* ext : {".png", ".tga"}) {
            std::string path = dir + "/" + src.folder + "/" + name + ext;
            if (fileExists(path)) {
                Image img = loadImage(path);
                if (img.valid()) return img;
            }
        }
    return {};
}

// Tints mostly-gray textures, the way the game colours grass, leaves and water.
void tintIfGray(std::vector<uint8_t>& px, uint32_t tint) {
    if (!tint) return;
    int opaque = 0, gray = 0;
    for (size_t i = 0; i < px.size(); i += 4) {
        if (px[i + 3] < 16) continue;
        ++opaque;
        int mx = std::max({px[i], px[i + 1], px[i + 2]}), mn = std::min({px[i], px[i + 1], px[i + 2]});
        if (mx - mn < 16) ++gray;
    }
    if (opaque == 0 || gray * 10 < opaque * 9) return;
    const int tr = (tint >> 16) & 255, tg = (tint >> 8) & 255, tb = tint & 255;
    for (size_t i = 0; i < px.size(); i += 4) {
        px[i] = (uint8_t)(px[i] * tr / 255);
        px[i + 1] = (uint8_t)(px[i + 1] * tg / 255);
        px[i + 2] = (uint8_t)(px[i + 2] * tb / 255);
    }
}

// Copies a square region (sx, sy, side) of `src` into tile `tile` of the atlas, nearest-neighbour scaled.
void blitTile(Atlas& atlas, int tile, const uint8_t* src, int srcStride, int sx, int sy, int side, uint32_t tint) {
    const int res = atlas.size / kAtlasCols;
    std::vector<uint8_t> px(res * res * 4);
    for (int y = 0; y < res; ++y)
        for (int x = 0; x < res; ++x) {
            const uint8_t* s = src + ((sy + y * side / res) * srcStride + (sx + x * side / res)) * 4;
            std::copy(s, s + 4, &px[(y * res + x) * 4]);
        }
    tintIfGray(px, tint);
    const int ox = (tile % kAtlasCols) * res, oy = (tile / kAtlasCols) * res;
    for (int y = 0; y < res; ++y)
        std::copy(&px[y * res * 4], &px[(y + 1) * res * 4], &atlas.rgba[((oy + y) * atlas.size + ox) * 4]);
}

}  // namespace

bool blitTextureFile(Atlas& atlas, int tile, const std::string& path) {
    if (tile < 0 || tile >= kAtlasCols * kAtlasCols || atlas.size <= 0) return false;
    Image img = loadImage(path);
    if (!img.valid()) return false;
    blitTile(atlas, tile, img.rgba.data(), img.w, 0, 0, std::min(img.w, img.h), 0);
    return true;
}

bool imageSize(const std::string& path, int& w, int& h) {
    int n = 0;
    return stbi_info(path.c_str(), &w, &h, &n) != 0;
}

std::string tileFilePath(int tile) {
    if (tile < 0 || tile >= T_COUNT) return "";
    const TileSource& src = kSources[tile];
    return std::string(src.folder) + "/" + *src.names.begin() + ".png";
}

Atlas loadTexturePack(const std::string& dir, const std::vector<uint8_t>& fallback, int fallbackSize) {
    Image terrain = loadImage(dir + "/terrain.png");
    if (terrain.valid() && (terrain.w != terrain.h || terrain.w % 16)) terrain = {};

    // Tile resolution: whatever the pack uses (16px classic, up to 64px HD packs).
    int res = 16;
    if (terrain.valid()) res = terrain.w / 16;
    else if (Image stone = findTexture(dir, kSources[T_STONE]); stone.valid()) res = stone.w;
    res = std::clamp(res, 16, 64);

    Atlas atlas;
    atlas.size = res * kAtlasCols;
    atlas.rgba.assign(atlas.size * atlas.size * 4, 0);
    // Start from the placeholders so missing tiles still look reasonable.
    const int fres = fallbackSize / kAtlasCols;
    for (int t = 0; t < kAtlasCols * kAtlasCols; ++t)
        blitTile(atlas, t, fallback.data(), fallbackSize, (t % kAtlasCols) * fres, (t / kAtlasCols) * fres, fres, 0);

    for (int t = 0; t < T_COUNT; ++t) {
        const TileSource& src = kSources[t];
        Image img = findTexture(dir, src);
        if (img.valid()) {
            // Animated textures are vertical strips of frames: use the first one.
            int side = std::min(img.w, img.h);
            blitTile(atlas, t, img.rgba.data(), img.w, 0, 0, side, src.tint);
            ++atlas.tilesFromPack;
        } else if (terrain.valid() && src.terrainIndex >= 0) {
            int tres = terrain.w / 16;
            int sx = (src.terrainIndex % 16) * tres, sy = (src.terrainIndex / 16) * tres;
            bool empty = true;
            for (int y = 0; y < tres && empty; ++y)
                for (int x = 0; x < tres && empty; ++x)
                    if (terrain.rgba[((sy + y) * terrain.w + sx + x) * 4 + 3]) empty = false;
            if (empty) continue;
            blitTile(atlas, t, terrain.rgba.data(), terrain.w, sx, sy, tres, src.tint);
            ++atlas.tilesFromPack;
        }
    }
    return atlas;
}

}  // namespace vc

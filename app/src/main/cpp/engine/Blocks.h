#pragma once
#include <cstdint>

namespace vc {

// Block IDs follow the classic Pocket Edition numbering so that worlds
// and assets from the original game can be mapped 1:1 later on.
// Block and item ids are 16 bits since 1.4.0 (mods get ids from kFirstModId up).
using ItemId = uint16_t;
constexpr int kItemIdLimit = 65000;  // ids are 1..64999; 0 is air
constexpr int kModIdBase = 2000;     // blocks and items of VoxelCraft Mods (see Mods.h) start here

enum BlockId : uint16_t {
    AIR = 0,
    STONE = 1,
    GRASS = 2,
    DIRT = 3,
    COBBLESTONE = 4,
    PLANKS = 5,
    BEDROCK = 7,
    WATER = 9,
    LAVA = 11,
    SAND = 12,
    GRAVEL = 13,
    IRON_ORE = 15,
    COAL_ORE = 16,
    TALL_GRASS = 31,
    LOG = 17,
    LEAVES = 18,
    GLASS = 20,
    DANDELION = 37,
    POPPY = 38,
    BRICKS = 45,
    OBSIDIAN = 49,
    CRAFTING_TABLE = 58,
    FURNACE = 61,
    LIT_FURNACE = 62,
    NETHERRACK = 87,
    WHEAT_CROP2 = 88,   // 1.3.0 (was soul sand before 1.2.0)
    WHEAT_RIPE = 89,    // 1.3.0 (was glowstone)
    PORTAL = 90,
    NETHER_BRICK = 112,
    HAY_BALE = 153,     // 1.3.0 (was nether quartz ore)
    // Building blocks (1.0)
    SANDSTONE = 24,
    WOOL = 35,  // white; the other colours follow from WOOL_ORANGE
    BOOKSHELF = 47,
    MOSSY_COBBLESTONE = 48,
    SNOW_BLOCK = 80,
    CLAY = 82,
    STONE_BRICKS = 98,
    WOOL_ORANGE = 164,  // 164..178: orange, magenta, light blue, yellow, lime, pink, gray,
                        // light gray, cyan, purple, blue, brown, green, red, black
    WOOL_BLACK = 178,
    // VoxelCraft ores and their storage blocks (1.0)
    BRONZE_ORE = 180,
    STEEL_ORE = 181,
    RUBY_ORE = 182,
    BRONZE_BLOCK = 183,
    STEEL_BLOCK = 184,
    RUBY_BLOCK = 185,
    // Update 1.0.0 (build 11): biomes, structures, doors
    LOOT_CHEST_RUIN = 50,      // look like chests, filled with loot the first time they open
    LOOT_CHEST_SHIP = 51,
    LOOT_CHEST_DUNGEON = 52,
    LOOT_CHEST_MINE = 53,
    CHEST = 54,
    ICE = 79,
    CACTUS = 81,
    FENCE = 85,
    HARDENED_STONE = 159,
    SPRUCE_PLANKS = 160,
    SPRUCE_LEAVES = 161,
    SPRUCE_LOG = 162,
    // Doors: base + facing (0 north, 1 east, 2 south, 3 west) + 4 if open + 8 for the upper half.
    OAK_DOOR = 113,   // 113..128
    IRON_DOOR = 129,  // 129..144
    // Trapdoor: base + facing + 4 if open + 8 if it sits at the top of the block.
    OAK_TRAPDOOR = 63,  // 63..78
    // Update 1.1.0: the new Nether, the Crimson Void, mobs and beds
    NETHER_GRASS = 6,
    NETHER_SPROUTS = 8,
    VOID_STONE = 10,
    VOID_GRASS = 14,
    VOID_LOG = 19,
    EYE_LOG = 21,
    VOID_LEAVES = 22,
    RED_SAND = 23,
    VOID_PORTAL = 25,
    VOID_STONE_BRICKS = 26,
    PUMPKIN = 27,
    CRIMSON_ROOTS = 28,
    LOOT_CHEST_FORTRESS = 55,
    // Bed: base + facing * 2 + 1 for the head (the foot is where the player places it).
    BED = 145,  // 145..152
    // Update 1.2.0: powerstone, TNT, new woods and saplings, the world's end
    WORLD_END = 29,          // the very bottom of the world: nothing breaks it
    TNT = 30,
    NUKE_TNT = 32,           // Halloween addon
    NETHERPERK_LOG = 33,
    NETHERPERK_CAP = 34,     // the glowing crown of a netherperk tree
    NETHERPERK_PLANKS = 36,
    VOID_PLANKS = 39,
    OAK_SAPLING = 40,
    PINE_SAPLING = 41,
    VOID_SAPLING = 42,
    NETHERPERK_SAPLING = 43,
    POWERSTONE_ORE = 44,
    POWERSTONE_BLOCK = 46,   // always powered
    POWER_CABLE = 56,
    POWER_CABLE_ON = 57,
    POWERSTONE_LAMP = 59,
    POWERSTONE_LAMP_ON = 60,
    SWITCH = 83,
    SWITCH_ON = 84,
    POWERSTONE_LIGHT = 86,   // the Nether's light (instead of glowstone)
    GIFT_BOX = 91,           // Winter addon
    CAKE = 92,               // Anniversary addon
    LOOT_CHEST_VOID = 93,    // Crimson Void dungeons and towers
    LOOT_CHEST_TEMPLE = 94,  // desert temples
    LOOT_CHEST_IGLOO = 95,
    LOOT_CHEST_TOWER = 96,   // overworld watchtowers
    // Items below 200 (the space above is full): 99..101 and 186..199.
    POWERSTONE_DUST = 99,
    POWERSTONE_CHIP = 100,
    CANDY_CANE = 101,
    // 1.3.0: farming, animals, the bow and villages
    FARMLAND = 97,
    WHEAT_SEEDS = 163,   // also the first stage of the crop
    WHEAT_CROP1 = 179,
    HOE = 102, BOW = 103, ARROW = 104, FEATHER = 105, WHEAT = 106, BREAD = 107,
    RAW_CHICKEN = 108, COOKED_CHICKEN = 109, RAW_BEEF = 110, STEAK = 111,
    MILK_BUCKET = 154, EGG_CHICKEN = 155, EGG_COW = 156, EGG_VILLAGER = 157,
    BUCKET = 186,
    WATER_BUCKET = 187,
    LAVA_BUCKET = 188,
    BOSS_COMPASS = 189,
    WOOD_SWORD = 190, STONE_SWORD = 191, IRON_SWORD = 192, BRONZE_SWORD = 193, STEEL_SWORD = 194, RUBY_SWORD = 195,
    EGG_PIG = 196, EGG_SHEEP = 197, EGG_SLIME = 198, EGG_FORGOTTEN = 199,
    // Ids from 200 to 255 are items, never stored in the world.
    STICK = 200,
    COAL = 201,
    IRON_INGOT = 202,
    FLINT = 203,
    BRONZE_INGOT = 204,
    STEEL_INGOT = 205,
    RUBY = 206,
    OAK_DOOR_ITEM = 207,
    IRON_DOOR_ITEM = 208,
    BOAT = 209,
    WOOD_PICKAXE = 210,
    STONE_PICKAXE = 211,
    IRON_PICKAXE = 212,
    WOOD_AXE = 213,
    STONE_AXE = 214,
    IRON_AXE = 215,
    WOOD_SHOVEL = 216,
    STONE_SHOVEL = 217,
    IRON_SHOVEL = 218,
    // Tools of the new metals: pickaxe, axe, shovel for bronze, steel and ruby.
    BRONZE_PICKAXE = 219, BRONZE_AXE = 220, BRONZE_SHOVEL = 221,
    STEEL_PICKAXE = 222, STEEL_AXE = 223, STEEL_SHOVEL = 224,
    RUBY_PICKAXE = 225, RUBY_AXE = 226, RUBY_SHOVEL = 227,
    // Armour: 228 + material * 4 + piece (helmet, chestplate, leggings, boots) for the
    // materials wood, cobblestone, bronze, steel and ruby.
    FIRST_ARMOR = 228,
    LAST_ARMOR = 247,
    VOID_FLINT_AND_STEEL = 248,
    RAW_PORK = 249,
    COOKED_PORK = 250,
    RAW_MUTTON = 251,
    COOKED_MUTTON = 252,
    BED_ITEM = 253,
    APPLE = 254,
    FLINT_AND_STEEL = 255,
    // 1.4.0: ids are 16-bit, new blocks start at 1000.
    // Stairs: STAIRS + material * 8 + facing (the tall back side: 0 north, 1 east, 2 south, 3 west) + 4 upside down.
    STAIRS = 1000,  // 1000..1095
    // Slabs: SLAB + material * 3 + 0 bottom half, 1 top half, 2 double (a full block).
    SLAB = 1100,    // 1100..1135
    // 1.4.0 caves: tar lakes deep down, dripstone; stained glass in the 16 wool colours.
    TAR = 1200,             // a thick black liquid
    TAR_BLOCK = 1201,       // hardened tar
    DRIPSTONE_BLOCK = 1202,
    DRIPSTONE_UP = 1203,    // pointed dripstone growing from the floor
    DRIPSTONE_DOWN = 1204,  // and hanging from the ceiling
    SNOW_LAYER = 1205,      // a thin cover of snow that builds up while it snows
    // Power cables and switches on walls: base + side of the wall (0 north, 1 east, 2 south, 3 west) + 4 when on.
    WALL_CABLE = 1230,      // 1230..1237
    WALL_SWITCH = 1240,     // 1240..1247
    STAINED_GLASS = 1210,   // 1210..1225: white, then the order of WOOL_ORANGE..WOOL_BLACK
    // 1.4.0 items
    STRING = 1300,
    MAP = 1301,
    TAR_BALL = 1302,
    EGG_SPIDER = 1303, EGG_BAT = 1304, EGG_TNT_ZOMBIE = 1305, EGG_TAR_MONSTER = 1306,
    // 1.4.0 Anniversary modpack (with the Anniversary addon on)
    ANNIV_LOGO = 1400,       // the VoxelCraft emblem
    CONFETTI_BLOCK = 1401,
    BALLOON = 1402,          // 1402..1404: red, blue, yellow
    PARTY_LANTERN = 1405,
    FESTIVE_BRICKS = 1406,
    PAINTED_PLANKS = 1407,   // 1407..1410: red, blue, yellow, green
    ANNIV_TOKEN = 1420,      // collectibles hidden in chests
    ANNIV_MEDAL = 1421,      // made from five tokens
    PARTY_POPPER = 1422,
    // 1.5.0 Forgotten Powerstone: powerstone stairs live here (the first range is full).
    STAIRS_EXTRA = 1140,     // 1140..1147: material 12 (powerstone bricks)
    EGG_HUSKING = 1307,
    // Blocks of the forgotten civilization, the wasteland and the fortress.
    POWERSTONE_BRICKS = 1500,
    CRACKED_POWERSTONE_BRICKS = 1501,
    CHISELED_POWERSTONE_BRICKS = 1502,
    POWERSTONE_GLASS = 1503,
    DEBRIS_GRAVEL = 1504,    // hides finds; the brusher uncovers them
    SKELETON_HEAD = 1505,    // 1505..1508: + turn (the face looks 0 south, 1 west, 2 north, 3 east)
    WASTELAND_GRASS = 1509,  // the dry grass of the Powerstone Wasteland (Cornerstone Dimension), on cornerstone
    DRY_GRASS = 1510,
    GRASSER_FLOWER = 1511,
    FERN = 1512,
    CRACKED_NETHER_BRICKS = 1513,
    CHISELED_NETHER_BRICKS = 1514,
    LOOT_CHEST_FORGOTTEN = 1515,  // forgotten villages
    LOOT_CHEST_DROWNED = 1516,    // drowned ruins
    LOOT_CHEST_PORTAL = 1517,     // broken portal ruins
    // Fences of other materials (FENCE is oak).
    SPRUCE_FENCE = 1520, VOID_FENCE = 1521, NETHERPERK_FENCE = 1522, NETHER_BRICK_FENCE = 1523, POWERSTONE_FENCE = 1524,
    // 1.5.1: an eye set into void bricks that watches the boss arenas (not craftable).
    VOID_BRICK_EYE = 1525,
    CRIMSON_STONE = 1526,    // the rock formations of the Crimson Desert
    // 1.5.7: torches (on the floor, and on a wall: 1528..1531 hang on the wall to the 0 north,
    // 1 east, 2 south, 3 west) and the crimson wood of the Cornerstone Dimension's mineshafts.
    TORCH = 1527,
    TORCH_WALL = 1528,
    CRIMSON_PLANKS = 1532,
    CRIMSON_LOG = 1533,
    // 1.5.7: the powerstone furnace: three times faster, and fuelled by power when wired up.
    POWERSTONE_FURNACE = 1534,
    POWERSTONE_FURNACE_ON = 1535,
    // 1.5.7: a power cable on a ceiling (off, on).
    CEILING_CABLE = 1536,
    CEILING_CABLE_ON = 1537,
    // 1.5.0 items
    BRUSHER = 1600,
    FORTRESS_COMPASS = 1601,
    OLD_DOCUMENT = 1602,     // 1602..1609: the eight pages of the forgotten kingdom's story
    ANCIENT_COIN = 1610,
    POWERSTONE_RELIC = 1611,
    POTTERY_SHARD = 1612,
    // 1.5.7 items
    SHIELD = 1613,           // in the off hand it stops attacks from the front
    EXPLORER_MAP = 1614,     // points the way to the nearest mineshaft (Exploration Maps experiment)
    WRITABLE_BOOK = 1615,    // 1.5.7 book and quill: `damage` holds the book's number in the world's books
    WRITTEN_BOOK = 1616,     // a signed book: read only
    EGG_HOGMAN = 1308,       // the hog man of the Cornerstone Dimension
};
inline bool isTorch(ItemId id) { return id >= TORCH && id < TORCH_WALL + 4; }
constexpr int kDocumentPages = 8;
inline bool isDocument(ItemId id) { return id >= OLD_DOCUMENT && id < OLD_DOCUMENT + kDocumentPages; }
inline bool isSkeletonHead(ItemId id) { return id >= SKELETON_HEAD && id < SKELETON_HEAD + 4; }
inline bool isFence(ItemId id) { return id == FENCE || (id >= SPRUCE_FENCE && id <= POWERSTONE_FENCE); }

// The materials stairs and slabs come in, in id order.
// Stairs of the first 12 materials are STAIRS + material * 8 + variant; later ones start at STAIRS_EXTRA.
constexpr int kShapeMaterials = 13;
constexpr int kFirstStairsMaterials = 12;
inline ItemId shapeMaterialBlock(int material) {
    const ItemId kBlocks[kShapeMaterials] = {PLANKS,  SPRUCE_PLANKS,     VOID_PLANKS, NETHERPERK_PLANKS,
                                             COBBLESTONE, STONE,         STONE_BRICKS, MOSSY_COBBLESTONE,
                                             BRICKS,  SANDSTONE,         NETHER_BRICK, VOID_STONE_BRICKS,
                                             POWERSTONE_BRICKS};
    return kBlocks[material < 0 || material >= kShapeMaterials ? 0 : material];
}
inline bool isStairs(ItemId id) {
    return (id >= STAIRS && id < STAIRS + kFirstStairsMaterials * 8) ||
           (id >= STAIRS_EXTRA && id < STAIRS_EXTRA + (kShapeMaterials - kFirstStairsMaterials) * 8);
}
// Which material a stair is made of, and its facing (bits 0-1) and upside-down bit (4).
inline int stairsMaterial(ItemId id) { return id >= STAIRS_EXTRA ? kFirstStairsMaterials + (id - STAIRS_EXTRA) / 8 : (id - STAIRS) / 8; }
inline int stairsVariant(ItemId id) { return id >= STAIRS_EXTRA ? (id - STAIRS_EXTRA) % 8 : (id - STAIRS) % 8; }
inline bool isSlab(ItemId id) { return id >= SLAB && id < SLAB + kShapeMaterials * 3; }
inline bool isDoubleSlab(ItemId id) { return isSlab(id) && (id - SLAB) % 3 == 2; }
inline ItemId stairsId(int material, int facing, bool upsideDown) {
    const int base = material < kFirstStairsMaterials ? STAIRS + material * 8 : STAIRS_EXTRA + (material - kFirstStairsMaterials) * 8;
    return (ItemId)(base + (facing & 3) + (upsideDown ? 4 : 0));
}
inline ItemId slabId(int material, int half) { return (ItemId)(SLAB + material * 3 + half); }  // 0 bottom, 1 top, 2 double
inline bool isStainedGlass(ItemId id) { return id >= STAINED_GLASS && id < STAINED_GLASS + 16; }
inline bool isDripstoneSpike(ItemId id) { return id == DRIPSTONE_UP || id == DRIPSTONE_DOWN; }
// Blocks with ids from 1000 up (everything else up there is an item).
inline bool isBalloon(ItemId id) { return id >= BALLOON && id < BALLOON + 3; }
inline bool isPaintedPlanks(ItemId id) { return id >= PAINTED_PLANKS && id < PAINTED_PLANKS + 4; }
// Everything of the Anniversary modpack: only usable while the addon is on.
inline bool isAnniversaryItem(ItemId id) { return id >= ANNIV_LOGO && id <= PARTY_POPPER; }
// Mod blocks and items (Mods.cpp).
enum Face : int;
bool modIsBlock(ItemId id);
int modTileFor(ItemId id, Face face);
bool modIsTransparent(ItemId id);
bool modIsEmissive(ItemId id);
int modFood(ItemId id);
inline bool isNewBlock(ItemId id) {
    if (id >= kModIdBase) return modIsBlock(id);
    return isStairs(id) || isSlab(id) || (id >= TAR && id <= SNOW_LAYER) || isStainedGlass(id) || (id >= WALL_CABLE && id < WALL_CABLE + 8) || (id >= WALL_SWITCH && id < WALL_SWITCH + 8) ||
           (id >= ANNIV_LOGO && id <= PAINTED_PLANKS + 3) || (id >= POWERSTONE_BRICKS && id <= CEILING_CABLE_ON && id != 1518 && id != 1519);
}
// The full block a stair or slab is cut from (its texture, hardness and tool).
inline ItemId shapeMaterial(ItemId id) {
    return isStairs(id) ? shapeMaterialBlock(stairsMaterial(id)) : isSlab(id) ? shapeMaterialBlock((id - SLAB) / 3) : id;
}

// The terrain atlas is a square of kAtlasCols x kAtlasCols tiles (32 x 32 since 1.4.0).
constexpr int kAtlasCols = 32;
constexpr float kTileUv = 1.0f / kAtlasCols;  // one tile in texture coordinates

// Tiles in the terrain atlas (see buildFallbackAtlas).
enum Tile : uint16_t {
    T_STONE, T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_COBBLE, T_PLANKS, T_BEDROCK,
    T_WATER, T_SAND, T_GRAVEL, T_LOG_SIDE, T_LOG_TOP, T_LEAVES, T_GLASS, T_BRICKS,
    T_LAVA, T_OBSIDIAN, T_NETHERRACK, T_FARMLAND, T_HAY_TOP, T_PORTAL, T_NETHER_BRICK,
    T_HAY_SIDE, T_FLINT_AND_STEEL, T_APPLE,
    T_COAL_ORE, T_IRON_ORE, T_CRAFTING_TOP, T_CRAFTING_SIDE, T_CRAFTING_FRONT,
    T_FURNACE_TOP, T_FURNACE_SIDE, T_FURNACE_FRONT, T_FURNACE_FRONT_LIT,
    T_STICK, T_COAL, T_IRON_INGOT, T_FLINT,
    T_WOOD_PICKAXE, T_STONE_PICKAXE, T_IRON_PICKAXE, T_WOOD_AXE, T_STONE_AXE, T_IRON_AXE,
    T_WOOD_SHOVEL, T_STONE_SHOVEL, T_IRON_SHOVEL,
    T_TALL_GRASS, T_DANDELION, T_POPPY,
    T_SANDSTONE_TOP, T_SANDSTONE_SIDE, T_BOOKSHELF, T_MOSSY_COBBLE, T_SNOW, T_CLAY, T_STONE_BRICKS,
    T_WOOL,  // 16 colours in a row
    T_BRONZE_ORE = T_WOOL + 16, T_STEEL_ORE, T_RUBY_ORE, T_BRONZE_BLOCK, T_STEEL_BLOCK, T_RUBY_BLOCK,
    T_BRONZE_INGOT, T_STEEL_INGOT, T_RUBY,
    T_FIRST_NEW_TOOL,                    // bronze, steel, ruby x pickaxe, axe, shovel
    T_FIRST_ARMOR = T_FIRST_NEW_TOOL + 9,  // 5 materials x 4 pieces
    T_SPRUCE_LOG_SIDE = T_FIRST_ARMOR + 20, T_SPRUCE_LOG_TOP, T_SPRUCE_LEAVES, T_SPRUCE_PLANKS, T_ICE,
    T_CACTUS_SIDE, T_CACTUS_TOP, T_HARDENED_STONE, T_CHEST_TOP, T_CHEST_SIDE, T_CHEST_FRONT,
    T_OAK_DOOR_LOWER, T_OAK_DOOR_UPPER, T_IRON_DOOR_LOWER, T_IRON_DOOR_UPPER, T_TRAPDOOR,
    T_OAK_DOOR_ITEM, T_IRON_DOOR_ITEM, T_BOAT,
    // 1.1.0
    T_NETHER_GRASS_TOP, T_NETHER_GRASS_SIDE, T_NETHER_SPROUTS, T_VOID_STONE, T_VOID_GRASS_TOP, T_VOID_GRASS_SIDE,
    T_VOID_LOG_SIDE, T_VOID_LOG_TOP, T_EYE_LOG, T_VOID_LEAVES, T_RED_SAND, T_VOID_PORTAL, T_VOID_STONE_BRICKS,
    T_PUMPKIN_SIDE, T_PUMPKIN_TOP, T_PUMPKIN_FACE, T_CRIMSON_ROOTS, T_BED_TOP_HEAD, T_BED_TOP_FOOT, T_BED_SIDE,
    T_VOID_FLINT_AND_STEEL, T_RAW_PORK, T_COOKED_PORK, T_RAW_MUTTON, T_COOKED_MUTTON, T_BED_ITEM,
    // Mob skins (box models textured from the atlas) and the sky.
    T_PIG_SKIN, T_PIG_FACE, T_SHEEP_WOOL, T_SHEEP_FACE, T_FORGOTTEN_FACE, T_FORGOTTEN_SHIRT, T_FORGOTTEN_SKIN,
    T_CRIMSON_BODY, T_CRIMSON_FACE, T_SUN, T_MOON, T_CLOUD, T_RAIN, T_SNOWFLAKE, T_STAR, T_SHEEP_SKIN,
    // 1.2.0
    T_WORLD_END, T_TNT_SIDE, T_TNT_TOP, T_TNT_BOTTOM, T_NUKE_SIDE, T_NETHERPERK_LOG_SIDE, T_NETHERPERK_LOG_TOP,
    T_NETHERPERK_CAP, T_NETHERPERK_PLANKS, T_VOID_PLANKS, T_OAK_SAPLING, T_PINE_SAPLING, T_VOID_SAPLING,
    T_NETHERPERK_SAPLING, T_POWERSTONE_ORE, T_POWERSTONE_BLOCK, T_CABLE, T_CABLE_ON, T_LAMP, T_LAMP_ON, T_SWITCH,
    T_SWITCH_ON, T_POWERSTONE_LIGHT, T_GIFT_SIDE, T_GIFT_TOP, T_CAKE_TOP, T_CAKE_SIDE, T_BUCKET, T_WATER_BUCKET,
    T_LAVA_BUCKET, T_COMPASS, T_FIRST_SWORD, T_EGG_PIG = T_FIRST_SWORD + 6, T_EGG_SHEEP, T_EGG_SLIME, T_EGG_FORGOTTEN,
    T_POWERSTONE_DUST, T_POWERSTONE_CHIP, T_CANDY_CANE, T_ZOMBIE_FACE, T_CONFETTI,
    // 1.3.0
    T_WHEAT0, T_WHEAT1, T_WHEAT2, T_WHEAT3, T_HOE, T_BOW, T_ARROW, T_FEATHER, T_WHEAT_ITEM, T_BREAD, T_RAW_CHICKEN,
    T_COOKED_CHICKEN, T_RAW_BEEF, T_STEAK, T_MILK_BUCKET, T_EGG_CHICKEN, T_EGG_COW, T_EGG_VILLAGER,
    T_CHICKEN_BODY, T_CHICKEN_FACE, T_COW_SKIN, T_COW_FACE, T_VILLAGER_FACE, T_VILLAGER_ROBE, T_VILLAGER_SKIN,
    T_CABLE_LINE, T_CABLE_LINE_ON, T_VILLAGER_SMITH, T_VILLAGER_FARMER, T_BOW_PULLED,
    // 1.4.0
    T_TAR, T_TAR_BLOCK, T_DRIPSTONE, T_DRIPSTONE_UP, T_DRIPSTONE_DOWN,
    T_STAINED_GLASS,  // 16 colours in a row
    T_STRING = T_STAINED_GLASS + 16, T_MAP, T_TAR_BALL, T_EGG_SPIDER, T_EGG_BAT, T_EGG_TNT_ZOMBIE, T_EGG_TAR_MONSTER,
    T_SPIDER_BODY, T_SPIDER_FACE, T_BAT_BODY, T_BAT_FACE, T_BAT_WING, T_TAR_SKIN, T_TAR_FACE, T_TNT_ZOMBIE_SHIRT,
    // 1.4.0 Anniversary modpack
    T_ANNIV_LOGO, T_ANNIV_LOGO_SIDE, T_CONFETTI_BLOCK, T_BALLOON,  // 3 balloon colours
    T_PARTY_LANTERN = T_BALLOON + 3, T_PARTY_LANTERN_TOP, T_FESTIVE_BRICKS, T_PAINTED_PLANKS,  // 4 colours
    T_ANNIV_TOKEN = T_PAINTED_PLANKS + 4, T_ANNIV_MEDAL, T_PARTY_POPPER,
    // 1.5.0 Forgotten Powerstone
    T_POWERSTONE_BRICKS, T_CRACKED_POWERSTONE_BRICKS, T_CHISELED_POWERSTONE_BRICKS, T_POWERSTONE_GLASS, T_DEBRIS_GRAVEL,
    T_SKULL, T_SKULL_FACE, T_WASTELAND_GRASS_TOP, T_WASTELAND_GRASS_SIDE, T_DRY_GRASS, T_GRASSER_FLOWER, T_FERN,
    T_CRACKED_NETHER_BRICKS, T_CHISELED_NETHER_BRICKS, T_BRUSHER, T_FORTRESS_COMPASS, T_DOCUMENT, T_ANCIENT_COIN,
    T_POWERSTONE_RELIC, T_POTTERY_SHARD, T_EGG_HUSKING, T_HUSKING_SKIN, T_HUSKING_FACE, T_HUSKING_ROBE, T_FIREBALL,
    // 1.5.1
    T_VOID_BRICK_EYE, T_CRIMSON_STONE,
    T_TORCH, T_CRIMSON_PLANKS, T_CRIMSON_LOG_SIDE, T_CRIMSON_LOG_TOP, T_SHIELD, T_EXPLORER_MAP, T_EGG_HOGMAN,
    T_HOGMAN_SKIN, T_HOGMAN_FACE, T_HOGMAN_TUNIC, T_PS_FURNACE_FRONT, T_PS_FURNACE_FRONT_ON,
    T_WRITABLE_BOOK, T_WRITTEN_BOOK,
    T_COUNT
};
static_assert(T_COUNT <= kAtlasCols * kAtlasCols, "the atlas has room for every tile");

enum Face : int { F_TOP, F_BOTTOM, F_NORTH, F_SOUTH, F_EAST, F_WEST };

inline bool isLiquid(ItemId id) { return id == WATER || id == LAVA || id == TAR; }
inline bool isDoor(ItemId id) { return id >= OAK_DOOR && id < IRON_DOOR + 16; }
inline bool isTrapdoor(ItemId id) { return id >= OAK_TRAPDOOR && id < OAK_TRAPDOOR + 16; }
inline bool isChest(ItemId id) {
    return id == CHEST || (id >= LOOT_CHEST_RUIN && id <= LOOT_CHEST_MINE) || id == LOOT_CHEST_FORTRESS ||
           (id >= LOOT_CHEST_VOID && id <= LOOT_CHEST_TOWER) || (id >= LOOT_CHEST_FORGOTTEN && id <= LOOT_CHEST_PORTAL);
}
inline bool isSapling(ItemId id) { return id >= OAK_SAPLING && id <= NETHERPERK_SAPLING; }
inline bool isWallCable(ItemId id) { return id >= WALL_CABLE && id < WALL_CABLE + 8; }
inline bool isWallSwitch(ItemId id) { return id >= WALL_SWITCH && id < WALL_SWITCH + 8; }
inline bool isCeilingCable(ItemId id) { return id == CEILING_CABLE || id == CEILING_CABLE_ON; }
inline bool isCable(ItemId id) { return id == POWER_CABLE || id == POWER_CABLE_ON || isWallCable(id) || isCeilingCable(id); }
inline bool isSwitch(ItemId id) { return id == SWITCH || id == SWITCH_ON || isWallSwitch(id); }
inline bool cableOn(ItemId id) { return id == POWER_CABLE_ON || id == CEILING_CABLE_ON || (isWallCable(id) && id - WALL_CABLE >= 4); }
inline bool switchOn(ItemId id) { return id == SWITCH_ON || (isWallSwitch(id) && id - WALL_SWITCH >= 4); }
// The same cable with or without power, the same switch flipped.
inline ItemId cableWithPower(ItemId id, bool on) {
    if (isWallCable(id)) return (ItemId)(WALL_CABLE + (id - WALL_CABLE) % 4 + (on ? 4 : 0));
    if (isCeilingCable(id)) return on ? (ItemId)CEILING_CABLE_ON : (ItemId)CEILING_CABLE;
    return on ? (ItemId)POWER_CABLE_ON : (ItemId)POWER_CABLE;
}
inline ItemId toggledSwitch(ItemId id) {
    if (isWallSwitch(id)) return (ItemId)(WALL_SWITCH + (id - WALL_SWITCH + 4) % 8);
    return id == SWITCH ? (ItemId)SWITCH_ON : (ItemId)SWITCH;
}
// Which wall a wall cable or switch hangs on (0 north, 1 east, 2 south, 3 west).
inline int wallSide(ItemId id) { return isWallCable(id) ? (id - WALL_CABLE) % 4 : isWallSwitch(id) ? (id - WALL_SWITCH) % 4 : -1; }
inline bool isLamp(ItemId id) { return id == POWERSTONE_LAMP || id == POWERSTONE_LAMP_ON; }
inline bool isTnt(ItemId id) { return id == TNT || id == NUKE_TNT; }
inline bool isSword(ItemId id) { return id >= WOOD_SWORD && id <= RUBY_SWORD; }
inline bool isSpawnEgg(ItemId id) {
    return (id >= EGG_PIG && id <= EGG_FORGOTTEN) || (id >= EGG_CHICKEN && id <= EGG_VILLAGER) || (id >= EGG_SPIDER && id <= EGG_HOGMAN);
}
// Wheat: seeds (stage 0) grow through two more stages into ripe wheat (stage 3), on farmland only.
inline int cropStage(ItemId id) {
    return id == WHEAT_SEEDS ? 0 : id == WHEAT_CROP1 ? 1 : id == WHEAT_CROP2 ? 2 : id == WHEAT_RIPE ? 3 : -1;
}
inline ItemId cropBlock(int stage) {
    const ItemId kStages[4] = {WHEAT_SEEDS, WHEAT_CROP1, WHEAT_CROP2, WHEAT_RIPE};
    return kStages[stage < 0 ? 0 : stage > 3 ? 3 : stage];
}
inline bool isCrop(ItemId id) { return cropStage(id) >= 0; }
inline bool isBed(ItemId id) { return id >= BED && id < BED + 8; }
// Blocks with their own shape (not a full cube): drawn and collided as boxes, see BlockShapes.h.
inline bool isShaped(ItemId id) {
    return isDoor(id) || isTrapdoor(id) || isChest(id) || isFence(id) || isSkeletonHead(id) || id == CACTUS || isBed(id) || isCable(id) ||
           isSwitch(id) || id == CAKE || id == FARMLAND || isStairs(id) || (isSlab(id) && !isDoubleSlab(id)) ||
           isBalloon(id) || id == PARTY_LANTERN || id == SNOW_LAYER || isTorch(id);
}
inline bool isLeaves(ItemId id) { return id == LEAVES || id == SPRUCE_LEAVES || id == VOID_LEAVES || id == NETHERPERK_CAP; }
inline bool isLog(ItemId id) {
    return id == LOG || id == SPRUCE_LOG || id == VOID_LOG || id == EYE_LOG || id == NETHERPERK_LOG || id == CRIMSON_LOG;
}
inline bool isPortal(ItemId id) { return id == PORTAL || id == VOID_PORTAL; }
// Small plants: drawn as two crossed planes, no collision, but they can be targeted and broken.
inline bool isPlant(ItemId id) {
    return id == TALL_GRASS || id == DANDELION || id == POPPY || id == NETHER_SPROUTS || id == CRIMSON_ROOTS || isSapling(id) ||
           isCrop(id) || isDripstoneSpike(id) || id == DRY_GRASS || id == GRASSER_FLOWER || id == FERN;
}
inline bool isSolid(ItemId id) { return id != AIR && !isLiquid(id) && !isPortal(id) && !isPlant(id); }
// What a tap or a mining finger can hit.
inline bool isTargetable(ItemId id) { return isSolid(id) || isPlant(id); }
// Opaque blocks fully hide the faces of their neighbours.
inline bool isOpaque(ItemId id) {
    return isSolid(id) && !isLeaves(id) && id != GLASS && id != POWERSTONE_GLASS && id != ICE && !isShaped(id) && !isStainedGlass(id) &&
           !(id >= kModIdBase && modIsTransparent(id));
}
// Rendered at full brightness regardless of shading.
inline bool isEmissive(ItemId id) {
    return id == LAVA || isPortal(id) || id == LIT_FURNACE || id == EYE_LOG || id == POWERSTONE_LIGHT ||
           id == POWERSTONE_LAMP_ON || id == NETHERPERK_CAP || cableOn(id) || id == POWERSTONE_BLOCK ||
           id == PARTY_LANTERN || id == ANNIV_LOGO || id == VOID_BRICK_EYE || isTorch(id) || (id >= kModIdBase && modIsEmissive(id));
}
inline bool isFurnace(ItemId id) { return id == FURNACE || id == LIT_FURNACE || id == POWERSTONE_FURNACE || id == POWERSTONE_FURNACE_ON; }
inline bool isPowerstoneFurnace(ItemId id) { return id == POWERSTONE_FURNACE || id == POWERSTONE_FURNACE_ON; }
// How far a block lights up its surroundings (0..15), day and night.
inline int lightLevel(ItemId id) {
    switch (id) {
        case POWERSTONE_LAMP_ON: case POWERSTONE_LIGHT: case PARTY_LANTERN: return 15;
        case LAVA: return 14;
        case TORCH: case TORCH_WALL: case TORCH_WALL + 1: case TORCH_WALL + 2: case TORCH_WALL + 3: return 14;
        case LIT_FURNACE: case POWERSTONE_BLOCK: case POWERSTONE_FURNACE_ON: return 12;
        case NETHERPERK_CAP: case PORTAL: case VOID_PORTAL: return 10;
        case EYE_LOG: case ANNIV_LOGO: case VOID_BRICK_EYE: return 8;
        case POWER_CABLE_ON: return 6;
        default:
            if (cableOn(id)) return 6;
            return id >= kModIdBase && modIsEmissive(id) ? 14 : 0;
    }
}
// Wool colour 0 (white) to 15 (black), or -1 for other blocks.
inline int woolColor(ItemId id) { return id == WOOL ? 0 : (id >= WOOL_ORANGE && id <= WOOL_BLACK) ? id - WOOL_ORANGE + 1 : -1; }
inline ItemId woolBlock(int color) { return color == 0 ? (ItemId)WOOL : (ItemId)(WOOL_ORANGE + color - 1); }
inline bool isArmor(ItemId id) { return id >= FIRST_ARMOR && id <= LAST_ARMOR; }
// Drawn in the blended pass after everything else.
inline bool isTranslucent(ItemId id) { return id == WATER || isPortal(id) || id == ICE || isStainedGlass(id); }
inline bool isFood(ItemId id) {
    return id == APPLE || (id >= RAW_PORK && id <= COOKED_MUTTON) || id == CANDY_CANE || id == BREAD ||
           (id >= RAW_CHICKEN && id <= STEAK) || id == MILK_BUCKET || (id >= kModIdBase && modFood(id) > 0);
}
// Old saves (chunk format VCC1) still hold the Nether blocks removed in 1.2.0, whose ids 1.3.0 reuses.
inline ItemId upgradeLegacyBlock(ItemId id) {
    switch (id) {
        case 88: return NETHERRACK;        // soul sand
        case 89: return POWERSTONE_LIGHT;  // glowstone
        case 153: return POWERSTONE_ORE;   // nether quartz ore
        default: return id;
    }
}

inline int tileFor(ItemId id, Face face) {
    switch (id) {
        case STONE: return T_STONE;
        case GRASS: return face == F_TOP ? T_GRASS_TOP : face == F_BOTTOM ? T_DIRT : T_GRASS_SIDE;
        case DIRT: return T_DIRT;
        case COBBLESTONE: return T_COBBLE;
        case PLANKS: return T_PLANKS;
        case BEDROCK: return T_BEDROCK;
        case WATER: return T_WATER;
        case SAND: return T_SAND;
        case GRAVEL: return T_GRAVEL;
        case LOG: return (face == F_TOP || face == F_BOTTOM) ? T_LOG_TOP : T_LOG_SIDE;
        case LEAVES: return T_LEAVES;
        case GLASS: return T_GLASS;
        case BRICKS: return T_BRICKS;
        case LAVA: return T_LAVA;
        case OBSIDIAN: return T_OBSIDIAN;
        case NETHERRACK: return T_NETHERRACK;
        case FARMLAND: return face == F_TOP ? T_FARMLAND : T_DIRT;
        case WHEAT_SEEDS: return T_WHEAT0;
        case WHEAT_CROP1: return T_WHEAT1;
        case WHEAT_CROP2: return T_WHEAT2;
        case WHEAT_RIPE: return T_WHEAT3;
        case PORTAL: return T_PORTAL;
        case NETHER_BRICK: return T_NETHER_BRICK;
        case HAY_BALE: return face == F_TOP || face == F_BOTTOM ? T_HAY_TOP : T_HAY_SIDE;
        case HOE: return T_HOE;
        case BOW: return T_BOW;
        case ARROW: return T_ARROW;
        case FEATHER: return T_FEATHER;
        case WHEAT: return T_WHEAT_ITEM;
        case BREAD: return T_BREAD;
        case RAW_CHICKEN: return T_RAW_CHICKEN;
        case COOKED_CHICKEN: return T_COOKED_CHICKEN;
        case RAW_BEEF: return T_RAW_BEEF;
        case STEAK: return T_STEAK;
        case MILK_BUCKET: return T_MILK_BUCKET;
        case EGG_CHICKEN: return T_EGG_CHICKEN;
        case EGG_COW: return T_EGG_COW;
        case EGG_VILLAGER: return T_EGG_VILLAGER;
        case FLINT_AND_STEEL: return T_FLINT_AND_STEEL;
        case APPLE: return T_APPLE;
        case COAL_ORE: return T_COAL_ORE;
        case IRON_ORE: return T_IRON_ORE;
        case CRAFTING_TABLE:
            return face == F_TOP ? T_CRAFTING_TOP : face == F_BOTTOM ? T_PLANKS
                 : (face == F_NORTH || face == F_SOUTH) ? T_CRAFTING_FRONT : T_CRAFTING_SIDE;
        case FURNACE:
        case LIT_FURNACE:
            // No block rotation yet, so the front shows on the north and south sides.
            if (face == F_TOP || face == F_BOTTOM) return T_FURNACE_TOP;
            if (face == F_NORTH || face == F_SOUTH) return id == LIT_FURNACE ? T_FURNACE_FRONT_LIT : T_FURNACE_FRONT;
            return T_FURNACE_SIDE;
        case SANDSTONE: return face == F_TOP || face == F_BOTTOM ? T_SANDSTONE_TOP : T_SANDSTONE_SIDE;
        case BOOKSHELF: return face == F_TOP || face == F_BOTTOM ? T_PLANKS : T_BOOKSHELF;
        case MOSSY_COBBLESTONE: return T_MOSSY_COBBLE;
        case SNOW_BLOCK: return T_SNOW;
        case CLAY: return T_CLAY;
        case STONE_BRICKS: return T_STONE_BRICKS;
        case BRONZE_ORE: return T_BRONZE_ORE;
        case STEEL_ORE: return T_STEEL_ORE;
        case RUBY_ORE: return T_RUBY_ORE;
        case BRONZE_BLOCK: return T_BRONZE_BLOCK;
        case STEEL_BLOCK: return T_STEEL_BLOCK;
        case RUBY_BLOCK: return T_RUBY_BLOCK;
        case BRONZE_INGOT: return T_BRONZE_INGOT;
        case STEEL_INGOT: return T_STEEL_INGOT;
        case RUBY: return T_RUBY;
        case SPRUCE_LOG: return (face == F_TOP || face == F_BOTTOM) ? T_SPRUCE_LOG_TOP : T_SPRUCE_LOG_SIDE;
        case SPRUCE_LEAVES: return T_SPRUCE_LEAVES;
        case SPRUCE_PLANKS: return T_SPRUCE_PLANKS;
        case ICE: return T_ICE;
        case CACTUS: return (face == F_TOP || face == F_BOTTOM) ? T_CACTUS_TOP : T_CACTUS_SIDE;
        case HARDENED_STONE: return T_HARDENED_STONE;
        case FENCE: return T_PLANKS;
        case OAK_DOOR_ITEM: return T_OAK_DOOR_ITEM;
        case IRON_DOOR_ITEM: return T_IRON_DOOR_ITEM;
        case BOAT: return T_BOAT;
        case TALL_GRASS: return T_TALL_GRASS;
        case DANDELION: return T_DANDELION;
        case POPPY: return T_POPPY;
        case NETHER_GRASS: return face == F_TOP ? T_NETHER_GRASS_TOP : face == F_BOTTOM ? T_NETHERRACK : T_NETHER_GRASS_SIDE;
        case NETHER_SPROUTS: return T_NETHER_SPROUTS;
        case VOID_STONE: return T_VOID_STONE;
        case VOID_GRASS: return face == F_TOP ? T_VOID_GRASS_TOP : face == F_BOTTOM ? T_VOID_STONE : T_VOID_GRASS_SIDE;
        case VOID_LOG: return (face == F_TOP || face == F_BOTTOM) ? T_VOID_LOG_TOP : T_VOID_LOG_SIDE;
        case EYE_LOG: return (face == F_TOP || face == F_BOTTOM) ? T_VOID_LOG_TOP : T_EYE_LOG;
        case VOID_LEAVES: return T_VOID_LEAVES;
        case RED_SAND: return T_RED_SAND;
        case VOID_PORTAL: return T_VOID_PORTAL;
        case VOID_STONE_BRICKS: return T_VOID_STONE_BRICKS;
        case PUMPKIN: return face == F_TOP || face == F_BOTTOM ? T_PUMPKIN_TOP : face == F_SOUTH ? T_PUMPKIN_FACE : T_PUMPKIN_SIDE;
        case CRIMSON_ROOTS: return T_CRIMSON_ROOTS;
        case VOID_FLINT_AND_STEEL: return T_VOID_FLINT_AND_STEEL;
        case RAW_PORK: return T_RAW_PORK;
        case COOKED_PORK: return T_COOKED_PORK;
        case RAW_MUTTON: return T_RAW_MUTTON;
        case COOKED_MUTTON: return T_COOKED_MUTTON;
        case BED_ITEM: return T_BED_ITEM;
        case WORLD_END: return T_WORLD_END;
        case TNT: return face == F_TOP ? T_TNT_TOP : face == F_BOTTOM ? T_TNT_BOTTOM : T_TNT_SIDE;
        case NUKE_TNT: return face == F_TOP ? T_TNT_TOP : face == F_BOTTOM ? T_TNT_BOTTOM : T_NUKE_SIDE;
        case NETHERPERK_LOG: return (face == F_TOP || face == F_BOTTOM) ? T_NETHERPERK_LOG_TOP : T_NETHERPERK_LOG_SIDE;
        case NETHERPERK_CAP: return T_NETHERPERK_CAP;
        case NETHERPERK_PLANKS: return T_NETHERPERK_PLANKS;
        case VOID_PLANKS: return T_VOID_PLANKS;
        case OAK_SAPLING: return T_OAK_SAPLING;
        case PINE_SAPLING: return T_PINE_SAPLING;
        case VOID_SAPLING: return T_VOID_SAPLING;
        case NETHERPERK_SAPLING: return T_NETHERPERK_SAPLING;
        case POWERSTONE_ORE: return T_POWERSTONE_ORE;
        case POWERSTONE_BLOCK: return T_POWERSTONE_BLOCK;
        case POWER_CABLE: return T_CABLE;
        case POWER_CABLE_ON: return T_CABLE_ON;
        case POWERSTONE_LAMP: return T_LAMP;
        case POWERSTONE_LAMP_ON: return T_LAMP_ON;
        case SWITCH: return face == F_TOP ? T_SWITCH : T_COBBLE;
        case SWITCH_ON: return face == F_TOP ? T_SWITCH_ON : T_COBBLE;
        case POWERSTONE_LIGHT: return T_POWERSTONE_LIGHT;
        case GIFT_BOX: return face == F_TOP || face == F_BOTTOM ? T_GIFT_TOP : T_GIFT_SIDE;
        case CAKE: return face == F_TOP ? T_CAKE_TOP : T_CAKE_SIDE;
        case POWERSTONE_DUST: return T_POWERSTONE_DUST;
        case POWERSTONE_CHIP: return T_POWERSTONE_CHIP;
        case CANDY_CANE: return T_CANDY_CANE;
        case BUCKET: return T_BUCKET;
        case WATER_BUCKET: return T_WATER_BUCKET;
        case LAVA_BUCKET: return T_LAVA_BUCKET;
        case BOSS_COMPASS: return T_COMPASS;
        case STICK: return T_STICK;
        case COAL: return T_COAL;
        case IRON_INGOT: return T_IRON_INGOT;
        case FLINT: return T_FLINT;
        case WOOD_PICKAXE: return T_WOOD_PICKAXE;
        case STONE_PICKAXE: return T_STONE_PICKAXE;
        case IRON_PICKAXE: return T_IRON_PICKAXE;
        case WOOD_AXE: return T_WOOD_AXE;
        case STONE_AXE: return T_STONE_AXE;
        case IRON_AXE: return T_IRON_AXE;
        case WOOD_SHOVEL: return T_WOOD_SHOVEL;
        case STONE_SHOVEL: return T_STONE_SHOVEL;
        case IRON_SHOVEL: return T_IRON_SHOVEL;
        default:
            if (woolColor(id) >= 0) return T_WOOL + woolColor(id);
            if (id >= kModIdBase) return modTileFor(id, face);
            if (isStairs(id) || isSlab(id)) return tileFor(shapeMaterial(id), face);
            if (isStainedGlass(id)) return T_STAINED_GLASS + (id - STAINED_GLASS);
            if (isBalloon(id)) return T_BALLOON + (id - BALLOON);
            if (isPaintedPlanks(id)) return T_PAINTED_PLANKS + (id - PAINTED_PLANKS);
            switch (id) {
                case TAR: return T_TAR;
                case TAR_BLOCK: return T_TAR_BLOCK;
                case DRIPSTONE_BLOCK: return T_DRIPSTONE;
                case DRIPSTONE_UP: return T_DRIPSTONE_UP;
                case DRIPSTONE_DOWN: return T_DRIPSTONE_DOWN;
                case SNOW_LAYER: return T_SNOW;
                case STRING: return T_STRING;
                case MAP: return T_MAP;
                case TAR_BALL: return T_TAR_BALL;
                case EGG_SPIDER: return T_EGG_SPIDER;
                case EGG_BAT: return T_EGG_BAT;
                case EGG_TNT_ZOMBIE: return T_EGG_TNT_ZOMBIE;
                case EGG_TAR_MONSTER: return T_EGG_TAR_MONSTER;
                case ANNIV_LOGO: return face == F_TOP || face == F_BOTTOM ? T_ANNIV_LOGO_SIDE : T_ANNIV_LOGO;
                case CONFETTI_BLOCK: return T_CONFETTI_BLOCK;
                case PARTY_LANTERN: return face == F_TOP || face == F_BOTTOM ? T_PARTY_LANTERN_TOP : T_PARTY_LANTERN;
                case FESTIVE_BRICKS: return T_FESTIVE_BRICKS;
                case ANNIV_TOKEN: return T_ANNIV_TOKEN;
                case ANNIV_MEDAL: return T_ANNIV_MEDAL;
                case PARTY_POPPER: return T_PARTY_POPPER;
                case EGG_HUSKING: return T_EGG_HUSKING;
                case CRIMSON_STONE: return T_CRIMSON_STONE;
                case CRIMSON_PLANKS: return T_CRIMSON_PLANKS;
                case POWERSTONE_FURNACE: case POWERSTONE_FURNACE_ON:
                    if (face == F_NORTH || face == F_SOUTH) return id == POWERSTONE_FURNACE_ON ? T_PS_FURNACE_FRONT_ON : T_PS_FURNACE_FRONT;
                    return face == F_TOP || face == F_BOTTOM ? T_CHISELED_POWERSTONE_BRICKS : T_POWERSTONE_BRICKS;
                case CRIMSON_LOG: return face == F_TOP || face == F_BOTTOM ? T_CRIMSON_LOG_TOP : T_CRIMSON_LOG_SIDE;
                case SHIELD: return T_SHIELD;
                case EXPLORER_MAP: return T_EXPLORER_MAP;
                case WRITABLE_BOOK: return T_WRITABLE_BOOK;
                case WRITTEN_BOOK: return T_WRITTEN_BOOK;
                case EGG_HOGMAN: return T_EGG_HOGMAN;
                case VOID_BRICK_EYE: return face == F_TOP || face == F_BOTTOM ? T_VOID_STONE_BRICKS : T_VOID_BRICK_EYE;
                case POWERSTONE_BRICKS: case POWERSTONE_FENCE: return T_POWERSTONE_BRICKS;
                case CRACKED_POWERSTONE_BRICKS: return T_CRACKED_POWERSTONE_BRICKS;
                case CHISELED_POWERSTONE_BRICKS: return face == F_TOP || face == F_BOTTOM ? T_POWERSTONE_BRICKS : T_CHISELED_POWERSTONE_BRICKS;
                case POWERSTONE_GLASS: return T_POWERSTONE_GLASS;
                case DEBRIS_GRAVEL: return T_DEBRIS_GRAVEL;
                case WASTELAND_GRASS: return face == F_TOP ? T_WASTELAND_GRASS_TOP : face == F_BOTTOM ? T_NETHERRACK : T_WASTELAND_GRASS_SIDE;
                case DRY_GRASS: return T_DRY_GRASS;
                case GRASSER_FLOWER: return T_GRASSER_FLOWER;
                case FERN: return T_FERN;
                case CRACKED_NETHER_BRICKS: return T_CRACKED_NETHER_BRICKS;
                case CHISELED_NETHER_BRICKS: return face == F_TOP || face == F_BOTTOM ? T_NETHER_BRICK : T_CHISELED_NETHER_BRICKS;
                case NETHER_BRICK_FENCE: return T_NETHER_BRICK;
                case SPRUCE_FENCE: return T_SPRUCE_PLANKS;
                case VOID_FENCE: return T_VOID_PLANKS;
                case NETHERPERK_FENCE: return T_NETHERPERK_PLANKS;
                case BRUSHER: return T_BRUSHER;
                case FORTRESS_COMPASS: return T_FORTRESS_COMPASS;
                case ANCIENT_COIN: return T_ANCIENT_COIN;
                case POWERSTONE_RELIC: return T_POWERSTONE_RELIC;
                case POTTERY_SHARD: return T_POTTERY_SHARD;
                default:
                    if (isDocument(id)) return T_DOCUMENT;
                    if (isTorch(id)) return T_TORCH;
                    if (isSkeletonHead(id)) {
                        static const Face kFront[4] = {F_SOUTH, F_WEST, F_NORTH, F_EAST};
                        return face == kFront[id - SKELETON_HEAD] ? T_SKULL_FACE : T_SKULL;
                    }
                    if (isWallCable(id) || isCeilingCable(id)) return cableOn(id) ? T_CABLE_LINE_ON : T_CABLE_LINE;
                    if (isWallSwitch(id)) {
                        // The lever shows on the side that faces away from the wall.
                        static const Face kOut[4] = {F_SOUTH, F_WEST, F_NORTH, F_EAST};
                        return face == kOut[wallSide(id)] ? (switchOn(id) ? T_SWITCH_ON : T_SWITCH) : T_COBBLE;
                    }
                    break;
            }
            if (id >= BRONZE_PICKAXE && id <= RUBY_SHOVEL) return T_FIRST_NEW_TOOL + (id - BRONZE_PICKAXE);
            if (isArmor(id)) return T_FIRST_ARMOR + (id - FIRST_ARMOR);
            if (isSword(id)) return T_FIRST_SWORD + (id - WOOD_SWORD);
            if (isSpawnEgg(id)) return T_EGG_PIG + (id - EGG_PIG);
            if (isChest(id)) return face == F_TOP || face == F_BOTTOM ? T_CHEST_TOP : (face == F_NORTH || face == F_SOUTH) ? T_CHEST_FRONT : T_CHEST_SIDE;
            if (isDoor(id)) {
                bool iron = id >= IRON_DOOR, upper = ((id - (iron ? IRON_DOOR : OAK_DOOR)) & 8) != 0;
                return iron ? (upper ? T_IRON_DOOR_UPPER : T_IRON_DOOR_LOWER) : (upper ? T_OAK_DOOR_UPPER : T_OAK_DOOR_LOWER);
            }
            if (isTrapdoor(id)) return T_TRAPDOOR;
            if (isBed(id)) return face == F_TOP ? (((id - BED) & 1) ? T_BED_TOP_HEAD : T_BED_TOP_FOOT) : face == F_BOTTOM ? T_PLANKS : T_BED_SIDE;
            return T_STONE;
    }
}


}  // namespace vc

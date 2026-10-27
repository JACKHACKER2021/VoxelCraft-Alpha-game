#include "GenConfig.h"
#include "Lang.h"

#include <algorithm>
#include <cmath>

#include "Blocks.h"
#include "Items.h"

namespace vc {

namespace {

const char* kManifest = R"({
  "name": "VoxelCraft Default",
  "description": "Výchozí biomy, dimenze a recepty. Klidně je uprav!",
  "version": "1.1.0",
  "format": 1
})";

const char* kOverworld = R"({
  // Hladina moře, jeskyně a rudy v normálním světě.
  "sea_level": 62,
  "caves": {"enabled": true, "size": 0.06},
  "biome_size": 320,
  "sky_color": [0.53, 0.74, 1.0],
  "ores": [
    {"block": "coal_ore", "veins": 16, "size": 8, "min_y": 5, "max_y": 100},
    {"block": "iron_ore", "veins": 10, "size": 6, "min_y": 5, "max_y": 60},
    {"block": "bronze_ore", "veins": 8, "size": 6, "min_y": 5, "max_y": 70},
    {"block": "steel_ore", "veins": 5, "size": 5, "min_y": 5, "max_y": 40},
    {"block": "ruby_ore", "veins": 2, "size": 4, "min_y": 3, "max_y": 20}
  ]
})";

const char* kNether = R"({
  // Nether: lávové moře, z čeho jsou stěny a podlaha (úhelný kámen a úhelná tráva), powerstone a mlha.
  "lava_level": 31,
  "blocks": {
    "fill": "netherrack",
    "liquid": "lava",
    "floor": "nether_grass",
    "shore": "gravel",
    "ore": "powerstone_ore",
    "ceiling": "bedrock"
  },
  "floor_amount": 0.25,
  "ore_chance": 0.01,
  "glowstone_clusters": 2,
  "caves": {"scale": 32, "openness": 0.05},
  "fog_color": [0.22, 0.04, 0.03]
})";

// The Nether of 1.0 and 1.1 (soul sand, quartz), replaced if the player never changed it.
const char* kNetherV1 = R"({
  // Nether: lávové moře, z čeho jsou stěny a podlaha, světlit a mlha.
  "lava_level": 31,
  "blocks": {
    "fill": "netherrack",
    "liquid": "lava",
    "floor": "soul_sand",
    "shore": "gravel",
    "ore": "quartz_ore",
    "ceiling": "bedrock"
  },
  "floor_amount": 0.25,
  "ore_chance": 0.008,
  "glowstone_clusters": 2,
  "caves": {"scale": 32, "openness": 0.05},
  "fog_color": [0.22, 0.04, 0.03]
})";

const char* kPlains = R"({
  "id": "plains",
  "name": "Pláně",
  "climate": {"temperature": 0.5, "humidity": 0.35},
  "terrain": {"height": 66, "variation": 5},
  "blocks": {"surface": "grass", "filler": "dirt", "filler_depth": 3, "beach": "sand", "underwater": "sand"},
  "trees": {"density": 0.003, "trunk": "log", "leaves": "leaves"},
  "plants": {"grass": 0.25, "flowers": 0.03}
})";

const char* kForest = R"({
  "id": "forest",
  "name": "Les",
  "climate": {"temperature": 0.45, "humidity": 0.7},
  "terrain": {"height": 68, "variation": 9},
  "blocks": {"surface": "grass", "filler": "dirt", "filler_depth": 3, "beach": "sand", "underwater": "dirt"},
  "trees": {"density": 0.05, "trunk": "log", "leaves": "leaves"},
  "plants": {"grass": 0.15, "flowers": 0.01}
})";

// Default biome files of 1.0.0, before the update changed them (see upgradableDefaults).
const char* kDesertV1 = R"({
  "id": "desert",
  "name": "Poušť",
  "climate": {"temperature": 0.9, "humidity": 0.1},
  "terrain": {"height": 66, "variation": 4},
  "blocks": {"surface": "sand", "filler": "sand", "filler_depth": 5, "beach": "sand", "underwater": "sand"},
  "trees": {"density": 0}
})";

const char* kMountainsV1 = R"({
  "id": "mountains",
  "name": "Hory",
  "climate": {"temperature": 0.15, "humidity": 0.45},
  "terrain": {"height": 84, "variation": 30},
  "blocks": {"surface": "grass", "filler": "dirt", "filler_depth": 2, "beach": "gravel", "underwater": "gravel"},
  "trees": {"density": 0.008, "trunk": "log", "leaves": "leaves"},
  "plants": {"grass": 0.1, "flowers": 0.005}
})";

const char* kDesert = R"({
  "id": "desert",
  "name": "Poušť",
  "climate": {"temperature": 0.9, "humidity": 0.1},
  "terrain": {"height": 66, "variation": 4},
  "blocks": {"surface": "sand", "filler": "sand", "filler_depth": 5, "beach": "sand", "underwater": "sand"},
  "trees": {"density": 0},
  "plants": {"cactus": 0.006}
})";

const char* kMountains = R"({
  // Vysoké hory: nad výškou "peaks.height" je tvrdý kámen, nad "snow_height" sníh.
  "id": "mountains",
  "name": "Hory",
  "climate": {"temperature": 0.25, "humidity": 0.45},
  "terrain": {"height": 92, "variation": 46},
  "blocks": {"surface": "grass", "filler": "dirt", "filler_depth": 2, "beach": "gravel", "underwater": "gravel"},
  "trees": {"density": 0.008, "trunk": "spruce_log", "leaves": "spruce_leaves", "shape": "pine"},
  "plants": {"grass": 0.1, "flowers": 0.005},
  "peaks": {"block": "hardened_stone", "height": 84, "snow_height": 102}
})";

const char* kSnowyTaiga = R"({
  "id": "snowy_taiga",
  "name": "Zasněžený jehličnatý les",
  "climate": {"temperature": 0.05, "humidity": 0.65},
  "terrain": {"height": 70, "variation": 10},
  "blocks": {"surface": "snow", "filler": "dirt", "filler_depth": 3, "beach": "gravel", "underwater": "gravel"},
  "trees": {"density": 0.045, "trunk": "spruce_log", "leaves": "spruce_leaves", "shape": "pine"},
  "plants": {"grass": 0, "flowers": 0},
  "freeze": true
})";

const char* kIcePlains = R"({
  "id": "ice_plains",
  "name": "Ledové pláně",
  "climate": {"temperature": 0.0, "humidity": 0.25},
  "terrain": {"height": 58, "variation": 6},
  "blocks": {"surface": "snow", "filler": "dirt", "filler_depth": 3, "beach": "snow", "underwater": "gravel"},
  "trees": {"density": 0.0015, "trunk": "spruce_log", "leaves": "spruce_leaves", "shape": "pine"},
  "plants": {"grass": 0, "flowers": 0},
  "freeze": true
})";

const char* kOcean = R"({
  "id": "ocean",
  "name": "Oceán",
  "climate": {"temperature": 0.55, "humidity": 0.95},
  "terrain": {"height": 46, "variation": 6},
  "blocks": {"surface": "sand", "filler": "sand", "filler_depth": 3, "beach": "sand", "underwater": "gravel"},
  "trees": {"density": 0}
})";

const char* kCrafting = R"({
  // Recepty pro výrobu. "pattern" jsou řádky mřížky, písmena odkazují do "key", "." je prázdné pole.
  "recipes": [
    {"type": "shapeless", "ingredients": ["log"], "result": "planks", "count": 4},
    {"type": "shaped", "pattern": ["A", "A"], "key": {"A": "planks"}, "result": "stick", "count": 4},
    {"type": "shaped", "pattern": ["AA", "AA"], "key": {"A": "planks"}, "result": "crafting_table"},
    {"type": "shaped", "pattern": ["AAA", "A.A", "AAA"], "key": {"A": "cobblestone"}, "result": "furnace"},
    {"type": "shapeless", "ingredients": ["iron_ingot", "flint"], "result": "flint_and_steel"},
    {"type": "shaped", "pattern": ["AAA", ".S.", ".S."], "key": {"A": "planks", "S": "stick"}, "result": "wooden_pickaxe"},
    {"type": "shaped", "pattern": ["AAA", ".S.", ".S."], "key": {"A": "cobblestone", "S": "stick"}, "result": "stone_pickaxe"},
    {"type": "shaped", "pattern": ["AAA", ".S.", ".S."], "key": {"A": "iron_ingot", "S": "stick"}, "result": "iron_pickaxe"},
    {"type": "shaped", "pattern": ["AA", "AS", ".S"], "key": {"A": "planks", "S": "stick"}, "result": "wooden_axe"},
    {"type": "shaped", "pattern": ["AA", "AS", ".S"], "key": {"A": "cobblestone", "S": "stick"}, "result": "stone_axe"},
    {"type": "shaped", "pattern": ["AA", "AS", ".S"], "key": {"A": "iron_ingot", "S": "stick"}, "result": "iron_axe"},
    {"type": "shaped", "pattern": ["A", "S", "S"], "key": {"A": "planks", "S": "stick"}, "result": "wooden_shovel"},
    {"type": "shaped", "pattern": ["A", "S", "S"], "key": {"A": "cobblestone", "S": "stick"}, "result": "stone_shovel"},
    {"type": "shaped", "pattern": ["A", "S", "S"], "key": {"A": "iron_ingot", "S": "stick"}, "result": "iron_shovel"}
  ]
})";

const char* kMetals = R"({
  // VoxelCraft 1.0: bronze, steel and ruby tools, armour of five materials, storage
  // blocks and new building blocks.
  "recipes": [
    {"type": "shaped", "pattern": ["AAA", ".S.", ".S."], "key": {"A": "bronze_ingot", "S": "stick"}, "result": "bronze_pickaxe"},
    {"type": "shaped", "pattern": ["AAA", ".S.", ".S."], "key": {"A": "steel_ingot", "S": "stick"}, "result": "steel_pickaxe"},
    {"type": "shaped", "pattern": ["AAA", ".S.", ".S."], "key": {"A": "ruby", "S": "stick"}, "result": "ruby_pickaxe"},
    {"type": "shaped", "pattern": ["AA", "AS", ".S"], "key": {"A": "bronze_ingot", "S": "stick"}, "result": "bronze_axe"},
    {"type": "shaped", "pattern": ["AA", "AS", ".S"], "key": {"A": "steel_ingot", "S": "stick"}, "result": "steel_axe"},
    {"type": "shaped", "pattern": ["AA", "AS", ".S"], "key": {"A": "ruby", "S": "stick"}, "result": "ruby_axe"},
    {"type": "shaped", "pattern": ["A", "S", "S"], "key": {"A": "bronze_ingot", "S": "stick"}, "result": "bronze_shovel"},
    {"type": "shaped", "pattern": ["A", "S", "S"], "key": {"A": "steel_ingot", "S": "stick"}, "result": "steel_shovel"},
    {"type": "shaped", "pattern": ["A", "S", "S"], "key": {"A": "ruby", "S": "stick"}, "result": "ruby_shovel"},
    {"type": "shaped", "pattern": ["AAA", "A.A"], "key": {"A": "planks"}, "result": "wooden_helmet"},
    {"type": "shaped", "pattern": ["A.A", "AAA", "AAA"], "key": {"A": "planks"}, "result": "wooden_chestplate"},
    {"type": "shaped", "pattern": ["AAA", "A.A", "A.A"], "key": {"A": "planks"}, "result": "wooden_leggings"},
    {"type": "shaped", "pattern": ["A.A", "A.A"], "key": {"A": "planks"}, "result": "wooden_boots"},
    {"type": "shaped", "pattern": ["AAA", "A.A"], "key": {"A": "cobblestone"}, "result": "stone_helmet"},
    {"type": "shaped", "pattern": ["A.A", "AAA", "AAA"], "key": {"A": "cobblestone"}, "result": "stone_chestplate"},
    {"type": "shaped", "pattern": ["AAA", "A.A", "A.A"], "key": {"A": "cobblestone"}, "result": "stone_leggings"},
    {"type": "shaped", "pattern": ["A.A", "A.A"], "key": {"A": "cobblestone"}, "result": "stone_boots"},
    {"type": "shaped", "pattern": ["AAA", "A.A"], "key": {"A": "bronze_ingot"}, "result": "bronze_helmet"},
    {"type": "shaped", "pattern": ["A.A", "AAA", "AAA"], "key": {"A": "bronze_ingot"}, "result": "bronze_chestplate"},
    {"type": "shaped", "pattern": ["AAA", "A.A", "A.A"], "key": {"A": "bronze_ingot"}, "result": "bronze_leggings"},
    {"type": "shaped", "pattern": ["A.A", "A.A"], "key": {"A": "bronze_ingot"}, "result": "bronze_boots"},
    {"type": "shaped", "pattern": ["AAA", "A.A"], "key": {"A": "steel_ingot"}, "result": "steel_helmet"},
    {"type": "shaped", "pattern": ["A.A", "AAA", "AAA"], "key": {"A": "steel_ingot"}, "result": "steel_chestplate"},
    {"type": "shaped", "pattern": ["AAA", "A.A", "A.A"], "key": {"A": "steel_ingot"}, "result": "steel_leggings"},
    {"type": "shaped", "pattern": ["A.A", "A.A"], "key": {"A": "steel_ingot"}, "result": "steel_boots"},
    {"type": "shaped", "pattern": ["AAA", "A.A"], "key": {"A": "ruby"}, "result": "ruby_helmet"},
    {"type": "shaped", "pattern": ["A.A", "AAA", "AAA"], "key": {"A": "ruby"}, "result": "ruby_chestplate"},
    {"type": "shaped", "pattern": ["AAA", "A.A", "A.A"], "key": {"A": "ruby"}, "result": "ruby_leggings"},
    {"type": "shaped", "pattern": ["A.A", "A.A"], "key": {"A": "ruby"}, "result": "ruby_boots"},
    {"type": "shaped", "pattern": ["AAA", "AAA", "AAA"], "key": {"A": "bronze_ingot"}, "result": "bronze_block"},
    {"type": "shaped", "pattern": ["AAA", "AAA", "AAA"], "key": {"A": "steel_ingot"}, "result": "steel_block"},
    {"type": "shaped", "pattern": ["AAA", "AAA", "AAA"], "key": {"A": "ruby"}, "result": "ruby_block"},
    {"type": "shapeless", "ingredients": ["bronze_block"], "result": "bronze_ingot", "count": 9},
    {"type": "shapeless", "ingredients": ["steel_block"], "result": "steel_ingot", "count": 9},
    {"type": "shapeless", "ingredients": ["ruby_block"], "result": "ruby", "count": 9},
    {"type": "shaped", "pattern": ["AA", "AA"], "key": {"A": "sand"}, "result": "sandstone"},
    {"type": "shaped", "pattern": ["AA", "AA"], "key": {"A": "stone"}, "result": "stone_bricks", "count": 4},
    {"type": "shapeless", "ingredients": ["cobblestone", "leaves"], "result": "mossy_cobblestone"},
    {"type": "shaped", "pattern": ["AAA", "SSS", "AAA"], "key": {"A": "planks", "S": "stick"}, "result": "bookshelf"}
  ],
  "smelting": [
    {"input": "bronze_ore", "output": "bronze_ingot"},
    {"input": "steel_ore", "output": "steel_ingot"},
    {"input": "clay", "output": "bricks"}
  ]
})";

const char* kUpdate1 = R"({
  // VoxelCraft 1.0.0 update: chests, fences, doors, trapdoor, boat, spruce.
  "recipes": [
    {"type": "shaped", "pattern": ["AAA", "A.A", "AAA"], "key": {"A": "planks"}, "result": "chest"},
    {"type": "shaped", "pattern": ["ASA", "ASA"], "key": {"A": "planks", "S": "stick"}, "result": "fence", "count": 3},
    {"type": "shaped", "pattern": ["AA", "AA", "AA"], "key": {"A": "planks"}, "result": "oak_door", "count": 3},
    {"type": "shaped", "pattern": ["AA", "AA", "AA"], "key": {"A": "iron_ingot"}, "result": "iron_door", "count": 3},
    {"type": "shaped", "pattern": ["AAA", "AAA"], "key": {"A": "planks"}, "result": "trapdoor", "count": 2},
    {"type": "shaped", "pattern": ["A.A", "AAA"], "key": {"A": "planks"}, "result": "boat"},
    {"type": "shapeless", "ingredients": ["spruce_log"], "result": "spruce_planks", "count": 4},
    {"type": "shapeless", "ingredients": ["spruce_planks"], "result": "planks"}
  ],
  "smelting": [
    {"input": "spruce_log", "output": "coal"}
  ],
  "fuels": [
    {"item": "spruce_log", "seconds": 15},
    {"item": "spruce_planks", "seconds": 15},
    {"item": "fence", "seconds": 15},
    {"item": "chest", "seconds": 15},
    {"item": "boat", "seconds": 20}
  ]
})";

// 1.1.0 - 1.5.0 (Eye Logs could be turned into void planks).
const char* kUpdate11V1 = R"({
  // VoxelCraft 1.1.0: beds, void stone bricks, planks from void logs, cooked meat.
  "recipes": [
    {"type": "shaped", "pattern": ["WWW", "PPP"], "key": {"W": "white_wool", "P": "planks"}, "result": "bed"},
    {"type": "shaped", "pattern": ["AA", "AA"], "key": {"A": "void_stone"}, "result": "void_stone_bricks", "count": 4},
    {"type": "shapeless", "ingredients": ["eye_log"], "result": "void_planks", "count": 4}
  ],
  "smelting": [
    {"input": "porkchop", "output": "cooked_porkchop"},
    {"input": "mutton", "output": "cooked_mutton"},
    {"input": "red_sand", "output": "glass"}
  ],
  "fuels": [
    {"item": "void_log", "seconds": 15},
    {"item": "eye_log", "seconds": 30}
  ]
})";

const char* kUpdate11 = R"({
  // VoxelCraft 1.1.0: beds, void stone bricks, cooked meat. (Eye Logs give no planks since 1.5.1.)
  "recipes": [
    {"type": "shaped", "pattern": ["WWW", "PPP"], "key": {"W": "white_wool", "P": "planks"}, "result": "bed"},
    {"type": "shaped", "pattern": ["AA", "AA"], "key": {"A": "void_stone"}, "result": "void_stone_bricks", "count": 4}
  ],
  "smelting": [
    {"input": "porkchop", "output": "cooked_porkchop"},
    {"input": "mutton", "output": "cooked_mutton"},
    {"input": "red_sand", "output": "glass"}
  ],
  "fuels": [
    {"item": "void_log", "seconds": 15},
    {"item": "eye_log", "seconds": 30}
  ]
})";

const char* kUpdate12 = R"({
  // VoxelCraft 1.2.0: swords, buckets, TNT, powerstone electronics, new planks.
  "recipes": [
    {"type": "shaped", "pattern": ["M", "M", "S"], "key": {"M": "planks", "S": "stick"}, "result": "wooden_sword"},
    {"type": "shaped", "pattern": ["M", "M", "S"], "key": {"M": "cobblestone", "S": "stick"}, "result": "stone_sword"},
    {"type": "shaped", "pattern": ["M", "M", "S"], "key": {"M": "iron_ingot", "S": "stick"}, "result": "iron_sword"},
    {"type": "shaped", "pattern": ["M", "M", "S"], "key": {"M": "bronze_ingot", "S": "stick"}, "result": "bronze_sword"},
    {"type": "shaped", "pattern": ["M", "M", "S"], "key": {"M": "steel_ingot", "S": "stick"}, "result": "steel_sword"},
    {"type": "shaped", "pattern": ["M", "M", "S"], "key": {"M": "ruby", "S": "stick"}, "result": "ruby_sword"},
    {"type": "shaped", "pattern": ["I.I", ".I."], "key": {"I": "iron_ingot"}, "result": "bucket"},
    {"type": "shaped", "pattern": ["SDS", "DSD", "SDS"], "key": {"S": "sand", "D": "powerstone_dust"}, "result": "tnt"},
    {"type": "shaped", "pattern": ["DDD", "DDD", "DDD"], "key": {"D": "powerstone_dust"}, "result": "powerstone_block"},
    {"type": "shaped", "pattern": ["DDD"], "key": {"D": "powerstone_dust"}, "result": "power_cable", "count": 8},
    {"type": "shaped", "pattern": ["DID"], "key": {"D": "powerstone_dust", "I": "iron_ingot"}, "result": "powerstone_chip", "count": 2},
    {"type": "shaped", "pattern": ["C", "S", "B"], "key": {"C": "powerstone_chip", "S": "stick", "B": "cobblestone"}, "result": "switch"},
    {"type": "shaped", "pattern": [".G.", "GCG", ".G."], "key": {"G": "glass", "C": "powerstone_chip"}, "result": "powerstone_lamp"},
    {"type": "shaped", "pattern": ["D", "G", "D"], "key": {"D": "powerstone_dust", "G": "glass"}, "result": "powerstone_light", "count": 2},
    {"type": "shapeless", "ingredients": ["void_log"], "result": "void_planks", "count": 4},
    {"type": "shapeless", "ingredients": ["netherperk_log"], "result": "netherperk_planks", "count": 4},
    {"type": "shapeless", "ingredients": ["void_planks"], "result": "planks"},
    {"type": "shapeless", "ingredients": ["netherperk_planks"], "result": "planks"},
    {"type": "shaped", "pattern": ["TTT", "TBT", "TTT"], "key": {"T": "tnt", "B": "powerstone_block"}, "result": "nuke_tnt"},
    {"type": "shaped", "pattern": ["WRW", "RCR", "WRW"], "key": {"W": "white_wool", "R": "red_wool", "C": "chest"}, "result": "gift_box"},
    {"type": "shapeless", "ingredients": ["apple", "snow"], "result": "candy_cane", "count": 3},
    {"type": "shapeless", "ingredients": ["apple", "apple", "apple", "snow"], "result": "cake"}
  ],
  "smelting": [
    {"input": "powerstone_ore", "output": "powerstone_dust"}
  ],
  "fuels": [
    {"item": "netherperk_log", "seconds": 15},
    {"item": "void_planks", "seconds": 15},
    {"item": "netherperk_planks", "seconds": 15},
    {"item": "wooden_sword", "seconds": 10}
  ]
})";

const char* kUpdate13 = R"({
  // VoxelCraft 1.3.0: farming, the bow, chickens and cows.
  "recipes": [
    {"type": "shaped", "pattern": ["MM", ".S", ".S"], "key": {"M": "planks", "S": "stick"}, "result": "hoe"},
    {"type": "shaped", "pattern": ["MM", ".S", ".S"], "key": {"M": "iron_ingot", "S": "stick"}, "result": "hoe"},
    {"type": "shaped", "pattern": [".SW", "S.W", ".SW"], "key": {"S": "stick", "W": "string"}, "result": "bow"},
    {"type": "shaped", "pattern": ["F", "S", "P"], "key": {"F": "flint", "S": "stick", "P": "feather"}, "result": "arrow", "count": 4},
    {"type": "shaped", "pattern": ["WWW"], "key": {"W": "wheat"}, "result": "bread"},
    {"type": "shaped", "pattern": ["WWW", "WWW", "WWW"], "key": {"W": "wheat"}, "result": "hay_block"},
    {"type": "shapeless", "ingredients": ["hay_block"], "result": "wheat", "count": 9}
  ],
  "smelting": [
    {"input": "chicken", "output": "cooked_chicken"},
    {"input": "beef", "output": "cooked_beef"}
  ],
  "fuels": [
    {"item": "bow", "seconds": 10}
  ]
})";

const char* kUpdate14 = R"({
  // VoxelCraft 1.4.0: stairs, slabs, stained glass, tar, dripstone and the map.
  "recipes": [
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "planks"}, "result": "oak_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "spruce_planks"}, "result": "spruce_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "void_planks"}, "result": "void_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "netherperk_planks"}, "result": "netherperk_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "cobblestone"}, "result": "cobblestone_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "stone"}, "result": "stone_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "stone_bricks"}, "result": "stone_brick_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "mossy_cobblestone"}, "result": "mossy_cobblestone_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "bricks"}, "result": "brick_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "sandstone"}, "result": "sandstone_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "nether_brick"}, "result": "nether_brick_stairs", "count": 4},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "void_stone_bricks"}, "result": "void_brick_stairs", "count": 4},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "planks"}, "result": "oak_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "spruce_planks"}, "result": "spruce_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "void_planks"}, "result": "void_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "netherperk_planks"}, "result": "netherperk_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "cobblestone"}, "result": "cobblestone_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "stone"}, "result": "stone_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "stone_bricks"}, "result": "stone_brick_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "mossy_cobblestone"}, "result": "mossy_cobblestone_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "bricks"}, "result": "brick_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "sandstone"}, "result": "sandstone_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "nether_brick"}, "result": "nether_brick_slab", "count": 6},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "void_stone_bricks"}, "result": "void_brick_slab", "count": 6},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "white_wool"}, "result": "white_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "orange_wool"}, "result": "orange_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "magenta_wool"}, "result": "magenta_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "light_blue_wool"}, "result": "light_blue_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "yellow_wool"}, "result": "yellow_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "lime_wool"}, "result": "lime_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "pink_wool"}, "result": "pink_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "gray_wool"}, "result": "gray_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "light_gray_wool"}, "result": "light_gray_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "cyan_wool"}, "result": "cyan_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "purple_wool"}, "result": "purple_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "blue_wool"}, "result": "blue_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "brown_wool"}, "result": "brown_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "green_wool"}, "result": "green_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "red_wool"}, "result": "red_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["GGG", "GWG", "GGG"], "key": {"G": "glass", "W": "black_wool"}, "result": "black_stained_glass", "count": 8},
    {"type": "shaped", "pattern": ["TT", "TT"], "key": {"T": "tar_ball"}, "result": "tar_block"},
    {"type": "shaped", "pattern": ["PP", "PP"], "key": {"P": "pointed_dripstone"}, "result": "dripstone_block"},
    {"type": "shaped", "pattern": ["WWW", "WCW", "WWW"], "key": {"W": "wheat", "C": "coal"}, "result": "map"},
    {"type": "shapeless", "ingredients": ["white_wool"], "result": "string", "count": 4},
    {"type": "shaped", "pattern": ["BBB", "BYB", "BBB"], "key": {"B": "bricks", "Y": "yellow_wool"}, "result": "festive_bricks", "count": 8},
    {"type": "shaped", "pattern": ["FFF", "FRF", "FFF"], "key": {"F": "festive_bricks", "R": "ruby"}, "result": "voxelcraft_logo"},
    {"type": "shapeless", "ingredients": ["red_wool", "yellow_wool", "blue_wool", "white_wool"], "result": "confetti_block", "count": 4},
    {"type": "shaped", "pattern": ["W", "S"], "key": {"W": "red_wool", "S": "string"}, "result": "red_balloon"},
    {"type": "shaped", "pattern": ["W", "S"], "key": {"W": "blue_wool", "S": "string"}, "result": "blue_balloon"},
    {"type": "shaped", "pattern": ["W", "S"], "key": {"W": "yellow_wool", "S": "string"}, "result": "yellow_balloon"},
    {"type": "shaped", "pattern": ["I", "P"], "key": {"I": "iron_ingot", "P": "powerstone_dust"}, "result": "party_lantern", "count": 2},
    {"type": "shaped", "pattern": ["PPP", "PWP", "PPP"], "key": {"P": "planks", "W": "red_wool"}, "result": "red_painted_planks", "count": 8},
    {"type": "shaped", "pattern": ["PPP", "PWP", "PPP"], "key": {"P": "planks", "W": "blue_wool"}, "result": "blue_painted_planks", "count": 8},
    {"type": "shaped", "pattern": ["PPP", "PWP", "PPP"], "key": {"P": "planks", "W": "yellow_wool"}, "result": "yellow_painted_planks", "count": 8},
    {"type": "shaped", "pattern": ["PPP", "PWP", "PPP"], "key": {"P": "planks", "W": "green_wool"}, "result": "green_painted_planks", "count": 8},
    {"type": "shapeless", "ingredients": ["anniversary_token", "anniversary_token", "anniversary_token", "anniversary_token", "anniversary_token"], "result": "anniversary_medal"},
    {"type": "shapeless", "ingredients": ["string", "confetti_block"], "result": "party_popper", "count": 4}
  ],
  "fuels": [
    {"item": "tar_ball", "seconds": 40},
    {"item": "tar_block", "seconds": 160}
  ]
})";

const char* kUpdate157 = R"({
  // VoxelCraft 1.5.7: torches, the shield, crimson wood and an easier powerstone lamp.
  "recipes": [
    {"type": "shaped", "pattern": ["C", "S"], "key": {"C": "coal", "S": "stick"}, "result": "torch", "count": 4},
    {"type": "shaped", "pattern": ["PIP", "PPP", ".P."], "key": {"P": "planks", "I": "iron_ingot"}, "result": "shield"},
    {"type": "shapeless", "ingredients": ["crimson_log"], "result": "crimson_planks", "count": 4},
    {"type": "shapeless", "ingredients": ["crimson_planks"], "result": "planks"},
    {"type": "shaped", "pattern": ["D", "G", "T"], "key": {"D": "powerstone_dust", "G": "glass", "T": "torch"}, "result": "powerstone_lamp"},
    {"type": "shaped", "pattern": [".C.", "CBC", ".C."], "key": {"C": "powerstone_chip", "B": "powerstone_block"}, "result": "powerstone_lamp", "count": 4},
    {"type": "shaped", "pattern": ["CCC", "CFC", "BBB"], "key": {"C": "powerstone_chip", "F": "furnace", "B": "powerstone_bricks"}, "result": "powerstone_furnace"},
    {"type": "shapeless", "ingredients": ["feather", "coal", "wheat"], "result": "writable_book"}
  ]
})";

const char* kUpdate15 = R"({
  // VoxelCraft 1.5.0 Forgotten Powerstone: powerstone building blocks, fences, the brusher.
  "recipes": [
    {"type": "shaped", "pattern": ["BD", "DB"], "key": {"B": "stone_bricks", "D": "powerstone_dust"}, "result": "powerstone_bricks", "count": 2},
    {"type": "shaped", "pattern": ["S", "S"], "key": {"S": "powerstone_slab"}, "result": "chiseled_powerstone_bricks"},
    {"type": "shaped", "pattern": ["GGG", "GDG", "GGG"], "key": {"G": "glass", "D": "powerstone_dust"}, "result": "powerstone_glass", "count": 8},
    {"type": "shaped", "pattern": ["M..", "MM.", "MMM"], "key": {"M": "powerstone_bricks"}, "result": "powerstone_stairs", "count": 4},
    {"type": "shaped", "pattern": ["MMM"], "key": {"M": "powerstone_bricks"}, "result": "powerstone_slab", "count": 6},
    {"type": "shaped", "pattern": ["S", "S"], "key": {"S": "nether_brick_slab"}, "result": "chiseled_nether_bricks"},
    {"type": "shaped", "pattern": ["ASA", "ASA"], "key": {"A": "spruce_planks", "S": "stick"}, "result": "spruce_fence", "count": 3},
    {"type": "shaped", "pattern": ["ASA", "ASA"], "key": {"A": "void_planks", "S": "stick"}, "result": "void_fence", "count": 3},
    {"type": "shaped", "pattern": ["ASA", "ASA"], "key": {"A": "netherperk_planks", "S": "stick"}, "result": "netherperk_fence", "count": 3},
    {"type": "shaped", "pattern": ["BBB", "BBB"], "key": {"B": "nether_brick"}, "result": "nether_brick_fence", "count": 6},
    {"type": "shaped", "pattern": ["BBB", "BBB"], "key": {"B": "powerstone_bricks"}, "result": "powerstone_fence", "count": 6},
    {"type": "shaped", "pattern": ["F", "I", "S"], "key": {"F": "feather", "I": "iron_ingot", "S": "stick"}, "result": "brusher"},
    {"type": "shapeless", "ingredients": ["ancient_coin", "ancient_coin", "ancient_coin", "ancient_coin"], "result": "powerstone_dust", "count": 2}
  ],
  "smelting": [
    {"input": "powerstone_bricks", "output": "cracked_powerstone_bricks"},
    {"input": "nether_brick", "output": "cracked_nether_bricks"}
  ],
  "fuels": [
    {"item": "spruce_fence", "seconds": 15},
    {"item": "fence", "seconds": 15}
  ]
})";

// 1.5.0 put the Powerstone Wasteland in the Overworld; since 1.5.1 it lies in the Cornerstone
// Dimension (World::generateNether), so this file is no longer part of the default pack.
const char* kWastelandV150 = R"({
  // 1.5.0: the Powerstone Wasteland, where the forgotten kingdom stood. Dry grass, Grasser
  // flowers, broken pillars and forgotten villages.
  "id": "powerstone_wasteland",
  "name": "Powerstonová pustina",
  "climate": {"temperature": 0.8, "humidity": 0.62},
  "terrain": {"height": 67, "variation": 6},
  "blocks": {"surface": "wasteland_grass", "filler": "dirt", "filler_depth": 3, "beach": "gravel", "underwater": "gravel"},
  "trees": {"density": 0.0008, "trunk": "log", "leaves": "leaves"},
  "plants": {"grass": 0.18, "flowers": 0.02}
})";

const char* kSmelting = R"({
  // Co pec taví a co v ní hoří (sekundy hoření).
  "smelting": [
    {"input": "cobblestone", "output": "stone"},
    {"input": "sand", "output": "glass"},
    {"input": "iron_ore", "output": "iron_ingot"},
    {"input": "log", "output": "coal"},
    {"input": "netherrack", "output": "nether_brick"}
  ],
  "fuels": [
    {"item": "coal", "seconds": 80},
    {"item": "log", "seconds": 15},
    {"item": "planks", "seconds": 15},
    {"item": "crafting_table", "seconds": 15},
    {"item": "wooden_pickaxe", "seconds": 10},
    {"item": "wooden_axe", "seconds": 10},
    {"item": "wooden_shovel", "seconds": 10},
    {"item": "stick", "seconds": 5},
    {"item": "lava", "seconds": 1000}
  ]
})";

ItemId blockOr(const Json& j, ItemId def, std::vector<std::string>* warnings, const std::string& where) {
    if (!j.isString()) return def;
    ItemId id = itemFromKey(j.str());
    if (id == AIR && j.str() != "air") {
        if (warnings) warnings->push_back(where + ": " + trf("neznámý blok \"{0}\"", {j.str()}));
        return def;
    }
    return id;
}

void colorOr(const Json& j, float out[3]) {
    if (!j.isArray() || j.size() < 3) return;
    for (int i = 0; i < 3; ++i) out[i] = std::clamp((float)j[i].num(out[i]), 0.0f, 1.0f);
}

Json color(const float c[3]) {
    Json a = Json::array();
    for (int i = 0; i < 3; ++i) a.push(Json::number(std::round(c[i] * 1000) / 1000));
    return a;
}

Json block(ItemId id) { return Json::string(itemKey(id)); }

BiomeDef parseBiome(const Json& b, std::vector<std::string>* w) {
    BiomeDef d;
    d.id = b["id"].str("biome");
    d.name = b["name"].str(d.id);
    const std::string where = "biom " + d.id;
    d.temperature = (float)b["climate"]["temperature"].num(0.5);
    d.humidity = (float)b["climate"]["humidity"].num(0.5);
    d.height = std::clamp((float)b["terrain"]["height"].num(66), 4.0f, 120.0f);
    d.variation = std::clamp((float)b["terrain"]["variation"].num(8), 0.0f, 60.0f);
    const Json& bl = b["blocks"];
    d.surface = blockOr(bl["surface"], GRASS, w, where);
    d.filler = blockOr(bl["filler"], DIRT, w, where);
    d.fillerDepth = std::clamp((int)bl["filler_depth"].num(3), 0, 20);
    d.beach = blockOr(bl["beach"], SAND, w, where);
    d.underwater = blockOr(bl["underwater"], d.beach, w, where);
    d.treeDensity = std::clamp((float)b["trees"]["density"].num(0), 0.0f, 1.0f);
    d.trunk = blockOr(b["trees"]["trunk"], LOG, w, where);
    d.leaves = blockOr(b["trees"]["leaves"], LEAVES, w, where);
    // Packs written before plants existed have no "plants": grassy biomes then get some anyway.
    bool grassy = d.surface == GRASS;
    d.grassDensity = std::clamp((float)b["plants"]["grass"].num(grassy ? 0.12 : 0), 0.0f, 1.0f);
    d.flowerDensity = std::clamp((float)b["plants"]["flowers"].num(grassy ? 0.015 : 0), 0.0f, 1.0f);
    d.cactusDensity = std::clamp((float)b["plants"]["cactus"].num(0), 0.0f, 1.0f);
    d.treeShape = b["trees"]["shape"].str("oak") == "pine" ? 1 : 0;
    d.freeze = b["freeze"].boolean(false);
    d.peakBlock = blockOr(b["peaks"]["block"], AIR, w, where);
    d.peakHeight = std::clamp((int)b["peaks"]["height"].num(128), 1, 128);
    d.snowHeight = std::clamp((int)b["peaks"]["snow_height"].num(128), 1, 128);
    return d;
}

}  // namespace

const std::vector<std::pair<std::string, std::string>>& defaultPackFiles() {
    static const std::vector<std::pair<std::string, std::string>> files = {
        {"manifest.json", kManifest},
        {"dimensions/overworld.json", kOverworld},
        {"dimensions/nether.json", kNether},
        {"biomes/plains.json", kPlains},
        {"biomes/forest.json", kForest},
        {"biomes/desert.json", kDesert},
        {"biomes/mountains.json", kMountains},
        {"biomes/ocean.json", kOcean},
        {"biomes/snowy_taiga.json", kSnowyTaiga},
        {"biomes/ice_plains.json", kIcePlains},
        {"recipes/crafting.json", kCrafting},
        {"recipes/smelting.json", kSmelting},
        {"recipes/metals.json", kMetals},
        {"recipes/update_1_0.json", kUpdate1},
        {"recipes/update_1_1.json", kUpdate11},
        {"recipes/update_1_2.json", kUpdate12},
        {"recipes/update_1_3.json", kUpdate13},
        {"recipes/update_1_4.json", kUpdate14},
        {"recipes/update_1_5.json", kUpdate15},
        {"recipes/update_1_5_7.json", kUpdate157},
    };
    return files;
}

void upgradeNether(GenConfig& c) {
    // 1.2.0: there is no soul sand or quartz any more; powerstone took their place. Their names
    // are read as the new blocks (itemFromKey), so old generator files need nothing else.
}

void addMissingOres(GenConfig& c) {
    for (const OreDef& d : GenConfig::defaults().overworld.ores) {
        bool present = false;
        for (const OreDef& o : c.overworld.ores) present |= o.block == d.block;
        if (!present) c.overworld.ores.push_back(d);
    }
}

const std::vector<std::pair<std::string, std::string>>& retiredDefaults() {
    static const std::vector<std::pair<std::string, std::string>> files = {
        {"biomes/powerstone_wasteland.json", kWastelandV150},
    };
    return files;
}

void retireBiomes(GenConfig& c) {
    // 1.5.1: the wasteland belongs to the Cornerstone Dimension, not to the Overworld.
    c.biomes.erase(std::remove_if(c.biomes.begin(), c.biomes.end(), [](const BiomeDef& b) { return b.id == "powerstone_wasteland"; }),
                   c.biomes.end());
}

const std::vector<std::pair<std::string, std::string>>& upgradableDefaults() {
    static const std::vector<std::pair<std::string, std::string>> files = {
        {"biomes/desert.json", kDesertV1},
        {"biomes/mountains.json", kMountainsV1},
        {"dimensions/nether.json", kNetherV1},
        {"recipes/update_1_1.json", kUpdate11V1},
    };
    return files;
}

const GenConfig& GenConfig::defaults() {
    static const GenConfig cfg = [] {
        Json doc = Json::object(), biomes = Json::array();
        std::string err;
        for (const auto& [path, text] : defaultPackFiles()) {
            Json j;
            if (!Json::parse(text, j, err)) continue;
            if (path == "dimensions/overworld.json") doc.set("overworld", j);
            else if (path == "dimensions/nether.json") doc.set("nether", j);
            else if (path.rfind("biomes/", 0) == 0) biomes.push(j);
        }
        doc.set("biomes", biomes);
        return fromJson(doc);
    }();
    return cfg;
}

GenConfig GenConfig::fromJson(const Json& doc, std::vector<std::string>* w) {
    GenConfig c;
    const Json& o = doc["overworld"];
    c.overworld.seaLevel = std::clamp((int)o["sea_level"].num(62), 1, 120);
    c.overworld.caves = o["caves"]["enabled"].boolean(true);
    c.overworld.caveSize = std::clamp((float)o["caves"]["size"].num(0.06), 0.0f, 0.5f);
    c.overworld.caverns = o["caves"]["caverns"].boolean(true);
    c.overworld.structures = o["structures"].boolean(true);
    c.overworld.biomeScale = std::clamp((float)o["biome_size"].num(320), 16.0f, 4096.0f);
    colorOr(o["sky_color"], c.overworld.sky);
    const Json& ores = o["ores"];
    for (size_t i = 0; i < ores.size(); ++i) {
        OreDef d;
        d.block = blockOr(ores[i]["block"], AIR, w, "ruda");
        d.veins = std::clamp((int)ores[i]["veins"].num(0), 0, 64);
        d.size = std::clamp((int)ores[i]["size"].num(6), 1, 64);
        d.minY = std::clamp((int)ores[i]["min_y"].num(5), 1, 126);
        d.maxY = std::clamp((int)ores[i]["max_y"].num(64), d.minY + 1, 127);
        if (d.block != AIR) c.overworld.ores.push_back(d);
    }

    const Json& n = doc["nether"];
    const Json& nb = n["blocks"];
    c.nether.lavaLevel = std::clamp((int)n["lava_level"].num(31), 0, 120);
    c.nether.fill = blockOr(nb["fill"], NETHERRACK, w, "nether");
    c.nether.liquid = blockOr(nb["liquid"], LAVA, w, "nether");
    c.nether.floor = blockOr(nb["floor"], NETHER_GRASS, w, "nether");
    c.nether.shore = blockOr(nb["shore"], GRAVEL, w, "nether");
    c.nether.ore = blockOr(nb["ore"], POWERSTONE_ORE, w, "nether");
    c.nether.ceiling = blockOr(nb["ceiling"], BEDROCK, w, "nether");
    c.nether.floorAmount = std::clamp((float)n["floor_amount"].num(0.25), 0.0f, 1.0f);
    c.nether.oreChance = std::clamp((float)n["ore_chance"].num(1 / 120.0), 0.0f, 1.0f);
    c.nether.glowstoneClusters = std::clamp((int)n["glowstone_clusters"].num(2), 0, 16);
    c.nether.caveScale = std::clamp((float)n["caves"]["scale"].num(32), 4.0f, 256.0f);
    c.nether.openness = std::clamp((float)n["caves"]["openness"].num(0.05), -1.0f, 1.0f);
    colorOr(n["fog_color"], c.nether.fog);

    const Json& biomes = doc["biomes"];
    for (size_t i = 0; i < biomes.size(); ++i) c.biomes.push_back(parseBiome(biomes[i], w));
    if (c.biomes.empty()) {
        if (w) w->push_back(tr("žádné biomy, použity výchozí"));
        c.biomes = defaults().biomes;
    }
    c.flat = doc["flat"].boolean(false);
    c.crimsonDesert = std::clamp((int)doc["crimson_desert"].num(2), 1, 2);
    c.generation = std::clamp((int)doc["generation"].num(2), 1, 2);
    return c;
}

Json GenConfig::toJson() const {
    Json o = Json::object();
    o.set("sea_level", Json::number(overworld.seaLevel));
    Json caves = Json::object();
    caves.set("enabled", Json::fromBool(overworld.caves));
    caves.set("size", Json::number(overworld.caveSize));
    caves.set("caverns", Json::fromBool(overworld.caverns));
    o.set("caves", caves);
    o.set("biome_size", Json::number(overworld.biomeScale));
    o.set("sky_color", color(overworld.sky));
    Json ores = Json::array();
    for (const OreDef& d : overworld.ores) {
        Json r = Json::object();
        r.set("block", block(d.block));
        r.set("veins", Json::number(d.veins));
        r.set("size", Json::number(d.size));
        r.set("min_y", Json::number(d.minY));
        r.set("max_y", Json::number(d.maxY));
        ores.push(r);
    }
    if (!overworld.structures) o.set("structures", Json::fromBool(false));
    o.set("ores", ores);

    Json n = Json::object(), nb = Json::object(), nc = Json::object();
    n.set("lava_level", Json::number(nether.lavaLevel));
    nb.set("fill", block(nether.fill));
    nb.set("liquid", block(nether.liquid));
    nb.set("floor", block(nether.floor));
    nb.set("shore", block(nether.shore));
    nb.set("ore", block(nether.ore));
    nb.set("ceiling", block(nether.ceiling));
    n.set("blocks", nb);
    n.set("floor_amount", Json::number(nether.floorAmount));
    n.set("ore_chance", Json::number(nether.oreChance));
    n.set("glowstone_clusters", Json::number(nether.glowstoneClusters));
    nc.set("scale", Json::number(nether.caveScale));
    nc.set("openness", Json::number(nether.openness));
    n.set("caves", nc);
    n.set("fog_color", color(nether.fog));

    Json bs = Json::array();
    for (const BiomeDef& d : biomes) {
        Json b = Json::object(), cl = Json::object(), te = Json::object(), bl = Json::object(), tr = Json::object();
        b.set("id", Json::string(d.id));
        b.set("name", Json::string(d.name));
        cl.set("temperature", Json::number(d.temperature));
        cl.set("humidity", Json::number(d.humidity));
        b.set("climate", cl);
        te.set("height", Json::number(d.height));
        te.set("variation", Json::number(d.variation));
        b.set("terrain", te);
        bl.set("surface", block(d.surface));
        bl.set("filler", block(d.filler));
        bl.set("filler_depth", Json::number(d.fillerDepth));
        bl.set("beach", block(d.beach));
        bl.set("underwater", block(d.underwater));
        b.set("blocks", bl);
        tr.set("density", Json::number(d.treeDensity));
        tr.set("trunk", block(d.trunk));
        tr.set("leaves", block(d.leaves));
        if (d.treeShape == 1) tr.set("shape", Json::string("pine"));
        b.set("trees", tr);
        if (d.freeze) b.set("freeze", Json::fromBool(true));
        if (d.peakBlock || d.snowHeight < 128) {
            Json pk = Json::object();
            if (d.peakBlock) pk.set("block", block(d.peakBlock));
            pk.set("height", Json::number(d.peakHeight));
            pk.set("snow_height", Json::number(d.snowHeight));
            b.set("peaks", pk);
        }
        Json pl = Json::object();
        pl.set("grass", Json::number(d.grassDensity));
        pl.set("flowers", Json::number(d.flowerDensity));
        if (d.cactusDensity > 0) pl.set("cactus", Json::number(d.cactusDensity));
        b.set("plants", pl);
        bs.push(b);
    }

    Json doc = Json::object();
    doc.set("flat", Json::fromBool(flat));
    doc.set("crimson_desert", Json::number(crimsonDesert));
    doc.set("generation", Json::number(generation));
    doc.set("overworld", o);
    doc.set("nether", n);
    doc.set("biomes", bs);
    return doc;
}

}  // namespace vc

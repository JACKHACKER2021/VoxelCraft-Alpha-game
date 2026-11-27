#pragma once
// 2.8.0 preview: the game's own data in the VC language. Every block and item of the game (its name,
// its texture on each face, how hard it is, how many fit in a slot) is in vc/blocks.json, and what
// structure chests hold is in vc/loot.json, both unpacked from the game's assets into <data>/vc.
// A pack may bring its own vc/blocks.json or vc/loot.json: its entries replace the game's by key,
// so blocks, items and loot can be changed without touching the code. Without the files the values
// written in the code are used (the normal edition always uses those).
#include <cstdint>
#include <string>
#include <vector>

#include "Blocks.h"

namespace vc {

class Json;

// Reads <dir>/vc/blocks.json and <dir>/vc/loot.json of each folder, later folders overriding earlier ones.
// Returns how many entries were read.
int loadVcData(const std::vector<std::string>& dirs, std::vector<std::string>* warnings = nullptr);
void clearVcData();

// The overrides (the hooks in Blocks.h and Items.cpp use these). -1 / negative / null: none.
extern const int16_t* gVcTiles;  // 6 per id
float vcHardness(ItemId id);     // < -1.5: none
int vcStack(ItemId id);          // 0: none
const char* vcName(ItemId id);   // null: none

struct VcLootEntry {
    ItemId id;
    int weight, minCount, maxCount;
};
struct VcLootTable {
    int rollsMin = 3, rollsMax = 6;
    std::vector<VcLootEntry> entries;
};
const VcLootTable* vcLoot(ItemId chest);

// The game's data as the code has it, written as the files (tools/export_vc_data.cpp).
std::string exportVcBlocks();
std::string exportVcLoot();  // in Loot.cpp, which has the tables
void vcExporting(bool on);   // while on, the overrides are not used
std::string vcKey(ItemId id);  // the key files name a block or item by ("" for ids the game does not use)

}  // namespace vc

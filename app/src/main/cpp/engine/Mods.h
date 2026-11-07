#pragma once
#include <string>
#include <utility>
#include <vector>

#include "BlockShapes.h"
#include "Blocks.h"
#include "Mobs.h"

namespace vc {

// VoxelCraft Mods (1.4.0, 2.2.0): blocks, items and mobs from JSON and PNG files in a pack.
//
// A pack (see Packs.h) may hold content.json:
//   {"namespace": "mymod",
//    "blocks": [{"id": "ruby_bricks", "name": "Ruby Bricks", "name_cs": "Rubínové cihly",
//                "texture": "ruby_bricks.png"  or  "textures": {"top": ..., "side": ..., "bottom": ...},
//                "hardness": 1.5, "tool": "pickaxe", "transparent": false, "light": false,
//                "drop": "mymod:ruby_dust", "group": "Lokomotivy",
//                "has_inventory": true, "inventory_size": 27, "rideable": false, "power": true,
//                "boxes": [[x0, y0, z0, x1, y1, z1], ...]}],
//    "items":  [{"id": "ruby_apple", "name": "Ruby Apple", "texture": "ruby_apple.png", "food": 6, "stack": 64,
//                "group": "Vagóny", "places": "mymod:ruby_bricks"}],
//    "mobs":   [{"id": "ruby_pig", "name": "Ruby Pig", "base": "pig", "texture": "ruby_pig.png",
//                "face": "ruby_pig_face.png", "health": 12, "spawn": 0.2,
//                "drops": [{"item": "mymod:ruby_apple", "count": 1}]}]}
// Textures are read from textures/mod/ of the pack. Recipes go to recipes/*.json as usual and
// name the new things "mymod:ruby_bricks". Mods add content only: they never run code.
//
// Every new block and item gets an id from 2000 up, remembered in mod_ids.txt so worlds keep
// their blocks when mods are switched off and on again or loaded in another order.
struct ModBlock {
    std::string key, name, nameCs;  // key "namespace:id"
    int tiles[6] = {};               // per Face
    float hardness = 1.5f;
    std::string tool;                // "pickaxe", "axe", "shovel" or ""
    bool transparent = false, light = false;
    std::string drop;                // key of what breaking it gives ("" = itself)
    std::string group;               // category/group in creative inventory
    bool hasInventory = false;       // container / self-inventory (e.g. Boxcar)
    int inventorySize = 0;           // container size (e.g. 27)
    bool rideable = false;           // can be ridden (e.g. locomotives)
    bool respondsToPower = false;    // activated by powerstone
    std::vector<Box> customBoxes;    // custom geometry boxes for 3D rendering and collision
};

struct ModItem {
    std::string key, name, nameCs;
    int tile = 0;
    int food = 0, stack = 64;
    std::string group;               // category/group in creative inventory
    std::string placesKey;           // if placer, key of block/entity placed
};

struct ModMob {
    std::string key, name, nameCs;
    MobType base = MobType::Pig;
    int skin = -1, face = -1;  // atlas tiles
    float health = 10, spawn = 0.1f;
    std::vector<std::pair<std::string, int>> drops;  // item key, count
};

class ModRegistry {
public:
    static ModRegistry& get();

    // Reads content.json of every pack directory (in order). Ids are kept in `idFile`.
    void load(const std::vector<std::string>& packDirs, const std::string& idFile, std::vector<std::string>* warnings = nullptr);
    void clear();

    const ModBlock* block(ItemId id) const;
    const ModItem* item(ItemId id) const;
    const std::vector<ModMob>& mobs() const { return mobs_; }
    const ModMob* mob(int variant) const;  // Mob::variant (1-based), null for 0
    ItemId find(const std::string& key) const;  // "namespace:id", AIR if unknown
    const char* name(ItemId id) const;          // in the current language
    const char* key(ItemId id) const;
    // The creative tab "Mods": every block and item, in load order.
    const std::vector<ItemId>& creative() const { return creative_; }
    // Atlas tiles to fill: (tile, PNG path).
    const std::vector<std::pair<int, std::string>>& textures() const { return textures_; }
    // How much each pack added, for the mod manager.
    struct Summary {
        int blocks = 0, items = 0, mobs = 0;
    };
    Summary summary(const std::string& packDir) const;

    const char* group(ItemId id) const;
    std::vector<std::string> groups() const;
    std::vector<ItemId> itemsInGroup(const std::string& grp) const;
    bool isShapedBlock(ItemId id) const;
    bool hasInventory(ItemId id) const;
    bool isRideable(ItemId id) const;
    bool respondsToPower(ItemId id) const;
    ItemId keyToId(const std::string& key) const { return find(key); }

private:
    struct Entry {
        ItemId id;
        bool isBlock;
        size_t index;
    };
    ItemId idFor(const std::string& key);
    int tileFor(const std::string& path);
    std::vector<ModBlock> blocks_;
    std::vector<ModItem> items_;
    std::vector<ModMob> mobs_;
    std::vector<Entry> entries_;                       // sorted by id
    std::vector<std::pair<std::string, ItemId>> ids_;  // every id ever given out
    std::vector<ItemId> creative_;
    std::vector<std::pair<int, std::string>> textures_;
    std::vector<std::pair<std::string, Summary>> summaries_;
    int nextTile_ = T_COUNT;
    bool idsChanged_ = false;
};

}  // namespace vc

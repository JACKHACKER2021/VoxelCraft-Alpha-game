#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <string>

#include "Blocks.h"

namespace vc {

// Items that never stand in the world: 200 and up, and the ranges 1.2.0 took below that.
inline bool isItemOnly(ItemId id) {
    return (id >= 186 && id < STAIRS) || (id >= STAIRS && !isNewBlock(id)) || (id >= 99 && id <= 111) || (id >= 154 && id <= 158);
}
inline bool isPlaceable(ItemId id) { return id != AIR && !isItemOnly(id); }

const char* itemName(ItemId id);
// Stable English keys used in JSON files ("stone", "oak_log", ...).
const char* itemKey(ItemId id);
ItemId itemFromKey(const std::string& key);  // AIR when unknown

enum class ToolKind { None, Pickaxe, Axe, Shovel, Sword };
ToolKind toolKind(ItemId id);
int toolTier(ItemId id);        // 0 hand, 1 wood, 2 stone, 3 iron
int toolDurability(ItemId id);  // uses before the tool breaks, 0 for non-tools
int maxStack(ItemId id);

// Armour: piece 0 helmet, 1 chestplate, 2 leggings, 3 boots; material 0 wood,
// 1 cobblestone, 2 bronze, 3 steel, 4 ruby. -1 for anything else.
int armorPiece(ItemId id);
int armorMaterial(ItemId id);
ItemId armorItem(int material, int piece);
int armorPoints(ItemId id);  // protection of one piece; a full ruby set gives 20
// Share of damage the worn armour takes away (4 % per point, at most 80 %).
struct ItemStack;
float armorReduction(const ItemStack armor[4]);

// True if breaking `block` with `tool` (AIR = bare hand) gives a drop.
bool canHarvest(ItemId block, ItemId tool);
// Seconds to break in survival with the given tool; negative means unbreakable.
float breakTime(ItemId block, ItemId tool = AIR);

// What breaking a block gives in survival (AIR for nothing). `roll` is a
// random number in [0, 1) used for chance drops (apples, flint).
ItemId dropFor(ItemId block, float roll, ItemId tool = AIR);

// Everything shown in the creative inventory, in display order.
extern const ItemId kCreativeItems[];
extern const int kCreativeItemCount;
// Tabs of the creative inventory: a name and a range of kCreativeItems.
struct CreativeTab {
    const char* name;
    int first, count;
};
extern const CreativeTab kCreativeTabs[];
extern const int kCreativeTabCount;

// 1.5.9: the creative inventory's categories, sorted from the same catalog (every item in
// exactly one), and groups of variants shown behind one button (stained glass, stairs...).
enum CreativeCategory {
    CatBuilding, CatGlass, CatLighting, CatNature, CatDecoration, CatPowerstone, CatTools, CatArmor, CatMaps,
    CatMaterials, CatEggs, CatCount
};
const char* creativeCategoryName(int category);
ItemId creativeCategoryIcon(int category);
const std::vector<ItemId>& creativeCategoryItems(int category);
// The group of variants an item belongs to in the creative inventory, or nullptr.
const char* creativeGroup(ItemId id);

struct ItemStack {
    ItemId id = AIR;
    uint8_t count = 0;
    uint16_t damage = 0;  // tools: uses so far
    bool empty() const { return id == AIR || count == 0; }
    bool operator==(const ItemStack& o) const { return id == o.id && count == o.count && damage == o.damage; }
};

constexpr int kMaxStack = 64;
constexpr int kHotbarSize = 9;
constexpr int kInventorySize = 36;  // slots 0-8 are the hotbar

// Moves `from` onto `to`: merges equal stackable items, otherwise swaps.
void moveOrSwap(ItemStack& from, ItemStack& to);

// Moves up to `count` items from `from` onto `to`, never creating or losing any:
//  - empty target: the items move there (a split when count < from.count),
//  - same item: they merge up to the stack limit, the rest stays in `from`,
//  - different item: the two stacks swap, but only when the whole stack moves.
// Returns how many items left `from` (for a swap: its whole count), 0 if nothing changed.
int transferItems(ItemStack& from, ItemStack& to, int count);

// Moves up to `count` items from `from` into the stacks [begin, end): first onto stacks
// of the same item, then into empty ones. Returns how many moved.
int moveIntoRange(ItemStack& from, int count, ItemStack* begin, ItemStack* end);

struct Inventory {
    std::array<ItemStack, kInventorySize> slots{};

    // Adds items, filling existing stacks first. Returns how many did not fit.
    int add(ItemId id, int count, uint16_t damage = 0);
    int add(const ItemStack& s) { return add(s.id, s.count, s.damage); }
    void removeOne(int slot);
    void clear() { slots.fill({}); }
    void moveOrSwap(int from, int to) { if (from != to) vc::moveOrSwap(slots[from], slots[to]); }
};

}  // namespace vc

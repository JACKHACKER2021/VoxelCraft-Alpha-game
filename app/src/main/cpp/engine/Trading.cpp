#include "Trading.h"

#include <algorithm>

namespace vc {

const std::vector<Trade>& tradesFor(Profession p) {
    static const std::vector<Trade> kFarmer = {
        {{WHEAT, 16, 0}, {RUBY, 1, 0}},
        {{RUBY, 1, 0}, {BREAD, 6, 0}},
        {{RUBY, 1, 0}, {WHEAT_SEEDS, 12, 0}},
        {{RAW_CHICKEN, 8, 0}, {RUBY, 1, 0}},
        {{RUBY, 2, 0}, {COOKED_PORK, 8, 0}},
        {{RUBY, 3, 0}, {HOE, 1, 0}},
    };
    static const std::vector<Trade> kSmith = {
        {{COAL, 12, 0}, {RUBY, 1, 0}},
        {{IRON_INGOT, 6, 0}, {RUBY, 1, 0}},
        {{RUBY, 3, 0}, {STEEL_SWORD, 1, 0}},
        {{RUBY, 4, 0}, {STEEL_PICKAXE, 1, 0}},
        {{RUBY, 6, 0}, {armorItem(3, 1), 1, 0}},
        {{RUBY, 2, 0}, {BUCKET, 1, 0}},
    };
    static const std::vector<Trade> kTrader = {
        {{RUBY, 1, 0}, {ARROW, 12, 0}},
        {{RUBY, 3, 0}, {BOW, 1, 0}},
        {{FEATHER, 10, 0}, {RUBY, 1, 0}},
        {{RUBY, 2, 0}, {GLASS, 16, 0}},
        {{RUBY, 2, 0}, {BOAT, 1, 0}},
        {{RUBY, 4, 0}, {EGG_COW, 1, 0}},
    };
    return p == Profession::Smith ? kSmith : p == Profession::Trader ? kTrader : kFarmer;
}

const char* professionName(Profession p) {
    return p == Profession::Smith ? "Kovář" : p == Profession::Trader ? "Obchodník" : "Farmář";
}

int countInInventory(const Inventory& inv, ItemId id) {
    int n = 0;
    for (const ItemStack& s : inv.slots)
        if (s.id == id) n += s.count;
    return n;
}

bool makeTrade(Inventory& inv, const Trade& t) {
    if (countInInventory(inv, t.cost.id) < t.cost.count) return false;
    Inventory trial = inv;
    int left = t.cost.count;
    for (ItemStack& s : trial.slots)
        if (s.id == t.cost.id && left > 0) {
            int take = std::min<int>(left, s.count);
            s.count = (uint8_t)(s.count - take);
            left -= take;
            if (s.count == 0) s = {};
        }
    if (trial.add(t.result) > 0) return false;  // no room
    inv = trial;
    return true;
}

}  // namespace vc

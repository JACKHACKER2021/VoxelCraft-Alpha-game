#include "Crafting.h"
#include "Lang.h"

#include "GenConfig.h"

#include <algorithm>
#include <string>
#include <vector>

namespace vc {

namespace {

RecipeBook& current() {
    static RecipeBook book = RecipeBook::defaults();
    return book;
}

ItemId item(const Json& j, std::vector<std::string>* w, const std::string& where) {
    ItemId id = itemFromKey(j.str(""));
    if (id == AIR && w) w->push_back(where + ": " + trf("neznámý předmět \"{0}\"", {j.str("")}));
    return id;
}

bool matchesAt(const ShapedRecipe& r, const ItemStack* grid, int size, int ox, int oy, bool mirror) {
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            ItemId want = AIR;
            int rx = x - ox, ry = y - oy;
            if (rx >= 0 && ry >= 0 && rx < r.w && ry < r.h) want = r.cells[ry * r.w + (mirror ? r.w - 1 - rx : rx)];
            const ItemStack& s = grid[y * size + x];
            if ((s.empty() ? (ItemId)AIR : s.id) != want) return false;
        }
    return true;
}

}  // namespace

void RecipeBook::addJson(const Json& doc, std::vector<std::string>* w) {
    if (doc["replace"].boolean(false)) *this = RecipeBook{};
    const Json& list = doc["recipes"];
    for (size_t i = 0; i < list.size(); ++i) {
        const Json& r = list[i];
        const std::string where = "recept " + std::to_string(i + 1);
        CraftResult result{item(r["result"], w, where), std::clamp((int)r["count"].num(1), 1, 64)};
        if (result.empty()) continue;
        // 1.5.1: an Eye Log never turns into void planks, whatever a pack says.
        bool eyeLog = false;
        for (size_t k = 0; k < r["ingredients"].size(); ++k) eyeLog |= itemFromKey(r["ingredients"][k].str("")) == EYE_LOG;
        for (const std::string& key : r["key"].keys()) eyeLog |= itemFromKey(r["key"][key].str("")) == EYE_LOG;
        if (eyeLog && result.id == VOID_PLANKS) continue;
        if (r["type"].str("shaped") == "shapeless") {
            ShapelessRecipe s{{}, result};
            bool ok = r["ingredients"].size() > 0;
            for (size_t k = 0; k < r["ingredients"].size(); ++k) {
                ItemId id = item(r["ingredients"][k], w, where);
                ok = ok && id != AIR;
                s.ingredients.push_back(id);
            }
            std::sort(s.ingredients.begin(), s.ingredients.end());
            if (ok && s.ingredients.size() <= 9) shapeless.push_back(std::move(s));
            continue;
        }
        const Json& pattern = r["pattern"];
        ShapedRecipe s;
        s.h = (int)pattern.size();
        for (size_t k = 0; k < pattern.size(); ++k) s.w = std::max(s.w, (int)pattern[k].str("").size());
        if (s.w == 0 || s.w > 3 || s.h > 3) {
            if (w) w->push_back(where + ": " + tr("vzor musí mít 1 až 3 řádky a sloupce"));
            continue;
        }
        bool ok = true;
        for (int y = 0; y < s.h; ++y) {
            std::string row = pattern[y].str("");
            for (int x = 0; x < s.w; ++x) {
                char c = x < (int)row.size() ? row[x] : '.';
                ItemId id = AIR;
                if (c != '.' && c != ' ') {
                    id = item(r["key"][std::string(1, c)], w, where + " (" + c + ")");
                    ok = ok && id != AIR;
                }
                s.cells.push_back(id);
            }
        }
        s.result = result;
        if (ok) shaped.push_back(std::move(s));
    }
    const Json& sm = doc["smelting"];
    for (size_t i = 0; i < sm.size(); ++i) {
        ItemId in = item(sm[i]["input"], w, "tavení"), out = item(sm[i]["output"], w, "tavení");
        if (in != AIR && out != AIR) smelting[in] = out;
    }
    const Json& fu = doc["fuels"];
    for (size_t i = 0; i < fu.size(); ++i) {
        ItemId id = item(fu[i]["item"], w, "palivo");
        if (id != AIR) fuels[id] = std::clamp((float)fu[i]["seconds"].num(0), 0.0f, 100000.0f);
    }
}

const RecipeBook& RecipeBook::defaults() {
    static const RecipeBook book = [] {
        RecipeBook b;
        for (const auto& [path, text] : defaultPackFiles()) {
            if (path.rfind("recipes/", 0) != 0) continue;
            Json j;
            std::string err;
            if (Json::parse(text, j, err)) b.addJson(j);
        }
        return b;
    }();
    return book;
}

const RecipeBook& recipes() { return current(); }
void setRecipes(RecipeBook book) { current() = std::move(book); }

CraftResult matchRecipe(const ItemStack* grid, int size) {
    const RecipeBook& book = recipes();
    for (const ShapedRecipe& r : book.shaped) {
        if (r.w > size || r.h > size) continue;
        for (int oy = 0; oy + r.h <= size; ++oy)
            for (int ox = 0; ox + r.w <= size; ++ox)
                if (matchesAt(r, grid, size, ox, oy, false) || matchesAt(r, grid, size, ox, oy, true)) return r.result;
    }
    std::vector<ItemId> items;
    for (int i = 0; i < size * size; ++i)
        if (!grid[i].empty()) items.push_back(grid[i].id);
    std::sort(items.begin(), items.end());
    for (const ShapelessRecipe& r : book.shapeless)
        if (r.ingredients == items) return r.result;
    return {};
}

ItemId smeltResult(ItemId id) {
    auto it = recipes().smelting.find(id);
    return it == recipes().smelting.end() ? (ItemId)AIR : it->second;
}

float fuelSeconds(ItemId id) {
    auto it = recipes().fuels.find(id);
    return it == recipes().fuels.end() ? 0.0f : it->second;
}

bool Furnace::canSmelt() const {
    if (input.empty()) return false;
    ItemId out = smeltResult(input.id);
    if (out == AIR) return false;
    return output.empty() || (output.id == out && output.count < maxStack(out));
}

bool Furnace::update(float dt) {
    bool wasBurning = burning();
    if (burnLeft > 0) burnLeft = std::max(0.0f, burnLeft - dt);
    if (!burning() && canSmelt() && !fuel.empty() && fuelSeconds(fuel.id) > 0) {
        burnTotal = burnLeft = fuelSeconds(fuel.id);
        if (--fuel.count == 0) fuel = {};
    }
    if (burning() && canSmelt()) {
        cook += dt;
        if (cook >= kSmeltSeconds) {
            cook = 0;
            ItemId out = smeltResult(input.id);
            if (output.empty()) output = {out, 1};
            else ++output.count;
            if (--input.count == 0) input = {};
        }
    } else {
        cook = std::max(0.0f, cook - dt * 2);  // progress cools down like in the original
    }
    return wasBurning != burning();
}

}  // namespace vc

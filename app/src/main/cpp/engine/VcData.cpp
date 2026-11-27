#include "VcData.h"

#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>

#include "Items.h"
#include "Json.h"
#include "Lang.h"

namespace vc {

const int16_t* gVcTiles = nullptr;

namespace {

bool gExporting = false;  // the code's own values while writing the files

constexpr int kIds = 65536;
std::unique_ptr<int16_t[]> gTiles;
std::vector<float> gHard;
std::vector<int> gStack;
std::map<ItemId, std::string> gNames;
std::map<ItemId, VcLootTable> gLoot;

bool builtIn(int id) { return !isModId(id) && id != AIR; }

bool readJson(const std::string& path, Json& doc, std::string& err) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    return Json::parse(ss.str(), doc, err);
}

}  // namespace

void clearVcData() {
    gVcTiles = nullptr;
    gTiles.reset();
    gHard.clear(), gStack.clear(), gNames.clear(), gLoot.clear();
}

int loadVcData(const std::vector<std::string>& dirs, std::vector<std::string>* warnings) {
    clearVcData();
    int count = 0;
    for (const std::string& d : dirs) {
        Json doc;
        std::string err;
        const std::string bp = d + "/vc/blocks.json";
        if (readJson(bp, doc, err)) {
            const Json& list = doc["blocks"];
            for (size_t i = 0; i < list.size(); ++i) {
                const Json& e = list[i];
                const ItemId id = itemFromKey(e["key"].str(""));
                if (id == AIR || !builtIn(id)) {
                    if (warnings) warnings->push_back(bp + ": " + trf("neznámý předmět \"{0}\"", {e["key"].str("")}));
                    continue;
                }
                if (e["tiles"].isArray()) {
                    if (!gTiles) {
                        gTiles.reset(new int16_t[(size_t)kIds * 6]);
                        std::fill(gTiles.get(), gTiles.get() + (size_t)kIds * 6, (int16_t)-1);
                    }
                    for (size_t f = 0; f < 6 && f < e["tiles"].size(); ++f) {
                        const int t = (int)e["tiles"][f].num(-1);
                        if (t >= 0 && t < kAtlasCols * kAtlasCols) gTiles[(size_t)id * 6 + f] = (int16_t)t;
                    }
                }
                if (e["hardness"].isNumber()) {
                    if (gHard.empty()) gHard.assign(kIds, -2.0f);
                    gHard[id] = (float)e["hardness"].num(0);
                }
                if (e["stack"].isNumber()) {
                    if (gStack.empty()) gStack.assign(kIds, 0);
                    gStack[id] = std::clamp((int)e["stack"].num(64), 1, 64);
                }
                if (e["name"].isString()) gNames[id] = e["name"].str("");
                ++count;
            }
        } else if (!err.empty() && warnings) {
            warnings->push_back(bp + ": " + err);
        }
        Json loot;
        err.clear();
        const std::string lp = d + "/vc/loot.json";
        if (readJson(lp, loot, err)) {
            for (const std::string& key : loot["chests"].keys()) {
                const Json& c = loot["chests"][key];
                const ItemId chest = itemFromKey(key);
                if (chest == AIR) continue;
                VcLootTable t;
                t.rollsMin = (int)c["rolls"][0].num(3), t.rollsMax = std::max(t.rollsMin, (int)c["rolls"][1].num(t.rollsMin));
                for (size_t i = 0; i < c["entries"].size(); ++i) {
                    const Json& e = c["entries"][i];
                    const ItemId id = itemFromKey(e["item"].str(""));
                    if (id == AIR) {
                        if (warnings) warnings->push_back(lp + ": " + trf("neznámý předmět \"{0}\"", {e["item"].str("")}));
                        continue;
                    }
                    const int lo = std::max(1, (int)e["count"][0].num(1));
                    t.entries.push_back({id, std::max(1, (int)e["weight"].num(1)), lo, std::max(lo, (int)e["count"][1].num(lo))});
                }
                if (!t.entries.empty()) gLoot[chest] = std::move(t), ++count;
            }
        } else if (!err.empty() && warnings) {
            warnings->push_back(lp + ": " + err);
        }
    }
    gVcTiles = gTiles.get();
    return count;
}

float vcHardness(ItemId id) { return gHard.empty() || gExporting ? -2.0f : gHard[id]; }
int vcStack(ItemId id) { return gStack.empty() || gExporting ? 0 : gStack[id]; }
const char* vcName(ItemId id) {
    if (gNames.empty() || gExporting) return nullptr;
    auto it = gNames.find(id);
    return it == gNames.end() ? nullptr : it->second.c_str();
}
const VcLootTable* vcLoot(ItemId chest) {
    if (gExporting) return nullptr;
    auto it = gLoot.find(chest);
    return it == gLoot.end() ? nullptr : &it->second;
}

void vcExporting(bool on) { gExporting = on; }

std::string vcKey(ItemId id) {
    // Its own name when it has one that leads back to it, else "vc:id/<number>" for anything the game names.
    const char* key = itemKey(id);
    if (key && *key && std::string(key) != "air" && itemFromKey(key) == id) return key;
    const char* name = itemName(id);
    if (!name || !*name) return "";
    return "vc:id/" + std::to_string((int)id);
}

std::string exportVcBlocks() {
    // Written with the code's values: nothing changes until someone edits the file.
    const int16_t* keep = gVcTiles;
    gVcTiles = nullptr, gExporting = true;
    std::ostringstream o;
    o << "{\"format\": \"vc:blocks\", \"version\": \"2.8.0\",\n \"blocks\": [\n";
    bool first = true;
    for (int id = 1; id < kIds; ++id) {
        if (!builtIn(id)) continue;
        const std::string key = vcKey((ItemId)id);
        if (key.empty()) continue;
        Json e = Json::object(), tiles = Json::array();
        e.set("key", Json::string(key));
        // Names stay with the game (they differ between editions and are translated); a pack may still set "name".
        for (int f = 0; f < 6; ++f) tiles.push(Json::number(tileFor((ItemId)id, (Face)f)));
        e.set("tiles", tiles);
        if (isPlaceable((ItemId)id)) e.set("hardness", Json::number(blockHardness((ItemId)id)));
        e.set("stack", Json::number(maxStack((ItemId)id)));
        o << (first ? "  " : ",\n  ") << e.dump();
        first = false;
    }
    o << "\n ]}\n";
    gVcTiles = keep, gExporting = false;
    return o.str();
}

}  // namespace vc

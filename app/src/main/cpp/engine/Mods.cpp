#include "Mods.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Json.h"
#include "Lang.h"

namespace vc {

namespace fs = std::filesystem;

namespace {

MobType baseMob(const std::string& name) {
    if (name == "sheep") return MobType::Sheep;
    if (name == "cow") return MobType::Cow;
    if (name == "chicken") return MobType::Chicken;
    if (name == "spider") return MobType::Spider;
    if (name == "zombie" || name == "forgotten_man") return MobType::ForgottenMan;
    if (name == "slime" || name == "pumpkin_slime") return MobType::PumpkinSlime;
    if (name == "villager") return MobType::Villager;
    return MobType::Pig;
}

bool validPart(const std::string& s) {
    if (s.empty() || s.size() > 48) return false;
    for (char c : s)
        if (!(std::islower((unsigned char)c) || std::isdigit((unsigned char)c) || c == '_' || c == '.' || c == '-')) return false;
    return true;
}

}  // namespace

ModRegistry& ModRegistry::get() {
    static ModRegistry registry;
    return registry;
}

void ModRegistry::clear() {
    blocks_.clear();
    items_.clear();
    mobs_.clear();
    entries_.clear();
    creative_.clear();
    textures_.clear();
    summaries_.clear();
    nextTile_ = T_COUNT;
}

ItemId ModRegistry::idFor(const std::string& key) {
    for (auto& [k, id] : ids_)
        if (k == key) return id;
    ItemId next = kModIdBase;
    for (auto& [k, id] : ids_) next = std::max<ItemId>(next, (ItemId)(id + 1));
    if (next >= kItemIdLimit) return AIR;
    ids_.emplace_back(key, next);
    idsChanged_ = true;
    return next;
}

int ModRegistry::tileFor(const std::string& path) {
    for (auto& [tile, p] : textures_)
        if (p == path) return tile;
    if (nextTile_ >= kAtlasCols * kAtlasCols) return T_STONE;  // the atlas is full
    textures_.emplace_back(nextTile_, path);
    return nextTile_++;
}

void ModRegistry::load(const std::vector<std::string>& packDirs, const std::string& idFile, std::vector<std::string>* warnings) {
    clear();
    ids_.clear();
    idsChanged_ = false;
    {
        std::ifstream in(idFile);
        std::string key;
        int id;
        while (in >> key >> id)
            if (id >= kModIdBase && id < kItemIdLimit) ids_.emplace_back(key, (ItemId)id);
    }
    auto warn = [&](const std::string& w) {
        if (warnings) warnings->push_back(w);
    };
    for (const std::string& dir : packDirs) {
        std::ifstream in(dir + "/content.json");
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        Json doc;
        std::string err;
        if (!Json::parse(ss.str(), doc, err)) {
            warn(fs::path(dir).filename().string() + "/content.json: " + err);
            continue;
        }
        const std::string ns = doc["namespace"].str(fs::path(dir).filename().string());
        if (!validPart(ns)) {
            warn(trf("Neplatný namespace módu: {0}", {ns}));
            continue;
        }
        Summary sum;
        const std::string tex = dir + "/textures/mod/";
        auto texture = [&](const Json& j, int fallback) {
            const std::string file = j.str("");
            if (file.empty() || file.find("..") != std::string::npos) return fallback;
            if (!fs::exists(tex + file)) {
                warn(trf("Chybí textura {0}", {ns + "/" + file}));
                return fallback;
            }
            return tileFor(tex + file);
        };
        const Json& blocks = doc["blocks"];
        for (size_t i = 0; i < blocks.size(); ++i) {
            const Json& b = blocks[i];
            const std::string id = b["id"].str("");
            if (!validPart(id)) continue;
            ModBlock m;
            m.key = ns + ":" + id;
            m.name = b["name"].str(id);
            m.nameCs = b["name_cs"].str("");
            const int all = texture(b["texture"], T_STONE);
            for (int& t : m.tiles) t = all;
            if (b["textures"].isObject()) {
                const Json& t = b["textures"];
                const int side = texture(t["side"], all);
                for (int f : {F_NORTH, F_SOUTH, F_EAST, F_WEST}) m.tiles[f] = side;
                m.tiles[F_TOP] = texture(t["top"], side);
                m.tiles[F_BOTTOM] = texture(t["bottom"], m.tiles[F_TOP]);
            }
            m.hardness = std::clamp((float)b["hardness"].num(1.5), 0.0f, 50.0f);
            m.tool = b["tool"].str("");
            m.transparent = b["transparent"].boolean(false);
            m.light = b["light"].boolean(false);
            m.drop = b["drop"].str("");
            const ItemId at = idFor(m.key);
            if (at == AIR) continue;
            entries_.push_back({at, true, blocks_.size()});
            blocks_.push_back(std::move(m));
            creative_.push_back(at);
            ++sum.blocks;
        }
        const Json& items = doc["items"];
        for (size_t i = 0; i < items.size(); ++i) {
            const Json& it = items[i];
            const std::string id = it["id"].str("");
            if (!validPart(id)) continue;
            ModItem m;
            m.key = ns + ":" + id;
            m.name = it["name"].str(id);
            m.nameCs = it["name_cs"].str("");
            m.tile = texture(it["texture"], T_STICK);
            m.food = std::clamp((int)it["food"].num(0), 0, 20);
            m.stack = std::clamp((int)it["stack"].num(64), 1, 64);
            const ItemId at = idFor(m.key);
            if (at == AIR) continue;
            entries_.push_back({at, false, items_.size()});
            items_.push_back(std::move(m));
            creative_.push_back(at);
            ++sum.items;
        }
        const Json& mobs = doc["mobs"];
        for (size_t i = 0; i < mobs.size() && mobs_.size() < 200; ++i) {
            const Json& mb = mobs[i];
            const std::string id = mb["id"].str("");
            if (!validPart(id)) continue;
            ModMob m;
            m.key = ns + ":" + id;
            m.name = mb["name"].str(id);
            m.nameCs = mb["name_cs"].str("");
            m.base = baseMob(mb["base"].str("pig"));
            m.skin = texture(mb["texture"], -1);
            m.face = texture(mb["face"], -1);
            m.health = std::clamp((float)mb["health"].num(mobInfo(m.base).health), 1.0f, 500.0f);
            m.spawn = std::clamp((float)mb["spawn"].num(0.1), 0.0f, 1.0f);
            const Json& drops = mb["drops"];
            for (size_t k = 0; k < drops.size(); ++k)
                m.drops.emplace_back(drops[k]["item"].str(""), std::clamp((int)drops[k]["count"].num(1), 1, 64));
            mobs_.push_back(std::move(m));
            ++sum.mobs;
        }
        summaries_.emplace_back(dir, sum);
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.id < b.id; });
    if (idsChanged_) {
        std::ofstream out(idFile);
        for (auto& [k, id] : ids_) out << k << " " << id << "\n";
    }
}

const ModBlock* ModRegistry::block(ItemId id) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(), id, [](const Entry& e, ItemId v) { return e.id < v; });
    return it != entries_.end() && it->id == id && it->isBlock ? &blocks_[it->index] : nullptr;
}

const ModItem* ModRegistry::item(ItemId id) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(), id, [](const Entry& e, ItemId v) { return e.id < v; });
    return it != entries_.end() && it->id == id && !it->isBlock ? &items_[it->index] : nullptr;
}

const ModMob* ModRegistry::mob(int variant) const {
    return variant >= 1 && variant <= (int)mobs_.size() ? &mobs_[(size_t)variant - 1] : nullptr;
}

ItemId ModRegistry::find(const std::string& key) const {
    for (const Entry& e : entries_)
        if ((e.isBlock ? blocks_[e.index].key : items_[e.index].key) == key) return e.id;
    return AIR;
}

const char* ModRegistry::name(ItemId id) const {
    const bool cs = language() == Language::Czech;
    if (const ModBlock* b = block(id)) return (cs && !b->nameCs.empty() ? b->nameCs : b->name).c_str();
    if (const ModItem* i = item(id)) return (cs && !i->nameCs.empty() ? i->nameCs : i->name).c_str();
    return nullptr;
}

const char* ModRegistry::key(ItemId id) const {
    if (const ModBlock* b = block(id)) return b->key.c_str();
    if (const ModItem* i = item(id)) return i->key.c_str();
    return nullptr;
}

ModRegistry::Summary ModRegistry::summary(const std::string& packDir) const {
    for (auto& [dir, s] : summaries_)
        if (dir == packDir) return s;
    return {};
}

// The hooks Blocks.h declares, so its inline helpers know mod blocks.
bool modIsBlock(ItemId id) { return ModRegistry::get().block(id) != nullptr; }
int modTileFor(ItemId id, Face face) {
    if (const ModBlock* b = ModRegistry::get().block(id)) return b->tiles[face];
    if (const ModItem* i = ModRegistry::get().item(id)) return i->tile;
    return T_STONE;
}
bool modIsTransparent(ItemId id) {
    const ModBlock* b = ModRegistry::get().block(id);
    return b && b->transparent;
}
bool modIsEmissive(ItemId id) {
    const ModBlock* b = ModRegistry::get().block(id);
    return b && b->light;
}
int modFood(ItemId id) {
    const ModItem* i = ModRegistry::get().item(id);
    return i ? i->food : 0;
}

}  // namespace vc

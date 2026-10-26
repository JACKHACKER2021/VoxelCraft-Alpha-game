#include "Store.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "Blocks.h"
#include "Png.h"
#include "TexturePack.h"

namespace vc {

namespace fs = std::filesystem;

namespace {

StoreItem item(const char* id, StoreItem::Kind kind, const char* title, const char* banner, const char* tag,
               const char* description, int price, float rating, float r, float g, float b) {
    StoreItem it;
    it.id = id;
    it.kind = kind;
    it.title = title;
    it.banner = banner;
    it.tag = tag;
    it.description = description;
    it.price = price;
    it.rating = rating;
    it.color[0] = r;
    it.color[1] = g;
    it.color[2] = b;
    return it;
}

// ---- mod pack contents

struct PackFile {
    const char* path;
    const char* text;
};

const PackFile kOhnivyNether[] = {
    {"manifest.json", R"({"name": "Ohnivý Nether", "description": "Nether z cihel, vysoké lávové moře a hodně světlitu.", "version": "1.0.0"})"},
    {"dimensions/nether.json", R"({
  "lava_level": 48,
  "blocks": {"fill": "nether_brick", "floor": "netherrack", "ore": "glowstone"},
  "ore_chance": 0.02,
  "glowstone_clusters": 5,
  "fog_color": [0.45, 0.12, 0.02]
})"},
    {"recipes/extra.json", R"({
  "recipes": [{"type": "shaped", "pattern": ["GG", "GG"], "key": {"G": "glowstone"}, "result": "glowstone", "count": 4}],
  "smelting": [{"input": "gravel", "output": "flint"}]
})"},
};

const PackFile kSouostrovi[] = {
    {"manifest.json", R"({"name": "Souostroví", "description": "Vyšší moře, ze kterého vykukují zelené ostrovy s plážemi.", "version": "1.0.0"})"},
    {"dimensions/overworld.json", R"({"sea_level": 70, "biome_size": 220})"},
    {"biomes/plains.json", R"({
  "id": "plains", "name": "Ostrovy",
  "climate": {"temperature": 0.5, "humidity": 0.35},
  "terrain": {"height": 73, "variation": 6},
  "blocks": {"surface": "grass", "filler": "dirt", "filler_depth": 3, "beach": "sand", "underwater": "sand"},
  "trees": {"density": 0.02, "trunk": "log", "leaves": "leaves"}
})"},
    {"biomes/ocean.json", R"({
  "id": "ocean", "name": "Hluboký oceán",
  "climate": {"temperature": 0.55, "humidity": 0.7},
  "terrain": {"height": 48, "variation": 8},
  "blocks": {"surface": "sand", "filler": "sand", "filler_depth": 3, "beach": "sand", "underwater": "gravel"},
  "trees": {"density": 0}
})"},
};

const PackFile kVelkeHory[] = {
    {"manifest.json", R"({"name": "Velké hory", "description": "Obří štíty, hluboká údolí a lesy na svazích.", "version": "1.0.0"})"},
    {"dimensions/overworld.json", R"({"biome_size": 480})"},
    {"biomes/mountains.json", R"({
  "id": "mountains", "name": "Velehory",
  "climate": {"temperature": 0.2, "humidity": 0.5},
  "terrain": {"height": 98, "variation": 45},
  "blocks": {"surface": "grass", "filler": "dirt", "filler_depth": 2, "beach": "gravel", "underwater": "gravel"},
  "trees": {"density": 0.02, "trunk": "log", "leaves": "leaves"}
})"},
};

const PackFile kBohateDoly[] = {
    {"manifest.json", R"({"name": "Bohaté doly", "description": "Trojnásobek uhlí a železa a širší jeskyně.", "version": "1.0.0"})"},
    {"dimensions/overworld.json", R"({
  "caves": {"enabled": true, "size": 0.09},
  "ores": [
    {"block": "coal_ore", "veins": 45, "size": 10, "min_y": 5, "max_y": 110},
    {"block": "iron_ore", "veins": 30, "size": 8, "min_y": 5, "max_y": 70}
  ]
})"},
};

template <size_t N>
bool writePack(PackManager& packs, const std::string& id, const PackFile (&files)[N], std::string& error) {
    for (const PackFile& f : files)
        if (!packs.writeFile(id, f.path, std::string(f.text) + "\n")) {
            error = std::string("nelze zapsat ") + f.path;
            return false;
        }
    return true;
}

// ---- texture packs, recoloured from our own placeholder textures

using Recolor = void (*)(uint8_t* px);

void night(uint8_t* p) {
    p[0] = (uint8_t)(p[0] * 0.45f + 8);
    p[1] = (uint8_t)(p[1] * 0.55f + 12);
    p[2] = (uint8_t)std::min(255.0f, p[2] * 0.8f + 40);
}

void pastel(uint8_t* p) {
    for (int i = 0; i < 3; ++i) p[i] = (uint8_t)(p[i] * 0.6f + 255 * 0.4f);
}

void sepia(uint8_t* p) {
    float r = p[0], g = p[1], b = p[2];
    p[0] = (uint8_t)std::min(255.0f, r * 0.393f + g * 0.769f + b * 0.189f);
    p[1] = (uint8_t)std::min(255.0f, r * 0.349f + g * 0.686f + b * 0.168f);
    p[2] = (uint8_t)std::min(255.0f, r * 0.272f + g * 0.534f + b * 0.131f);
}

bool writeTexturePack(PackManager& packs, const std::string& id, const std::string& name, const std::string& description,
                      Recolor recolor, std::string& error) {
    if (!packs.writeFile(id, "manifest.json",
                         "{\"name\": \"" + name + "\", \"description\": \"" + description + "\", \"version\": \"1.0.0\"}\n")) {
        error = "nelze zapsat manifest.json";
        return false;
    }
    int size = 0;
    std::vector<uint8_t> atlas = buildFallbackAtlas(size);
    const int res = size / kAtlasCols;
    for (int t = 0; t < T_COUNT; ++t) {
        std::vector<uint8_t> tile(res * res * 4);
        for (int y = 0; y < res; ++y)
            for (int x = 0; x < res; ++x) {
                const uint8_t* src = &atlas[(((t / kAtlasCols) * res + y) * size + (t % kAtlasCols) * res + x) * 4];
                uint8_t* dst = &tile[(y * res + x) * 4];
                std::copy(src, src + 4, dst);
                recolor(dst);
            }
        fs::path path = fs::path(packs.packDir(id)) / "textures" / tileFilePath(t);
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (!writePng(path.string(), res, res, tile)) {
            error = "nelze zapsat " + path.filename().string();
            return false;
        }
    }
    return true;
}

}  // namespace

const std::vector<StoreItem>& storeCatalog() {
    using K = StoreItem::Kind;
    static const std::vector<StoreItem> items = {
        item("skin:knight", K::Skin, "Rytíř", "RYTÍŘ", "SKIN", "Plná zbroj s červeným erbem. Pro hrdiny jeskyní.", 300, 4.8f, 0.36f, 0.38f, 0.42f),
        item("skin:robot", K::Skin, "Robot V-01", "ROBOT", "SKIN", "Kovový pomocník se svítícíma očima.", 450, 4.7f, 0.12f, 0.3f, 0.36f),
        item("skin:astronaut", K::Skin, "Astronautka", "ASTRONAUT", "SKIN", "Skafandr pro výpravy do neznáma.", 500, 4.9f, 0.13f, 0.2f, 0.38f),
        item("skin:nether_walker", K::Skin, "Poutník Netheru", "POUTNÍK", "SKIN", "Žhnoucí oči a plášť z popela.", 400, 4.6f, 0.42f, 0.1f, 0.06f),
        item("pack:ohnivy_nether", K::ModPack, "Ohnivý Nether", "OHNIVÝ NETHER", "MODPACK",
             "Nether z cihel, vysoké lávové moře a záplava světlitu.", 600, 4.9f, 0.4f, 0.11f, 0.08f),
        item("pack:souostrovi", K::ModPack, "Souostroví", "SOUOSTROVÍ", "SVĚT",
             "Vyšší moře a stovky ostrůvků s písečnými plážemi.", 500, 4.7f, 0.08f, 0.3f, 0.42f),
        item("pack:velke_hory", K::ModPack, "Velké hory", "VELKÉ HORY", "SVĚT", "Obří štíty a hluboká údolí.", 450, 4.8f, 0.24f, 0.3f, 0.2f),
        item("pack:bohate_doly", K::ModPack, "Bohaté doly", "BOHATÉ DOLY", "SVĚT", "Trojnásobek rud a širší jeskyně.", 400, 4.5f, 0.3f, 0.24f, 0.16f),
        item("textures:night", K::Textures, "Noční svět", "NOČNÍ SVĚT", "TEXTURY", "Tmavé modré textury, svět jako za úplňku.", 350, 4.6f, 0.07f, 0.09f, 0.2f),
        item("textures:pastel", K::Textures, "Pastelové kostky", "PASTEL", "TEXTURY", "Jemné světlé barvy.", 300, 4.4f, 0.55f, 0.45f, 0.55f),
        item("textures:sepia", K::Textures, "Retro sépie", "SÉPIE", "TEXTURY", "Svět jako stará fotografie.", 250, 4.5f, 0.4f, 0.3f, 0.18f),
        item("vip", K::Vip, "VIP", "VIP", "VIP",
             "Zlaté jméno, 2× mince z denních odměn, točení navíc každý den a dva VIP skiny.", 3000, 5.0f, 0.55f, 0.42f, 0.05f),
        item("addon:winter", K::Addon, "Zimní doplněk", "ZIMA", "DOPLNĚK",
             "Všude sníh a sněžení, vánoční textury, dárky a cukrové hůlky.", 850, 4.9f, 0.55f, 0.7f, 0.85f),
        item("addon:halloween", K::Addon, "Halloween", "HALLOWEEN", "DOPLNĚK",
             "Rudá obloha, hustá mlha, hordy zombie a jaderné TNT.", 950, 4.8f, 0.45f, 0.15f, 0.05f),
        item("addon:anniversary", K::Addon, "Výroční balíček", "1. NAROZENINY", "JEN S KÓDEM",
             "První narozeniny VoxelCraftu: konfety, dort a slavnostní menu. Kód vyhraješ v kole štěstí.", 0, 5.0f, 0.6f, 0.2f, 0.45f),
    };
    return items;
}

const char* addonName(const std::string& id) {
    if (id == "addon:winter") return "Zimní doplněk";
    if (id == "addon:halloween") return "Halloween";
    if (id == "addon:anniversary") return "Výroční balíček";
    return "";
}

bool installStoreItem(const StoreItem& it, PackManager& packs, std::string& error) {
    if (it.kind == StoreItem::Kind::Skin || it.kind == StoreItem::Kind::Vip || it.kind == StoreItem::Kind::Addon) return true;
    std::string id = "store_" + it.id.substr(it.id.find(':') + 1);
    bool ok = true;
    if (it.id == "pack:ohnivy_nether") ok = writePack(packs, id, kOhnivyNether, error);
    else if (it.id == "pack:souostrovi") ok = writePack(packs, id, kSouostrovi, error);
    else if (it.id == "pack:velke_hory") ok = writePack(packs, id, kVelkeHory, error);
    else if (it.id == "pack:bohate_doly") ok = writePack(packs, id, kBohateDoly, error);
    else if (it.id == "textures:night") ok = writeTexturePack(packs, id, it.title, it.description, night, error);
    else if (it.id == "textures:pastel") ok = writeTexturePack(packs, id, it.title, it.description, pastel, error);
    else if (it.id == "textures:sepia") ok = writeTexturePack(packs, id, it.title, it.description, sepia, error);
    else {
        error = "neznámá položka";
        return false;
    }
    if (ok) packs.setEnabled(id, true);
    return ok;
}

bool developerFree(const StoreItem& it, const Profile& profile) {
    return profile.isDeveloper() &&
           (it.kind == StoreItem::Kind::Skin || it.kind == StoreItem::Kind::ModPack || it.kind == StoreItem::Kind::Textures);
}

BuyResult buyStoreItem(const StoreItem& it, Profile& profile, PackManager& packs, std::string& error) {
    if (profile.owns(it.id)) return BuyResult::AlreadyOwned;
    // The developer check comes first, so a test never takes coins.
    const bool free = developerFree(it, profile);
    if (!free && profile.coins < it.price) return BuyResult::NotEnoughCoins;
    if (!installStoreItem(it, packs, error)) return BuyResult::Failed;
    if (!free) profile.coins -= it.price;
    profile.owned.insert(it.id);
    return BuyResult::Bought;
}

}  // namespace vc

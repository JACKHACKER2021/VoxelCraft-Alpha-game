// 2.7.0: writes the game's encyclopedia as Markdown (docs/ENCYKLOPEDIE.md in Czech, docs/ENCYCLOPEDIA.md in
// English) from the game's own data: the in-game encyclopedia with its pictures, every creature, every block
// and item of the creative tabs with its icon and recipes, the achievements and the commands.
// Build: g++ -std=c++17 -Iapp/src/main/cpp tools/encyclopedia_doc.cpp <engine objects> -lz -o enc_doc
// Run:   ./enc_doc <repo root>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <zlib.h>

#include "engine/Achievements.h"
#include "engine/Blocks.h"
#include "engine/Commands.h"
#include "engine/Crafting.h"
#include "engine/Items.h"
#include "engine/Lang.h"
#include "engine/Mobs.h"
#include "engine/Png.h"
#include "engine/TexturePack.h"

using namespace vc;
namespace fs = std::filesystem;

#include "engine/Encyclopedia.inc"
#include "engine/EncyclopediaPictures.inc"

namespace {

std::vector<uint8_t> atlas;
int atlasSize = 0;

// The icon of an item: its tile, 16 x 16, scaled to 32 x 32.
void writeIcon(const fs::path& file, ItemId id) {
    // The tile as it is, 16 x 16 (the page shows it at 24 pixels).
    const int tile = tileFor(id, F_SOUTH), t = atlasSize / kAtlasCols;
    std::vector<uint8_t> out((size_t)t * t * 4);
    for (int y = 0; y < t; ++y)
        std::copy_n(&atlas[((size_t)((tile / kAtlasCols) * t + y) * atlasSize + (tile % kAtlasCols) * t) * 4], t * 4, &out[(size_t)y * t * 4]);
    writePng(file.string(), t, t, out);
}

std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) o += c == '|' ? std::string("\\|") : std::string(1, c);
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) return 1;
    const fs::path root = argv[1], img = root / "docs/img/enc";
    fs::create_directories(img / "icons");
    atlas = buildFallbackAtlas(atlasSize);

    // The pictures of the in-game encyclopedia.
    for (const EncPicture& p : kPictures) {
        std::vector<uint8_t> rgb((size_t)kPicW * kPicH * 3);
        uLongf len = rgb.size();
        if (uncompress(rgb.data(), &len, p.data, p.size) != Z_OK) continue;
        std::vector<uint8_t> rgba((size_t)kPicW * kPicH * 4);
        for (int i = 0; i < kPicW * kPicH; ++i) rgba[i * 4] = rgb[i * 3], rgba[i * 4 + 1] = rgb[i * 3 + 1], rgba[i * 4 + 2] = rgb[i * 3 + 2], rgba[i * 4 + 3] = 255;
        writePng((img / (std::string(p.id) + ".png")).string(), kPicW, kPicH, rgba);
    }

    // What makes each item: crafting (shaped / shapeless) and the furnace.
    const RecipeBook& book = RecipeBook::defaults();
    std::set<ItemId> iconed;
    auto icon = [&](ItemId id) {
        if (!iconed.count(id)) writeIcon(img / "icons" / (std::to_string(id) + ".png"), id), iconed.insert(id);
        return "<img src=\"img/enc/icons/" + std::to_string(id) + ".png\" width=\"24\">";
    };

    for (int lang = 0; lang < 2; ++lang) {
        setLanguage(lang == 0 ? Language::Czech : Language::English);
        const bool cz = lang == 0;
        std::ostringstream o;
        o << (cz ? "## 📖 Encyklopedie hry (2.7.1)\n\n" : "## 📖 The game encyclopedia (2.7.1)\n\n");
        o << (cz ? "Celá hra na jednom místě: dimenze, biomy, stavby, mechaniky, tvorové, všechny bloky a předměty s recepty, úspěchy a příkazy. "
                   "Vygenerováno přímo z dat hry (`tools/encyclopedia_doc.cpp`), takže sedí s tím, co ve hře je.\n\n"
                 : "The whole game in one place: dimensions, biomes, structures, mechanics, creatures, every block and item with its recipes, "
                   "achievements and commands. Generated from the game's own data (`tools/encyclopedia_doc.cpp`), so it matches the game.\n\n");
        // Contents.
        o << (cz ? "**Obsah:**\n\n" : "**Contents:**\n\n");
        for (const char* c : kEncCategories) o << "- " << tr(c) << "\n";
        o << "- " << (cz ? "Recepty a suroviny" : "Recipes and materials") << "\n- " << (cz ? "Úspěchy" : "Achievements") << "\n- " << (cz ? "Příkazy" : "Commands") << "\n\n";

        // The written entries, by category, with their pictures.
        for (const char* cat : kEncCategories) {
            const std::string c = cat;
            if (c == "Bloky a předměty" || c == "Moby") continue;
            o << "### " << tr(cat) << "\n\n";
            for (const EncEntry& e : kEncyclopedia) {
                if (c != e.category) continue;
                o << "#### " << icon(e.icon) << " " << tr(e.title) << "\n\n";
                if (*e.picture) o << "<img src=\"img/enc/" << e.picture << ".png\" width=\"384\">\n\n";
                o << tr(e.text) << "\n\n";
            }
        }

        // Creatures.
        o << "### " << tr("Moby") << "\n\n";
        o << (cz ? "| | Tvor | Životy | Útok | Druh |\n|---|---|---|---|---|\n" : "| | Creature | Health | Attack | Kind |\n|---|---|---|---|---|\n");
        static const ItemId kEggOf[(int)MobType::Count] = {
            EGG_PIG, EGG_SHEEP, EGG_SLIME, EGG_FORGOTTEN, CRIMSON_HEAD, EGG_CHICKEN, EGG_COW, EGG_VILLAGER, EGG_SPIDER, EGG_BAT, EGG_TNT_ZOMBIE,
            EGG_TAR_MONSTER, EGG_HUSKING, EGG_HOGMAN, EGG_CELESTLING, EGG_REACH_STALKER, EGG_SKY_DRIFTER, CELESTIAL_WINGS, EGG_GOLEM, EGG_LURKER,
            EGG_SPORELING, EGG_MOTH, EGG_ASH_FOX, EGG_FOX, EGG_GHOST, EGG_SNOWMAN};
        for (int t = 0; t < (int)MobType::Count; ++t) {
            const MobInfo& m = mobInfo((MobType)t);
            const std::string kind = m.boss ? (cz ? "boss" : "boss") : m.hostile ? (cz ? "nepřátelský" : "hostile") : (cz ? "mírumilovný" : "peaceful");
            o << "| " << icon(kEggOf[t]) << " | **" << tr(m.name) << "** | " << (int)m.health << " | " << (int)m.damage << " | " << kind << " |\n";
        }
        o << "\n";

        // Every block and item, by creative tab, with recipes.
        o << "### " << tr("Bloky a předměty") << "\n\n";
        for (int cat = 0; cat < CatCount; ++cat) {
            const auto& items = creativeCategoryItems(cat);
            if (items.empty()) continue;
            o << "#### " << icon(creativeCategoryIcon(cat)) << " " << tr(creativeCategoryName(cat)) << " (" << items.size() << ")\n\n<details>\n<summary>"
              << (cz ? "Rozbalit" : "Open") << "</summary>\n\n";
            o << (cz ? "| | Název | Jak ho získat |\n|---|---|---|\n" : "| | Name | How to get it |\n|---|---|---|\n");
            for (ItemId id : items) {
                std::vector<std::string> ways;
                for (const ShapedRecipe& r : book.shaped) {
                    if (r.result.id != id) continue;
                    std::map<ItemId, int> n;
                    for (ItemId c : r.cells)
                        if (c != AIR) ++n[c];
                    std::string w;
                    for (const auto& [k, v] : n) w += (w.empty() ? "" : " + ") + std::to_string(v) + "× " + tr(itemName(k));
                    ways.push_back((cz ? "výroba: " : "crafting: ") + w + (r.result.count > 1 ? " → " + std::to_string(r.result.count) + "×" : ""));
                    if (ways.size() >= 2) break;
                }
                for (const ShapelessRecipe& r : book.shapeless) {
                    if (r.result.id != id || ways.size() >= 3) continue;
                    std::string w;
                    for (ItemId c : r.ingredients) w += (w.empty() ? "" : " + ") + tr(itemName(c));
                    ways.push_back((cz ? "výroba: " : "crafting: ") + w + (r.result.count > 1 ? " → " + std::to_string(r.result.count) + "×" : ""));
                }
                for (const auto& [in, out] : book.smelting)
                    if (out == id && ways.size() < 4) ways.push_back((cz ? "pec: " : "furnace: ") + std::string(tr(itemName(in))));
                std::string how;
                for (const std::string& w : ways) how += (how.empty() ? "" : "<br>") + esc(w);
                if (how.empty()) how = cz ? "těžba, nález nebo kreativ" : "mining, found or creative";
                o << "| " << icon(id) << " | " << esc(tr(itemName(id))) << " | " << how << " |\n";
            }
            o << "\n</details>\n\n";
        }

        // Achievements and commands.
        o << "### " << (cz ? "Úspěchy" : "Achievements") << "\n\n" << (cz ? "| Úspěch | Jak ho získat | Voxel Bucks |\n|---|---|---|\n" : "| Achievement | How to get it | Voxel Bucks |\n|---|---|---|\n");
        for (int i = 0; i < kAchievementCount; ++i)
            o << "| **" << esc(tr(kAchievements[i].name)) << "** | " << esc(tr(kAchievements[i].description)) << " | " << kAchievements[i].coins << " |\n";
        o << "\n### " << (cz ? "Příkazy" : "Commands") << "\n\n" << (cz ? "| Příkaz | Co dělá | Jen s povolenými příkazy |\n|---|---|---|\n" : "| Command | What it does | Cheats only |\n|---|---|---|\n");
        for (int i = 0; i < kCommandCount; ++i)
            o << "| `" << esc(kCommands[i].usage) << "` | " << esc(tr(kCommands[i].description)) << " | " << (kCommands[i].cheat ? "✓" : "") << " |\n";
        o << "\n";
        std::ofstream(argv[2] + std::string(cz ? "/enc_cs.md" : "/enc_en.md")) << o.str();
    }
    std::printf("icons %zu, pictures %zu\n", iconed.size(), sizeof(kPictures) / sizeof(kPictures[0]));
    return 0;
}

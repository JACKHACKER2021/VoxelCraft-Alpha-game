#include "JavaImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "Blocks.h"
#include "Items.h"
#include "Json.h"
#include "TexturePack.h"

namespace vc {

namespace fs = std::filesystem;

namespace {

std::string readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool readJson(const fs::path& p, Json& out) {
    std::string err;
    return fs::is_regular_file(p) && Json::parse(readText(p), out, err);
}

// "ruby_bricks" -> "Ruby Bricks"
std::string pretty(const std::string& id) {
    std::string out;
    bool up = true;
    for (char c : id) {
        if (c == '_') {
            out += ' ';
            up = true;
        } else {
            out += up ? (char)std::toupper((unsigned char)c) : c;
            up = false;
        }
    }
    return out;
}

// "mymod:block/ruby" -> ("mymod", "block/ruby"); a missing namespace means minecraft.
std::pair<std::string, std::string> splitRef(const std::string& ref) {
    const size_t colon = ref.find(':');
    if (colon == std::string::npos) return {"minecraft", ref};
    return {ref.substr(0, colon), ref.substr(colon + 1)};
}

std::string modName(const fs::path& dir) {
    Json j;
    if (readJson(dir / "fabric.mod.json", j) || readJson(dir / "quilt.mod.json", j)) {
        if (j["name"].isString()) return j["name"].str();
        if (j["quilt_loader"]["metadata"]["name"].isString()) return j["quilt_loader"]["metadata"]["name"].str();
    }
    // Forge / NeoForge: displayName="..." in mods.toml
    for (const char* toml : {"META-INF/mods.toml", "META-INF/neoforge.mods.toml"}) {
        std::istringstream in(readText(dir / toml));
        std::string line;
        while (std::getline(in, line)) {
            const size_t k = line.find("displayName");
            const size_t q1 = line.find('"', k), q2 = q1 == std::string::npos ? q1 : line.find('"', q1 + 1);
            if (k != std::string::npos && q2 != std::string::npos) return line.substr(q1 + 1, q2 - q1 - 1);
        }
    }
    if (readJson(dir / "pack.mcmeta", j) && j["pack"]["description"].isString()) return j["pack"]["description"].str();
    return "";
}

}  // namespace

JavaImportReport convertJavaMod(const std::string& javaDir, const std::string& packDir) {
    JavaImportReport rep;
    const fs::path root(javaDir), pack(packDir);
    std::error_code ec;
    fs::create_directories(pack / "textures" / "mod", ec);
    fs::create_directories(pack / "recipes", ec);
    rep.name = modName(root);
    {
        std::ifstream in(root / "classes.txt");
        in >> rep.classes;
    }
    // Minecraft's own files are never used (they are Mojang's): only counted.
    for (const char* top : {"assets", "data"}) {
        const fs::path vanilla = root / top / "minecraft";
        if (fs::is_directory(vanilla))
            for (auto& e : fs::recursive_directory_iterator(vanilla, ec))
                if (e.is_regular_file()) ++rep.skippedMinecraft;
    }

    // Namespaces of the mod (everything but minecraft) in assets/ and data/.
    std::set<std::string> spaces;
    for (const char* top : {"assets", "data"})
        if (fs::is_directory(root / top))
            for (auto& e : fs::directory_iterator(root / top, ec))
                if (e.is_directory() && e.path().filename() != "minecraft") spaces.insert(e.path().filename().string());

    Json blocks = Json::array(), items = Json::array();
    std::set<std::string> known;  // "ns:id" of everything converted
    std::map<std::string, size_t> blockIndex;
    std::string mainNs;

    for (const std::string& ns : spaces) {
        if (mainNs.empty()) mainNs = ns;
        const fs::path assets = root / "assets" / ns;
        Json en, cs;
        readJson(assets / "lang" / "en_us.json", en);
        readJson(assets / "lang" / "cs_cz.json", cs);
        auto nameOf = [&](const std::string& kind, const std::string& id, Json& target) {
            const std::string key = kind + "." + ns + "." + id;
            target.set("name", Json::string(en[key].str(pretty(id))));
            if (cs[key].isString()) target.set("name_cs", Json::string(cs[key].str()));
            if (en[key].isString()) ++rep.names;
        };
        // A texture reference of a model -> a file in the pack, copied once.
        std::map<std::string, std::string> copied;
        auto texture = [&](const std::string& ref) -> std::string {
            auto [tns, path] = splitRef(ref);
            if (tns == "minecraft" || tns != ns) return "";  // Minecraft's textures are not ours to use
            if (auto it = copied.find(path); it != copied.end()) return it->second;
            const fs::path src = assets / "textures" / (path + ".png");
            int w = 0, h = 0;
            if (!imageSize(src.string(), w, h) || w < 8 || w > 512 || h < w) return "";
            std::string flat = ns + "_" + path;
            std::replace(flat.begin(), flat.end(), '/', '_');
            flat += ".png";
            fs::copy_file(src, pack / "textures" / "mod" / flat, fs::copy_options::overwrite_existing, ec);
            if (ec) return "";
            ++rep.textures;
            copied[path] = flat;
            return flat;
        };
        // A block model: a cube with one texture, a column (ends and side) or bottom/top/side.
        auto cubeOf = [&](const std::string& modelRef, Json& out) -> bool {
            auto [mns, mpath] = splitRef(modelRef);
            Json model;
            if (mns != ns || !readJson(assets / "models" / (mpath + ".json"), model)) return false;
            std::string parent = splitRef(model["parent"].str("")).second;
            const Json& t = model["textures"];
            if (parent == "block/cube_all" || parent == "block/leaves") {
                const std::string all = texture(t["all"].str(""));
                if (all.empty()) return false;
                out.set("texture", Json::string(all));
                if (parent == "block/leaves") out.set("transparent", Json::fromBool(true));
                return true;
            }
            if (parent == "block/cube_column" || parent == "block/cube_column_horizontal" || parent == "block/cube_bottom_top") {
                const bool column = parent != "block/cube_bottom_top";
                const std::string side = texture(t["side"].str("")), top = texture(t[column ? "end" : "top"].str(""));
                const std::string bottom = column ? top : texture(t["bottom"].str(""));
                if (side.empty() || top.empty() || bottom.empty()) return false;
                Json tex = Json::object();
                tex.set("side", Json::string(side));
                tex.set("top", Json::string(top));
                tex.set("bottom", Json::string(bottom));
                out.set("texture", Json::string(side));
                out.set("textures", tex);
                return true;
            }
            return false;
        };

        // Blocks: from the blockstates (first variant), else every block model.
        std::vector<std::pair<std::string, std::string>> blockModels;  // id, model ref
        if (fs::is_directory(assets / "blockstates")) {
            for (auto& e : fs::directory_iterator(assets / "blockstates", ec)) {
                Json bs;
                if (e.path().extension() != ".json" || !readJson(e.path(), bs)) continue;
                const std::string id = e.path().stem().string();
                std::string model;
                if (bs["variants"].isObject() && bs["variants"].size() > 0) {
                    const Json& v = bs["variants"][bs["variants"].keys()[0]];
                    model = v.isArray() ? v[0]["model"].str("") : v["model"].str("");
                }
                if (model.empty()) {
                    ++rep.skippedShapes;  // multipart blocks (fences, walls, pipes)
                    rep.notes.push_back(ns + ":" + id);
                    continue;
                }
                blockModels.emplace_back(id, model.find(':') == std::string::npos ? ns + ":" + model : model);
            }
        } else if (fs::is_directory(assets / "models" / "block")) {
            for (auto& e : fs::directory_iterator(assets / "models" / "block", ec))
                if (e.path().extension() == ".json") blockModels.emplace_back(e.path().stem().string(), ns + ":block/" + e.path().stem().string());
        }
        std::sort(blockModels.begin(), blockModels.end());
        for (auto& [id, model] : blockModels) {
            Json b = Json::object();
            b.set("id", Json::string(id));
            if (!cubeOf(model, b)) {
                ++rep.skippedShapes;
                rep.notes.push_back(ns + ":" + id);
                continue;
            }
            nameOf("block", id, b);
            b.set("hardness", Json::number(1.5));
            blockIndex[ns + ":" + id] = blocks.size();
            blocks.push(b);
            known.insert(ns + ":" + id);
            ++rep.blocks;
        }
        // Items: flat item models (generated / handheld) with a texture of their own.
        if (fs::is_directory(assets / "models" / "item")) {
            std::vector<fs::path> files;
            for (auto& e : fs::directory_iterator(assets / "models" / "item", ec))
                if (e.path().extension() == ".json") files.push_back(e.path());
            std::sort(files.begin(), files.end());
            for (const fs::path& f : files) {
                const std::string id = f.stem().string();
                if (known.count(ns + ":" + id)) continue;  // the item of a block
                Json model;
                if (!readJson(f, model)) continue;
                const std::string parent = splitRef(model["parent"].str("")).second;
                if (parent != "item/generated" && parent != "item/handheld") continue;
                const std::string tex = texture(model["textures"]["layer0"].str(""));
                if (tex.empty()) continue;
                Json it = Json::object();
                it.set("id", Json::string(id));
                it.set("texture", Json::string(tex));
                nameOf("item", id, it);
                if (parent == "item/handheld") it.set("stack", Json::number(1));
                items.push(it);
                known.insert(ns + ":" + id);
                ++rep.items;
            }
        }
    }

    // Recipes and block drops from data/<namespace>/.
    auto itemKey = [&](const std::string& ref) -> std::string {
        const std::string full = ref.find(':') == std::string::npos ? "minecraft:" + ref : ref;
        if (known.count(full)) return full;
        if (full.rfind("minecraft:", 0) == 0 && itemFromKey(full) != AIR) return full.substr(10);
        return "";
    };
    // An ingredient: an item ("ns:id" or {"item": ...}); of alternatives the first one; tags are not supported.
    auto ingredient = [&](const Json& j) -> std::string {
        const Json& one = j.isArray() ? j[0] : j;
        if (one.isString()) return one.str().empty() || one.str()[0] == '#' ? "" : itemKey(one.str());
        if (one["item"].isString()) return itemKey(one["item"].str());
        return "";
    };
    Json recipes = Json::array(), smelting = Json::array();
    for (const std::string& ns : spaces) {
        for (const char* dirName : {"recipes", "recipe"}) {
            const fs::path dir = root / "data" / ns / dirName;
            if (!fs::is_directory(dir)) continue;
            std::vector<fs::path> files;
            for (auto& e : fs::recursive_directory_iterator(dir, ec))
                if (e.path().extension() == ".json") files.push_back(e.path());
            std::sort(files.begin(), files.end());
            for (const fs::path& f : files) {
                Json r;
                if (!readJson(f, r)) continue;
                const std::string type = splitRef(r["type"].str("")).second;
                const Json& res = r["result"];
                const std::string out = itemKey(res.isString() ? res.str() : res["item"].isString() ? res["item"].str() : res["id"].str(""));
                const int count = std::clamp((int)res["count"].num(1), 1, 64);
                bool ok = !out.empty();
                Json v = Json::object();
                if (ok && type == "crafting_shaped") {
                    Json pattern = Json::array(), key = Json::object();
                    for (size_t i = 0; i < r["pattern"].size(); ++i) {
                        std::string row = r["pattern"][i].str("");
                        if (row.find('.') != std::string::npos) ok = false;
                        std::replace(row.begin(), row.end(), ' ', '.');
                        pattern.push(Json::string(row));
                    }
                    for (const std::string& k : r["key"].keys()) {
                        const std::string in = ingredient(r["key"][k]);
                        if (in.empty()) ok = false;
                        key.set(k, Json::string(in));
                    }
                    v.set("type", Json::string("shaped"));
                    v.set("pattern", pattern);
                    v.set("key", key);
                } else if (ok && type == "crafting_shapeless") {
                    Json list = Json::array();
                    for (size_t i = 0; i < r["ingredients"].size(); ++i) {
                        const std::string in = ingredient(r["ingredients"][i]);
                        if (in.empty()) ok = false;
                        list.push(Json::string(in));
                    }
                    v.set("type", Json::string("shapeless"));
                    v.set("ingredients", list);
                } else if (ok && (type == "smelting" || type == "blasting" || type == "smoking" || type == "campfire_cooking")) {
                    const std::string in = ingredient(r["ingredient"]);
                    if (in.empty()) {
                        ++rep.skippedRecipes;
                        continue;
                    }
                    Json s = Json::object();
                    s.set("input", Json::string(in));
                    s.set("output", Json::string(out));
                    smelting.push(s);
                    ++rep.recipes;
                    continue;
                } else {
                    ok = false;
                }
                if (!ok) {
                    ++rep.skippedRecipes;
                    continue;
                }
                v.set("result", Json::string(out));
                if (count > 1) v.set("count", Json::number(count));
                recipes.push(v);
                ++rep.recipes;
            }
        }
        // Loot tables of blocks: what breaking the block gives, when it is a single other item.
        for (const char* dirName : {"loot_tables", "loot_table"}) {
            const fs::path dir = root / "data" / ns / dirName / "blocks";
            if (!fs::is_directory(dir)) continue;
            for (auto& e : fs::directory_iterator(dir, ec)) {
                Json t;
                if (e.path().extension() != ".json" || !readJson(e.path(), t)) continue;
                const std::string block = ns + ":" + e.path().stem().string();
                auto it = blockIndex.find(block);
                if (it == blockIndex.end()) continue;
                const std::string drop = itemKey(t["pools"][0]["entries"][0]["name"].str(""));
                if (drop.empty() || drop == block) continue;
                // Json has no setter by index: rebuild the block with the drop.
                Json b = blocks[it->second];
                b.set("drop", Json::string(drop));
                Json again = Json::array();
                for (size_t i = 0; i < blocks.size(); ++i) again.push(i == it->second ? b : blocks[i]);
                blocks = again;
                ++rep.drops;
            }
        }
    }

    if (mainNs.empty()) mainNs = "javamod";
    if (rep.name.empty()) rep.name = pretty(mainNs);
    Json content = Json::object();
    content.set("namespace", Json::string(mainNs));
    content.set("blocks", blocks);
    content.set("items", items);
    std::ofstream(pack / "content.json") << content.dump();
    if (recipes.size() || smelting.size()) {
        Json file = Json::object();
        file.set("recipes", recipes);
        file.set("smelting", smelting);
        std::ofstream(pack / "recipes" / ("java_" + mainNs + ".json")) << file.dump();
    }
    if (!fs::exists(pack / "manifest.json")) {
        Json m = Json::object();
        m.set("name", Json::string(rep.name));
        m.set("description", Json::string("Java Edition"));
        m.set("version", Json::string("1.0"));
        std::ofstream(pack / "manifest.json") << m.dump();
    }
    std::ofstream out(pack / "import_report.txt");
    out << "name=" << rep.name << "\nblocks=" << rep.blocks << "\nitems=" << rep.items << "\nrecipes=" << rep.recipes
        << "\ntextures=" << rep.textures << "\ndrops=" << rep.drops << "\nnames=" << rep.names
        << "\nskipped_minecraft=" << rep.skippedMinecraft << "\nskipped_shapes=" << rep.skippedShapes
        << "\nskipped_recipes=" << rep.skippedRecipes << "\nclasses=" << rep.classes << "\n";
    for (size_t i = 0; i < rep.notes.size() && i < 50; ++i) out << "shape=" << rep.notes[i] << "\n";
    return rep;
}

std::vector<std::string> readImportReport(const std::string& packDir) {
    std::vector<std::string> lines;
    std::ifstream in(packDir + "/import_report.txt");
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

}  // namespace vc

#include "BedrockImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "Json.h"

namespace vc {

namespace fs = std::filesystem;

int BedrockImportReport::errors() const {
    return (int)std::count_if(lines.begin(), lines.end(), [](const ImportLine& l) { return l.status == 'x'; });
}
int BedrockImportReport::warnings() const {
    return (int)std::count_if(lines.begin(), lines.end(), [](const ImportLine& l) { return l.status == '!'; });
}

namespace {

std::string readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Bedrock JSON may have // and /* */ comments; they go before parsing (not inside strings).
std::string stripComments(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool str = false;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (str) {
            out += c;
            if (c == '\\' && i + 1 < s.size()) out += s[++i];
            else if (c == '"') str = false;
            continue;
        }
        if (c == '"') str = true, out += c;
        else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') ++i;
            out += '\n';
        } else if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) ++i;
            ++i;
        } else {
            out += c;
        }
    }
    return out;
}

bool readJson(const fs::path& p, Json& out, std::string* error = nullptr) {
    std::string err;
    if (!fs::is_regular_file(p)) {
        if (error) *error = "soubor chybí";
        return false;
    }
    if (Json::parse(stripComments(readText(p)), out, err)) return true;
    if (error) *error = err;
    return false;
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// "zoo:zebra" -> "zebra"; ids are made safe for content.json.
std::string localId(const std::string& id) {
    std::string s = lower(id.substr(id.find(':') == std::string::npos ? 0 : id.find(':') + 1));
    for (char& c : s)
        if (!std::isalnum((unsigned char)c) && c != '_') c = '_';
    return s.substr(0, 40);
}

std::string pretty(const std::string& id) {
    std::string out;
    bool up = true;
    for (char c : localId(id)) {
        if (c == '_') out += ' ', up = true;
        else out += up ? (char)std::toupper((unsigned char)c) : c, up = false;
    }
    return out;
}

struct Pack {
    fs::path dir;
    bool behaviour = false, resources = false;
};

// The texture path of a short name in terrain_texture.json / item_texture.json ("textures": string,
// array, or {"path": ...}).
std::string texturePath(const Json& data, const std::string& name) {
    const Json& t = data[name]["textures"];
    if (t.isString()) return t.str();
    if (t.isArray() && t.size()) return t[0].isString() ? t[0].str() : t[0]["path"].str("");
    if (t.isObject()) return t["path"].str("");
    return "";
}

}  // namespace

BedrockImportReport convertBedrockAddon(const std::string& bedrockDir, const std::string& packDir) {
    BedrockImportReport rep;
    const fs::path root(bedrockDir), pack(packDir);
    std::error_code ec;
    fs::create_directories(pack / "textures" / "mod", ec);
    auto line = [&](char st, const std::string& file, const std::string& text, std::vector<std::string> args = {}) {
        rep.lines.push_back({st, file, text, std::move(args)});
    };

    // The packs inside: every folder with a manifest.json that lists modules.
    std::vector<Pack> packs;
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->path().filename() != "manifest.json") continue;
        Json m;
        std::string err;
        const std::string rel = fs::relative(it->path(), root, ec).generic_string();
        if (!readJson(it->path(), m, &err)) {
            line('x', rel, "manifest.json nejde přečíst: {0}", {err});
            continue;
        }
        Pack p;
        p.dir = it->path().parent_path();
        for (size_t i = 0; i < m["modules"].size(); ++i) {
            const std::string type = m["modules"][i]["type"].str("");
            p.behaviour |= type == "data" || type == "script";
            p.resources |= type == "resources";
        }
        if (rep.name.empty()) rep.name = m["header"]["name"].str("");
        if (!p.behaviour && !p.resources) {
            line('!', rel, "manifest bez modulů: beru jako resource pack");
            p.resources = true;
        }
        line('+', rel, p.behaviour ? "manifest.json > Behavior Voxel" : "manifest.json > Resource Voxel");
        packs.push_back(p);
    }
    if (packs.empty()) {
        // No manifest at all: try the folder itself as both packs.
        line('x', "manifest.json", "chybí: zkouším soubory i bez něj");
        packs.push_back({root, true, true});
    }
    if (rep.name.size() >= 2 && rep.name[0] == 'p' && rep.name.find('.') != std::string::npos) rep.name.clear();  // "pack.name" keys
    if (rep.name.empty()) rep.name = "Bedrock " + root.parent_path().filename().string();

    // Resource side: the texture names, the names in texts/*.lang, the client entities.
    Json terrain = Json::object(), itemTex = Json::object();
    std::map<std::string, Json> blocksJson;  // RP blocks.json entries by id
    std::map<std::string, std::string> names, namesCs;
    struct ClientEntity { std::string texture, geometry; fs::path rp; };
    std::map<std::string, ClientEntity> clients;
    std::map<std::string, Json> geometries;  // geometry identifier -> its description and bones
    for (const Pack& p : packs) {
        if (!p.resources) continue;
        Json j;
        if (readJson(p.dir / "textures" / "terrain_texture.json", j)) {
            for (const std::string& k : j["texture_data"].keys()) {
                Json e = Json::object();
                e.set("path", Json::string(texturePath(j["texture_data"], k)));
                e.set("rp", Json::string(p.dir.string()));
                terrain.set(k, e);
            }
            line('+', "textures/terrain_texture.json", "> atlas bloků ({0})", {std::to_string(j["texture_data"].size())});
        }
        if (readJson(p.dir / "textures" / "item_texture.json", j)) {
            for (const std::string& k : j["texture_data"].keys()) {
                Json e = Json::object();
                e.set("path", Json::string(texturePath(j["texture_data"], k)));
                e.set("rp", Json::string(p.dir.string()));
                itemTex.set(k, e);
            }
            line('+', "textures/item_texture.json", "> atlas předmětů ({0})", {std::to_string(j["texture_data"].size())});
        }
        if (readJson(p.dir / "blocks.json", j))
            for (const std::string& k : j.keys()) blocksJson[k] = j[k];
        for (const char* lang : {"en_US.lang", "cs_CZ.lang"}) {
            std::ifstream in(p.dir / "texts" / lang);
            std::string l;
            while (std::getline(in, l)) {
                const size_t eq = l.find('=');
                if (eq == std::string::npos || l.rfind("##", 0) == 0) continue;
                std::string v = l.substr(eq + 1);
                if (const size_t tab = v.find('\t'); tab != std::string::npos) v = v.substr(0, tab);
                while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
                (std::string(lang) == "cs_CZ.lang" ? namesCs : names)[l.substr(0, eq)] = v;
            }
        }
        for (const char* dirName : {"entity", "entities"}) {
            for (auto& e : fs::directory_iterator(p.dir / dirName, ec)) {
                if (e.path().extension() != ".json" || !readJson(e.path(), j)) continue;
                const Json& d = j["minecraft:client_entity"]["description"];
                const std::string id = d["identifier"].str("");
                if (id.empty()) continue;
                ClientEntity c;
                c.texture = d["textures"]["default"].str(d["textures"].size() ? d["textures"][d["textures"].keys()[0]].str("") : "");
                c.geometry = d["geometry"]["default"].str(d["geometry"].size() ? d["geometry"][d["geometry"].keys()[0]].str("") : "");
                c.rp = p.dir;
                clients[id] = c;
            }
        }
        for (auto it = fs::recursive_directory_iterator(p.dir / "models", ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (it->path().string().find(".geo.json") == std::string::npos && it->path().extension() != ".json") continue;
            std::string err;
            if (!readJson(it->path(), j, &err)) {
                line('x', fs::relative(it->path(), root, ec).generic_string(), "model nejde přečíst: {0}", {err});
                continue;
            }
            const Json& list = j["minecraft:geometry"];
            for (size_t i = 0; i < list.size(); ++i) geometries[list[i]["description"]["identifier"].str("")] = list[i];
            for (const std::string& k : j.keys())  // the old format: "geometry.name": {...}
                if (k.rfind("geometry.", 0) == 0) geometries[k.substr(0, k.find(':'))] = j[k];
        }
    }

    // Copies one texture (png; a tga is noted) into textures/mod and returns its name there.
    std::set<std::string> copied;
    auto copyTexture = [&](const fs::path& rp, const std::string& path, const std::string& as) -> std::string {
        if (path.empty()) return "";
        for (const char* ext : {".png", ".tga"}) {
            const fs::path src = rp / (path + ext);
            if (!fs::is_regular_file(src)) continue;
            if (std::string(ext) == ".tga") {
                line('!', path + ext, "textura TGA: zatím jen PNG");
                return "";
            }
            const std::string name = as + ".png";
            if (!copied.count(name)) {
                fs::copy_file(src, pack / "textures" / "mod" / name, fs::copy_options::overwrite_existing, ec);
                copied.insert(name);
                ++rep.textures;
            }
            return name;
        }
        line('x', path + ".png", "textura chybí: zůstane růžovo-černá");
        return "";
    };
    auto terrainTexture = [&](const std::string& shortName, const std::string& as) -> std::string {
        const Json& t = terrain[shortName];
        if (t.isNull()) {
            line('!', "terrain_texture.json", "neznámá textura '{0}'", {shortName});
            return "";
        }
        return copyTexture(t["rp"].str(""), t["path"].str(""), as);
    };
    auto nameOf = [&](const std::string& kind, const std::string& id) {
        for (const std::string& key : {kind + "." + id + ".name", kind + "." + id, kind + "." + localId(id) + ".name"}) {
            if (names.count(key)) return std::make_pair(names[key], namesCs.count(key) ? namesCs[key] : names[key]);
        }
        return std::make_pair(pretty(id), pretty(id));
    };

    Json blocks = Json::array(), items = Json::array(), mobs = Json::array();
    std::string ns;
    int scripts = 0;
    for (const Pack& p : packs) {
        if (!p.behaviour) continue;
        {
            std::ifstream s(root / "scripts.txt");
            int n = 0;
            if (s >> n) scripts = n;
        }
        Json j;
        // Blocks.
        for (auto& e : fs::directory_iterator(p.dir / "blocks", ec)) {
            if (e.path().extension() != ".json") continue;
            const std::string rel = fs::relative(e.path(), root, ec).generic_string();
            std::string err;
            if (!readJson(e.path(), j, &err)) {
                line('x', rel, "nejde přečíst: {0}", {err});
                continue;
            }
            const Json& b = j["minecraft:block"];
            const std::string id = b["description"]["identifier"].str("");
            if (id.empty()) continue;
            if (ns.empty()) ns = localId(id.substr(0, id.find(':')));
            const Json& comp = b["components"];
            if (!comp["minecraft:geometry"].isNull() && comp["minecraft:geometry"].str(comp["minecraft:geometry"]["identifier"].str("")) != "minecraft:geometry.full_block") {
                line('!', rel, "{0}: vlastní tvar > krychle", {id});
            }
            Json vb = Json::object();
            const auto nm = nameOf("tile", id);
            vb.set("id", Json::string(localId(id)));
            vb.set("name", Json::string(nm.first));
            vb.set("name_cs", Json::string(nm.second));
            const Json& mat = comp["minecraft:material_instances"];
            std::string tex;
            if (mat.size()) tex = mat[mat.keys()[0]]["texture"].str("");
            if (tex.empty() && blocksJson.count(id)) {
                const Json& t = blocksJson[id]["textures"];
                tex = t.isString() ? t.str() : t["side"].str(t["up"].str(""));
            }
            const std::string file = terrainTexture(tex, "block_" + localId(id));
            if (!file.empty()) vb.set("texture", Json::string(file));
            const double secs = comp["minecraft:destructible_by_mining"]["seconds_to_destroy"].num(comp["minecraft:destroy_time"].num(1.5));
            vb.set("hardness", Json::number(std::clamp(secs, 0.1, 20.0)));
            if (comp["minecraft:light_emission"].num(0) > 0) vb.set("light", Json::fromBool(true));
            blocks.push(vb);
            ++rep.blocks;
            line('+', rel, "{0} > VC blok", {id});
        }
        // Items.
        for (auto& e : fs::directory_iterator(p.dir / "items", ec)) {
            if (e.path().extension() != ".json") continue;
            const std::string rel = fs::relative(e.path(), root, ec).generic_string();
            std::string err;
            if (!readJson(e.path(), j, &err)) {
                line('x', rel, "nejde přečíst: " + err);
                continue;
            }
            const Json& it = j["minecraft:item"];
            const std::string id = it["description"]["identifier"].str("");
            if (id.empty()) continue;
            if (ns.empty()) ns = localId(id.substr(0, id.find(':')));
            const Json& comp = it["components"];
            const Json& icon = comp["minecraft:icon"];
            const std::string iconName = icon.isString() ? icon.str() : icon["texture"].str(icon["textures"]["default"].str(""));
            Json vi = Json::object();
            const auto nm = nameOf("item", id);
            vi.set("id", Json::string(localId(id)));
            vi.set("name", Json::string(nm.first));
            vi.set("name_cs", Json::string(nm.second));
            const Json& t = itemTex[iconName];
            const std::string file = t.isNull() ? "" : copyTexture(t["rp"].str(""), t["path"].str(""), "item_" + localId(id));
            if (file.empty()) line('!', rel, "{0}: ikona '{1}' nenalezena", {id, iconName});
            else vi.set("texture", Json::string(file));
            if (comp["minecraft:food"].isObject()) vi.set("food", Json::number(comp["minecraft:food"]["nutrition"].num(4)));
            vi.set("stack", Json::number(comp["minecraft:max_stack_size"].isNumber() ? comp["minecraft:max_stack_size"].num(64) : comp["minecraft:max_stack_size"]["value"].num(64)));
            items.push(vi);
            ++rep.items;
            line('+', rel, "{0} > VC předmět", {id});
        }
        // Entities.
        for (auto& e : fs::directory_iterator(p.dir / "entities", ec)) {
            if (e.path().extension() != ".json") continue;
            const std::string rel = fs::relative(e.path(), root, ec).generic_string();
            std::string err;
            if (!readJson(e.path(), j, &err)) {
                line('x', rel, "nejde přečíst: " + err);
                continue;
            }
            const Json& en = j["minecraft:entity"];
            const std::string id = en["description"]["identifier"].str("");
            if (id.empty() || id.rfind("minecraft:", 0) == 0) {
                if (!id.empty()) line('!', rel, "{0}: úprava tvora ze hry Minecraft se nepřebírá", {id});
                continue;
            }
            if (ns.empty()) ns = localId(id.substr(0, id.find(':')));
            const Json& comp = en["components"];
            std::set<std::string> family;
            for (size_t i = 0; i < comp["minecraft:type_family"]["family"].size(); ++i) family.insert(comp["minecraft:type_family"]["family"][i].str(""));
            // The body: from the geometry's bones (legs, arms, wings) and the families.
            std::string base = family.count("monster") ? "zombie" : "pig";
            std::vector<std::string> modelNote;
            const ClientEntity* ce = clients.count(id) ? &clients[id] : nullptr;
            if (ce && geometries.count(ce->geometry)) {
                const Json& g = geometries[ce->geometry];
                std::set<std::string> bones;
                for (size_t i = 0; i < g["bones"].size(); ++i) bones.insert(lower(g["bones"][i]["name"].str("")));
                auto has = [&](const char* s) { return std::any_of(bones.begin(), bones.end(), [&](const std::string& b) { return b.find(s) != std::string::npos; }); };
                const double h = g["description"]["visible_bounds_height"].num(1);
                if (has("leg4") || has("leg7") || family.count("spider") || family.count("arthropod")) base = "spider";
                else if (has("wing")) base = "chicken";
                else if (has("arm") && has("leg")) base = family.count("monster") ? "zombie" : "villager";
                else if (has("leg")) base = h > 1.2 ? "cow" : family.count("sheep") ? "sheep" : "pig";
                else if (family.count("slime") || bones.size() <= 2) base = "slime";
                ++rep.models;
                modelNote = {ce->geometry, std::to_string(bones.size()), base};
            }
            Json vm = Json::object();
            const auto nm = nameOf("entity", id);
            vm.set("id", Json::string(localId(id)));
            vm.set("name", Json::string(nm.first));
            vm.set("name_cs", Json::string(nm.second));
            vm.set("base", Json::string(base));
            const Json& hp = comp["minecraft:health"];
            vm.set("health", Json::number(std::clamp(hp["max"].num(hp["value"].num(10)), 1.0, 400.0)));
            vm.set("spawn", Json::number(0.1));
            if (ce) {
                const std::string file = copyTexture(ce->rp, ce->texture, "mob_" + localId(id));
                if (!file.empty()) vm.set("texture", Json::string(file)), vm.set("face", Json::string(file));
            } else {
                line('!', rel, "{0}: chybí vzhled (resource pack)", {id});
            }
            mobs.push(vm);
            ++rep.mobs;
            line('+', rel, "{0} > VC mob", {id});
            if (!modelNote.empty()) line('!', "geo.json", "{0} ({1} kostí) > tělo: {2}", modelNote);
            if (comp.size() > 0) {
                static const std::set<std::string> kKnown = {"minecraft:health", "minecraft:type_family", "minecraft:movement", "minecraft:physics",
                                                             "minecraft:collision_box", "minecraft:loot", "minecraft:pushable", "minecraft:nameable"};
                int known = 0;
                for (const std::string& k : comp.keys()) known += kKnown.count(k) ? 1 : 0;
                if (known < (int)comp.size())
                    line('!', rel, "komponenty: {0} z {1} známých", {std::to_string(known), std::to_string(comp.size())});
            }
        }
        if (fs::is_directory(p.dir / "loot_tables")) line('!', "loot_tables/", "loot zatím jen základní dropy");
        if (fs::is_directory(p.dir / "functions")) line('x', "functions/", "příkazové funkce nejdou spustit");
        if (fs::is_directory(p.dir / "animations") || fs::is_directory(p.dir / "animation_controllers"))
            line('!', "animations/", "animace: chůze a útok podle těla VC");
    }
    for (const Pack& p : packs)
        if (p.resources && fs::is_directory(p.dir / "ui")) line('x', "ui/", "UI se nepřebírá (un-UI)");
    rep.scripts = scripts;
    if (scripts > 0) line('x', "scripts/", "{0} skriptů: kód nejde spustit", {std::to_string(scripts)});

    if (ns.empty()) ns = "bedrock";
    Json content = Json::object();
    content.set("namespace", Json::string(ns));
    content.set("blocks", blocks);
    content.set("items", items);
    content.set("mobs", mobs);
    std::ofstream(pack / "content.json") << content.dump();
    Json m = Json::object();
    m.set("name", Json::string(rep.name));
    m.set("description", Json::string("Bedrock > VC (un-UI)"));
    m.set("version", Json::string("1.0"));
    m.set("origin", Json::string("bedrock"));
    std::ofstream(pack / "manifest.json") << m.dump();
    std::ofstream out(pack / "import_lines.txt");
    for (const ImportLine& l : rep.lines) {
        out << l.status << "|" << l.file << "|" << l.text << "|";
        for (size_t i = 0; i < l.args.size(); ++i) out << (i ? "\t" : "") << l.args[i];
        out << "\n";
    }
    std::ofstream(pack / "import_report.txt") << "name=" << rep.name << "\nblocks=" << rep.blocks << "\nitems=" << rep.items << "\nmobs=" << rep.mobs
                                              << "\ntextures=" << rep.textures << "\nmodels=" << rep.models << "\nscripts=" << rep.scripts << "\n";
    return rep;
}

std::vector<ImportLine> readImportLines(const std::string& packDir) {
    std::vector<ImportLine> out;
    std::ifstream in(packDir + "/import_lines.txt");
    std::string l;
    while (std::getline(in, l)) {
        const size_t a = l.find('|'), b = a == std::string::npos ? a : l.find('|', a + 1);
        if (b == std::string::npos || a != 1) continue;
        const size_t c = l.find('|', b + 1);
        ImportLine il{l[0], l.substr(2, b - 2), l.substr(b + 1, c == std::string::npos ? std::string::npos : c - b - 1), {}};
        if (c != std::string::npos) {
            std::stringstream args(l.substr(c + 1));
            std::string arg;
            while (std::getline(args, arg, '\t')) il.args.push_back(arg);
        }
        out.push_back(il);
    }
    return out;
}

}  // namespace vc

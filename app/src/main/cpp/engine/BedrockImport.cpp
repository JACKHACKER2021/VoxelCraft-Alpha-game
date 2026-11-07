#include "BedrockImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#include "Json.h"

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

std::pair<std::string, std::string> splitId(const std::string& ref) {
    const size_t colon = ref.find(':');
    if (colon == std::string::npos) return {"custom", ref};
    return {ref.substr(0, colon), ref.substr(colon + 1)};
}

std::map<std::string, std::string> loadLang(const fs::path& p) {
    std::map<std::string, std::string> m;
    std::istringstream in(readText(p));
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            while (!k.empty() && (k.back() == ' ' || k.back() == '\t' || k.back() == '\r')) k.pop_back();
            while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();
            m[k] = v;
        }
    }
    return m;
}

void copyDirectoryRecursive(const fs::path& src, const fs::path& dst, int& fileCount) {
    std::error_code ec;
    if (!fs::is_directory(src)) return;
    fs::create_directories(dst, ec);
    for (const auto& entry : fs::recursive_directory_iterator(src, ec)) {
        const auto& path = entry.path();
        auto rel = fs::relative(path, src, ec);
        auto target = dst / rel;
        if (entry.is_directory()) {
            fs::create_directories(target, ec);
        } else if (entry.is_regular_file()) {
            fs::create_directories(target.parent_path(), ec);
            fs::copy_file(path, target, fs::copy_options::overwrite_existing, ec);
            fileCount++;
        }
    }
}

std::string sanitizeFilename(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.') out += c;
        else out += '_';
    }
    return out;
}

std::string translateGroup(const std::string& raw, const std::map<std::string, std::string>& lang) {
    std::string text = raw;
    if (lang.count(raw)) text = lang.at(raw);

    std::string lower = text;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);

    if (lower.find("locomotive") != std::string::npos || lower.find("engine") != std::string::npos) return "Lokomotivy";
    if (lower.find("rolling_stock") != std::string::npos || lower.find("car") != std::string::npos) return "Vagóny";
    if (lower.find("buffer") != std::string::npos) return "Nárazníky";
    if (lower.find("switch") != std::string::npos) return "Výhybky";
    if (lower.find("signal") != std::string::npos) return "Návěstidla & Signály";
    if (lower.find("rail") != std::string::npos || lower.find("track") != std::string::npos) return "Koleje";
    if (lower.find("trestle") != std::string::npos || lower.find("platform") != std::string::npos ||
        lower.find("tunnel") != std::string::npos || lower.find("tile") != std::string::npos)
        return "Konstrukce a Nástupiště";
    if (lower.find("ticket") != std::string::npos) return "Jízdenky";
    return text.empty() ? "Vlaky & Doplňky" : text;
}

}  // namespace

bool isBedrockAddon(const std::string& dir) {
    const fs::path p(dir);
    Json m;
    if (readJson(p / "manifest.json", m)) return true;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(p, ec)) {
        if (entry.is_directory()) {
            if (readJson(entry.path() / "manifest.json", m)) return true;
            if (fs::is_directory(entry.path() / "blocks") || fs::is_directory(entry.path() / "entities")) return true;
        }
    }
    return fs::is_directory(p / "blocks") || fs::is_directory(p / "entities") || fs::is_regular_file(p / "textures" / "terrain_texture.json");
}

BedrockImportReport convertBedrockAddon(const std::string& bedrockDir, const std::string& packDir) {
    BedrockImportReport rep;
    const fs::path root(bedrockDir), dst(packDir);
    std::error_code ec;
    fs::create_directories(dst / "textures" / "mod", ec);
    fs::create_directories(dst / "recipes", ec);
    fs::create_directories(dst / "models", ec);
    fs::create_directories(dst / "scripts", ec);

    // Collect all candidate root folders (either root itself or subfolders like TRAINS BP / TRAINS RP)
    std::vector<fs::path> bpDirs, rpDirs;
    std::vector<fs::path> allDirs = {root};
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (entry.is_directory()) allDirs.push_back(entry.path());
    }

    for (const auto& d : allDirs) {
        bool hasBp = fs::is_directory(d / "entities") || fs::is_directory(d / "blocks") ||
                     fs::is_directory(d / "items") || fs::is_directory(d / "recipes");
        bool hasRp = fs::is_directory(d / "models") || fs::is_directory(d / "textures") ||
                     fs::is_directory(d / "texts") || fs::is_directory(d / "entity");
        if (hasBp) bpDirs.push_back(d);
        if (hasRp) rpDirs.push_back(d);
    }
    if (bpDirs.empty()) bpDirs.push_back(root);
    if (rpDirs.empty()) rpDirs.push_back(root);

    // Read manifest from first BP or RP with manifest.json
    Json m;
    std::string packName = "VoxelCraft Trains Addon";
    std::string packDesc = "";
    std::string packVer = "2.2.0";
    for (const auto& d : allDirs) {
        if (readJson(d / "manifest.json", m)) {
            if (m["header"]["name"].isString()) packName = m["header"]["name"].str();
            else if (m["name"].isString()) packName = m["name"].str();

            if (m["header"]["description"].isString()) packDesc = m["header"]["description"].str();
            else if (m["description"].isString()) packDesc = m["description"].str();

            if (m["header"]["version"].isArray() && m["header"]["version"].size() >= 3) {
                packVer = std::to_string((int)m["header"]["version"][0].num()) + "." +
                          std::to_string((int)m["header"]["version"][1].num()) + "." +
                          std::to_string((int)m["header"]["version"][2].num());
            }
            break;
        }
    }
    rep.name = packName;

    // Load language files from all RP dirs
    std::map<std::string, std::string> langEn, langCs;
    for (const auto& rp : rpDirs) {
        auto e = loadLang(rp / "texts" / "en_US.lang");
        langEn.insert(e.begin(), e.end());
        auto c = loadLang(rp / "texts" / "cs_CZ.lang");
        langCs.insert(c.begin(), c.end());
        auto c2 = loadLang(rp / "texts" / "cs.lang");
        langCs.insert(c2.begin(), c2.end());
    }

    // Load texture maps
    std::map<std::string, std::string> terrainTex, itemTex;
    for (const auto& rp : rpDirs) {
        Json tj;
        if (readJson(rp / "textures" / "terrain_texture.json", tj)) {
            const auto& tdata = tj["texture_data"];
            if (tdata.isObject()) {
                for (const auto& k : tdata.keys()) {
                    const auto& v = tdata[k];
                    if (v["textures"].isString()) terrainTex[k] = v["textures"].str();
                    else if (v["textures"].isArray() && v["textures"].size() > 0 && v["textures"][0].isString())
                        terrainTex[k] = v["textures"][0].str();
                }
            }
        }
        Json ij;
        if (readJson(rp / "textures" / "item_texture.json", ij)) {
            const auto& idata = ij["texture_data"];
            if (idata.isObject()) {
                for (const auto& k : idata.keys()) {
                    const auto& v = idata[k];
                    if (v["textures"].isString()) itemTex[k] = v["textures"].str();
                    else if (v["textures"].isArray() && v["textures"].size() > 0 && v["textures"][0].isString())
                        itemTex[k] = v["textures"][0].str();
                }
            }
        }
    }

    // Load geometry models (.geo.json)
    struct ModelDef {
        std::vector<std::vector<double>> boxes;
    };
    std::map<std::string, ModelDef> geometries;
    for (const auto& rp : rpDirs) {
        if (!fs::is_directory(rp / "models")) continue;
        for (const auto& e : fs::recursive_directory_iterator(rp / "models", ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") continue;
            Json gj;
            if (!readJson(e.path(), gj)) continue;
            const Json& glist = gj["minecraft:geometry"];
            if (!glist.isArray()) continue;
            for (size_t gi = 0; gi < glist.size(); ++gi) {
                const Json& g = glist[gi];
                std::string gid = g["description"]["identifier"].str();
                if (gid.empty()) continue;
                ModelDef def;
                const Json& bones = g["bones"];
                for (size_t bi = 0; bi < bones.size() && def.boxes.size() < 10; ++bi) {
                    const Json& cubes = bones[bi]["cubes"];
                    for (size_t ci = 0; ci < cubes.size() && def.boxes.size() < 10; ++ci) {
                        const Json& cube = cubes[ci];
                        const Json& o = cube["origin"];
                        const Json& s = cube["size"];
                        if (o.isArray() && s.isArray() && o.size() >= 3 && s.size() >= 3) {
                            double x0 = (o[0].num() + 8.0) / 16.0;
                            double y0 = o[1].num() / 16.0;
                            double z0 = (o[2].num() + 8.0) / 16.0;
                            double x1 = x0 + s[0].num() / 16.0;
                            double y1 = y0 + s[1].num() / 16.0;
                            double z1 = z0 + s[2].num() / 16.0;
                            // normalize coordinates
                            x0 = std::clamp(x0, -2.0, 3.0);
                            y0 = std::clamp(y0, 0.0, 4.0);
                            z0 = std::clamp(z0, -2.0, 3.0);
                            x1 = std::clamp(x1, -2.0, 3.0);
                            y1 = std::clamp(y1, 0.0, 4.0);
                            z1 = std::clamp(z1, -2.0, 3.0);
                            def.boxes.push_back({x0, y0, z0, x1, y1, z1});
                        }
                    }
                }
                if (!def.boxes.empty()) {
                    geometries[gid] = def;
                    rep.models++;
                }
            }
        }
    }

    // Load client entity mappings (identifier -> geometry & texture)
    struct ClientEntityDef {
        std::string geometry;
        std::string texture;
    };
    std::map<std::string, ClientEntityDef> clientEntities;
    for (const auto& rp : rpDirs) {
        if (!fs::is_directory(rp / "entity")) continue;
        for (const auto& e : fs::recursive_directory_iterator(rp / "entity", ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") continue;
            Json cj;
            if (!readJson(e.path(), cj)) continue;
            const Json& desc = cj["minecraft:client_entity"]["description"];
            std::string eid = desc["identifier"].str();
            if (eid.empty()) continue;
            ClientEntityDef ced;
            ced.geometry = desc["geometry"]["default"].str();
            const Json& tex = desc["textures"];
            if (tex.isObject()) {
                if (tex["default"].isString()) ced.texture = tex["default"].str();
                else if (tex.keys().size() > 0) ced.texture = tex[tex.keys()[0]].str();
            }
            clientEntities[eid] = ced;
        }
    }

    // Helper to find and copy texture
    auto resolveTexture = [&](const std::string& pathOrName, const std::string& defaultName) -> std::string {
        if (pathOrName.empty()) return defaultName + ".png";
        std::string path = pathOrName;
        if (path.rfind(".png") == std::string::npos) path += ".png";
        for (const auto& rp : rpDirs) {
            fs::path cand = rp / path;
            if (fs::is_regular_file(cand)) {
                std::string dstName = sanitizeFilename(cand.filename().string());
                fs::copy_file(cand, dst / "textures" / "mod" / dstName, fs::copy_options::overwrite_existing, ec);
                rep.textures++;
                return dstName;
            }
            cand = rp / "textures" / path;
            if (fs::is_regular_file(cand)) {
                std::string dstName = sanitizeFilename(cand.filename().string());
                fs::copy_file(cand, dst / "textures" / "mod" / dstName, fs::copy_options::overwrite_existing, ec);
                rep.textures++;
                return dstName;
            }
        }
        return defaultName + ".png";
    };

    std::string packNs = "pod_trn";
    Json blocksArr = Json::array();
    Json itemsArr = Json::array();

    // 1. Parse BP Blocks
    for (const auto& bp : bpDirs) {
        if (!fs::is_directory(bp / "blocks")) continue;
        for (const auto& e : fs::recursive_directory_iterator(bp / "blocks", ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") continue;
            Json bj;
            if (!readJson(e.path(), bj)) continue;
            const Json& mb = bj["minecraft:block"];
            if (!mb.isObject()) continue;
            std::string ident = mb["description"]["identifier"].str();
            if (ident.empty()) continue;
            auto [ns, id] = splitId(ident);
            packNs = ns;

            Json blk = Json::object();
            blk.set("id", Json::string(id));
            std::string enName = langEn.count("tile." + ident + ".name") ? langEn["tile." + ident + ".name"] : pretty(id);
            blk.set("name", Json::string(enName));
            if (langCs.count("tile." + ident + ".name")) {
                blk.set("name_cs", Json::string(langCs["tile." + ident + ".name"]));
            }

            // Creative Group
            std::string rawGroup = mb["description"]["menu_category"]["group"].str();
            blk.set("group", Json::string(translateGroup(rawGroup, langEn)));

            // Geometry & Boxes
            std::string geom;
            if (mb["components"]["minecraft:geometry"].isString()) geom = mb["components"]["minecraft:geometry"].str();
            else if (mb["components"]["minecraft:geometry"]["identifier"].isString()) geom = mb["components"]["minecraft:geometry"]["identifier"].str();

            if (!geom.empty() && geometries.count(geom)) {
                Json boxArr = Json::array();
                for (const auto& b : geometries[geom].boxes) {
                    Json one = Json::array();
                    for (double val : b) one.push(Json::number(val));
                    boxArr.push(one);
                }
                blk.set("boxes", boxArr);
            }

            // Power / Redstone responsiveness
            bool power = ident.find("signal") != std::string::npos || ident.find("power") != std::string::npos ||
                         ident.find("switch") != std::string::npos || ident.find("turn_table") != std::string::npos;
            if (power) blk.set("power", Json::fromBool(true));

            // Hardness
            double hardness = 1.5;
            if (mb["components"]["minecraft:destructible_by_mining"]["seconds_to_destroy"].isNumber()) {
                hardness = mb["components"]["minecraft:destructible_by_mining"]["seconds_to_destroy"].num();
            }
            blk.set("hardness", Json::number(hardness));
            blk.set("tool", Json::string("pickaxe"));

            // Texture
            std::string matTex;
            const auto& mat = mb["components"]["minecraft:material_instances"]["*"]["texture"];
            if (mat.isString()) matTex = mat.str();
            std::string texRel = terrainTex.count(matTex) ? terrainTex[matTex] : (terrainTex.count(id) ? terrainTex[id] : matTex);
            std::string finalTex = resolveTexture(texRel, id);
            blk.set("texture", Json::string(finalTex));

            blocksArr.push(blk);
            rep.blocks++;
        }
    }

    // 2. Parse BP Entities (e.g. Boxcar, Locomotives)
    for (const auto& bp : bpDirs) {
        if (!fs::is_directory(bp / "entities")) continue;
        for (const auto& e : fs::recursive_directory_iterator(bp / "entities", ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") continue;
            Json ej;
            if (!readJson(e.path(), ej)) continue;
            const Json& me = ej["minecraft:entity"];
            if (!me.isObject()) continue;
            std::string ident = me["description"]["identifier"].str();
            if (ident.empty()) continue;
            auto [ns, id] = splitId(ident);

            const Json& comps = me["components"];
            bool hasInv = comps["minecraft:inventory"].isObject();
            int invSize = hasInv ? (int)comps["minecraft:inventory"]["inventory_size"].num(27) : 0;
            bool rideable = comps["minecraft:rideable"].isObject();

            Json blk = Json::object();
            blk.set("id", Json::string(id));
            std::string enName = langEn.count("entity." + ident + ".name") ? langEn["entity." + ident + ".name"] : pretty(id);
            blk.set("name", Json::string(enName));
            if (langCs.count("entity." + ident + ".name")) {
                blk.set("name_cs", Json::string(langCs["entity." + ident + ".name"]));
            }

            if (rideable) blk.set("group", Json::string("Lokomotivy"));
            else if (hasInv) blk.set("group", Json::string("Vagóny"));
            else blk.set("group", Json::string("Vlaky"));

            if (hasInv) {
                blk.set("has_inventory", Json::fromBool(true));
                blk.set("inventory_size", Json::number(invSize));
            }
            if (rideable) blk.set("rideable", Json::fromBool(true));

            // Geometry & Boxes
            std::string geom = clientEntities.count(ident) ? clientEntities[ident].geometry : "";
            if (!geom.empty() && geometries.count(geom)) {
                Json boxArr = Json::array();
                for (const auto& b : geometries[geom].boxes) {
                    Json one = Json::array();
                    for (double val : b) one.push(Json::number(val));
                    boxArr.push(one);
                }
                blk.set("boxes", boxArr);
            } else {
                // Default representative vehicle box: 3 blocks long, 1.2 high
                Json boxArr = Json::array();
                Json one = Json::array();
                one.push(Json::number(-0.5)); one.push(Json::number(0.0)); one.push(Json::number(-1.0));
                one.push(Json::number(1.5)); one.push(Json::number(1.5)); one.push(Json::number(2.0));
                boxArr.push(one);
                blk.set("boxes", boxArr);
            }

            blk.set("hardness", Json::number(3.0));
            blk.set("tool", Json::string("pickaxe"));

            // Texture
            std::string texRel = clientEntities.count(ident) ? clientEntities[ident].texture : "";
            std::string finalTex = resolveTexture(texRel, id);
            blk.set("texture", Json::string(finalTex));

            blocksArr.push(blk);
            rep.blocks++;
        }
    }

    // 3. Parse BP Items (including Placers)
    for (const auto& bp : bpDirs) {
        if (!fs::is_directory(bp / "items")) continue;
        for (const auto& e : fs::recursive_directory_iterator(bp / "items", ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") continue;
            Json ij;
            if (!readJson(e.path(), ij)) continue;
            const Json& mi = ij["minecraft:item"];
            if (!mi.isObject()) continue;
            std::string ident = mi["description"]["identifier"].str();
            if (ident.empty()) continue;
            auto [ns, id] = splitId(ident);

            Json itm = Json::object();
            itm.set("id", Json::string(id));
            std::string enName = langEn.count("item." + ident + ".name") ? langEn["item." + ident + ".name"] : pretty(id);
            itm.set("name", Json::string(enName));
            if (langCs.count("item." + ident + ".name")) {
                itm.set("name_cs", Json::string(langCs["item." + ident + ".name"]));
            }

            // Creative Group
            std::string rawGroup = mi["description"]["menu_category"]["group"].str();
            itm.set("group", Json::string(translateGroup(rawGroup, langEn)));
            itm.set("stack", Json::number(mi["components"]["minecraft:max_stack_size"].num(64)));

            // Placer link (places the matching block/entity)
            itm.set("places", Json::string(ident));

            // Texture
            std::string iconKey = mi["components"]["minecraft:icon"].str();
            std::string texRel = itemTex.count(iconKey) ? itemTex[iconKey] : (itemTex.count(ident) ? itemTex[ident] : iconKey);
            std::string finalTex = resolveTexture(texRel, id);
            itm.set("texture", Json::string(finalTex));

            itemsArr.push(itm);
            rep.items++;
        }
    }

    // Copy models and scripts directories
    for (const auto& rp : rpDirs) {
        if (fs::is_directory(rp / "models")) copyDirectoryRecursive(rp / "models", dst / "models", rep.models);
    }
    for (const auto& bp : bpDirs) {
        if (fs::is_directory(bp / "scripts")) copyDirectoryRecursive(bp / "scripts", dst / "scripts", rep.scripts);
    }

    Json content = Json::object();
    content.set("namespace", Json::string(packNs));
    content.set("blocks", blocksArr);
    content.set("items", itemsArr);
    if (rep.models > 0) content.set("has_models", Json::fromBool(true));
    if (rep.scripts > 0) content.set("has_scripts", Json::fromBool(true));

    // Write manifest.json & content.json
    Json manifest = Json::object();
    manifest.set("name", Json::string(packName));
    manifest.set("description", Json::string(packDesc));
    manifest.set("version", Json::string(packVer));
    std::ofstream(dst / "manifest.json") << manifest.dump();
    std::ofstream(dst / "content.json") << content.dump();

    return rep;
}

}  // namespace vc

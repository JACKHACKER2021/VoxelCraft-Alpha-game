#include "BedrockImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

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

}  // namespace

bool isBedrockAddon(const std::string& dir) {
    const fs::path p(dir);
    Json m;
    if (readJson(p / "manifest.json", m)) {
        if (m.has("format_version") && m.has("header")) return true;
        if (m.has("modules")) return true;
    }
    return fs::is_directory(p / "blocks") || fs::is_regular_file(p / "textures" / "terrain_texture.json");
}

BedrockImportReport convertBedrockAddon(const std::string& bedrockDir, const std::string& packDir) {
    BedrockImportReport rep;
    const fs::path src(bedrockDir), dst(packDir);
    std::error_code ec;
    fs::create_directories(dst / "textures" / "mod", ec);
    fs::create_directories(dst / "recipes", ec);
    fs::create_directories(dst / "models", ec);
    fs::create_directories(dst / "scripts", ec);

    // Read manifest
    Json m;
    std::string packName = "Bedrock Addon";
    std::string packDesc = "";
    std::string packVer = "1.0.0";
    if (readJson(src / "manifest.json", m)) {
        if (m["header"]["name"].isString()) packName = m["header"]["name"].str();
        else if (m["name"].isString()) packName = m["name"].str();

        if (m["header"]["description"].isString()) packDesc = m["header"]["description"].str();
        else if (m["description"].isString()) packDesc = m["description"].str();

        if (m["header"]["version"].isArray() && m["header"]["version"].size() >= 3) {
            packVer = std::to_string((int)m["header"]["version"][0].num()) + "." +
                      std::to_string((int)m["header"]["version"][1].num()) + "." +
                      std::to_string((int)m["header"]["version"][2].num());
        }
    }
    rep.name = packName;

    // Load language files
    auto langEn = loadLang(src / "texts" / "en_US.lang");
    auto langCs = loadLang(src / "texts" / "cs_CZ.lang");
    if (langCs.empty()) langCs = loadLang(src / "texts" / "cs.lang");

    // Load texture maps
    std::map<std::string, std::string> terrainTex;
    Json terrainJson;
    if (readJson(src / "textures" / "terrain_texture.json", terrainJson)) {
        const auto& tdata = terrainJson["texture_data"];
        if (tdata.isObject()) {
            for (const auto& k : tdata.keys()) {
                const auto& v = tdata[k];
                if (v["textures"].isString()) terrainTex[k] = v["textures"].str();
                else if (v["textures"].isArray() && v["textures"].size() > 0 && v["textures"][0].isString())
                    terrainTex[k] = v["textures"][0].str();
            }
        }
    }

    std::string packNs = "bedrock";
    Json blocksArr = Json::array();
    Json itemsArr = Json::array();

    // Parse blocks
    if (fs::is_directory(src / "blocks")) {
        for (const auto& e : fs::directory_iterator(src / "blocks", ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") continue;
            Json bj;
            if (!readJson(e.path(), bj)) continue;
            const auto& mb = bj["minecraft:block"];
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

            // Custom geometry or shape support
            std::string geom;
            if (mb["components"]["minecraft:geometry"].isString()) {
                geom = mb["components"]["minecraft:geometry"].str();
            } else if (mb["components"]["minecraft:geometry"]["identifier"].isString()) {
                geom = mb["components"]["minecraft:geometry"]["identifier"].str();
            }
            if (!geom.empty()) {
                blk.set("geometry", Json::string(geom));
                rep.notes.push_back("Geometrie pro blok: " + id + " -> " + geom);
            }

            double hardness = 1.5;
            if (mb["components"]["minecraft:destructible_by_mining"]["seconds_to_destroy"].isNumber()) {
                hardness = mb["components"]["minecraft:destructible_by_mining"]["seconds_to_destroy"].num();
            }
            blk.set("hardness", Json::number(hardness));
            blk.set("tool", Json::string("pickaxe"));

            // Find texture
            std::string texShort;
            const auto& mat = mb["components"]["minecraft:material_instances"]["*"]["texture"];
            if (mat.isString()) texShort = mat.str();
            if (texShort.empty()) texShort = id;

            std::string texRel = terrainTex.count(texShort) ? terrainTex[texShort] : "textures/blocks/" + texShort;
            if (texRel.rfind(".png") == std::string::npos) texRel += ".png";
            fs::path texFile = src / texRel;
            if (!fs::is_regular_file(texFile)) texFile = src / "textures" / "blocks" / (id + ".png");

            std::string dstPng = id + ".png";
            if (fs::is_regular_file(texFile)) {
                fs::copy_file(texFile, dst / "textures" / "mod" / dstPng, fs::copy_options::overwrite_existing, ec);
                rep.textures++;
            }
            blk.set("texture", Json::string(dstPng));

            blocksArr.push(blk);
            rep.blocks++;
        }
    }

    // Parse items
    if (fs::is_directory(src / "items")) {
        for (const auto& e : fs::directory_iterator(src / "items", ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") continue;
            Json ij;
            if (!readJson(e.path(), ij)) continue;
            const auto& mi = ij["minecraft:item"];
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
            itm.set("stack", Json::number(64));

            std::string dstPng = id + ".png";
            fs::path texFile = src / "textures" / "items" / dstPng;
            if (fs::is_regular_file(texFile)) {
                fs::copy_file(texFile, dst / "textures" / "mod" / dstPng, fs::copy_options::overwrite_existing, ec);
                rep.textures++;
            }
            itm.set("texture", Json::string(dstPng));

            itemsArr.push(itm);
            rep.items++;
        }
    }

    // Preserve 3D geometry models (models/blocks, models/entity, etc.)
    if (fs::is_directory(src / "models")) {
        copyDirectoryRecursive(src / "models", dst / "models", rep.models);
        if (rep.models > 0) {
            rep.notes.push_back("Zkopírováno " + std::to_string(rep.models) + " 3D modelů geometrie.");
        }
    }

    // Preserve scripts (scripts/, scripts/server, scripts/client)
    if (fs::is_directory(src / "scripts")) {
        copyDirectoryRecursive(src / "scripts", dst / "scripts", rep.scripts);
        if (rep.scripts > 0) {
            rep.notes.push_back("Zachováno " + std::to_string(rep.scripts) + " skriptů addonu.");
        }
    }

    Json content = Json::object();
    content.set("namespace", Json::string(packNs));
    content.set("blocks", blocksArr);
    content.set("items", itemsArr);
    if (rep.models > 0) content.set("has_models", Json::fromBool(true));
    if (rep.scripts > 0) content.set("has_scripts", Json::fromBool(true));

    // Write manifest.json
    Json manifest = Json::object();
    manifest.set("name", Json::string(packName));
    manifest.set("description", Json::string(packDesc));
    manifest.set("version", Json::string(packVer));
    std::ofstream(dst / "manifest.json") << manifest.dump();

    // Write content.json
    std::ofstream(dst / "content.json") << content.dump();

    return rep;
}

}  // namespace vc

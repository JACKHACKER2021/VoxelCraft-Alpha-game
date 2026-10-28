#include "Packs.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace vc {

namespace fs = std::filesystem;

namespace {

std::string readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeText(const fs::path& p, const std::string& text) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    fs::path tmp = p;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        if (!out.flush()) return false;
    }
    fs::rename(tmp, p, ec);
    return !ec;
}

bool parseFile(const fs::path& p, Json& out, std::vector<std::string>* w, const std::string& label) {
    std::string err;
    if (Json::parse(readText(p), out, err)) return true;
    if (w) w->push_back(label + ": " + err);
    return false;
}

std::string slug(const std::string& name) {
    std::string s;
    for (unsigned char c : name) {
        if (std::isalnum(c)) s += (char)std::tolower(c);
        else if (!s.empty() && s.back() != '_') s += '_';
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s.empty() ? "pack" : s;
}

std::vector<fs::path> jsonFiles(const fs::path& dir) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file() && e.path().extension() == ".json") out.push_back(e.path());
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

PackManager::PackManager(const std::string& dataDir) : root_(dataDir + "/packs"), statePath_(dataDir + "/packs.json") {}

void PackManager::ensureDefaultPack() {
    const fs::path dir = fs::path(root_) / "default";
    if (!fs::exists(dir / "manifest.json")) {
        resetDefaultPack();
        return;
    }
    // Upgrade a default pack from an older version without touching the player's edits:
    // new files are added, and the overworld gets the ores that came with VoxelCraft 1.0.
    for (const auto& [path, text] : defaultPackFiles())
        if (!fs::exists(dir / path)) writeText(dir / path, std::string(text) + "\n");
    for (const auto& [path, oldText] : retiredDefaults())
        if (readText(dir / path) == oldText + "\n") {
            std::error_code ec;
            fs::remove(dir / path, ec);
        }
    for (const auto& [path, oldText] : upgradableDefaults())
        if (readText(dir / path) == oldText + "\n")  // untouched since an older version
            for (const auto& [newPath, text] : defaultPackFiles())
                if (newPath == path) writeText(dir / path, std::string(text) + "\n");
    Json overworld;
    std::string err;
    if (Json::parse(readText(dir / "dimensions" / "overworld.json"), overworld, err)) {
        Json ores = overworld["ores"].size() ? overworld["ores"] : Json::array();
        bool changed = false;
        Json defaults;
        for (const auto& [path, text] : defaultPackFiles())
            if (path == "dimensions/overworld.json") Json::parse(text, defaults, err);
        for (size_t i = 0; i < defaults["ores"].size(); ++i) {
            const Json& d = defaults["ores"][i];
            bool present = false;
            for (size_t k = 0; k < ores.size(); ++k) present |= ores[k]["block"].str() == d["block"].str();
            if (!present) {
                ores.push(d);
                changed = true;
            }
        }
        if (changed) {
            overworld.set("ores", ores);
            writeText(dir / "dimensions" / "overworld.json", overworld.dump() + "\n");
        }
    }
}

void PackManager::resetDefaultPack() {
    for (const auto& [path, text] : defaultPackFiles()) writeText(fs::path(root_) / "default" / path, std::string(text) + "\n");
}

std::vector<std::string> PackManager::enabledOrder() const {
    Json j;
    std::string err;
    if (!fs::exists(statePath_) || !Json::parse(readText(statePath_), j, err)) return {"default"};
    std::vector<std::string> order;
    for (size_t i = 0; i < j["enabled"].size(); ++i) order.push_back(j["enabled"][i].str(""));
    return order;
}

void PackManager::saveOrder(const std::vector<std::string>& order) {
    Json j = Json::object(), arr = Json::array();
    for (const std::string& id : order) arr.push(Json::string(id));
    j.set("enabled", arr);
    writeText(statePath_, j.dump() + "\n");
}

std::vector<PackInfo> PackManager::list() const {
    std::vector<PackInfo> packs;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(root_, ec)) {
        if (!e.is_directory()) continue;
        PackInfo p;
        p.id = e.path().filename().string();
        p.dir = e.path().string();
        p.builtin = p.id == "default";
        Json m;
        std::string err;
        Json::parse(readText(e.path() / "manifest.json"), m, err);
        // Our own manifest, or the "header" block of a Bedrock style manifest.
        p.name = m["name"].str(m["header"]["name"].str(p.id));
        p.description = m["description"].str(m["header"]["description"].str(""));
        p.version = m["version"].str("");
        packs.push_back(p);
    }
    std::vector<std::string> order = enabledOrder();
    for (PackInfo& p : packs) p.enabled = std::find(order.begin(), order.end(), p.id) != order.end();
    auto rank = [&](const PackInfo& p) {
        auto it = std::find(order.begin(), order.end(), p.id);
        return it == order.end() ? 1000 : (int)(it - order.begin());
    };
    std::sort(packs.begin(), packs.end(), [&](const PackInfo& a, const PackInfo& b) {
        if (rank(a) != rank(b)) return rank(a) < rank(b);
        return a.name < b.name;
    });
    return packs;
}

void PackManager::setEnabled(const std::string& id, bool enabled) {
    std::vector<std::string> order = enabledOrder();
    order.erase(std::remove(order.begin(), order.end(), id), order.end());
    if (enabled) order.push_back(id);
    saveOrder(order);
}

void PackManager::move(const std::string& id, int delta) {
    std::vector<std::string> order = enabledOrder();
    auto it = std::find(order.begin(), order.end(), id);
    if (it == order.end()) return;
    int i = (int)(it - order.begin()), j = std::clamp(i + delta, 0, (int)order.size() - 1);
    std::swap(order[i], order[j]);
    saveOrder(order);
}

std::string PackManager::createPack(const std::string& name) {
    std::string base = slug(name), id = base;
    for (int n = 2; fs::exists(fs::path(root_) / id); ++n) id = base + "_" + std::to_string(n);
    for (const auto& [path, text] : defaultPackFiles()) {
        std::string content = text;
        if (path == "manifest.json") {
            Json m = Json::object();
            m.set("name", Json::string(name));
            m.set("description", Json::string("Vlastní balíček vytvořený ve hře"));
            m.set("version", Json::string("1.0.0"));
            m.set("format", Json::number(1));
            content = m.dump();
        }
        writeText(fs::path(root_) / id / path, content + "\n");
    }
    return id;
}

void PackManager::deletePack(const std::string& id) {
    if (id.empty() || id == "default" || id.find('/') != std::string::npos || id.find("..") != std::string::npos) return;
    std::error_code ec;
    fs::remove_all(fs::path(root_) / id, ec);
    setEnabled(id, false);
}

std::vector<std::string> PackManager::files(const std::string& id) const {
    std::vector<std::string> out;
    fs::path dir = fs::path(root_) / id;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_regular_file() && it->path().extension() == ".json")
            out.push_back(fs::relative(it->path(), dir, ec).generic_string());
    }
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        // manifest first, then folders in a fixed order
        auto key = [](const std::string& p) { return (p == "manifest.json" ? "0" : "1") + p; };
        return key(a) < key(b);
    });
    return out;
}

std::string PackManager::readFile(const std::string& id, const std::string& path) const {
    return readText(fs::path(root_) / id / path);
}

bool PackManager::writeFile(const std::string& id, const std::string& path, const std::string& text) {
    if (path.find("..") != std::string::npos) return false;
    return writeText(fs::path(root_) / id / path, text);
}

GenConfig PackManager::buildConfig(std::vector<std::string>* w) const {
    Json overworld = Json::object(), nether = Json::object();
    std::vector<std::string> biomeOrder;
    std::map<std::string, Json> biomes;
    for (const std::string& id : enabledOrder()) {
        fs::path dir = fs::path(root_) / id;
        if (!fs::exists(dir)) continue;
        for (const char* dim : {"overworld", "nether"}) {
            fs::path f = dir / "dimensions" / (std::string(dim) + ".json");
            Json j;
            if (!fs::exists(f) || !parseFile(f, j, w, id + "/dimensions/" + dim + ".json")) continue;
            Json& target = std::string(dim) == "overworld" ? overworld : nether;
            for (const std::string& k : j.keys()) target.set(k, j[k]);
        }
        for (const fs::path& f : jsonFiles(dir / "biomes")) {
            Json b;
            if (!parseFile(f, b, w, id + "/biomes/" + f.filename().string())) continue;
            std::string bid = b["id"].str(f.stem().string());
            if (!b["enabled"].boolean(true)) {
                biomes.erase(bid);
                biomeOrder.erase(std::remove(biomeOrder.begin(), biomeOrder.end(), bid), biomeOrder.end());
                continue;
            }
            if (!b.has("id")) b.set("id", Json::string(bid));
            if (!biomes.count(bid)) biomeOrder.push_back(bid);
            biomes[bid] = b;
        }
    }
    Json doc = Json::object(), list = Json::array();
    for (const std::string& id : biomeOrder) list.push(biomes[id]);
    doc.set("overworld", overworld);
    doc.set("nether", nether);
    doc.set("biomes", list);
    doc.set("generation", Json::number(GenConfig::defaults().generation));  // new worlds get the newest generator
    return GenConfig::fromJson(doc, w);
}

RecipeBook PackManager::buildRecipes(std::vector<std::string>* w) const {
    RecipeBook book;
    bool any = false;
    for (const std::string& id : enabledOrder())
        for (const fs::path& f : jsonFiles(fs::path(root_) / id / "recipes")) {
            Json j;
            if (!parseFile(f, j, w, id + "/recipes/" + f.filename().string())) continue;
            book.addJson(j, w);
            any = true;
        }
    // With no recipe files at all, keep the built-in ones so survival stays playable.
    return any ? book : RecipeBook::defaults();
}

std::string PackManager::shaderPackDir() const {
    std::string out;
    for (const std::string& id : enabledOrder())
        if (fs::is_directory(fs::path(root_) / id / "shaders")) out = (fs::path(root_) / id).string();
    return out;
}

std::vector<std::string> PackManager::textureDirs() const {
    std::vector<std::string> out;
    for (const std::string& id : enabledOrder()) {
        fs::path t = fs::path(root_) / id / "textures";
        if (fs::is_directory(t)) out.push_back(t.string());
    }
    return out;
}

}  // namespace vc

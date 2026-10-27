#include "Save.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace vc {

namespace fs = std::filesystem;

namespace {

using KeyValues = std::map<std::string, std::string>;

KeyValues readKeyValues(const std::string& path) {
    KeyValues kv;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return kv;
}

bool writeFileAtomic(const std::string& path, const std::string& content) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out << content;
        if (!out.flush()) return false;
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    return !ec;
}

float num(const KeyValues& kv, const char* key, float def) {
    auto it = kv.find(key);
    return it == kv.end() ? def : std::strtof(it->second.c_str(), nullptr);
}

std::string oneLine(std::string s) {
    std::replace(s.begin(), s.end(), '\n', ' ');
    std::replace(s.begin(), s.end(), '\r', ' ');
    return s;
}

}  // namespace

int64_t unixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint32_t seedFromText(const std::string& text) {
    std::string t = text;
    t.erase(0, t.find_first_not_of(' '));
    t.erase(t.find_last_not_of(' ') + 1);
    if (t.empty()) {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        return (uint32_t)(now ^ (now >> 32)) | 1u;
    }
    char* end = nullptr;
    long long v = std::strtoll(t.c_str(), &end, 10);
    if (end && *end == '\0') return (uint32_t)v;
    int32_t h = 0;
    for (unsigned char c : t) h = 31 * h + c;
    return (uint32_t)h;
}

std::vector<WorldInfo> listWorlds(const std::string& root) {
    std::vector<WorldInfo> worlds;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;
        WorldInfo info;
        PlayerState ignored;
        if (loadLevel(entry.path().string(), info, ignored)) worlds.push_back(info);
    }
    std::sort(worlds.begin(), worlds.end(), [](const WorldInfo& a, const WorldInfo& b) { return a.lastPlayed > b.lastPlayed; });
    return worlds;
}

std::string createWorldFolder(const std::string& root) {
    std::error_code ec;
    fs::create_directories(root, ec);
    for (int64_t n = unixNow();; ++n) {
        fs::path p = fs::path(root) / ("world_" + std::to_string(n));
        if (!fs::exists(p, ec)) {
            fs::create_directories(p, ec);
            return p.string();
        }
    }
}

bool saveLevel(const WorldInfo& info, const PlayerState& p) {
    std::ostringstream o;
    o << "version=1\n";
    o << "name=" << oneLine(info.name) << "\n";
    o << "seed=" << info.seed << "\n";
    o << "mode=" << (int)info.mode << "\n";
    o << "lastPlayed=" << info.lastPlayed << "\n";
    o << "showCoordinates=" << (info.showCoordinates ? 1 : 0) << "\nkeepInventory=" << (info.keepInventory ? 1 : 0)
      << "\nallowCommands=" << (info.allowCommands ? 1 : 0) << "\nexp.oreRedesign=" << (info.expOreRedesign ? 1 : 0)
      << "\nexp.flowingLiquids=" << (info.expFlowingLiquids ? 1 : 0) << "\nexp.explorationMaps=" << (info.expExplorationMaps ? 1 : 0) << "\n";
    if (p.valid) {
        o << "player.x=" << p.pos.x << "\nplayer.y=" << p.pos.y << "\nplayer.z=" << p.pos.z << "\n";
        o << "spawn.x=" << p.spawn.x << "\nspawn.y=" << p.spawn.y << "\nspawn.z=" << p.spawn.z << "\n";
        o << "yaw=" << p.yaw << "\npitch=" << p.pitch << "\ndimension=" << p.dimension << "\n";
        o << "health=" << p.health << "\nfood=" << p.food << "\nsaturation=" << p.saturation << "\nair=" << p.air << "\n";
        o << "flying=" << (p.flying ? 1 : 0) << "\nselected=" << p.selectedSlot << "\n";
        o << "inventory=";
        for (int i = 0; i < kInventorySize; ++i)
            o << (i ? "," : "") << (int)p.inventory.slots[i].id << ":" << (int)p.inventory.slots[i].count << ":"
              << p.inventory.slots[i].damage;
        if (p.hasDeath) o << "\ndeath=" << p.deathX << ":" << p.deathZ;
        o << "\nplaces=";
        for (size_t i = 0; i < p.structures.size(); ++i) o << (i ? "," : "") << p.structures[i].first << ":" << p.structures[i].second;
        o << "\ntime=" << (long long)p.dayTime << "\nraining=" << (p.raining ? 1 : 0) << "\nweatherTimer=" << p.weatherTimer
          << "\nbosses=" << p.bossesDefeated << "\narenas=";
        for (size_t i = 0; i < p.bossArenas.size(); ++i) o << (i ? "," : "") << p.bossArenas[i].first << ":" << p.bossArenas[i].second;
        if (p.hasShowcase) o << "\nshowcase=" << p.showcaseX << ":" << p.showcaseY << ":" << p.showcaseZ;
        if (p.documentsRead) o << "\npages=" << p.documentsRead;
        o << "\narmor=";
        for (int i = 0; i < 4; ++i) o << (i ? "," : "") << (int)p.armor[i].id << ":" << (int)p.armor[i].count << ":" << p.armor[i].damage;
        if (!p.offhand.empty()) o << "\noffhand=" << (int)p.offhand.id << ":" << (int)p.offhand.count << ":" << p.offhand.damage;
        o << "\n";
    }
    return writeFileAtomic(info.folder + "/level.dat", o.str());
}

bool loadLevel(const std::string& folder, WorldInfo& info, PlayerState& p) {
    KeyValues kv = readKeyValues(folder + "/level.dat");
    if (!kv.count("seed")) return false;
    info.folder = folder;
    info.name = kv["name"];
    info.seed = (uint32_t)std::strtoul(kv["seed"].c_str(), nullptr, 10);
    info.mode = num(kv, "mode", 0) == 1 ? GameMode::Survival : GameMode::Creative;
    info.lastPlayed = std::strtoll(kv["lastPlayed"].c_str(), nullptr, 10);
    info.showCoordinates = num(kv, "showCoordinates", 0) != 0;
    info.keepInventory = num(kv, "keepInventory", 0) != 0;
    info.allowCommands = num(kv, "allowCommands", 0) != 0;
    info.expOreRedesign = num(kv, "exp.oreRedesign", 0) != 0;
    info.expFlowingLiquids = num(kv, "exp.flowingLiquids", 0) != 0;
    info.expExplorationMaps = num(kv, "exp.explorationMaps", 0) != 0;

    p = PlayerState{};
    if (!kv.count("player.x")) return true;
    p.valid = true;
    p.pos = Vec3(num(kv, "player.x", 0), num(kv, "player.y", 80), num(kv, "player.z", 0));
    p.spawn = Vec3(num(kv, "spawn.x", 0), num(kv, "spawn.y", 80), num(kv, "spawn.z", 0));
    p.yaw = num(kv, "yaw", 0);
    p.pitch = num(kv, "pitch", 0);
    p.dimension = (int)num(kv, "dimension", 0);
    p.health = num(kv, "health", 20);
    p.food = num(kv, "food", 20);
    p.saturation = num(kv, "saturation", 5);
    p.air = num(kv, "air", 15);
    p.flying = num(kv, "flying", 0) != 0;
    p.selectedSlot = std::clamp((int)num(kv, "selected", 0), 0, kHotbarSize - 1);
    std::istringstream inv(kv["inventory"]);
    std::string slot;
    for (int i = 0; i < kInventorySize && std::getline(inv, slot, ','); ++i) {
        int id = 0, count = 0, damage = 0;
        if (std::sscanf(slot.c_str(), "%d:%d:%d", &id, &count, &damage) >= 2 && id > 0 && id < kItemIdLimit && count > 0)
            p.inventory.slots[i] = {(ItemId)id, (uint8_t)std::min(count, maxStack((ItemId)id)), (uint16_t)std::max(0, damage)};
    }
    if (kv.count("death") && std::sscanf(kv["death"].c_str(), "%d:%d", &p.deathX, &p.deathZ) == 2) p.hasDeath = true;
    p.dayTime = std::max(0.0, (double)num(kv, "time", 1000));
    p.raining = num(kv, "raining", 0) != 0;
    p.weatherTimer = std::clamp((float)num(kv, "weatherTimer", 600), 1.0f, 3600.0f);
    p.bossesDefeated = std::max(0, (int)num(kv, "bosses", 0));
    p.documentsRead = std::clamp((int)num(kv, "pages", 0), 0, 255);
    if (kv.count("showcase") && std::sscanf(kv["showcase"].c_str(), "%d:%d:%d", &p.showcaseX, &p.showcaseY, &p.showcaseZ) == 3)
        p.hasShowcase = true;
    std::istringstream arenas(kv["arenas"]);
    while (std::getline(arenas, slot, ',') && p.bossArenas.size() < 64) {
        int x, z;
        if (std::sscanf(slot.c_str(), "%d:%d", &x, &z) == 2) p.bossArenas.emplace_back(x, z);
    }
    std::istringstream places(kv["places"]);
    while (std::getline(places, slot, ',') && p.structures.size() < 16) {
        int x, z;
        if (std::sscanf(slot.c_str(), "%d:%d", &x, &z) == 2) p.structures.emplace_back(x, z);
    }
    std::istringstream arm(kv["armor"]);
    for (int i = 0; i < 4 && std::getline(arm, slot, ','); ++i) {
        int id = 0, count = 0, damage = 0;
        // Only the right piece in each slot (a broken file must not put a sword on the head).
        if (std::sscanf(slot.c_str(), "%d:%d:%d", &id, &count, &damage) >= 2 && armorPiece((ItemId)id) == i && count > 0)
            p.armor[i] = {(ItemId)id, 1, (uint16_t)std::max(0, damage)};
    }
    if (int id = 0, count = 0, damage = 0; std::sscanf(kv["offhand"].c_str(), "%d:%d:%d", &id, &count, &damage) >= 2 && count > 0 && id > 0)
        p.offhand = {(ItemId)id, (uint8_t)std::min(count, maxStack((ItemId)id)), (uint16_t)std::max(0, damage)};
    return true;
}

namespace {
void writeStack(std::ostream& o, const ItemStack& s) { o << " " << (int)s.id << " " << (int)s.count << " " << s.damage; }
bool readStack(std::istream& in, ItemStack& s) {
    int id = 0, count = 0, damage = 0;
    if (!(in >> id >> count >> damage)) return false;
    s = (id > 0 && id < kItemIdLimit && count > 0) ? ItemStack{(ItemId)id, (uint8_t)std::min(count, maxStack((ItemId)id)), (uint16_t)damage}
                                          : ItemStack{};
    return true;
}
}  // namespace

bool saveGenerator(const std::string& folder, const GenConfig& config) {
    return writeFileAtomic(folder + "/generator.json", config.toJson().dump() + "\n");
}

GenConfig loadGenerator(const std::string& folder) {
    std::ifstream in(folder + "/generator.json");
    if (!in) return GenConfig::defaults();
    std::stringstream ss;
    ss << in.rdbuf();
    Json doc;
    std::string err;
    if (!Json::parse(ss.str(), doc, err)) return GenConfig::defaults();
    GenConfig c = GenConfig::fromJson(doc);
    if (!doc.has("crimson_desert")) c.crimsonDesert = 1;  // made before 1.5.1: keep its desert as it was
    if (!doc.has("generation")) c.generation = 1;         // made before 1.5.7: no new caves or structures
    if (!c.flat) addMissingOres(c);  // only chunks generated from now on contain them
    upgradeNether(c);
    retireBiomes(c);
    return c;
}

bool saveFurnaces(const std::string& folder, const std::map<FurnaceKey, Furnace>& furnaces) {
    std::ostringstream o;
    for (const auto& [k, f] : furnaces) {
        if (f.empty() && !f.burning()) continue;
        o << k.dim << " " << k.x << " " << k.y << " " << k.z;
        writeStack(o, f.input);
        writeStack(o, f.fuel);
        writeStack(o, f.output);
        o << " " << f.burnLeft << " " << f.burnTotal << " " << f.cook << "\n";
    }
    return writeFileAtomic(folder + "/furnaces.dat", o.str());
}

std::map<FurnaceKey, Furnace> loadFurnaces(const std::string& folder) {
    std::map<FurnaceKey, Furnace> furnaces;
    std::ifstream in(folder + "/furnaces.dat");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        FurnaceKey k;
        Furnace f;
        if (ls >> k.dim >> k.x >> k.y >> k.z && readStack(ls, f.input) && readStack(ls, f.fuel) && readStack(ls, f.output) &&
            ls >> f.burnLeft >> f.burnTotal >> f.cook)
            furnaces[k] = f;
    }
    return furnaces;
}

bool saveChests(const std::string& folder, const std::map<FurnaceKey, std::array<ItemStack, 27>>& chests) {
    std::ostringstream o;
    for (const auto& [k, c] : chests) {
        o << k.dim << " " << k.x << " " << k.y << " " << k.z;
        for (const ItemStack& s : c) writeStack(o, s);
        o << "\n";
    }
    return writeFileAtomic(folder + "/chests.dat", o.str());
}

std::map<FurnaceKey, std::array<ItemStack, 27>> loadChests(const std::string& folder) {
    std::map<FurnaceKey, std::array<ItemStack, 27>> chests;
    std::ifstream in(folder + "/chests.dat");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        FurnaceKey k;
        std::array<ItemStack, 27> c{};
        if (!(ls >> k.dim >> k.x >> k.y >> k.z)) continue;
        bool ok = true;
        for (ItemStack& s : c) ok = ok && readStack(ls, s);
        if (ok) chests[k] = c;
    }
    return chests;
}

void deleteWorld(const std::string& folder) {
    std::error_code ec;
    fs::remove_all(folder, ec);
}

Settings loadSettings(const std::string& path) {
    KeyValues kv = readKeyValues(path);
    Settings s;
    auto flag = [&](const char* key, bool def) { return num(kv, key, def ? 1.0f : 0.0f) != 0; };
    s.sensitivity = std::clamp(num(kv, "sensitivity", s.sensitivity), 0.25f, 3.0f);
    s.invertY = flag("invertY", s.invertY);
    s.autoJump = flag("autoJump", s.autoJump);
    s.vibrate = flag("vibrate", s.vibrate);
    s.lefty = flag("lefty", s.lefty);
    s.buttonScale = std::clamp(num(kv, "buttonScale", s.buttonScale), 0.6f, 1.6f);
    s.guiScale = std::clamp((int)num(kv, "guiScale", (float)s.guiScale), 0, 6);
    s.brightness = std::clamp(num(kv, "brightness", s.brightness), 0.6f, 1.5f);
    s.fov = std::clamp(num(kv, "fov", s.fov), 50.0f, 110.0f);
    s.renderDistance = std::clamp((int)num(kv, "renderDistance", (float)s.renderDistance), 2, 12);
    s.hideGui = flag("hideGui", s.hideGui);
    s.clouds = flag("clouds", s.clouds);
    s.cameraMode = std::clamp((int)num(kv, "cameraMode", 0), 0, 2);
    s.viewBobbing = flag("viewBobbing", s.viewBobbing);
    s.showMap = flag("showMap", s.showMap);
    s.graphics = num(kv, "graphics", 1) == 0 ? GraphicsMode::Classic : GraphicsMode::Enhanced;
    s.shadows = flag("shadows", s.shadows);
    s.waves = flag("waves", s.waves);
    s.batterySaver = std::clamp((int)num(kv, "batterySaver", 1), 0, 2);
    s.musicVolume = std::clamp(num(kv, "musicVolume", s.musicVolume), 0.0f, 1.0f);
    s.soundVolume = std::clamp(num(kv, "soundVolume", s.soundVolume), 0.0f, 1.0f);
    s.uiSounds = flag("uiSounds", s.uiSounds);
    if (auto it = kv.find("language"); it != kv.end()) s.language = it->second;
    s.devGodMode = flag("devGodMode", false);
    s.devFly = flag("devFly", false);
    s.devInstantMine = flag("devInstantMine", false);
    s.devDebug = flag("devDebug", false);
    s.devNoclip = flag("devNoclip", false);
    s.devSpeed = std::clamp(num(kv, "devSpeed", 1.0f), 1.0f, 4.0f);
    s.expDayNight = flag("expDayNight", false);
    s.expFastFurnace = flag("expFastFurnace", false);
    s.expOreMap = flag("expOreMap", false);
    s.expFastGrowth = flag("expFastGrowth", false);
    s.expLowGravity = flag("expLowGravity", false);
    s.expBigBoom = flag("expBigBoom", false);
    s.calmMode = flag("calmMode", false);
    s.tutorialStep = (int)num(kv, "tutorialStep", 0);
    s.mpPvp = flag("mpPvp", false);
    s.mpProtectSpawn = flag("mpProtectSpawn", true);
    return s;
}

void saveSettings(const std::string& path, const Settings& s) {
    std::ostringstream o;
    o << "sensitivity=" << s.sensitivity << "\ninvertY=" << s.invertY << "\nautoJump=" << s.autoJump
      << "\nvibrate=" << s.vibrate << "\nlefty=" << s.lefty << "\nbuttonScale=" << s.buttonScale
      << "\nguiScale=" << s.guiScale << "\nbrightness=" << s.brightness << "\nfov=" << s.fov
      << "\nrenderDistance=" << s.renderDistance << "\nhideGui=" << s.hideGui << "\nclouds=" << s.clouds << "\ncameraMode=" << s.cameraMode
      << "\nviewBobbing=" << s.viewBobbing << "\nmusicVolume=" << s.musicVolume << "\nsoundVolume=" << s.soundVolume
      << "\nuiSounds=" << s.uiSounds << "\nlanguage=" << s.language << "\nshowMap=" << s.showMap << "\ngraphics=" << (int)s.graphics << "\nshadows=" << s.shadows << "\nwaves=" << s.waves << "\nbatterySaver=" << s.batterySaver
      << "\ndevGodMode=" << s.devGodMode << "\ndevFly=" << s.devFly << "\ndevInstantMine=" << s.devInstantMine
      << "\ndevDebug=" << s.devDebug << "\ndevNoclip=" << s.devNoclip << "\ndevSpeed=" << s.devSpeed
      << "\nexpDayNight=" << s.expDayNight << "\nexpFastFurnace=" << s.expFastFurnace << "\nexpOreMap=" << s.expOreMap << "\nexpFastGrowth=" << s.expFastGrowth << "\nexpLowGravity=" << s.expLowGravity
      << "\nexpBigBoom=" << s.expBigBoom << "\ncalmMode=" << s.calmMode << "\ntutorialStep=" << s.tutorialStep << "\nmpPvp=" << s.mpPvp << "\nmpProtectSpawn=" << s.mpProtectSpawn
      << "\n";
    writeFileAtomic(path, o.str());
}

}  // namespace vc

#include "Commands.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "Blocks.h"
#include "Items.h"
#include "Lang.h"
#include "Session.h"
#include "Sky.h"
#include "Trains.h"

namespace vc {

const CommandInfo kCommands[] = {
    {"/help", "Seznam příkazů", false},
    {"/list", "Hráči ve hře", false},
    {"/seed", "Seed světa", false},
    {"/pos", "Kde právě jsi", false},
    {"/gamemode <survival|creative>", "Změní herní režim", true},
    {"/tp <x> <y> <z> | <hráč>", "Teleport (~ = od tvé pozice)", true},
    {"/give <věc> [počet]", "Dá ti věc (např. ruby, iron_ingot)", true},
    {"/time [day|noon|evening|night|midnight|tiky]", "Denní doba", true},
    {"/weather <clear|rain>", "Počasí", true},
    {"/summon <pig|sheep|slime|forgotten|crimson|husking|...>", "Přivolá bytost", true},
    {"/locate <fortress|arena>", "Najde pevnost (dimenze úhelného kamene) nebo arénu (Crimson Void)", false},
    {"/spawn", "Teleport na spawn", true},
    {"/heal", "Doplní zdraví a jídlo", true},
    {"/clear", "Vyprázdní inventář", true},
    {"/kill", "Zemřeš (jen v Přežití)", true},
    {"/rail <straight|curve|cross|stop|line|remove> [north|east|south|west|počet]", "Položí kolejový díl tam, kde stojíš", true},
    {"/train <spawn|go|stop|reverse|ride|remove> [vozy|rychlost]", "Vlak: vytvoř (steam boxcar passenger), jeď, zastav, otoč, nastup, odstraň", true},
};
const int kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

namespace {

std::vector<std::string> split(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// "12", "-3.5", "~", "~4" (relative to `base`). False if it is not a coordinate.
bool coordinate(const std::string& s, float base, float& out) {
    if (s.empty()) return false;
    bool rel = s[0] == '~';
    std::string num = rel ? s.substr(1) : s;
    float v = 0;
    if (!num.empty()) {
        char* end = nullptr;
        v = std::strtof(num.c_str(), &end);
        if (!end || *end || !std::isfinite(v)) return false;
    }
    out = rel ? base + v : v;
    return true;
}

std::string fmt(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", v);
    return b;
}

}  // namespace

std::vector<std::string> runCommand(Session& s, const std::string& line, bool cheats) {
    std::vector<std::string> args = split(line);
    if (args.empty() || args[0].size() < 2 || args[0][0] != '/') return {tr("Neznámý příkaz. Napiš /help")};
    const std::string cmd = lower(args[0].substr(1));
    auto info = [&](const std::string& name) -> const CommandInfo* {
        for (int i = 0; i < kCommandCount; ++i) {
            std::string u = kCommands[i].usage;
            if (u.substr(1, u.find(' ') == std::string::npos ? std::string::npos : u.find(' ') - 1) == name) return &kCommands[i];
        }
        return nullptr;
    };
    const CommandInfo* ci = info(cmd == "gm" ? "gamemode" : cmd);
    if (!ci) return {trf("Neznámý příkaz: /{0}. Napiš /help", {cmd})};
    if (ci->cheat && !cheats)
        return {tr("Příkazy nejsou v tomto světě povolené (zapni Povolit příkazy v úpravě světa)")};
    auto usage = [&] { return std::vector<std::string>{trf("Použití: {0}", {tr(ci->usage)})}; };
    Player& p = s.player();

    if (cmd == "help") {
        std::vector<std::string> out = {tr("Příkazy:")};
        for (int i = 0; i < kCommandCount; ++i)
            if (cheats || !kCommands[i].cheat) out.push_back(tr(kCommands[i].usage) + " - " + tr(kCommands[i].description));
        return out;
    }
    if (cmd == "seed") return {trf("Seed: {0}", {std::to_string((int32_t)s.info().seed)})};
    if (cmd == "pos") {
        std::string where = trf("Pozice: {0} {1} {2}", {fmt(p.pos.x), fmt(p.pos.y), fmt(p.pos.z)});
        std::string place = s.currentWorld().placeName((int)std::floor(p.pos.x), (int)std::floor(p.pos.z));
        if (!place.empty()) where += " · " + tr(place);
        return {where};
    }
    if (cmd == "gamemode" || cmd == "gm") {
        if (args.size() < 2) return usage();
        std::string m = lower(args[1]);
        GameMode mode;
        if (m == "survival" || m == "s" || m == "0" || m == "prezitie" || m == "přežití") mode = GameMode::Survival;
        else if (m == "creative" || m == "c" || m == "1" || m == "kreativni" || m == "kreativní") mode = GameMode::Creative;
        else return usage();
        s.setMode(mode);
        if (mode == GameMode::Survival) p.flying = false;
        return {trf("Herní režim: {0}", {tr(mode == GameMode::Survival ? "Přežití" : "Kreativní")})};
    }
    if (cmd == "tp") {
        if (args.size() == 2) {
            // To another player of a LAN game.
            for (const RemoteAvatar& a : s.remotePlayers)
                if (lower(a.name) == lower(args[1])) {
                    if (a.dim != s.dimension()) return {trf("{0} je v jiné dimenzi", {a.name})};
                    s.teleport(a.pose.feet + Vec3(0, 0.1f, 0));
                    return {trf("Teleport k hráči {0}", {a.name})};
                }
            return {trf("Hráč {0} tu není", {args[1]})};
        }
        float x, y, z;
        if (args.size() < 4 || !coordinate(args[1], p.pos.x, x) || !coordinate(args[2], p.pos.y, y) ||
            !coordinate(args[3], p.pos.z, z))
            return usage();
        // Plain whole numbers mean the middle of that block, like the original.
        if (args[1].find('.') == std::string::npos && args[1][0] != '~') x += 0.5f;
        if (args[3].find('.') == std::string::npos && args[3][0] != '~') z += 0.5f;
        y = std::clamp(y, 1.0f, (float)kChunkHeight + 32);
        if (std::fabs(x) > 3e7f || std::fabs(z) > 3e7f) return usage();
        s.teleport(Vec3(x, y, z));
        return {trf("Teleport na {0} {1} {2}", {fmt(x), fmt(y), fmt(z)})};
    }
    if (cmd == "give") {
        if (args.size() < 2) return usage();
        std::string key = lower(args[1]);
        if (key.rfind("minecraft:", 0) == 0) key = key.substr(10);
        ItemId id = itemFromKey(key);
        if (id == AIR && std::all_of(key.begin(), key.end(), ::isdigit) && key.size() < 6) {
            int n = std::atoi(key.c_str());
            if (n > 0 && n < kItemIdLimit && itemKey((ItemId)n) && *itemKey((ItemId)n)) id = (ItemId)n;
        }
        if (id == AIR) return {trf("Neznámá věc: {0}", {args[1]})};
        int count = args.size() > 2 ? std::atoi(args[2].c_str()) : 1;
        count = std::clamp(count, 1, 64 * 9);
        int max = maxStack(id);
        for (int left = count; left > 0;) {
            int n = std::min(left, max);
            s.giveItem({id, (uint8_t)n});
            left -= n;
        }
        return {trf("Dostal(a) jsi {0}x {1}", {std::to_string(count), tr(itemName(id))})};
    }
    if (cmd == "time") {
        if (args.size() < 2) {
            float t = (float)std::fmod(s.timeOfDay(), 24000.0);
            return {trf("Den {0} · {1} ({2})", {std::to_string((long long)(s.timeOfDay() / 24000) + 1), tr(dayPhaseName(dayPhase(t))),
                                                std::to_string((int)t)})};
        }
        std::string a = lower(args[1] == "set" && args.size() > 2 ? args[2] : args[1]);
        int ticks;
        if (a == "day" || a == "morning") ticks = 1000;
        else if (a == "noon") ticks = 6000;
        else if (a == "evening") ticks = 12000;
        else if (a == "night") ticks = 14000;
        else if (a == "midnight") ticks = 18000;
        else if (!a.empty() && std::all_of(a.begin(), a.end(), ::isdigit) && a.size() < 9) ticks = std::atoi(a.c_str()) % 24000;
        else return usage();
        s.setTimeOfDay((float)ticks);
        return {trf("Čas nastaven na {0}", {std::to_string(ticks)})};
    }
    if (cmd == "weather") {
        if (args.size() < 2) return usage();
        std::string a = lower(args[1]);
        if (a == "clear") s.setWeather(false);
        else if (a == "rain" || a == "snow") s.setWeather(true);
        else return usage();
        return {tr(a == "clear" ? "Jasno" : "Začalo pršet (nebo sněžit)")};
    }
    if (cmd == "summon") {
        if (args.size() < 2) return usage();
        std::string a = lower(args[1]);
        MobType type;
        if (a == "pig") type = MobType::Pig;
        else if (a == "sheep") type = MobType::Sheep;
        else if (a == "slime" || a == "pumpkin_slime") type = MobType::PumpkinSlime;
        else if (a == "forgotten" || a == "forgotten_man" || a == "zombie") type = MobType::ForgottenMan;
        else if (a == "crimson" || a == "crimson_man" || a == "boss") type = MobType::CrimsonMan;
        else if (a == "chicken") type = MobType::Chicken;
        else if (a == "cow") type = MobType::Cow;
        else if (a == "villager") type = MobType::Villager;
        else if (a == "spider") type = MobType::Spider;
        else if (a == "bat") type = MobType::Bat;
        else if (a == "tnt_zombie") type = MobType::TntZombie;
        else if (a == "tar" || a == "tar_monster") type = MobType::TarMonster;
        else if (a == "husking") type = MobType::Husking;
        else if (a == "hogman" || a == "hog_man") type = MobType::Hogman;
        else if (a == "celestling") type = MobType::Celestling;
        else if (a == "reach_stalker" || a == "stalker") type = MobType::ReachStalker;
        else if (a == "sky_drifter" || a == "drifter") type = MobType::SkyDrifter;
        else return usage();
        s.summon(type);
        return {trf("Přivoláno: {0}", {tr(mobInfo(type).name)})};
    }
    if (cmd == "locate") {
        int x, y, z;
        if (!s.currentWorld().nearestStructure((int)std::floor(p.pos.x), (int)std::floor(p.pos.z), x, y, z))
            return {tr("Pevnosti jsou v dimenzi úhelného kamene, aréna v Crimson Void")};
        return {trf("Nejbližší: {0} {1} {2} ({3} bloků)", {std::to_string(x), std::to_string(y), std::to_string(z),
                                                         std::to_string((int)std::hypot(x - p.pos.x, z - p.pos.z))})};
    }
    if (cmd == "spawn") {
        s.teleportToSpawn();
        return {tr("Teleport na spawn")};
    }
    if (cmd == "heal") {
        s.healAndFeed();
        return {tr("Zdraví a jídlo doplněny")};
    }
    if (cmd == "clear") {
        s.inventory().clear();
        return {tr("Inventář vyprázdněn")};
    }
    if (cmd == "kill") {
        if (!s.survival()) return {tr("V Kreativním režimu nelze zemřít")};
        s.kill();
        return {};
    }
    if (cmd == "rail" || cmd == "train") {
        // VoxelCraft Trains: everything happens where the player stands, looking along `heading`.
        TrainSystem& ts = s.trains();
        const int dim = s.dimIndex();
        const int x = (int)std::floor(p.pos.x), y = (int)std::floor(p.pos.y + 0.05f), z = (int)std::floor(p.pos.z);
        const float hx = std::sin(p.yaw), hz = -std::cos(p.yaw);
        const int heading = std::fabs(hx) > std::fabs(hz) ? (hx > 0 ? 1 : 3) : (hz < 0 ? 0 : 2);
        const int kDx[4] = {0, 1, 0, -1}, kDz[4] = {-1, 0, 1, 0};
        if (args.size() < 2) return usage();
        const std::string sub = lower(args[1]);
        if (cmd == "rail") {
            auto dirArg = [&](size_t i, int def) {
                if (args.size() <= i) return def;
                const std::string d = lower(args[i]);
                return d == "north" ? 0 : d == "east" ? 1 : d == "south" ? 2 : d == "west" ? 3 : def;
            };
            std::string why;
            if (sub == "remove") return {ts.removePiece(dim, x, y, z) ? "OK" : tr("Tady není kolej")};
            if (sub == "line") {
                int n = args.size() > 2 ? std::clamp(std::atoi(args[2].c_str()), 1, 64) : 8, done = 0;
                for (int i = 0; i < n; ++i)
                    if (ts.placePiece(s.world(), dim, x + kDx[heading] * i, y, z + kDz[heading] * i, PieceKind::Straight, heading, &why)) ++done;
                    else break;
                return {trf("Položeno kolejí: {0}", {std::to_string(done)}) + (done < n && !why.empty() ? " (" + tr(why) + ")" : "")};
            }
            PieceKind kind;
            int dir = heading;
            if (sub == "straight") kind = PieceKind::Straight, dir = dirArg(2, heading);
            else if (sub == "curve") kind = PieceKind::Curve, dir = dirArg(2, heading);  // ports: dir and the next one clockwise
            else if (sub == "cross") kind = PieceKind::Cross;
            else if (sub == "stop") kind = PieceKind::Stop, dir = dirArg(2, (heading + 2) & 3);  // the side the track comes from
            else return usage();
            if (!ts.placePiece(s.world(), dim, x, y, z, kind, dir, &why)) return {tr(why)};
            return {"OK"};
        }
        // /train
        auto kindOf = [](const std::string& w, CarKind& k) {
            const std::string a = lower(w);
            if (a == "steam" || a == "s") k = CarKind::Steam;
            else if (a == "boxcar" || a == "b") k = CarKind::Boxcar;
            else if (a == "passenger" || a == "p") k = CarKind::Passenger;
            else return false;
            return true;
        };
        if (sub == "spawn") {
            std::vector<CarKind> cars;
            for (size_t i = 2; i < args.size() && cars.size() < 8; ++i) {
                CarKind k;
                if (!kindOf(args[i], k)) return {trf("Neznámý vůz: {0} (steam, boxcar, passenger)", {args[i]})};
                cars.push_back(k);
            }
            if (cars.empty()) cars = {CarKind::Steam, CarKind::Boxcar, CarKind::Passenger};
            std::string why;
            int idx = ts.spawnTrain(dim, x, y, z, heading, cars, &why);
            if (idx < 0) return {tr(why.empty() ? "Tady není kolej" : why)};
            return {why.empty() ? std::string("OK") : tr(why)};
        }
        int idx = ts.nearestTrain(dim, p.pos, 24.0f);
        if (idx < 0) return {tr("Poblíž není žádný vlak")};
        if (sub == "go") {
            ts.setTarget(idx, args.size() > 2 ? (float)std::atof(args[2].c_str()) : 6.0f);
            return {"OK"};
        }
        if (sub == "stop") { ts.setTarget(idx, 0); return {"OK"}; }
        if (sub == "reverse") { ts.reverse(idx); return {"OK"}; }
        if (sub == "ride") { s.rideTrain(idx); return {"OK"}; }
        if (sub == "remove") {
            if (s.trainRide() == idx) s.rideTrain(-1);
            else if (s.trainRide() > idx) s.rideTrain(s.trainRide() - 1);
            ts.removeTrain(idx);
            return {"OK"};
        }
        return usage();
    }
    return {trf("Neznámý příkaz: /{0}. Napiš /help", {cmd})};  // /list is answered by the game
}

}  // namespace vc

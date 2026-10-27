#include "Rewards.h"

#include <algorithm>
#include <cctype>
#include <ctime>

#include "Blocks.h"
#include "Items.h"
#include "Lang.h"
#include "Store.h"

namespace vc {

namespace {

const char* const kAddons[] = {"addon:winter", "addon:halloween", "addon:anniversary"};
const char* const kWheelSkins[] = {"skin:knight", "skin:robot", "skin:astronaut", "skin:nether_walker"};
constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";  // no 0/O or 1/I to mix up

uint32_t mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

std::string checksum(const std::string& body) {
    uint32_t h = 0x1E7Au;
    for (char c : body) h = mix(h ^ (uint8_t)c);
    std::string out;
    out += kAlphabet[h % 32];
    out += kAlphabet[(h >> 5) % 32];
    return out;
}

void grantFor(Profile& p, const std::string& key, long long now, long long seconds) {
    long long& u = p.until[key];
    u = std::max(u, now) + seconds;
}

}  // namespace

long long nowSeconds() { return (long long)std::time(nullptr); }

int dayNumber(int ymd) {
    std::tm tm{};
    tm.tm_year = ymd / 10000 - 1900;
    tm.tm_mon = (ymd / 100) % 100 - 1;
    tm.tm_mday = ymd % 100;
    tm.tm_hour = 12;
    return (int)(timegm(&tm) / 86400);
}

const DailyReward& dailyReward(int day) {
    static const DailyReward kDays[7] = {
        {50, 0, {}, false},
        {75, 0, {{APPLE, 5}, {COOKED_PORK, 3}}, false},
        {100, 0, {}, false},
        {50, 1, {}, false},
        {150, 0, {{RUBY, 3}, {STEEL_INGOT, 4}}, false},
        {200, 0, {{TNT, 4}, {OAK_SAPLING, 4}}, false},
        {400, 1, {}, true},
    };
    return kDays[std::clamp(day, 1, 7) - 1];
}

int nextRewardDay(const Profile& p, int today) {
    if (p.lastRewardDay == today) return 0;
    // Yesterday continues the streak; a missed day starts it again.
    bool continues = p.lastRewardDay && dayNumber(today) - dayNumber(p.lastRewardDay) == 1;
    return continues ? p.rewardStreak % 7 + 1 : 1;
}

std::string claimDailyReward(Profile& p, int today, long long now, uint32_t seed) {
    int day = nextRewardDay(p, today);
    if (!day) return "";
    const DailyReward& r = dailyReward(day);
    p.rewardStreak = day;
    p.lastRewardDay = today;
    int coins = r.coins * (isVip(p, now) ? 2 : 1);
    p.coins += coins;
    p.bonusSpins += r.spins;
    for (auto& it : r.items) p.pendingItems.push_back(it);
    std::string text = trf("Den {0}: +{1} mincí", {std::to_string(day), std::to_string(coins)});
    if (r.spins) text += ", " + tr("točení navíc");
    if (!r.items.empty()) text += ", " + tr("věci do příštího světa");
    if (r.addon) {
        const char* a = kAddons[mix(seed) % 3];
        grantFor(p, a, now, 8 * 3600);
        text += ", " + trf("doplněk na 8 hodin: {0}", {tr(addonName(a))});
    }
    return text;
}

const std::vector<WheelSegment>& wheelSegments() {
    static const std::vector<WheelSegment> s = {
        {WheelPrize::Coins10, "10 mincí", 24, {0.55f, 0.45f, 0.2f}},
        {WheelPrize::Coins50, "50 mincí", 20, {0.2f, 0.45f, 0.75f}},
        {WheelPrize::Skin, "Skin zdarma", 6, {0.75f, 0.3f, 0.6f}},
        {WheelPrize::Coins100, "100 mincí", 14, {0.25f, 0.6f, 0.3f}},
        {WheelPrize::Addon, "Doplněk 8 h", 8, {0.3f, 0.55f, 0.75f}},
        {WheelPrize::Coins250, "250 mincí", 7, {0.7f, 0.35f, 0.15f}},
        {WheelPrize::Vip, "VIP 24 h", 3, {0.85f, 0.7f, 0.1f}},
        {WheelPrize::Code, "Výroční kód", 4, {0.8f, 0.2f, 0.25f}},
        {WheelPrize::Coins500, "500 mincí", 3, {0.45f, 0.2f, 0.7f}},
    };
    return s;
}

int spinsAvailable(const Profile& p, int today, long long now) {
    int daily = 1 + (isVip(p, now) ? 1 : 0);
    int used = p.lastSpinDay == today ? p.spinsUsed : 0;
    return std::max(0, daily - used) + p.bonusSpins;
}

int spinWheel(Profile& p, int today, long long now, uint32_t seed, std::string& message, std::string& code) {
    if (spinsAvailable(p, today, now) <= 0) return -1;
    if (p.lastSpinDay != today) {
        p.lastSpinDay = today;
        p.spinsUsed = 0;
    }
    int daily = 1 + (isVip(p, now) ? 1 : 0);
    if (p.spinsUsed < daily) ++p.spinsUsed;
    else --p.bonusSpins;

    const auto& segs = wheelSegments();
    int total = 0;
    for (const WheelSegment& s : segs) total += s.weight;
    int pick = (int)(mix(seed) % (uint32_t)total), index = 0;
    for (size_t i = 0; i < segs.size(); ++i) {
        if (pick < segs[i].weight) {
            index = (int)i;
            break;
        }
        pick -= segs[i].weight;
    }
    code.clear();
    switch (segs[index].prize) {
        case WheelPrize::Coins10: p.coins += 10; message = trf("Vyhrál(a) jsi {0} mincí!", {"10"}); break;
        case WheelPrize::Coins50: p.coins += 50; message = trf("Vyhrál(a) jsi {0} mincí!", {"50"}); break;
        case WheelPrize::Coins100: p.coins += 100; message = trf("Vyhrál(a) jsi {0} mincí!", {"100"}); break;
        case WheelPrize::Coins250: p.coins += 250; message = trf("Vyhrál(a) jsi {0} mincí!", {"250"}); break;
        case WheelPrize::Coins500: p.coins += 500; message = trf("Vyhrál(a) jsi {0} mincí!", {"500"}); break;
        case WheelPrize::Vip:
            grantFor(p, "vip", now, 24 * 3600);
            message = tr("Vyhrál(a) jsi VIP na 24 hodin!");
            break;
        case WheelPrize::Skin: {
            std::vector<const char*> missing;
            for (const char* s : kWheelSkins)
                if (!p.owns(s)) missing.push_back(s);
            if (missing.empty()) {
                p.coins += 300;
                message = tr("Všechny skiny už máš: +300 mincí!");
            } else {
                const char* s = missing[mix(seed * 31) % missing.size()];
                p.owned.insert(s);
                message = tr("Vyhrál(a) jsi nový skin!");
            }
            break;
        }
        case WheelPrize::Addon: {
            const char* a = kAddons[mix(seed * 17) % 3];
            grantFor(p, a, now, 8 * 3600);
            message = trf("Doplněk {0} na 8 hodin!", {tr(addonName(a))});
            break;
        }
        case WheelPrize::Code:
            code = makeAnniversaryCode(seed);
            p.codes.push_back(code);
            if (p.codes.size() > 32) p.codes.erase(p.codes.begin());
            message = trf("Výroční kód: {0}", {code});
            break;
        default: break;
    }
    return index;
}

std::string makeAnniversaryCode(uint32_t seed) {
    std::string body;
    uint32_t h = mix(seed ^ 0xB1D7u);
    for (int i = 0; i < 4; ++i) {
        body += kAlphabet[h % 32];
        h = mix(h + i);
    }
    return "VC1Y-" + body + "-" + checksum(body);
}

bool validCode(const std::string& raw) {
    std::string c;
    for (char ch : raw)
        if (!std::isspace((unsigned char)ch)) c += (char)std::toupper((unsigned char)ch);
    if (c.size() != 12 || c.compare(0, 5, "VC1Y-") != 0 || c[9] != '-') return false;
    return checksum(c.substr(5, 4)) == c.substr(10, 2);
}

bool redeemCode(Profile& p, const std::string& code, std::string& message) {
    if (!validCode(code)) {
        message = tr("Neplatný kód");
        return false;
    }
    if (p.owns("addon:anniversary")) {
        message = tr("Výroční balíček už máš");
        return false;
    }
    p.owned.insert("addon:anniversary");
    p.addonsOff.erase("addon:anniversary");
    message = tr("Výroční balíček odemčen!");
    return true;
}

bool isVip(const Profile& p, long long now) {
    // 1.5.9: the developer account tests everything; nothing is written to its purchases.
    if (p.isDeveloper()) return true;
    if (p.owns("vip")) return true;
    auto it = p.until.find("vip");
    return it != p.until.end() && it->second > now;
}

bool addonOwned(const Profile& p, const std::string& id, long long now) {
    if (p.isDeveloper()) return true;  // 1.5.9: developer testing access, separate from purchases
    if (p.owns(id)) return true;
    auto it = p.until.find(id);
    return it != p.until.end() && it->second > now;
}

std::set<std::string> activeAddons(const Profile& p, long long now) {
    std::set<std::string> out;
    for (const char* a : kAddons)
        if (addonOwned(p, a, now) && !p.addonsOff.count(a)) out.insert(std::string(a).substr(6));
    return out;
}

}  // namespace vc

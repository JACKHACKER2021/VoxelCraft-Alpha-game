#include "Profile.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

#include "Achievements.h"
#include "Json.h"

namespace vc {

namespace {
constexpr int kDailyBonus = 50;
constexpr int kBlocksPerCoin = 10;
}  // namespace

int todayYmd() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    return (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
}

int Profile::claimDailyBonus(int today) {
    if (today == lastBonusDay) return 0;
    lastBonusDay = today;
    coins += kDailyBonus;
    return kDailyBonus;
}

int Profile::unlock(const std::string& id) {
    const Achievement* a = findAchievement(id);
    if (!a || !achievements.insert(id).second) return 0;
    coins += a->coins;
    return a->coins;
}

int Profile::countBlock() {
    if (++blocksTowardsCoin < kBlocksPerCoin) return 0;
    blocksTowardsCoin = 0;
    ++coins;
    return 1;
}

Profile Profile::load(const std::string& path) {
    Profile p;
    std::ifstream in(path);
    if (!in) return p;
    std::stringstream ss;
    ss << in.rdbuf();
    Json j;
    std::string err;
    if (!Json::parse(ss.str(), j, err)) return p;
    p.name = j["name"].str(p.name).substr(0, 24);
    p.skin = j["skin"].str(p.skin);
    p.developer = j["developer"].boolean(false);
    p.coins = std::max(0, (int)j["coins"].num(p.coins));
    p.lastBonusDay = (int)j["last_bonus_day"].num(0);
    p.lastGiftDay = (int)j["last_gift_day"].num(0);
    p.blocksTowardsCoin = (int)j["blocks_towards_coin"].num(0);
    for (size_t i = 0; i < j["owned"].size(); ++i) p.owned.insert(j["owned"][i].str(""));
    for (size_t i = 0; i < j["achievements"].size(); ++i) p.achievements.insert(j["achievements"][i].str(""));
    p.blocksMined = std::max(0, (int)j["blocks_mined"].num(0));
    p.rewardStreak = std::clamp((int)j["reward_streak"].num(0), 0, 7);
    p.lastRewardDay = (int)j["last_reward_day"].num(0);
    p.lastSpinDay = (int)j["last_spin_day"].num(0);
    p.spinsUsed = std::max(0, (int)j["spins_used"].num(0));
    p.bonusSpins = std::clamp((int)j["bonus_spins"].num(0), 0, 99);
    const Json& until = j["until"];
    for (const std::string& k : until.keys()) p.until[k] = (long long)until[k].num(0);
    for (size_t i = 0; i < j["addons_off"].size(); ++i) p.addonsOff.insert(j["addons_off"][i].str(""));
    for (size_t i = 0; i < j["pending_items"].size() && i < 64; ++i)
        p.pendingItems.emplace_back((int)j["pending_items"][i][0].num(0), (int)j["pending_items"][i][1].num(0));
    for (size_t i = 0; i < j["codes"].size() && i < 32; ++i) p.codes.push_back(j["codes"][i].str(""));
    return p;
}

bool Profile::save(const std::string& path) const {
    Json j = Json::object(), ownedList = Json::array();
    j.set("name", Json::string(name));
    j.set("skin", Json::string(skin));
    if (developer) j.set("developer", Json::fromBool(true));
    j.set("coins", Json::number(coins));
    j.set("last_bonus_day", Json::number(lastBonusDay));
    j.set("last_gift_day", Json::number(lastGiftDay));
    j.set("blocks_towards_coin", Json::number(blocksTowardsCoin));
    for (const std::string& o : owned) ownedList.push(Json::string(o));
    j.set("owned", ownedList);
    Json ach = Json::array();
    for (const std::string& a : achievements) ach.push(Json::string(a));
    j.set("achievements", ach);
    j.set("blocks_mined", Json::number(blocksMined));
    j.set("reward_streak", Json::number(rewardStreak));
    j.set("last_reward_day", Json::number(lastRewardDay));
    j.set("last_spin_day", Json::number(lastSpinDay));
    j.set("spins_used", Json::number(spinsUsed));
    j.set("bonus_spins", Json::number(bonusSpins));
    Json u = Json::object();
    for (auto& [k, v] : until) u.set(k, Json::number((double)v));
    j.set("until", u);
    Json off = Json::array();
    for (const std::string& a : addonsOff) off.push(Json::string(a));
    j.set("addons_off", off);
    Json pend = Json::array();
    for (auto& [id, n] : pendingItems) {
        Json e = Json::array();
        e.push(Json::number(id));
        e.push(Json::number(n));
        pend.push(e);
    }
    j.set("pending_items", pend);
    Json cl = Json::array();
    for (const std::string& c : codes) cl.push(Json::string(c));
    j.set("codes", cl);
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out << j.dump() << "\n";
        if (!out.flush()) return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace vc

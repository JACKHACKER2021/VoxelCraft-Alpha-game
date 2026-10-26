#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace vc {

// The local player profile (profile.json): name, skin, coins and what was
// bought in the VoxelCraft store. Coins are earned by playing only.
struct Profile {
    std::string name = "Hráč";
    std::string skin = "explorer";
    int coins = 250;
    std::set<std::string> owned = {"skin:explorer", "skin:miner", "skin:gardener"};
    int lastBonusDay = 0;  // yyyymmdd of the last daily bonus
    int lastGiftDay = 0;   // yyyymmdd of the last Christmas present (Winter addon)
    int blocksTowardsCoin = 0;
    std::set<std::string> achievements;  // unlocked achievement ids
    int blocksMined = 0;                  // in survival, for the miner achievements
    bool developer = false;  // unlocked with the developer login (see Developer.h)
    // 1.2.0: daily rewards, the wheel of fortune, VIP and addons (see Rewards.h).
    int rewardStreak = 0;        // 1..7: the day of the reward track claimed last
    int lastRewardDay = 0;       // yyyymmdd of the last claimed daily reward
    int lastSpinDay = 0, spinsUsed = 0, bonusSpins = 0;
    std::map<std::string, long long> until;  // temporary unlocks ("vip", "addon:winter", "skin:robot") -> unix seconds
    std::set<std::string> addonsOff;          // owned addons the player switched off
    std::vector<std::pair<int, int>> pendingItems;  // item id, count: given in the next world
    std::vector<std::string> codes;           // anniversary codes won on the wheel
    static constexpr const char* kDeveloperName = "Hoppy_Hopsy21";
    // Developer tools are on only for the developer account's name.
    bool isDeveloper() const { return developer && name == kDeveloperName; }

    bool owns(const std::string& item) const { return owned.count(item) > 0; }
    // Gives the daily bonus once per calendar day. Returns the coins added.
    int claimDailyBonus(int today);
    // Counts a broken block; every 10 blocks earn a coin. Returns coins added.
    int countBlock();
    // Unlocks an achievement once and pays its coins. Returns the coins (0 if it was already unlocked).
    int unlock(const std::string& id);

    static Profile load(const std::string& path);
    bool save(const std::string& path) const;
};

int todayYmd();

}  // namespace vc

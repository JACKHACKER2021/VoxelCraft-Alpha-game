#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Profile.h"

namespace vc {

// Everything the store gives away: the 7-day daily rewards, the wheel of fortune,
// VIP and the addons (Winter, Halloween, Anniversary), bought or won for a while.
// Like the rest of the store, only coins earned in the game are ever involved.

constexpr int kVipPrice = 3000;

// Days since 1970 for a yyyymmdd date (to tell "yesterday" apart from a gap).
int dayNumber(int ymd);

struct DailyReward {
    int coins = 0;
    int spins = 0;                               // extra spins of the wheel
    std::vector<std::pair<int, int>> items;      // item id, count (given in the next world)
    bool addon = false;                          // a random addon for 8 hours
};
const DailyReward& dailyReward(int day);         // day 1..7
// Which day of the track today's claim would be (1..7), or 0 if it was claimed today.
int nextRewardDay(const Profile& p, int today);
// Claims today's reward; returns a short description (Czech key with values), or "" if already claimed.
std::string claimDailyReward(Profile& p, int today, long long now, uint32_t seed);

enum class WheelPrize { Coins10, Coins50, Coins100, Coins250, Coins500, Vip, Skin, Addon, Code, Count };
struct WheelSegment {
    WheelPrize prize;
    const char* label;  // Czech key
    int weight;
    float color[3];
};
const std::vector<WheelSegment>& wheelSegments();
int spinsAvailable(const Profile& p, int today, long long now);
// Spins the wheel (uses a spin). Returns the segment index (-1 if no spin was left) and
// describes the prize in `message`; a won anniversary code is returned in `code`.
int spinWheel(Profile& p, int today, long long now, uint32_t seed, std::string& message, std::string& code);

// Anniversary codes: "VC1Y-ABCD-EF", where the last part checks the rest.
std::string makeAnniversaryCode(uint32_t seed);
bool validCode(const std::string& code);
// Redeems a code typed in the store; false with a message when it is not valid.
bool redeemCode(Profile& p, const std::string& code, std::string& message);

bool isVip(const Profile& p, long long now);
// "winter", "halloween", "anniversary": owned (and switched on) or won for a while.
std::set<std::string> activeAddons(const Profile& p, long long now);
bool addonOwned(const Profile& p, const std::string& id, long long now);  // id: "addon:winter"
long long nowSeconds();

}  // namespace vc

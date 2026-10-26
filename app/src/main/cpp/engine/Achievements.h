#pragma once
#include <string>

namespace vc {

// Achievements: goals in survival worlds that pay bonus coins once each.
struct Achievement {
    const char* id;
    const char* name;         // Czech key of the translation table
    const char* description;
    int coins;
};

extern const Achievement kAchievements[];
extern const int kAchievementCount;
const Achievement* findAchievement(const std::string& id);

}  // namespace vc

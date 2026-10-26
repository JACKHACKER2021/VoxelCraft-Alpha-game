#pragma once
#include <string>
#include <vector>

#include "Packs.h"
#include "Profile.h"

namespace vc {

// The VoxelCraft store: our own skins, texture packs and mod packs, bought
// with coins earned in the game. Nothing here costs real money.
struct StoreItem {
    enum class Kind { Skin, Textures, ModPack, Vip, Addon };
    std::string id;  // "skin:robot", "textures:night", "pack:ohnivy_nether"
    Kind kind;
    std::string title, banner, tag, description;
    int price = 0;
    float rating = 0;
    float color[3] = {0.2f, 0.2f, 0.2f};  // banner colour
};

const std::vector<StoreItem>& storeCatalog();
// Display names of the addons ("addon:winter" -> "Zimní doplněk").
const char* addonName(const std::string& id);

// Installs a bought item (skins only need to be owned). Returns false with
// a message when files could not be written.
bool installStoreItem(const StoreItem& item, PackManager& packs, std::string& error);

// 1.5.1: the developer account (signed in with its password, see Developer.h; a changed nickname
// alone is not enough) tests every modpack, texture pack and skin without spending coins.
bool developerFree(const StoreItem& item, const Profile& profile);

// Tries to buy: checks coins, installs, records ownership and takes the coins (none for developerFree).
enum class BuyResult { Bought, AlreadyOwned, NotEnoughCoins, Failed };
BuyResult buyStoreItem(const StoreItem& item, Profile& profile, PackManager& packs, std::string& error);

}  // namespace vc

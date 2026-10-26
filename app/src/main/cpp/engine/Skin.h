#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vc {

// A player skin in the common 64x64 layout (64x32 legacy skins are expanded).
// Body parts are boxes whose six faces are laid out in the usual net:
// for a box at (u, v) with size (w, h, d):
//   top    (u+d, v)          w x d     bottom (u+d+w, v)      w x d
//   right  (u, v+d)          d x h     front  (u+d, v+d)      w x h
//   left   (u+d+w, v+d)      d x h     back   (u+2d+w, v+d)   w x h
struct Skin {
    static constexpr int kSize = 64;
    std::string id, name;
    std::vector<uint8_t> rgba;  // 64 * 64 * 4
    bool slim = false;          // 3 pixel wide arms
    bool developerOnly = false; // shown and usable only with developer access
    bool vipOnly = false;       // for VIP players (see Rewards.h)

    bool valid() const { return rgba.size() == (size_t)kSize * kSize * 4; }
    // Loads a PNG from disk; accepts 64x64 and 64x32.
    static bool loadPng(const std::string& path, Skin& out, std::string& error);
    // Converts raw RGBA of 64x32 or 64x64 into a 64x64 skin.
    static bool fromRgba(const uint8_t* rgba, int w, int h, Skin& out);
};

struct SkinPart {
    // Texture origin and box size in skin pixels.
    int u, v, w, h, d;
};

// The classic body parts (base layer) and their overlay (hat, jacket, ...).
namespace skinparts {
constexpr SkinPart kHead{0, 0, 8, 8, 8}, kHat{32, 0, 8, 8, 8};
constexpr SkinPart kBody{16, 16, 8, 12, 4}, kJacket{16, 32, 8, 12, 4};
constexpr SkinPart kRightArm{40, 16, 4, 12, 4}, kRightSleeve{40, 32, 4, 12, 4};
constexpr SkinPart kLeftArm{32, 48, 4, 12, 4}, kLeftSleeve{48, 48, 4, 12, 4};
constexpr SkinPart kRightLeg{0, 16, 4, 12, 4}, kRightPants{0, 32, 4, 12, 4};
constexpr SkinPart kLeftLeg{16, 48, 4, 12, 4}, kLeftPants{0, 48, 4, 12, 4};
}  // namespace skinparts

// Our own built-in skins, drawn procedurally (no files from other games).
std::vector<Skin> builtinSkins();
// Hoppy_Hopsy21's own skin, for the developer account only.
Skin developerSkin();

}  // namespace vc

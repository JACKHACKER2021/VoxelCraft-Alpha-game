#pragma once
#include <functional>
#include <vector>

#include "Math.h"
#include "Mesher.h"
#include "Skin.h"
#include "Blocks.h"

namespace vc {

struct PlayerPose {
    Vec3 feet;
    float yaw = 0, headPitch = 0;
    float swing = 0;  // limb swing angle in radians (walking animation)
    bool slim = false;
    float attack = 1;        // 0..1 progress of a strike with the right arm (1 = at rest)
    float place = 1;         // 0..1 progress of a placing push
    ItemId held = 0;        // item in the right hand (0 = empty)
    ItemId armor[4] = {};   // worn helmet, chestplate, leggings, boots
    bool riding = false;     // sitting in a boat
    ItemId offhand = 0;      // 1.5.7: a shield or torch in the left hand (0 = none shown)
    bool sneaking = false;   // 1.5.7: crouched, the shield raised in front
};

// Builds the textured boxes of a player model (base layer plus the slightly
// larger overlay layer) in world space. UVs address a single 64x64 skin texture.
std::vector<Vertex> buildPlayerModel(const PlayerPose& pose);
// What the player wears and holds, textured from the terrain atlas: armour pieces
// over the body and the item in the right hand.
std::vector<Vertex> buildPlayerExtras(const PlayerPose& pose);

// Appends one skin-textured box given in model pixels (lo..hi, grown by `grow` for overlay
// layers). `map` turns model-pixel positions into output positions (world or view space).
// Faces get the classic shading times `light`. Shared by the player model and the
// first-person arm, so both use the same skin layout.
void appendSkinBox(std::vector<Vertex>& out, const SkinPart& part, Vec3 lo, Vec3 hi, float grow, float light,
                   const std::function<Vec3(const Vec3&)>& map);

}  // namespace vc

#pragma once
#include "Math.h"
#include "World.h"
#include "Blocks.h"

namespace vc {

struct RayHit {
    bool hit = false;
    int x, y, z;     // block that was hit
    int nx, ny, nz;  // face normal, i.e. where a new block would go
    float fy = 0.5f;  // height of the hit point inside the hit block (0 bottom .. 1 top)
    int faceY = 0;    // shaped blocks: +1/-1 when the ray hit the top/bottom of a box (a slab's top)
};

// Voxel traversal (Amanatides & Woo). Water is ignored like in the original.
RayHit raycast(const World& world, const Vec3& origin, const Vec3& dir, float maxDist);

class Player {
public:
    static constexpr float kWidth = 0.6f, kHeight = 1.8f, kEyeHeight = 1.62f;
    static constexpr float kReach = 6.0f;  // Pocket Edition reach distance

    Vec3 pos;  // feet position
    Vec3 vel;
    float yaw = 0, pitch = 0;  // radians
    bool onGround = false;
    bool inWater = false;
    bool inLava = false;
    bool inTar = false;      // 1.4.0: deep cave tar, very slow
    bool headInWater = false;  // eyes under the surface: underwater fog
    bool headInLava = false;
    bool headInTar = false;
    bool flying = false;       // creative flight
    bool autoJump = false;     // hop up one-block steps automatically
    float fallDistance = 0;    // blocks fallen since last on the ground
    float landedFall = 0;      // set for one update when landing, for fall damage
    float speedMul = 1;     // developer speed
    float gravityMul = 1;   // the low gravity experiment
    bool noclip = false;    // developer: flying through blocks
    float walked = 0;          // horizontal distance moved by the last update

    Vec3 eye() const { return pos + Vec3(0, kEyeHeight, 0); }
    Vec3 forward() const;
    bool touches(const World& world, ItemId block) const;  // e.g. cactus
    bool collides(const World& world, const Vec3& p) const;  // the body at `p` overlaps a block

    // moveX/moveZ are joystick axes in [-1, 1] relative to the view direction.
    // `descend` only matters while flying.
    void update(const World& world, float dt, float moveX, float moveZ, bool jump, bool descend = false);
    bool eyeInWater(const World& world) const;
    // True if a block at (x, y, z) would overlap the player.
    bool intersectsBlock(int x, int y, int z) const;
    // True if any block the player overlaps has the given id.
    bool touchesBlock(const World& world, ItemId id) const;

private:
    // Returns true if a block stopped the movement.
    bool moveAxis(const World& world, int axis, float amount);
    bool canStep_ = false;  // on the ground this frame: may walk up half a block
};

}  // namespace vc

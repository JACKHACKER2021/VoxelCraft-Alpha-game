#include "Player.h"

#include "BlockShapes.h"

#include <algorithm>
#include <cmath>

#include "Blocks.h"

namespace vc {

RayHit raycast(const World& world, const Vec3& origin, const Vec3& dir, float maxDist) {
    RayHit r;
    int x = (int)std::floor(origin.x), y = (int)std::floor(origin.y), z = (int)std::floor(origin.z);
    int stepX = dir.x > 0 ? 1 : -1, stepY = dir.y > 0 ? 1 : -1, stepZ = dir.z > 0 ? 1 : -1;
    auto boundary = [](float o, float d, int cell, int step) {
        if (d == 0) return INFINITY;
        float next = step > 0 ? cell + 1 - o : o - cell;
        return next / std::fabs(d);
    };
    float tMaxX = boundary(origin.x, dir.x, x, stepX);
    float tMaxY = boundary(origin.y, dir.y, y, stepY);
    float tMaxZ = boundary(origin.z, dir.z, z, stepZ);
    float tDeltaX = dir.x != 0 ? 1 / std::fabs(dir.x) : INFINITY;
    float tDeltaY = dir.y != 0 ? 1 / std::fabs(dir.y) : INFINITY;
    float tDeltaZ = dir.z != 0 ? 1 / std::fabs(dir.z) : INFINITY;
    int nx = 0, ny = 0, nz = 0;
    float t = 0;

    while (t <= maxDist) {
        ItemId id = world.getBlock(x, y, z);
        if (isTargetable(id)) {
            if (!isShaped(id)) {
                r = {true, x, y, z, nx, ny, nz, std::clamp(origin.y + dir.y * t - y, 0.0f, 1.0f)};
                return r;
            }
            // Shaped blocks are hit only on their boxes (an open door lets the ray through).
            Box boxes[10];
            int n = blockBoxes(world, x, y, z, id, false, boxes);
            float bestT = INFINITY;
            int bnx = 0, bny = 0, bnz = 0;
            for (int i = 0; i < n; ++i) {
                const float lo[3] = {x + boxes[i].x0, y + boxes[i].y0, z + boxes[i].z0};
                const float hi[3] = {x + boxes[i].x1, y + boxes[i].y1, z + boxes[i].z1};
                const float o[3] = {origin.x, origin.y, origin.z}, d[3] = {dir.x, dir.y, dir.z};
                float t0 = 0, t1 = maxDist;
                int axis = -1;
                bool hit = true;
                for (int k = 0; k < 3 && hit; ++k) {
                    if (std::fabs(d[k]) < 1e-9f) {
                        hit = o[k] >= lo[k] && o[k] <= hi[k];
                        continue;
                    }
                    float a = (lo[k] - o[k]) / d[k], b = (hi[k] - o[k]) / d[k];
                    if (a > b) std::swap(a, b);
                    if (a > t0) {
                        t0 = a;
                        axis = k;
                    }
                    t1 = std::min(t1, b);
                    hit = t0 <= t1;
                }
                if (hit && t0 < bestT) {
                    bestT = t0;
                    bnx = axis == 0 ? (d[0] > 0 ? -1 : 1) : 0;
                    bny = axis == 1 ? (d[1] > 0 ? -1 : 1) : 0;
                    bnz = axis == 2 ? (d[2] > 0 ? -1 : 1) : 0;
                }
            }
            if (bestT < INFINITY) {
                // The new-block side stays the cell face the ray entered through.
                r = {true, x, y, z, nx, ny, nz};
                if (nx == 0 && ny == 0 && nz == 0) r = {true, x, y, z, bnx, bny, bnz};
                r.fy = std::clamp(origin.y + dir.y * bestT - y, 0.0f, 1.0f);
                r.faceY = bny;
                return r;
            }
        }
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX; t = tMaxX; tMaxX += tDeltaX; nx = -stepX; ny = nz = 0;
        } else if (tMaxY < tMaxZ) {
            y += stepY; t = tMaxY; tMaxY += tDeltaY; ny = -stepY; nx = nz = 0;
        } else {
            z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; nz = -stepZ; nx = ny = 0;
        }
    }
    return r;
}

Vec3 Player::forward() const {
    return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
}

bool Player::collides(const World& world, const Vec3& p) const {
    if (noclip && flying) return false;
    const float h = kWidth / 2;
    int x0 = (int)std::floor(p.x - h), x1 = (int)std::floor(p.x + h - 1e-4f);
    int y0 = (int)std::floor(p.y), y1 = (int)std::floor(p.y + kHeight - 1e-4f);
    int z0 = (int)std::floor(p.z - h), z1 = (int)std::floor(p.z + h - 1e-4f);
    // Fences are 1.5 blocks high, so look one block lower too.
    for (int x = x0; x <= x1; ++x)
        for (int y = y0 - 1; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z) {
                ItemId id = world.getBlock(x, y, z);
                if (!isSolid(id)) continue;
                if (!isShaped(id)) {
                    if (y >= y0) return true;
                    continue;
                }
                // Doors, fences and the like: only their boxes are in the way.
                Box boxes[10];
                int n = blockBoxes(world, x, y, z, id, true, boxes);
                for (int i = 0; i < n; ++i) {
                    const Box& b = boxes[i];
                    if (p.x + h > x + b.x0 && p.x - h < x + b.x1 && p.z + h > z + b.z0 && p.z - h < z + b.z1 &&
                        p.y + kHeight > y + b.y0 && p.y < y + b.y1)
                        return true;
                }
            }
    return false;
}

bool Player::touches(const World& world, ItemId block) const {
    const float h = kWidth / 2 + 0.05f;
    for (int x = (int)std::floor(pos.x - h); x <= (int)std::floor(pos.x + h); ++x)
        for (int y = (int)std::floor(pos.y - 0.05f); y <= (int)std::floor(pos.y + kHeight); ++y)
            for (int z = (int)std::floor(pos.z - h); z <= (int)std::floor(pos.z + h); ++z)
                if (world.getBlock(x, y, z) == block) return true;
    return false;
}

bool Player::intersectsBlock(int x, int y, int z) const {
    const float h = kWidth / 2;
    return pos.x + h > x && pos.x - h < x + 1 && pos.y + kHeight > y && pos.y < y + 1 &&
           pos.z + h > z && pos.z - h < z + 1;
}

bool Player::touchesBlock(const World& world, ItemId id) const {
    const float h = kWidth / 2;
    for (int x = (int)std::floor(pos.x - h); x <= (int)std::floor(pos.x + h); ++x)
        for (int y = (int)std::floor(pos.y); y <= (int)std::floor(pos.y + kHeight); ++y)
            for (int z = (int)std::floor(pos.z - h); z <= (int)std::floor(pos.z + h); ++z)
                if (world.getBlock(x, y, z) == id) return true;
    return false;
}

bool Player::moveAxis(const World& world, int axis, float amount) {
    // Small sub-steps so fast falls never tunnel through a block.
    int steps = (int)std::ceil(std::fabs(amount) / 0.25f);
    float step = steps ? amount / steps : 0;
    for (int i = 0; i < steps; ++i) {
        Vec3 next = pos;
        (axis == 0 ? next.x : axis == 1 ? next.y : next.z) += step;
        if (collides(world, next) && axis != 1 && canStep_) {
            // Walk up half-block steps (slabs, stairs) without jumping.
            Vec3 up = next;
            up.y += 0.51f;
            if (!collides(world, up) && !collides(world, pos + Vec3(0, 0.51f, 0))) {
                pos = up;
                continue;
            }
        }
        if (collides(world, next)) {
            if (axis == 1) {
                if (step < 0) onGround = true;
                vel.y = 0;
            }
            return true;
        }
        pos = next;
    }
    return false;
}

bool Player::eyeInWater(const World& world) const {
    Vec3 e = eye();
    return world.getBlock((int)std::floor(e.x), (int)std::floor(e.y), (int)std::floor(e.z)) == WATER;
}

void Player::update(const World& world, float dt, float moveX, float moveZ, bool jump, bool descend) {
    dt = std::min(dt, 0.05f);
    // Swimming counts as soon as the feet or the body are in a liquid (like the bounding
    // box test of the classic games), so swimming up lifts the feet all the way to the
    // surface instead of stopping when a single point above the feet leaves the water.
    const int bx = (int)std::floor(pos.x), bz = (int)std::floor(pos.z);
    ItemId body = world.getBlock(bx, (int)std::floor(pos.y + 0.4f), bz);
    ItemId feetIn = world.getBlock(bx, (int)std::floor(pos.y + 0.05f), bz);
    inWater = body == WATER || feetIn == WATER;
    inLava = body == LAVA || feetIn == LAVA;
    inTar = body == TAR || feetIn == TAR;
    bool inLiquid = inWater || inLava || inTar;
    // What the eyes see decides the underwater fog.
    Vec3 e = eye();
    ItemId head = world.getBlock((int)std::floor(e.x), (int)std::floor(e.y), (int)std::floor(e.z));
    headInWater = head == WATER;
    headInLava = head == LAVA;
    headInTar = head == TAR;

    float speed = (flying ? 10.0f : inTar ? 0.9f : inLava ? 1.2f : inWater ? 2.0f : 4.3f) * speedMul;
    float s = std::sin(yaw), c = std::cos(yaw);
    // Forward on the horizontal plane is (sin yaw, -cos yaw), right is (cos yaw, sin yaw).
    vel.x = (s * moveZ + c * moveX) * speed;
    vel.z = (-c * moveZ + s * moveX) * speed;

    if (flying) {
        vel.y = (jump ? 7.0f : 0.0f) - (descend ? 7.0f : 0.0f);
    } else if (inLiquid) {
        // Lava and tar are thick: slow to sink in and slow to climb out of.
        const bool thick = inLava || inTar;
        vel.y -= (thick ? 3.0f : 6.0f) * dt;
        vel.y = std::max(vel.y, thick ? -1.0f : -2.0f);
        if (jump) vel.y = inTar ? 1.2f : inLava ? 1.5f : 2.5f;
    } else {
        vel.y -= 28.0f * gravityMul * dt;
        vel.y = std::max(vel.y, -60.0f);
        if (jump && onGround) vel.y = 8.4f;
    }

    Vec3 before = pos;
    bool wasOnGround = onGround;
    onGround = false;
    canStep_ = wasOnGround && !flying;
    bool blocked = moveAxis(world, 0, vel.x * dt);
    blocked |= moveAxis(world, 2, vel.z * dt);
    bool moving = moveX != 0 || moveZ != 0;
    if (autoJump && blocked && wasOnGround && !flying && !inLiquid && moving) {
        // Only hop when the step is a single block with room above it.
        Vec3 dir = Vec3(vel.x, 0, vel.z).normalized();
        if (!collides(world, pos + dir * 0.35f + Vec3(0, 1.05f, 0))) vel.y = 8.4f;
    }
    // Climbing out of water or lava: once the body is above the surface the land rules
    // apply (strong gravity, jumping only from the ground), so a player floating at the
    // surface could never get up onto a shore one block higher. While pushing against
    // an edge that has room above it, swim up along it like the classic games do. This
    // also covers the moment the feet are just above the surface.
    ItemId feet = world.getBlock((int)std::floor(pos.x), (int)std::floor(pos.y - 0.9f), (int)std::floor(pos.z));
    bool atLiquid = inLiquid || isLiquid(feet);
    if (!flying && atLiquid && blocked && (jump || moving)) {
        Vec3 dir = Vec3(vel.x, 0, vel.z).normalized();
        if (!collides(world, pos + dir * 0.35f + Vec3(0, 1.05f, 0)))
            vel.y = std::max(vel.y, (inLava || feet == LAVA) ? 3.5f : 5.0f);
    }
    canStep_ = false;
    moveAxis(world, 1, vel.y * dt);
    walked = std::hypot(pos.x - before.x, pos.z - before.z);

    // Fall tracking for damage: liquids and flight break a fall.
    landedFall = 0;
    if (flying || inLiquid) {
        fallDistance = 0;
    } else if (onGround) {
        landedFall = fallDistance;
        fallDistance = 0;
    } else if (pos.y < before.y) {
        fallDistance += before.y - pos.y;
    }
    // Touching the ground ends creative flight, like in the original.
    if (flying && onGround && descend) flying = false;
}

}  // namespace vc

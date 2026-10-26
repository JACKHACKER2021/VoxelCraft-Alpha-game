#include "Portal.h"

#include <cstdlib>
#include <vector>

#include "Blocks.h"

namespace vc {

namespace {

constexpr int kMaxPortalSize = 21;
constexpr int kSearchRadius = 16;

bool tryAxis(World& world, int x, int y, int z, int ax, int az, ItemId frame, ItemId portal) {
    if (world.getBlock(x, y, z) != AIR) return false;

    // Drop to the bottom of the interior...
    int steps = 0;
    while (world.getBlock(x, y - 1, z) == AIR && ++steps < kMaxPortalSize) --y;
    if (world.getBlock(x, y - 1, z) != frame) return false;
    // ...then slide to its left edge.
    steps = 0;
    while (world.getBlock(x - ax, y, z - az) == AIR && ++steps < kMaxPortalSize) { x -= ax; z -= az; }
    if (world.getBlock(x - ax, y, z - az) != frame) return false;

    int w = 0;
    while (w <= kMaxPortalSize && world.getBlock(x + ax * w, y, z + az * w) == AIR) ++w;
    int h = 0;
    while (h <= kMaxPortalSize && world.getBlock(x, y + h, z) == AIR) ++h;
    if (w < 2 || h < 3 || w > kMaxPortalSize || h > kMaxPortalSize) return false;

    // Corners are optional, like in the original game.
    for (int i = 0; i < w; ++i) {
        int bx = x + ax * i, bz = z + az * i;
        if (world.getBlock(bx, y - 1, bz) != frame || world.getBlock(bx, y + h, bz) != frame) return false;
        for (int j = 0; j < h; ++j)
            if (world.getBlock(bx, y + j, bz) != AIR) return false;
    }
    for (int j = 0; j < h; ++j) {
        if (world.getBlock(x - ax, y + j, z - az) != frame) return false;
        if (world.getBlock(x + ax * w, y + j, z + az * w) != frame) return false;
    }

    for (int i = 0; i < w; ++i)
        for (int j = 0; j < h; ++j) world.setBlock(x + ax * i, y + j, z + az * i, portal);
    return true;
}

bool isSafeSpot(const World& world, int x, int y, int z) {
    // Room for a 4x5 frame along X plus one free block in front and behind.
    for (int dx = -1; dx <= 2; ++dx) {
        if (!isSolid(world.getBlock(x + dx, y - 1, z))) return false;
        for (int dy = 0; dy < 4; ++dy)
            for (int dz = -1; dz <= 1; ++dz)
                if (world.getBlock(x + dx, y + dy, z + dz) != AIR) return false;
    }
    return true;
}

void buildPortal(World& world, int x, int y, int z, ItemId frame, ItemId portal) {
    // Clear the space around it and add a small obsidian platform so the
    // player never steps out into lava or open air.
    for (int dx = -1; dx <= 2; ++dx)
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = 0; dy < 4; ++dy) world.setBlock(x + dx, y + dy, z + dz, AIR);
            if (!isSolid(world.getBlock(x + dx, y - 1, z + dz))) world.setBlock(x + dx, y - 1, z + dz, frame);
        }
    for (int dx = -1; dx <= 2; ++dx) {
        world.setBlock(x + dx, y - 1, z, frame);
        world.setBlock(x + dx, y + 3, z, frame);
    }
    for (int dy = 0; dy < 3; ++dy) {
        world.setBlock(x - 1, y + dy, z, frame);
        world.setBlock(x + 2, y + dy, z, frame);
        world.setBlock(x, y + dy, z, portal);
        world.setBlock(x + 1, y + dy, z, portal);
    }
}

}  // namespace

bool tryLightPortal(World& world, int x, int y, int z, bool voidPortal) {
    ItemId frame = voidPortal ? VOID_STONE : OBSIDIAN, portal = voidPortal ? VOID_PORTAL : PORTAL;
    return tryAxis(world, x, y, z, 1, 0, frame, portal) || tryAxis(world, x, y, z, 0, 1, frame, portal);
}

void collapsePortal(World& world, int x, int y, int z) {
    std::vector<int> stack = {x, y, z};
    while (!stack.empty()) {
        int cz = stack.back(); stack.pop_back();
        int cy = stack.back(); stack.pop_back();
        int cx = stack.back(); stack.pop_back();
        if (!isPortal(world.getBlock(cx, cy, cz))) continue;
        world.setBlock(cx, cy, cz, AIR);
        const int d[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (auto& o : d) stack.insert(stack.end(), {cx + o[0], cy + o[1], cz + o[2]});
    }
}

Vec3 findOrBuildPortal(World& world, int x, int z, bool voidPortal) {
    const ItemId frame = voidPortal ? VOID_STONE : OBSIDIAN, portal = voidPortal ? VOID_PORTAL : PORTAL;
    // 1) Reuse a nearby portal: pick the closest lowest portal block.
    int bestDist = 1 << 30;
    Vec3 best;
    for (int dx = -kSearchRadius; dx <= kSearchRadius; ++dx)
        for (int dz = -kSearchRadius; dz <= kSearchRadius; ++dz)
            for (int y = 1; y < kChunkHeight - 1; ++y) {
                if (world.getBlock(x + dx, y, z + dz) != portal || world.getBlock(x + dx, y - 1, z + dz) == portal) continue;
                int dist = dx * dx + dz * dz;
                if (dist < bestDist) {
                    bestDist = dist;
                    best = Vec3(x + dx + 0.5f, (float)y, z + dz + 0.5f);
                }
            }
    if (bestDist < (1 << 30)) return best;

    // 2) Find a safe place close to the target, scanning outward ring by ring.
    // The Nether and the Crimson Void are searched top to bottom for an open cave or surface.
    bool nether = world.dimension() != Dimension::Overworld;
    int yMin = nether ? world.lavaLevel() + 2 : (world.config().flat ? 4 : world.seaLevel() + 1), yMax = nether ? 110 : kChunkHeight - 6;
    for (int r = 0; r <= 8; ++r)
        for (int dx = -r; dx <= r; ++dx)
            for (int dz = -r; dz <= r; ++dz) {
                if (std::abs(dx) != r && std::abs(dz) != r) continue;
                int px = x + dx, pz = z + dz;
                if (nether) {
                    for (int y = yMin; y <= yMax; ++y)
                        if (isSafeSpot(world, px, y, pz)) { buildPortal(world, px, y, pz, frame, portal); return Vec3(px + 1.0f, (float)y, pz + 0.5f); }
                } else {
                    int y = std::max(world.skyHeight(px, pz), yMin);
                    if (isSafeSpot(world, px, y, pz)) { buildPortal(world, px, y, pz, frame, portal); return Vec3(px + 1.0f, (float)y, pz + 0.5f); }
                }
            }

    // 3) No good spot: carve one out, like the original game does.
    int y = nether ? 64 : std::max(world.skyHeight(x, z), yMin);
    buildPortal(world, x, y, z, frame, portal);
    return Vec3(x + 1.0f, (float)y, z + 0.5f);
}

}  // namespace vc

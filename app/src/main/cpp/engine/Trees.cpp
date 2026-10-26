#include "Trees.h"

#include <algorithm>
#include <cstdlib>
#include <set>

#include "BlockShapes.h"
#include "Blocks.h"
#include "World.h"

namespace vc {

namespace {

const int kDirs[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

bool freeForTree(ItemId b) { return b == AIR || isPlant(b) || isLeaves(b); }

void setIfFree(World& w, int x, int y, int z, ItemId id) {
    ItemId b = w.getBlock(x, y, z);
    if (b == AIR || (isPlant(b) && !isSapling(b))) w.setBlockLoading(x, y, z, id);
}

}  // namespace

TreeKind treeForSapling(ItemId sapling) {
    switch (sapling) {
        case PINE_SAPLING: return TreeKind::Pine;
        case VOID_SAPLING: return TreeKind::Void;
        case NETHERPERK_SAPLING: return TreeKind::Netherperk;
        default: return TreeKind::Oak;
    }
}

bool saplingCanGrowOn(ItemId sapling, ItemId ground) {
    switch (sapling) {
        case VOID_SAPLING: return ground == VOID_GRASS || ground == GRASS || ground == DIRT;
        case NETHERPERK_SAPLING: return ground == NETHER_GRASS || ground == NETHERRACK;
        default: return ground == GRASS || ground == DIRT || ground == SNOW_BLOCK;
    }
}

bool growTree(World& w, int x, int y, int z, TreeKind kind, uint32_t seed) {
    const int trunk = kind == TreeKind::Pine ? 6 + (int)(seed % 4) : 4 + (int)(seed % 3);
    if (y + trunk + 3 >= kChunkHeight) return false;
    for (int k = 1; k <= trunk + 1; ++k)
        if (!freeForTree(w.getBlock(x, y + k, z))) return false;
    const ItemId log = kind == TreeKind::Pine ? SPRUCE_LOG : kind == TreeKind::Void ? VOID_LOG
                       : kind == TreeKind::Netherperk ? NETHERPERK_LOG : LOG;
    const ItemId leaves = kind == TreeKind::Pine ? SPRUCE_LEAVES : kind == TreeKind::Void ? VOID_LEAVES
                          : kind == TreeKind::Netherperk ? NETHERPERK_CAP : LEAVES;
    w.setBlockLoading(x, y, z, AIR);
    if (w.getBlock(x, y - 1, z) == GRASS) w.setBlock(x, y - 1, z, DIRT);
    if (kind == TreeKind::Pine) {
        for (int dy = 2; dy <= trunk + 1; ++dy) {
            int fromTop = trunk + 1 - dy;
            int r = fromTop == 0 ? 0 : fromTop % 2 == 1 ? 1 : std::min(2, 1 + fromTop / 3);
            for (int dx = -r; dx <= r; ++dx)
                for (int dz = -r; dz <= r; ++dz)
                    if (!(std::abs(dx) == r && std::abs(dz) == r && r > 0)) setIfFree(w, x + dx, y + dy, z + dz, leaves);
        }
    } else if (kind == TreeKind::Netherperk) {
        const int top = y + trunk - 1;
        for (int dy = -1; dy <= 2; ++dy) {
            int rad = dy == 2 ? 1 : dy == -1 ? 3 : 3 - (dy == 1);
            for (int dx = -rad; dx <= rad; ++dx)
                for (int dz = -rad; dz <= rad; ++dz) {
                    if (dx * dx + dz * dz > rad * rad + 1) continue;
                    if (dy == -1 && dx * dx + dz * dz < (rad - 1) * (rad - 1)) continue;
                    setIfFree(w, x + dx, top + dy, z + dz, leaves);
                }
        }
    } else {
        // Oak-like crown: two wide layers and two narrow ones.
        for (int dy = trunk - 2; dy <= trunk + 1; ++dy) {
            int r = dy >= trunk ? 1 : 2;
            for (int dx = -r; dx <= r; ++dx)
                for (int dz = -r; dz <= r; ++dz) {
                    bool corner = std::abs(dx) == r && std::abs(dz) == r;
                    if (corner && (dy == trunk + 1 || ((seed >> (dx + dz + 8)) & 1))) continue;
                    setIfFree(w, x + dx, y + dy, z + dz, leaves);
                }
        }
    }
    for (int k = 0; k < trunk; ++k) w.setBlockLoading(x, y + k, z, log);
    if (kind == TreeKind::Void) w.setBlock(x, y + 1, z, EYE_LOG);
    return true;
}

bool leafSupported(const World& w, int x, int y, int z) {
    // Breadth first through connected leaves, at most 4 steps.
    struct Node { int x, y, z, d; };
    std::vector<Node> queue = {{x, y, z, 0}};
    std::set<std::array<int, 3>> seen = {{x, y, z}};
    for (size_t i = 0; i < queue.size(); ++i) {
        Node n = queue[i];
        for (auto& d : kDirs) {
            int nx = n.x + d[0], ny = n.y + d[1], nz = n.z + d[2];
            ItemId b = w.getBlock(nx, ny, nz);
            if (isLog(b)) return true;
            if (n.d < 4 && isLeaves(b) && seen.insert({nx, ny, nz}).second) queue.push_back({nx, ny, nz, n.d + 1});
        }
    }
    return false;
}

// ---------------------------------------------------------------- powerstone

bool isPowerBlock(ItemId id) {
    return isCable(id) || isSwitch(id) || isLamp(id) || id == POWERSTONE_BLOCK || isTnt(id) || (isDoor(id) && id >= IRON_DOOR);
}

namespace {

bool isSource(ItemId id) { return id == POWERSTONE_BLOCK || switchOn(id); }

bool poweredBy(const World& w, int x, int y, int z) {
    for (auto& d : kDirs) {
        ItemId b = w.getBlock(x + d[0], y + d[1], z + d[2]);
        if (isSource(b) || cableOn(b)) return true;
    }
    return false;
}

void applyConsumer(World& w, int x, int y, int z, std::vector<std::array<int, 3>>& ignite) {
    ItemId b = w.getBlock(x, y, z);
    bool on = poweredBy(w, x, y, z);
    if (isLamp(b)) {
        ItemId want = on ? POWERSTONE_LAMP_ON : POWERSTONE_LAMP;
        if (b != want) w.setBlock(x, y, z, want);
    } else if (isTnt(b) && on) {
        ignite.push_back({x, y, z});
    } else if (isDoor(b) && b >= IRON_DOOR) {
        // Iron doors open with power (both halves).
        DoorState s = doorState(b);
        if (s.open != on) {
            int lower = s.upper ? y - 1 : y;
            for (int k = 0; k < 2; ++k) {
                ItemId half = w.getBlock(x, lower + k, z);
                if (!isDoor(half)) continue;
                DoorState hs = doorState(half);
                hs.open = on;
                w.setBlock(x, lower + k, z, doorId(IRON_DOOR, hs));
            }
        }
    }
}

}  // namespace

void updatePower(World& w, int x, int y, int z, std::vector<std::array<int, 3>>& ignite) {
    std::vector<std::array<int, 3>> seeds = {{x, y, z}};
    for (auto& d : kDirs) seeds.push_back({x + d[0], y + d[1], z + d[2]});
    std::set<std::array<int, 3>> done;
    std::vector<std::array<int, 3>> consumers;
    for (auto& s : seeds) {
        if (!isCable(w.getBlock(s[0], s[1], s[2])) || done.count(s)) continue;
        // The whole connected cable network: powered if any cable touches a source.
        std::vector<std::array<int, 3>> net = {s};
        done.insert(s);
        bool powered = false;
        for (size_t i = 0; i < net.size() && net.size() < 4096; ++i) {
            for (auto& d : kDirs) {
                std::array<int, 3> n = {net[i][0] + d[0], net[i][1] + d[1], net[i][2] + d[2]};
                ItemId b = w.getBlock(n[0], n[1], n[2]);
                if (isSource(b)) powered = true;
                if (isCable(b) && done.insert(n).second) net.push_back(n);
                if (isLamp(b) || isTnt(b) || (isDoor(b) && b >= IRON_DOOR)) consumers.push_back(n);
            }
        }
        for (auto& c : net) {
            const ItemId now = w.getBlock(c[0], c[1], c[2]), want = cableWithPower(now, powered);
            if (now != want) w.setBlock(c[0], c[1], c[2], want);
        }
    }
    // Things next to the change itself (a lamp beside a power block, a switch flipped).
    for (auto& s : seeds) consumers.push_back(s);
    for (auto& s : seeds)
        for (auto& d : kDirs) consumers.push_back({s[0] + d[0], s[1] + d[1], s[2] + d[2]});
    std::sort(consumers.begin(), consumers.end());
    consumers.erase(std::unique(consumers.begin(), consumers.end()), consumers.end());
    for (auto& c : consumers) applyConsumer(w, c[0], c[1], c[2], ignite);
}

}  // namespace vc

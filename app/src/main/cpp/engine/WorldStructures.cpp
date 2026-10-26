#include "WorldStructures.h"

#include <algorithm>
#include <cmath>

#include "Blocks.h"
#include "Noise.h"

namespace vc {

namespace {

struct Rng {
    uint32_t s;
    int next(int n) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (int)(s % (uint32_t)n);
    }
    bool chance(int percent) { return next(100) < percent; }
};

Structure box(int sx, int sy, int sz) {
    Structure s;
    s.sx = sx;
    s.sy = sy;
    s.sz = sz;
    s.cells.assign((size_t)sx * sy * sz, Structure::kVoid);
    return s;
}

// A small house that time has eaten: mossy walls with gaps, a fallen roof, a chest in a corner.
Structure ruin(Rng& r) {
    Structure s = box(7, 5, 7);
    for (int y = 0; y < 5; ++y)
        for (int z = 0; z < 7; ++z)
            for (int x = 0; x < 7; ++x) {
                bool wall = x == 0 || x == 6 || z == 0 || z == 6;
                ItemId& c = s.at(x, y, z);
                if (y == 0) c = r.chance(30) ? MOSSY_COBBLESTONE : COBBLESTONE;  // floor
                else if (y <= 3 && wall) {
                    // The higher, the more is missing.
                    if (r.chance(15 + y * 18)) continue;
                    bool corner = (x == 0 || x == 6) && (z == 0 || z == 6);
                    c = corner ? LOG : r.chance(35) ? MOSSY_COBBLESTONE : y == 1 ? COBBLESTONE : PLANKS;
                } else if (y <= 3) {
                    c = AIR;
                } else if (y == 4 && !wall && r.chance(25)) {
                    c = PLANKS;  // what is left of the roof
                }
            }
    s.at(3, 1, 0) = s.at(3, 2, 0) = AIR;  // doorway
    s.at(1 + r.next(2) * 4, 1, 5) = LOOT_CHEST_RUIN;
    if (r.chance(50)) s.at(5, 1, 1) = CRAFTING_TABLE;
    return s;
}

// A sunken ship: a plank hull lying on the sea floor with a broken mast.
Structure shipwreck(Rng& r) {
    const int len = 13, wid = 5, hgt = 6;
    Structure s = box(wid, hgt + 3, len);
    for (int z = 0; z < len; ++z) {
        // The bow and stern narrow to a point.
        int inset = z < 2 ? 2 - z : z > len - 3 ? z - (len - 3) : 0;
        for (int y = 0; y < hgt; ++y)
            for (int x = 0; x < wid; ++x) {
                int lo = inset, hi = wid - 1 - inset;
                if (x < lo || x > hi) continue;
                bool hull = y == 0 || x == lo || x == hi;
                if (y > 3 && hull && y != 0) continue;  // low sides
                ItemId& c = s.at(x, y, z);
                if (hull) c = r.chance(12) ? (ItemId)Structure::kVoid : (ItemId)(y == 0 ? PLANKS : SPRUCE_PLANKS);  // holes
                else if (y == 3 && z > 2 && z < len - 3 && r.chance(70)) c = PLANKS;  // deck
                else c = AIR;
            }
    }
    // Broken mast and a chest below deck.
    int mast = 2 + r.next(3);
    for (int y = 1; y < 4 + mast; ++y) s.at(2, y, len / 2) = SPRUCE_LOG;
    s.at(2, 1, 3) = LOOT_CHEST_SHIP;
    if (r.chance(60)) s.at(2, 1, len - 4) = LOOT_CHEST_SHIP;
    return s;
}

// A stone brick room with mossy patches, an iron-barred feel from fences and two chests.
Structure dungeon(Rng& r) {
    Structure s = box(9, 6, 9);
    for (int y = 0; y < 6; ++y)
        for (int z = 0; z < 9; ++z)
            for (int x = 0; x < 9; ++x) {
                bool shell = x == 0 || x == 8 || z == 0 || z == 8 || y == 0 || y == 5;
                s.at(x, y, z) = shell ? (ItemId)(r.chance(30) ? MOSSY_COBBLESTONE : STONE_BRICKS) : (ItemId)AIR;
            }
    for (int y = 1; y <= 3; ++y) s.at(4, y, 0) = AIR;  // an opening into the rock around
    s.at(4, 1, 0) = AIR;
    for (int k : {2, 6}) s.at(k, 1, 4) = FENCE, s.at(k, 2, 4) = FENCE;
    s.at(1, 1, 1 + r.next(7)) = LOOT_CHEST_DUNGEON;
    s.at(7, 1, 1 + r.next(7)) = LOOT_CHEST_DUNGEON;
    return s;
}

// Mineshaft: a central room and corridors that branch off and branch again, with
// wooden supports every few blocks and the odd chest. Up to 96 x 7 x 96 blocks.
Structure mineshaft(Rng& r) {
    const int size = 96, height = 7, mid = size / 2;
    Structure s = box(size, height, size);
    auto carve = [&](int x, int y, int z, ItemId id) {
        if (x >= 0 && x < size && y >= 0 && y < height && z >= 0 && z < size) s.at(x, y, z) = id;
    };
    // Central room
    for (int z = mid - 4; z <= mid + 4; ++z)
        for (int x = mid - 4; x <= mid + 4; ++x)
            for (int y = 1; y <= 4; ++y) carve(x, y, z, AIR);
    struct Branch {
        int x, z, dx, dz, depth;
    };
    std::vector<Branch> todo = {{mid, mid, 1, 0, 0}, {mid, mid, -1, 0, 0}, {mid, mid, 0, 1, 0}, {mid, mid, 0, -1, 0}};
    int corridors = 0;
    while (!todo.empty() && corridors < 18) {
        Branch b = todo.back();
        todo.pop_back();
        ++corridors;
        int length = 12 + r.next(22);
        int x = b.x, z = b.z;
        for (int step = 0; step < length; ++step) {
            x += b.dx;
            z += b.dz;
            if (x < 3 || z < 3 || x > size - 4 || z > size - 4) break;
            const int px = b.dz != 0 ? 1 : 0, pz = b.dx != 0 ? 1 : 0;  // across the corridor
            for (int w = -1; w <= 1; ++w)
                for (int y = 1; y <= 3; ++y) carve(x + px * w, y, z + pz * w, AIR);
            if (step % 4 == 2) {
                // A support: two fence posts and a plank beam.
                carve(x - px, 1, z - pz, FENCE);
                carve(x - px, 2, z - pz, FENCE);
                carve(x + px, 1, z + pz, FENCE);
                carve(x + px, 2, z + pz, FENCE);
                for (int w = -1; w <= 1; ++w) carve(x + px * w, 3, z + pz * w, PLANKS);
            }
            if (step % 7 == 5 && r.chance(9)) carve(x + px, 1, z + pz, LOOT_CHEST_MINE);
            // Side branches.
            if (b.depth < 3 && step > 4 && r.chance(6)) {
                todo.push_back({x, z, b.dz, b.dx, b.depth + 1});
                todo.push_back({x, z, -b.dz, -b.dx, b.depth + 1});
            }
        }
    }
    // At least one chest in the room.
    carve(mid + 3, 1, mid - 3, LOOT_CHEST_MINE);
    return s;
}

void fill(Structure& s, int x0, int y0, int z0, int x1, int y1, int z1, ItemId id) {
    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (x >= 0 && y >= 0 && z >= 0 && x < s.sx && y < s.sy && z < s.sz) s.at(x, y, z) = id;
}

// A hollow nether brick room: floor, walls and roof, with air inside and glowstone in the roof.
void room(Structure& s, int x0, int y0, int z0, int x1, int y1, int z1) {
    fill(s, x0, y0, z0, x1, y1, z1, NETHER_BRICK);
    fill(s, x0 + 1, y0 + 1, z0 + 1, x1 - 1, y1 - 1, z1 - 1, AIR);
    for (int z = z0 + 2; z < z1 - 1; z += 4)
        for (int x = x0 + 2; x < x1 - 1; x += 4) s.at(x, y1, z) = POWERSTONE_LIGHT;
}

// The Nether fortress: four rooms full of loot joined by corridors to a great hall,
// where a huge rectangle of void stone waits to be lit with the void flint and steel.
Structure fortress(Rng& r) {
    const int n = 53, c = 26;
    Structure s = box(n, 20, n);
    // Rooms at the ends of the four corridors.
    const int rooms[4][2] = {{c, 5}, {c, n - 6}, {5, c}, {n - 6, c}};
    for (auto& rm : rooms) {
        room(s, rm[0] - 6, 0, rm[1] - 5, rm[0] + 6, 8, rm[1] + 5);
        if (rm[0] == c) room(s, rm[0] - 5, 0, rm[1] - 6, rm[0] + 5, 8, rm[1] + 6);
        int chests = 2 + r.next(2);
        for (int i = 0; i < chests; ++i) {
            int dx = (i % 2 ? 4 : -4), dz = (i < 2 ? -3 : 3);
            s.at(rm[0] + dx, 1, rm[1] + dz) = LOOT_CHEST_FORTRESS;
        }
        // A little decoration: soul sand and nether grass patches, fences as railings.
        for (int k = 0; k < 6; ++k) s.at(rm[0] - 3 + r.next(7), 0, rm[1] - 3 + r.next(7)) = r.chance(50) ? NETHERRACK : NETHER_GRASS;
    }
    // Corridors (7 wide, 7 tall) between the rooms and the hall.
    room(s, c - 3, 0, 10, c + 3, 6, 16);
    room(s, c - 3, 0, n - 17, c + 3, 6, n - 11);
    room(s, 10, 0, c - 3, 16, 6, c + 3);
    room(s, n - 17, 0, c - 3, n - 11, 6, c + 3);
    for (int k = 11; k <= 15; k += 2) {  // windows with fences
        s.at(c - 3, 3, k) = s.at(c + 3, 3, k) = FENCE;
        s.at(c - 3, 3, n - 1 - k) = s.at(c + 3, 3, n - 1 - k) = FENCE;
        s.at(k, 3, c - 3) = s.at(k, 3, c + 3) = FENCE;
        s.at(n - 1 - k, 3, c - 3) = s.at(n - 1 - k, 3, c + 3) = FENCE;
    }
    // The great hall.
    room(s, c - 10, 0, c - 10, c + 10, 15, c + 10);
    for (int y = 4; y <= 8; y += 4)
        for (int k = c - 8; k <= c + 8; k += 4) {
            s.at(c - 10, y, k) = s.at(c + 10, y, k) = FENCE;
            if (std::abs(k - c) > 2) s.at(k, y, c - 10) = s.at(k, y, c + 10) = FENCE;
        }
    // Doorways along the whole way: hall <-> corridors <-> rooms.
    fill(s, c - 2, 1, 4, c + 2, 4, n - 5, AIR);
    fill(s, 4, 1, c - 2, n - 5, 4, c + 2, AIR);
    // Raised platform and the huge void portal frame (7 x 9 inside), standing along X.
    fill(s, c - 6, 1, c - 3, c + 6, 1, c + 3, NETHER_BRICK);
    fill(s, c - 4, 2, c, c + 4, 12, c, VOID_STONE);
    fill(s, c - 3, 3, c, c + 3, 11, c, AIR);
    for (int k = -6; k <= 6; k += 12) {  // eye pillars beside it
        fill(s, c + k, 2, c, c + k, 6, c, VOID_STONE_BRICKS);
        s.at(c + k, 7, c) = EYE_LOG;
    }
    // Two chests on the platform, one on each side of the frame.
    s.at(c - 5, 2, c - 2) = LOOT_CHEST_FORTRESS;
    s.at(c + 5, 2, c + 2) = LOOT_CHEST_FORTRESS;
    // 1.5.0: weathered, carved and fenced nether bricks, and at the foot of the frame the
    // carved powerstone of the kingdom that built the first portals.
    for (int y = 0; y < s.sy; ++y)
        for (int z = 0; z < n; ++z)
            for (int x = 0; x < n; ++x) {
                ItemId& b = s.at(x, y, z);
                if (b == FENCE) b = NETHER_BRICK_FENCE;
                else if (b == NETHER_BRICK && r.chance(12)) b = CRACKED_NETHER_BRICKS;
                else if (b == NETHER_BRICK && y % 4 == 2 && r.chance(6)) b = CHISELED_NETHER_BRICKS;
            }
    for (int k = -6; k <= 6; k += 2) {
        s.at(c + k, 1, c - 3) = CHISELED_POWERSTONE_BRICKS;
        s.at(c + k, 1, c + 3) = CHISELED_POWERSTONE_BRICKS;
    }
    return s;
}

// The boss arena of the Crimson Void: a round platform of void stone bricks in a ring of
// lava, eight obsidian pillars with glowing eye logs, and an altar in the middle.
Structure bossArena(Rng& r) {
    const int R = kArenaRadius, n = 2 * R + 5, c = n / 2;
    Structure s = box(n, 20, n);
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x) {
            int dx = x - c, dz = z - c, d2 = dx * dx + dz * dz;
            if (d2 > (R + 2) * (R + 2)) continue;
            fill(s, x, 0, z, x, 2, z, VOID_STONE);
            bool moat = d2 > R * R;
            s.at(x, 3, z) = moat ? LAVA : ((dx + dz) % 7 == 0 && r.chance(40)) ? VOID_STONE : VOID_STONE_BRICKS;
            fill(s, x, 4, z, x, 19, z, AIR);
        }
    // A bridge over the lava on the south side.
    fill(s, c - 1, 3, c + R - 1, c + 1, 3, n - 1, VOID_STONE_BRICKS);
    for (int i = 0; i < 8; ++i) {
        float a = i * 3.14159265f / 4;
        int px = c + (int)std::lround(std::cos(a) * (R - 3)), pz = c + (int)std::lround(std::sin(a) * (R - 3));
        fill(s, px, 4, pz, px, 9, pz, OBSIDIAN);
        s.at(px, 10, pz) = EYE_LOG;
    }
    fill(s, c - 2, 4, c - 2, c + 2, 4, c + 2, VOID_STONE_BRICKS);
    fill(s, c - 1, 4, c - 1, c + 1, 5, c + 1, OBSIDIAN);
    s.at(c, 6, c) = EYE_LOG;
    // 1.5.1: Void Brick Eyes watch from the altar's corners and from the arena floor.
    for (int k : {-2, 2})
        for (int j : {-2, 2}) s.at(c + k, 5, c + j) = VOID_BRICK_EYE;
    for (int i = 0; i < 8; ++i) {
        const float a = (i + 0.5f) * 3.14159265f / 4;
        s.at(c + (int)std::lround(std::cos(a) * (R - 6)), 3, c + (int)std::lround(std::sin(a) * (R - 6))) = VOID_BRICK_EYE;
    }
    return s;
}

// A spiral of single blocks up the inside of a round wall: something to climb without ladders.
void spiral(Structure& s, int cx, int cz, int radius, int y0, int y1, ItemId block) {
    const int ring[][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
    for (int y = y0, k = 0; y <= y1; ++y, ++k) {
        int dx = ring[k % 8][0] * radius, dz = ring[k % 8][1] * radius;
        s.at(cx + dx, y, cz + dz) = block;
    }
}

// Under the Crimson Void: a hall of void bricks lit by eyes, with loot (and maybe the compass).
Structure voidDungeon(Rng& r) {
    Structure s = box(11, 7, 11);
    fill(s, 0, 0, 0, 10, 6, 10, VOID_STONE_BRICKS);
    fill(s, 1, 1, 1, 9, 5, 9, AIR);
    for (int k : {2, 8}) {
        fill(s, k, 1, 2, k, 5, 2, VOID_STONE);
        fill(s, k, 1, 8, k, 5, 8, VOID_STONE);
        s.at(k, 4, 2) = s.at(k, 4, 8) = EYE_LOG;
    }
    s.at(5, 1, 1) = LOOT_CHEST_VOID;
    if (r.chance(60)) s.at(5, 1, 9) = LOOT_CHEST_VOID;
    s.at(5, 6, 5) = POWERSTONE_LIGHT;
    for (int k = 0; k < 8; ++k) s.at(1 + r.next(9), 0, 1 + r.next(9)) = r.chance(50) ? VOID_STONE : OBSIDIAN;
    return s;
}

// A tall tower of void bricks with a spiral inside, chests on two floors and eyes on top.
Structure voidTower(Rng& r) {
    const int n = 11, c = 5, h = 34;
    Structure s = box(n, h, n);
    for (int y = 0; y < h - 2; ++y)
        for (int z = 0; z < n; ++z)
            for (int x = 0; x < n; ++x) {
                int dx = x - c, dz = z - c, d2 = dx * dx + dz * dz;
                if (d2 > 26) continue;
                bool wall = d2 >= 17;
                s.at(x, y, z) = wall ? ((y % 9 == 4 && (dx == 0 || dz == 0)) ? (ItemId)AIR : (ItemId)VOID_STONE_BRICKS)  // windows
                                     : (y == 0 ? (ItemId)VOID_STONE_BRICKS : (ItemId)AIR);
            }
    s.at(c, 1, 0) = s.at(c, 2, 0) = AIR;  // the door
    spiral(s, c, c, 3, 1, h - 4, VOID_STONE_BRICKS);
    // Floors with a hole where the spiral passes.
    for (int fy : {12, h - 4}) {
        for (int z = 1; z < n - 1; ++z)
            for (int x = 1; x < n - 1; ++x) {
                int dx = x - c, dz = z - c;
                if (dx * dx + dz * dz < 17 && !(std::abs(dx) <= 1 && dz <= -1)) s.at(x, fy, z) = VOID_STONE_BRICKS;
            }
        s.at(c + 1, fy + 1, c + 1) = LOOT_CHEST_VOID;
        s.at(c, fy + 3, c) = POWERSTONE_LIGHT;
    }
    // Battlements with glowing eyes.
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x) {
            int dx = x - c, dz = z - c, d2 = dx * dx + dz * dz;
            if (d2 >= 17 && d2 <= 26 && (x + z) % 2 == 0) s.at(x, h - 2, z) = r.chance(30) ? EYE_LOG : VOID_STONE_BRICKS;
        }
    return s;
}

// A sandstone pyramid with a treasure room in its heart.
Structure desertTemple(Rng& r) {
    const int n = 15, h = 9;
    Structure s = box(n, h, n);
    for (int y = 0; y < h; ++y) fill(s, y, y, y, n - 1 - y, y, n - 1 - y, SANDSTONE);
    fill(s, 3, 1, 3, 11, 3, 11, AIR);                       // the treasure room
    fill(s, 7, 1, 0, 7, 2, 3, AIR);                         // the corridor in
    for (int k : {4, 10}) s.at(k, 1, 4) = s.at(k, 1, 10) = LOOT_CHEST_TEMPLE;
    if (r.chance(40)) s.at(7, 0, 7) = TNT;                  // a nasty surprise under the middle
    s.at(7, 4, 7) = POWERSTONE_LIGHT;
    fill(s, 6, 0, 6, 8, 0, 8, r.chance(50) ? RED_SAND : SANDSTONE);
    return s;
}

// A snow dome with a bed, a crafting table and a chest.
Structure igloo(Rng& r) {
    Structure s = box(9, 6, 9);
    for (int y = 0; y < 6; ++y)
        for (int z = 0; z < 9; ++z)
            for (int x = 0; x < 9; ++x) {
                float dx = x - 4, dz = z - 4, dy = y * 1.25f;
                float d = std::sqrt(dx * dx + dz * dz + dy * dy);
                if (d <= 4.4f) s.at(x, y, z) = (d >= 3.4f || y == 0) ? (ItemId)SNOW_BLOCK : (ItemId)AIR;
            }
    s.at(4, 1, 0) = s.at(4, 2, 0) = s.at(4, 1, 1) = s.at(4, 2, 1) = AIR;  // entrance
    s.at(2, 1, 4) = BED + 0;   // foot, head towards the north
    s.at(2, 1, 3) = BED + 1;
    s.at(6, 1, 4) = LOOT_CHEST_IGLOO;
    s.at(5, 1, 6) = r.chance(50) ? CRAFTING_TABLE : FURNACE;
    s.at(4, 4, 4) = POWERSTONE_LIGHT;
    return s;
}

// A watchtower of cobblestone and spruce with a lookout on top.
Structure watchtower(Rng& r) {
    const int n = 7, c = 3, h = 16;
    Structure s = box(n, h, n);
    for (int y = 0; y < h - 4; ++y)
        for (int z = 1; z < n - 1; ++z)
            for (int x = 1; x < n - 1; ++x) {
                bool wall = x == 1 || x == n - 2 || z == 1 || z == n - 2;
                bool corner = (x == 1 || x == n - 2) && (z == 1 || z == n - 2);
                s.at(x, y, z) = !wall ? (y == 0 ? (ItemId)COBBLESTONE : (ItemId)AIR)
                                      : corner ? (ItemId)SPRUCE_LOG
                                               : (y % 4 == 2 && (x == c || z == c)) ? (ItemId)AIR : (ItemId)COBBLESTONE;
            }
    s.at(c, 1, 1) = s.at(c, 2, 1) = AIR;
    spiral(s, c, c, 1, 1, h - 5, SPRUCE_PLANKS);
    fill(s, 0, h - 4, 0, n - 1, h - 4, n - 1, SPRUCE_PLANKS);  // the lookout
    s.at(c, h - 4, c) = AIR;                                  // the way up
    for (int k = 0; k < n; ++k) {
        s.at(k, h - 3, 0) = s.at(k, h - 3, n - 1) = FENCE;
        s.at(0, h - 3, k) = s.at(n - 1, h - 3, k) = FENCE;
    }
    s.at(1, h - 3, 1) = LOOT_CHEST_TOWER;
    if (r.chance(50)) s.at(5, h - 3, 5) = CRAFTING_TABLE;
    return s;
}

// A house of planks and logs with glass windows and a door facing the middle of the
// village (door side: 0 north, 1 east, 2 south, 3 west), a pitched roof and some furniture.
void house(Structure& s, Rng& r, int x0, int z0, int w, int d, int doorSide, ItemId extra) {
    const int h = 4;
    for (int y = 0; y <= h; ++y)
        for (int z = 0; z < d; ++z)
            for (int x = 0; x < w; ++x) {
                const bool wall = x == 0 || x == w - 1 || z == 0 || z == d - 1;
                const bool corner = (x == 0 || x == w - 1) && (z == 0 || z == d - 1);
                ItemId& c = s.at(x0 + x, y, z0 + z);
                if (y == 0) c = COBBLESTONE;
                else if (y < h) c = !wall ? (ItemId)AIR : corner ? (ItemId)LOG
                                       : (y == 2 && (x == w / 2 || z == d / 2)) ? (ItemId)GLASS : (ItemId)PLANKS;
                else c = SPRUCE_PLANKS;  // flat part of the roof
            }
    // A smaller layer on top makes the roof pitched.
    for (int z = 1; z < d - 1; ++z)
        for (int x = 1; x < w - 1; ++x) s.at(x0 + x, h + 1, z0 + z) = SPRUCE_PLANKS;
    // The door, in the middle of its side (the door id takes the facing of that side).
    int dx = doorSide == 1 ? w - 1 : doorSide == 3 ? 0 : w / 2, dz = doorSide == 0 ? 0 : doorSide == 2 ? d - 1 : d / 2;
    const int facing = doorSide;
    s.at(x0 + dx, 1, z0 + dz) = (ItemId)(OAK_DOOR + facing);
    s.at(x0 + dx, 2, z0 + dz) = (ItemId)(OAK_DOOR + facing + 8);
    // Inside: a bed, a chest, and the trade's own block.
    s.at(x0 + 1, 1, z0 + 1) = CHEST;
    s.at(x0 + w - 2, 1, z0 + 1) = extra;
    if (r.chance(60)) s.at(x0 + 1, 1, z0 + d - 2) = CRAFTING_TABLE;
}

// A village: houses round a well, gravel paths, lamps and two fields of wheat with water.
// ---------------------------------------------------------------- 1.5.0 forgotten kingdom

// Powerstone bricks as time left them: some cracked, a few carved.
ItemId agedBrick(Rng& r) {
    const int k = r.next(100);
    return k < 30 ? (ItemId)CRACKED_POWERSTONE_BRICKS : k < 36 ? (ItemId)CHISELED_POWERSTONE_BRICKS : (ItemId)POWERSTONE_BRICKS;
}

// A lamp that still glows: the powerstone block above it has powered it for centuries.
void ancientLamp(Structure& s, int x, int y, int z) {
    if (y + 1 >= s.sy) return;
    s.at(x, y, z) = POWERSTONE_LAMP_ON;
    s.at(x, y + 1, z) = POWERSTONE_BLOCK;
}

// An abandoned house of powerstone bricks: walls missing more the higher they are, a roof of
// slabs half fallen in, a chest, sometimes a lamp still lit and a skull on the floor.
void forgottenHouse(Structure& s, Rng& r, int x0, int z0, int w, int d, int doorSide) {
    const int h = 4;
    for (int y = 0; y <= h; ++y)
        for (int z = 0; z < d; ++z)
            for (int x = 0; x < w; ++x) {
                const bool wall = x == 0 || x == w - 1 || z == 0 || z == d - 1;
                const bool corner = (x == 0 || x == w - 1) && (z == 0 || z == d - 1);
                ItemId& c = s.at(x0 + x, y, z0 + z);
                if (y == 0) c = r.chance(10) ? (ItemId)DEBRIS_GRAVEL : agedBrick(r);
                else if (y < h) {
                    if (!wall) c = AIR;
                    else if (corner) c = CHISELED_POWERSTONE_BRICKS;
                    else if (r.chance(y * 9)) c = AIR;  // fallen out
                    else if (y == 2 && (x == w / 2 || z == d / 2)) c = r.chance(70) ? (ItemId)POWERSTONE_GLASS : (ItemId)AIR;
                    else c = agedBrick(r);
                } else if (!r.chance(35)) {
                    c = slabId(12, 0);  // the roof, with holes where it fell in
                }
            }
    // The doorway: an opening without a door, in the middle of its side.
    const int dx = doorSide == 1 ? w - 1 : doorSide == 3 ? 0 : w / 2, dz = doorSide == 0 ? 0 : doorSide == 2 ? d - 1 : d / 2;
    s.at(x0 + dx, 1, z0 + dz) = AIR;
    s.at(x0 + dx, 2, z0 + dz) = AIR;
    s.at(x0 + 1, 1, z0 + d - 2) = LOOT_CHEST_FORGOTTEN;
    if (r.chance(60)) s.at(x0 + w - 2, 1, z0 + d - 2) = BOOKSHELF;
    if (r.chance(45)) s.at(x0 + w - 2, 1, z0 + 1) = (ItemId)(SKELETON_HEAD + r.next(4));
    if (r.chance(55)) ancientLamp(s, x0 + w / 2, h - 1, z0 + d / 2);
    else s.at(x0 + w / 2, h - 1, z0 + d / 2) = POWERSTONE_LAMP;  // gone dark
    if (r.chance(50)) {
        // Grass has grown through the floor.
        const int gx = x0 + 1 + r.next(w - 2), gz = z0 + 1 + r.next(d - 2);
        if (s.at(gx, 1, gz) == AIR) {
            s.at(gx, 0, gz) = WASTELAND_GRASS;
            s.at(gx, 1, gz) = DRY_GRASS;
        }
    }
}

// A village of the forgotten kingdom: ruined powerstone houses round a plaza with a lamp
// column, cracked paths overgrown with dry grass and Grasser flowers, lamp posts that
// partly still glow, and debris to brush through.
Structure forgottenVillage(Rng& r) {
    const int n = kForgottenVillageSize, c = n / 2, R = n / 2;
    Structure s = box(n, 11, n);
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x) {
            const int dx = x - c, dz = z - c;
            if (dx * dx + dz * dz > R * R) continue;
            s.at(x, 0, z) = WASTELAND_GRASS;
            for (int y = 1; y < 11; ++y) s.at(x, y, z) = AIR;
            if (std::abs(dx) <= 1 || std::abs(dz) <= 1) {
                // The old roads: cracked bricks, gravel, here and there grass grows over them.
                const int k = r.next(100);
                s.at(x, 0, z) = k < 35 ? (ItemId)CRACKED_POWERSTONE_BRICKS : k < 60 ? (ItemId)GRAVEL : k < 75 ? (ItemId)POWERSTONE_BRICKS
                              : k < 83 ? (ItemId)DEBRIS_GRAVEL : (ItemId)WASTELAND_GRASS;
            } else if (r.chance(14)) {
                s.at(x, 1, z) = r.chance(18) ? (ItemId)GRASSER_FLOWER : (ItemId)DRY_GRASS;
            } else if (r.chance(2)) {
                s.at(x, 0, z) = DEBRIS_GRAVEL;
            }
        }
    // The plaza: carved bricks round a column with a lamp that never went out.
    for (int z = c - 3; z <= c + 3; ++z)
        for (int x = c - 3; x <= c + 3; ++x) {
            s.at(x, 0, z) = (std::abs(x - c) == 3 || std::abs(z - c) == 3) ? (ItemId)CHISELED_POWERSTONE_BRICKS : agedBrick(r);
            s.at(x, 1, z) = AIR;
        }
    for (int y = 1; y <= 4; ++y) s.at(c, y, c) = y == 4 ? (ItemId)CHISELED_POWERSTONE_BRICKS : agedBrick(r);
    ancientLamp(s, c, 5, c);
    for (int k : {-1, 1}) {
        s.at(c + k, 1, c) = stairsId(12, k > 0 ? 3 : 1, false);
        s.at(c, 1, c + k) = stairsId(12, k > 0 ? 0 : 2, false);
    }
    // Houses in the quarters, doorways towards the roads, and two smaller ones.
    forgottenHouse(s, r, c + 4, c - 11, 7, 7, 2);
    forgottenHouse(s, r, c - 10, c - 11, 7, 7, 2);
    forgottenHouse(s, r, c - 10, c + 4, 7, 7, 0);
    forgottenHouse(s, r, c + 4, c + 4, 7, 7, 0);
    if (r.chance(75)) forgottenHouse(s, r, c + 12, c - 3, 5, 6, 3);
    if (r.chance(75)) forgottenHouse(s, r, c - 16, c - 3, 5, 6, 1);
    // Lamp posts along the roads: powerstone fence posts, some still lit, some broken off.
    for (int k : {-9, 9})
        for (auto [lx, lz] : {std::pair<int, int>{c + k, c + 2}, std::pair<int, int>{c - 2, c + k}}) {
            s.at(lx, 1, lz) = POWERSTONE_FENCE;
            if (r.chance(30)) continue;  // only a stump is left
            s.at(lx, 2, lz) = POWERSTONE_FENCE;
            if (r.chance(60)) ancientLamp(s, lx, 3, lz);
            else s.at(lx, 3, lz) = POWERSTONE_LAMP;
        }
    return s;
}

// Ruins under a lake or the sea: rooms of stone bricks and powerstone, a corridor between
// them, gravel full of finds, skulls, a lamp still glowing in the deep and a chest or two.
Structure drownedRuins(Rng& r) {
    const int n = kDrownedRuinsSize;
    Structure s = box(n, 8, n);
    auto wallBlock = [&]() {
        const int k = r.next(100);
        return k < 35 ? (ItemId)STONE_BRICKS : k < 60 ? (ItemId)MOSSY_COBBLESTONE : k < 85 ? (ItemId)CRACKED_POWERSTONE_BRICKS
                      : (ItemId)POWERSTONE_BRICKS;
    };
    auto chamber = [&](int x0, int z0, int x1, int z1, int height) {
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) {
                const bool wall = x == x0 || x == x1 || z == z0 || z == z1;
                const int k = r.next(100);
                s.at(x, 0, z) = k < 18 ? (ItemId)DEBRIS_GRAVEL : k < 55 ? (ItemId)GRAVEL : k < 80 ? (ItemId)STONE_BRICKS : (ItemId)MOSSY_COBBLESTONE;
                for (int y = 1; y <= height; ++y) {
                    if (!wall) s.at(x, y, z) = AIR;
                    else if (r.chance(10 + y * 12)) s.at(x, y, z) = AIR;  // broken walls
                    else s.at(x, y, z) = wallBlock();
                }
            }
    };
    chamber(0, 0, 8, 8, 5);
    chamber(10, 10, n - 1, n - 1, 4);
    // The corridor joining them.
    for (int k = 4; k <= 14; ++k) {
        for (int w = -1; w <= 1; ++w) {
            const int x = k, z = k + w;
            if (x < 0 || z < 0 || x >= n || z >= n) continue;
            s.at(x, 0, z) = r.chance(20) ? (ItemId)DEBRIS_GRAVEL : (ItemId)GRAVEL;
            for (int y = 1; y <= 3; ++y) s.at(x, y, z) = AIR;
        }
        if (k % 3 == 0 && k + 2 < n && k - 2 >= 0) {
            s.at(k, 1, k - 2) = wallBlock();
            s.at(k, 1, k + 2) = wallBlock();
        }
    }
    s.at(2, 1, 6) = LOOT_CHEST_DROWNED;
    if (r.chance(60)) s.at(n - 3, 1, n - 3) = LOOT_CHEST_DROWNED;
    for (int i = 0; i < 1 + r.next(3); ++i) s.at(1 + r.next(7), 1, 1 + r.next(5)) = (ItemId)(SKELETON_HEAD + r.next(4));
    ancientLamp(s, 4, 3, 4);
    if (r.chance(50)) ancientLamp(s, 14, 2, 14);
    return s;
}

// A broken portal of the kingdom's research: an obsidian frame with missing pieces on a
// cracked platform, carved pillars, debris and a chest with the compass to a fortress
// (the Cornerstone Dimension itself stays behind the portals).
Structure brokenPortal(Rng& r) {
    const int n = kBrokenPortalSize, c = n / 2;
    Structure s = box(n, 9, n);
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x) {
            const int dx = x - c, dz = z - c, d2 = dx * dx + dz * dz;
            if (d2 > c * c + 2) continue;
            const int k = r.next(100);
            ItemId floor = k < 34 ? (ItemId)CRACKED_POWERSTONE_BRICKS : k < 56 ? (ItemId)STONE_BRICKS : k < 70 ? (ItemId)MOSSY_COBBLESTONE
                         : k < 82 ? (ItemId)DEBRIS_GRAVEL : k < 92 ? (ItemId)GRAVEL : (ItemId)POWERSTONE_BRICKS;
            if (d2 > (c - 1) * (c - 1) && r.chance(45)) continue;  // a ragged edge
            s.at(x, 0, z) = floor;
            for (int y = 1; y < 9; ++y) s.at(x, y, z) = AIR;
        }
    // The frame: 4 wide and 5 tall inside, standing along x; a third of it is gone.
    const int fx0 = c - 2, fx1 = c + 3, fz = c, top = 6;
    for (int y = 1; y <= top; ++y)
        for (int x = fx0; x <= fx1; ++x) {
            const bool edge = x == fx0 || x == fx1 || y == 1 || y == top;
            if (!edge) continue;
            if (r.chance(y > 3 ? 45 : 20)) {
                // A fallen piece lies somewhere nearby.
                if (r.chance(50)) s.at(1 + r.next(n - 2), 1, fz + 2 + r.next(3)) = r.chance(60) ? (ItemId)OBSIDIAN : (ItemId)CRACKED_POWERSTONE_BRICKS;
                continue;
            }
            s.at(x, y, fz) = r.chance(15) ? (ItemId)CRACKED_POWERSTONE_BRICKS : (ItemId)OBSIDIAN;
        }
    // Carved pillars on both sides, one with a dead lamp.
    for (int side : {-1, 1}) {
        const int px = side < 0 ? fx0 - 2 : fx1 + 2;
        const int height = 2 + r.next(4);
        for (int y = 1; y <= height; ++y) s.at(px, y, fz) = y == height ? (ItemId)CHISELED_POWERSTONE_BRICKS : agedBrick(r);
        if (side > 0) s.at(px, height + 1, fz) = POWERSTONE_LAMP;
    }
    s.at(c - 3, 1, fz + 3) = LOOT_CHEST_PORTAL;
    if (r.chance(50)) s.at(c + 2, 1, fz - 2) = (ItemId)(SKELETON_HEAD + r.next(4));
    return s;
}

Structure village(Rng& r) {
    const int n = kVillageSize, c = n / 2, R = n / 2;
    Structure s = box(n, 9, n);
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x) {
            int dx = x - c, dz = z - c;
            if (dx * dx + dz * dz > R * R) continue;  // a round village keeps the land around it
            s.at(x, 0, z) = GRASS;
            for (int y = 1; y < 9; ++y) s.at(x, y, z) = AIR;  // a flat place to live
            if (std::abs(dx) <= 1 || std::abs(dz) <= 1) s.at(x, 0, z) = GRAVEL;  // the main paths
        }
    // The well in the middle.
    for (int z = c - 2; z <= c + 2; ++z)
        for (int x = c - 2; x <= c + 2; ++x) {
            bool rim = std::abs(x - c) == 2 || std::abs(z - c) == 2;
            s.at(x, 0, z) = rim ? COBBLESTONE : WATER;
            if (rim) s.at(x, 1, z) = COBBLESTONE;
        }
    for (int k : {-2, 2})
        for (int j : {-2, 2}) {
            s.at(c + k, 2, c + j) = FENCE;
            s.at(c + k, 3, c + j) = FENCE;
        }
    for (int z = c - 2; z <= c + 2; ++z)
        for (int x = c - 2; x <= c + 2; ++x) s.at(x, 4, z) = COBBLESTONE;
    // Houses in the four quarters, doors towards the paths; a smithy has a furnace.
    house(s, r, c + 4, c - 11, 7, 7, 2, FURNACE);      // north-east, door south
    house(s, r, c - 10, c - 11, 7, 7, 2, BOOKSHELF);   // north-west
    house(s, r, c - 10, c + 4, 7, 7, 0, HAY_BALE);     // south-west, door north
    if (r.chance(70)) house(s, r, c + 13, c - 4, 5, 7, 3, CRAFTING_TABLE);  // east, door west
    // Fields: farmland with a water channel down the middle, wheat at every stage.
    for (int z = c + 4; z <= c + 12; ++z)
        for (int x = c + 4; x <= c + 12; ++x) {
            bool channel = x == c + 8;
            s.at(x, 0, z) = channel ? WATER : FARMLAND;
            if (!channel) s.at(x, 1, z) = cropBlock(r.chance(55) ? 3 : r.next(3));
        }
    for (int k = 0; k < 3; ++k) s.at(c + 13, 1, c + 6 + k) = HAY_BALE;
    // Lamps along the paths.
    for (int k : {-8, 8}) {
        for (auto [lx, lz] : {std::pair<int, int>{c + k, c + 2}, std::pair<int, int>{c - 2, c + k}}) {
            s.at(lx, 1, lz) = FENCE;
            s.at(lx, 2, lz) = FENCE;
            s.at(lx, 3, lz) = POWERSTONE_LIGHT;
        }
    }
    return s;
}

}  // namespace

Structure buildWorldStructure(WorldStructure kind, uint32_t seed) {
    Rng r{seed | 1};
    switch (kind) {
        case WorldStructure::Ruin: return ruin(r);
        case WorldStructure::Shipwreck: return shipwreck(r);
        case WorldStructure::Dungeon: return dungeon(r);
        case WorldStructure::Fortress: return fortress(r);
        case WorldStructure::BossArena: return bossArena(r);
        case WorldStructure::VoidDungeon: return voidDungeon(r);
        case WorldStructure::VoidTower: return voidTower(r);
        case WorldStructure::DesertTemple: return desertTemple(r);
        case WorldStructure::Igloo: return igloo(r);
        case WorldStructure::Watchtower: return watchtower(r);
        case WorldStructure::Village: return village(r);
        case WorldStructure::ForgottenVillage: return forgottenVillage(r);
        case WorldStructure::DrownedRuins: return drownedRuins(r);
        case WorldStructure::BrokenPortal: return brokenPortal(r);
        default: return mineshaft(r);
    }
}

Structure anniversaryShowcase() {
    Structure s;
    s.name = "Anniversary showcase";
    s.sx = kShowcaseW;
    s.sy = 9;
    s.sz = kShowcaseD;
    s.cells.assign((size_t)s.sx * s.sy * s.sz, AIR);
    // The floor: stone bricks in a festive frame, a painted path to the logo and confetti here and there.
    for (int x = 0; x < s.sx; ++x)
        for (int z = 0; z < s.sz; ++z) {
            const bool border = x == 0 || z == 0 || x == s.sx - 1 || z == s.sz - 1;
            ItemId floor = border ? (ItemId)FESTIVE_BRICKS : (ItemId)STONE_BRICKS;
            if (!border && x >= 19 && x <= 23 && z >= 9) floor = (ItemId)(PAINTED_PLANKS + (x + z) % 4);
            else if (!border && hash3(x, 0, z, 0xB1D7u) % 9 == 0) floor = CONFETTI_BLOCK;
            s.at(x, 0, z) = floor;
        }
    // The back wall: light stone in a festive frame, the letters standing out in front of it.
    for (int x = 1; x < s.sx - 1; ++x)
        for (int y = 1; y < 8; ++y)
            s.at(x, y, 1) = (x == 1 || x == s.sx - 2 || y == 7) ? (ItemId)FESTIVE_BRICKS : (ItemId)SANDSTONE;
    static const char* const kLetters[10][5] = {
        {"X.X", "X.X", "X.X", "X.X", ".X."},  // V
        {"XXX", "X.X", "X.X", "X.X", "XXX"},  // O
        {"X.X", "X.X", ".X.", "X.X", "X.X"},  // X
        {"XXX", "X..", "XX.", "X..", "XXX"},  // E
        {"X..", "X..", "X..", "X..", "XXX"},  // L
        {"XXX", "X..", "X..", "X..", "XXX"},  // C
        {"XX.", "X.X", "XX.", "X.X", "X.X"},  // R
        {".X.", "X.X", "XXX", "X.X", "X.X"},  // A
        {"XXX", "X..", "XX.", "X..", "X.."},  // F
        {"XXX", ".X.", ".X.", ".X.", ".X."},  // T
    };
    for (int i = 0; i < 10; ++i)
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (kLetters[i][row][col] == 'X') s.at(2 + i * 4 + col, 6 - row, 2) = (ItemId)(PAINTED_PLANKS + i % 4);
    // Lanterns along the top of the wall.
    for (int x = 3; x < s.sx - 2; x += 6) s.at(x, 8, 1) = PARTY_LANTERN;
    // The logo on a pedestal in the middle, a lantern on top.
    s.at(21, 1, 8) = STONE_BRICKS;
    s.at(21, 2, 8) = ANNIV_LOGO;
    s.at(21, 3, 8) = PARTY_LANTERN;
    // The birthday cake on a festive table.
    s.at(15, 1, 9) = FESTIVE_BRICKS;
    s.at(15, 2, 9) = CAKE;
    // The gift chest.
    s.at(kShowcaseChest[0], kShowcaseChest[1], kShowcaseChest[2]) = CHEST;
    // Lanterns on posts at the front corners, balloons all around.
    for (int x : {1, s.sx - 2}) {
        s.at(x, 1, s.sz - 2) = STONE_BRICKS;
        s.at(x, 2, s.sz - 2) = PARTY_LANTERN;
    }
    for (int k = 0; k < 9; ++k) {
        const int x = 3 + k * 4 + (k % 2), z = k % 2 ? 4 : 5;
        if (s.at(x, 1, z) == AIR) s.at(x, 1, z) = (ItemId)(BALLOON + k % 3);
    }
    for (int z = 4; z < s.sz - 1; z += 3) {
        s.at(2, 1, z) = (ItemId)(BALLOON + z % 3);
        s.at(s.sx - 3, 1, z) = (ItemId)(BALLOON + (z + 1) % 3);
    }
    // Benches of oak stairs facing the letters.
    for (int x = 6; x < s.sx - 6; ++x)
        if (x < 18 || x > 24) s.at(x, 1, 12) = stairsId(0, 2, false);
    return s;
}

}  // namespace vc

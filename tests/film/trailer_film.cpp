// Films the VoxelCraft trailer: every shot is the real game, played by a script.
// Usage: trailer_film <out dir> <shot name|all>   (frames go to <out>/<shot>/f_#####.png)
#include <chrono>
#include <fstream>
#include <map>

#include <thread>

#include "engine/Multiplayer.h"
#include "engine/Skin.h"
#include "engine/BlockShapes.h"
#include "engine/Mobs.h"
#include "engine/Portal.h"
#include "engine/Rewards.h"
#include "engine/Trees.h"
#include "engine/Items.h"
#include "engine/Packs.h"
#include "engine/WorldStructures.h"
#include "film_core.h"

namespace {

using ShotFn = std::function<void(const fs::path&)>;
std::map<std::string, ShotFn>& shots() {
    static std::map<std::string, ShotFn> s;
    return s;
}
struct Register {
    Register(const char* name, ShotFn fn) { shots()[name] = std::move(fn); }
};
#define SHOT(name) \
    void shot_##name(const fs::path& out); \
    Register reg_##name(#name, shot_##name); \
    void shot_##name(const fs::path& out)


// ---------------------------------------------------------------- helpers

// The first free cell above the ground (feet height) at x, z.
int feetY(const World& w, int x, int z) {
    for (int y = kChunkHeight - 2; y > 1; --y) {
        ItemId b = w.getBlock(x, y, z);
        if (isSolid(b) && !isLeaves(b)) return y + 1;
        if (isLiquid(b)) return y + 1;
    }
    return 64;
}

// A tree with a free, flat spot `dist` blocks east of it.
bool findTree(World& w, const Vec3& near, int dist, int& tx, int& ty, int& tz) {
    const int cx = (int)near.x, cz = (int)near.z;
    for (int r = 2; r < 60; ++r)
        for (int x = cx - r; x <= cx + r; ++x)
            for (int z = cz - r; z <= cz + r; ++z) {
                if (std::abs(x - cx) != r && std::abs(z - cz) != r) continue;
                int base = -1;
                for (int yy = 50; yy < 110; ++yy)
                    if (isLog(w.getBlock(x, yy, z)) && isSolid(w.getBlock(x, yy - 1, z)) && !isLog(w.getBlock(x, yy - 1, z))) {
                        base = yy;
                        break;
                    }
                if (base < 0 || !isLog(w.getBlock(x, base + 2, z))) continue;
                bool ok = true;
                for (int k = 1; k <= dist && ok; ++k)
                    ok = feetY(w, x + k, z) == base && w.getBlock(x + k, base - 1, z) == GRASS && w.getBlock(x + k, base + 1, z) == AIR &&
                         !isLeaves(w.getBlock(x + k, base + 2, z));
                if (!ok) continue;
                tx = x, ty = base, tz = z;
                return true;
            }
    return false;
}

// Clears a flat stage of grass around (x, z) at feet height y (for building scenes).
void flatten(World& w, int x0, int z0, int x1, int z1, int y, ItemId top = GRASS) {
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z) {
            for (int yy = y; yy < y + 12; ++yy) w.setBlock(x, yy, z, AIR);
            w.setBlock(x, y - 1, z, top);
            for (int yy = y - 4; yy < y - 1; ++yy) w.setBlock(x, yy, z, DIRT);
        }
}

Vec3 topOf(int x, int y, int z) { return Vec3(x + 0.5f, y + 0.98f, z + 0.5f); }   // aim at the top face of block y
Vec3 centre(int x, int y, int z) { return Vec3(x + 0.5f, y + 0.5f, z + 0.5f); }


// A block of the given kind in a fresh copy of the world, nearest to the origin first.
bool findBlock(uint32_t seed, Dimension dim, ItemId id, int& fx, int& fy, int& fz, int maxR = 45) {
    World w(seed, dim);
    for (int r = 0; r <= maxR; ++r)
        for (int cx = -r; cx <= r; ++cx)
            for (int cz = -r; cz <= r; ++cz) {
                if (std::abs(cx) != r && std::abs(cz) != r) continue;
                w.ensureChunksAround(cx, cz, 0, 1000);
                for (int x = 0; x < 16; ++x)
                    for (int z = 0; z < 16; ++z)
                        for (int y = 1; y < kChunkHeight; ++y)
                            if (w.getBlock(cx * 16 + x, y, cz * 16 + z) == id) {
                                fx = cx * 16 + x, fy = y, fz = cz * 16 + z;
                                std::printf("found block %d at %d %d %d\n", id, fx, fy, fz);
                                return true;
                            }
            }
    return false;
}

bool findBiome(const World& w, const std::string& id, int& bx, int& bz) {
    for (int r = 0; r < 5000; r += 32)
        for (int a = 0; a < 32; ++a) {
            int x = (int)(std::cos(a * kPi / 16) * r), z = (int)(std::sin(a * kPi / 16) * r);
            const BiomeDef* b = w.biomeAt(x, z);
            const BiomeDef* b2 = w.biomeAt(x + 24, z + 24);
            const BiomeDef* b3 = w.biomeAt(x - 24, z - 24);
            if (b && b2 && b3 && b->id == id && b2->id == id && b3->id == id) {
                bx = x, bz = z;
                return true;
            }
        }
    return false;
}

// Teleports and waits for the terrain around.
void travel(Director& d, const Vec3& to) {
    d.s().teleport(to);
    d.idle(40);
}


// An obsidian frame (flush with the ground) in front of (x, y, z): interior x+1..x+2, y..y+2.
void obsidianFrame(World& w, int x, int y, int z) {
    for (int dx = 0; dx < 4; ++dx) {
        w.setBlock(x + dx, y - 1, z, OBSIDIAN);
        w.setBlock(x + dx, y + 3, z, OBSIDIAN);
    }
    for (int dy = 0; dy < 3; ++dy) {
        w.setBlock(x, y + dy, z, OBSIDIAN);
        w.setBlock(x + 3, y + dy, z, OBSIDIAN);
        w.setBlock(x + 1, y + dy, z, AIR);
        w.setBlock(x + 2, y + dy, z, AIR);
    }
}

// Waits (not filming) until the player is in `dim` and the loading screen is over.
void arrive(Director& d, Dimension dim) {
    for (int i = 0; i < 600 && d.s().currentWorld().dimension() != dim; ++i) d.idle(1);
    d.idle(90);
}

// Straight into the Nether through a portal (not filmed).
void goNether(Director& d) {
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    obsidianFrame(w, x - 1, y, z - 2);
    tryLightPortal(w, x, y, z - 2);
    p.pos = Vec3(x + 0.5f, (float)y, z - 1.5f);
    arrive(d, Dimension::Nether);
}

// From the Nether to the Crimson Void through the fortress portal (not filmed).
void goVoid(Director& d) {
    Session& s = d.s();
    World& w = s.currentWorld();
    int fx, fy, fz;
    w.nearestStructure((int)s.player().pos.x, (int)s.player().pos.z, fx, fy, fz);
    travel(d, Vec3(fx + 0.5f, fy + 1.0f, fz + 6.5f));
    bool lit = false;
    for (int dy = 0; dy < 6 && !lit; ++dy) lit = tryLightPortal(w, fx, fy + dy, fz, true);
    for (int y = fy; y < fy + 14; ++y)
        if (w.getBlock(fx, y, fz) == VOID_PORTAL) {
            s.player().pos = Vec3(fx + 0.5f, (float)y, fz + 0.5f);
            break;
        }
    arrive(d, Dimension::Void);
}

void heal(Director& d) { d.s().healAndFeed(); }

// ---------------------------------------------------------------- menus

SHOT(s00_splash) {
    Director d(out, "s00", true);
    d.shot("s00_splash");
    d.seconds(4.5f);
}

SHOT(s01_title) {
    Director d(out, "s01");
    d.shot("s01_title");
    d.seconds(7);
}

SHOT(s02_new_world) {
    Director d(out, "s02");
    d.idle(24);
    d.shot("s02_new_world");
    d.seconds(1.2f);
    d.tap(222, 121, 18);   // Play
    d.tap(164, 245, 12);   // New world
    d.game->onTextResult(1, "My First World");
    d.frames(18);
    d.tap(216, 245, 0);    // Create and play
    d.seconds(5.5f);
}

SHOT(s03_walk) {
    Director d(out, "s03");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    Player& p = s.player();
    p.yaw = 0.6f;
    p.pitch = -0.05f;
    d.shot("s03_walk");
    d.walk(true);
    d.seconds(8, [&](float k) {
        p.yaw += 0.006f * std::sin(k * 9);
        p.pitch = -0.05f + 0.08f * std::sin(k * 5);
    });
    d.walk(false);
    d.seconds(0.5f);
}


// ---------------------------------------------------------------- 0.1 - 0.4: survival basics

// Inventory and crafting-table slots (GUI coordinates, see the snapshots).
float invX(int col) { return 160.5f + 20 * col; }
constexpr float kInvRow0 = 150, kInvHotbar = 214;           // survival inventory
constexpr float kTableRow0 = 140, kTableHotbar = 204.5f;    // crafting table, furnace, chest

SHOT(s04_chop) {
    Director d(out, "s04");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    int tx, ty, tz;
    if (!findTree(w, p.pos, 4, tx, ty, tz)) std::printf("no tree\n");
    p.pos = Vec3(tx + 7.5f, (float)ty, tz + 0.5f);
    p.yaw = -kPi / 2;
    p.pitch = 0;
    s.inventory().clear();
    d.give(0, STEEL_AXE);
    d.idle(30);
    d.shot("s04_chop");
    d.walk(true);
    for (int i = 0; i < 90 && p.pos.x > tx + 2.6f; ++i) d.frame([&] { d.look(centre(tx, ty + 1, tz), 0.1f); });
    d.walk(false);
    d.mine(tx, ty + 1, tz);
    d.mine(tx, ty, tz);
    d.mine(tx, ty + 2, tz);
    // Pick up what fell.
    d.walk(true);
    d.frames(16, [&](int) { d.look(Vec3(tx + 0.5f, ty + 0.3f, tz + 0.5f), 0.2f); });
    d.walk(false);
    d.seconds(1.2f);
}

SHOT(s05_inventory) {
    Director d(out, "s05");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    s.inventory().clear();
    d.give(0, STEEL_AXE);
    d.give(1, LOG, 3);
    d.give(9, COBBLESTONE, 12);
    d.give(10, STICK, 4);
    d.give(11, APPLE, 3);
    s.selectedSlot() = 0;
    s.player().pitch = -0.1f;
    d.idle(20);
    d.shot("s05_inventory");
    d.seconds(0.6f);
    d.tap(Director::kMenuX, Director::kMenuY, 14);   // open the inventory
    d.tap(invX(1), kInvHotbar, 10);                     // pick the logs
    d.tap(240.5f, 74.5f, 16);                           // into the 2x2 grid: planks appear
    d.tap(310.5f, 84.5f, 14);                           // take the planks
    d.tap(310.5f, 84.5f, 14);
    d.tap(310.5f, 84.5f, 18);
    d.seconds(1.0f);
    d.tap(326, 47, 10);                                 // close
}

SHOT(s06_crafting_table) {
    Director d(out, "s06");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 4, z - 6, x + 4, z + 3, y);
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0;
    p.pitch = 0.1f;
    s.inventory().clear();
    d.give(0, CRAFTING_TABLE);
    for (int i = 0; i < 3; ++i) d.give(9 + i, COBBLESTONE, 1);
    d.give(12, STICK, 1);
    d.give(13, STICK, 1);
    d.give(14, PLANKS, 16);
    s.selectedSlot() = 0;
    d.idle(30);
    d.shot("s06_crafting_table");
    d.seconds(0.4f);
    d.use(topOf(x, y - 1, z - 3));                      // place the table
    d.seconds(0.6f);
    d.use(centre(x, y, z - 3));                         // and open it
    d.frames(10);
    // A stone pickaxe: three cobblestone on top, two sticks down the middle.
    const float gx[3] = {182.5f, 202.5f, 222.5f}, gy[3] = {74.5f, 94.5f, 114.5f};
    for (int i = 0; i < 3; ++i) {
        d.tap(invX(i), kTableRow0, 6);
        d.tap(gx[i], gy[0], 8);
    }
    d.tap(invX(3), kTableRow0, 6);
    d.tap(gx[1], gy[1], 8);
    d.tap(invX(4), kTableRow0, 6);
    d.tap(gx[1], gy[2], 14);
    d.tap(282.5f, 94.5f, 20);                           // the pickaxe
    d.seconds(0.8f);
}

SHOT(s07_furnace) {
    Director d(out, "s07");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 4, z - 6, x + 4, z + 3, y);
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, FURNACE);
    d.give(9, IRON_ORE, 8);
    d.give(10, COAL, 4);
    d.idle(30);
    d.shot("s07_furnace");
    d.use(topOf(x, y - 1, z - 3));
    d.seconds(0.3f);
    d.use(centre(x, y, z - 3));
    d.frames(8);
    d.tap(invX(0), kTableRow0, 6);   // iron ore into the input
    d.tap(226, 74, 8);
    d.tap(invX(1), kTableRow0, 6);   // coal as fuel
    d.tap(226, 115, 8);
    d.seconds(4.5f);
}

SHOT(s08_build) {
    Director d(out, "s08");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 6, z - 9, x + 6, z + 3, y);
    p.pos = Vec3(x + 0.5f, (float)y, z + 1.5f);
    p.yaw = 0;
    p.pitch = -0.3f;
    s.inventory().clear();
    d.give(0, PLANKS, 64);
    d.give(1, GLASS, 16);
    d.idle(30);
    d.shot("s08_build");
    // A wall of planks with a window, row by row, in front of the player.
    for (int row = 0; row < 3; ++row)
        for (int dx = -2; dx <= 2; ++dx) {
            bool window = row == 1 && std::abs(dx) <= 1 && dx != 0;
            s.selectedSlot() = window ? 1 : 0;
            Vec3 at = row == 0 ? topOf(x + dx, y - 1, z - 3) : topOf(x + dx, y + row - 1, z - 3);
            d.use(at);
        }
    d.seconds(1.5f, [&](float k) { p.yaw = 0.25f * std::sin(k * 3); });
}

SHOT(s09_mine) {
    Director d(out, "s09");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 5, z - 8, x + 5, z + 3, y);
    // A rock face with ores.
    for (int dx = -3; dx <= 3; ++dx)
        for (int dy = 0; dy < 4; ++dy)
            for (int dz = -6; dz >= -8; --dz) w.setBlock(x + dx, y + dy, z + dz, (dx + dy) % 3 ? STONE : COBBLESTONE);
    w.setBlock(x - 1, y, z - 6, COAL_ORE);
    w.setBlock(x, y + 1, z - 6, IRON_ORE);
    w.setBlock(x + 1, y, z - 6, BRONZE_ORE);
    w.setBlock(x - 2, y + 1, z - 6, STEEL_ORE);
    w.setBlock(x + 2, y + 1, z - 6, RUBY_ORE);
    p.pos = Vec3(x + 0.5f, (float)y, z - 3.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, STEEL_PICKAXE);
    d.idle(30);
    d.shot("s09_mine");
    d.mine(x - 1, y, z - 6);
    d.mine(x, y + 1, z - 6);
    d.mine(x + 1, y, z - 6);
    d.mine(x + 2, y + 1, z - 6);
    d.walk(true);
    d.frames(12, [&](int) { d.look(Vec3(x + 0.5f, y + 0.2f, z - 5.5f), 0.2f); });
    d.walk(false);
    d.seconds(1);
}

// ---------------------------------------------------------------- 0.5 - 1.0: menus, store, skins, creative

SHOT(s10_settings) {
    Director d(out, "s10");
    d.idle(10);
    d.shot("s10_settings");
    d.seconds(0.5f);
    d.tap(378, 121, 26);   // Settings
    d.tap(103, 81, 26);    // Video
    d.tap(103, 107, 26);   // Audio
    d.tap(103, 133, 26);   // Profile
    d.tap(103, 159, 26);   // Resources
    d.tap(539, 244, 10);   // Back
}

SHOT(s11_packs) {
    Director d(out, "s11");
    d.idle(10);
    d.shot("s11_packs");
    d.seconds(0.4f);
    d.tap(300, 177, 30);   // Packs & add-ons
    d.tap(426, 168, 30);   // Edit JSON files
    d.tap(200, 156, 40);   // the Nether's generator file
}

SHOT(s12_store) {
    Director d(out, "s12");
    d.idle(10);
    d.shot("s12_store");
    d.seconds(0.3f);
    d.tap(222, 149, 24);   // Store
    // Scroll through the cards with a finger.
    d.down(2, 300, 200);
    for (int i = 0; i < 30; ++i) {
        d.move(2, 300, 200 - i * 5);
        d.frame();
    }
    d.up(2, 300, 50);
    d.frames(20);
    d.tap(90, 44, 24);     // Skins
    d.tap(250, 44, 24);    // Textures
}

SHOT(s13_skins) {
    Director d(out, "s13");
    d.idle(10);
    d.shot("s13_skins");
    d.seconds(0.3f);
    d.tap(378, 149, 30);   // Profile & skins
    d.tap(405, 217, 24);   // from behind
    d.tap(405, 217, 24);
}

SHOT(s14_third_person) {
    Director d(out, "s14");
    d.settings().cameraMode = 1;
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    Player& p = s.player();
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(4, i), 1};
    d.give(0, RUBY_PICKAXE);
    s.selectedSlot() = 0;
    p.yaw = 1.2f;
    d.idle(10);
    d.shot("s14_third_person");
    d.walk(true);
    d.seconds(6, [&](float k) { p.yaw = 1.2f + 0.8f * std::sin(k * 3); });
    d.walk(false);
    d.settings().cameraMode = 2;   // from the front
    d.seconds(2.5f, [&](float k) { p.yaw += 0.01f; });
}

SHOT(s15_creative) {
    Director d(out, "s15");
    d.openWorld("Creative", 777, GameMode::Creative);
    Session& s = d.s();
    s.player().pitch = -0.15f;
    d.idle(10);
    d.shot("s15_creative");
    d.seconds(0.4f);
    d.tap(Director::kMenuX, Director::kMenuY, 18);   // the creative inventory
    d.tap(114, 60, 18);
    d.tap(114, 84, 14);    // Tools
    d.tap(175, 36, 10);
    d.tap(114, 108, 18);   // Armour
    d.tap(114, 132, 18);   // Materials
    d.tap(114, 36, 10);    // Building
    d.tap(195, 36, 14);
    d.game->onBack();
    d.frames(8);
    // Fly up: double tap jump, then hold it.
    d.tap(Director::kJumpX, Director::kJumpY, 2);
    d.tap(Director::kJumpX, Director::kJumpY, 2);
    d.down(3, Director::kJumpX, Director::kJumpY);
    d.seconds(1.5f);
    d.up(3, Director::kJumpX, Director::kJumpY);
    d.walk(true);
    d.seconds(3, [&](float k) { s.player().pitch = -0.35f; s.player().yaw += 0.004f; });
    d.walk(false);
}

SHOT(s16_structures) {
    Director d(out, "s16");
    d.openWorld("Creative", 777, GameMode::Creative);
    Session& s = d.s();
    {
        World& w = s.currentWorld();
        Player& p = s.player();
        const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
        flatten(w, x - 14, z - 16, x + 14, z + 12, y);
        p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
        p.yaw = 0;
    }
    s.player().pitch = -0.25f;
    d.idle(10);
    d.shot("s16_structures");
    d.seconds(0.3f);
    d.game->onBack();      // pause
    d.frames(12);
    d.tap(300, 142, 24);   // Structures
    d.tap(164, 222, 16);   // turn 90 degrees
    d.tap(436, 222, 10);   // place in front
    d.seconds(0.6f);
    d.down(7, 59.5f, 243.5f);    // step back to see it
    d.seconds(2.2f);
    d.up(7, 59.5f, 243.5f);
    d.seconds(1.2f, [&](float k) { s.player().pitch += (0.05f - s.player().pitch) * 0.1f; });
}

SHOT(s17_languages) {
    Director d(out, "s17");
    d.idle(10);
    d.shot("s17_languages");
    const Language order[] = {Language::English, Language::Czech, Language::Spanish, Language::Russian, Language::German,
                              Language::English};
    for (Language l : order) {
        setLanguage(l);
        d.seconds(1.1f);
    }
}

// ---------------------------------------------------------------- build 10 - 12: the world grows

SHOT(s18_doors_chest) {
    Director d(out, "s18");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 6, z - 8, x + 6, z + 3, y);
    for (int dx = -3; dx <= 3; ++dx)
        for (int dy = 0; dy < 4; ++dy)
            if (!(dx == 0 && dy < 2)) w.setBlock(x + dx, y + dy, z - 4, dy == 3 ? COBBLESTONE : PLANKS);
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, OAK_DOOR_ITEM, 1);
    d.give(1, CHEST, 1);
    d.give(9, APPLE, 6);
    d.give(10, RUBY, 4);
    d.idle(30);
    d.shot("s18_doors_chest");
    d.use(topOf(x, y - 1, z - 4));       // the door in the gap
    d.seconds(0.4f);
    d.use(centre(x, y, z - 4));          // open
    d.seconds(0.6f);
    d.use(centre(x, y, z - 4));          // close
    d.seconds(0.4f);
    d.use(centre(x, y, z - 4));          // open again
    s.selectedSlot() = 1;
    d.use(topOf(x + 2, y - 1, z - 2));   // a chest
    d.seconds(0.3f);
    d.use(centre(x + 2, y, z - 2));
    d.frames(8);
    d.tap(invX(0), kTableRow0 + 20, 8);  // apples into the chest
    d.tap(invX(0), 74, 10);
    d.tap(invX(1), kTableRow0 + 20, 8);
    d.tap(invX(1), 74, 14);
    d.seconds(1);
}

SHOT(s19_biomes) {
    Director d(out, "s19");
    d.openWorld("Explorer", 4242, GameMode::Creative);
    Session& s = d.s();
    Player& p = s.player();
    const char* biomes[] = {"snowy_taiga", "desert", "mountains", "ice_plains", "ocean"};
    int k = 0;
    for (const char* b : biomes) {
        int bx = 0, bz = 0;
        if (!findBiome(s.currentWorld(), b, bx, bz)) std::printf("no %s\n", b);
        p.flying = true;
        s.currentWorld().ensureChunksAround(floorDiv(bx, 16), floorDiv(bz, 16), 2, 1000);
        int top = 0;
        for (int dx = -16; dx <= 16; dx += 4)
            for (int dz = -16; dz <= 16; dz += 4) {
                int yy = kChunkHeight - 2;
                while (yy > 1 && s.currentWorld().getBlock(bx + dx, yy, bz + dz) == AIR) --yy;
                top = std::max(top, yy);
            }
        travel(d, Vec3(bx + 0.5f, (float)top + 6, bz + 0.5f));
        p.flying = true;
        p.yaw = 0.7f + k * 1.1f;
        p.pitch = -0.25f;
        d.idle(30);
        d.shot(std::string("s19_") + std::to_string(k) + "_" + b);
        d.walk(true);
        d.seconds(3.6f, [&](float t) { p.yaw += 0.003f; });
        d.walk(false);
        ++k;
    }
}

void structureShot(const fs::path& out, const char* name, uint8_t chest, float standDist, float height) {
    Director d(out, name);
    int cx, cy, cz;
    if (!findBlock(4242, Dimension::Overworld, chest, cx, cy, cz, 110)) return;
    d.openWorld("Trailer", 4242, GameMode::Creative);
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(cx + standDist + 0.5f, cy + height, cz + 0.5f));
    p.flying = true;
    d.lookNow(centre(cx, cy, cz));
    d.idle(40);
    d.shot(name);
    d.seconds(4, [&](float k) {
        p.flying = true;
        float a = k * 0.9f;
        float ex = cx + 0.5f + std::cos(a) * standDist, ez = cz + 0.5f + std::sin(a) * standDist;
        int clear = (int)(cy + height);
        while (clear < kChunkHeight - 2 && (s.currentWorld().getBlock((int)std::floor(ex), clear, (int)std::floor(ez)) != AIR ||
                                             s.currentWorld().getBlock((int)std::floor(ex), clear + 1, (int)std::floor(ez)) != AIR))
            ++clear;
        p.pos = Vec3(ex, std::max(cy + height - k * height * 0.4f, (float)clear), ez);
        d.lookNow(centre(cx, cy, cz));
    });
}
SHOT(s20_ruin) { structureShot(out, "s20_ruin", LOOT_CHEST_RUIN, 9, 5); }
SHOT(s21_shipwreck) { structureShot(out, "s21_shipwreck", LOOT_CHEST_SHIP, 8, 4); }
SHOT(s22_dungeon) { structureShot(out, "s22_dungeon", LOOT_CHEST_DUNGEON, 3.5f, 1.5f); }
SHOT(s23_mineshaft) { structureShot(out, "s23_mineshaft", LOOT_CHEST_MINE, 4, 1.5f); }

SHOT(s24_boat) {
    Director d(out, "s24");
    d.settings().cameraMode = 1;
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    // A shore: land with deep water a few blocks east.
    int sx = 0, sz = 0, sy = 0, ox = 0, oz = 0;
    findBiome(w, "ocean", ox, oz);
    // Walk west from the open sea to the first land; stand there facing the water.
    for (int x = ox; x > ox - 400; --x) {
        if (x % 16 == 0) w.ensureChunksAround(floorDiv(x, 16), floorDiv(oz, 16), 1, 1000);
        int y = feetY(w, x, oz);
        if (w.getBlock(x, y - 1, oz) != WATER) {
            sx = x, sy = y, sz = oz;
            break;
        }
    }
    std::printf("shore at %d %d %d\n", sx, sy, sz);
    travel(d, Vec3(sx + 0.5f, (float)sy, sz + 0.5f));
    Player& p = s.player();
    p.yaw = kPi / 2;
    s.inventory().clear();
    d.give(0, BOAT);
    d.idle(20);
    d.shot("s24_boat");
    d.use(topOf(sx + 3, sy - 2, sz));    // the boat onto the water
    d.seconds(0.4f);
    d.use(Vec3(sx + 3.5f, sy - 0.6f, sz + 0.5f));  // get in
    d.walk(true);
    d.seconds(6, [&](float k) { p.yaw = kPi / 2 + 0.4f * std::sin(k * 3); });
    d.walk(false);
}

SHOT(s25_chat) {
    Director d(out, "s25");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    s.player().yaw = 1.0f;
    d.idle(10);
    d.shot("s25_chat");
    d.seconds(0.6f);
    d.say("Hi! Who wants to build a castle?");
    d.seconds(1.2f);
    d.say("/time set noon");
    d.seconds(1.0f);
    d.say("/give ruby 5");
    d.seconds(1.0f);
    d.say("/weather rain");
    d.seconds(2.5f);
}

SHOT(s26_lan) {
    Director d(out, "s26");
    WorldInfo hostInfo;
    hostInfo.folder = createWorldFolder((d.data / "host_worlds").string());
    hostInfo.name = "Island";
    hostInfo.seed = 4242;
    hostInfo.mode = GameMode::Survival;
    Session hostWorld(hostInfo, PlayerState{});
    Multiplayer host;
    std::string err;
    std::vector<uint8_t> friendSkin = builtinSkins()[3].rgba;
    host.host(hostWorld, "Island", "Friend", friendSkin, false, err);
    Settings hs;
    hs.renderDistance = 2;
    auto pump = [&] {
        hostWorld.update(1 / kFps, d.t, 600, 270, hs);
        host.update(&hostWorld, 1 / kFps, d.t);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    };
    d.idle(4);
    d.shot("s26_lan");
    d.tap(222, 121, 0);   // Play
    for (int i = 0; i < 40; ++i) d.frame(pump);
    d.tap(300, 33, 0);    // the LAN game
    for (int i = 0; i < 400 && !d.game->session(); ++i) {
        bool r = d.recording;
        d.recording = i % 3 == 0 && r;
        d.frame(pump);
        d.recording = r;
    }
    for (int i = 0; i < 40; ++i) d.frame(pump);
    Player& me = d.s().player();
    me.yaw = 0;
    me.pitch = -0.05f;
    Player& f = hostWorld.player();
    f.pos = me.pos + Vec3(3, 0, -9);
    for (int i = 0; i < 120; ++i) d.frame([&] {
            pump();
            Vec3 goal = me.pos + Vec3(0.5f, 0, -3);
            Vec3 dir = goal - f.pos;
            float dist = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            f.yaw = yawTo(f.pos, goal);
            hostWorld.autopilot.moveZ = dist > 0.6f ? 1 : 0;
            if (dist <= 0.6f) f.yaw = yawTo(f.pos, me.pos);
        });
    d.say("Friend joined the game");
    for (int i = 0; i < 40; ++i) d.frame(pump);
    host.stop();
}

// ---------------------------------------------------------------- 1.1: the Crimson Void update

SHOT(s27_nether_portal) {
    Director d(out, "s27");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 5, z - 7, x + 5, z + 3, y);
    obsidianFrame(w, x - 1, y, z - 4);
    p.pos = Vec3(x + 0.5f, (float)y, z + 1.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, FLINT_AND_STEEL);
    d.idle(30);
    d.shot("s27_nether_portal");
    d.seconds(0.5f, [&](float) { d.look(centre(x, y + 1, z - 4), 0.2f); });
    d.use(topOf(x, y - 1, z - 4));      // light it
    d.seconds(0.8f, [&](float) { d.look(centre(x, y + 1, z - 4), 0.2f); });
    d.walk(true);
    for (int i = 0; i < 200 && s.currentWorld().dimension() != Dimension::Nether; ++i)
        d.frame([&] { d.look(centre(x, y + 1, z - 5), 0.2f); });
    d.walk(false);
    d.seconds(4);   // the loading screen, then the Nether
    s.player().yaw += 0.0f;
    d.seconds(2, [&](float) { s.player().yaw += 0.01f; });
}

SHOT(s28_nether_meadow) {
    Director d(out, "s28");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    goNether(d);
    Session& s = d.s();
    Player& p = s.player();
    d.settings().cameraMode = 1;
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(3, i), 1};
    d.give(0, STEEL_SWORD);
    s.selectedSlot() = 0;
    d.idle(10);
    d.shot("s28_nether_meadow");
    d.walk(true);
    d.seconds(6, [&](float k) {
        heal(d);
        p.yaw += 0.006f * std::sin(k * 7);
    });
    d.walk(false);
}

SHOT(s29_fortress_void_portal) {
    Director d(out, "s29");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    goNether(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    int fx, fy, fz;
    w.nearestStructure((int)s.player().pos.x, (int)s.player().pos.z, fx, fy, fz);
    travel(d, Vec3(fx + 0.5f, fy + 1.0f, fz + 9.5f));
    Player& p = s.player();
    s.inventory().clear();
    d.give(0, VOID_FLINT_AND_STEEL);
    int bottom = fy;
    for (int y = fy - 3; y < fy + 8; ++y)
        if (w.getBlock(fx, y, fz) == VOID_STONE) {
            bottom = y;
            break;
        }
    p.pos = Vec3(fx + 0.5f, (float)bottom + 1, fz + 6.5f);
    d.lookNow(centre(fx, bottom + 5, fz));
    d.idle(20);
    d.shot("s29_fortress_void_portal");
    d.seconds(1.5f, [&](float) { heal(d); d.look(centre(fx, bottom + 6, fz), 0.05f); });
    d.use(topOf(fx, bottom, fz));
    if (w.getBlock(fx, bottom + 1, fz) != VOID_PORTAL)
        for (int dy = 0; dy < 6; ++dy)
            if (tryLightPortal(w, fx, bottom + dy, fz, true)) break;
    d.seconds(1.5f, [&](float) { heal(d); d.look(centre(fx, bottom + 4, fz), 0.08f); });
    d.walk(true);
    for (int i = 0; i < 200 && s.currentWorld().dimension() != Dimension::Void; ++i)
        d.frame([&] { heal(d); d.look(centre(fx, bottom + 2, fz - 3), 0.2f); });
    d.walk(false);
    d.seconds(5, [&](float) { heal(d); s.player().yaw += 0.01f; });
}

void voidBiomeShot(const fs::path& out, const char* name, World::VoidBiome want) {
    Director d(out, name);
    d.openWorld("Trailer", 4242, GameMode::Survival);
    goNether(d);
    goVoid(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 0, bz = 0;
    for (int r = 0; r < 3000; r += 40) {
        bool f = false;
        for (int a = 0; a < 16 && !f; ++a) {
            int x = (int)(std::cos(a * kPi / 8) * r), z = (int)(std::sin(a * kPi / 8) * r);
            if (w.voidBiomeAt(x, z) == want && w.voidBiomeAt(x + 40, z) == want && w.voidBiomeAt(x, z + 40) == want) bx = x, bz = z, f = true;
        }
        if (f) break;
    }
    w.ensureChunksAround(floorDiv(bx, 16), floorDiv(bz, 16), 2, 1000);
    travel(d, Vec3(bx + 0.5f, (float)feetY(w, bx, bz), bz + 0.5f));
    d.settings().cameraMode = 1;
    Player& p = s.player();
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(4, i), 1};
    d.give(0, RUBY_SWORD);
    s.selectedSlot() = 0;
    p.yaw = 0.8f;
    d.idle(20);
    d.shot(name);
    d.walk(true);
    d.seconds(6, [&](float k) {
        heal(d);
        p.yaw += 0.005f * std::sin(k * 5);
    });
    d.walk(false);
}
SHOT(s30_void_forest) { voidBiomeShot(out, "s30_void_forest", World::VoidBiome::Purple); }
SHOT(s31_crimson_desert) { voidBiomeShot(out, "s31_crimson_desert", World::VoidBiome::Crimson); }

SHOT(s32_boss) {
    Director d(out, "s32");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    goNether(d);
    goVoid(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    int ax, ay, az;
    w.nearestStructure((int)s.player().pos.x, (int)s.player().pos.z, ax, ay, az);
    w.ensureChunksAround(floorDiv(ax, 16), floorDiv(az, 16), 3, 1000);
    travel(d, Vec3(ax + 0.5f, (float)ay, az + 14.5f));
    d.settings().cameraMode = 1;
    Player& p = s.player();
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(4, i), 1};
    d.give(0, RUBY_SWORD);
    s.selectedSlot() = 0;
    p.yaw = 0;
    p.pitch = 0.05f;
    d.idle(10);
    d.shot("s32_boss");
    d.walk(true);
    for (int i = 0; i < 100 && !s.mobs().boss(2); ++i) d.frame([&] { heal(d); p.yaw = yawTo(p.pos, Vec3(ax + 0.5f, 0, az + 0.5f)); });
    d.walk(false);
    // The fight: hit him whenever he is close.
    for (int i = 0; i < 260; ++i) {
        d.frame([&] {
            heal(d);
            const Mob* b = s.mobs().boss(2);
            if (!b) return;
            Vec3 at = b->pos + Vec3(0, 1.6f, 0);
            d.look(at, 0.3f);
            float dist = std::hypot(b->pos.x - p.pos.x, b->pos.z - p.pos.z);
            s.autopilot.moveZ = dist > 3.0f ? 1.0f : 0.0f;
            if (i % 9 == 0 && dist < 4.5f) {
                d.down(1, Director::kCenterX, Director::kCenterY);
            } else if (i % 9 == 2) {
                d.up(1, Director::kCenterX, Director::kCenterY);
            }
        });
    }
    s.autopilot.moveZ = 0;
}

void meadow(Director& d, int& x, int& y, int& z) {
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 12, z - 16, x + 12, z + 6, y);
}

SHOT(s33_animals) {
    Director d(out, "s33");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 3.5f);
    p.yaw = 0;
    p.pitch = -0.1f;
    for (int i = 0; i < 4; ++i) s.mobs().spawn(MobType::Pig, 0, Vec3(x - 4.0f + i * 2.2f, (float)y, z - 6.0f - (i % 2) * 2));
    for (int i = 0; i < 4; ++i) s.mobs().spawn(MobType::Sheep, 0, Vec3(x - 3.0f + i * 2.4f, (float)y, z - 11.0f + (i % 2)));
    d.give(0, IRON_SWORD);
    s.selectedSlot() = 0;
    d.idle(10);
    d.shot("s33_animals");
    d.seconds(3, [&](float k) { p.yaw = 0.3f * std::sin(k * 2.5f); });
    // Go for the nearest pig.
    for (int i = 0; i < 90; ++i) d.frame([&] {
            const Mob* pig = nullptr;
            float best = 1e9f;
            for (const Mob& m : s.mobs().mobs())
                if (m.type == MobType::Pig && m.alive()) {
                    float dd = std::hypot(m.pos.x - p.pos.x, m.pos.z - p.pos.z);
                    if (dd < best) best = dd, pig = &m;
                }
            if (!pig) return;
            d.look(pig->pos + Vec3(0, 0.5f, 0), 0.3f);
            s.autopilot.moveZ = best > 2.2f ? 1.0f : 0.0f;
            if (best < 3 && i % 8 == 0) d.down(1, Director::kCenterX, Director::kCenterY);
            if (i % 8 == 2) d.up(1, Director::kCenterX, Director::kCenterY);
        });
    s.autopilot.moveZ = 0;
    d.seconds(1.5f);
}

SHOT(s34_night_monsters) {
    Director d(out, "s34");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 3.5f);
    p.yaw = 0;
    p.pitch = 0.25f;
    s.setTimeOfDay(17600);
    d.give(0, STEEL_SWORD);
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(2, i), 1};
    s.selectedSlot() = 0;
    d.idle(10);
    d.shot("s34_night_monsters");
    d.seconds(2, [&](float k) { p.pitch = 0.25f - 0.3f * smooth(k); });
    for (int i = 0; i < 3; ++i) s.mobs().spawn(MobType::ForgottenMan, 0, Vec3(x - 4.0f + i * 4, (float)y, z - 12.0f));
    s.mobs().spawn(MobType::PumpkinSlime, 0, Vec3(x + 6.0f, (float)y, z - 8.0f));
    for (int i = 0; i < 170; ++i) d.frame([&] {
            heal(d);
            const Mob* t = nullptr;
            float best = 1e9f;
            for (const Mob& m : s.mobs().mobs())
                if (mobInfo(m.type).hostile && m.alive()) {
                    float dd = std::hypot(m.pos.x - p.pos.x, m.pos.z - p.pos.z);
                    if (dd < best) best = dd, t = &m;
                }
            if (!t) return;
            d.look(t->pos + Vec3(0, 1.2f, 0), 0.25f);
            if (best < 3.5f && i % 7 == 0) d.down(1, Director::kCenterX, Director::kCenterY);
            if (i % 7 == 2) d.up(1, Director::kCenterX, Director::kCenterY);
        });
}

SHOT(s35_bed) {
    Director d(out, "s35");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0;
    s.setTimeOfDay(19000);
    s.inventory().clear();
    d.give(0, BED_ITEM);
    d.idle(10);
    d.shot("s35_bed");
    d.seconds(0.5f);
    d.use(topOf(x, y - 1, z - 2));
    d.seconds(0.4f);
    d.use(centre(x, y, z - 2));
    d.seconds(4.5f);
}

SHOT(s36_sky) {
    Director d(out, "s36");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y + 0.0f, z + 0.5f);
    d.settings().hideGui = true;
    d.idle(10);
    d.shot("s36_sky");
    // A whole day in ten seconds: morning, day, evening, night, midnight.
    d.seconds(10, [&](float k) {
        s.setTimeOfDay(23000 + k * 24000);
        p.yaw = -kPi / 2 + k * kPi;
        p.pitch = 0.35f;
    });
    d.settings().hideGui = false;
}

SHOT(s37_weather) {
    Director d(out, "s37");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    Player& p = s.player();
    p.yaw = 0.8f;
    s.setTimeOfDay(5000);
    s.setWeather(true);
    d.idle(160);
    d.shot("s37_0_rain");
    d.walk(true);
    d.seconds(3.5f, [&](float) { p.yaw += 0.003f; });
    d.walk(false);
    int bx, bz;
    findBiome(s.currentWorld(), "snowy_taiga", bx, bz);
    travel(d, Vec3(bx + 0.5f, (float)feetY(s.currentWorld(), bx, bz), bz + 0.5f));
    d.idle(120);
    d.shot("s37_1_snow");
    d.walk(true);
    d.seconds(3.5f, [&](float) { p.yaw += 0.003f; });
    d.walk(false);
}

// ---------------------------------------------------------------- 1.2: powerstone, TNT, saplings...

SHOT(s39_powerstone) {
    Director d(out, "s39");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    // A switch, a cable along the ground, three lamps and an iron door in a little wall.
    w.setBlock(x - 3, y, z - 3, SWITCH);
    for (int dx = -2; dx <= 4; ++dx) w.setBlock(x + dx, y, z - 3, POWER_CABLE);
    for (int k = 0; k < 3; ++k) w.setBlock(x - 1 + k * 2, y, z - 4, POWERSTONE_LAMP);
    for (int dz = -5; dz >= -7; --dz)
        for (int dy = 0; dy < 3; ++dy) w.setBlock(x + 5, y + dy, z + dz, dz == -6 && dy < 2 ? AIR : STONE_BRICKS);
    w.setBlock(x + 5, y, z - 4, POWER_CABLE);
    w.setBlock(x + 5, y, z - 6, doorId(IRON_DOOR, DoorState{3, false, false}));
    DoorState up{3, false, true};
    w.setBlock(x + 5, y + 1, z - 6, doorId(IRON_DOOR, up));
    w.setBlock(x + 5, y, z - 5, POWER_CABLE);
    p.pos = Vec3(x - 1.5f, (float)y, z + 0.5f);
    p.yaw = 0.2f;
    p.pitch = -0.35f;
    s.inventory().clear();
    d.give(0, POWER_CABLE, 16);
    d.idle(30);
    d.shot("s39_powerstone");
    d.seconds(1, [&](float) { d.look(centre(x, y, z - 3), 0.1f); });
    d.use(topOf(x - 3, y - 1, z - 2));   // one more cable by hand
    d.seconds(0.5f);
    s.selectedSlot() = 1;               // empty hand
    const Vec3 sw(x - 2.5f, y + 0.12f, z - 2.5f);
    d.use(sw);                           // flip the switch
    std::printf("switch now %d, lamp %d\n", s.currentWorld().getBlock(x - 3, y, z - 3), s.currentWorld().getBlock(x - 1, y, z - 4));
    d.seconds(2.5f, [&](float k) { d.look(centre(x + 2, y + 1, z - 5), 0.05f); });
    d.use(sw);                           // and off again
    d.seconds(1.0f);
    d.use(sw);
    d.seconds(1.5f);
}

SHOT(s40_tnt) {
    Director d(out, "s40");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = 0; dy < 2; ++dy)
            for (int dz = -9; dz >= -10; --dz) w.setBlock(x + dx, y + dy, z + dz, TNT);
    for (int dx = -4; dx <= 4; ++dx)
        for (int dy = 0; dy < 3; ++dy) w.setBlock(x + dx, y + dy, z - 12, STONE_BRICKS);
    p.pos = Vec3(x + 0.5f, (float)y, z - 6.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, FLINT_AND_STEEL);
    d.idle(30);
    d.shot("s40_tnt");
    d.seconds(0.6f, [&](float) { d.look(centre(x, y + 1, z - 9), 0.2f); });
    d.use(centre(x, y, z - 9));
    // Run!
    d.down(7, 59.5f, 243.5f);
    d.seconds(1.6f, [&](float) { d.look(centre(x, y + 1, z - 9), 0.3f); });
    d.up(7, 59.5f, 243.5f);
    d.seconds(4.5f, [&](float) { heal(d); d.look(centre(x, y + 1, z - 9), 0.2f); });
}

SHOT(s41_bucket) {
    Director d(out, "s41");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -5; dz >= -6; --dz) w.setBlock(x + dx, y - 1, z + dz, WATER);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z - 2.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, BUCKET, 1);
    d.give(1, LAVA_BUCKET, 1);
    d.idle(30);
    d.shot("s41_bucket");
    d.use(Vec3(x + 0.5f, y - 0.1f, z - 4.5f));     // scoop water
    d.seconds(0.8f);
    d.use(topOf(x + 3, y - 1, z - 4));              // pour it out over there
    d.seconds(1.0f);
    s.selectedSlot() = 1;
    d.use(topOf(x - 3, y - 1, z - 4));              // and lava
    d.seconds(2.0f, [&](float) { d.look(centre(x - 3, y, z - 4), 0.05f); });
}

SHOT(s42_saplings) {
    Director d(out, "s42");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 1.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, OAK_SAPLING, 4);
    d.give(1, PINE_SAPLING, 4);
    d.idle(30);
    d.shot("s42_saplings");
    s.selectedSlot() = 0;
    d.use(topOf(x - 3, y - 1, z - 2));
    s.selectedSlot() = 1;
    d.use(topOf(x, y - 1, z - 3));
    s.selectedSlot() = 0;
    d.use(topOf(x + 3, y - 1, z - 2));
    // Twenty seconds later (in a time-lapse)...
    d.stepsPerFrame = 6;
    d.down(7, 59.5f, 243.5f);
    d.seconds(4.2f, [&](float) { p.pitch += (0.25f - p.pitch) * 0.05f; if (p.pos.z > z + 6) d.up(7, 59.5f, 243.5f); });
    d.up(7, 59.5f, 243.5f);
    d.stepsPerFrame = 1;
    d.seconds(1.5f);
}

SHOT(s43_eggs_swords) {
    Director d(out, "s43");
    d.openWorld("Trailer", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, EGG_PIG, 3);
    d.give(1, EGG_SHEEP, 3);
    d.give(2, EGG_FORGOTTEN, 1);
    d.give(3, RUBY_SWORD, 1);
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(4, i), 1};
    d.idle(30);
    d.shot("s43_eggs_swords");
    s.selectedSlot() = 0;
    d.use(topOf(x - 2, y - 1, z - 3));
    s.selectedSlot() = 1;
    d.use(topOf(x + 2, y - 1, z - 3));
    s.selectedSlot() = 2;
    d.use(topOf(x, y - 1, z - 4));
    s.selectedSlot() = 3;
    for (int i = 0; i < 110; ++i) d.frame([&] {
            heal(d);
            const Mob* t = nullptr;
            for (const Mob& m : s.mobs().mobs())
                if (m.type == MobType::ForgottenMan && m.alive()) t = &m;
            if (!t) return;
            d.look(t->pos + Vec3(0, 1.3f, 0), 0.3f);
            float dist = std::hypot(t->pos.x - p.pos.x, t->pos.z - p.pos.z);
            if (dist < 3.5f && i % 7 == 0) d.down(1, Director::kCenterX, Director::kCenterY);
            if (i % 7 == 2) d.up(1, Director::kCenterX, Director::kCenterY);
        });
    d.seconds(1);
}

SHOT(s44_void_tower) {
    Director d(out, "s44");
    int cx, cy, cz;
    if (!findBlock(4242, Dimension::Void, LOOT_CHEST_VOID, cx, cy, cz, 30)) return;
    d.openWorld("Trailer", 4242, GameMode::Survival);
    goNether(d);
    goVoid(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    w.ensureChunksAround(floorDiv(cx, 16), floorDiv(cz, 16), 2, 1000);
    // Stand next to the chest, on a free cell.
    Vec3 stand(cx + 0.5f, (float)cy, cz + 0.5f);
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (auto& dd : dirs)
        for (int k = 2; k >= 1; --k)
            if (w.getBlock(cx + dd[0] * k, cy, cz + dd[1] * k) == AIR && w.getBlock(cx + dd[0] * k, cy + 1, cz + dd[1] * k) == AIR &&
                isSolid(w.getBlock(cx + dd[0] * k, cy - 1, cz + dd[1] * k))) {
                stand = Vec3(cx + dd[0] * k + 0.5f, (float)cy, cz + dd[1] * k + 0.5f);
                goto found;
            }
found:
    travel(d, stand);
    s.inventory().clear();
    d.give(0, RUBY_SWORD);
    d.lookNow(centre(cx, cy, cz));
    d.idle(10);
    d.shot("s44_void_tower");
    d.seconds(0.8f, [&](float) { heal(d); });
    d.use(centre(cx, cy, cz));
    d.seconds(2.5f, [&](float) { heal(d); });
    d.game->onBack();
    d.frames(4);
    d.give(1, BOSS_COMPASS);
    s.selectedSlot() = 1;
    d.seconds(3, [&](float k) { heal(d); s.player().yaw += 0.02f; s.player().pitch *= 0.95f; });
}

SHOT(s45_temple) { structureShot(out, "s45_temple", LOOT_CHEST_TEMPLE, 10, 6); }
SHOT(s46_igloo) { structureShot(out, "s46_igloo", LOOT_CHEST_IGLOO, 7, 4); }
SHOT(s47_watchtower) { structureShot(out, "s47_watchtower", LOOT_CHEST_TOWER, 10, 7); }

// ---------------------------------------------------------------- 1.2: the store

SHOT(s48_vip) {
    Director d(out, "s48");
    d.game->toolProfile().coins = 4200;
    d.idle(10);
    d.shot("s48_vip");
    d.tap(222, 149, 20);   // Store
    d.tap(390, 44, 24);    // VIP and addons
    d.tap(256, 134.5f, 18);  // Buy VIP
    d.tap(219, 162.5f, 30);  // confirm
    d.seconds(1);
}

SHOT(s49_rewards_wheel) {
    Director d(out, "s49");
    Profile& pr = d.game->toolProfile();
    pr.bonusSpins = 2;
    d.idle(10);
    d.shot("s49_rewards_wheel");
    d.tap(222, 149, 16);   // Store
    d.tap(490, 44, 24);    // Rewards
    d.tap(150, 140, 30);   // claim day 1
    for (int k = 0; k < 3; ++k) {
        d.tap(455, 225, 0); // spin!
        d.seconds(4.3f);
    }
}

SHOT(s50_code) {
    Director d(out, "s50");
    d.idle(10);
    d.shot("s50_code");
    d.tap(222, 149, 16);
    d.tap(390, 44, 24);
    d.game->onTextResult(15, makeAnniversaryCode(2026));
    d.seconds(2.5f);
}

void addonWorld(Director& d, const char* addon) {
    Profile& pr = d.game->toolProfile();
    pr.owned.insert(addon);
    d.openWorld("Trailer", 4242, GameMode::Survival);
}

SHOT(s51_winter) {
    Director d(out, "s51");
    addonWorld(d, "addon:winter");
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    s.setTimeOfDay(4000);
    int x, y, z;
    x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    w.setBlock(x + 1, y, z - 3, GIFT_BOX);
    w.setBlock(x - 2, y, z - 4, GIFT_BOX);
    p.yaw = 0;
    d.idle(120);
    d.shot("s51_winter");
    d.seconds(2, [&](float) { d.look(centre(x + 1, y, z - 3), 0.05f); });
    d.use(centre(x + 1, y, z - 3));     // a present!
    d.seconds(1.5f);
    d.walk(true);
    d.seconds(3, [&](float k) { p.yaw += 0.004f; p.pitch *= 0.9f; });
    d.walk(false);
}

SHOT(s52_halloween) {
    Director d(out, "s52");
    addonWorld(d, "addon:halloween");
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 4.5f);
    p.yaw = 0;
    s.setTimeOfDay(12600);
    d.give(0, STEEL_SWORD);
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(3, i), 1};
    for (int i = 0; i < 6; ++i)
        s.mobs().spawn(MobType::ForgottenMan, 0, Vec3(x - 5.0f + i * 2.0f, (float)y, z - 12.0f - (i % 2) * 2));
    d.idle(20);
    d.shot("s52_halloween");
    for (int i = 0; i < 170; ++i) d.frame([&] {
            heal(d);
            const Mob* t = nullptr;
            float best = 1e9f;
            for (const Mob& m : s.mobs().mobs())
                if (m.type == MobType::ForgottenMan && m.alive()) {
                    float dd = std::hypot(m.pos.x - p.pos.x, m.pos.z - p.pos.z);
                    if (dd < best) best = dd, t = &m;
                }
            if (!t) return;
            d.look(t->pos + Vec3(0, 1.3f, 0), 0.15f);
            if (best < 3.5f && i % 7 == 0) d.down(1, Director::kCenterX, Director::kCenterY);
            if (i % 7 == 2) d.up(1, Director::kCenterX, Director::kCenterY);
        });
}

SHOT(s53_nuke) {
    Director d(out, "s53");
    d.game->toolProfile().owned.insert("addon:halloween");
    d.openWorld("Trailer", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    s.setTimeOfDay(5000);
    w.setBlock(x, y, z - 14, NUKE_TNT);
    p.pos = Vec3(x + 0.5f, (float)y, z - 11.0f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, FLINT_AND_STEEL);
    d.idle(30);
    d.shot("s53_nuke");
    d.use(centre(x, y, z - 14));
    d.down(7, 59.5f, 243.5f);
    d.seconds(2.8f, [&](float) { heal(d); d.look(centre(x, y + 1, z - 14), 0.3f); });
    d.up(7, 59.5f, 243.5f);
    d.seconds(9, [&](float) { heal(d); d.look(centre(x, y, z - 14), 0.2f); });
}

SHOT(s54_anniversary) {
    Director d(out, "s54");
    addonWorld(d, "addon:anniversary");
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    w.setBlock(x, y, z - 3, CAKE);
    w.setBlock(x - 2, y, z - 4, CAKE);
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0.1f;
    p.pitch = 0.15f;
    d.idle(30);
    d.shot("s54_anniversary");
    d.seconds(3, [&](float k) { p.yaw = 0.1f - 0.3f * k; p.pitch = 0.15f - 0.4f * k; });
    d.use(centre(x, y, z - 3));
    d.seconds(2);
}

// ---------------------------------------------------------------- 1.3.0

bool findVillage(Director& d, int& vx, int& vy, int& vz) {
    World& w = d.s().currentWorld();
    const Vec3 p = d.s().player().pos;
    if (!w.nearestStructure((int)p.x, (int)p.z, vx, vy, vz)) return false;
    std::printf("village at %d %d %d\n", vx, vy, vz);
    return true;
}

SHOT(v01_village_fly) {
    Director d(out, "v01");
    d.openWorld("Village", 4242, GameMode::Creative);
    int vx, vy, vz;
    if (!findVillage(d, vx, vy, vz)) return;
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(vx + 0.5f, vy + 22.0f, vz + 30.5f));
    d.idle(60);
    d.settings().hideGui = true;
    d.shot("v01_village_fly");
    d.seconds(7, [&](float k) {
        p.flying = true;
        float a = kPi / 2 + k * 1.6f;
        p.pos = Vec3(vx + 0.5f + std::cos(a) * 30, vy + 20 - k * 8, vz + 0.5f + std::sin(a) * 30);
        d.lookNow(Vec3(vx + 0.5f, (float)vy, vz + 0.5f));
    });
}

SHOT(v02_play_menu) {
    Director d(out, "v02");
    d.game->toolSettings().calmMode = false;
    d.idle(10);
    d.shot("v02_play_menu");
    d.seconds(0.4f);
    d.tap(222, 121, 30);   // Play: Singleplayer
    d.tap(182, 15, 40);    // Multiplayer
    d.tap(80, 15, 20);     // back to Singleplayer
}

// A field: farmland with a water channel, planted, grown and harvested.
SHOT(v03_farming) {
    Director d(out, "v03");
    d.openWorld("Farm", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    for (int dz = -2; dz >= -6; --dz) w.setBlock(x - 3, y - 1, z + dz, WATER);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = -0.25f;
    p.pitch = -0.6f;
    s.inventory().clear();
    d.give(0, HOE);
    d.give(1, WHEAT_SEEDS, 16);
    d.idle(30);
    d.shot("v03_farming");
    for (int dz = -2; dz >= -4; --dz)
        for (int dx = -2; dx <= -1; ++dx) d.use(topOf(x + dx, y - 1, z + dz));
    s.selectedSlot() = 1;
    for (int dz = -2; dz >= -4; --dz)
        for (int dx = -2; dx <= -1; ++dx) d.use(Vec3(x + dx + 0.5f, y - 1 + 0.9f, z + dz + 0.5f));
    d.seconds(0.5f);
    // Half a minute later...
    d.stepsPerFrame = 8;
    d.seconds(4.5f);
    d.stepsPerFrame = 1;
    d.seconds(0.6f);
    s.selectedSlot() = 2;
    for (int dz = -2; dz >= -3; --dz)
        for (int dx = -2; dx <= -1; ++dx) d.mine(x + dx, y, z + dz, 30);
    d.seconds(1);
}

SHOT(v04_animals) {
    Director d(out, "v04");
    d.openWorld("Farm", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 3.5f);
    p.yaw = 0;
    p.pitch = -0.2f;
    s.mobs().mobs().clear();
    for (int i = 0; i < 5; ++i) s.mobs().spawn(MobType::Chicken, 0, Vec3(x - 4.0f + i * 1.7f, (float)y, z - 4.0f - (i % 2) * 1.5f));
    for (int i = 0; i < 3; ++i) s.mobs().spawn(MobType::Cow, 0, Vec3(x - 3.0f + i * 3.0f, (float)y, z - 9.0f));
    s.inventory().clear();
    d.give(0, BUCKET, 1);
    d.idle(10);
    d.shot("v04_animals");
    d.seconds(3, [&](float k) { p.yaw = 0.25f * std::sin(k * 3); });
    // Walk to a cow and milk it.
    for (int i = 0; i < 120; ++i) {
        const Mob* cow = nullptr;
        float best = 1e9f;
        for (const Mob& m : s.mobs().mobs())
            if (m.type == MobType::Cow) {
                float dd = std::hypot(m.pos.x - p.pos.x, m.pos.z - p.pos.z);
                if (dd < best) best = dd, cow = &m;
            }
        if (!cow) break;
        const Vec3 at = cow->pos + Vec3(0, 0.9f, 0);
        if (best < 2.4f) {
            d.lookNow(at);
            d.tap(Director::kCenterX, Director::kCenterY, 2);
            if (s.inventory().slots[0].id == MILK_BUCKET) break;
            continue;
        }
        s.autopilot.moveZ = 1;
        d.frame([&] { d.look(at, 0.3f); });
    }
    s.autopilot.moveZ = 0;
    d.seconds(2);
}

SHOT(v05_bow) {
    Director d(out, "v05");
    d.openWorld("Farm", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    // Targets: hay bales.
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = 0; dy < 2; ++dy) w.setBlock(x + dx * 3, y + dy, z - 12, HAY_BALE);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, BOW);
    d.give(1, ARROW, 32);
    s.mobs().mobs().clear();
    d.idle(20);
    d.shot("v05_bow");
    const Vec3 targets[3] = {centre(x - 3, y + 1, z - 12), centre(x, y + 1, z - 12), centre(x + 3, y + 1, z - 12)};
    for (const Vec3& at : targets) {
        d.frames(8, [&](int) { d.look(at + Vec3(0, 0.6f, 0), 0.3f); });
        d.down(1, Director::kCenterX, Director::kCenterY);
        d.frames(22, [&](int) { d.look(at + Vec3(0, 0.6f, 0), 0.3f); });
        d.up(1, Director::kCenterX, Director::kCenterY);
        d.frames(14);
    }
    // And something that moves.
    s.mobs().spawn(MobType::PumpkinSlime, 0, Vec3(x + 0.5f, (float)y, z - 9.0f));
    for (int shot = 0; shot < 3; ++shot) {
        const Mob* m = s.mobs().mobs().empty() ? nullptr : &s.mobs().mobs()[0];
        if (!m) break;
        d.frames(6, [&](int) {
            heal(d);
            if (!s.mobs().mobs().empty()) d.look(s.mobs().mobs()[0].pos + Vec3(0, 0.8f, 0), 0.35f);
        });
        d.down(1, Director::kCenterX, Director::kCenterY);
        d.frames(20, [&](int) {
            heal(d);
            if (!s.mobs().mobs().empty()) d.look(s.mobs().mobs()[0].pos + Vec3(0, 0.8f, 0), 0.35f);
        });
        d.up(1, Director::kCenterX, Director::kCenterY);
        d.frames(10, [&](int) { heal(d); });
    }
    d.seconds(1);
}

SHOT(v06_village_trade) {
    Director d(out, "v06");
    d.openWorld("Village", 4242, GameMode::Survival);
    int vx, vy, vz;
    if (!findVillage(d, vx, vy, vz)) return;
    Session& s = d.s();
    Player& p = s.player();
    travel(d, Vec3(vx + 0.5f, (float)vy, vz + 12.5f));
    d.idle(60);
    p.yaw = 0;
    p.pitch = -0.05f;
    s.inventory().clear();
    d.give(0, RUBY, 20);
    d.give(1, WHEAT, 48);
    d.give(2, COAL, 24);
    s.selectedSlot() = 3;
    d.shot("v06_village_trade");
    d.walk(true);
    d.seconds(3.0f, [&](float) {
        heal(d);
        d.look(Vec3(vx + 0.5f, vy + 1.0f, vz + 0.5f), 0.1f);
    });
    d.walk(false);
    // Up to a villager, and trade.
    for (int i = 0; i < 160; ++i) {
        const Mob* v = nullptr;
        float best = 1e9f;
        for (const Mob& m : s.mobs().mobs())
            if (m.type == MobType::Villager) {
                float dd = std::hypot(m.pos.x - p.pos.x, m.pos.z - p.pos.z);
                if (dd < best) best = dd, v = &m;
            }
        if (!v) {
            d.frame();
            continue;
        }
        const Vec3 at = v->pos + Vec3(0, 1.4f, 0);
        if (best < 2.6f) {
            s.autopilot.moveZ = 0;
            d.lookNow(at);
            d.tap(Director::kCenterX, Director::kCenterY, 4);
            if (d.game->session() && s.wantsTrade < 0) break;
            continue;
        }
        s.autopilot.moveZ = 1;
        d.frame([&] { d.look(at, 0.3f); });
    }
    s.autopilot.moveZ = 0;
    d.frames(16);
    for (auto [bx, byy] : {std::pair<float, float>{254.5f, 116}, {397.5f, 116}, {254.5f, 142}, {254.5f, 116}}) d.tap(bx, byy, 18);
    d.seconds(1.5f);
    d.game->onBack();
    d.seconds(1.0f);
}

SHOT(v07_cables) {
    Director d(out, "v07");
    d.openWorld("Farm", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    w.setBlock(x - 2, y, z - 1, SWITCH);
    w.setBlock(x + 2, y, z - 4, POWERSTONE_LAMP);
    w.setBlock(x - 2, y, z - 4, POWERSTONE_LAMP);
    w.setBlock(x, y, z - 5, POWERSTONE_LAMP);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 1.5f);
    p.yaw = 0;
    p.pitch = -0.8f;
    s.inventory().clear();
    d.give(0, POWER_CABLE, 64);
    d.idle(20);
    d.shot("v07_cables");
    // A wire that turns a corner and splits into a cross.
    const int path[][2] = {{-1, -1}, {0, -1}, {0, -2}, {0, -3}, {0, -4}, {1, -4}, {-1, -4}};
    for (auto& c : path) d.use(topOf(x + c[0], y - 1, z + c[1]));
    s.selectedSlot() = 1;
    d.use(Vec3(x - 1.5f, y + 0.12f, z - 0.5f));  // power on
    d.seconds(2.5f, [&](float k) { p.pitch = -0.8f + 0.25f * k; });
}

SHOT(v08_calm) {
    Director d(out, "v08");
    d.idle(10);
    d.shot("v08_calm");
    d.tap(378, 121, 20);   // Settings
    d.tap(103, 107, 24);   // Audio
    d.game->toolSettings().calmMode = true;
    d.seconds(2.2f);
}

SHOT(v09_lan) {
    Director d(out, "v09");
    WorldInfo hostInfo;
    hostInfo.folder = createWorldFolder((d.data / "host_worlds").string());
    hostInfo.name = "Farm Friends";
    hostInfo.seed = 4242;
    hostInfo.mode = GameMode::Survival;
    Session hostWorld(hostInfo, PlayerState{});
    Multiplayer host;
    std::string err;
    host.host(hostWorld, "Farm Friends", "Friend", builtinSkins()[2].rgba, false, err);
    Settings hs;
    hs.renderDistance = 2;
    auto pump = [&] {
        hostWorld.update(1 / kFps, d.t, 600, 270, hs);
        host.update(&hostWorld, 1 / kFps, d.t);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    };
    hostWorld.setTimeOfDay(11800);  // the host's evening sky comes along
    d.idle(4);
    d.shot("v09_lan");
    d.tap(222, 121, 0);
    for (int i = 0; i < 20; ++i) d.frame(pump);
    d.tap(182, 15, 0);    // Multiplayer
    for (int i = 0; i < 40; ++i) d.frame(pump);
    d.tap(300, 45, 0);    // the game
    for (int i = 0; i < 400 && !d.game->session(); ++i) {
        bool r = d.recording;
        d.recording = i % 3 == 0 && r;
        d.frame(pump);
        d.recording = r;
    }
    for (int i = 0; i < 50; ++i) d.frame(pump);
    Player& me = d.s().player();
    me.yaw = 0;
    me.pitch = -0.05f;
    Player& f = hostWorld.player();
    f.pos = me.pos + Vec3(2, 0, -8);
    for (int i = 0; i < 100; ++i) d.frame([&] {
            pump();
            Vec3 goal = me.pos + Vec3(0.5f, 0, -3);
            Vec3 dir = goal - f.pos;
            float dist = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            f.yaw = dist > 0.6f ? yawTo(f.pos, goal) : yawTo(f.pos, me.pos);
            hostWorld.autopilot.moveZ = dist > 0.6f ? 1 : 0;
        });
    d.game->onBack();      // pause: who is playing
    for (int i = 0; i < 50; ++i) d.frame(pump);
    host.stop();
}


// ---------------------------------------------------------------- 1.4.0

// A cavern with a lake (water or tar) and dripstone near the origin of seed 1234.
bool findCavern(Director& d, int& fx, int& fy, int& fz, bool wantTar) {
    World cw(1234, Dimension::Overworld);
    cw.ensureChunksAround(0, 0, 8, 1000);
    int best = -1;
    for (auto& [k, c] : cw.chunks())
        for (int x = 2; x < 14; x += 2)
            for (int z = 2; z < 14; z += 2)
                for (int y = kCaveLakeLevel + 1; y < 40; ++y) {
                    if (c->get(x, y, z) != AIR || !isLiquid(c->get(x, y - 1, z))) continue;
                    if (wantTar != (c->get(x, y - 1, z) == TAR)) continue;
                    int room = 0, drip = 0;
                    for (int dy = 1; dy < 8 && y + dy < kChunkHeight; ++dy) room += c->get(x, y + dy, z) == AIR;
                    for (int dx = -2; dx <= 2; ++dx)
                        for (int dz = -2; dz <= 2; ++dz)
                            for (int dy = 0; dy < 10; ++dy)
                                drip += isDripstoneSpike(c->get(x + dx, y + dy, z + dz));
                    const int score = room * 3 + drip * 2;
                    if (score > best) best = score, fx = c->cx * 16 + x, fy = y, fz = c->cz * 16 + z;
                }
    (void)d;
    return best >= 0;
}

SHOT(w01_caves) {
    Director d(out, "w01");
    d.openWorld("Caves", 1234, GameMode::Creative);
    int x, y, z;
    if (!findCavern(d, x, y, z, false)) return;
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(x + 0.5f, (float)y + 0.5f, z + 0.5f));
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    for (int i = 0; i < 4; ++i) s.mobs().spawn(MobType::Bat, 0, Vec3(x + 1.5f + i, y + 3.0f, z - 2.5f + i));
    d.idle(10);
    d.shot("w01_caves");
    d.seconds(7, [&](float k) {
        p.flying = true;
        p.pos = Vec3(x + 0.5f, y + 0.5f + k * 1.5f, z + 0.5f);
        p.yaw = 0.8f + k * 2.6f;
        p.pitch = -0.1f + 0.15f * std::sin(k * 3);
    });
}

SHOT(w02_tar) {
    Director d(out, "w02");
    d.openWorld("Tar", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    for (int dx = -3; dx <= 3; ++dx)
        for (int dz = -12; dz <= -8; ++dz) w.setBlock(x + dx, y - 1, z + dz, (std::abs(dx) + std::abs(dz + 10)) < 4 ? TAR : TAR_BLOCK);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 3.5f);
    p.yaw = 0;
    p.pitch = -0.25f;
    s.mobs().mobs().clear();
    s.mobs().spawn(MobType::TarMonster, 0, Vec3(x + 0.5f, (float)y, z - 10.0f));
    for (int i = 0; i < 3; ++i) s.mobs().spawn(MobType::Pig, 0, Vec3(x - 2.0f + i * 2, (float)y, z - 4.0f));
    d.idle(4);
    d.shot("w02_tar");
    d.seconds(9, [&](float) { p.pitch = -0.25f; });
}

SHOT(w03_tnt_zombie) {
    Director d(out, "w03");
    d.openWorld("Night", 4242, GameMode::Survival);
    Session& s = d.s();
    int x, y, z;
    meadow(d, x, y, z);
    s.setTimeOfDay(15500);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 2.5f);
    p.yaw = 0;
    p.pitch = -0.1f;
    s.mobs().mobs().clear();
    s.mobs().spawn(MobType::TntZombie, 0, Vec3(x + 0.5f, (float)y, z - 9.0f));
    d.idle(4);
    d.shot("w03_tnt_zombie");
    for (int i = 0; i < 24 * 7; ++i) {
        d.frame([&] {
            heal(d);
            p.pos.x = x + 0.5f, p.pos.z = z + 2.5f;
            if (!s.mobs().mobs().empty()) d.look(s.mobs().mobs()[0].pos + Vec3(0, 1.2f, 0), 0.2f);
        });
        if (s.mobs().countType(0, MobType::TntZombie, p.pos, 40) == 0) break;
    }
    d.seconds(2.5f, [&](float) { heal(d); });
}

SHOT(w04_spider) {
    Director d(out, "w04");
    d.openWorld("Spider", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    for (int dx = -4; dx <= 4; ++dx)
        for (int dy = 0; dy < 4; ++dy) w.setBlock(x + dx, y + dy, z - 6, COBBLESTONE);
    s.setTimeOfDay(13200);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 4.5f);
    p.yaw = 0;
    p.pitch = 0.05f;
    s.mobs().mobs().clear();
    s.mobs().spawn(MobType::Spider, 0, Vec3(x + 0.5f, (float)y, z - 9.0f));
    s.setMode(GameMode::Survival);
    d.idle(4);
    d.shot("w04_spider");
    d.seconds(6, [&](float) {
        heal(d);
        p.pos.x = x + 0.5f, p.pos.z = z + 4.5f;
        if (!s.mobs().mobs().empty()) d.look(s.mobs().mobs()[0].pos + Vec3(0, 0.5f, 0), 0.12f);
    });
}

SHOT(w05_stairs) {
    Director d(out, "w05");
    d.openWorld("Build", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 2.5f);
    p.yaw = 0;
    p.pitch = -0.45f;
    s.inventory().clear();
    d.give(0, stairsId(6, 0, false), 64);
    d.give(1, slabId(8, 0), 64);
    d.give(2, stairsId(0, 0, false), 64);
    d.idle(10);
    d.shot("w05_stairs");
    // A staircase of stone brick stairs up a small wall, slabs beside it.
    for (int k = 0; k < 4; ++k) {
        for (int j = 0; j < k; ++j) w.setBlock(x - 1 + 0, y + j, z - 3 - k, STONE_BRICKS);
        s.selectedSlot() = 0;
        d.use(topOf(x, y - 1 + k, z - 2 - k));
    }
    s.selectedSlot() = 1;
    for (int k = 0; k < 4; ++k) d.use(topOf(x + 2, y - 1, z - 2 - k));
    d.use(Vec3(x + 2.5f, y + 0.5f, z - 2.5f + 0.0f));  // a second slab on the first: a double slab
    s.selectedSlot() = 2;
    for (int k = 0; k < 3; ++k) d.use(topOf(x - 2, y - 1, z - 2 - k));
    d.seconds(1.5f, [&](float k) { p.pitch = -0.45f + 0.2f * k; });
}

SHOT(w06_glass) {
    Director d(out, "w06");
    d.openWorld("Glass", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    for (int c = 0; c < 16; ++c)
        for (int dy = 0; dy < 4; ++dy) w.setBlock(x - 8 + c, y + dy, z - 7, (ItemId)(STAINED_GLASS + c));
    Player& p = s.player();
    p.pos = Vec3(x - 6.5f, (float)y + 1, z - 2.5f);
    d.settings().hideGui = true;
    d.idle(10);
    d.shot("w06_glass");
    d.seconds(5.5f, [&](float k) {
        p.pos = Vec3(x - 6.5f + k * 13, (float)y + 0.6f, z - 2.5f);
        d.lookNow(Vec3(x - 4.0f + k * 9, y + 1.5f, z - 7.0f));
    });
}

SHOT(w07_map) {
    Director d(out, "w07");
    d.openWorld("Map", 4242, GameMode::Survival);
    Session& s = d.s();
    Player& p = s.player();
    s.inventory().clear();
    d.give(0, MAP, 1);
    s.selectedSlot() = 0;
    p.yaw = 0.4f;
    p.pitch = 0;
    d.idle(10);
    d.shot("w07_map");
    d.walk(true);
    d.seconds(7, [&](float k) {
        heal(d);
        p.yaw = 0.4f + std::sin(k * 4) * 0.6f;
    });
    d.walk(false);
}

SHOT(w08_lamps) {
    Director d(out, "w08");
    d.openWorld("Lamps", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    s.setTimeOfDay(18000);
    for (int dx = -5; dx <= 5; ++dx)
        for (int dy = 0; dy < 4; ++dy) w.setBlock(x + dx, y + dy, z - 6, STONE_BRICKS);
    // A lamp on a brick in front of the wall, still off; powerstone lights glowing nearby.
    w.setBlock(x + 4, y, z - 5, STONE_BRICKS);
    w.setBlock(x + 4, y + 1, z - 5, POWERSTONE_LAMP);
    w.setBlock(x - 4, y, z - 4, POWERSTONE_LIGHT);
    w.setBlock(x - 2, y + 3, z - 5, PARTY_LANTERN);
    Player& p = s.player();
    p.pos = Vec3(x + 1.5f, (float)y, z - 1.0f);
    p.yaw = 0;
    p.pitch = -0.1f;
    s.inventory().clear();
    d.give(0, SWITCH, 4);
    d.give(1, POWER_CABLE, 64);
    d.idle(10);
    d.shot("w08_lamps");
    d.seconds(1);
    // A switch on the wall and a cable along the wall to the lamp.
    s.selectedSlot() = 0;
    d.use(Vec3(x + 0.5f, y + 1.5f, z - 5.0f + 0.001f));
    s.selectedSlot() = 1;
    for (int k = 1; k <= 3; ++k) d.use(Vec3(x + k + 0.5f, y + 1.5f, z - 5.0f + 0.001f));
    d.seconds(0.6f);
    d.use(Vec3(x + 0.5f, y + 1.5f, z - 4.9f));  // flip the switch
    d.seconds(3, [&](float k) {
        p.yaw = 0.25f * k;
        p.pitch = -0.1f + 0.1f * k;
    });
}

SHOT(w09_snow) {
    Director d(out, "w09");
    d.openWorld("Snow", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 0, bz = 0;
    if (!findBiome(w, "snowy_taiga", bx, bz)) return;
    travel(d, Vec3(bx + 0.5f, (float)feetY(w, bx, bz) + 1, bz + 0.5f));
    d.idle(40);
    Player& p = s.player();
    p.flying = false;
    s.setTimeOfDay(5000);
    s.setWeather(true);
    d.settings().hideGui = true;
    // Clear the snow cover here first, so it can be seen settling.
    for (int x = bx - 20; x <= bx + 20; ++x)
        for (int z = bz - 20; z <= bz + 20; ++z)
            for (int y = 40; y < 110; ++y)
                if (w.getBlock(x, y, z) == SNOW_LAYER) w.setBlock(x, y, z, AIR);
    d.idle(10);
    d.shot("w09_snow");
    d.stepsPerFrame = 6;
    d.seconds(7, [&](float k) {
        p.yaw = k * 1.2f;
        p.pitch = -0.35f;
    });
    d.stepsPerFrame = 1;
}

SHOT(w10_anniversary) {
    Director d(out, "w10");
    addonWorld(d, "addon:anniversary");
    Session& s = d.s();
    d.idle(60);
    const PlayerState& st = s.stats();
    if (!st.hasShowcase) {
        std::printf("no showcase\n");
        return;
    }
    Player& p = s.player();
    p.flying = true;
    s.setMode(GameMode::Creative);
    d.settings().hideGui = true;
    // Clear the land in front of the open side, so the camera sees the whole showcase.
    World& w = s.currentWorld();
    const int ox = st.showcaseX, oy = st.showcaseY, oz = st.showcaseZ;
    for (int x = ox - 6; x < ox + kShowcaseW + 6; ++x)
        for (int z = oz + kShowcaseD; z < oz + kShowcaseD + 34; ++z) {
            for (int y = oy; y < oy + 40; ++y)
                if (w.getBlock(x, y, z) != AIR) w.setBlock(x, y, z, AIR);
            for (int y = oy - 1; y > 0 && !isSolid(w.getBlock(x, y, z)); --y) w.setBlock(x, y, z, DIRT);
            w.setBlock(x, oy - 1, z, GRASS);
        }
    const Vec3 c(ox + kShowcaseW / 2.0f, (float)oy, oz + 2.0f);
    p.pos = Vec3(c.x - 10, c.y + 12, oz + kShowcaseD + 24.0f);
    d.idle(20);
    d.shot("w10_anniversary");
    d.seconds(8, [&](float k) {
        const float e = k * k * (3 - 2 * k);
        p.flying = true;
        p.pos = Vec3(c.x - 10 + e * 14, c.y + 12 - e * 8, oz + kShowcaseD + 24.0f - e * 16);
        d.lookNow(Vec3(c.x - 2 + e * 4, c.y + 3, c.z + 3));
    });
}

struct Col {
    int r, g, b;
};

void writeSampleMod(const fs::path& dir) {
    fs::create_directories(dir / "textures" / "mod");
    auto tex = [&](const char* name, Col a, Col b) {
        std::vector<uint8_t> px(16 * 16 * 4);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const bool alt = (x / 4 + y / 4) % 2 == 0 || x % 8 == 0 || y % 8 == 0;
                const Col c = alt ? a : b;
                uint8_t* o = &px[(y * 16 + x) * 4];
                o[0] = (uint8_t)c.r, o[1] = (uint8_t)c.g, o[2] = (uint8_t)c.b, o[3] = 255;
            }
        writePng((dir / "textures" / "mod" / name).string(), 16, 16, px);
    };
    tex("crystal.png", {120, 220, 255}, {60, 140, 220});
    tex("moss_tiles.png", {90, 160, 70}, {60, 110, 50});
    tex("sunstone.png", {255, 210, 80}, {230, 150, 40});
    tex("berry.png", {200, 40, 90}, {120, 20, 60});
    std::ofstream(dir / "manifest.json") << R"({"name": "Crystal Mod", "description": "A sample VoxelCraft mod", "version": "1.0"})";
    std::ofstream(dir / "content.json") << R"({"namespace": "crystal",
      "blocks": [{"id": "crystal_block", "name": "Crystal Block", "texture": "crystal.png", "transparent": true},
                 {"id": "moss_tiles", "name": "Moss Tiles", "texture": "moss_tiles.png"},
                 {"id": "sunstone", "name": "Sunstone", "texture": "sunstone.png", "light": true}],
      "items": [{"id": "glow_berry", "name": "Glow Berry", "texture": "berry.png", "food": 4}],
      "mobs": [{"id": "crystal_pig", "name": "Crystal Pig", "base": "pig", "texture": "crystal.png", "health": 14}]})";
}

SHOT(w11_mods) {
    Director d(out, "w11");
    writeSampleMod(d.data / "packs" / "crystal");
    d.openWorld("Mods", 4242, GameMode::Creative);
    // Switch the mod on, as the mod manager does.
    d.game->toolPacks().setEnabled("crystal", true);
    d.game->toolRebuildPacks();
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 3.5f);
    p.yaw = 0;
    p.pitch = -0.3f;
    s.mobs().mobs().clear();
    s.mobs().spawn(MobType::Pig, 0, Vec3(x - 3.5f, (float)y, z - 4.0f));
    s.mobs().mobs().back().variant = 1;
    const ItemId crystal = itemFromKey("crystal:crystal_block"), moss = itemFromKey("crystal:moss_tiles"), sun = itemFromKey("crystal:sunstone");
    s.inventory().clear();
    d.give(0, crystal, 64);
    d.give(1, moss, 64);
    d.give(2, sun, 64);
    d.idle(10);
    d.shot("w11_mods");
    d.tap(Director::kMenuX, Director::kMenuY, 16);  // the creative inventory
    for (int t = 0; t < 8; ++t) d.tap(50, 44 + t * 24, 2);  // find the Mods tab (the last one)
    d.seconds(1.6f);
    d.game->onBack();
    d.frames(6);
    for (int k = 0; k < 3; ++k) {
        s.selectedSlot() = k;
        for (int j = 0; j < 2; ++j) d.use(topOf(x - 1 + j + k * 2, y - 1, z - 1));
    }
    (void)w;
    d.seconds(2, [&](float k) { p.pitch = -0.3f + 0.15f * k; });
}

SHOT(w12_multiplayer) {
    Director d(out, "w12");
    d.idle(10);
    d.shot("w12_multiplayer");
    d.tap(222, 121, 16);  // Play
    d.tap(182, 15, 20);   // Multiplayer
    d.tap(510, 36, 72);   // No internet?: the hotspot guide
    d.game->onBack();
    d.frames(4);
    d.game->onBack();
    d.frames(4);
    // A world opened to friends: the pause menu shows its room code, tap it for the invite.
    d.openWorld("Friends", 4242, GameMode::Creative);
    d.idle(20);
    d.game->onBack();      // pause
    d.frames(10);
    d.tap(300, 170, 16);   // Open to LAN
    d.frames(20);
    d.tap(300, 170, 72);   // the room code: invite players
}

SHOT(w13_recipes) {
    Director d(out, "w13");
    d.openWorld("Recipes", 4242, GameMode::Survival);
    Session& s = d.s();
    s.inventory().clear();
    d.give(0, LOG, 12);
    d.give(1, PLANKS, 32);
    d.give(2, COBBLESTONE, 30);
    d.give(3, STICK, 16);
    d.give(4, (ItemId)177, 4);
    d.give(5, GLASS, 16);
    d.idle(10);
    s.wantsCrafting = true;
    d.idle(4);
    d.shot("w13_recipes");
    d.seconds(0.8f);
    d.tap(287, 60, 20);   // Recipes
    for (auto [bx, by] : {std::pair<float, float>{362, 75}, {382, 75}, {402, 95}})
        d.tap(bx, by, 22);
    d.seconds(1.2f);
}

SHOT(w14_leaves) {
    Director d(out, "w14");
    d.openWorld("Forest", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    int tx, ty, tz;
    if (!findTree(w, p.pos, 3, tx, ty, tz)) return;
    p.flying = true;
    travel(d, Vec3(tx + 6.5f, ty + 3.0f, tz + 6.5f));
    d.settings().hideGui = true;
    d.idle(10);
    d.shot("w14_leaves");
    d.seconds(6, [&](float k) {
        p.flying = true;
        const float a = 0.8f + k * 1.8f;
        p.pos = Vec3(tx + 0.5f + std::cos(a) * 6, ty + 2.5f + k, tz + 0.5f + std::sin(a) * 6);
        d.lookNow(Vec3(tx + 0.5f, ty + 4.0f, tz + 0.5f));
    });
}

}  // namespace

int main(int argc, char** argv) {
    fs::path out = argc > 1 ? argv[1] : "build/film";
    std::string which = argc > 2 ? argv[2] : "all";
    fs::create_directories(out);
    for (auto& [name, fn] : shots())
        if (which == "all" || which == name || (which.back() == '*' && name.rfind(which.substr(0, which.size() - 1), 0) == 0)) {
            auto t0 = std::chrono::steady_clock::now();
            fn(out);
            std::printf("%s: %.0f s\n", name.c_str(), std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
            std::fflush(stdout);
        }
    return 0;
}

// ---------------------------------------------------------------- 1.5.0 Forgotten Powerstone

// The plaza of the forgotten village nearest the origin: the lamp column stands at (x, z);
// y is the feet height on the plaza.
bool findForgottenVillage(Director& d, int& x, int& y, int& z) {
    int fx, fy, fz;
    if (!findBlock(4242, Dimension::Overworld, LOOT_CHEST_FORGOTTEN, fx, fy, fz, 60)) return false;
    travel(d, Vec3(fx + 0.5f, fy + 8.0f, fz + 0.5f));
    World& w = d.s().currentWorld();
    w.ensureChunksAround(floorDiv(fx, 16), floorDiv(fz, 16), 3, 1000);
    for (int dx = -26; dx <= 26; ++dx)
        for (int dz = -26; dz <= 26; ++dz)
            for (int yy = fy - 6; yy < fy + 10; ++yy)
                if (w.getBlock(fx + dx, yy, fz + dz) == POWERSTONE_LAMP_ON && w.getBlock(fx + dx, yy - 1, fz + dz) == CHISELED_POWERSTONE_BRICKS) {
                    x = fx + dx, y = yy - 4, z = fz + dz;
                    std::printf("forgotten village plaza at %d %d %d\n", x, y, z);
                    return true;
                }
    return false;
}

SHOT(f01_wasteland) {
    Director d(out, "f01");
    d.openWorld("Wasteland", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 0, bz = 0;
    if (!findBiome(w, "powerstone_wasteland", bx, bz)) return;
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(bx + 0.5f, (float)feetY(w, bx, bz) + 7, bz + 0.5f));
    s.setTimeOfDay(6500);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(30);
    const Vec3 start = p.pos;
    d.shot("f01_wasteland");
    d.seconds(8, [&](float k) {
        p.flying = true;
        p.yaw = -0.6f + k * 0.9f;
        const Vec3 f(std::sin(p.yaw), 0, -std::cos(p.yaw));
        p.pos = start + f * (k * 30) + Vec3(0, std::sin(k * 3) * 1.0f, 0);
        p.pitch = -0.32f;
    });
}

SHOT(f02_forgotten_village) {
    Director d(out, "f02");
    d.openWorld("Forgotten", 4242, GameMode::Creative);
    int x, y, z;
    if (!findForgottenVillage(d, x, y, z)) return;
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    s.setTimeOfDay(5500);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    p.pos = Vec3(x + 0.5f, y + 16.0f, z + 34.0f);
    d.idle(30);
    d.shot("f02_forgotten_village");
    d.seconds(8, [&](float k) {
        const float e = k * k * (3 - 2 * k);
        p.flying = true;
        p.pos = Vec3(x + 0.5f - 12 + e * 12, y + 16.0f - e * 10, z + 34.0f - e * 24);
        d.lookNow(Vec3(x + 0.5f, y + 2.0f, z + 0.5f));
    });
}

SHOT(f03_village_night) {
    Director d(out, "f03");
    d.openWorld("Forgotten", 4242, GameMode::Creative);
    int x, y, z;
    if (!findForgottenVillage(d, x, y, z)) return;
    Session& s = d.s();
    Player& p = s.player();
    p.flying = false;
    s.setTimeOfDay(18000);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    p.pos = Vec3(x + 0.5f, (float)y + 0.2f, z + 17.5f);
    d.idle(30);
    d.shot("f03_village_night");
    d.seconds(7, [&](float k) {
        p.flying = true;
        p.pos = Vec3(x + 0.5f, y + 0.6f + k * 1.2f, z + 17.5f - k * 11);
        p.yaw = std::sin(k * 2.5f) * 0.35f;
        p.pitch = 0.08f;
    });
}

SHOT(f04_archaeology) {
    Director d(out, "f04");
    d.openWorld("Forgotten", 4242, GameMode::Survival);
    int x, y, z;
    if (!findForgottenVillage(d, x, y, z)) return;
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    s.setTimeOfDay(6000);
    s.mobs().mobs().clear();
    // A patch of debris gravel on the road south of the plaza.
    const int gx = x, gz = z + 6;
    w.setBlock(gx, y - 1, gz, DEBRIS_GRAVEL);
    w.setBlock(gx, y, gz, AIR);
    w.setBlock(gx, y + 1, gz, AIR);
    p.pos = Vec3(gx + 0.5f, (float)y, gz + 2.3f);
    s.inventory().clear();
    d.give(0, BRUSHER);
    d.give(1, OLD_DOCUMENT + 0);
    s.selectedSlot() = 0;
    d.lookNow(topOf(gx, y - 1, gz));
    d.idle(20);
    d.shot("f04_archaeology");
    d.seconds(0.6f, [&](float) { heal(d); d.lookNow(topOf(gx, y - 1, gz)); });
    d.down(1, Director::kCenterX, Director::kCenterY);
    d.seconds(2.4f, [&](float) { heal(d); d.lookNow(topOf(gx, y - 1, gz)); });
    d.up(1, Director::kCenterX, Director::kCenterY);
    d.seconds(1.6f, [&](float k) { heal(d); p.pitch = -0.7f + k * 0.1f; });
    // The page of the chronicle.
    s.selectedSlot() = 1;
    d.seconds(0.5f);
    s.readDocument(0);
    d.seconds(4.5f);
    d.game->onBack();
    d.seconds(0.5f);
}

SHOT(f05_husking) {
    Director d(out, "f05");
    d.openWorld("Wasteland", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 0, bz = 0;
    if (!findBiome(w, "powerstone_wasteland", bx, bz)) return;
    travel(d, Vec3(bx + 0.5f, (float)feetY(w, bx, bz), bz + 0.5f));
    const int y = feetY(w, bx, bz);
    flatten(w, bx - 8, bz - 20, bx + 8, bz + 4, y, WASTELAND_GRASS);
    for (int i = 0; i < 10; ++i) w.setBlock(bx - 7 + (i * 5) % 15, y, bz - 18 + i * 2, i % 3 ? DRY_GRASS : GRASSER_FLOWER);
    Player& p = s.player();
    p.pos = Vec3(bx + 0.5f, (float)y, bz + 2.5f);
    p.yaw = 0;
    p.pitch = 0.02f;
    s.setTimeOfDay(7000);
    s.mobs().mobs().clear();
    s.inventory().clear();
    d.give(0, STEEL_SWORD);
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(3, i), 1};
    s.mobs().spawn(MobType::Husking, 0, Vec3(bx - 2.5f, (float)y, bz - 10.5f));
    s.mobs().spawn(MobType::Husking, 0, Vec3(bx + 3.5f, (float)y, bz - 13.5f));
    d.idle(4);
    d.shot("f05_husking");
    d.seconds(8, [&](float k) {
        heal(d);
        p.yaw = std::sin(k * 3) * 0.12f;
        p.pitch = 0.02f;
    });
}

SHOT(f06_drowned_ruins) {
    Director d(out, "f06");
    int fx, fy, fz;
    if (!findBlock(4242, Dimension::Overworld, LOOT_CHEST_DROWNED, fx, fy, fz, 60)) return;
    d.openWorld("Drowned", 4242, GameMode::Creative);
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(fx + 0.5f, fy + 5.0f, fz + 9.5f));
    s.setTimeOfDay(6000);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(20);
    d.shot("f06_drowned_ruins");
    d.seconds(7, [&](float k) {
        p.flying = true;
        p.pos = Vec3(fx + 0.5f + std::sin(k * 1.6f) * 6, fy + 5.0f - k * 2, fz + 9.5f - k * 4);
        d.lookNow(Vec3(fx + 4.0f, (float)fy, fz + 4.0f));
    });
}

SHOT(f07_broken_portal) {
    Director d(out, "f07");
    int fx, fy, fz;
    if (!findBlock(4242, Dimension::Overworld, LOOT_CHEST_PORTAL, fx, fy, fz, 60)) return;
    d.openWorld("Portal", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    travel(d, Vec3(fx + 0.5f, (float)fy + 1, fz + 6.5f));
    s.setTimeOfDay(6000);
    s.mobs().mobs().clear();
    s.inventory().clear();
    p.pos = Vec3(fx + 0.5f, (float)feetY(w, fx, fz + 6), fz + 7.5f);
    d.lookNow(Vec3(fx + 3.0f, fy + 2.0f, fz - 3.0f));
    d.idle(20);
    d.shot("f07_broken_portal");
    d.seconds(2.5f, [&](float) { heal(d); d.look(Vec3(fx + 3.0f, fy + 3.0f, fz - 3.0f), 0.04f); });
    // Walk to the chest and open it.
    p.pos = Vec3(fx + 0.5f, (float)fy, fz + 2.5f);
    d.use(centre(fx, fy, fz));
    d.seconds(3.0f);
    // Take the compass and look at its needle.
    for (int i = 0; i < kChestSize; ++i) {
        ChestContents& c = s.chestFor(s.openChest);
        if (c[i].id == FORTRESS_COMPASS) {
            s.inventory().slots[0] = c[i];
            c[i] = {};
        }
    }
    d.game->onBack();
    s.selectedSlot() = 0;
    d.seconds(3.5f, [&](float k) { heal(d); p.yaw += 0.02f; p.pitch = 0.05f * k; });
}

SHOT(f08_cornerstone) {
    Director d(out, "f08");
    d.openWorld("Cornerstone", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z), y = feetY(w, x, z);
    flatten(w, x - 5, z - 7, x + 5, z + 3, y);
    obsidianFrame(w, x - 1, y, z - 4);
    tryLightPortal(w, x, y, z - 4);
    p.pos = Vec3(x + 0.5f, (float)y, z + 1.0f);
    p.yaw = 0;
    s.inventory().clear();
    d.give(0, FORTRESS_COMPASS);
    s.selectedSlot() = 0;
    s.mobs().mobs().clear();
    d.idle(30);
    d.shot("f08_cornerstone");
    d.seconds(1.2f, [&](float) { d.look(centre(x, y + 1, z - 4), 0.2f); });
    d.walk(true);
    d.seconds(1.0f, [&](float) { heal(d); d.look(centre(x, y + 1, z - 5), 0.2f); });
    d.walk(false);
    // Stand in the portal until it takes the player across.
    p.pos = Vec3(x + 0.5f, (float)y, z - 3.5f);
    for (int i = 0; i < 300 && s.currentWorld().dimension() != Dimension::Nether; ++i)
        d.frame([&] {
            heal(d);
            if (s.currentWorld().dimension() != Dimension::Nether) s.player().pos = Vec3(x + 0.5f, (float)y, z - 3.5f);
        });
    d.idle(60);  // the loading screen
    // Step out of the arrival portal and look over the Cornerstone Dimension.
    {
        Player& q = s.player();
        const Vec3 f(std::sin(q.yaw), 0, -std::cos(q.yaw));
        for (int k = 3; k < 8; ++k) {
            const Vec3 at = q.pos + f * (float)k;
            World& nw = s.currentWorld();
            if (!isSolid(nw.getBlock((int)std::floor(at.x), (int)std::floor(at.y), (int)std::floor(at.z)))) q.pos = at;
        }
        d.idle(10);
    }
    // The compass needle turns towards the fortress.
    d.seconds(4.5f, [&](float k) { heal(d); s.player().yaw += 0.012f * std::sin(k * 4); });
}

SHOT(f09_fortress) {
    Director d(out, "f09");
    d.openWorld("Fortress", 4242, GameMode::Creative);
    goNether(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    int fx, fy, fz;
    w.nearestStructure((int)s.player().pos.x, (int)s.player().pos.z, fx, fy, fz);
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(fx + 0.5f, fy + 6.0f, fz + 7.5f));
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(30);
    // Inside the great hall: a slow turn past its carved walls, fenced windows and the frame.
    d.shot("f09_fortress");
    d.seconds(8, [&](float k) {
        p.flying = true;
        p.pos = Vec3(fx + 0.5f + std::sin(k * 2.2f) * 4, fy + 5.0f, fz + 7.5f - k * 2);
        p.yaw = kPi * 0.85f + k * kPi * 1.3f;
        p.pitch = -0.12f;
    });
}

SHOT(f10_blocks) {
    Director d(out, "f10");
    d.openWorld("Blocks", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    flatten(w, x - 8, z - 10, x + 8, z + 3, y);
    // Half a house of powerstone: the player finishes it.
    for (int dx = -3; dx <= 3; ++dx)
        for (int dy = 0; dy < 3; ++dy) w.setBlock(x + dx, y + dy, z - 6, dy == 1 && (dx == -1 || dx == 1) ? POWERSTONE_GLASS : (dx + dy) % 3 ? POWERSTONE_BRICKS : CRACKED_POWERSTONE_BRICKS);
    for (int dz = -5; dz <= -3; ++dz) w.setBlock(x - 3, y, z + dz, POWERSTONE_BRICKS), w.setBlock(x + 3, y, z + dz, POWERSTONE_BRICKS);
    w.setBlock(x, y + 1, z - 5, POWERSTONE_LAMP_ON);
    w.setBlock(x, y, z - 5, POWERSTONE_BLOCK);
    for (int dx = -6; dx <= -4; ++dx) w.setBlock(x + dx, y, z - 2, POWERSTONE_FENCE);
    for (int dx = 4; dx <= 6; ++dx) w.setBlock(x + dx, y, z - 2, NETHER_BRICK_FENCE);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 2.5f);
    p.yaw = 0;
    p.pitch = -0.25f;
    s.inventory().clear();
    d.give(0, CHISELED_POWERSTONE_BRICKS, 64);
    d.give(1, stairsId(12, 0, false), 64);
    d.give(2, slabId(12, 0), 64);
    d.give(3, POWERSTONE_GLASS, 64);
    d.give(4, SPRUCE_FENCE, 64);
    d.give(5, SKELETON_HEAD, 8);
    s.setTimeOfDay(6000);
    s.mobs().mobs().clear();
    d.idle(20);
    d.shot("f10_blocks");
    d.seconds(0.6f);
    s.selectedSlot() = 0;
    for (int dx = -3; dx <= 3; dx += 2) d.use(topOf(x + dx, y + 2, z - 6));
    s.selectedSlot() = 1;
    for (int dx = -2; dx <= 2; ++dx) d.use(topOf(x + dx, y - 1, z - 2));
    s.selectedSlot() = 2;
    d.use(topOf(x - 2, y + 3, z - 6));
    d.use(topOf(x + 2, y + 3, z - 6));
    s.selectedSlot() = 4;
    d.use(topOf(x - 4, y - 1, z));
    d.use(topOf(x + 4, y - 1, z));
    s.selectedSlot() = 5;
    d.use(topOf(x + 1, y - 1, z));
    d.seconds(1.2f);
    // The new blocks and fences in the creative inventory.
    d.tap(Director::kMenuX, Director::kMenuY, 40);
    d.seconds(0.4f);
    d.game->onBack();
    d.seconds(0.4f);
}

SHOT(f11_nature) {
    Director d(out, "f11");
    d.openWorld("Nature", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 0, bz = 0;
    if (!findBiome(w, "forest", bx, bz)) return;
    travel(d, Vec3(bx + 0.5f, (float)feetY(w, bx, bz) + 1, bz + 0.5f));
    Player& p = s.player();
    int tx, ty, tz;
    if (!findTree(w, p.pos, 3, tx, ty, tz)) return;
    // Ferns and Grasser flowers around the trunk.
    for (int i = 0; i < 8; ++i) {
        const int gx = tx + 1 + i % 4, gz = tz - 2 + i / 2;
        if (w.getBlock(gx, ty, gz) == AIR && w.getBlock(gx, ty - 1, gz) == GRASS) w.setBlock(gx, ty, gz, i % 3 ? FERN : GRASSER_FLOWER);
    }
    p.flying = true;
    s.setTimeOfDay(5000);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(20);
    d.shot("f11_nature");
    d.seconds(6, [&](float k) {
        p.flying = true;
        const float a = -0.6f + k * 1.6f;
        p.pos = Vec3(tx + 0.5f + std::sin(a) * 4.5f, ty + 1.2f, tz + 0.5f + std::cos(a) * 4.5f);
        d.lookNow(Vec3(tx + 0.5f, ty + 1.8f, tz + 0.5f));
    });
}

// 1.5.1 check: the tools in hand, still and swinging.
SHOT(t_tools) {
    Director d(out, "t_tools");
    d.openWorld("Tools", 4242, GameMode::Survival);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    Player& p = s.player();
    p.pos = Vec3(x + 0.5f, (float)y, z + 0.5f);
    p.yaw = 0;
    p.pitch = -0.35f;
    s.inventory().clear();
    d.give(0, IRON_PICKAXE);
    d.give(1, IRON_AXE);
    d.give(2, STEEL_SHOVEL);
    d.give(3, HOE);
    s.selectedSlot() = 0;
    for (int k = 1; k < 6; ++k) w.setBlock(x, y, z - k, STONE), w.setBlock(x, y + 1, z - k, STONE);
    d.idle(30);
    d.shot("t_tools");
    for (int slot = 0; slot < 4; ++slot) {
        s.selectedSlot() = slot;
        d.seconds(0.8f, [&](float) { heal(d); });
        d.down(1, Director::kCenterX, Director::kCenterY);
        d.seconds(0.6f, [&](float) { heal(d); });
        d.up(1, Director::kCenterX, Director::kCenterY);
    }
    // From behind and from the front: the tools in the hand.
    for (int cam = 1; cam <= 2; ++cam) {
        d.settings().cameraMode = cam;
        for (int slot = 0; slot < 2; ++slot) {
            s.selectedSlot() = slot;
            d.seconds(0.7f, [&](float) { heal(d); });
            d.down(1, Director::kCenterX, Director::kCenterY);
            d.seconds(0.5f, [&](float) { heal(d); });
            d.up(1, Director::kCenterX, Director::kCenterY);
        }
    }
}

// ---------------------------------------------------------------- 1.5.1 the fixed Forgotten Powerstone

// Wasteland ground in the Nether near the player: a spot whose floor is wasteland grass.
bool findNetherWasteland(Director& d, int& bx, int& by, int& bz) {
    Session& s = d.s();
    World& w = s.currentWorld();
    const int px = (int)s.player().pos.x, pz = (int)s.player().pos.z;
    for (int r = 0; r < 1500; r += 24)
        for (int a = 0; a < 24; ++a) {
            const int x = px + (int)(std::cos(a * kPi / 12) * r), z = pz + (int)(std::sin(a * kPi / 12) * r);
            if (!w.wastelandAt(x, z) || !w.wastelandAt(x + 20, z) || !w.wastelandAt(x, z - 30)) continue;
            travel(d, Vec3(x + 0.5f, 70, z + 0.5f));
            w.ensureChunksAround(floorDiv(x, 16), floorDiv(z, 16), 3, 1000);
            for (int y = 30; y < 100; ++y)
                if (w.getBlock(x, y - 1, z) == WASTELAND_GRASS && w.getBlock(x, y, z) == AIR && w.getBlock(x, y + 1, z) == AIR) {
                    bx = x, by = y, bz = z;
                    std::printf("nether wasteland at %d %d %d\n", x, y, z);
                    return true;
                }
        }
    return false;
}

bool findNetherVillage(Director& d, int& x, int& y, int& z) {
    int fx, fy, fz;
    if (!findBlock(4242, Dimension::Nether, LOOT_CHEST_FORGOTTEN, fx, fy, fz, 40)) return false;
    goNether(d);
    travel(d, Vec3(fx + 0.5f, fy + 8.0f, fz + 0.5f));
    World& w = d.s().currentWorld();
    w.ensureChunksAround(floorDiv(fx, 16), floorDiv(fz, 16), 3, 1000);
    for (int dx = -26; dx <= 26; ++dx)
        for (int dz = -26; dz <= 26; ++dz)
            for (int yy = fy - 6; yy < fy + 10; ++yy)
                if (w.getBlock(fx + dx, yy, fz + dz) == POWERSTONE_LAMP_ON && w.getBlock(fx + dx, yy - 1, fz + dz) == CHISELED_POWERSTONE_BRICKS) {
                    x = fx + dx, y = yy - 4, z = fz + dz;
                    std::printf("nether village plaza at %d %d %d\n", x, y, z);
                    return true;
                }
    x = fx, y = fy, z = fz;
    return true;
}

SHOT(g01_wasteland) {
    Director d(out, "g01");
    d.openWorld("Wasteland", 4242, GameMode::Creative);
    goNether(d);
    int bx, by, bz;
    if (!findNetherWasteland(d, bx, by, bz)) return;
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    p.pos = Vec3(bx + 0.5f, by + 6.0f, bz + 0.5f);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(30);
    const Vec3 start = p.pos;
    d.shot("g01_wasteland");
    d.seconds(8, [&](float k) {
        p.flying = true;
        p.yaw = -0.4f + k * 0.8f;
        const Vec3 f(std::sin(p.yaw), 0, -std::cos(p.yaw));
        p.pos = start + f * (k * 24) + Vec3(0, std::sin(k * 3) * 0.8f, 0);
        p.pitch = -0.25f;
    });
}

SHOT(g02_village) {
    Director d(out, "g02");
    d.openWorld("Forgotten", 4242, GameMode::Creative);
    int x, y, z;
    if (!findNetherVillage(d, x, y, z)) return;
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    p.pos = Vec3(x + 0.5f, y + 12.0f, z + 26.0f);
    d.idle(30);
    d.shot("g02_village");
    d.seconds(8, [&](float k) {
        const float e = k * k * (3 - 2 * k);
        p.flying = true;
        p.pos = Vec3(x + 0.5f - 10 + e * 10, y + 12.0f - e * 8, z + 26.0f - e * 18);
        d.lookNow(Vec3(x + 0.5f, y + 2.0f, z + 0.5f));
    });
}

SHOT(g03_archaeology) {
    Director d(out, "g03");
    d.openWorld("Forgotten", 4242, GameMode::Survival);
    int x, y, z;
    if (!findNetherVillage(d, x, y, z)) return;
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    s.mobs().mobs().clear();
    const int gx = x, gz = z + 6;
    w.setBlock(gx, y - 1, gz, DEBRIS_GRAVEL);
    w.setBlock(gx, y, gz, AIR);
    w.setBlock(gx, y + 1, gz, AIR);
    p.pos = Vec3(gx + 0.5f, (float)y, gz + 2.3f);
    s.inventory().clear();
    d.give(0, BRUSHER);
    d.give(1, OLD_DOCUMENT + 0);
    s.selectedSlot() = 0;
    d.lookNow(topOf(gx, y - 1, gz));
    d.idle(20);
    d.shot("g03_archaeology");
    d.seconds(0.6f, [&](float) { heal(d); d.lookNow(topOf(gx, y - 1, gz)); });
    d.down(1, Director::kCenterX, Director::kCenterY);
    d.seconds(2.4f, [&](float) { heal(d); d.lookNow(topOf(gx, y - 1, gz)); });
    d.up(1, Director::kCenterX, Director::kCenterY);
    d.seconds(1.2f, [&](float k) { heal(d); p.pitch = -0.7f + k * 0.1f; });
    s.selectedSlot() = 1;
    d.seconds(0.5f);
    s.readDocument(0);
    d.seconds(3.5f);
    d.game->onBack();
    d.seconds(0.4f);
}

SHOT(g04_husking) {
    Director d(out, "g04");
    d.openWorld("Wasteland", 4242, GameMode::Survival);
    goNether(d);
    int bx, y, bz;
    if (!findNetherWasteland(d, bx, y, bz)) return;
    Session& s = d.s();
    World& w = s.currentWorld();
    flatten(w, bx - 8, bz - 20, bx + 8, bz + 4, y, WASTELAND_GRASS);
    for (int dx = -8; dx <= 8; ++dx)
        for (int dz = -20; dz <= 4; ++dz)
            for (int dy = 0; dy < 5; ++dy) w.setBlock(bx + dx, y + dy, bz + dz, AIR);
    for (int i = 0; i < 10; ++i) w.setBlock(bx - 7 + (i * 5) % 15, y, bz - 18 + i * 2, i % 3 ? DRY_GRASS : GRASSER_FLOWER);
    Player& p = s.player();
    p.flying = false;
    p.pos = Vec3(bx + 0.5f, (float)y, bz + 2.5f);
    p.yaw = 0;
    p.pitch = 0.02f;
    s.mobs().mobs().clear();
    s.inventory().clear();
    d.give(0, STEEL_SWORD);
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(3, i), 1};
    s.mobs().spawn(MobType::Husking, 1, Vec3(bx - 2.5f, (float)y, bz - 10.5f));
    s.mobs().spawn(MobType::Husking, 1, Vec3(bx + 3.5f, (float)y, bz - 13.5f));
    d.idle(4);
    d.shot("g04_husking");
    d.seconds(7, [&](float k) {
        heal(d);
        p.yaw = std::sin(k * 3) * 0.12f;
        p.pitch = 0.02f;
    });
}

SHOT(g05_crimson_desert) {
    Director d(out, "g05");
    d.openWorld("Crimson", 4242, GameMode::Creative);
    goNether(d);
    goVoid(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 1 << 30, bz = 0;
    const int px = (int)s.player().pos.x, pz = (int)s.player().pos.z;
    for (int r = 0; r < 3000 && bx == (1 << 30); r += 40)
        for (int a = 0; a < 16 && bx == (1 << 30); ++a) {
            const int x = px + (int)(std::cos(a * 0.3927f) * r), z = pz + (int)(std::sin(a * 0.3927f) * r);
            if (w.voidBiomeAt(x, z) == World::VoidBiome::Crimson && w.voidBiomeAt(x + 40, z) == World::VoidBiome::Crimson &&
                w.voidBiomeAt(x, z - 40) == World::VoidBiome::Crimson)
                bx = x, bz = z;
        }
    if (bx == (1 << 30)) return;
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(bx + 0.5f, (float)w.voidHeight(bx, bz) + 9, bz + 0.5f));
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(30);
    const Vec3 start = p.pos;
    d.shot("g05_crimson_desert");
    d.seconds(9, [&](float k) {
        p.flying = true;
        p.yaw = 0.3f + k * 0.7f;
        const Vec3 f(std::sin(p.yaw), 0, -std::cos(p.yaw));
        p.pos = start + f * (k * 36);
        p.pos.y = std::max(start.y, (float)w.voidHeight((int)p.pos.x, (int)p.pos.z) + 7);
        p.pitch = -0.3f;
    });
}

SHOT(g06_eyes) {
    Director d(out, "g06");
    d.openWorld("Eyes", 4242, GameMode::Creative);
    goNether(d);
    goVoid(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    Player& p = s.player();
    const int x = (int)std::floor(p.pos.x), z = (int)std::floor(p.pos.z) - 4, y = (int)std::floor(p.pos.y);
    // A little altar: Void Brick Eyes in a void brick wall, an Eye Log column in the middle.
    for (int dx = -3; dx <= 3; ++dx)
        for (int dy = -1; dy < 4; ++dy) w.setBlock(x + dx, y + dy, z, (dx == -2 || dx == 2) && dy == 1 ? VOID_BRICK_EYE : VOID_STONE_BRICKS);
    for (int dy = 0; dy < 3; ++dy) w.setBlock(x, y + dy, z + 1, EYE_LOG);
    for (int dx = -3; dx <= 3; ++dx)
        for (int dz = 1; dz <= 5; ++dz)
            for (int dy = 0; dy < 4; ++dy)
                if (!(dx == 0 && dz == 1 && dy < 3)) w.setBlock(x + dx, y + dy, z + dz, AIR);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    p.flying = true;
    p.pos = Vec3(x + 0.5f, (float)y + 0.2f, z + 4.5f);
    d.idle(20);
    d.shot("g06_eyes");
    d.seconds(7, [&](float k) {
        p.flying = true;
        p.pos = Vec3(x + 0.5f + std::sin(k * 2) * 1.2f, (float)y + 0.2f, z + 4.5f - k * 1.2f);
        d.lookNow(Vec3(x + 0.5f, y + 1.5f, z + 1.0f));
    });
}

SHOT(g07_trees) {
    Director d(out, "g07");
    d.openWorld("Trees", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 0, bz = 0;
    if (!findBiome(w, "forest", bx, bz)) return;
    travel(d, Vec3(bx + 0.5f, (float)feetY(w, bx, bz) + 1, bz + 0.5f));
    Player& p = s.player();
    int tx, ty, tz;
    if (!findTree(w, p.pos, 3, tx, ty, tz)) return;
    s.setTimeOfDay(5000);
    p.flying = true;
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(20);
    d.shot("g07_trees");
    d.seconds(6, [&](float k) {
        p.flying = true;
        const float a = -0.6f + k * 1.8f;
        p.pos = Vec3(tx + 0.5f + std::sin(a) * 4.5f, ty + 1.5f + k * 2, tz + 0.5f + std::cos(a) * 4.5f);
        d.lookNow(Vec3(tx + 0.5f, ty + 2.5f, tz + 0.5f));
    });
}

SHOT(g08_ores) {
    Director d(out, "g08");
    d.expOres = true;
    d.openWorld("Ores", 4242, GameMode::Creative);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    // A rock wall with every redesigned ore in it.
    const ItemId ores[] = {COAL_ORE, IRON_ORE, BRONZE_ORE, STEEL_ORE, RUBY_ORE, POWERSTONE_ORE};
    for (int dx = -4; dx <= 4; ++dx)
        for (int dy = 0; dy < 4; ++dy) {
            const int i = (dx + 4 + dy * 3) % 9;
            w.setBlock(x + dx, y + dy, z - 4, i < 6 ? ores[i] : STONE);
        }
    Player& p = s.player();
    p.flying = true;
    p.pos = Vec3(x + 0.5f, (float)y + 0.8f, z + 1.5f);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    s.setTimeOfDay(6000);
    d.idle(20);
    d.shot("g08_ores");
    d.seconds(6, [&](float k) {
        p.flying = true;
        p.pos = Vec3(x + 0.5f - 3 + k * 6, (float)y + 0.8f, z + 1.5f - k);
        d.lookNow(Vec3(x + 0.5f, y + 1.8f, z - 3.5f));
    });
}

// ---------------------------------------------------------------- 1.5.5 Shaders

// Variant: 0 Classic, 1 Enhanced (Vibrant), 2 the "Golden Hour" pack, 3 the "Frost" pack.
void shaderVariant(Director& d, int variant) {
    d.settings().graphics = variant == 0 ? GraphicsMode::Classic : GraphicsMode::Enhanced;
    if (variant < 2) return;
    const std::string id = variant == 2 ? "golden_hour" : "frost";
    const fs::path dir = d.data / "packs" / id;
    fs::create_directories(dir / "shaders");
    std::ofstream((dir / "manifest.json").string()) << "{\"name\": \"" << id << "\", \"description\": \"\", \"version\": \"1.0.0\"}";
    std::ofstream((dir / "shaders/shader.json").string())
        << (variant == 2 ? R"({"name": "Golden Hour", "saturation": 1.45, "contrast": 1.12, "warmth": 0.34, "vignette": 0.45,
                               "glow": 0.6, "shade": [0.9, 0.8, 0.95], "fog_tint": [1.15, 0.92, 0.75], "fog_strength": 1.2})"
                         : R"({"name": "Frost", "saturation": 0.85, "contrast": 1.1, "warmth": -0.22, "vignette": 0.35,
                               "glow": 0.2, "shade": [0.75, 0.88, 1.2], "fog_tint": [0.85, 0.95, 1.15], "fog_strength": 1.4})");
    d.game->toolPacks().setEnabled(id, true);
    d.game->toolRebuildPacks();
}

const char* kVariantName[] = {"classic", "enhanced", "golden", "frost"};

void shaderForest(const fs::path& out, int variant) {
    const std::string name = std::string("h01_forest_") + kVariantName[variant];
    Director d(out, name);
    d.openWorld("Shaders", 4242, GameMode::Creative);
    shaderVariant(d, variant);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 0, bz = 0;
    if (!findBiome(w, "forest", bx, bz)) return;
    travel(d, Vec3(bx + 0.5f, (float)feetY(w, bx, bz) + 1, bz + 0.5f));
    Player& p = s.player();
    p.flying = true;
    const int y = feetY(w, bx, bz);
    s.setTimeOfDay(variant == 3 ? 3000 : 9200);  // afternoon: long shadows
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    p.pos = Vec3(bx + 0.5f, y + 9.0f, bz + 0.5f);
    d.idle(30);
    d.shot(name);
    d.seconds(6, [&](float k) {
        p.flying = true;
        p.yaw = 0.9f + k * 0.5f;
        p.pitch = -0.18f;
        p.pos = Vec3(bx + 0.5f + k * 10, y + 9.0f - k * 2, bz + 0.5f - k * 6);
    });
}
SHOT(h01_forest_classic) { shaderForest(out, 0); }
SHOT(h01_forest_enhanced) { shaderForest(out, 1); }
SHOT(h01_forest_golden) { shaderForest(out, 2); }
SHOT(h01_forest_frost) { shaderForest(out, 3); }

void shaderNight(const fs::path& out, int variant) {
    const std::string name = std::string("h02_night_") + kVariantName[variant];
    Director d(out, name);
    d.openWorld("Shaders", 4242, GameMode::Creative);
    shaderVariant(d, variant);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    // A little camp: lamps, a powerstone path and a torch-lit stone hut.
    for (int dz = -8; dz <= 0; ++dz) w.setBlock(x, y - 1, z + dz, POWERSTONE_BRICKS);
    for (int dz = -8; dz <= 0; dz += 4) w.setBlock(x - 2, y, z + dz, POWERSTONE_LAMP_ON), w.setBlock(x + 2, y, z + dz, POWERSTONE_LAMP_ON);
    for (int dx = -2; dx <= 2; ++dx)
        for (int dy = 0; dy < 4; ++dy) w.setBlock(x + dx, y + dy, z - 11, dy == 1 && dx == 0 ? AIR : COBBLESTONE);
    w.setBlock(x, y, z - 11, AIR);
    w.setBlock(x, y + 3, z - 10, POWERSTONE_LAMP_ON);
    Player& p = s.player();
    p.flying = true;
    s.setTimeOfDay(18500);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    p.pos = Vec3(x + 0.5f, y + 1.6f, z + 3.5f);
    d.idle(30);
    d.shot(name);
    d.seconds(6, [&](float k) {
        p.flying = true;
        p.pos = Vec3(x + 0.5f + std::sin(k * 2) * 1.5f, y + 1.6f + k * 1.5f, z + 3.5f - k * 7);
        d.lookNow(Vec3(x + 0.5f, y + 1.0f, z - 9.0f));
    });
}
SHOT(h02_night_classic) { shaderNight(out, 0); }
SHOT(h02_night_enhanced) { shaderNight(out, 1); }
SHOT(h02_night_golden) { shaderNight(out, 2); }

void shaderVillage(const fs::path& out, int variant) {
    const std::string name = std::string("h03_village_") + kVariantName[variant];
    Director d(out, name);
    d.openWorld("Forgotten", 4242, GameMode::Creative);
    shaderVariant(d, variant);
    int x, y, z;
    if (!findNetherVillage(d, x, y, z)) return;
    Session& s = d.s();
    Player& p = s.player();
    p.flying = true;
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    p.pos = Vec3(x + 0.5f, y + 9.0f, z + 18.0f);
    d.idle(30);
    d.shot(name);
    d.seconds(6, [&](float k) {
        p.flying = true;
        p.pos = Vec3(x + 0.5f - 6 + k * 12, y + 9.0f - k * 4, z + 18.0f - k * 6);
        d.lookNow(Vec3(x + 0.5f, y + 2.0f, z + 0.5f));
    });
}
SHOT(h03_village_classic) { shaderVillage(out, 0); }
SHOT(h03_village_enhanced) { shaderVillage(out, 1); }

void shaderDesert(const fs::path& out, int variant) {
    const std::string name = std::string("h04_crimson_") + kVariantName[variant];
    Director d(out, name);
    d.openWorld("Crimson", 4242, GameMode::Creative);
    shaderVariant(d, variant);
    goNether(d);
    goVoid(d);
    Session& s = d.s();
    World& w = s.currentWorld();
    int bx = 1 << 30, bz = 0;
    const int px = (int)s.player().pos.x, pz = (int)s.player().pos.z;
    for (int r = 0; r < 3000 && bx == (1 << 30); r += 40)
        for (int a = 0; a < 16 && bx == (1 << 30); ++a) {
            const int x = px + (int)(std::cos(a * 0.3927f) * r), z = pz + (int)(std::sin(a * 0.3927f) * r);
            if (w.voidBiomeAt(x, z) == World::VoidBiome::Crimson && w.voidBiomeAt(x + 40, z) == World::VoidBiome::Crimson &&
                w.voidBiomeAt(x, z - 40) == World::VoidBiome::Crimson)
                bx = x, bz = z;
        }
    if (bx == (1 << 30)) return;
    Player& p = s.player();
    p.flying = true;
    travel(d, Vec3(bx + 0.5f, (float)w.voidHeight(bx, bz) + 9, bz + 0.5f));
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    d.idle(30);
    const Vec3 start = p.pos;
    d.shot(name);
    d.seconds(6, [&](float k) {
        p.flying = true;
        p.yaw = 0.3f + k * 0.5f;
        const Vec3 f(std::sin(p.yaw), 0, -std::cos(p.yaw));
        p.pos = start + f * (k * 24);
        p.pos.y = std::max(start.y, (float)w.voidHeight((int)p.pos.x, (int)p.pos.z) + 7);
        p.pitch = -0.3f;
    });
}
SHOT(h04_crimson_classic) { shaderDesert(out, 0); }
SHOT(h04_crimson_enhanced) { shaderDesert(out, 1); }

// Shadows that follow the sun (a time-lapse) over a pond, a stone tower and a young tree.
void shaderTimelapse(const fs::path& out, int variant) {
    const std::string name = std::string("h05_timelapse_") + kVariantName[variant];
    Director d(out, name);
    d.openWorld("Shaders", 4242, GameMode::Creative);
    shaderVariant(d, variant);
    Session& s = d.s();
    World& w = s.currentWorld();
    int x, y, z;
    meadow(d, x, y, z);
    flatten(w, x - 10, z - 16, x + 10, z + 2, y);
    for (int dx = -3; dx <= 3; ++dx)
        for (int dz = -12; dz <= -8; ++dz) w.setBlock(x + dx, y - 1, z + dz, WATER), w.setBlock(x + dx, y - 2, z + dz, SAND);
    for (int dy = 0; dy < 8; ++dy)
        for (int dx = 5; dx <= 6; ++dx)
            for (int dz = -7; dz <= -6; ++dz) w.setBlock(x + dx, y + dy, z + dz, dy == 7 && (dx + dz) % 2 ? AIR : STONE_BRICKS);
    for (int dy = 0; dy < 4; ++dy) w.setBlock(x - 6, y + dy, z - 6, LOG);
    for (int dx = -2; dx <= 2; ++dx)
        for (int dz = -2; dz <= 2; ++dz)
            for (int dy = 3; dy < 6; ++dy)
                if (std::abs(dx) + std::abs(dz) + (dy == 5) * 2 < 4 && !(dx == 0 && dz == 0 && dy < 4)) w.setBlock(x - 6 + dx, y + dy, z - 6 + dz, LEAVES);
    Player& p = s.player();
    p.flying = true;
    p.pos = Vec3(x + 0.5f, y + 7.0f, z + 6.5f);
    d.settings().hideGui = true;
    s.mobs().mobs().clear();
    s.setTimeOfDay(1500);
    d.idle(30);
    d.shot(name);
    d.seconds(7, [&](float k) {
        p.flying = true;
        s.setTimeOfDay(1500 + k * 9000);  // from morning to late afternoon
        d.lookNow(Vec3(x + 0.5f, y + 1.0f, z - 7.0f));
    });
}
SHOT(h05_timelapse_classic) { shaderTimelapse(out, 0); }
SHOT(h05_timelapse_enhanced) { shaderTimelapse(out, 1); }

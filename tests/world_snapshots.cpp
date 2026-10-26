// Software renderer for the 3D world, used to look at real game geometry on a
// machine without a GPU. It draws exactly the triangles the game uploads
// (chunk meshes, particles, the player model) with the same alpha test, light
// and fog rules as the GLSL shader in Renderer.cpp. Output: build/snapshots/world_*.png
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "engine/Blocks.h"
#include "engine/Mesher.h"
#include "engine/Particles.h"
#include "engine/Player.h"
#include "engine/PlayerModel.h"
#include "engine/Skin.h"
#include "engine/TexturePack.h"
#include "engine/World.h"
#include "engine/WorldStructures.h"
#include "engine/Structure.h"
#include "engine/FirstPerson.h"
#include "engine/Boats.h"
#include "engine/Items.h"
#include "engine/Mobs.h"
#include "engine/Sky.h"
#include "engine/GenConfig.h"

using namespace vc;

void writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);

#include "soft_render.h"

namespace {
// Finds a forest column with a tree near (x, z).
bool findTree(const World& w, int& tx, int& tz, int& ty) {
    for (int r = 0; r < 60; ++r)
        for (int x = tx - r; x <= tx + r; ++x)
            for (int z = tz - r; z <= tz + r; ++z) {
                for (int y = 120; y > 0; --y)
                    if (w.getBlock(x, y, z) == LOG && w.getBlock(x, y + 1, z) == LEAVES) {
                        tx = x; tz = z; ty = y;
                        return true;
                    }
            }
    return false;
}

}  // namespace

// A flight over the generated world, one PNG per frame (for a preview video).
int flyover(const std::filesystem::path& out, int frames, const std::vector<uint8_t>& atlas, int atlasSize) {
    gImageW = 640;
    gImageH = 360;
    gQuiet = true;
    World w(20261024, Dimension::Overworld);
    Scene scene;
    int meshedCx = INT32_MIN, meshedCz = INT32_MIN;
    float x = 0, z = 0, height = 0, yaw = -kPi / 2;
    PlayerPose walker;
    for (int f = 0; f < frames; ++f) {
        float t = f / 24.0f;
        // Gently curving path at about 9 blocks a second.
        yaw = 0.6f * std::sin(t * 0.21f);
        Player heading;
        heading.yaw = yaw;
        Vec3 dir = heading.forward();
        dir = Vec3(dir.x, 0, dir.z).normalized();
        x += dir.x * 0.4f;
        z += dir.z * 0.4f;
        int cx = floorDiv((int)x, 16), cz = floorDiv((int)z, 16);
        if (std::abs(cx - meshedCx) + std::abs(cz - meshedCz) >= 1) {
            scene = meshWorld(w, cx, cz, 4);
            meshedCx = cx;
            meshedCz = cz;
        }
        float ground = 0;
        for (int dx = -6; dx <= 6; dx += 3)
            for (int dz = -6; dz <= 6; dz += 3) ground = std::max(ground, (float)w.surfaceHeight((int)x + dx, (int)z + dz));
        ground = std::max(ground, 62.0f);
        float want = ground + 13;
        height = f == 0 ? want : height + (want - height) * 0.08f;
        Player cam;
        cam.pos = Vec3(x, height, z);
        cam.yaw = yaw;
        cam.pitch = -0.32f;
        // A player walks ahead of the camera.
        Vec3 fwd = cam.forward();
        Vec3 ahead = cam.pos + Vec3(fwd.x, 0, fwd.z).normalized() * 9;
        walker.feet = Vec3(ahead.x, (float)w.surfaceHeight((int)std::floor(ahead.x), (int)std::floor(ahead.z)) + 1, ahead.z);
        walker.yaw = cam.yaw;
        walker.swing = std::sin(t * 9) * 0.7f;
        walker.held = STEEL_PICKAXE;
        for (int i = 0; i < 4; ++i) walker.armor[i] = (ItemId)armorItem(2, i);
        Scene s = scene;
        s.model = buildPlayerModel(walker);
        s.modelSkin = builtinSkins()[1].rgba;
        std::vector<Vertex> extras = buildPlayerExtras(walker);
        s.particles.insert(s.particles.end(), extras.begin(), extras.end());
        char name[64];
        std::snprintf(name, sizeof name, "fly_%04d.png", f);
        render(s, cam, (out / name).string(), atlas, atlasSize);
        if (f % 24 == 0) std::printf("frame %d / %d\n", f, frames);
    }
    return 0;
}

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    fs::path out = argc > 1 ? argv[1] : "build/snapshots";
    fs::create_directories(out);
    int atlasSize = 0;
    std::vector<uint8_t> atlas = buildFallbackAtlas(atlasSize);
    if (argc > 3 && std::string(argv[2]) == "--flyover") return flyover(out, std::atoi(argv[3]), atlas, atlasSize);
    if (argc > 2 && std::string(argv[2]) == "--atlas") {
        writePng((out / "atlas.png").string(), atlasSize, atlasSize, atlas);
        return 0;
    }

    World w(4242, Dimension::Overworld);
    // Look for a forest with flowers around.
    int tx = 40, tz = 40, ty = 0;
    w.ensureChunksAround(floorDiv(tx, 16), floorDiv(tz, 16), 6, 1000);
    if (!findTree(w, tx, tz, ty)) std::printf("no tree found\n");
    Scene scene = meshWorld(w, floorDiv(tx, 16), floorDiv(tz, 16), 4);

    // 1) A tree from a few blocks away, at eye height.
    Player cam;
    int gy = w.surfaceHeight(tx + 7, tz + 1) + 1;
    cam.pos = Vec3(tx + 7.5f, (float)gy + 1.5f, tz + 1.5f);
    cam.yaw = -kPi / 2 - 0.15f;
    cam.pitch = 0.12f;
    render(scene, cam, (out / "world_01_tree.png").string(), atlas, atlasSize);

    // 1b) The same tree from further away, slightly from above: the crown's shape and shade.
    cam.pos = Vec3(tx + 12.5f, (float)ty + 3.0f, tz + 6.5f);
    cam.yaw = -kPi / 2 - 0.45f;
    cam.pitch = -0.3f;
    render(scene, cam, (out / "world_01b_tree_shape.png").string(), atlas, atlasSize);

    // 2) Looking up into a crown from below the leaves: no holes into an empty inside.
    cam.pos = Vec3(tx + 1.5f, (float)ty - 0.5f, tz + 0.5f);
    cam.pitch = 1.0f;
    cam.yaw = -kPi / 2;
    render(scene, cam, (out / "world_02_under_crown.png").string(), atlas, atlasSize);

    // 3) Grass and flowers: a wide look over the landscape.
    cam.pos = Vec3(tx + 0.5f, (float)w.surfaceHeight(tx, tz) + 4.0f, tz + 20.5f);
    cam.yaw = 0;
    cam.pitch = -0.25f;
    render(scene, cam, (out / "world_03_meadow.png").string(), atlas, atlasSize);

    // 4) Dig particles right after breaking a block next to the camera, and the player model.
    int bx = tx + 4, bz = tz + 4, by = w.surfaceHeight(bx, bz);
    ParticleSystem ps;
    ItemId broken = w.getBlock(bx, by, bz);
    ps.spawnBurst(w, bx, by, bz, broken);
    ps.spawnChips(w, bx, by, bz, 0, 1, 0, broken, 8);
    w.setBlock(bx, by, bz, AIR);
    for (int i = 0; i < 4; ++i) ps.update(w, 1 / 60.0f);
    Scene s4 = meshWorld(w, floorDiv(bx, 16), floorDiv(bz, 16), 3);
    cam.pos = Vec3(bx + 0.5f, (float)by + 0.8f, bz + 3.5f);
    cam.yaw = 0;
    cam.pitch = -0.55f;
    Vec3 f = cam.forward();
    Vec3 right = cross(f, {0, 1, 0}).normalized(), up = cross(right, f).normalized();
    ps.buildVertices(right, up, s4.particles);
    std::printf("particles in the picture: %zu\n", ps.count());
    PlayerPose pose;
    pose.feet = Vec3(bx - 1.5f, (float)w.surfaceHeight(bx - 2, bz) + 1, bz + 0.5f);
    pose.yaw = kPi / 2;
    pose.swing = 0.5f;
    s4.model = buildPlayerModel(pose);
    s4.modelSkin = builtinSkins()[0].rgba;
    render(s4, cam, (out / "world_04_dig_particles.png").string(), atlas, atlasSize);

    // 5) VCStructures: the built-in house (turned so the door faces the camera) and the tower.
    {
        std::vector<Structure> built = builtinStructures();
        Structure house = built[0].rotated(2), tower = built[1];
        int hx = tx - 20, hz = tz + 30;
        w.ensureChunksAround(floorDiv(hx, 16), floorDiv(hz, 16), 4, 1000);
        int hy = w.surfaceHeight(hx + 3, hz + 3) + 1;
        std::vector<BlockChange> undo;
        std::string err;
        if (!placeStructure(w, house, hx, hy, hz, undo, err)) std::printf("house: %s\n", err.c_str());
        size_t houseBlocks = undo.size();
        if (!placeStructure(w, tower, hx + 10, w.surfaceHeight(hx + 12, hz + 2) + 1, hz, undo, err))
            std::printf("tower: %s\n", err.c_str());
        std::printf("structures: house changed %zu blocks, tower %zu\n", houseBlocks, undo.size());
        Scene s5 = meshWorld(w, floorDiv(hx + 6, 16), floorDiv(hz, 16), 3);
        cam.pos = Vec3(hx + 7.0f, (float)hy + 4.0f, hz + 19.0f);
        cam.yaw = 0.25f;
        cam.pitch = -0.2f;
        render(s5, cam, (out / "world_05_structures.png").string(), atlas, atlasSize);
    }

    // 7) The new biomes: find a spot of each and look over it from a hill.
    {
        World wb(4242, Dimension::Overworld);
        const char* ids[] = {"snowy_taiga", "ice_plains", "desert", "mountains"};
        for (const char* id : ids) {
            int fx = 0, fz = 0, bestScore = -1;
            for (int x = -400; x <= 400 && bestScore < 9; x += 24)
                for (int z = -400; z <= 400 && bestScore < 9; z += 24) {
                    const BiomeDef* b = wb.biomeAt(x, z);
                    if (!b || b->id != id) continue;
                    int score = 0;  // prefer places deep inside the biome
                    for (int dx = -1; dx <= 1; ++dx)
                        for (int dz = -1; dz <= 1; ++dz) {
                            const BiomeDef* n = wb.biomeAt(x + dx * 24, z + dz * 24);
                            score += n && n->id == id;
                        }
                    if (score > bestScore) bestScore = score, fx = x, fz = z;
                }
            Player cb;
            int gy = wb.surfaceHeight(fx, fz + 30);
            cb.pos = Vec3(fx + 0.5f, (float)std::max(gy, 64) + 8, fz + 30.5f);
            cb.yaw = 0;
            cb.pitch = -0.28f;
            if (std::string(id) == "mountains") {
                // Look at the highest peak nearby from a distance, slightly from below.
                int px = fx, pz = fz, ph = 0;
                for (int x = fx - 64; x <= fx + 64; x += 4)
                    for (int z = fz - 64; z <= fz + 64; z += 4)
                        if (int h = wb.surfaceHeight(x, z); h > ph) ph = h, px = x, pz = z;
                fx = px;
                fz = pz;
                cb.pos = Vec3(px + 0.5f, (float)ph - 6, pz + 45.5f);
                cb.pitch = 0.12f;
            }
            Scene sb = meshWorld(wb, floorDiv(fx, 16), floorDiv(fz + 20, 16), 4);
            render(sb, cb, (out / (std::string("world_14_biome_") + id + ".png")).string(), atlas, atlasSize);
        }
    }

    // 8) Generated structures: find each kind of loot chest and look at it.
    {
        World ws(4242, Dimension::Overworld);
        struct Want {
            uint8_t chest;
            const char* name;
            int searchX, searchZ;
        };
        const Want wants[] = {{LOOT_CHEST_RUIN, "ruin", 0, 0}, {LOOT_CHEST_SHIP, "shipwreck", -2000, 784},
                              {LOOT_CHEST_DUNGEON, "dungeon", 0, 0}, {LOOT_CHEST_MINE, "mineshaft", 0, 0}};
        for (const Want& want : wants) {
            int fx = 0, fy = 0, fz = 0;
            bool found = false;
            for (int r = 0; r <= 14 && !found; ++r)
                for (int cx = floorDiv(want.searchX, 16) - r; cx <= floorDiv(want.searchX, 16) + r && !found; ++cx)
                    for (int cz = floorDiv(want.searchZ, 16) - r; cz <= floorDiv(want.searchZ, 16) + r && !found; ++cz) {
                        ws.ensureChunksAround(cx, cz, 0, 1);
                        const Chunk* c = ws.getChunk(cx, cz);
                        for (int x = 0; x < 16 && !found; ++x)
                            for (int z = 0; z < 16 && !found; ++z)
                                for (int y = 1; y < kChunkHeight && !found; ++y)
                                    if (c->get(x, y, z) == want.chest) {
                                        fx = cx * 16 + x, fy = y, fz = cz * 16 + z;
                                        found = true;
                                    }
                    }
            if (!found) continue;
            Scene sc = meshWorld(ws, floorDiv(fx, 16), floorDiv(fz, 16), 3);
            Player cam;
            // Above ground from outside; underground from inside the room or corridor.
            bool under = want.chest == LOOT_CHEST_DUNGEON || want.chest == LOOT_CHEST_MINE;
            cam.pos = under ? Vec3(fx + 0.5f, (float)fy, fz + 0.5f) : Vec3(fx + 7.5f, (float)fy + 5, fz + 7.5f);
            cam.yaw = under ? 0.6f : -2.35f;
            cam.pitch = under ? -0.1f : -0.45f;
            if (under) {
                // Step back from the chest to see the room.
                for (int k = 0; k < 4; ++k) {
                    Vec3 back = cam.pos - cam.forward() * 1.0f;
                    if (isSolid(ws.getBlock((int)std::floor(back.x), (int)std::floor(back.y + 1.6f), (int)std::floor(back.z)))) break;
                    cam.pos = back;
                }
            }
            render(sc, cam, (out / (std::string("world_15_") + want.name + ".png")).string(), atlas, atlasSize);
        }
    }

    // 9) Third person: armour, a strike with a ruby pickaxe, holding a block, sitting in a boat.
    {
        GenConfig flatCfg = GenConfig::defaults();
        flatCfg.flat = true;
        World wp(4242, Dimension::Overworld, flatCfg);
        int px = 8, pz = 8;
        wp.ensureChunksAround(0, 0, 3, 100);
        for (int x = 4; x < 13; ++x)
            for (int z = 4; z < 13; ++z) wp.setBlock(x, 3, z, WATER);  // a pond for the boat
        Scene base = meshWorld(wp, floorDiv(px, 16), floorDiv(pz, 16), 2);
        struct Shot {
            const char* name;
            int material;
            uint8_t held;
            float attack;
            bool boat;
        };
        const Shot shots[] = {{"world_16_third_person_steel", 3, RUBY_PICKAXE, 0.35f, false},
                              {"world_17_third_person_bronze", 2, BRICKS, 1, false},
                              {"world_18_third_person_boat", 4, AIR, 1, true}};
        for (const Shot& sh : shots) {
            Scene sc = base;
            PlayerPose pose;
            pose.feet = Vec3(px + 0.5f, 4, pz + 0.5f);
            pose.yaw = 0.6f;
            pose.held = sh.held;
            pose.attack = sh.attack;
            for (int k = 0; k < 4; ++k) pose.armor[k] = armorItem(sh.material, k);
            if (sh.boat) {
                BoatSystem boats;
                boats.spawn(0, pose.feet + Vec3(0, -0.15f, 0), pose.yaw);
                std::vector<Vertex> bv;
                boats.buildVertices(0, bv);
                sc.particles.insert(sc.particles.end(), bv.begin(), bv.end());
                pose.riding = true;
                pose.feet.y -= 0.3f;
            }
            sc.model = buildPlayerModel(pose);
            sc.modelSkin = builtinSkins()[0].rgba;
            std::vector<Vertex> extras = buildPlayerExtras(pose);
            sc.particles.insert(sc.particles.end(), extras.begin(), extras.end());
            Player cam;
            cam.pos = pose.feet + Vec3(1.5f, -0.5f, -1.8f);
            cam.yaw = std::atan2(pose.feet.x - cam.pos.x, -(pose.feet.z - cam.pos.z));
            cam.pitch = -0.12f;
            render(sc, cam, (out / (std::string(sh.name) + ".png")).string(), atlas, atlasSize);
        }
    }

    // 6) The first-person hand: empty, holding a block, mid-swing while mining, placing, holding an item.
    {
        Scene s6 = meshWorld(w, floorDiv(tx, 16), floorDiv(tz, 16), 3);
        s6.modelSkin = builtinSkins()[0].rgba;
        Player c6;
        c6.pos = Vec3(tx + 0.5f, (float)w.surfaceHeight(tx, tz + 24) + 1, tz + 24.5f);
        c6.yaw = 0;
        c6.pitch = -0.1f;
        struct Shot {
            const char* name;
            uint8_t held;
            float swingAt, placeAt;
        };
        const Shot shots[] = {{"world_06_hand_empty", AIR, -1, -1},     {"world_07_hand_block", GRASS, -1, -1},
                              {"world_08_hand_mining", AIR, 0.35f, -1}, {"world_09_hand_block_mining", COBBLESTONE, 0.4f, -1},
                              {"world_10_hand_place", PLANKS, -1, 0.5f}, {"world_11_hand_item", STONE_PICKAXE, -1, -1},
                              {"world_12_hand_ruby_pickaxe", RUBY_PICKAXE, -1, -1}, {"world_13_hand_steel_axe_mining", STEEL_AXE, 0.4f, -1}};
        for (const Shot& sh : shots) {
            HandAnimator hand;
            hand.update(0, 0, true, false, sh.held, 0);
            if (sh.swingAt >= 0) {
                hand.swing();
                hand.update(sh.swingAt * HandAnimator::kSwingTime, 0, true, false, sh.held, 0);
            }
            if (sh.placeAt >= 0) {
                hand.place();
                hand.update(sh.placeAt * HandAnimator::kPlaceTime, 0, true, false, sh.held, 0);
            }
            buildFirstPerson(hand, 1.0f, s6.handSkin, s6.handAtlas);
            render(s6, c6, (out / (std::string(sh.name) + ".png")).string(), atlas, atlasSize);
        }
    }

    // 1.1.0: mobs on a meadow, the sky through the day, rain and snow, a bed.
    {
        World mw(4242, Dimension::Overworld);
        int bx = 0, bz = 0;
        for (int r = 0; r < 600 && !(mw.biomeAt(bx, bz) && mw.biomeAt(bx, bz)->id == "plains"); r += 16) bx = r;
        mw.ensureChunksAround(floorDiv(bx, 16), floorDiv(bz, 16), 5, 1000);
        const int g = mw.surfaceHeight(bx, bz) + 1;
        for (int x = bx - 10; x <= bx + 10; ++x)
            for (int z = bz - 6; z <= bz + 14; ++z) {
                int gy = mw.surfaceHeight(x, z);
                for (int y = std::min(gy + 1, g); y < g + 14; ++y) mw.setBlock(x, y, z, AIR);  // a clear stage
                for (int y = gy; y < g; ++y) mw.setBlock(x, y, z, GRASS);
            }
        MobSystem ms;
        const MobType kinds[] = {MobType::Pig, MobType::Sheep, MobType::PumpkinSlime, MobType::ForgottenMan, MobType::CrimsonMan};
        for (int i = 0; i < 5; ++i) {
            ms.spawn(kinds[i], 0, Vec3(bx - 5.0f + i * 2.4f, (float)g, bz + 0.5f));
            ms.mobs().back().yaw = kPi + 0.35f;  // facing the camera, a bit turned
            ms.mobs().back().swing = (i % 2) ? 0.4f : -0.3f;
            ms.mobs().back().phase = 1;
        }
        mw.setBlock(bx + 5, g, bz - 3, BED + 2 * 2);      // a bed facing south
        mw.setBlock(bx + 5, g, bz - 2, BED + 2 * 2 + 1);
        Scene s19 = meshWorld(mw, floorDiv(bx, 16), floorDiv(bz, 16), 4);
        ms.buildVertices(0, 0.5f, s19.particles);
        cam.pos = Vec3(bx + 0.5f, g + 2.2f, bz + 8.5f);
        cam.yaw = 0;
        cam.pitch = -0.18f;
        render(s19, cam, (out / "world_19_mobs.png").string(), atlas, atlasSize);
        // The same meadow at sunset, at night under the stars, and in the rain.
        const GenConfig def = GenConfig::defaults();
        struct Moment { const char* name; float ticks; float rain; float yaw, pitch; };
        const Moment moments[] = {{"world_20_sunset", 11700, 0, -kPi / 2 + 0.25f, 0.12f},
                                  {"world_21_night", 18000, 0, kPi / 2, 0.55f},
                                  {"world_22_rain", 6000, 1, 0, -0.12f}};
        for (const Moment& m : moments) {
            Scene sc = s19;
            SkyState sky = computeSky(m.ticks, def.overworld.sky, m.rain);
            std::copy(sky.sky, sky.sky + 3, gSky);
            gBrightness = sky.light;
            Player c = cam;
            if (m.rain == 0) c.pos = c.pos + Vec3(0, 9, 0);  // above the trees, towards the sky
            c.yaw = m.yaw;
            c.pitch = m.pitch;
            std::vector<Vertex> sunMoon, stars, clouds, rain;
            buildSkyObjects(c.eye(), m.ticks, sunMoon, stars);
            sc.sky = sunMoon;
            if (sky.stars > 0.5f) sc.sky.insert(sc.sky.end(), stars.begin(), stars.end());
            buildClouds(c.eye(), 30, sky.clouds, clouds);
            sc.particles.insert(sc.particles.end(), clouds.begin(), clouds.end());
            if (m.rain > 0) {
                buildPrecipitation(mw, c.eye(), Precipitation::Rain, 0.7f, 1, sky.light * 0.9f, rain);
                sc.particles.insert(sc.particles.end(), rain.begin(), rain.end());
            }
            render(sc, c, (out / (std::string(m.name) + ".png")).string(), atlas, atlasSize);
        }
        const float day[3] = {0.53f, 0.74f, 1.0f};
        std::copy(day, day + 3, gSky);
        gBrightness = 1;
        // Snow in the snowy pine forest.
        int sx = 0, sz = 0;
        for (int r = 0; r < 3000; r += 48) {
            const BiomeDef* b = mw.biomeAt(r, 0);
            if (b && b->id == "snowy_taiga") { sx = r; break; }
            b = mw.biomeAt(0, r);
            if (b && b->id == "snowy_taiga") { sz = r; break; }
        }
        mw.ensureChunksAround(floorDiv(sx, 16), floorDiv(sz, 16), 5, 1000);
        Scene s23 = meshWorld(mw, floorDiv(sx, 16), floorDiv(sz, 16), 4);
        cam.pos = Vec3(sx + 0.5f, mw.surfaceHeight(sx, sz) + 5.0f, sz + 0.5f);
        cam.yaw = 0.6f;
        cam.pitch = -0.25f;
        SkyState snowSky = computeSky(5000, def.overworld.sky, 1);
        std::copy(snowSky.sky, snowSky.sky + 3, gSky);
        gBrightness = snowSky.light;
        std::vector<Vertex> snow;
        buildPrecipitation(mw, cam.eye(), Precipitation::Snow, 3.0f, 1, snowSky.light, snow);
        s23.particles.insert(s23.particles.end(), snow.begin(), snow.end());
        render(s23, cam, (out / "world_23_snow.png").string(), atlas, atlasSize);
        std::copy(day, day + 3, gSky);
        gBrightness = 1;
    }
    // 24) 1.4.0 stairs and slabs: a staircase of every material, slabs, upside-down and double ones.
    {
        int bx = tx + 6, bz = tz - 10, by = w.surfaceHeight(bx, bz) + 1;
        for (int x = bx - 6; x < bx + 30; ++x)
            for (int z = bz - 6; z < bz + 14; ++z) {
                for (int y = by; y < by + 24; ++y) w.setBlock(x, y, z, AIR);
                for (int y = by - 3; y < by; ++y) w.setBlock(x, y, z, y == by - 1 ? GRASS : DIRT);
            }
        for (int m = 0; m < kShapeMaterials; ++m) {
            const int x = bx + m * 2;
            w.setBlock(x, by, bz, stairsId(m, 0, false));
            w.setBlock(x, by, bz - 1, shapeMaterialBlock(m));
            w.setBlock(x, by + 1, bz - 1, stairsId(m, 0, false));
            w.setBlock(x, by, bz + 2, slabId(m, 0));
            w.setBlock(x + 1, by, bz + 2, slabId(m, 2));
            w.setBlock(x, by + 2, bz + 2, slabId(m, 1));
            w.setBlock(x + 1, by + 2, bz, stairsId(m, 1, true));
        }
        Scene s24 = meshWorld(w, floorDiv(bx + 10, 16), floorDiv(bz, 16), 3);
        cam.pos = Vec3(bx + 4.5f, by + 3.0f, bz + 7.5f);
        cam.yaw = 0.35f;
        cam.pitch = -0.3f;
        render(s24, cam, (out / "world_24_stairs_slabs.png").string(), atlas, atlasSize);
    }
    // 25) 1.4.0 monsters on a meadow: spider, bat, TNT zombie (hissing), tar monster.
    {
        int bx = tx + 6, bz = tz - 10, by = w.surfaceHeight(bx, bz) + 1;
        for (int x = bx - 6; x < bx + 30; ++x)
            for (int z = bz - 6; z < bz + 14; ++z)
                for (int y = by; y < by + 24; ++y) w.setBlock(x, y, z, AIR);
        MobSystem ms;
        const MobType kinds[] = {MobType::Spider, MobType::Bat, MobType::TntZombie, MobType::TarMonster};
        for (int i = 0; i < 4; ++i) {
            ms.spawn(kinds[i], 0, Vec3(bx - 3.0f + i * 2.6f, (float)by + (i == 1 ? 1.2f : 0), bz + 0.5f));
            ms.mobs().back().yaw = kPi + 0.35f;
            ms.mobs().back().swing = (i % 2) ? 0.4f : -0.3f;
        }
        ms.mobs()[2].fuse = 0.05f;
        for (int x = bx + 3; x < bx + 7; ++x)
            for (int z = bz - 1; z < bz + 2; ++z) w.setBlock(x, by - 1, z, TAR);
        Scene s25 = meshWorld(w, floorDiv(bx, 16), floorDiv(bz, 16), 3);
        ms.buildVertices(0, 0.5f, s25.particles);
        cam.pos = Vec3(bx + 1.0f, by + 2.0f, bz + 7.0f);
        cam.yaw = 0;
        cam.pitch = -0.2f;
        render(s25, cam, (out / "world_25_new_mobs.png").string(), atlas, atlasSize);
    }
    // 27) The Anniversary showcase on a meadow.
    {
        int bx = tx + 2, bz = tz - 30, by = w.surfaceHeight(bx + 21, bz + 7) + 1;
        Structure plaza = anniversaryShowcase();
        for (int x = bx - 4; x < bx + kShowcaseW + 4; ++x)
            for (int z = bz - 4; z < bz + kShowcaseD + 22; ++z) {
                for (int y = by; y < by + 24; ++y) w.setBlock(x, y, z, AIR);
                for (int y = by - 3; y < by; ++y) w.setBlock(x, y, z, y == by - 1 ? GRASS : DIRT);
            }
        for (int x = 0; x < plaza.sx; ++x)
            for (int y = 0; y < plaza.sy; ++y)
                for (int z = 0; z < plaza.sz; ++z) w.setBlock(bx + x, by - 1 + y, bz + z, plaza.at(x, y, z));
        Scene s27 = meshWorld(w, floorDiv(bx + 21, 16), floorDiv(bz + 7, 16), 3);
        Player c27;
        c27.pos = Vec3(bx + 21.5f, by + 3.0f, bz + 27.0f);
        c27.yaw = 0;
        c27.pitch = -0.2f;
        render(s27, c27, (out / "world_27_anniversary.png").string(), atlas, atlasSize);
        c27.pos = Vec3(bx + 15.5f, by + 1.0f, bz + 13.0f);
        c27.yaw = 0.25f;
        c27.pitch = -0.15f;
        render(s27, c27, (out / "world_27b_anniversary.png").string(), atlas, atlasSize);
    }
    // 28) Night with lamps: powerstone light, lamps and a lantern light up the ground around them.
    {
        int bx = tx + 2, bz = tz - 30, by = w.surfaceHeight(bx + 21, bz + 7) + 1;
        for (int x = bx - 10; x < bx + 30; ++x)
            for (int z = bz - 10; z < bz + 30; ++z) {
                for (int y = by; y < by + 24; ++y) w.setBlock(x, y, z, AIR);
                for (int y = by - 3; y < by; ++y) w.setBlock(x, y, z, y == by - 1 ? GRASS : DIRT);
            }
        w.setBlock(bx + 4, by, bz + 4, POWERSTONE_LIGHT);
        w.setBlock(bx + 12, by, bz + 6, POWERSTONE_LAMP_ON);
        w.setBlock(bx + 12, by + 1, bz + 6, POWERSTONE_LAMP_ON);
        w.setBlock(bx + 8, by, bz - 2, PARTY_LANTERN);
        for (int x = bx + 14; x < bx + 20; ++x)
            for (int y = by; y < by + 4; ++y) w.setBlock(x, y, bz + 2, STONE_BRICKS);
        // On the wall: a switch, a cable running up and along it, and a lamp on top.
        w.setBlock(bx + 16, by + 1, bz + 3, WALL_SWITCH + 4);
        w.setBlock(bx + 16, by + 2, bz + 3, WALL_CABLE + 4);
        w.setBlock(bx + 16, by + 3, bz + 3, WALL_CABLE + 4);
        w.setBlock(bx + 17, by + 3, bz + 3, WALL_CABLE + 4);
        w.setBlock(bx + 18, by + 3, bz + 3, WALL_CABLE + 4);
        w.setBlock(bx + 18, by + 4, bz + 2, POWERSTONE_LAMP_ON);
        w.setBlock(bx + 17, by, bz + 5, POWER_CABLE_ON);
        w.setBlock(bx + 18, by, bz + 5, SWITCH_ON);
        Scene s28 = meshWorld(w, floorDiv(bx + 8, 16), floorDiv(bz + 4, 16), 3);
        Player c28;
        c28.pos = Vec3(bx + 12.5f, by + 1.0f, bz + 11.0f);
        c28.yaw = 0.35f;
        c28.pitch = -0.35f;
        const float keep = gBrightness;
        float keepSky[3];
        std::copy(gSky, gSky + 3, keepSky);
        gBrightness = 0.18f;
        gSky[0] = 0.02f, gSky[1] = 0.03f, gSky[2] = 0.08f;
        render(s28, c28, (out / "world_28_night_lamps.png").string(), atlas, atlasSize);
        gBrightness = keep;
        std::copy(keepSky, keepSky + 3, gSky);
    }
    // 26) Caves 2.0: inside a cavern with a lake (water or tar) and dripstone.
    {
        World cw(1234, Dimension::Overworld);
        int fx = 0, fy = 0, fz = 0, best = 0;
        cw.ensureChunksAround(0, 0, 6, 1000);
        for (auto& [k, c] : cw.chunks())
            for (int x = 2; x < 14; x += 2)
                for (int z = 2; z < 14; z += 2)
                    for (int y = kCaveLakeLevel + 1; y < 40; ++y) {
                        if (c->get(x, y, z) != AIR || !isLiquid(c->get(x, y - 1, z))) continue;
                        // Room around: count air above and around.
                        int room = 0;
                        for (int d = 1; d < 8 && y + d < kChunkHeight; ++d) room += c->get(x, y + d, z) == AIR;
                        int drip = 0;
                        for (int dx = -2; dx <= 2; ++dx)
                            for (int dz = -2; dz <= 2; ++dz)
                                for (int dy = 0; dy < 10; ++dy)
                                    if (x + dx >= 0 && x + dx < 16 && z + dz >= 0 && z + dz < 16)
                                        drip += isDripstoneSpike(c->get(x + dx, y + dy, z + dz));
                        int score = room * 3 + drip * 2 + (c->get(x, y - 1, z) == TAR ? 10 : 0);
                        if (score > best) best = score, fx = c->cx * 16 + x, fy = y, fz = c->cz * 16 + z;
                    }
        std::printf("cave view at %d %d %d (score %d)\n", fx, fy, fz, best);
        Scene s26 = meshWorld(cw, floorDiv(fx, 16), floorDiv(fz, 16), 3);
        Player cc;
        cc.pos = Vec3(fx + 0.5f, (float)fy, fz + 0.5f);
        cc.yaw = 0.8f;
        cc.pitch = -0.15f;
        render(s26, cc, (out / "world_26_cave.png").string(), atlas, atlasSize);
        cc.yaw = 0.8f + kPi;
        render(s26, cc, (out / "world_26b_cave.png").string(), atlas, atlasSize);
    }
    // 29-33) 1.5.0 Forgotten Powerstone.
    {
        auto stamp = [](World& world, const Structure& st, int ox, int oy, int oz, ItemId around, int pad, bool water) {
            for (int x = ox - pad; x < ox + st.sx + pad; ++x)
                for (int z = oz - pad; z < oz + st.sz + pad + 10; ++z) {
                    for (int y = oy; y < oy + 28; ++y) world.setBlock(x, y, z, water && y < oy + 9 ? (ItemId)WATER : (ItemId)AIR);
                    for (int y = oy - 4; y < oy; ++y) world.setBlock(x, y, z, y == oy - 1 ? around : DIRT);
                }
            for (int x = 0; x < st.sx; ++x)
                for (int y = 0; y < st.sy; ++y)
                    for (int z = 0; z < st.sz; ++z) {
                        ItemId b = st.at(x, y, z);
                        if (b == Structure::kVoid) continue;
                        if (b == AIR && water) b = WATER;
                        world.setBlock(ox + x, oy - 1 + y, oz + z, b);
                    }
        };
        // 29) The wasteland as the generator makes it.
        World ww(4242, Dimension::Overworld);
        // The middle of a stretch of wasteland: the column with the most wasteland around it.
        int wx = 0, wz = 0, bestCount = -1;
        for (int x = -400; x <= 400; x += 16)
            for (int z = -400; z <= 400; z += 16) {
                int n = 0;
                for (int dx = -48; dx <= 48; dx += 16)
                    for (int dz = -48; dz <= 48; dz += 16) {
                        const BiomeDef* b = ww.biomeAt(x + dx, z + dz);
                        n += b && b->id == "powerstone_wasteland";
                    }
                if (n > bestCount && ww.surfaceHeight(x, z) > 63) bestCount = n, wx = x, wz = z;
            }
        std::printf("wasteland view at %d %d (%d of 49 samples)\n", wx, wz, bestCount);
        ww.ensureChunksAround(floorDiv(wx, 16), floorDiv(wz, 16), 5, 1000);
        Scene s29 = meshWorld(ww, floorDiv(wx, 16), floorDiv(wz, 16), 4);
        Player c29;
        c29.pos = Vec3(wx + 0.5f, ww.surfaceHeight(wx, wz) + 7.0f, wz + 0.5f);
        c29.yaw = kPi;
        c29.pitch = -0.28f;
        render(s29, c29, (out / "world_29_wasteland.png").string(), atlas, atlasSize);
        c29.yaw = kPi / 2;
        render(s29, c29, (out / "world_29b_wasteland.png").string(), atlas, atlasSize);
        // 30) A forgotten village.
        const Structure village = buildWorldStructure(WorldStructure::ForgottenVillage, 1234);
        int bx = tx - 20, bz = tz + 40, by = w.surfaceHeight(bx, bz) + 1;
        stamp(w, village, bx, by, bz, WASTELAND_GRASS, 4, false);
        Scene s30 = meshWorld(w, floorDiv(bx + 20, 16), floorDiv(bz + 20, 16), 3);
        Player c30;
        c30.pos = Vec3(bx + 20.5f, by + 13.0f, bz + 48.0f);
        c30.yaw = 0;
        c30.pitch = -0.45f;
        render(s30, c30, (out / "world_30_forgotten_village.png").string(), atlas, atlasSize);
        c30.pos = Vec3(bx + 20.5f, by + 1.6f, bz + 30.0f);
        c30.pitch = -0.05f;
        render(s30, c30, (out / "world_30b_forgotten_village.png").string(), atlas, atlasSize);
        // 31) Drowned ruins under water.
        const Structure drowned = buildWorldStructure(WorldStructure::DrownedRuins, 99);
        bx = tx + 60, bz = tz + 40, by = w.surfaceHeight(bx, bz) - 6;
        stamp(w, drowned, bx, by, bz, GRAVEL, 6, true);
        Scene s31 = meshWorld(w, floorDiv(bx + 9, 16), floorDiv(bz + 9, 16), 3);
        Player c31;
        c31.pos = Vec3(bx + 9.5f, by + 11.0f, bz + 27.0f);
        c31.yaw = 0;
        c31.pitch = -0.5f;
        render(s31, c31, (out / "world_31_drowned_ruins.png").string(), atlas, atlasSize);
        // 32) A broken portal ruin.
        const Structure portal = buildWorldStructure(WorldStructure::BrokenPortal, 4321);
        bx = tx + 100, bz = tz + 40, by = w.surfaceHeight(bx, bz) + 1;
        stamp(w, portal, bx, by, bz, GRASS, 6, false);
        Scene s32 = meshWorld(w, floorDiv(bx + 6, 16), floorDiv(bz + 6, 16), 3);
        Player c32;
        c32.pos = Vec3(bx + 6.5f, by + 4.0f, bz + 18.0f);
        c32.yaw = 0;
        c32.pitch = -0.25f;
        render(s32, c32, (out / "world_32_broken_portal.png").string(), atlas, atlasSize);
        // 33) The new blocks and the Husking: a powerstone house, fences, heads, flowers, bark.
        bx = tx + 140, bz = tz + 40, by = w.surfaceHeight(bx, bz) + 1;
        for (int x = bx - 8; x < bx + 26; ++x)
            for (int z = bz - 8; z < bz + 22; ++z) {
                for (int y = by; y < by + 24; ++y) w.setBlock(x, y, z, AIR);
                for (int y = by - 3; y < by; ++y) w.setBlock(x, y, z, y == by - 1 ? (x < bx + 9 ? WASTELAND_GRASS : GRASS) : DIRT);
            }
        for (int x = bx; x < bx + 7; ++x)
            for (int y = by; y < by + 4; ++y) {
                const bool window = y == by + 1 && (x == bx + 2 || x == bx + 4);
                w.setBlock(x, y, bz, window ? POWERSTONE_GLASS : y == by + 3 ? CHISELED_POWERSTONE_BRICKS : (x + y) % 3 ? POWERSTONE_BRICKS : CRACKED_POWERSTONE_BRICKS);
            }
        for (int x = bx - 1; x < bx + 8; ++x) w.setBlock(x, by + 4, bz, slabId(12, 0));
        for (int x = bx; x < bx + 7; ++x) w.setBlock(x, by, bz + 1, stairsId(12, 0, false));
        w.setBlock(bx + 3, by + 2, bz + 1, POWERSTONE_LAMP_ON);
        const ItemId fences[6] = {FENCE, SPRUCE_FENCE, VOID_FENCE, NETHERPERK_FENCE, NETHER_BRICK_FENCE, POWERSTONE_FENCE};
        for (int i = 0; i < 6; ++i) {
            w.setBlock(bx + 10 + i * 2, by, bz + 1, fences[i]);
            w.setBlock(bx + 11 + i * 2, by, bz + 1, fences[i]);
        }
        for (int i = 0; i < 4; ++i) w.setBlock(bx + 1 + i * 2, by, bz + 4, (ItemId)(SKELETON_HEAD + (i == 3 ? 1 : 0)));
        w.setBlock(bx + 2, by - 1, bz + 6, DEBRIS_GRAVEL);
        w.setBlock(bx + 3, by - 1, bz + 6, DEBRIS_GRAVEL);
        for (int i = 0; i < 12; ++i) {
            const ItemId plant = i % 3 == 0 ? GRASSER_FLOWER : i % 3 == 1 ? DRY_GRASS : FERN;
            w.setBlock(bx - 3 + i * 2, by, bz + 8 + (i % 2), plant);
        }
        for (int k = 0; k < 5; ++k) w.setBlock(bx + 22, by + k, bz + 2, LOG);
        for (int dx = -2; dx <= 2; ++dx)
            for (int dz = -2; dz <= 2; ++dz)
                for (int dy = 3; dy <= 5; ++dy)
                    if (std::abs(dx) + std::abs(dz) + (dy == 5) * 2 < 4 && !(dx == 0 && dz == 0 && dy < 5)) w.setBlock(bx + 22 + dx, by + dy, bz + 2 + dz, LEAVES);
        MobSystem ms;
        ms.spawn(MobType::Husking, 0, Vec3(bx + 6.5f, (float)by, bz + 6.5f));
        ms.mobs().back().yaw = kPi + 0.3f;
        ms.mobs().back().attackAnim = 0.6f;
        ms.spawn(MobType::Husking, 0, Vec3(bx + 9.5f, (float)by, bz + 9.0f));
        ms.mobs().back().yaw = kPi - 0.4f;
        ms.mobs().back().swing = 0.5f;
        Scene s33 = meshWorld(w, floorDiv(bx + 8, 16), floorDiv(bz + 4, 16), 3);
        ms.buildVertices(0, 0.5f, s33.particles);
        Player c33;
        c33.pos = Vec3(bx + 8.5f, by + 2.5f, bz + 17.0f);
        c33.yaw = 0;
        c33.pitch = -0.15f;
        render(s33, c33, (out / "world_33_powerstone_blocks.png").string(), atlas, atlasSize);
        c33.pos = Vec3(bx + 7.5f, by + 1.6f, bz + 11.5f);
        c33.yaw = 0.1f;
        c33.pitch = -0.1f;
        render(s33, c33, (out / "world_33b_husking.png").string(), atlas, atlasSize);
    }
    return 0;
}

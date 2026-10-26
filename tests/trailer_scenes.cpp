// Films the story scenes of the VoxelCraft trailer: a real game Session driven by a
// script (walking, turning, chopping, building, mining) and fly-overs of the
// generated worlds, drawn with the software renderer. Frames go to <out>/<scene>/f_#####.png;
// tools/trailer2.py turns them into the video.
#include <array>
#include <chrono>
#include <memory>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/Blocks.h"
#include "engine/Items.h"
#include "engine/Mesher.h"
#include "engine/Particles.h"
#include "engine/Player.h"
#include "engine/PlayerModel.h"
#include "engine/Portal.h"
#include "engine/Save.h"
#include "engine/Session.h"
#include "engine/Skin.h"
#include "engine/TexturePack.h"
#include "engine/World.h"
#include "soft_render.h"

using namespace vc;
namespace fs = std::filesystem;

namespace {

constexpr float kFps = 24;
std::vector<uint8_t> gAtlas;
int gAtlasSize = 0;
fs::path gOut;

// Chunk meshes kept between frames; only chunks that changed are rebuilt.
struct MeshCache {
    const World* world = nullptr;
    std::unordered_map<int64_t, ChunkMesh> meshes;

    Scene scene(World& w, const Vec3& center, int radius) {
        if (&w != world) {
            meshes.clear();
            world = &w;
            for (auto& [k, c] : w.chunks()) c->dirty = true;
        }
        const int pcx = floorDiv((int)std::floor(center.x), kChunkSize), pcz = floorDiv((int)std::floor(center.z), kChunkSize);
        w.ensureChunksAround(pcx, pcz, radius + 1, 1000);
        Scene s;
        s.world = &w;
        for (auto& [k, c] : w.chunks()) {
            int dx = c->cx - pcx, dz = c->cz - pcz;
            if (dx * dx + dz * dz > radius * radius || !canMesh(w, c->cx, c->cz)) continue;
            const int64_t key = (int64_t)(((uint64_t)(uint32_t)c->cx << 32) | (uint32_t)c->cz);
            auto it = meshes.find(key);
            if (it == meshes.end() || c->dirty) {
                ChunkMesh m;
                buildChunkMesh(w, *c, m);
                c->dirty = false;
                it = meshes.insert_or_assign(key, std::move(m)).first;
            }
            s.opaque.insert(s.opaque.end(), it->second.opaque.begin(), it->second.opaque.end());
            s.translucent.insert(s.translucent.end(), it->second.translucent.begin(), it->second.translucent.end());
        }
        return s;
    }
};

Player lookAt(const Vec3& eye, const Vec3& target) {
    Player p;
    Vec3 d = (target - eye).normalized();
    p.yaw = std::atan2(d.x, -d.z);
    p.pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f));
    p.pos = eye - Vec3(0, Player::kEyeHeight, 0);
    return p;
}

float yawTo(const Vec3& from, const Vec3& to) { return std::atan2(to.x - from.x, -(to.z - from.z)); }
float pitchTo(const Vec3& from, const Vec3& to) {
    Vec3 d = to - from;
    return std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z));
}
float smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
Vec3 mix(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
float angleMix(float a, float b, float t) {
    float d = std::remainder(b - a, 2 * kPi);
    return a + d * t;
}

struct Film {
    std::string name;
    int frame = 0;
    fs::path dir;
    explicit Film(const std::string& n) : name(n), dir(gOut / n) { fs::create_directories(dir); }
    void shot(const Scene& s, const Player& cam) {
        char file[32];
        std::snprintf(file, sizeof file, "f_%05d.png", frame++);
        render(s, cam, (dir / file).string(), gAtlas, gAtlasSize);
    }
};

// ---------------------------------------------------------------- the story

struct Story {
    WorldInfo info;
    std::unique_ptr<Session> s;
    Settings settings;
    double t = 0;
    MeshCache cache;
    std::vector<uint8_t> heroSkin = developerSkin().rgba, friendSkin = builtinSkins()[0].rgba;
    Vec3 camPos;
    bool camSet = false;
    float placeAnim = 1;  // drives the arm while the script places blocks
    bool friendOn = false;
    PlayerPose friendPose;

    void step(int substeps = 2) {
        for (int i = 0; i < substeps; ++i) s->update(1 / kFps / substeps, t += 1 / kFps / substeps, 600, 270, settings);
        placeAnim = std::min(1.0f, placeAnim + 1 / kFps / 0.25f);
    }

    Scene scene(const Player& cam) {
        World& w = s->currentWorld();
        Scene sc = cache.scene(w, s->player().pos, 4);
        PlayerPose pose = s->pose();
        if (placeAnim < 1) pose.place = placeAnim;
        sc.model = buildPlayerModel(pose);
        sc.modelSkin = heroSkin;
        std::vector<Vertex> extras = buildPlayerExtras(pose);
        sc.particles.insert(sc.particles.end(), extras.begin(), extras.end());
        std::vector<Vertex> v;
        s->drops().buildVertices(s->dimension(), (float)t, v);
        sc.particles.insert(sc.particles.end(), v.begin(), v.end());
        Vec3 f = cam.forward(), right = cross(f, {0, 1, 0}).normalized(), up = cross(right, f).normalized();
        v.clear();
        s->particles().buildVertices(right, up, v);
        sc.particles.insert(sc.particles.end(), v.begin(), v.end());
        if (friendOn) {
            sc.extraModels.emplace_back(buildPlayerModel(friendPose), friendSkin);
            std::vector<Vertex> fe = buildPlayerExtras(friendPose);
            sc.particles.insert(sc.particles.end(), fe.begin(), fe.end());
        }
        return sc;
    }

    // A camera that eases towards where it should be.
    Player follow(const Vec3& want, const Vec3& target, float speed = 0.08f) {
        if (!camSet) {
            camPos = want;
            camSet = true;
        }
        camPos = mix(camPos, want, speed);
        return lookAt(camPos, target);
    }

    // Aims the player's view (first person, screen centre) at a block centre.
    void aim(const Vec3& block) {
        Player& p = s->player();
        p.yaw = yawTo(p.eye(), block);
        p.pitch = pitchTo(p.eye(), block);
    }

    // Holds the finger on the screen centre until the block is gone (or time runs out).
    template <class Shoot>
    void mine(int x, int y, int z, Shoot shoot) {
        World& w = s->currentWorld();
        aim(Vec3(x + 0.5f, y + 0.5f, z + 0.5f));
        s->touchDown(1, 300, 135, t);
        for (int i = 0; i < 72 && w.getBlock(x, y, z) != AIR; ++i) {
            aim(Vec3(x + 0.5f, y + 0.5f, z + 0.5f));
            step();
            shoot();
        }
        s->touchUp(1, 300, 135, t);
        for (int i = 0; i < 6; ++i) {
            step();
            shoot();
        }
    }
};

int findTree(World& w, int& tx, int& tz, int& base) {
    for (int r = 0; r < 80; ++r)
        for (int x = tx - r; x <= tx + r; ++x)
            for (int z = tz - r; z <= tz + r; ++z) {
                if (std::abs(x - tx) != r && std::abs(z - tz) != r) continue;
                int g = w.surfaceHeight(x, z);
                if (w.getBlock(x, g + 1, z) == LOG && w.getBlock(x, g + 2, z) == LOG && w.getBlock(x, g + 3, z) == LOG) {
                    // An open meadow towards +x for the build.
                    bool open = true;
                    for (int dx = 2; dx <= 10 && open; ++dx)
                        for (int dz = -3; dz <= 7 && open; ++dz) {
                            int gy = w.surfaceHeight(x + dx, z + dz);
                            open = std::abs(gy - g) <= 0 && w.getBlock(x + dx, gy + 1, z + dz) != LOG &&
                                   !isLeaves(w.getBlock(x + dx, gy + 2, z + dz)) && !isLeaves(w.getBlock(x + dx, gy + 3, z + dz));
                        }
                    if (open) {
                        tx = x;
                        tz = z;
                        base = g + 1;
                        return 1;
                    }
                }
            }
    return 0;
}

void storyScenes() {
    Story st;
    fs::path root = fs::temp_directory_path() / "vc_trailer_world";
    fs::remove_all(root);
    st.info.folder = createWorldFolder(root.string());
    st.info.seed = 4242;
    st.info.mode = GameMode::Survival;
    st.info.name = "Trailer";
    saveGenerator(st.info.folder, GenConfig::defaults());
    st.s = std::make_unique<Session>(st.info, PlayerState{});
    st.settings.renderDistance = 4;
    st.settings.cameraMode = 0;
    Session& s = *st.s;
    World& w = s.currentWorld();
    int tx = 30, tz = 30, base = 0;
    w.ensureChunksAround(2, 2, 6, 1000);
    if (!findTree(w, tx, tz, base)) std::printf("no tree\n");
    const int g = base - 1;
    std::printf("tree at %d %d %d\n", tx, base, tz);
    Player& p = s.player();
    p.pos = Vec3(tx + 7.5f, (float)base, tz + 0.5f);
    p.yaw = -kPi / 2;  // towards the tree (-x)
    p.pitch = 0;
    s.inventory().clear();
    s.inventory().slots[0] = {STEEL_AXE, 1};
    s.inventory().slots[1] = {PLANKS, 64};
    s.inventory().slots[2] = {STEEL_PICKAXE, 1};
    for (int i = 0; i < 4; ++i) s.armor()[i] = {armorItem(3, i), 1};
    s.selectedSlot() = 0;
    for (int i = 0; i < 30; ++i) st.step();

    // 1) Arrival: the camera sweeps down around the hero, who looks around.
    {
        Film f("01_arrival");
        for (int i = 0; i < 240; ++i) {
            float k = i / 240.0f;
            p.yaw = -kPi / 2 + 0.7f * std::sin(k * 6.0f);
            p.pitch = 0.15f * std::sin(k * 4.0f);
            st.step();
            float ang = 0.6f + k * 2.2f, rad = 16 - 10 * smooth(k), hgt = 12 - 9.5f * smooth(k);
            Vec3 c = p.pos + Vec3(std::cos(ang) * rad, hgt, std::sin(ang) * rad);
            Player cam = lookAt(c, p.pos + Vec3(0, 1.3f, 0));
            f.shot(st.scene(cam), cam);
        }
        st.camSet = false;
    }

    // 2) Walk to the tree and chop it.
    {
        Film f("02_chop");
        auto camera = [&] {
            Vec3 want = p.pos + Vec3(1.2f, 2.4f, 5.0f);
            return st.follow(want, (p.pos + Vec3(tx + 0.5f, (float)base + 1, tz + 0.5f)) * 0.5f + Vec3(0, 1.2f, 0));
        };
        p.yaw = -kPi / 2;
        p.pitch = 0;
        s.autopilot.moveZ = 1;
        for (int i = 0; i < 120 && p.pos.x > tx + 2.4f; ++i) {
            st.step();
            Player cam = camera();
            f.shot(st.scene(cam), cam);
        }
        s.autopilot.moveZ = 0;
        auto shoot = [&] {
            Player cam = camera();
            f.shot(st.scene(cam), cam);
        };
        st.mine(tx, base + 1, tz, shoot);
        st.mine(tx, base, tz, shoot);
        st.mine(tx, base + 2, tz, shoot);
        for (int i = 0; i < 24; ++i) {
            st.step();
            shoot();
        }
    }

    // 3) Build a little house of planks in the meadow.
    {
        Film f("03_build");
        s.selectedSlot() = 1;
        st.camSet = false;
        const int hx = tx + 6, hz = tz + 2;  // front wall at x = hx, from z = hz - 2 to hz + 2
        std::vector<std::array<int, 3>> blocks;
        for (int y = 0; y < 3; ++y) {
            for (int dz = -2; dz <= 2; ++dz)
                if (!(dz == 0 && y < 2)) blocks.push_back({hx, base + y, hz + dz});  // door gap
            for (int dx = 1; dx <= 3; ++dx) {
                blocks.push_back({hx + dx, base + y, hz - 2});
                blocks.push_back({hx + dx, base + y, hz + 2});
            }
        }
        for (int dx = 0; dx <= 3; ++dx)
            for (int dz = -2; dz <= 2; ++dz) blocks.push_back({hx + dx, base + 3, hz + dz});
        p.pos = Vec3(tx + 2.5f, (float)base, hz + 0.5f);
        for (int i = 0; i < 12; ++i) {
            p.yaw = angleMix(p.yaw, yawTo(p.pos, Vec3(hx + 1.5f, 0, hz + 0.5f)), 0.25f);
            st.step();
            Player cam = st.follow(p.pos + Vec3(-3.5f, 4.5f, -6.0f), Vec3(hx + 1.0f, base + 1.5f, hz + 0.5f));
            f.shot(st.scene(cam), cam);
        }
        int k = 0;
        for (auto& b : blocks) {
            Vec3 c(b[0] + 0.5f, b[1] + 0.5f, b[2] + 0.5f);
            for (int i = 0; i < 4; ++i) {
                p.yaw = angleMix(p.yaw, yawTo(p.eye(), c), 0.5f);
                p.pitch = pitchTo(p.eye(), c);
                if (i == 2) {
                    w.setBlock(b[0], b[1], b[2], PLANKS);
                    st.placeAnim = 0;
                }
                st.step();
                float orbit = k / (float)blocks.size();
                Vec3 want = Vec3(hx + 1.5f, (float)base, hz + 0.5f) +
                            Vec3(-std::cos(orbit * 1.2f + 2.3f) * 11, 6.5f, -std::sin(orbit * 1.2f + 2.3f) * 11);
                Player cam = st.follow(want, Vec3(hx + 1.0f, base + 1.5f, hz + 0.5f), 0.06f);
                f.shot(st.scene(cam), cam);
            }
            ++k;
        }
        for (int i = 0; i < 30; ++i) {
            st.step();
            Player cam = st.follow(Vec3(hx - 13.0f, base + 6.0f, hz + 9.0f), Vec3(hx + 1.5f, base + 1.5f, hz + 0.5f), 0.04f);
            f.shot(st.scene(cam), cam);
        }
    }

    // 4) Mine bronze, steel and ruby out of a rock.
    {
        Film f("04_mine");
        st.camSet = false;
        s.selectedSlot() = 2;
        const int rx = tx + 1, rz = tz - 6;
        for (int dx = 0; dx < 3; ++dx)
            for (int dy = 0; dy < 3; ++dy)
                for (int dz = 0; dz < 3; ++dz) w.setBlock(rx + dx, base + dy, rz + dz, dy == 2 && (dx + dz) % 2 ? COBBLESTONE : STONE);
        w.setBlock(rx, base, rz + 2, BRONZE_ORE);
        w.setBlock(rx + 1, base + 1, rz + 2, STEEL_ORE);
        w.setBlock(rx + 2, base, rz + 2, RUBY_ORE);
        p.pos = Vec3(rx + 1.5f, (float)base, rz + 6.5f);
        p.yaw = 0;
        auto camera = [&] { return st.follow(p.pos + Vec3(3.2f, 2.3f, 2.2f), Vec3(rx + 1.5f, base + 1.0f, rz + 2.0f), 0.1f); };
        auto shoot = [&] {
            Player cam = camera();
            f.shot(st.scene(cam), cam);
        };
        s.autopilot.moveZ = 1;
        for (int i = 0; i < 60 && p.pos.z > rz + 4.6f; ++i) {
            p.yaw = 0;
            st.step();
            shoot();
        }
        s.autopilot.moveZ = 0;
        st.mine(rx, base, rz + 2, shoot);
        st.mine(rx + 1, base + 1, rz + 2, shoot);
        st.mine(rx + 2, base, rz + 2, shoot);
        for (int i = 0; i < 30; ++i) {
            st.step();
            shoot();
        }
    }

    // 5) Friends: a second player walks up and both stand by the house.
    {
        Film f("05_friends");
        st.camSet = false;
        const int hx = tx + 6, hz = tz + 2;
        p.pos = Vec3(hx - 2.5f, (float)base, hz + 1.5f);
        p.yaw = kPi / 2 + 0.3f;
        st.friendOn = true;
        Vec3 fpos(hx - 12.5f, (float)base, hz - 4.5f);
        for (int i = 0; i < 168; ++i) {
            Vec3 goal(hx - 2.5f, (float)base, hz - 0.8f);
            Vec3 d = goal - fpos;
            float dist = std::sqrt(d.x * d.x + d.z * d.z);
            float speed = dist > 0.3f ? 4.3f / kFps : 0;
            if (dist > 0.3f) fpos = fpos + Vec3(d.x / dist * speed, 0, d.z / dist * speed);
            st.friendPose.feet = Vec3(fpos.x, (float)w.surfaceHeight((int)std::floor(fpos.x), (int)std::floor(fpos.z)) + 1, fpos.z);
            st.friendPose.yaw = dist > 0.3f ? yawTo(fpos, goal) : angleMix(st.friendPose.yaw, yawTo(fpos, p.pos), 0.2f);
            st.friendPose.swing = dist > 0.3f ? std::sin(i * 0.55f) * 0.8f : 0;
            st.friendPose.held = RUBY_PICKAXE;
            for (int a = 0; a < 4; ++a) st.friendPose.armor[a] = armorItem(4, a);
            st.friendPose.headPitch = 0;
            if (dist <= 0.3f) p.yaw = angleMix(p.yaw, yawTo(p.pos, fpos), 0.2f);
            st.step();
            Player cam = st.follow(Vec3(hx - 9.5f, base + 2.8f, hz + 6.5f), Vec3(hx - 4.0f, base + 1.3f, hz + 0.0f), 0.05f);
            f.shot(st.scene(cam), cam);
        }
        st.friendOn = false;
    }
}

// ---------------------------------------------------------------- fly-overs

// Finds a column of the given overworld biome near the origin.
bool findBiome(World& w, const std::string& id, int& x, int& z) {
    for (int r = 0; r < 4000; r += 48)
        for (int a = 0; a < 24; ++a) {
            int px = (int)(std::cos(a * kPi / 12) * r), pz = (int)(std::sin(a * kPi / 12) * r);
            const BiomeDef* b = w.biomeAt(px, pz);
            const BiomeDef* b2 = w.biomeAt(px + 40, pz);
            if (b && b2 && b->id == id && b2->id == id) {
                x = px;
                z = pz;
                return true;
            }
        }
    return false;
}

void flight(const std::string& name, World& w, Vec3 start, float heading, int frames, float height, float speed,
            float pitch = -0.3f) {
    Film f(name);
    MeshCache cache;
    float h = -1;
    Vec3 pos = start;
    for (int i = 0; i < frames; ++i) {
        float yaw = heading + 0.25f * std::sin(i / 60.0f);
        Player dir;
        dir.yaw = yaw;
        Vec3 d = dir.forward();
        pos = pos + Vec3(d.x, 0, d.z).normalized() * speed;
        float ground = 0;
        for (int dx = -4; dx <= 4; dx += 4)
            for (int dz = -4; dz <= 4; dz += 4) {
                int y = kChunkHeight - 2;
                while (y > 1 && !isSolid(w.getBlock((int)pos.x + dx, y, (int)pos.z + dz)) && w.getBlock((int)pos.x + dx, y, (int)pos.z + dz) != WATER &&
                       w.getBlock((int)pos.x + dx, y, (int)pos.z + dz) != LAVA)
                    --y;
                ground = std::max(ground, (float)y);
            }
        w.ensureChunksAround(floorDiv((int)pos.x, kChunkSize), floorDiv((int)pos.z, kChunkSize), 5, 1000);
        float want = ground + height;
        h = h < 0 ? want : h + (want - h) * 0.08f;
        Player cam;
        cam.pos = Vec3(pos.x, h, pos.z);
        cam.yaw = yaw;
        cam.pitch = pitch;
        f.shot(cache.scene(w, cam.pos, 4), cam);
    }
}

void flyovers() {
    World w(4242, Dimension::Overworld);
    const char* biomes[] = {"snowy_taiga", "mountains", "desert", "ice_plains"};
    gFogStart = 48;
    gFogEnd = 80;
    for (int i = 0; i < 4; ++i) {
        int x = 0, z = 0;
        if (!findBiome(w, biomes[i], x, z)) std::printf("no %s\n", biomes[i]);
        std::printf("%s at %d %d\n", biomes[i], x, z);
        w.ensureChunksAround(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize), 5, 1000);
        flight(std::string("06_") + std::to_string(i) + "_" + biomes[i], w, Vec3(x + 0.5f, 0, z + 0.5f), 1.2f + i * 0.9f, 96,
               i == 1 ? 18 : 12, 0.35f);
    }
}

// ---------------------------------------------------------------- the coming 1.1.0

void teaser() {
    GenConfig config = GenConfig::defaults();
    // The Nether fortress hall with the void portal frame, which then lights up.
    {
        World nether(4242, Dimension::Nether, config);
        int x, y, z;
        nether.nearestStructure(0, 0, x, y, z);
        nether.ensureChunksAround(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize), 5, 1000);
        std::printf("fortress at %d %d %d\n", x, y, z);
        const float fog[3] = {0.22f, 0.04f, 0.03f};
        std::copy(fog, fog + 3, gSky);
        gFogStart = 8;
        gFogEnd = 60;
        Film f("07_fortress");
        MeshCache cache;
        for (int i = 0; i < 144; ++i) {
            float k = i / 143.0f;
            if (i == 72)
                for (int dy = 0; dy < 6; ++dy)
                    if (tryLightPortal(nether, x, y + dy, z, true)) break;
            Vec3 eye(x + 0.5f + std::sin(k * 1.4f) * 3, y + 4.5f + 2 * (1 - k), z + 9.0f - 3.5f * smooth(k));
            Player cam = lookAt(eye, Vec3(x + 0.5f, y + 6.5f, z + 0.5f));
            f.shot(cache.scene(nether, eye, 4), cam);
        }
    }
    // The Crimson Void: the purple forest of eye trees, then the boss arena.
    {
        World voidWorld(4242, Dimension::Void, config);
        int fx = 0, fz = 0;
        for (int r = 0; r < 3000; r += 40) {
            bool found = false;
            for (int a = 0; a < 16 && !found; ++a) {
                int px = (int)(std::cos(a * kPi / 8) * r), pz = (int)(std::sin(a * kPi / 8) * r);
                if (voidWorld.voidBiomeAt(px, pz) == World::VoidBiome::Purple && voidWorld.voidBiomeAt(px + 60, pz) == World::VoidBiome::Purple) {
                    fx = px;
                    fz = pz;
                    found = true;
                }
            }
            if (found) break;
        }
        const float fog[3] = {0.16f, 0.05f, 0.24f};
        std::copy(fog, fog + 3, gSky);
        gFogStart = 16;
        gFogEnd = 64;
        voidWorld.ensureChunksAround(floorDiv(fx, kChunkSize), floorDiv(fz, kChunkSize), 5, 1000);
        flight("08_void_forest", voidWorld, Vec3(fx + 0.5f, 0, fz + 0.5f), 1.57f, 120, 7, 0.25f, -0.15f);
        int ax, ay, az;
        voidWorld.nearestStructure(fx, fz, ax, ay, az);
        voidWorld.ensureChunksAround(floorDiv(ax, kChunkSize), floorDiv(az, kChunkSize), 5, 1000);
        const float lavaFog[3] = {0.42f, 0.10f, 0.04f};
        std::copy(lavaFog, lavaFog + 3, gSky);
        gFogStart = 20;
        gFogEnd = 75;
        Film f("09_arena");
        MeshCache cache;
        for (int i = 0; i < 120; ++i) {
            float a = 0.8f + i / 120.0f * 1.6f;
            Vec3 eye(ax + std::cos(a) * 34, ay + 16 - i * 0.05f, az + std::sin(a) * 34);
            Player cam = lookAt(eye, Vec3(ax + 0.5f, ay + 2.0f, az + 0.5f));
            f.shot(cache.scene(voidWorld, Vec3(ax, ay, az), 4), cam);
        }
    }
    const float sky[3] = {0.53f, 0.74f, 1.0f};
    std::copy(sky, sky + 3, gSky);
}

}  // namespace

int main(int argc, char** argv) {
    gOut = argc > 1 ? argv[1] : "build/trailer";
    fs::create_directories(gOut);
    gAtlas = buildFallbackAtlas(gAtlasSize);
    gImageW = 960;
    gImageH = 540;
    gQuiet = true;
    std::string only = argc > 2 ? argv[2] : "";
    auto timed = [&](const char* name, const std::function<void()>& fn) {
        if (!only.empty() && only != name) return;
        auto t0 = std::chrono::steady_clock::now();
        fn();
        std::printf("%s: %.0f s\n", name, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        std::fflush(stdout);
    };
    timed("story", storyScenes);
    timed("flyovers", flyovers);
    timed("teaser", teaser);
    return 0;
}

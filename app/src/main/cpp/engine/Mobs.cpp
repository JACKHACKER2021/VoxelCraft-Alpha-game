#include "Mobs.h"
#include "VcModel.h"
#include "VcBuiltin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "Blocks.h"
#include "Items.h"
#include "Mods.h"
#include "World.h"

namespace vc {

namespace {

constexpr float kTile = kTileUv;
constexpr float kGravity = 28.0f;

const MobInfo kInfo[] = {
    {"Prase", 0.9f, 0.9f, 10, 1.6f, 0, false, false},
    {"Ovce", 0.9f, 1.3f, 8, 1.5f, 0, false, false},
    {"Dýňový sliz", 0.9f, 0.9f, 12, 3.6f, 2, true, false},
    {"Zapomenutý muž", 0.6f, 1.9f, 20, 2.6f, 3, true, false},
    {"Karmínový muž", 1.6f, 3.4f, 300, 2.3f, 9, true, true},
    {"Slepice", 0.5f, 0.8f, 4, 1.4f, 0, false, false},
    {"Kráva", 0.95f, 1.4f, 10, 1.4f, 0, false, false},
    {"Vesničan", 0.6f, 1.9f, 20, 1.3f, 0, false, false},
    {"Pavouk", 1.3f, 0.9f, 16, 3.2f, 2, true, false},
    {"Netopýr", 0.5f, 0.6f, 6, 2.6f, 0, false, false},
    {"TNT zombie", 0.6f, 1.9f, 20, 2.5f, 0, true, false},
    {"Dehtová příšera", 1.1f, 1.7f, 30, 1.9f, 5, true, false},
    {"Husking", 0.6f, 1.95f, 24, 2.2f, 4, true, false},
    {"Prasák", 0.7f, 1.95f, 22, 2.7f, 5, true, false},
    {"Celestling", 0.5f, 0.7f, 8, 1.8f, 0, false, false},
    {"Reach Stalker", 0.6f, 2.3f, 26, 3.0f, 5, true, false},
    {"Sky Drifter", 1.4f, 0.8f, 14, 2.2f, 0, false, false},
    {"Strážce Nebe", 1.1f, 3.9f, 260, 2.6f, 8, true, true},
    // 2.6.0
    {"Úhelný golem", 1.2f, 2.0f, 60, 1.2f, 7, false, false},   // peaceful until its ore is dug (then it hunts)
    {"Mechový plaz", 0.8f, 0.55f, 16, 3.6f, 3, true, false},   // lies in wait, springs at the player
    {"Sporovka", 0.6f, 0.95f, 8, 1.6f, 0, false, false},       // hops; a hit puffs a cloud of spores
    {"Žhavá můra", 0.7f, 0.5f, 4, 2.2f, 0, false, false},      // flies round, glows
    {"Popelavý lišák", 0.6f, 0.75f, 12, 2.6f, 0, false, false},  // the Cornerstone's fox, tamed with raw meat
    {"Lišák", 0.6f, 0.75f, 12, 2.6f, 0, false, false},          // the fox of the snowy and dappled forests
    {"Duch", 0.6f, 1.5f, 10, 1.6f, 0, false, false},             // floats at night, flees from light
    {"Sněhulák", 0.7f, 1.8f, 20, 0.0f, 0, false, false},         // stands guard, throws snowballs at monsters
};
static_assert(sizeof(kInfo) / sizeof(kInfo[0]) == (size_t)MobType::Count, "info for every mob");

bool solidAt(const World& w, float x, float y, float z) {
    ItemId b = w.getBlock((int)std::floor(x), (int)std::floor(y), (int)std::floor(z));
    // 2.6.0: a thin cover (fallen snow, leaf litter, a carpet) is walked through, not climbed onto like a full block
    if (kFeatures260 && (b == SNOW_LAYER || b == LEAF_LITTER || isCarpet(b))) return false;
    return isSolid(b) && b != CACTUS;
}

bool boxCollides(const World& w, const Vec3& p, float half, float height) {
    for (float x : {p.x - half, p.x + half - 0.001f})
        for (float z : {p.z - half, p.z + half - 0.001f})
            for (float y = p.y; y < p.y + height; y += std::min(1.0f, height)) {
                if (solidAt(w, x, y, z)) return true;
                if (y + 1 >= p.y + height && solidAt(w, x, p.y + height - 0.001f, z)) return true;
            }
    return false;
}

float yawTo(const Vec3& from, const Vec3& to) { return std::atan2(to.x - from.x, -(to.z - from.z)); }
float turnTo(float yaw, float want, float maxStep) {
    float d = std::remainder(want - yaw, 2 * kPi);
    return yaw + std::clamp(d, -maxStep, maxStep);
}

// A box in mob space (x right, y up, -z forward) turned by `yaw` around the feet,
// with an optional swing `pitch` around a pivot (legs and arms). Tiles per face: top,
// bottom, north (front), south, east, west.
struct Builder {
    std::vector<Vertex>& out;
    Vec3 feet;
    float yaw, scale, light;
    // A mod's mob: its skin replaces every tile, its face the original face tile.
    int skin = -1, face = -1, baseFace = -1;
    float fall = 0;  // dying: the whole body tips over sideways around the feet, in one piece

    Vec3 toWorld(Vec3 p) const {
        p = p * scale;
        if (fall != 0) p = Vec3(p.x * std::cos(fall) - p.y * std::sin(fall), p.x * std::sin(fall) + p.y * std::cos(fall) + std::sin(fall) * 0.3f * scale, p.z);
        float c = std::cos(yaw), s = std::sin(yaw);
        // yaw 0 looks to -z; positive yaw turns towards +x (like the player).
        return feet + Vec3(p.x * c - p.z * s, p.y, p.x * s + p.z * c);
    }
    void box(Vec3 lo, Vec3 hi, const int tiles[6], float pitch = 0, Vec3 pivot = {}, float roll = 0) const {
        Vec3 c[8];
        for (int i = 0; i < 8; ++i) {
            Vec3 p((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z);
            if (pitch != 0 || roll != 0) {
                Vec3 d = p - pivot;
                // Pitch around x (limb swing), then roll around z (falling over).
                float y1 = d.y * std::cos(pitch) - d.z * std::sin(pitch), z1 = d.y * std::sin(pitch) + d.z * std::cos(pitch);
                float x2 = d.x * std::cos(roll) - y1 * std::sin(roll), y2 = d.x * std::sin(roll) + y1 * std::cos(roll);
                p = pivot + Vec3(x2, y2, z1);
            }
            c[i] = toWorld(p);
        }
        // Faces as corner index quads (counter-clockwise from outside) and their shade.
        const int f[6][4] = {{2, 6, 7, 3}, {0, 1, 5, 4}, {1, 0, 2, 3}, {4, 5, 7, 6}, {5, 1, 3, 7}, {0, 4, 6, 2}};
        const float shade[6] = {1.0f, 0.5f, 0.8f, 0.8f, 0.6f, 0.6f};
        for (int k = 0; k < 6; ++k) {
            int t = tiles[k];
            if (skin >= 0) t = (t == baseFace && face >= 0) ? face : skin;
            float u0 = (t % kAtlasCols) * kTile, v0 = (t / kAtlasCols) * kTile, u1 = u0 + kTile, v1 = v0 + kTile;
            float l = light * shade[k];
            Vertex a{c[f[k][0]].x, c[f[k][0]].y, c[f[k][0]].z, u0, v1, l};
            Vertex b{c[f[k][1]].x, c[f[k][1]].y, c[f[k][1]].z, u1, v1, l};
            Vertex cc{c[f[k][2]].x, c[f[k][2]].y, c[f[k][2]].z, u1, v0, l};
            Vertex d{c[f[k][3]].x, c[f[k][3]].y, c[f[k][3]].z, u0, v0, l};
            out.insert(out.end(), {a, b, cc, a, cc, d});
        }
    }
};

void tilesAll(int t, int out[6]) {
    for (int i = 0; i < 6; ++i) out[i] = t;
}

}  // namespace

const MobInfo& mobInfo(MobType type) { return kInfo[std::min((int)type, (int)MobType::Count - 1)]; }

float MobSystem::random() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (rng_ % 100000) / 100000.0f;
}

void MobSystem::spawn(MobType type, int dim, const Vec3& feet) {
    Mob m;
    m.type = type;
    m.dim = dim;
    m.pos = feet;
    m.health = mobInfo(type).health;
    m.yaw = random() * 2 * kPi;
    m.wanderTimer = 1 + random() * 3;
    m.uid = nextUid_++;
    mobs_.push_back(m);
}

int MobSystem::findUid(uint32_t uid) const {
    for (size_t i = 0; i < mobs_.size(); ++i)
        if (mobs_[i].uid == uid) return (int)i;
    return -1;
}

void MobSystem::followRemote(float dt) {
    for (Mob& m : mobs_) {
        m.hurtTime = std::max(0.0f, m.hurtTime - dt);
        m.age += dt;
        const Vec3 d = m.target - m.pos;
        const float step = std::min(1.0f, dt * 8);
        m.pos = m.pos + d * step;
        const float speed = std::sqrt(d.x * d.x + d.z * d.z) * 8;
        m.phase += dt * speed * 3.2f;
        m.swing = std::sin(m.phase) * std::min(1.0f, speed / std::max(1.0f, mobInfo(m.type).speed)) * 0.8f;
        if (!m.alive()) m.deathTime += dt;
    }
}

void MobSystem::spawnVillager(int dim, const Vec3& feet, Profession profession, const Vec3& home) {
    spawn(MobType::Villager, dim, feet);
    mobs_.back().profession = profession;
    mobs_.back().home = home;
    mobs_.back().hasHome = true;
}

int MobSystem::countType(int dim, MobType type, const Vec3& near, float radius) const {
    int n = 0;
    for (const Mob& m : mobs_)
        if (m.dim == dim && m.type == type && (m.pos - near).length() < radius) ++n;
    return n;
}

void MobSystem::update(const World& world, int dim, const Vec3& target, bool huntable, float dt,
                       std::vector<MobEvent>& events) {
    for (size_t i = 0; i < mobs_.size(); ++i) {
        Mob& m = mobs_[i];
        if (m.dim != dim) continue;
        const MobInfo& info = mobInfo(m.type);
        m.age += dt;
        m.hurtTime = std::max(0.0f, m.hurtTime - dt);
        m.attackCooldown = std::max(0.0f, m.attackCooldown - dt);
        m.attackAnim = std::max(0.0f, m.attackAnim - dt * 2.5f);
        if (!m.alive()) {
            const float before = m.deathTime;
            m.deathTime += dt;
            // The boss does not just fall over: he freezes, twitches and roars, then blows up.
            if (info.boss) {
                if (before < 1.2f && m.deathTime >= 1.2f) events.push_back({MobEvent::BossRoar, m.type, m.pos, 0, {}, -1, 0});
                if (m.deathTime < kBossDeathTime) continue;
                events.push_back({MobEvent::Explode, m.type, m.pos + Vec3(0, 1.0f, 0), 0, {}, -1, 0});
            }
            if (m.deathTime > 0.9f) {
                events.push_back({MobEvent::Died, m.type, m.pos + Vec3(0, 0.5f, 0), 0, {}, m.killer, m.variant});
                mobs_.erase(mobs_.begin() + (long)i);
                --i;
            }
            continue;
        }
        // Waiting in unloaded chunks: freeze.
        if (!world.getChunk(floorDiv((int)std::floor(m.pos.x), kChunkSize), floorDiv((int)std::floor(m.pos.z), kChunkSize))) continue;

        // The tar monster goes for animals and villagers too, whichever is closest.
        Vec3 goal = target;
        int prey = -1;
        bool canHunt = huntable;
        if (m.type == MobType::TarMonster) {
            float best = huntable ? (target - m.pos).length() : 1e9f;
            for (size_t j = 0; j < mobs_.size(); ++j) {
                const Mob& o = mobs_[j];
                if (j == i || o.dim != dim || !o.alive() || mobInfo(o.type).hostile || o.type == MobType::Bat) continue;
                float d = (o.pos - m.pos).length();
                if (d < 14 && d < best) best = d, prey = (int)j, goal = o.pos;
            }
            canHunt = huntable || prey >= 0;
        }
        // 2.6.0: the snowman throws snowballs at the nearest monster.
        if (m.type == MobType::Snowman) {
            int best = -1;
            float bd = 11;
            for (size_t j = 0; j < mobs_.size(); ++j) {
                const Mob& o = mobs_[j];
                if (j == i || o.dim != dim || !o.alive() || !mobInfo(o.type).hostile) continue;
                const float d = (o.pos - m.pos).length();
                if (d < bd) bd = d, best = (int)j;
            }
            if (best >= 0) {
                m.yaw = turnTo(m.yaw, yawTo(m.pos, mobs_[(size_t)best].pos), dt * 6);
                if (m.attackCooldown <= 0) {
                    m.attackCooldown = 1.4f;
                    m.attackAnim = 1;
                    events.push_back({MobEvent::Snowball, m.type, mobs_[(size_t)best].pos + Vec3(0, 0.8f, 0), 0, {}});
                    hurt(best, 3, m.pos);
                }
            }
            m.vel.x = m.vel.z = 0;
        }
        // 2.6.0: a tamed fox goes for monsters near its player.
        const bool fox = m.type == MobType::AshFox || m.type == MobType::Fox;
        if (fox && m.tamed && !m.sitting) {
            float best = 9;
            for (size_t j = 0; j < mobs_.size(); ++j) {
                const Mob& o = mobs_[j];
                if (j == i || o.dim != dim || !o.alive() || !mobInfo(o.type).hostile || mobInfo(o.type).boss) continue;
                const float d = (o.pos - target).length();
                if (d < best) best = d, prey = (int)j;
            }
            if (prey >= 0) goal = mobs_[(size_t)prey].pos;
        }
        Vec3 toPlayer = goal - m.pos;
        const float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float want = 0;  // forward speed this frame
        bool hunting = info.hostile && canHunt && dist < (info.boss ? 40.0f : 22.0f) && std::fabs(toPlayer.y) < 12;
        // 2.6.0: the golem hunts only while angry; the moss lurker waits until the player comes close.
        if (m.angry > 0) m.angry -= dt;
        if (m.type == MobType::Villager && m.calm > 0) m.calm -= dt;  // 2.6.0: sweets again the next night
        if (m.type == MobType::CornerstoneGolem) hunting = m.angry > 0 && canHunt && dist < 24 && std::fabs(toPlayer.y) < 8;
        if (m.type == MobType::MossLurker) {
            if (hunting && dist < 5) m.angry = 8;
            if (m.angry <= 0) hunting = false;
        }
        // 2.6.0: resting: now and then a calm animal sits down for a while (a tamed fox when told to).
        if (m.sitTimer > 0 && (m.sitTimer -= dt) <= 0 && !(fox && m.tamed)) m.sitting = false;
        if (hunting || m.panic > 0) m.sitting = false, m.sitTimer = 0;
        m.sit = std::clamp(m.sit + (m.sitting ? dt : -dt) * 3.0f, 0.0f, 1.0f);
        if (m.type == MobType::Hogman) {
            // 1.5.7: a gift of powerstone keeps it peaceful for a while; it looks the stone over and pays.
            if (m.calm > 0) m.calm -= dt, hunting = false;
            if (m.barter > 0 && (m.barter -= dt) <= 0) events.push_back({MobEvent::Barter, m.type, m.pos + Vec3(0, 1.0f, 0), 0, {}});
            if (m.barter > 0) {
                m.yaw = turnTo(m.yaw, yawTo(m.pos, goal), dt * 4);
                want = 0;
                m.vel.x *= 0.5f, m.vel.z *= 0.5f;
            }
        }
        if (hunting && m.type == MobType::TntZombie) {
            // Walks up to the player, stops and hisses; after a moment it blows up.
            m.yaw = turnTo(m.yaw, yawTo(m.pos, goal), dt * 6);
            if (dist < 2.4f && std::fabs(toPlayer.y) < 2.5f) {
                m.fuse += dt;
                if (m.fuse >= 1.6f) {
                    events.push_back({MobEvent::Explode, m.type, m.pos + Vec3(0, 0.8f, 0), 0, {}});
                    mobs_.erase(mobs_.begin() + (long)i);
                    --i;
                    continue;
                }
            } else {
                m.fuse = std::max(0.0f, m.fuse - dt);
                want = info.speed;
            }
        } else if (hunting && m.type == MobType::Husking) {
            // Keeps out of reach: closes in from afar, backs off when the player comes near,
            // and in between stands still and spits a fireball every few seconds.
            m.yaw = turnTo(m.yaw, yawTo(m.pos, goal), dt * 6);
            want = dist > 13 ? info.speed : dist < 6 ? -info.speed * 0.8f : 0;
            if (dist < 18 && std::fabs(toPlayer.y) < 8 && m.attackCooldown <= 0) {
                m.attackCooldown = 2.4f + random() * 0.8f;
                m.attackAnim = 1;
                const Vec3 fwd(std::sin(m.yaw), 0, -std::cos(m.yaw));
                const Vec3 mouth = m.pos + Vec3(0, 1.6f, 0) + fwd * 0.4f;
                const Vec3 aim = (goal + Vec3(0, 1.2f, 0) - mouth).normalized();
                events.push_back({MobEvent::Fireball, m.type, mouth, info.damage, aim * 12.0f});
            }
        } else if (hunting && m.type == MobType::ReachStalker && dist > 5 && m.attackCooldown <= 0) {
            // 1.9.5: the Reach Stalker blinks: it vanishes and appears a few blocks behind the player.
            m.attackCooldown = 3.5f + random() * 2;
            const float a = random() * 2 * kPi;
            const Vec3 to = goal + Vec3(std::cos(a) * 3, 0.5f, std::sin(a) * 3);
            if (!boxCollides(world, to, info.width / 2, info.height)) {
                events.push_back({MobEvent::Teleport, m.type, m.pos, 0, {}});
                m.pos = to;
                m.vel = {};
                events.push_back({MobEvent::Teleport, m.type, m.pos, 0, {}});
            }
        } else if (hunting) {
            m.yaw = turnTo(m.yaw, yawTo(m.pos, goal), dt * 6);
            want = dist > (info.boss ? 1.6f : 0.9f) ? info.speed : 0;
            // Melee: close enough and facing the player.
            float reach = info.boss ? 2.8f : m.type == MobType::PumpkinSlime ? 1.1f : 1.5f;
            if (prey >= 0 && dist < 1.6f && std::fabs(toPlayer.y) < 1.8f && m.attackCooldown <= 0) {
                // Tar swallows the animal; where it dies, the tar spreads and another monster rises.
                m.attackCooldown = 1.0f;
                m.attackAnim = 1;
                const Vec3 at = mobs_[(size_t)prey].pos;
                if (hurt(prey, info.damage, m.pos)) events.push_back({MobEvent::TarSpread, m.type, at, 0, {}});
            } else if (prey < 0 && dist < reach && std::fabs(toPlayer.y) < (info.boss ? 3.0f : 1.8f) && m.attackCooldown <= 0) {
                // 1.6.0: the Crimson Man's blow throws the player far back.
                Vec3 knock = Vec3(toPlayer.x, 0, toPlayer.z).normalized() * (info.boss ? 15.0f : 4.0f) + Vec3(0, info.boss ? 9.0f : 3.5f, 0);
                events.push_back({MobEvent::HitPlayer, m.type, m.pos, info.damage, knock});
                m.attackCooldown = info.boss ? 1.6f : 1.0f;
                m.attackAnim = 1;
            }
            // The boss leaps at the player now and then.
            if (info.boss && m.onGround && dist > 6 && std::fmod(m.age, 7.0f) < dt) {
                m.vel = Vec3(toPlayer.x, 0, toPlayer.z).normalized() * 9.0f + Vec3(0, 11, 0);
                events.push_back({MobEvent::BossRoar, m.type, m.pos, 0, {}});
            }
            // 1.6.0: a roaring wave that knocks everything around him away, and a call that
            // brings forgotten men to fight for him.
            if (info.boss && dist < 14 && std::fmod(m.age + 4.0f, 9.0f) < dt) {
                m.attackAnim = 1;
                events.push_back({MobEvent::BossWave, m.type, m.pos, 4.0f, {}});
            }
            if (info.boss && std::fmod(m.age + 1.0f, 16.0f) < dt) events.push_back({MobEvent::BossSummon, m.type, m.pos, 0, {}});
        } else if (fox && m.tamed && prey >= 0) {
            // 2.6.0: the tamed fox runs at the monster and bites.
            m.sitting = false;
            m.yaw = turnTo(m.yaw, yawTo(m.pos, goal), dt * 8);
            want = dist > 1.0f ? info.speed * 1.3f : 0;
            if (dist < 1.4f && m.attackCooldown <= 0) {
                m.attackCooldown = 0.8f;
                m.attackAnim = 1;
                hurt(prey, 3, m.pos);
            }
        } else if (fox && m.tamed && !m.sitting) {
            // Follows its player; far behind, it catches up at once.
            const Vec3 d = target - m.pos;
            const float far = std::sqrt(d.x * d.x + d.z * d.z);
            if (far > 24) m.pos = target + Vec3(1.2f, 0.2f, 1.2f), m.vel = {};
            else if (far > 3.5f) {
                m.yaw = turnTo(m.yaw, yawTo(m.pos, target), dt * 6);
                want = info.speed * (far > 8 ? 1.3f : 0.9f);
            }
        } else if (m.sitting) {
            want = 0;
        } else if (m.panic > 0) {
            m.panic -= dt;
            m.yaw = turnTo(m.yaw, yawTo(target, m.pos), dt * 8);
            want = info.speed * 1.8f;
        } else {
            if ((m.wanderTimer -= dt) <= 0) {
                m.wandering = random() < 0.6f;
                // 2.6.0: the new creatures sit down to rest now and then.
                if (!m.wandering && m.type >= MobType::CornerstoneGolem && m.type <= MobType::Fox && m.type != MobType::EmberMoth && random() < 0.5f)
                    m.sitting = true, m.sitTimer = 4 + random() * 6;
                m.wanderTimer = 3 + random() * 5;
                float a = random() * 2 * kPi, r = 3 + random() * 6;
                m.target = m.pos + Vec3(std::cos(a) * r, 0, std::sin(a) * r);
                // Villagers keep to their village.
                if (m.hasHome && (m.target - m.home).length() > 12) m.target = m.home + Vec3(std::cos(a) * 4, 0, std::sin(a) * 4);
            }
            if (m.wandering) {
                Vec3 d = m.target - m.pos;
                if (d.x * d.x + d.z * d.z < 0.5f) m.wandering = false;
                m.yaw = turnTo(m.yaw, yawTo(m.pos, m.target), dt * 3);
                want = info.speed * 0.6f;
            }
        }

        // Slimes hop instead of walking; bats flutter about.
        Vec3 fwd(std::sin(m.yaw), 0, -std::cos(m.yaw));
        if (m.type == MobType::SkyDrifter) {
            // 1.9.5: glides in wide, slow circles over the islands, wings spread.
            if (!m.wandering || (m.target - m.pos).length() < 1.5f) {
                float a = random() * 2 * kPi;
                m.target = m.pos + Vec3(std::cos(a) * 12, random() * 4 - 2, std::sin(a) * 12);
                m.wandering = true;
            }
            Vec3 d = (m.target - m.pos).normalized() * info.speed;
            m.vel = m.vel + (d - m.vel) * std::min(1.0f, 0.8f * dt);
            m.yaw = turnTo(m.yaw, yawTo(m.pos, m.target), dt * 1.2f);
        } else if (m.type == MobType::Celestling || m.type == MobType::Sporeling) {
            // Light as a feather: it bobs along in long, floaty hops.
            float accel = m.onGround ? 10.0f : 1.5f;
            m.vel.x += (fwd.x * want - m.vel.x) * std::min(1.0f, accel * dt);
            m.vel.z += (fwd.z * want - m.vel.z) * std::min(1.0f, accel * dt);
            if (m.onGround && want > 0 && std::fmod(m.age, 1.3f) < dt) m.vel.y = 4.0f;
        } else if (m.type == MobType::Bat || m.type == MobType::EmberMoth || m.type == MobType::Ghost) {
            if (m.type == MobType::Ghost && m.panic > 0) {
                // 2.6.0: light scares the ghost: it flies away from the player.
                const Vec3 away = Vec3(m.pos.x - target.x, 0.3f, m.pos.z - target.z).normalized();
                m.target = m.pos + away * 6.0f;
                m.wandering = true;
            }
            if (!m.wandering || (m.target - m.pos).length() < 0.6f) {
                float a = random() * 2 * kPi;
                m.target = m.pos + Vec3(std::cos(a) * 4, random() * 3 - 1.4f, std::sin(a) * 4);
                m.wandering = true;
            }
            Vec3 d = (m.target - m.pos).normalized() * info.speed;
            m.vel = m.vel + (d - m.vel) * std::min(1.0f, 3 * dt);
            m.vel.y += std::sin(m.age * 9) * 4 * dt;  // the wing beats
            m.yaw = turnTo(m.yaw, yawTo(m.pos, m.target), dt * 5);
        } else if (m.type == MobType::PumpkinSlime) {
            if (m.onGround) {
                m.vel.x *= 0.5f;
                m.vel.z *= 0.5f;
                if (want > 0 && std::fmod(m.age, hunting ? 0.9f : 1.6f) < dt) {
                    m.vel = m.vel + fwd * (hunting ? 4.5f : 2.5f) + Vec3(0, 7.5f, 0);
                }
            }
        } else {
            float accel = m.onGround ? 12.0f : 2.0f;
            m.vel.x += (fwd.x * want - m.vel.x) * std::min(1.0f, accel * dt);
            m.vel.z += (fwd.z * want - m.vel.z) * std::min(1.0f, accel * dt);
        }

        // Water carries mobs up; gravity everywhere else.
        ItemId inside = world.getBlock((int)std::floor(m.pos.x), (int)std::floor(m.pos.y + 0.3f), (int)std::floor(m.pos.z));
        if (m.type == MobType::Bat || m.type == MobType::SkyDrifter || m.type == MobType::EmberMoth || m.type == MobType::Ghost) {}  // flies
        else if (m.dim == 3) m.vel.y -= kGravity * 0.45f * dt;  // 1.9.5: low gravity in the Celestial Reach
        else if (inside == WATER) m.vel.y = std::min(m.vel.y + 30 * dt, 2.5f);
        else if (inside == TAR || (kFeatures260 && inside == LAVA)) m.vel.y = std::max(m.vel.y - 6 * dt, -1.0f);  // sinks slowly
        else m.vel.y -= kGravity * dt;
        // The tar monster heals in tar.
        if (m.type == MobType::TarMonster && inside == TAR) m.health = std::min(info.health, m.health + dt * 2);
        m.vel.y = std::max(m.vel.y, m.type == MobType::Chicken ? -3.0f : -40.0f);  // chickens flutter down

        const float half = info.width / 2;
        // Move axis by axis; walking into a step makes walkers jump.
        bool blocked = false;
        Vec3 p = m.pos;
        Vec3 nx = p + Vec3(m.vel.x * dt, 0, 0);
        if (!boxCollides(world, nx, half, info.height)) p = nx;
        else blocked = true, m.vel.x = 0;
        Vec3 nz = p + Vec3(0, 0, m.vel.z * dt);
        if (!boxCollides(world, nz, half, info.height)) p = nz;
        else blocked = true, m.vel.z = 0;
        Vec3 ny = p + Vec3(0, m.vel.y * dt, 0);
        m.onGround = false;
        if (!boxCollides(world, ny, half, info.height)) {
            p = ny;
        } else {
            if (m.vel.y < 0) {
                m.onGround = true;
                p.y = std::floor(p.y + 0.001f);
                if (boxCollides(world, p, half, info.height)) p.y = m.pos.y;
            }
            m.vel.y = 0;
        }
        if (blocked && (m.type == MobType::Bat || m.type == MobType::SkyDrifter || m.type == MobType::EmberMoth || m.type == MobType::Ghost)) m.wandering = false;  // turn around at walls
        else if (blocked && want > 0 && m.type == MobType::Spider) m.vel.y = 3.5f;  // spiders climb walls
        else if (blocked && m.onGround && want > 0 && m.type != MobType::PumpkinSlime) m.vel.y = info.boss ? 10.5f : 8.2f;
        if (p.y < -30) p.y = -30;
        m.pos = p;

        // Limbs swing with the walking speed.
        float speed = std::sqrt(m.vel.x * m.vel.x + m.vel.z * m.vel.z);
        m.phase += dt * speed * (info.boss ? 1.6f : 3.2f);
        m.swing = std::sin(m.phase) * std::min(1.0f, speed / std::max(1.0f, info.speed)) * 0.8f;
        // Lava and the void hurt everyone but the boss.
        if (kFeatures260 && inside == LAVA && !info.boss) {
            // 2.6.0: lava burns without the knock-back of a hit (that bounced creatures up and kept them on top);
            // the creatures of fire do not mind it, a snowman melts quickly.
            const bool fireproof = m.type == MobType::EmberMoth || m.type == MobType::AshFox || m.type == MobType::Hogman;
            if (!fireproof) {
                m.health -= dt * (m.type == MobType::Snowman ? 12.0f : 4.0f);
                m.hurtTime = std::max(m.hurtTime, 0.2f);
                if (!info.hostile) m.panic = 4;
                m.sitting = false;
                m.vel.x *= std::max(0.0f, 1 - 3 * dt), m.vel.z *= std::max(0.0f, 1 - 3 * dt);  // thick: slow going
                if (m.health <= 0 && m.deathTime <= 0) m.deathTime = 0.001f;
            }
        }
        else if (!info.boss && (inside == LAVA || m.pos.y < -20)) hurt((int)i, dt * 8, m.pos);
    }
}

int MobSystem::pick(int dim, const Vec3& origin, const Vec3& dir, float maxDist, float* distance) const {
    int best = -1;
    float bestT = maxDist;
    for (size_t i = 0; i < mobs_.size(); ++i) {
        const Mob& m = mobs_[i];
        if (m.dim != dim || !m.alive()) continue;
        const MobInfo& info = mobInfo(m.type);
        const float h = info.width / 2 + 0.1f;
        Vec3 lo = m.pos - Vec3(h, 0, h), hi = m.pos + Vec3(h, info.height, h);
        // Slab test of the ray against the box.
        float t0 = 0, t1 = bestT;
        bool hit = true;
        for (int a = 0; a < 3 && hit; ++a) {
            float o = a == 0 ? origin.x : a == 1 ? origin.y : origin.z, d = a == 0 ? dir.x : a == 1 ? dir.y : dir.z;
            float l = a == 0 ? lo.x : a == 1 ? lo.y : lo.z, u = a == 0 ? hi.x : a == 1 ? hi.y : hi.z;
            if (std::fabs(d) < 1e-6f) {
                if (o < l || o > u) hit = false;
                continue;
            }
            float ta = (l - o) / d, tb = (u - o) / d;
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) hit = false;
        }
        if (hit && t0 < bestT) {
            bestT = t0;
            best = (int)i;
        }
    }
    if (distance) *distance = bestT;
    return best;
}

bool MobSystem::hurt(int index, float damage, const Vec3& from, int killer) {
    if (index < 0 || index >= (int)mobs_.size()) return false;
    Mob& m = mobs_[index];
    if (!m.alive()) return false;
    m.killer = killer;
    const MobInfo& info = mobInfo(m.type);
    m.health -= damage;
    m.hurtTime = 0.35f;
    if (!info.boss) {
        Vec3 away = Vec3(m.pos.x - from.x, 0, m.pos.z - from.z).normalized();
        m.vel = m.vel + away * 6.0f + Vec3(0, 4.5f, 0);
    }
    if (!info.hostile) m.panic = 4;  // animals run away
    if (m.type == MobType::Hogman) m.calm = 0, m.barter = 0;  // a hit makes it angry again
    // 2.6.0: a hit golem or lurker fights back; a tamed fox only steps aside.
    if (m.type == MobType::CornerstoneGolem || m.type == MobType::MossLurker) m.angry = 20, m.panic = 0;
    if (m.tamed) m.panic = 0;
    m.sitting = false;
    if (m.health <= 0) {
        m.deathTime = 0.001f;
        return true;
    }
    return false;
}

void MobSystem::angerGolems(int dim, const Vec3& at, float radius) {
    for (Mob& m : mobs_)
        if (m.dim == dim && m.type == MobType::CornerstoneGolem && m.alive() && (m.pos - at).length() < radius) m.angry = 25;
}

int MobSystem::count(int dim, bool hostile, const Vec3& near, float radius) const {
    int n = 0;
    for (const Mob& m : mobs_)
        if (m.dim == dim && mobInfo(m.type).hostile == hostile && (m.pos - near).length() < radius) ++n;
    return n;
}

bool MobSystem::hostileNear(int dim, const Vec3& pos, float radius) const {
    for (const Mob& m : mobs_)
        if (m.dim == dim && m.alive() && mobInfo(m.type).hostile && (m.pos - pos).length() < radius) return true;
    return false;
}

const Mob* MobSystem::boss(int dim) const {
    for (const Mob& m : mobs_)
        if (m.dim == dim && mobInfo(m.type).boss) return &m;
    return nullptr;
}

void MobSystem::despawn(int dim, const Vec3& player, float radius, bool daylightHostiles) {
    mobs_.erase(std::remove_if(mobs_.begin(), mobs_.end(),
                               [&](const Mob& m) {
                                   const MobInfo& info = mobInfo(m.type);
                                   if (m.dim != dim || !info.hostile || info.boss) return false;
                                   float d = (m.pos - player).length();
                                   return d > radius || (daylightHostiles && !m.cave && m.type != MobType::TarMonster && m.type != MobType::Husking && d > 16);
                               }),
                mobs_.end());
}

void MobSystem::clearHostiles() {
    mobs_.erase(std::remove_if(mobs_.begin(), mobs_.end(), [](const Mob& m) { return mobInfo(m.type).hostile; }), mobs_.end());
}

void MobSystem::buildVertices(int dim, float time, std::vector<Vertex>& out) const {
    out.clear();
    for (const Mob& m : mobs_) {
        if (m.dim != dim) continue;
        float light = m.hurtTime > 0 ? 1.7f : 1.0f;
        if (m.fuse > 0 && std::fmod(m.fuse * (4 + m.fuse * 6), 1.0f) > 0.5f) light = 2.4f;  // flashes faster and faster
        float roll = m.alive() ? 0 : std::min(1.0f, m.deathTime / 0.5f) * kPi / 2;
        Vec3 feet = m.pos;
        float yaw = m.yaw;
        if (!m.alive() && mobInfo(m.type).boss) {
            // The boss's death: standing, shaking harder and harder, turning in jerks, flashing.
            const float k = std::min(1.0f, m.deathTime / kBossDeathTime);
            roll = 0;
            feet = feet + Vec3(std::sin(time * 71.0f) * 0.25f * k, std::fabs(std::sin(time * 23.0f)) * 0.15f * k, std::cos(time * 59.0f) * 0.25f * k);
            yaw += std::sin(time * 9.0f + m.deathTime * 13.0f) * 1.2f * k;
            light = std::fmod(time * (3 + k * 14), 1.0f) > 0.5f ? 2.6f : 1.0f;
        }
        Builder b{out, feet, yaw, 1.0f, light};
        // Every part turns with the body (before, each limb rolled round its own joint and the
        // body came apart); the limbs keep their places and the mob just topples over.
        b.fall = roll;
        roll = 0;
        if (!m.alive() && !mobInfo(m.type).boss) b.light = std::max(b.light, 1.4f);  // a red-white flash as it dies
        {
            // 2.8.0 preview (VC language): every creature's body is a GLB model from models/mobs/ (see VcBuiltin.h).
            // A few still have the old body: mod reskins, the Celestial Reach's pig and sheep, a fox in its collar,
            // and the bosses and the slime whose moves (slams, squashing) the VC models cannot show yet.
            const bool keepOld = ModRegistry::get().mob(m.variant) || (m.dim == 3 && (m.type == MobType::Pig || m.type == MobType::Sheep)) ||
                                 (m.type == MobType::Fox && m.tamed && m.collar >= 0) || m.type == MobType::CornerstoneGolem ||
                                 m.type == MobType::SkyGuardian || m.type == MobType::PumpkinSlime;
            if (const VcModel* model = keepOld ? nullptr : builtinMobModel(m.type, m.sit > 0.5f)) {
                VcPose pose;
                pose.walk = m.sit > 0.5f ? 0 : m.swing;
                pose.fall = b.fall;
                if (model->flapSpeed > 0)
                    pose.flap = std::sin(time * model->flapSpeed + m.age * 3) * (m.onGround ? model->flapRest : model->flapAmp);
                if (m.type == MobType::Villager) {
                    // The robe of the villager's trade.
                    const int robe = m.profession == Profession::Smith ? T_VILLAGER_SMITH : m.profession == Profession::Farmer ? T_VILLAGER_FARMER : T_VILLAGER_ROBE;
                    for (int r : {T_VILLAGER_SMITH, T_VILLAGER_FARMER, T_VILLAGER_ROBE})
                        if (r != robe) pose.swaps.emplace_back(r, robe);
                }
                if (m.type == MobType::ForgottenMan && zombieSkin) pose.swaps.emplace_back(T_FORGOTTEN_FACE, T_ZOMBIE_FACE);
                Vec3 at = feet;
                float l = light;
                if (m.type == MobType::Ghost) {
                    at.y += std::sin(time * 2.0f + m.age) * 1.2f / 16.0f;  // it bobs in the air
                    l = std::max(l, 3.2f);                                  // and glows pale in the dark
                }
                buildVcModel(*model, pose, VcTexRect{}, at, yaw, l, out);
                continue;
            }
        }
        if (const ModMob* mod = ModRegistry::get().mob(m.variant); mod && mod->model >= 0) {
            // 2.8.0: a creature with its own model (VcModel, from a geo.json or .vcmodel.json): the legs
            // swing as it walks (bones named leg..., the left and right ones in turn), arms the other way.
            if (const VcModel* model = ModRegistry::get().model(mod->model)) {
                VcPose pose;
                const float swing = m.swing * 50.0f;  // degrees
                for (const VcBone& bone : model->bones) {
                    std::string n = bone.name;
                    for (char& ch : n) ch = (char)std::tolower((unsigned char)ch);
                    if (n.find("leg") == std::string::npos && n.find("arm") == std::string::npos) continue;
                    const bool odd = n.find("right") != std::string::npos || n.find("1") != std::string::npos || n.find("2") != std::string::npos;
                    const float sign = (odd ? 1.0f : -1.0f) * (n.find("arm") != std::string::npos ? -1.0f : 1.0f);
                    pose.rotation[bone.name] = Vec3(swing * sign, 0, 0);
                }
                buildVcModel(*model, pose, mod->modelTex, feet, yaw, light, out);
                continue;
            }
        }
        if (const ModMob* mod = ModRegistry::get().mob(m.variant); mod && mod->skin >= 0) {
            b.skin = mod->skin;
            b.face = mod->face;
            switch (m.type) {
                case MobType::Pig: b.baseFace = T_PIG_FACE; break;
                case MobType::Sheep: b.baseFace = T_SHEEP_FACE; break;
                case MobType::Cow: b.baseFace = T_COW_FACE; break;
                case MobType::Chicken: b.baseFace = T_CHICKEN_FACE; break;
                case MobType::Spider: b.baseFace = T_SPIDER_FACE; break;
                case MobType::Villager: b.baseFace = T_VILLAGER_FACE; break;
                case MobType::PumpkinSlime: b.baseFace = T_PUMPKIN_FACE; break;
                default: b.baseFace = zombieSkin ? T_ZOMBIE_FACE : T_FORGOTTEN_FACE; break;
            }
        }
        int t[6];
        auto faceTiles = [&](int all, int front) {
            tilesAll(all, t);
            t[2] = front;
        };
        const Vec3 rollPivot(0, 0, 0);
        switch (m.type) {
            case MobType::Pig:
            case MobType::Sheep: {
                bool sheep = m.type == MobType::Sheep;
                // 1.9.0 build 3: in the Celestial Reach they are celestial: sky white with gold flecks.
                const bool sky = m.dim == 3 && !ModRegistry::get().mob(m.variant);
                const int woolT = sky ? T_CELESTIAL_WOOL : T_SHEEP_WOOL, sheepSkin = sky ? T_CELESTIAL_SHEEP_SKIN : T_SHEEP_SKIN,
                          pigSkin = sky ? T_CELESTIAL_PIG_SKIN : T_PIG_SKIN;
                int skin = sheep ? woolT : pigSkin, face = sheep ? (sky ? T_CELESTIAL_SHEEP_FACE : T_SHEEP_FACE) : (sky ? T_CELESTIAL_PIG_FACE : T_PIG_FACE);
                int legT[6];
                tilesAll(sheep ? sheepSkin : pigSkin, legT);
                float legH = sheep ? 0.45f : 0.35f;
                for (int k = 0; k < 4; ++k) {
                    float lx = (k % 2 ? 0.17f : -0.17f), lz = (k < 2 ? -0.3f : 0.3f);
                    float sw = (k == 0 || k == 3 ? m.swing : -m.swing);
                    b.box({lx - 0.1f, 0, lz - 0.1f}, {lx + 0.1f, legH, lz + 0.1f}, legT, sw, {lx, legH, lz}, roll);
                }
                tilesAll(skin, t);
                float w = sheep ? 0.32f : 0.28f, top = legH + (sheep ? 0.6f : 0.45f);
                b.box({-w, legH - 0.05f, -0.45f}, {w, top, 0.45f}, t, 0, {}, roll);
                // The head: the face in front, wool on top (sheep), skin elsewhere.
                faceTiles(sheep ? sheepSkin : pigSkin, face);
                if (sheep) t[0] = woolT;
                float hy = top - 0.12f;
                b.box({-0.22f, hy - 0.42f, -0.78f}, {0.22f, hy, -0.4f}, t, 0, {}, roll);
                break;
            }
            case MobType::PumpkinSlime: {
                // Squashes when landing and stretches in the air.
                float stretch = std::clamp(1.0f + m.vel.y * 0.04f, 0.75f, 1.25f);
                float h = 0.9f * stretch, w = 0.45f / std::sqrt(stretch);
                t[0] = t[1] = T_PUMPKIN_TOP;
                t[2] = T_PUMPKIN_FACE;
                t[3] = t[4] = t[5] = T_PUMPKIN_SIDE;
                b.box({-w, 0, -w}, {w, h, w}, t, 0, {}, roll);
                break;
            }
            case MobType::ForgottenMan: {
                int legT[6], skinT[6];
                tilesAll(T_FORGOTTEN_SHIRT, legT);
                tilesAll(T_FORGOTTEN_SKIN, skinT);
                b.box({-0.24f, 0, -0.12f}, {-0.01f, 0.75f, 0.12f}, legT, m.swing, {-0.12f, 0.75f, 0}, roll);
                b.box({0.01f, 0, -0.12f}, {0.24f, 0.75f, 0.12f}, legT, -m.swing, {0.12f, 0.75f, 0}, roll);
                tilesAll(T_FORGOTTEN_SHIRT, t);
                b.box({-0.25f, 0.75f, -0.13f}, {0.25f, 1.45f, 0.13f}, t, 0, {}, roll);
                // Arms reach forward like a sleepwalker.
                float reach = kPi / 2 + 0.15f * std::sin(time * 2 + m.age) + m.attackAnim * 0.6f;
                b.box({-0.38f, 0.75f, -0.11f}, {-0.26f, 1.43f, 0.11f}, skinT, reach, {-0.32f, 1.37f, 0}, roll);
                b.box({0.26f, 0.75f, -0.11f}, {0.38f, 1.43f, 0.11f}, skinT, reach, {0.32f, 1.37f, 0}, roll);
                faceTiles(T_FORGOTTEN_SKIN, zombieSkin ? T_ZOMBIE_FACE : T_FORGOTTEN_FACE);
                b.box({-0.24f, 1.45f, -0.24f}, {0.24f, 1.93f, 0.24f}, t, 0, {}, roll);
                break;
            }
            case MobType::Chicken: {
                // A small white body, a head with a beak, flapping wings and thin yellow legs.
                int legT[6];
                tilesAll(T_HAY_TOP, legT);
                b.box({-0.1f, 0, -0.03f}, {-0.05f, 0.3f, 0.03f}, legT, m.swing, {-0.08f, 0.3f, 0}, roll);
                b.box({0.05f, 0, -0.03f}, {0.1f, 0.3f, 0.03f}, legT, -m.swing, {0.08f, 0.3f, 0}, roll);
                tilesAll(T_CHICKEN_BODY, t);
                b.box({-0.19f, 0.28f, -0.25f}, {0.19f, 0.6f, 0.25f}, t, 0, {}, roll);
                float flap = m.onGround ? 0.1f * std::sin(time * 3 + m.age) : 0.9f * std::sin(time * 30);
                b.box({-0.25f, 0.33f, -0.18f}, {-0.19f, 0.55f, 0.2f}, t, 0, {-0.19f, 0.55f, 0}, roll + flap);
                b.box({0.19f, 0.33f, -0.18f}, {0.25f, 0.55f, 0.2f}, t, 0, {0.19f, 0.55f, 0}, roll - flap);
                faceTiles(T_CHICKEN_BODY, T_CHICKEN_FACE);
                b.box({-0.13f, 0.5f, -0.38f}, {0.13f, 0.82f, -0.18f}, t, 0, {}, roll);
                break;
            }
            case MobType::Cow: {
                int legT[6];
                tilesAll(T_COW_SKIN, legT);
                const float legH = 0.5f;
                for (int k = 0; k < 4; ++k) {
                    float lx = (k % 2 ? 0.22f : -0.22f), lz = (k < 2 ? -0.38f : 0.38f);
                    float sw = (k == 0 || k == 3 ? m.swing : -m.swing);
                    b.box({lx - 0.12f, 0, lz - 0.12f}, {lx + 0.12f, legH, lz + 0.12f}, legT, sw, {lx, legH, lz}, roll);
                }
                tilesAll(T_COW_SKIN, t);
                b.box({-0.38f, legH - 0.05f, -0.6f}, {0.38f, legH + 0.65f, 0.6f}, t, 0, {}, roll);
                faceTiles(T_COW_SKIN, T_COW_FACE);
                const float hy = legH + 0.75f;
                b.box({-0.26f, hy - 0.5f, -0.95f}, {0.26f, hy, -0.55f}, t, 0, {}, roll);
                int horn[6];
                tilesAll(T_SNOW, horn);
                b.box({-0.34f, hy - 0.08f, -0.8f}, {-0.26f, hy + 0.08f, -0.72f}, horn, 0, {}, roll);
                b.box({0.26f, hy - 0.08f, -0.8f}, {0.34f, hy + 0.08f, -0.72f}, horn, 0, {}, roll);
                break;
            }
            case MobType::Villager: {
                // A long robe in the colour of the trade, arms folded in front, a big nose.
                const int robeTile = m.profession == Profession::Smith ? T_VILLAGER_SMITH
                                     : m.profession == Profession::Farmer ? T_VILLAGER_FARMER : T_VILLAGER_ROBE;
                int robe[6], skinT[6];
                tilesAll(robeTile, robe);
                tilesAll(T_VILLAGER_SKIN, skinT);
                b.box({-0.24f, 0, -0.13f}, {-0.01f, 0.75f, 0.13f}, robe, m.swing * 0.6f, {-0.12f, 0.75f, 0}, roll);
                b.box({0.01f, 0, -0.13f}, {0.24f, 0.75f, 0.13f}, robe, -m.swing * 0.6f, {0.12f, 0.75f, 0}, roll);
                b.box({-0.26f, 0.7f, -0.15f}, {0.26f, 1.48f, 0.15f}, robe, 0, {}, roll);
                b.box({-0.3f, 1.0f, -0.32f}, {0.3f, 1.2f, -0.14f}, robe, 0, {}, roll);   // folded arms
                b.box({-0.12f, 1.02f, -0.34f}, {0.12f, 1.18f, -0.3f}, skinT, 0, {}, roll);  // hands
                faceTiles(T_VILLAGER_SKIN, T_VILLAGER_FACE);
                b.box({-0.24f, 1.48f, -0.24f}, {0.24f, 2.0f, 0.24f}, t, 0, {}, roll);
                b.box({-0.05f, 1.55f, -0.33f}, {0.05f, 1.75f, -0.24f}, skinT, 0, {}, roll);  // nose
                break;
            }
            case MobType::CrimsonMan: {
                int body[6];
                tilesAll(T_CRIMSON_BODY, body);
                b.box({-0.55f, 0, -0.3f}, {-0.1f, 1.2f, 0.3f}, body, m.swing, {-0.32f, 1.2f, 0}, roll);
                b.box({0.1f, 0, -0.3f}, {0.55f, 1.2f, 0.3f}, body, -m.swing, {0.32f, 1.2f, 0}, roll);
                b.box({-0.85f, 1.2f, -0.45f}, {0.85f, 2.7f, 0.45f}, body, 0, {}, roll);
                // Long arms hanging to the knees; they smash down when it attacks.
                float slam = m.attackAnim * 2.2f;
                b.box({-1.3f, 0.6f, -0.25f}, {-0.85f, 2.6f, 0.25f}, body, -m.swing * 0.7f + slam, {-1.07f, 2.5f, 0}, roll);
                b.box({0.85f, 0.6f, -0.25f}, {1.3f, 2.6f, 0.25f}, body, m.swing * 0.7f + slam, {1.07f, 2.5f, 0}, roll);
                faceTiles(T_CRIMSON_BODY, T_CRIMSON_FACE);
                b.box({-0.4f, 2.7f, -0.55f}, {0.4f, 3.4f, 0.2f}, t, 0, {}, roll);
                break;
            }
            case MobType::Spider: {
                // A round abdomen, a small body with the red-eyed head, and eight legs that bend down.
                int legT[6];
                tilesAll(T_SPIDER_BODY, legT);
                tilesAll(T_SPIDER_BODY, t);
                b.box({-0.38f, 0.22f, 0.02f}, {0.38f, 0.78f, 0.75f}, t, 0, {}, roll);
                b.box({-0.26f, 0.25f, -0.3f}, {0.26f, 0.6f, 0.05f}, t, 0, {}, roll);
                faceTiles(T_SPIDER_BODY, T_SPIDER_FACE);
                b.box({-0.25f, 0.22f, -0.62f}, {0.25f, 0.62f, -0.28f}, t, 0, {}, roll);
                for (int k = 0; k < 8; ++k) {
                    const float side = k < 4 ? -1.0f : 1.0f, z = -0.24f + (k % 4) * 0.12f;
                    const float step = (k % 2 ? m.swing : -m.swing) * 0.5f;
                    const float x0 = side < 0 ? -0.95f : 0.22f, x1 = side < 0 ? -0.22f : 0.95f;
                    // Tilted down towards the tips (a roll around the body side), stepping back and forth.
                    b.box({x0, 0.44f, z - 0.03f}, {x1, 0.5f, z + 0.03f}, legT, step, {side * 0.22f, 0.47f, z}, roll - side * 0.55f);
                }
                break;
            }
            case MobType::Celestling: {
                // A small round sky sprite with stubby feet and two glowing eyes; it glows a little.
                b.light = std::max(b.light, 1.25f);
                int legT[6];
                tilesAll(T_CELESTLING_BODY, legT);
                b.box({-0.15f, 0, -0.05f}, {-0.04f, 0.18f, 0.06f}, legT, m.swing, {-0.1f, 0.18f, 0}, roll);
                b.box({0.04f, 0, -0.05f}, {0.15f, 0.18f, 0.06f}, legT, -m.swing, {0.1f, 0.18f, 0}, roll);
                faceTiles(T_CELESTLING_BODY, T_CELESTLING_FACE);
                const float bob = std::sin(time * 3 + m.age) * 0.03f;
                b.box({-0.25f, 0.16f + bob, -0.25f}, {0.25f, 0.66f + bob, 0.25f}, t, 0, {}, roll);
                break;
            }
            case MobType::SkyGuardian:
                // The boss: a Reach Stalker grown huge and bright, with a Sky Drifter's wings on his back.
                b.scale = 1.7f;
                b.light = std::max(b.light, 1.15f);
                {
                    int wing[6];
                    tilesAll(T_DRIFTER_WING, wing);
                    const float flap = std::sin(time * 1.8f + m.age) * 0.3f;
                    b.box({-1.4f, 1.3f, 0.12f}, {-0.2f, 1.36f, 0.6f}, wing, 0, {-0.2f, 1.33f, 0.2f}, roll + flap);
                    b.box({0.2f, 1.3f, 0.12f}, {1.4f, 1.36f, 0.6f}, wing, 0, {0.2f, 1.33f, 0.2f}, roll - flap);
                }
                [[fallthrough]];
            case MobType::ReachStalker: {
                // Tall and thin, long arms, a pale face with no mouth.
                int legT[6], armT[6];
                tilesAll(T_STALKER_BODY, legT);
                tilesAll(T_STALKER_BODY, armT);
                b.box({-0.18f, 0, -0.09f}, {-0.02f, 1.1f, 0.09f}, legT, m.swing * 0.7f, {-0.1f, 1.1f, 0}, roll);
                b.box({0.02f, 0, -0.09f}, {0.18f, 1.1f, 0.09f}, legT, -m.swing * 0.7f, {0.1f, 1.1f, 0}, roll);
                tilesAll(T_STALKER_BODY, t);
                b.box({-0.22f, 1.1f, -0.11f}, {0.22f, 1.85f, 0.11f}, t, 0, {}, roll);
                const float reach = 0.3f + m.attackAnim * 1.2f;
                b.box({-0.32f, 0.75f, -0.07f}, {-0.22f, 1.83f, 0.07f}, armT, reach, {-0.27f, 1.8f, 0}, roll);
                b.box({0.22f, 0.75f, -0.07f}, {0.32f, 1.83f, 0.07f}, armT, reach, {0.27f, 1.8f, 0}, roll);
                faceTiles(T_STALKER_BODY, T_STALKER_FACE);
                b.box({-0.2f, 1.85f, -0.2f}, {0.2f, 2.28f, 0.2f}, t, 0, {}, roll);
                break;
            }
            case MobType::SkyDrifter: {
                // A long flat ray of the sky with broad slow wings and a tail.
                tilesAll(T_DRIFTER_BODY, t);
                b.box({-0.25f, 0.3f, -0.7f}, {0.25f, 0.5f, 0.5f}, t, 0, {}, roll);
                faceTiles(T_DRIFTER_BODY, T_DRIFTER_FACE);
                b.box({-0.18f, 0.32f, -0.95f}, {0.18f, 0.5f, -0.7f}, t, 0, {}, roll);
                int wing[6];
                tilesAll(T_DRIFTER_WING, wing);
                const float flap = std::sin(time * 2.2f + m.age) * 0.35f;
                b.box({-1.3f, 0.38f, -0.5f}, {-0.25f, 0.42f, 0.35f}, wing, 0, {-0.25f, 0.4f, 0}, roll + flap);
                b.box({0.25f, 0.38f, -0.5f}, {1.3f, 0.42f, 0.35f}, wing, 0, {0.25f, 0.4f, 0}, roll - flap);
                b.box({-0.06f, 0.36f, 0.5f}, {0.06f, 0.42f, 1.2f}, t, std::sin(time * 3) * 0.2f, {0, 0.4f, 0.5f}, roll);
                break;
            }
            // 2.6.0: the new creatures, in model pixels (16 to a block). `sit` folds them down to rest,
            // `breathe` makes them breathe while standing about.
            case MobType::CornerstoneGolem:
            case MobType::MossLurker:
            case MobType::Sporeling:
            case MobType::EmberMoth:
            case MobType::AshFox:
            case MobType::Fox:
            case MobType::Ghost:
            case MobType::Snowman: {
                const float sit = m.sit, breathe = std::sin(time * 2.0f + m.age) * 0.4f, P = 1 / 16.0f;
                auto bx = [&](float x0, float y0, float z0, float x1, float y1, float z1, const int tl[6], float pitch = 0, Vec3 piv = {}, float rl = 0) {
                    b.box({x0 * P, y0 * P, z0 * P}, {x1 * P, y1 * P, z1 * P}, tl, pitch, piv * P, rl);
                };
                int a6[6], f6[6], c6[6];
                if (m.type == MobType::CornerstoneGolem) {
                    tilesAll(T_GOLEM_SKIN, a6);
                    const float drop = 9 * sit;  // sitting: the body goes down onto folded legs
                    bx(-4, 0, -2, -1, 10, 2, a6, m.swing * (1 - sit) - 1.45f * sit, {-2.5f, 10 - drop, 0});
                    bx(1, 0, -2, 4, 10, 2, a6, -m.swing * (1 - sit) - 1.45f * sit, {2.5f, 10 - drop, 0});
                    bx(-5, 10 - drop, -3, 5, 22 - drop + breathe * 0.5f, 3, a6);
                    tilesAll(T_GILDED_COBBLE, c6);
                    bx(-6, 19 - drop, -4, 6, 23 - drop + breathe * 0.5f, 4, c6);
                    bx(-3, 14 - drop, -3.5f, 3, 16 - drop, -3, c6);
                    const float armSwing = -m.swing * 0.6f * (1 - sit) + m.attackAnim * 1.6f;
                    bx(-9, 8 - drop, -2, -5, 22 - drop, 2, a6, armSwing, {-7, 21 - drop, 0});
                    bx(5, 8 - drop, -2, 9, 22 - drop, 2, a6, -armSwing + 2 * m.attackAnim * 1.6f * 0, {7, 21 - drop, 0});
                    tilesAll(T_GOLEM_SKIN, f6);
                    f6[2] = T_GOLEM_FACE;
                    bx(-3, 23 - drop, -3, 3, 28 - drop, 3, f6);
                    tilesAll(T_MOSSY_CORNERSTONE_TOP, c6);
                    bx(-2, 28 - drop, -1, 2, 30 - drop, 1, c6);
                } else if (m.type == MobType::MossLurker) {
                    tilesAll(T_LURKER_SKIN, a6);
                    const float drop = 2 * sit;
                    for (int k = 0; k < 4; ++k) {
                        const float lx = k % 2 ? 3 : -5, lz = k < 2 ? -6 : 3;
                        bx(lx, 0, lz, lx + 2, 3, lz + 2, a6, (k == 0 || k == 3 ? m.swing : -m.swing) * (1 - sit), {lx + 1, 3, lz + 1});
                    }
                    bx(-3, 2 - drop, -8, 3, 6 - drop + breathe * 0.4f, 6, a6);
                    tilesAll(T_MOSSY_CORNERSTONE_TOP, c6);
                    bx(-3, 6 - drop + breathe * 0.4f, -6, 3, 7 - drop + breathe * 0.4f, 4, c6);
                    bx(-2, 7 - drop + breathe * 0.4f, -4, 2, 8 - drop + breathe * 0.4f, 2, c6);
                    tilesAll(T_LURKER_SKIN, f6);
                    f6[2] = T_LURKER_FACE;
                    bx(-2, 2 - drop, -12, 2, 5.5f - drop, -8, f6, -m.attackAnim * 0.5f, {0, 3, -8});
                    const float wag = std::sin(time * 2.5f + m.age) * 0.25f;
                    bx(-1, 3 - drop, 6, 1, 5 - drop, 14, a6, 0, {0, 4, 6}, wag);
                    bx(-0.5f, 3 - drop, 14, 0.5f, 4 - drop, 18, a6, 0, {0, 4, 6}, wag * 1.5f);
                } else if (m.type == MobType::Sporeling) {
                    tilesAll(T_SPORELING_SKIN, a6);
                    const float drop = 2.5f * sit;
                    bx(-3, 0, -1, -1, 3 - drop, 1, a6, m.swing * (1 - sit), {-2, 3, 0});
                    bx(1, 0, -1, 3, 3 - drop, 1, a6, -m.swing * (1 - sit), {2, 3, 0});
                    tilesAll(T_SPORELING_SKIN, f6);
                    f6[2] = T_SPORELING_FACE;
                    bx(-3, 3 - drop, -3, 3, 8 - drop + breathe * 0.3f, 3, f6);
                    tilesAll(T_SPORE_PAD_TOP, c6);
                    const float bob = std::sin(time * 3.0f + m.age) * 0.3f;
                    bx(-6, 8 - drop + bob, -6, 6, 11 - drop + bob, 6, c6);
                    bx(-4, 11 - drop + bob, -4, 4, 13 - drop + bob, 4, c6);
                    int w6[6];
                    tilesAll(T_FOX_WHITE, w6);
                    bx(-5, 13 - drop + bob, -1, -4, 15 - drop + bob, 0, w6);
                    bx(3, 12 - drop + bob, 2, 4, 14 - drop + bob, 3, w6);
                    bx(0, 13 - drop + bob, 3, 1, 15.5f - drop + bob, 4, w6);
                } else if (m.type == MobType::Ghost) {
                    // A sheet with a face, bobbing in the air, its hem in waves.
                    const float bob = std::sin(time * 2.0f + m.age) * 1.2f;
                    tilesAll(T_GHOST, a6);
                    tilesAll(T_GHOST, f6);
                    f6[2] = T_GHOST_FACE;
                    const float keep = b.light;
                    b.light = std::max(keep, 3.2f);  // it glows pale in the dark
                    bx(-4, 8 + bob, -4, 4, 20 + bob, 4, f6);
                    for (int k = 0; k < 4; ++k) {
                        const float hx = -4 + k * 2, wave = std::sin(time * 4.0f + k) * 1.2f;
                        bx(hx, 4 + bob + wave, -4, hx + 2, 8 + bob, 4, a6);
                    }
                    bx(-6, 12 + bob, -1, -4, 16 + bob, 1, a6, 0, {-4, 15 + bob, 0}, 0.4f + std::sin(time * 3.0f) * 0.2f);
                    bx(4, 12 + bob, -1, 6, 16 + bob, 1, a6, 0, {4, 15 + bob, 0}, -0.4f - std::sin(time * 3.0f) * 0.2f);
                    b.light = keep;
                } else if (m.type == MobType::Snowman) {
                    // Three balls of snow, a face with a carrot nose, a black hat; it throws with an arm.
                    tilesAll(T_SNOW, a6);
                    bx(-5, 0, -5, 5, 9, 5, a6);
                    bx(-4, 9, -4, 4, 16, 4, a6);
                    tilesAll(T_SNOW, f6);
                    f6[2] = T_SNOWMAN_FACE;
                    bx(-3.5f, 16, -3.5f, 3.5f, 23, 3.5f, f6);
                    int hat6[6];
                    tilesAll(T_FOX_DARK, hat6);
                    bx(-4.5f, 23, -4.5f, 4.5f, 24, 4.5f, hat6);
                    bx(-3, 24, -3, 3, 28, 3, hat6);
                    int stick6[6];
                    tilesAll(T_LOG_SIDE, stick6);
                    bx(4, 12, -0.5f, 9, 13, 0.5f, stick6, 0, {4, 12.5f, 0}, -0.4f - m.attackAnim * 1.2f);
                    bx(-9, 12, -0.5f, -4, 13, 0.5f, stick6, 0, {-4, 12.5f, 0}, 0.4f);
                } else if (m.type == MobType::EmberMoth) {
                    tilesAll(T_MOTH_BODY, a6);
                    bx(-2, 6, -5, 2, 10, 5, a6);
                    bx(-2, 7, -8, 2, 11, -5, a6);
                    bx(-2, 10, -10, -1, 13, -9, a6);
                    bx(1, 10, -10, 2, 13, -9, a6);
                    const float flap = std::sin(time * 14.0f + m.age * 3) * 0.7f;
                    tilesAll(T_MOTH_WING, c6);
                    bx(-12, 9, -4, -2, 10, 3, c6, 0, {-2, 9.5f, 0}, flap);
                    bx(2, 9, -4, 12, 10, 3, c6, 0, {2, 9.5f, 0}, -flap);
                    bx(-9, 8, 2, -2, 9, 7, c6, 0, {-2, 8.5f, 0}, flap * 0.8f);
                    bx(2, 8, 2, 9, 9, 7, c6, 0, {2, 8.5f, 0}, -flap * 0.8f);
                    tilesAll(T_MOTH_GLOW, c6);
                    const float keep = b.light;
                    b.light = std::max(keep, 1.6f + 0.3f * std::sin(time * 4.0f));  // the glowing tail
                    bx(-1, 5, 5, 1, 7, 8, c6);
                    b.light = keep;
                } else {
                    // Foxes: the orange one of the forests, the ash grey one of the Cornerstone.
                    const bool ash = m.type == MobType::AshFox;
                    const int fur = ash ? T_ASHFOX_FUR : T_FOX_FUR, faceT = ash ? T_ASHFOX_FACE : T_FOX_FACE;
                    int fur6[6], dark6[6], white6[6];
                    tilesAll(fur, fur6);
                    tilesAll(ash ? T_ASHFOX_FUR : T_FOX_DARK, dark6);
                    tilesAll(T_FOX_WHITE, white6);
                    // Sitting: the back goes down, the front legs stay up and the body leans back.
                    // The rear goes down and the chest stays up, so the body tilts back (front higher).
                    const float lean = 0.5f * sit, rear = 3.0f * sit;
                    bx(-3, 0, -5, -1, 5, -3, dark6, m.swing * (1 - sit), {-2, 5, -4});
                    bx(1, 0, -5, 3, 5, -3, dark6, -m.swing * (1 - sit), {2, 5, -4});
                    bx(-3, 0, 2, -1, 5 - rear, 4, dark6, -m.swing * (1 - sit), {-2, 5, 3});
                    bx(1, 0, 2, 3, 5 - rear, 4, dark6, m.swing * (1 - sit), {2, 5, 3});
                    (void)white6;
                    int body6[6];
                    tilesAll(fur, body6);
                    body6[1] = T_FOX_WHITE;  // a white belly
                    bx(-3, 5, -6, 3, 10 + breathe * 0.3f, 5, body6, lean, {0, 7, -5});
                    tilesAll(fur, f6);
                    f6[2] = faceT;
                    const float lift = 1.5f * sit;
                    bx(-3, 7 + lift, -10, 3, 12 + lift, -6, f6);
                    if (m.tamed && m.collar >= 0) {
                        // 2.7.0: the collar, in the colour it was dyed, round the neck.
                        int col6[6];
                        tilesAll(T_WOOL + m.collar, col6);
                        bx(-3.7f, 5.4f + lift, -7.0f, 3.7f, 8.4f + lift, -5.0f, col6);
                    }
                    // The muzzle: fur on top, white below, a small black nose at its tip.
                    int snout6[6];
                    tilesAll(fur, snout6);
                    snout6[1] = T_FOX_WHITE;
                    snout6[2] = T_FOX_WHITE;
                    bx(-1.5f, 7 + lift, -12.5f, 1.5f, 9.5f + lift, -10, snout6);
                    int nose6[6];
                    tilesAll(T_FOX_DARK, nose6);
                    bx(-0.6f, 8.6f + lift, -12.9f, 0.6f, 9.6f + lift, -12.4f, nose6);
                    int ear6[6];
                    tilesAll(fur, ear6);
                    ear6[0] = ash ? T_MOTH_GLOW : T_FOX_DARK;  // dark (or glowing) ear tips
                    bx(-3, 12 + lift, -9, -1, 15 + lift, -8, ear6);
                    bx(1, 12 + lift, -9, 3, 15 + lift, -8, ear6);
                    const float wag = std::sin(time * (m.tamed ? 7.0f : 3.0f) + m.age) * (m.tamed ? 0.35f : 0.2f);
                    bx(-2, 7 - rear, 5, 2, 11 - rear, 12, fur6, 0.25f * sit, {0, 9, 5}, wag);
                    int tip6[6];
                    tilesAll(ash ? T_MOTH_GLOW : T_FOX_WHITE, tip6);
                    if (ash) b.light = std::max(b.light, 1.5f);
                    bx(-1.5f, 7.5f - rear, 12, 1.5f, 10.5f - rear, 15, tip6, 0.25f * sit, {0, 9, 5}, wag);
                    b.light = light;
                }
                break;
            }
            case MobType::Bat: {
                tilesAll(T_BAT_BODY, t);
                b.box({-0.11f, 0.15f, -0.1f}, {0.11f, 0.45f, 0.12f}, t, 0, {}, roll);
                faceTiles(T_BAT_BODY, T_BAT_FACE);
                b.box({-0.12f, 0.43f, -0.13f}, {0.12f, 0.62f, 0.08f}, t, 0, {}, roll);
                int wing[6];
                tilesAll(T_BAT_WING, wing);
                const float flap = std::sin(time * 22 + m.age * 3) * 0.9f;
                b.box({-0.62f, 0.38f, -0.08f}, {-0.11f, 0.41f, 0.16f}, wing, 0, {-0.11f, 0.4f, 0}, roll + flap);
                b.box({0.11f, 0.38f, -0.08f}, {0.62f, 0.41f, 0.16f}, wing, 0, {0.11f, 0.4f, 0}, roll - flap);
                break;
            }
            case MobType::TntZombie: {
                // A zombie in a vest of TNT sticks, with a block of TNT strapped to its back.
                int legT[6], skinT[6], tnt[6];
                tilesAll(T_FORGOTTEN_SHIRT, legT);
                tilesAll(T_FORGOTTEN_SKIN, skinT);
                tilesAll(T_TNT_SIDE, tnt);
                tnt[0] = T_TNT_TOP;
                tnt[1] = T_TNT_BOTTOM;
                b.box({-0.24f, 0, -0.12f}, {-0.01f, 0.75f, 0.12f}, legT, m.swing, {-0.12f, 0.75f, 0}, roll);
                b.box({0.01f, 0, -0.12f}, {0.24f, 0.75f, 0.12f}, legT, -m.swing, {0.12f, 0.75f, 0}, roll);
                tilesAll(T_TNT_ZOMBIE_SHIRT, t);
                b.box({-0.25f, 0.75f, -0.13f}, {0.25f, 1.45f, 0.13f}, t, 0, {}, roll);
                b.box({-0.2f, 0.85f, 0.13f}, {0.2f, 1.35f, 0.42f}, tnt, 0, {}, roll);
                float reach = kPi / 2 + 0.15f * std::sin(time * 2 + m.age);
                b.box({-0.38f, 0.75f, -0.11f}, {-0.26f, 1.43f, 0.11f}, skinT, reach, {-0.32f, 1.37f, 0}, roll);
                b.box({0.26f, 0.75f, -0.11f}, {0.38f, 1.43f, 0.11f}, skinT, reach, {0.32f, 1.37f, 0}, roll);
                faceTiles(T_FORGOTTEN_SKIN, T_ZOMBIE_FACE);
                b.box({-0.24f, 1.45f, -0.24f}, {0.24f, 1.93f, 0.24f}, t, 0, {}, roll);
                break;
            }
            case MobType::Husking: {
                // A thin man in a torn robe without arms; the head hangs to one side and twitches.
                int robe[6], skinT[6];
                tilesAll(T_HUSKING_ROBE, robe);
                tilesAll(T_HUSKING_SKIN, skinT);
                const float sway = std::sin(time * 1.7f + m.age) * 0.05f;
                b.box({-0.22f, 0, -0.11f}, {-0.02f, 0.78f, 0.11f}, robe, m.swing * 0.7f, {-0.12f, 0.78f, 0}, roll);
                b.box({0.02f, 0, -0.11f}, {0.22f, 0.78f, 0.11f}, robe, -m.swing * 0.7f, {0.12f, 0.78f, 0}, roll);
                b.box({-0.24f, 0.75f, -0.12f}, {0.24f, 1.5f, 0.12f}, robe, 0, {}, roll + sway);
                b.box({-0.07f, 1.48f, -0.07f}, {0.07f, 1.58f, 0.07f}, skinT, 0, {}, roll + sway);  // the neck
                // The head: bent far to the side, jerking now and then, thrown back when it spits.
                const float jerk = std::fmod(m.age * 0.7f, 3.0f) < 0.12f ? 0.25f : 0;
                const float tilt = 0.55f + std::sin(time * 9 + m.age) * 0.04f + jerk;
                faceTiles(T_HUSKING_SKIN, T_HUSKING_FACE);
                b.box({-0.22f, 1.56f, -0.22f}, {0.22f, 2.0f, 0.22f}, t, -m.attackAnim * 0.5f, {0, 1.58f, 0}, roll + tilt);
                break;
            }
            case MobType::Hogman: {
                // 1.5.7: a stocky pig man in a leather tunic: pink arms, a snout and tusks.
                int tunic[6], skinT[6];
                tilesAll(T_HOGMAN_TUNIC, tunic);
                tilesAll(T_HOGMAN_SKIN, skinT);
                b.box({-0.24f, 0, -0.13f}, {-0.01f, 0.72f, 0.13f}, tunic, m.swing, {-0.12f, 0.72f, 0}, roll);
                b.box({0.01f, 0, -0.13f}, {0.24f, 0.72f, 0.13f}, tunic, -m.swing, {0.12f, 0.72f, 0}, roll);
                b.box({-0.29f, 0.72f, -0.16f}, {0.29f, 1.45f, 0.16f}, tunic, 0, {}, roll);
                // Calm and bartering: arms down and turning the stone over; angry: raised to strike.
                const float arm = m.barter > 0 ? 1.0f + 0.2f * std::sin(time * 8) : m.calm > 0 ? 0.1f * std::sin(time * 2 + m.age)
                                                                                       : -m.swing * 0.8f + m.attackAnim * 1.4f;
                b.box({-0.42f, 0.75f, -0.11f}, {-0.29f, 1.43f, 0.11f}, skinT, arm, {-0.35f, 1.37f, 0}, roll);
                b.box({0.29f, 0.75f, -0.11f}, {0.42f, 1.43f, 0.11f}, skinT, m.swing * 0.8f, {0.35f, 1.37f, 0}, roll);
                faceTiles(T_HOGMAN_SKIN, T_HOGMAN_FACE);
                b.box({-0.26f, 1.45f, -0.25f}, {0.26f, 1.93f, 0.25f}, t, 0, {}, roll);
                b.box({-0.11f, 1.5f, -0.33f}, {0.11f, 1.66f, -0.25f}, skinT, 0, {}, roll);  // the snout
                b.box({-0.3f, 1.84f, -0.06f}, {-0.24f, 1.98f, 0.06f}, skinT, 0, {}, roll);  // ears
                b.box({0.24f, 1.84f, -0.06f}, {0.3f, 1.98f, 0.06f}, skinT, 0, {}, roll);
                break;
            }
            case MobType::TarMonster: {
                // A dripping heap of tar: stumpy legs, a wobbling body, long arms and glowing eyes.
                int tar[6];
                tilesAll(T_TAR_SKIN, tar);
                const float wob = std::sin(time * 3 + m.age) * 0.04f;
                b.box({-0.4f, 0, -0.2f}, {-0.08f, 0.5f, 0.2f}, tar, m.swing * 0.6f, {-0.24f, 0.5f, 0}, roll);
                b.box({0.08f, 0, -0.2f}, {0.4f, 0.5f, 0.2f}, tar, -m.swing * 0.6f, {0.24f, 0.5f, 0}, roll);
                b.box({-0.55f - wob, 0.45f, -0.35f}, {0.55f + wob, 1.35f + wob, 0.35f}, tar, 0, {}, roll);
                const float grab = kPi / 3 + m.attackAnim * 1.2f;
                b.box({-0.8f, 0.3f, -0.15f}, {-0.55f, 1.3f, 0.15f}, tar, grab, {-0.67f, 1.25f, 0}, roll);
                b.box({0.55f, 0.3f, -0.15f}, {0.8f, 1.3f, 0.15f}, tar, grab, {0.67f, 1.25f, 0}, roll);
                faceTiles(T_TAR_SKIN, T_TAR_FACE);
                b.box({-0.32f, 1.3f + wob, -0.36f}, {0.32f, 1.75f + wob, 0.22f}, t, 0, {}, roll);
                // Drops of tar hanging from the body.
                b.box({-0.3f, 0.3f, -0.36f}, {-0.2f, 0.45f, -0.3f}, tar, 0, {}, roll);
                b.box({0.15f, 0.25f, -0.36f}, {0.25f, 0.45f, -0.3f}, tar, 0, {}, roll);
                break;
            }
            default: break;
        }
        (void)rollPivot;
    }
}

void MobSystem::save(const std::string& folder) const {
    std::ostringstream o;
    o << "VCMOBS5\n";  // 2.6.0: + tamed; 2.7.0: + collar
    for (const Mob& m : mobs_)
        if (!mobInfo(m.type).hostile && m.alive())
            o << (int)m.type << " " << m.dim << " " << m.pos.x << " " << m.pos.y << " " << m.pos.z << " " << m.yaw << " " << m.health
              << " " << (int)m.profession << " " << (m.hasHome ? 1 : 0) << " " << m.home.x << " " << m.home.y << " " << m.home.z
              << " " << m.variant << " " << (m.tamed ? 1 : 0) << " " << (int)m.collar << "\n";
    std::ofstream(folder + "/mobs.dat") << o.str();
}

void MobSystem::load(const std::string& folder) {
    mobs_.clear();
    std::ifstream in(folder + "/mobs.dat");
    std::string magic;
    if (!std::getline(in, magic) || (magic != "VCMOBS1" && magic != "VCMOBS2" && magic != "VCMOBS3" && magic != "VCMOBS4" && magic != "VCMOBS5")) return;
    const bool v5 = magic == "VCMOBS5";
    const bool v2 = magic != "VCMOBS1", v3 = magic == "VCMOBS3" || magic == "VCMOBS4" || v5, v4 = magic == "VCMOBS4" || v5;
    int type, dim;
    float x, y, z, yaw, health;
    while (in >> type >> dim >> x >> y >> z >> yaw >> health) {
        int profession = 0, hasHome = 0;
        float hx = 0, hy = 0, hz = 0;
        int variant = 0;
        if (v2 && !(in >> profession >> hasHome >> hx >> hy >> hz)) break;
        if (v3 && !(in >> variant)) break;
        int tamed = 0;
        if (v4 && !(in >> tamed)) break;
        int collar = -1;
        if (v5 && !(in >> collar)) break;
        if (type < 0 || type >= (int)MobType::Count || mobInfo((MobType)type).hostile) continue;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
        Mob m;
        m.type = (MobType)type;
        m.dim = std::clamp(dim, 0, 3);
        m.pos = Vec3(x, y, z);
        m.yaw = yaw;
        m.health = std::clamp(health, 1.0f, mobInfo(m.type).health);
        m.wanderTimer = 1 + random() * 4;
        m.profession = (Profession)std::clamp(profession, 0, (int)Profession::Count - 1);
        m.hasHome = hasHome != 0 && std::isfinite(hx) && std::isfinite(hy) && std::isfinite(hz);
        m.home = Vec3(hx, hy, hz);
        m.variant = (uint16_t)(ModRegistry::get().mob(variant) ? variant : 0);
        m.tamed = tamed != 0;
        m.collar = (int8_t)std::clamp(collar, -1, 15);
        if (m.tamed && m.collar < 0 && kFeatures270) m.collar = 14;  // 2.7.0: foxes tamed before get a red collar
        if (m.tamed) m.sitting = true;  // a tamed fox waits where it was left
        m.uid = nextUid_++;
        mobs_.push_back(m);
        if (mobs_.size() > 200) break;
    }
}

std::vector<std::pair<ItemId, int>> mobDrops(MobType type, float roll) {
    switch (type) {
        case MobType::Pig: return {{RAW_PORK, 1 + (int)(roll * 3)}};
        case MobType::Sheep: return {{WOOL, 1 + (int)(roll * 2)}, {RAW_MUTTON, 1 + (int)(roll * 2) % 2}};
        // 1.9.5 build 3: and honey balls.
        case MobType::PumpkinSlime:
            return roll < 0.35f ? std::vector<std::pair<ItemId, int>>{{PUMPKIN, 1}, {HONEY_BALL, 1}}
                                : std::vector<std::pair<ItemId, int>>{{HONEY_BALL, 1 + (int)(roll * 3)}, {COAL, 1}};
        case MobType::ForgottenMan:
            return roll < 0.12f ? std::vector<std::pair<ItemId, int>>{{IRON_INGOT, 1}, {COAL, 1}}
                                : std::vector<std::pair<ItemId, int>>{{COAL, 1 + (int)(roll * 2)}};
        case MobType::Chicken: return {{RAW_CHICKEN, 1}, {FEATHER, (int)(roll * 3)}};
        case MobType::Cow:
            if (kFeatures270 && roll > 0.3f) return {{RAW_BEEF, 1 + (int)(roll * 3)}, {LEATHER, 1 + (int)(roll * 1.8f)}};  // 2.7.0: hide
            return {{RAW_BEEF, 1 + (int)(roll * 3)}};
        case MobType::Spider: return {{STRING, 1 + (int)(roll * 2.5f)}};
        case MobType::TntZombie: return {{roll < 0.5f ? (ItemId)TNT : (ItemId)COAL, 1}};
        case MobType::TarMonster: return {{TAR_BALL, 1 + (int)(roll * 2.5f)}};
        case MobType::Husking:
            if (roll < 0.1f) return {{(ItemId)(OLD_DOCUMENT + (int)(roll * 79) % kDocumentPages), 1}, {POWERSTONE_DUST, 1}};
            return roll < 0.4f ? std::vector<std::pair<ItemId, int>>{{ANCIENT_COIN, 1}, {POWERSTONE_DUST, 1}}
                               : std::vector<std::pair<ItemId, int>>{{POWERSTONE_DUST, 1 + (int)(roll * 2)}};
        case MobType::Hogman:
            return roll < 0.2f ? std::vector<std::pair<ItemId, int>>{{RAW_PORK, 2}, {CRIMSON_LOG, 2}}
                               : std::vector<std::pair<ItemId, int>>{{RAW_PORK, 1 + (int)(roll * 2)}};
        case MobType::Celestling: return {{PALE_CRYSTAL, 1 + (int)(roll * 2)}};
        case MobType::ReachStalker: return roll < 0.3f ? std::vector<std::pair<ItemId, int>>{{CELESTIUM, 1}, {PALE_CRYSTAL, 1}}
                                                       : std::vector<std::pair<ItemId, int>>{{PALE_CRYSTAL, 1 + (int)(roll * 2)}};
        case MobType::SkyDrifter: return {{FEATHER, 2 + (int)(roll * 3)}};
        case MobType::SkyGuardian: return {{CELESTIAL_WINGS, 1}, {CELESTIUM, 6}, {PALE_CRYSTAL, 8}};
        // 2.6.0
        case MobType::CornerstoneGolem:
            return roll < 0.35f ? std::vector<std::pair<ItemId, int>>{{NETHERRACK, 3}, {roll < 0.15f ? (ItemId)CS_STEEL_ORE : (ItemId)CS_BRONZE_ORE, 1}}
                                : std::vector<std::pair<ItemId, int>>{{NETHERRACK, 2 + (int)(roll * 3)}, {CS_IRON_ORE, 1}};
        case MobType::MossLurker: return {{MOSSY_CORNERSTONE, 1}, {RAW_BEEF, 1 + (int)(roll * 2)}};
        case MobType::Sporeling: return {{SPORE_PAD, 1}};
        case MobType::EmberMoth: return {{EMBER_DUST, 1 + (int)(roll * 2)}};
        case MobType::AshFox:
        case MobType::Fox: return {};
        case MobType::Ghost: return {{CARAMEL, 1}};
        case MobType::Snowman: return {{SNOW_BLOCK, 2}};
        case MobType::CrimsonMan:
            // 1.7.0 preview: his twelve heads, which open the Celestial portal, and the compass to it.
            return {{CRIMSON_HEAD, 12}, {CELESTIAL_COMPASS, 1}};
        default: return {};
    }
}

}  // namespace vc

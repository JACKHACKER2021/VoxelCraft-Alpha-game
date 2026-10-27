#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Math.h"
#include "Mesher.h"
#include "Blocks.h"

namespace vc {

class World;

// VoxelCraft's creatures. Pigs, sheep, chickens and cows wander the meadows; villagers
// live in villages and trade; at night the pumpkin slime and the forgotten man hunt the
// player; the Crimson Man guards the arena of the Crimson Void. Since 1.4.0 spiders climb
// walls, bats flutter in caves, the TNT zombie blows up when it stands by the player, and
// the tar monster of the deep caves turns the animals it kills into more tar monsters. Since
// 1.5.0 the Husking haunts the Powerstone Wasteland: it keeps its distance and spits fire.
enum class MobType : uint8_t {
    Pig, Sheep, PumpkinSlime, ForgottenMan, CrimsonMan, Chicken, Cow, Villager,
    Spider, Bat, TntZombie, TarMonster,  // 1.4.0
    Husking,                             // 1.5.0: armless, head bent aside, spits fireballs
    Hogman,                              // 1.5.7: a pig man of the Cornerstone Dimension; calmed by powerstone
    Count
};

// What a villager does (and so what it trades): see Trading.h.
enum class Profession : uint8_t { Farmer, Smith, Trader, Count };

struct MobInfo {
    const char* name;  // Czech key
    float width, height, health, speed, damage;
    bool hostile, boss;
};
const MobInfo& mobInfo(MobType type);

struct Mob {
    MobType type = MobType::Pig;
    int dim = 0;
    Vec3 pos, vel;
    float yaw = 0, health = 10;
    bool onGround = false;
    float hurtTime = 0;      // red flash after a hit
    float deathTime = 0;     // > 0 while falling over
    float attackCooldown = 0, wanderTimer = 0, panic = 0, phase = 0, swing = 0, age = 0, attackAnim = 0;
    Vec3 target;             // where it wanders to
    bool wandering = false;
    Profession profession = Profession::Farmer;  // villagers only
    Vec3 home;               // villagers stay near it
    bool hasHome = false;
    float fuse = 0;          // TNT zombie: seconds it has been hissing
    uint32_t uid = 0;        // the same mob on every player's screen
    uint16_t variant = 0;    // a mod's mob drawn on this body (ModRegistry::mob), 0 = the original
    int killer = -1;         // multiplayer: the guest who dealt the last blow
    bool cave = false;       // spawned underground: daylight does not send it away
    float calm = 0;          // 1.5.7 hog man: seconds it stays peaceful after a gift of powerstone
    float barter = 0;        // 1.5.7 hog man: > 0 while it looks over the powerstone, then it pays
    bool alive() const { return deathTime <= 0; }
};

// What happened during an update, for the game to react to.
struct MobEvent {
    enum Kind { HitPlayer, Died, BossRoar, Explode, TarSpread, Fireball, Barter } kind;
    MobType type;
    Vec3 pos;
    float damage = 0;
    Vec3 knock;  // pushes the player (Fireball: its velocity)
    int killer = -1;  // Died: the guest who killed it (multiplayer), or -1
    int variant = 0;  // Died: a mod's mob (its drops)
};

class MobSystem {
public:
    void spawn(MobType type, int dim, const Vec3& feet);
    // A villager of a profession who keeps near `home`.
    void spawnVillager(int dim, const Vec3& feet, Profession profession, const Vec3& home);
    int countType(int dim, MobType type, const Vec3& near, float radius) const;
    // AI and physics. `target` is the player (hunted when `huntable`, e.g. not in Creative).
    void update(const World& world, int dim, const Vec3& target, bool huntable, float dt, std::vector<MobEvent>& events);
    // The closest living mob hit by the ray, or -1.
    int pick(int dim, const Vec3& origin, const Vec3& dir, float maxDist, float* distance = nullptr) const;
    // Hurts a mob (knocked away from `from`); returns true if it died. `killer`: a guest's id.
    bool hurt(int index, float damage, const Vec3& from, int killer = -1);
    int findUid(uint32_t uid) const;
    // A guest: mobs move smoothly towards the positions the host sent (kept in `target`).
    void followRemote(float dt);
    void buildVertices(int dim, float time, std::vector<Vertex>& out) const;

    int count(int dim, bool hostile, const Vec3& near, float radius) const;
    bool hostileNear(int dim, const Vec3& pos, float radius) const;
    const Mob* boss(int dim) const;
    // Removes hostile mobs far from the player (and all of them in daylight, when asked).
    void despawn(int dim, const Vec3& player, float radius, bool daylightHostiles);
    void clearHostiles();
    bool zombieSkin = false;  // Halloween: forgotten men look like zombies
    const std::vector<Mob>& mobs() const { return mobs_; }
    std::vector<Mob>& mobs() { return mobs_; }

    // Pigs and sheep are kept with the world (mobs.dat); monsters are not.
    void save(const std::string& folder) const;
    void load(const std::string& folder);

private:
    std::vector<Mob> mobs_;
    uint32_t rng_ = 0x9E3779B9u;
    uint32_t nextUid_ = 1;
    float random();
};

// Items a mob leaves behind (meat, wool, ...), decided by `roll` in 0..1.
std::vector<std::pair<ItemId, int>> mobDrops(MobType type, float roll);

}  // namespace vc

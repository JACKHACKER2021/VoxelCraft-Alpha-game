#pragma once
#include <cstdint>
#include <vector>

#include "Math.h"
#include "Mesher.h"
#include "World.h"
#include "Blocks.h"

namespace vc {

// Small textured cubes-as-sprites that fly off a block while it is mined
// and when it breaks. Each particle shows a 4x4 pixel piece of the block's
// own texture, so the colours always match the block (and texture packs).
class ParticleSystem {
public:
    static constexpr int kMaxParticles = 240;  // keeps phones smooth

    // While mining: a few chips flying off the face that is being hit.
    void spawnChips(const World& world, int x, int y, int z, int nx, int ny, int nz, ItemId block, int count);
    // When a block breaks: a burst filling the whole block.
    void spawnBurst(const World& world, int x, int y, int z, ItemId block);
    // Anniversary confetti falling around `centre`.
    void spawnConfetti(const World& world, const Vec3& centre, int count);
    void update(const World& world, float dt);
    // Camera-facing quads for the renderer (atlas texture).
    void buildVertices(const Vec3& cameraRight, const Vec3& cameraUp, std::vector<Vertex>& out) const;
    size_t count() const { return particles_.size(); }
    void clear() { particles_.clear(); }

private:
    struct Particle {
        Vec3 pos, vel;
        float life, maxLife, size;
        float u0, v0, u1, v1;
        float light;
        float fall = 1;  // gravity multiplier (confetti flutters down)
    };
    void add(const World& world, Vec3 pos, Vec3 vel, ItemId block, float life);
    float random();  // [0, 1)

    std::vector<Particle> particles_;
    uint32_t rng_ = 0x2468ACE1u;
};

}  // namespace vc

#include "Particles.h"

#include <algorithm>
#include <cmath>

#include "Blocks.h"

namespace vc {

namespace {
constexpr float kTile = kTileUv;
constexpr float kGravity = 16.0f;
}  // namespace

float ParticleSystem::random() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (rng_ & 0xFFFFFF) / 16777216.0f;
}

void ParticleSystem::add(const World& world, Vec3 pos, Vec3 vel, ItemId block, float life) {
    if (particles_.size() >= (size_t)kMaxParticles) particles_.erase(particles_.begin());  // drop the oldest
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = p.maxLife = life;
    p.size = 0.07f + random() * 0.06f;
    // A random 4x4 pixel piece of the block's texture; plants use their middle part,
    // where the stem and petals are, so the piece is rarely fully transparent.
    int tile = tileFor(block, block == GRASS ? F_NORTH : F_SOUTH);
    int px = isPlant(block) ? 5 + (int)(random() * 3) : (int)(random() * 12);
    int py = isPlant(block) ? 6 + (int)(random() * 6) : (int)(random() * 12);
    p.u0 = (tile % kAtlasCols) * kTile + px / 16.0f * kTile;
    p.v0 = (tile / kAtlasCols) * kTile + py / 16.0f * kTile;
    p.u1 = p.u0 + 4 / 16.0f * kTile;
    p.v1 = p.v0 + 4 / 16.0f * kTile;
    bool lit = world.dimension() == Dimension::Nether ||
               (int)std::floor(pos.y) >= world.skyHeight((int)std::floor(pos.x), (int)std::floor(pos.z));
    p.light = isEmissive(block) ? 1.0f : lit ? 0.9f : 0.5f;
    particles_.push_back(p);
}

void ParticleSystem::spawnChips(const World& world, int x, int y, int z, int nx, int ny, int nz, ItemId block, int count) {
    if (block == AIR) return;
    Vec3 center(x + 0.5f, y + 0.5f, z + 0.5f), n((float)nx, (float)ny, (float)nz);
    for (int i = 0; i < count; ++i) {
        // A random point on the hit face, nudged just outside it.
        Vec3 offset((random() - 0.5f) * 0.9f, (random() - 0.5f) * 0.9f, (random() - 0.5f) * 0.9f);
        if (nx) offset.x = nx * 0.52f;
        if (ny) offset.y = ny * 0.52f;
        if (nz) offset.z = nz * 0.52f;
        Vec3 vel = n * (1.0f + random() * 1.5f) + Vec3((random() - 0.5f) * 1.6f, 1.2f + random() * 1.5f, (random() - 0.5f) * 1.6f);
        add(world, center + offset, vel, block, 0.35f + random() * 0.35f);
    }
}

void ParticleSystem::spawnBurst(const World& world, int x, int y, int z, ItemId block) {
    if (block == AIR) return;
    // 4x4x4 grid of pieces, like the block falling apart.
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) {
                if (random() < 0.55f) continue;  // about 29 pieces
                Vec3 local((i + 0.5f) / 4, (j + 0.5f) / 4, (k + 0.5f) / 4);
                Vec3 out = (local - Vec3(0.5f, 0.5f, 0.5f)) * 4.0f;
                Vec3 vel(out.x + (random() - 0.5f), 1.5f + out.y + random() * 2, out.z + (random() - 0.5f));
                add(world, Vec3((float)x, (float)y, (float)z) + local, vel, block, 0.5f + random() * 0.6f);
            }
}

void ParticleSystem::spawnConfetti(const World& world, const Vec3& centre, int count) {
    for (int i = 0; i < count; ++i) {
        Vec3 pos = centre + Vec3((random() - 0.5f) * 10, 3 + random() * 3, (random() - 0.5f) * 10);
        add(world, pos, Vec3((random() - 0.5f) * 1.5f, 1 + random() * 2, (random() - 0.5f) * 1.5f), AIR, 2.5f + random());
        // A coloured 4x4 square of the confetti tile, glowing a little.
        Particle& p = particles_.back();
        int px = (int)(random() * 3) * 4, py = (int)(random() * 3) * 4;
        p.u0 = (T_CONFETTI % kAtlasCols) * kTile + px / 16.0f * kTile;
        p.v0 = (T_CONFETTI / kAtlasCols) * kTile + py / 16.0f * kTile;
        p.u1 = p.u0 + 4 / 16.0f * kTile;
        p.v1 = p.v0 + 4 / 16.0f * kTile;
        p.size = 0.06f;
        p.light = 1.0f;
        p.fall = 0.06f;
    }
}

void ParticleSystem::update(const World& world, float dt) {
    for (Particle& p : particles_) {
        p.life -= dt;
        p.vel.y -= kGravity * p.fall * dt;
        if (p.fall < 1) p.vel.y = std::max(p.vel.y, -1.2f);
        Vec3 next = p.pos + p.vel * dt;
        if (isSolid(world.getBlock((int)std::floor(next.x), (int)std::floor(next.y), (int)std::floor(next.z)))) {
            // Landed or hit a wall: stop and slide a little.
            if (isSolid(world.getBlock((int)std::floor(p.pos.x), (int)std::floor(next.y), (int)std::floor(p.pos.z)))) p.vel.y = 0;
            p.vel.x *= 0.3f;
            p.vel.z *= 0.3f;
            next = p.pos + Vec3(p.vel.x, 0, p.vel.z) * dt;
            if (isSolid(world.getBlock((int)std::floor(next.x), (int)std::floor(next.y), (int)std::floor(next.z)))) next = p.pos;
        }
        p.pos = next;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.life <= 0; }),
                     particles_.end());
}

void ParticleSystem::buildVertices(const Vec3& right, const Vec3& up, std::vector<Vertex>& out) const {
    out.clear();
    out.reserve(particles_.size() * 6);
    for (const Particle& p : particles_) {
        // Shrink during the last third of the life instead of fading (alpha-tested texture).
        float s = p.size * std::min(1.0f, p.life / (p.maxLife * 0.33f));
        Vec3 r = right * s, u = up * s;
        Vec3 a = p.pos - r - u, b = p.pos + r - u, c = p.pos + r + u, d = p.pos - r + u;
        Vertex va{a.x, a.y, a.z, p.u0, p.v1, p.light}, vb{b.x, b.y, b.z, p.u1, p.v1, p.light};
        Vertex vc{c.x, c.y, c.z, p.u1, p.v0, p.light}, vd{d.x, d.y, d.z, p.u0, p.v0, p.light};
        out.insert(out.end(), {va, vb, vc, va, vc, vd});
    }
}

}  // namespace vc

#pragma once
#include <cstdint>

namespace vc {

// Seeded 2D/3D gradient (Perlin) noise with fractal helpers.
class Noise {
public:
    explicit Noise(uint32_t seed);
    float perlin2(float x, float y) const;
    float perlin3(float x, float y, float z) const;
    float fbm2(float x, float y, int octaves) const;

private:
    uint8_t perm_[512];
};

// Cheap deterministic hash for per-column decisions (trees etc.).
inline uint32_t hash3(int x, int y, int z, uint32_t seed) {
    uint32_t h = seed ^ (uint32_t)x * 0x8da6b343u ^ (uint32_t)y * 0xd8163841u ^ (uint32_t)z * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

}  // namespace vc

#pragma once
#include <vector>

#include "World.h"

namespace vc {

struct Vertex {
    float x, y, z;
    float u, v;
    float light;      // sky light and shading, dimmed at night
    float block = 0;  // 1.4.0: light from lamps and other glowing blocks, the same day and night
};

struct ChunkMesh {
    std::vector<Vertex> opaque;       // solid + alpha-tested (leaves, glass), lava
    std::vector<Vertex> translucent;  // water and portals, drawn after opaque with blending
};

// A chunk can only be meshed once its four neighbours exist, otherwise
// border faces would be wrong.
bool canMesh(const World& world, int cx, int cz);
void buildChunkMesh(const World& world, const Chunk& chunk, ChunkMesh& out);

}  // namespace vc

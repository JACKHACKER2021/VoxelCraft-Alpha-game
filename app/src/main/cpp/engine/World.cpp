#include <cstring>
#include "World.h"
#include "BlockShapes.h"

#include <zlib.h>

#include "WorldStructures.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "Blocks.h"
#include "Math.h"

namespace vc {

void Chunk::recalcHeight(int x, int z) {
    int y = kChunkHeight - 1;
    while (y > 0 && !isOpaque(get(x, y, z))) --y;
    heightmap[x * kChunkSize + z] = (uint8_t)(y + 1);
}

void Chunk::recalcHeightmap() {
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) recalcHeight(x, z);
}

World::World(uint32_t seed, Dimension dim, const GenConfig& config)
    : seed_(seed), dim_(dim), config_(config), terrain_(seed), detail_(seed * 31 + 7), caves_(seed * 17 + 3),
      temperature_(seed * 7 + 11), humidity_(seed * 13 + 5) {
    deep_ = kFeatures220 && dim == Dimension::Overworld && config.generation >= 4 && !config.oldWorld && !config.flat && config.boundedChunks <= 0 &&
            !config.biomes.empty();
}

Chunk* World::getChunk(int cx, int cz) const {
    auto it = chunks_.find(key(cx, cz));
    return it == chunks_.end() ? nullptr : it->second.get();
}

ItemId World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= kChunkHeight) return AIR;
    Chunk* c = getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize));
    return c ? c->get(floorMod(x, kChunkSize), y, floorMod(z, kChunkSize)) : (ItemId)AIR;
}

int World::skyHeight(int x, int z) const {
    Chunk* c = getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize));
    return c ? c->heightmap[floorMod(x, kChunkSize) * kChunkSize + floorMod(z, kChunkSize)] : 0;
}

void World::markDirty(int cx, int cz) {
    if (Chunk* c = getChunk(cx, cz)) c->dirty = true;
}

bool World::setBlock(int x, int y, int z, ItemId id) {
    if (y < 0 || y >= kChunkHeight) return false;
    int cx = floorDiv(x, kChunkSize), cz = floorDiv(z, kChunkSize);
    Chunk* c = getChunk(cx, cz);
    if (!c) return false;
    int lx = floorMod(x, kChunkSize), lz = floorMod(z, kChunkSize);
    if (editLog && c->get(lx, y, lz) != id) editLog->push_back({x, y, z, id});
    if (changeLog && c->get(lx, y, lz) != id && changeLog->size() < 65536) changeLog->push_back({x, y, z});
    // A lamp lights up to 15 blocks away: the neighbouring chunks' meshes change too.
    if (lightLevel(id) > 0 || lightLevel(c->get(lx, y, lz)) > 0)
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz) markDirty(cx + dx, cz + dz);
    c->set(lx, y, lz, id);
    c->recalcHeight(lx, lz);
    c->dirty = true;
    c->modified = true;
    // Faces on chunk borders belong to the neighbour's mesh too.
    if (lx == 0) markDirty(cx - 1, cz);
    if (lx == kChunkSize - 1) markDirty(cx + 1, cz);
    if (lz == 0) markDirty(cx, cz - 1);
    if (lz == kChunkSize - 1) markDirty(cx, cz + 1);
    return true;
}

World::Column World::column(int x, int z) const {
    Column col;
    if (config_.oldWorld || (config_.boundedChunks > 0 && dim_ == Dimension::Overworld)) {
        // The height formula of VoxelCraft 0.1-0.3, unchanged (see generateOldWorld).
        const float continent = terrain_.fbm2(x / 256.0f, z / 256.0f, 4);
        const float hills = detail_.fbm2(x / 64.0f, z / 64.0f, 4);
        col.height = std::clamp((int)(64 + continent * 28 + hills * 10 * (0.5f + std::max(0.0f, continent))), 4, kGenHeight - 10);
        col.biome = config_.biomes.empty() ? nullptr : &config_.biomes.front();
        return col;
    }
    if (config_.flat || config_.biomes.empty()) {
        ItemId layers[8];
        col.height = config_.flat ? flatLayers(config_.flatPreset, layers) - 1 : config_.overworld.seaLevel + 2;
        col.biome = config_.biomes.empty() ? nullptr : &config_.biomes.front();
        return col;
    }
    // Climate map: each biome sits at a (temperature, humidity) point; columns
    // blend the nearby biomes so heights change smoothly at the borders.
    const float scale = config_.overworld.biomeScale;
    float t = std::clamp(0.5f + temperature_.fbm2(x / scale, z / scale, 3) * 1.7f, 0.0f, 1.0f);
    float hum = std::clamp(0.5f + humidity_.fbm2(x / scale + 50, z / scale - 50, 3) * 1.7f, 0.0f, 1.0f);
    float total = 0, base = 0, variation = 0, best = 1e9f;
    for (const BiomeDef& b : config_.biomes) {
        float d2 = (t - b.temperature) * (t - b.temperature) + (hum - b.humidity) * (hum - b.humidity);
        float w = 1.0f / (d2 * d2 + 1e-6f);
        total += w;
        base += w * b.height;
        variation += w * b.variation;
        if (d2 < best) {
            best = d2;
            col.biome = &b;
        }
    }
    base /= total;
    variation /= total;
    col.temperature = t;
    float continent = terrain_.fbm2(x / 256.0f, z / 256.0f, 4);
    float hills = detail_.fbm2(x / 64.0f, z / 64.0f, 4);
    col.height = std::clamp((int)(base + continent * 10 + hills * variation), 4, kGenHeight - 10);
    return col;
}

int World::surfaceHeight(int x, int z) const { return column(x, z).height + yShift(); }

const BiomeDef* World::biomeAt(int x, int z) const {
    return dim_ == Dimension::Overworld && !config_.biomes.empty() ? column(x, z).biome : nullptr;
}

bool World::setBlockLoading(int x, int y, int z, ItemId id) {
    if (y < 0 || y >= kChunkHeight) return false;
    int cx = floorDiv(x, kChunkSize), cz = floorDiv(z, kChunkSize);
    if (!getChunk(cx, cz)) generateChunk(cx, cz);  // saved when it unloads again, since it is modified
    return setBlock(x, y, z, id);
}

Chunk* World::generateChunk(int cx, int cz) {
    auto chunk = std::make_unique<Chunk>(cx, cz);
    if (!loadChunk(*chunk)) {
        if (dim_ == Dimension::Nether && config_.oldWorld) generateOldNether(*chunk);
        else if (dim_ == Dimension::Nether) generateNether(*chunk);
        else if (dim_ == Dimension::Void) generateVoid(*chunk);
        else if (dim_ == Dimension::Celestial) generateCelestial(*chunk);
        else generateOverworld(*chunk);
    }
    chunk->recalcHeightmap();
    Chunk* raw = chunk.get();
    chunks_[key(cx, cz)] = std::move(chunk);
    markDirty(cx - 1, cz); markDirty(cx + 1, cz);
    markDirty(cx, cz - 1); markDirty(cx, cz + 1);
    return raw;
}

// 1.8.0 Old World: the Overworld generator of VoxelCraft 0.1-0.3 (commit 5686078, "Menus, world
// saves ..."), carried over line by line: one height formula from two noises, stone, three blocks
// of dirt, grass, sand on the shores, sea at 62, worm caves, and round oaks in forest patches.
// The noises are seeded the same way as back then, so a seed gives the same land as in 0.3.
// Not part of it (it did not exist yet): biomes, ores, caverns, structures, deepstone.
// 1.9.5 build 2: the classic bounded world. Inside the square: the old rolling terrain covered in
// an oak forest, with lakes, sandy shores and gravel on the lake beds and in patches; coal and
// iron in the stone. On the edge a wall of bedrock from the bottom to the top; outside nothing.
bool World::insideBounds(int x, int z) const {
    const int half = config_.boundedChunks * kChunkSize / 2;
    return config_.boundedChunks <= 0 || (x >= -half && x < half && z >= -half && z < half);
}

void World::generateBounded(Chunk& c) {
    constexpr int kSea = 62;
    const int half = config_.boundedChunks * kChunkSize / 2;
    const int cx = c.cx, cz = c.cz;
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            const bool wall = (wx == -half - 1 || wx == half || wz == -half - 1 || wz == half) && wx >= -half - 1 && wx <= half && wz >= -half - 1 &&
                              wz <= half;
            if (wall) {
                for (int y = 0; y < kChunkHeight; ++y) c.set(x, y, z, WORLD_END);
                continue;
            }
            if (!insideBounds(wx, wz)) continue;
            const int h = std::min(column(wx, wz).height, kGenHeight - 20);
            const bool shore = h <= kSea + 1;
            const bool gravelPatch = detail_.perlin2(wx / 11.0f + 40, wz / 11.0f - 70) > 0.45f;
            for (int y = 0; y <= std::max(h, kSea); ++y) {
                ItemId id = AIR;
                if (y == 0) id = WORLD_END;
                else if (y < h - 3) {
                    id = STONE;
                    const uint32_t r = hash3(wx, y, wz, seed_ ^ 0x0EEu);
                    if (r % 90 == 0) id = COAL_ORE;
                    else if (r % 160 == 1 && y < 50) id = IRON_ORE;
                } else if (y < h) {
                    id = shore ? (h < kSea - 1 && gravelPatch ? GRAVEL : SAND) : DIRT;
                } else if (y == h) {
                    id = h < kSea ? (gravelPatch ? GRAVEL : SAND) : shore ? SAND : gravelPatch && h < kSea + 4 ? GRAVEL : GRASS;
                } else if (y <= kSea) {
                    id = WATER;
                }
                c.set(x, y, z, id);
            }
        }
    // The oak forest: trees everywhere on grass, away from the wall.
    for (int x = 2; x <= 13; ++x)
        for (int z = 2; z <= 13; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            if (!insideBounds(wx - 3, wz - 3) || !insideBounds(wx + 3, wz + 3)) continue;
            const int h = std::min(column(wx, wz).height, kGenHeight - 20);
            const uint32_t r = hash3(wx, 1, wz, seed_ ^ 0x0A4Eu);
            if (c.get(x, h, z) != GRASS) continue;
            if (r % 100 >= 6) {
                if (r % 100 < 30 && c.get(x, h + 1, z) == AIR) c.set(x, h + 1, z, r % 7 == 0 ? (r % 2 ? DANDELION : POPPY) : TALL_GRASS);
                continue;
            }
            const int trunk = 4 + (int)((r >> 8) % 3);
            for (int dy = 1; dy <= trunk; ++dy) c.set(x, h + dy, z, LOG);
            for (int dy = trunk - 1; dy <= trunk + 2; ++dy) {
                const int rr = dy >= trunk + 1 ? 1 : 2;
                for (int dx = -rr; dx <= rr; ++dx)
                    for (int dz = -rr; dz <= rr; ++dz) {
                        if (std::abs(dx) == rr && std::abs(dz) == rr && ((r >> (dx + dz + 10)) & 1)) continue;
                        if (c.get(x + dx, h + dy, z + dz) == AIR) c.set(x + dx, h + dy, z + dz, LEAVES);
                    }
            }
        }
}

void World::generateOldWorld(Chunk& c) {
    constexpr int kOldSeaLevel = 62;
    const int cx = c.cx, cz = c.cz;
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            const int h = column(wx, wz).height;
            const bool beach = h <= kOldSeaLevel + 1;
            for (int y = 0; y < kGenHeight; ++y) {
                ItemId id = AIR;
                if (y == 0 || (y < 4 && (hash3(wx, y, wz, seed_) & 1))) id = BEDROCK;
                else if (y < h - 3) id = STONE;
                else if (y < h) id = beach ? SAND : DIRT;
                else if (y == h) id = beach ? SAND : GRASS;
                else if (y <= kOldSeaLevel) id = WATER;
                // Worm-like caves carved by 3D noise, kept away from the sea floor.
                if (id != AIR && id != WATER && id != BEDROCK && y < h - 4 && y > 4) {
                    const float n = caves_.perlin3(wx / 24.0f, y / 16.0f, wz / 24.0f);
                    if (std::fabs(n) < 0.06f) id = AIR;
                }
                c.set(x, y, z, id);
            }
        }
    }
    // Trees in a second pass. (0.3 placed them in the same pass, so the next columns cut half of
    // each crown away; that bug is the one thing left out.)
    for (int x = 2; x <= 13; ++x) {
        for (int z = 2; z <= 13; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            const int h = column(wx, wz).height;
            if (h > kOldSeaLevel + 1) {
                const uint32_t r = hash3(wx, 0, wz, seed_ ^ 0x7EEEu);
                const float forest = detail_.perlin2(wx / 48.0f + 100, wz / 48.0f);
                if (r % 100 < (forest > 0.2f ? 4u : 1u) && c.get(x, h, z) == GRASS) {
                    // The old oak: a 4-6 block trunk and a crown with ragged corners.
                    const int y = h + 1, trunk = 4 + (int)((r >> 8) % 3);
                    if (y + trunk + 2 >= kGenHeight) continue;
                    c.set(x, y - 1, z, DIRT);
                    for (int dy = trunk - 2; dy <= trunk + 1; ++dy) {
                        const int rr = dy >= trunk ? 1 : 2;
                        for (int dx = -rr; dx <= rr; ++dx)
                            for (int dz = -rr; dz <= rr; ++dz) {
                                const bool corner = std::abs(dx) == rr && std::abs(dz) == rr;
                                if (corner && (dy == trunk + 1 || ((r >> (dx + dz + 8)) & 1))) continue;
                                if (c.get(x + dx, y + dy, z + dz) == AIR) c.set(x + dx, y + dy, z + dz, LEAVES);
                            }
                    }
                    for (int dy = 0; dy < trunk; ++dy) c.set(x, y + dy, z, LOG);
                }
            }
        }
    }
}

// 1.8.0 Old World: the Nether of VoxelCraft 0.2-0.3 (commit feed712), not the Cornerstone
// Dimension: open netherrack caverns over a lava sea at 31, bedrock floor and roof, soul sand
// patches, gravel at the shore, glowstone hanging from the ceilings. Blocks that no longer exist
// are the ones old saves turn into (upgradeLegacyBlock): soul sand -> netherrack, quartz ore ->
// powerstone ore, glowstone -> powerstone light.
void World::generateOldNether(Chunk& c) {
    constexpr int kOldLavaLevel = 31;
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = c.cx * kChunkSize + x, wz = c.cz * kChunkSize + z;
            const float soul = detail_.perlin2(wx / 20.0f, wz / 20.0f);
            for (int y = 0; y < kGenHeight; ++y) {
                ItemId id;
                const uint32_t h = hash3(wx, y, wz, seed_ ^ 0xBEEFu);
                if (y == 0 || y == kGenHeight - 1 || (y < 5 && (int)(h & 3) < 4 - y) ||
                    (y > kGenHeight - 6 && (int)(h & 3) < y - (kGenHeight - 6)))
                    id = BEDROCK;
                else {
                    const float n = caves_.perlin3(wx / 32.0f, y / 20.0f, wz / 32.0f) + 0.5f * terrain_.perlin3(wx / 12.0f, y / 10.0f, wz / 12.0f);
                    const float edge = std::max(0.0f, (24 - y) / 24.0f) + std::max(0.0f, (y - 96) / 28.0f);
                    if (n + edge * 1.2f > 0.05f) id = (h % 120 == 0) ? upgradeLegacyBlock(153) : (ItemId)NETHERRACK;
                    else id = y <= kOldLavaLevel ? (ItemId)LAVA : (ItemId)AIR;
                }
                c.set(x, y, z, id);
            }
            for (int y = kOldLavaLevel + 1; y < kGenHeight - 2; ++y) {
                if (c.get(x, y, z) != NETHERRACK || c.get(x, y + 1, z) != AIR) continue;
                if (soul > 0.25f) {
                    c.set(x, y, z, upgradeLegacyBlock(88));
                    if (c.get(x, y - 1, z) == NETHERRACK) c.set(x, y - 1, z, upgradeLegacyBlock(88));
                } else if (y <= kOldLavaLevel + 3 && soul < -0.35f) {
                    c.set(x, y, z, GRAVEL);
                }
            }
        }
    const uint32_t r = hash3(c.cx, 0, c.cz, seed_ ^ 0x6105u);
    for (int i = 0; i < (int)(r % 3); ++i) {
        const int x = 3 + (int)((r >> (4 + i * 6)) % 10), z = 3 + (int)((r >> (7 + i * 6)) % 10);
        int y = kGenHeight - 8;
        while (y > 40 && !(c.get(x, y, z) == AIR && isSolid(c.get(x, y + 1, z)))) --y;
        if (y <= 40) continue;
        for (int k = 0; k < 40; ++k) {
            const uint32_t q = hash3(c.cx * 97 + i, k, c.cz, seed_);
            const int dx = (int)(q % 5) - 2, dz = (int)((q >> 3) % 5) - 2, dy = -(int)((q >> 6) % 5);
            if (c.get(x + dx, y + dy, z + dz) == AIR) c.set(x + dx, y + dy, z + dz, upgradeLegacyBlock(89));
        }
    }
}

void World::generateOverworld(Chunk& c) {
    if (config_.oldWorld) {
        generateOldWorld(c);
        return;
    }
    if (config_.boundedChunks > 0) {
        generateBounded(c);
        return;
    }
    const int cx = c.cx, cz = c.cz;
    const int sea = config_.overworld.seaLevel;
    if (config_.biomes.empty() && !config_.flat) return;
    Column cols[kChunkSize][kChunkSize];
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            if (config_.flat) {
                // Superflat: bedrock, two dirt layers and grass, like the classic flat world;
                // 1.9.0: or the layers of the picked look (desert, snow, stone, nether, void).
                ItemId layers[8];
                const int n = flatLayers(config_.flatPreset, layers);
                for (int y = 0; y < n; ++y) c.set(x, y, z, layers[y]);
                continue;
            }
            Column col = column(wx, wz);
            const BiomeDef& b = *col.biome;
            int h = col.height;
            // Deep cave lakes are tar instead of water in some regions.
            const bool tarRegion = config_.overworld.caverns && detail_.perlin2(wx / 110.0f + 700.0f, wz / 110.0f - 300.0f) > 0.32f;
            bool shore = h <= sea + 1;
            ItemId top = h < sea ? b.underwater : shore ? b.beach : b.surface;
            ItemId under = h < sea ? b.underwater : shore ? b.beach : b.filler;
            if (b.peakBlock && h > sea + 8) {
                // Steep mountain sides are bare rock: compare with the neighbouring columns.
                int steep = 0;
                for (auto [dx, dz] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
                    steep = std::max(steep, std::abs(column(wx + dx, wz + dz).height - h));
                if (steep >= 3) top = under = b.peakBlock;
            }
            for (int y = 0; y < kGenHeight; ++y) {
                ItemId id = AIR;
                if (y == 0) id = WORLD_END;
                else if (y < 4 && (hash3(wx, y, wz, seed_) & 1)) id = BEDROCK;
                else if (y < h - b.fillerDepth) id = STONE;
                else if (y < h) id = under;
                else if (y == h) id = top;
                else if (y <= sea) id = ((b.freeze || col.temperature < 0.12f) && y == sea) ? (ItemId)ICE : (ItemId)WATER;
                // 2.2.0: patches of thin ice on frozen water, which give way under you.
                if (kFeatures220 && id == ICE && detail_.perlin2(wx / 9.0f + 77, wz / 9.0f - 31) > 0.25f) id = THIN_ICE;
                // High mountains: hardened stone above the peak height, snow on the very top.
                if (b.peakBlock && y > b.peakHeight && y <= h && id != BEDROCK) id = b.peakBlock;
                if (y == h && h > b.snowHeight) id = SNOW_BLOCK;
                // 2.2.0: drifts of dried snow on snowy ground, two blocks deep.
                if (kFeatures220 && (y == h || y == h - 1) && (id == SNOW_BLOCK || (b.freeze && y == h && id == top && top != ICE)) &&
                    detail_.perlin2(wx / 14.0f - 220, wz / 14.0f + 90) > 0.42f)
                    id = DRIED_SNOW;
                // 1.6.0 (generation 4): the deep layer of deepstone, with a wavy top. 1.7.0: about 40
                // blocks thick, so the deep caves have room.
                const bool gen4 = config_.generation >= 4;
                if (gen4 && id == STONE && y < 40 + (int)(detail_.perlin2(wx / 23.0f + 41.0f, wz / 23.0f) * 4))
                    id = DEEPSTONE;

                // Worm-like caves carved by 3D noise, kept away from the sea floor.
                bool carved = false;
                // 1.5.7 (generation 2): wider tunnels in two crossing networks, and sinkholes where
                // the tunnels break through to the surface so caves can be walked into.
                const bool gen2 = config_.generation >= 2;
                // 1.6.0 (generation 4): more sinkholes, and now and then a great shaft straight down into
                // a deep hall. Only on dry land away from the shore, so the surface is not torn up.
                const bool landOk = h > sea + 3 && !b.peakBlock;
                const bool sinkhole = gen2 && h > sea + 2 && caves_.perlin2(wx / 64.0f + 900.0f, wz / 64.0f - 300.0f) > (gen4 ? 0.36f : 0.42f);
                const float shaftN = gen4 && landOk ? caves_.perlin2(wx / 44.0f - 1500.0f, wz / 44.0f + 2100.0f) : -1.0f;
                const bool shaft = shaftN > 0.6f;
                const int roof = sinkhole || shaft ? h + 1 : h - 4;
                if (config_.overworld.caves && id != AIR && id != WATER && id != BEDROCK && y < roof && y > 4) {
                    float n = caves_.perlin3(wx / 24.0f, y / 16.0f, wz / 24.0f);
                    // The shaft: open from the surface down to the deep halls, its walls ragged.
                    if (shaft && y > 12 && shaftN + caves_.perlin3(wx / 6.0f, y / 6.0f, wz / 6.0f) * 0.05f > 0.62f) id = AIR, carved = true;
                    const float size = config_.overworld.caveSize * (gen4 ? 1.9f : gen2 ? 1.35f : 1.0f) *
                                       (sinkhole && y > h - 8 ? 1.6f : 1.0f);
                    if (std::fabs(n) < size) id = AIR, carved = true;
                    if (gen2 && !carved && y < h - 6) {
                        float m = caves_.perlin3(wx / 30.0f - 170.0f, y / 13.0f + 40.0f, wz / 30.0f + 260.0f);
                        if (std::fabs(m) < config_.overworld.caveSize * 1.1f) id = AIR, carved = true;
                    }
                }
                // Caves 2.0: big caverns from a slow noise, wider the deeper they are. Their floors
                // below kCaveLakeLevel fill with water, or with tar in some regions.
                if (config_.overworld.caverns && !carved && id != AIR && id != WATER && id != BEDROCK && y < h - 10 && y > 5) {
                    const float depth = 1.0f - (float)y / (float)std::max(h, 1);
                    float n = caves_.perlin3(wx / 52.0f + 500.0f, y / (gen2 ? 28.0f : 22.0f), wz / 52.0f);
                    if (n > 0.43f - depth * (gen2 ? 0.2f : 0.14f)) id = AIR, carved = true;
                    // 1.6.0: great halls, tall and wide, between the deep layer and the surface.
                    if (gen4 && !carved && y > 8 && y < h - 14) {
                        const float g = caves_.perlin3(wx / 70.0f - 1300.0f, y / 30.0f, wz / 70.0f + 800.0f);
                        if (g > 0.44f - depth * 0.14f) id = AIR, carved = true;
                    }
                }
                if (carved && config_.overworld.caverns && y <= kCaveLakeLevel)
                    id = tarRegion ? (ItemId)TAR : (ItemId)WATER;
                // 1.5.7: the deepest hollows glow with lava pools.
                if (carved && gen2 && y <= 7 && caves_.perlin2(wx / 40.0f - 700.0f, wz / 40.0f) > -0.1f) id = LAVA;
                c.set(x, y, z, id);
            }
            cols[x][z] = col;
        }
    }
    if (config_.flat) return;
    // Vegetation in a second pass: a tree reaches two blocks into its neighbours' columns,
    // and filling those columns afterwards used to wipe half of every crown away.
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            const BiomeDef& b = *cols[x][z].biome;
            int h = cols[x][z].height;
            bool shore = h <= sea + 1;
            bool tree = false;
            if (!shore && b.treeDensity > 0 && x >= 2 && x <= 13 && z >= 2 && z <= 13) {
                uint32_t r = hash3(wx, 0, wz, seed_ ^ 0x7EEEu);
                if ((r % 100000) < (uint32_t)(b.treeDensity * 100000) && isSolid(c.get(x, h, z)) && c.get(x, h + 1, z) == AIR) {
                    if (b.treeShape == 1) placePine(c, x, h + 1, z, r, b.trunk, b.leaves);
                    // 1.8.0 Dappled Forest: each tree gets red, orange or yellow leaves.
                    else placeTree(c, x, h + 1, z, r, b.trunk, b.leaves == RED_LEAVES ? (ItemId)(RED_LEAVES + (r >> 11) % 3) : b.leaves);
                    tree = true;
                }
            }
            // Cacti on sand, 1 to 3 blocks tall, with air around them like in the desert of the classic game.
            if (!tree && b.cactusDensity > 0 && c.get(x, h, z) == SAND && h + 4 < kGenHeight && x >= 1 && x <= 14 &&
                z >= 1 && z <= 14) {
                uint32_t r = hash3(wx, 3, wz, seed_ ^ 0xCAC7u);
                bool clear = c.get(x + 1, h + 1, z) == AIR && c.get(x - 1, h + 1, z) == AIR && c.get(x, h + 1, z + 1) == AIR &&
                             c.get(x, h + 1, z - 1) == AIR && c.get(x, h + 1, z) == AIR;
                if (clear && (r % 100000) < (uint32_t)(b.cactusDensity * 100000)) {
                    int tall = 1 + (int)((r >> 17) % 3);
                    for (int k = 1; k <= tall; ++k) c.set(x, h + k, z, CACTUS);
                    tree = true;
                }
            }
            // Cold lands lie under a thin cover of snow.
            const bool cold = b.freeze || cols[x][z].temperature < 0.12f;
            if (!tree && cold && h + 1 < kGenHeight && isOpaque(c.get(x, h, z)) && c.get(x, h + 1, z) == AIR) {
                c.set(x, h + 1, z, SNOW_LAYER);
                continue;
            }
            // Tall grass and flowers grow on grass with air above (not under leaves of this column's tree).
            const ItemId ground = c.get(x, h, z);
            if (!tree && h + 1 < kGenHeight && ground == GRASS && c.get(x, h + 1, z) == AIR) {
                uint32_t r = hash3(wx, 1, wz, seed_ ^ 0x9A55u) % 100000;
                // Flowers grow in patches: a slow noise decides where they bloom.
                float patch = detail_.perlin2(wx / 18.0f + 300, wz / 18.0f) > 0.1f ? 3.0f : 0.3f;
                const uint32_t pick = hash3(wx, 2, wz, seed_) % 100;
                if (r < (uint32_t)(b.flowerDensity * patch * 100000)) {
                    // The Grasser flower grows in the Cornerstone Dimension only (1.5.1).
                    c.set(x, h + 1, z, (pick & 1) ? (ItemId)DANDELION : (ItemId)POPPY);
                } else if (r < (uint32_t)((b.flowerDensity * patch + b.grassDensity) * 100000)) {
                    // Grass comes in kinds now: ferns grow in the woods.
                    const bool woods = b.treeDensity >= 0.02f;
                    c.set(x, h + 1, z, woods && pick < 35 ? (ItemId)FERN : (ItemId)TALL_GRASS);
                }
            }
        }
    }
    // Dripstone in the caverns of some regions: spikes hanging from the ceilings and growing
    // from the floors, on a base of dripstone blocks.
    if (config_.overworld.caverns) {
        for (int x = 0; x < kChunkSize; ++x)
            for (int z = 0; z < kChunkSize; ++z) {
                const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
                if (detail_.perlin2(wx / 70.0f - 900.0f, wz / 70.0f + 120.0f) < 0.12f) continue;
                const int top = cols[x][z].height - 10;
                for (int y = kCaveLakeLevel + 1; y < top; ++y) {
                    if (c.get(x, y, z) != AIR) continue;
                    const uint32_t r = hash3(wx, y, wz, seed_ ^ 0xD819u) % 100;
                    if (r < 9 && c.get(x, y + 1, z) == STONE) {
                        c.set(x, y + 1, z, DRIPSTONE_BLOCK);
                        c.set(x, y, z, DRIPSTONE_DOWN);
                    } else if (r >= 94 && c.get(x, y - 1, z) == STONE) {
                        c.set(x, y - 1, z, DRIPSTONE_BLOCK);
                        c.set(x, y, z, DRIPSTONE_UP);
                    }
                }
            }
    }
    if (config_.generation >= 4 && config_.overworld.caves) decorateCaves(c, cols);
    uint32_t salt = 0xC0A1u;
    for (const OreDef& ore : config_.overworld.ores) placeOres(c, ore, salt += 0x9E37u);
    if (config_.generation >= 4) {
        // Hardium: small, rare veins, only in deepstone.
        OreDef hardium;
        hardium.block = HARDIUM_ORE;
        hardium.veins = 3;
        hardium.size = 4;
        hardium.minY = 3;
        hardium.maxY = 30;
        placeOres(c, hardium, 0x4A2Du);
    }
    stampStructures(c);
    if (deep_) deepenOverworld(c);
}

// 2.2.0 "Cornerstone and Frost": the deeper Overworld. The generators build the world 128 blocks high;
// here every column is lifted by kDeepShift and the room below is filled with the deep layers.
void World::liftColumns(Chunk& c) const {
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            ItemId* col = &c.blocks[Chunk::index(x, 0, z)];
            std::memmove(col + kDeepShift, col, sizeof(ItemId) * (kChunkHeight - kDeepShift));
            std::fill(col, col + kDeepShift, (ItemId)AIR);
            // The old floor (the world's end and bedrock) becomes deepstone: the world goes on below it.
            for (int y = kDeepShift; y < kDeepShift + 5; ++y)
                if (col[y] == BEDROCK || col[y] == WORLD_END) col[y] = DEEPSTONE;
        }
}

void World::deepenOverworld(Chunk& c) {
    liftColumns(c);
    const int cx = c.cx, cz = c.cz;
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            // Layers: deepstone above, then a wavy border to abyssal rock, with thin bands of each in the other.
            const int border = 24 + (int)(detail_.perlin2(wx / 31.0f - 17, wz / 31.0f + 5) * 6);
            const int band1 = 40 + (int)(detail_.perlin2(wx / 19.0f + 60, wz / 19.0f) * 3);
            const int band2 = 12 + (int)(detail_.perlin2(wx / 17.0f, wz / 17.0f - 80) * 3);
            for (int y = 0; y < kDeepShift; ++y) {
                ItemId id = y < border ? (ItemId)ABYSSAL_ROCK : (ItemId)DEEPSTONE;
                if (y == band1 || y == band1 + 1) id = ABYSSAL_ROCK;
                if (y == band2) id = DEEPSTONE;
                if (y < 4 && (hash3(wx, y, wz, seed_ ^ 0xDEE9u) % 3) < (uint32_t)(4 - y)) id = BEDROCK;
                if (y == 0) id = WORLD_END;  // the very bottom, as before
                c.set(x, y, z, id);
            }
            // Caves: winding tunnels everywhere and great halls in the abyssal rock; lava fills the lowest floors.
            for (int y = 4; y < kDeepShift + 14; ++y) {
                const ItemId here = c.get(x, y, z);
                if (here != DEEPSTONE && here != ABYSSAL_ROCK && here != STONE) continue;
                const float t1 = caves_.perlin3(wx / 28.0f, y / 16.0f, wz / 28.0f);
                const float t2 = caves_.perlin3(wx / 28.0f + 300, y / 16.0f, wz / 28.0f - 200);
                const bool tunnel = std::fabs(t1) < 0.075f && std::fabs(t2) < 0.12f;
                const float hall = caves_.perlin3(wx / 45.0f - 500, y / 22.0f, wz / 45.0f + 700) - std::fabs(y - 20) / 60.0f;
                const bool cavern = y < kDeepShift - 4 && hall > 0.17f;
                if (tunnel || cavern) c.set(x, y, z, y <= 9 ? (ItemId)LAVA : (ItemId)AIR);
            }
        }
    // Cave walls: obsidian where lava meets rock, moss and glowing fungus on cavern floors.
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            for (int y = 5; y < kDeepShift; ++y) {
                const ItemId here = c.get(x, y, z);
                if (here != DEEPSTONE && here != ABYSSAL_ROCK) continue;
                const ItemId above = c.get(x, y + 1, z);
                if (above == LAVA || (y > 0 && c.get(x, y - 1, z) == LAVA)) {
                    if (hash3(wx, y, wz, seed_ ^ 0x0B5Du) % 3 == 0) c.set(x, y, z, OBSIDIAN);
                } else if (above == AIR && y > 10) {
                    const uint32_t r = hash3(wx, y, wz, seed_ ^ 0xF00Du) % 100;
                    if (r < 14) c.set(x, y, z, MOSSY_DEEPSTONE);
                    else if (r < 18) c.set(x, y, z, FUNGAL_DEEPSTONE);
                }
            }
        }
    // Ores of the deep: hardium in both layers, rubies and steel mostly in the abyssal rock.
    OreDef ore;
    ore.block = HARDIUM_ORE, ore.veins = 4, ore.size = 5, ore.minY = 4, ore.maxY = kDeepShift - 2;
    placeOres(c, ore, 0xD31Au);
    ore.block = RUBY_ORE, ore.veins = 1, ore.size = 3, ore.minY = 4, ore.maxY = 30;
    placeOres(c, ore, 0xD31Bu);
    ore.block = STEEL_ORE, ore.veins = 1, ore.size = 4, ore.minY = 4, ore.maxY = kDeepShift - 2;
    placeOres(c, ore, 0xD31Cu);
    if (kFeatures230) stampBronzeHalls(c);  // 2.3.0
    if (kFeatures250) stampMetro(c);  // 2.5.0
}

// 2.3.0: the Bronze Halls, a big dungeon in the deep layers: 25 halls of bronze in a 5 x 5 grid joined by
// corridors, every kind of bronze (plain, cut, chiseled, grate) in every age (shiny, tarnished, rusty, the
// rust thicker the farther from the middle), bulbs in the ceilings, lava glowing under grate floors,
// spawners in the corner halls and chests of bronze loot; the treasury in the middle. A shaft with vines
// leads down from a small bronze marker on the surface. One in most regions of 400 x 400 blocks.
namespace {
constexpr int kHallRegion = 400, kHallCell = 21, kHallRoom = 15, kHallGrid = 5, kHallSize = kHallCell * (kHallGrid - 1) + kHallRoom;
constexpr int kHallFloor = 14, kHallHeight = 10;
}

namespace {
// 2.5.0: the shape of a metro station. Along x: the hall [0, L), tunnels T blocks out of each end.
// Across z: walls at 0 and W - 1, platforms of 3 and track trenches of 3 in turn (rails in the middle).
struct MetroShape {
    int L, T, tracks, W;
    explicit MetroShape(bool big) : L(big ? 90 : 56), T(big ? 64 : 48), tracks(big ? 4 : 2), W(5 + 6 * (big ? 4 : 2)) {}
    bool trench(int z) const { const int m = (z - 1) % 6; return z >= 1 && z < W - 1 && (m == 3 || m == 4 || m == 5) && (z - 1) / 6 < tracks; }
    bool rail(int z) const { return trench(z) && (z - 1) % 6 == 4; }
};
// The block of the metro at hall-local (x, y, z) (y 0 = floor), or kVoid where it leaves the rock alone.
ItemId metroBlock(const MetroShape& m, int x, int y, int z, uint32_t seed) {
    constexpr ItemId kVoid = Structure::kVoid;
    if (z < -6 || z >= m.W || y < 0 || y > 8 || x < -m.T || x >= m.L + m.T) return kVoid;
    const uint32_t r = hash3(x, y, z, seed);
    // The entrance shaft beside the hall (x 5..7, z -5..-1): handled with the land above in stampMetro.
    if (z < 0) {
        if (x >= 4 && x <= 8 && z >= -5) {
            const bool core = x >= 5 && x <= 7 && z >= -4 && z <= -2;
            if (y == 0) return STONE_BRICKS;
            if (core) return (z == -4 && x == 6 && y >= 2) ? (ItemId)VINES : (ItemId)AIR;
            if (x == 6 && z == -1 && (y == 2 || y == 3)) return AIR;  // the doorway into the shaft
            return STONE_BRICKS;
        }
        return kVoid;
    }
    if (x >= 0 && x < m.L) {
        // The hall.
        const bool wall = z == 0 || z == m.W - 1;
        if (wall) {
            if (x == 6 && z == 0 && (y == 2 || y == 3)) return AIR;  // the way to the shaft
            if (y == 4) return (ItemId)(CONCRETE + 11);               // a blue stripe on the tiles
            return r % 11 == 0 ? (ItemId)COBBLESTONE : r % 7 == 0 ? (ItemId)MOSSY_COBBLESTONE : (ItemId)STONE_BRICKS;
        }
        if (y == 8) {
            const bool lamp = x % 7 == 3 && !m.trench(z) && (z - 1) % 6 == 1;
            if (lamp) return r % 10 < 3 ? (ItemId)AIR : (ItemId)POWERSTONE_LIGHT;  // some lamps fell down
            return STONE_BRICKS;
        }
        if (y == 0) return m.trench(z) ? (ItemId)TRACK_BED : (ItemId)HARDENED_CONCRETE;
        if (y == 1) {
            if (m.rail(z)) return r % 9 == 0 ? (ItemId)GRAVEL : (ItemId)RAIL_EW;
            if (m.trench(z)) return AIR;
            return r % 13 == 0 ? (ItemId)HARDENED_CONCRETE : (ItemId)PLATFORM;
        }
        // Lattice pillars on the platforms between the tracks, rubble here and there.
        if (!m.trench(z) && (z - 1) % 6 == 1 && x % 9 == 4 && z > 1 && z < m.W - 2) return LATTICE;
        if (y == 2 && !m.trench(z) && r % 37 == 0) return GRAVEL;
        if (y == 7 && r % 23 == 0) return VINES;
        return AIR;
    }
    // The tunnels: one tube per track, collapsed at the far end.
    const int dist = x < 0 ? -x : x - m.L + 1;
    const int k = (z - 1) % 6;
    const int t = (z - 1) / 6;
    if (z < 1 || t >= m.tracks) return kVoid;
    const bool tube = k == 3 || k == 4 || k == 5;
    const bool side = k == 2 || k == 0;
    if (!tube && !side) return kVoid;
    if (dist > m.T - 6) return y <= 5 ? (ItemId)(tube ? GRAVEL : STONE_BRICKS) : kVoid;  // the cave-in
    if (side) return y <= 5 ? (ItemId)STONE_BRICKS : kVoid;
    if (y == 0) return TRACK_BED;
    if (y == 1 && k == 4) return r % 6 == 0 ? (ItemId)AIR : (ItemId)RAIL_EW;
    if (y == 5) return dist % 12 == 6 && k == 4 && r % 3 ? (ItemId)POWERSTONE_LIGHT : (ItemId)STONE_BRICKS;
    if (y > 5) return kVoid;
    return AIR;
}
}  // namespace

bool World::metroIn(int rx, int rz, int& ox, int& oz, bool& big) const {
    if (!kFeatures250 || dim_ != Dimension::Overworld || !config_.overworld.structures || config_.generation < 7) return false;
    const uint32_t h = hash3(rx, 2505, rz, seed_ ^ 0x3E7Fu);
    if (h % 100 >= 55) return false;
    big = (h >> 7) % 3 == 0;
    const MetroShape m(big);
    ox = rx * kMetroRegion + m.T + 8 + (int)((h >> 9) % (uint32_t)(kMetroRegion - m.L - 2 * m.T - 16));
    oz = rz * kMetroRegion + 16 + (int)((h >> 19) % (uint32_t)(kMetroRegion - m.W - 32));
    // The shaft must come up on dry land (not in a lake or the sea, nor on a peak).
    const Column top = column(ox + 6, oz - 3);
    if (!top.biome || top.biome->peakBlock || top.height <= config_.overworld.seaLevel + 1) return false;
    return true;
}

void World::stampMetro(Chunk& c) {
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize;
    for (int rx = floorDiv(x0, kMetroRegion) - 1; rx <= floorDiv(x0 + 15, kMetroRegion); ++rx)
        for (int rz = floorDiv(z0, kMetroRegion) - 1; rz <= floorDiv(z0 + 15, kMetroRegion); ++rz) {
            int ox, oz;
            bool big;
            if (!metroIn(rx, rz, ox, oz, big)) continue;
            const MetroShape m(big);
            if (ox + m.L + m.T < x0 || ox - m.T > x0 + 15 || oz + m.W < z0 || oz - 6 > z0 + 15) continue;
            const uint32_t seed = hash3(rx, 77, rz, seed_ ^ 0x3E80u);
            for (int x = 0; x < kChunkSize; ++x)
                for (int z = 0; z < kChunkSize; ++z) {
                    const int lx = x0 + x - ox, lz = z0 + z - oz;
                    for (int y = 0; y <= 8; ++y) {
                        ItemId id = metroBlock(m, lx, y, lz, seed);
                        if (id == Structure::kVoid) continue;
                        const ItemId old = c.get(x, kMetroY + y, z);
                        if (old == BEDROCK || old == WORLD_END) continue;
                        c.set(x, kMetroY + y, z, id);
                    }
                    // The chests on the side platforms (two in the big one).
                    for (int k = 0; k < (big ? 2 : 1); ++k) {
                        const int cxL = m.L / 2 + k * 20 - 10 * k, czL = k ? m.W - 3 : 2;
                        if (lx == cxL && lz == czL) c.set(x, kMetroY + 2, z, LOOT_CHEST_METRO);
                    }
                    // The shaft up to the land, with a little stone entrance on top.
                    if (lx >= 4 && lx <= 8 && lz >= -5 && lz <= -1) {
                        const bool core = lx >= 5 && lx <= 7 && lz >= -4 && lz <= -2;
                        const int top = column(x0 + x, z0 + z).height + kDeepShift;  // (stamped after the lift)
                        for (int y = kMetroY + 9; y <= top + 3 && y < kChunkHeight - 1; ++y) {
                            ItemId id = core ? (ItemId)((lz == -4 && lx == 6) ? VINES : AIR) : (ItemId)STONE_BRICKS;
                            if (!core && y > top && lx == 6 && lz == -1 && y <= top + 2) id = AIR;  // the door
                            if (y == top + 3) id = core ? (ItemId)STONE_BRICKS : (ItemId)slabId(6, 0);
                            c.set(x, y, z, id);
                        }
                    }
                }
        }
}

bool World::bronzeHallsIn(int rx, int rz, int& ox, int& oz) const {
    const uint32_t h = hash3(rx, 777, rz, seed_ ^ 0xB80Eu);
    if (h % 100 >= 70) return false;  // most regions have one
    ox = rx * kHallRegion + 40 + (int)((h >> 8) % (kHallRegion - kHallSize - 80));
    oz = rz * kHallRegion + 40 + (int)((h >> 18) % (kHallRegion - kHallSize - 80));
    return true;
}

ItemId World::bronzeHallsBlock(int lx, int ly, int lz, uint32_t seed) const {
    // lx, lz in 0..kHallSize-1, ly in 0..kHallHeight-1 (0 floor, top ceiling). solid rock where nothing is built.
    constexpr int R = kHallRoom, C = kHallCell, M = R / 2, last = kHallGrid - 1;
    auto inRoom = [](int v) { return v % C < R; };
    auto mid = [](int v) { return std::abs(v % C - M) <= 1; };  // where corridors join a hall, 3 wide
    const int ri = lx / C, rj = lz / C;
    const bool room = inRoom(lx) && inRoom(lz);
    const bool corrX = !inRoom(lx) && inRoom(lz) && mid(lz);
    const bool corrZ = inRoom(lx) && !inRoom(lz) && mid(lx);
    const uint32_t r = hash3(lx, ly, lz, seed);
    // The rust grows thicker the farther a hall lies from the treasury in the middle.
    const int dist = std::abs(ri - kHallGrid / 2) + std::abs(rj - kHallGrid / 2);
    auto aged = [&](int base) {
        const int roll = (int)(r % 100);
        const int stage = roll < 5 + dist * 12 ? 2 : roll < 30 + dist * 12 ? 1 : 0;
        return bronzeVariant(base, stage, roll % 29 == 0);
    };
    const int top = kHallHeight - 1;
    if (room) {
        const int x = lx % C, z = lz % C;
        const bool treasury = ri == kHallGrid / 2 && rj == kHallGrid / 2;
        const bool cornerHall = (ri == 0 || ri == last) && (rj == 0 || rj == last);
        const bool guardHall = !treasury && !cornerHall && (ri + rj) % 2 == 1;  // spawners every other hall
        const bool edgeX = x == 0 || x == R - 1, edgeZ = z == 0 || z == R - 1;
        const bool openingSide = (x == 0 && ri > 0) || (x == R - 1 && ri < last) || (z == 0 && rj > 0) || (z == R - 1 && rj < last);
        const bool doorway = ((edgeX && mid(lz)) || (edgeZ && mid(lx))) && openingSide && ly >= 1 && ly <= 4;
        if (edgeX && edgeZ) return ly == 4 || ly == 7 ? (ItemId)(BRONZE_BULB + 1) : aged(2);  // corner pillars with lamps
        if (edgeX || edgeZ) {
            const int along = edgeX ? z : x;
            const int face = edgeX ? (x == 0 ? 1 : 3) : (z == 0 ? 2 : 0);
            if (doorway) {
                // An arch 3 wide and 4 high: stair corners, a chiseled keystone; open bronze doors in the middle.
                if (ly == 4) return along == M ? aged(2) : stairsId(42, face, true);
                if (along == M && ly <= 2 && !treasury) return doorId(BRONZE_DOOR, {face, true, ly == 2});
                return AIR;
            }
            // Walls in bands: chiseled skirting, cut bronze, grate windows over lit bulbs, a chiseled frieze.
            if (ly == 1 || ly == 6) return aged(2);
            if (ly == 3 && along % 4 == 2) return aged(3);
            if (ly == 2 && along % 4 == 2) return (ItemId)(BRONZE_BULB + 1);
            return aged(along % 2 ? 1 : 0);
        }
        if (ly == top) {
            // A coffered ceiling: cut bronze beams, plain panels, a bulb in every bay, a big grate skylight.
            if (std::abs(x - M) <= 1 && std::abs(z - M) <= 1) return aged(3);
            if (x % 4 == 2 && z % 4 == 2) return (ItemId)(BRONZE_BULB + 1);
            return (x % 4 == 0 || z % 4 == 0) ? aged(1) : aged(0);
        }
        if (ly == top - 1 && (x == 1 || x == R - 2 || z == 1 || z == R - 2)) return slabId(42, 1);  // a cornice
        if (ly == 0) {
            if (treasury) {
                const int d = std::max(std::abs(x - M), std::abs(z - M));
                if (d == 4) return aged(3);  // a ring of grates over lava round the dais
                return d % 2 ? bronzeVariant(2, 0, true) : bronzeVariant(0, 0, true);  // polished rings
            }
            if (x == M || z == M) return aged(2);  // a chiseled cross
            if (guardHall && std::abs(x - M) == 3 && std::abs(z - M) == 3) return aged(3);  // lava vents
            return (x / 2 + z / 2) % 2 ? aged(0) : aged(1);
        }
        if (treasury) {
            // The dais: two tiers of stairs up to a chiseled top, four chests, lamp posts at the corners.
            const int dx = std::abs(x - M), dz = std::abs(z - M), d = std::max(dx, dz);
            if (d <= 3) {
                if (ly == 1) return d == 3 ? (dx == dz ? aged(2) : stairsId(41, dx == 3 ? (x < M ? 1 : 3) : (z < M ? 2 : 0), false)) : aged(2);
                if (ly == 2 && d <= 2) {
                    if (d == 2) return dx == dz ? aged(2) : stairsId(41, dx == 2 ? (x < M ? 1 : 3) : (z < M ? 2 : 0), false);
                    return dx + dz == 1 ? (ItemId)LOOT_CHEST_BRONZE : aged(2);
                }
                if (ly == 3 && dx == 0 && dz == 0) return slabId(41, 0);
                if (ly >= 2 && ly <= 4 && dx == 3 && dz == 3) return ly == 4 ? (ItemId)(BRONZE_BULB + 1) : aged(2);
            }
            // Columns round the room.
            if ((x == 2 || x == R - 3) && (z == 2 || z == R - 3)) return aged(2);
            return AIR;
        }
        // The other halls: a ring of pillars, benches along the walls, spawners or chests.
        if ((x == 4 || x == R - 5) && (z == 4 || z == R - 5)) return ly == top - 1 ? slabId(42, 1) : ly == 4 ? (ItemId)(BRONZE_BULB + 1) : aged(2);
        if (ly == 1 && (x == 1 || x == R - 2) && z >= 2 && z <= R - 3 && !mid(lz)) return stairsId(41, x == 1 ? 3 : 1, false);
        if (ly == 1 && (z == 1 || z == R - 2) && x >= 2 && x <= R - 3 && !mid(lx)) return stairsId(41, z == 1 ? 0 : 2, false);
        if (guardHall || cornerHall) {
            if (ly == 1 && x == M && z == M) return MOB_SPAWNER;
            if (ly == 1 && cornerHall && ((x == 2 && z == 2) || (x == R - 3 && z == R - 3))) return LOOT_CHEST_BRONZE;
            return AIR;
        }
        if (ly == 1 && x == M && z == M) return (r >> 8) % 3 ? (ItemId)LOOT_CHEST_BRONZE : (ItemId)AIR;
        if (ly == 1 && std::abs(x - M) + std::abs(z - M) == 1) return slabId(41, 0);
        return AIR;
    }
    if (corrX || corrZ) {
        const int across = corrX ? lz % C : lx % C;
        if (ly == 0) return across == M ? aged(3) : aged(1);   // a grate down the middle over the lava
        if (ly == 5) return across == M ? (ItemId)(BRONZE_BULB + 1) : aged(1);
        if (ly == 4) return across == M ? (ItemId)AIR : stairsId(42, corrX ? (across < M ? 2 : 0) : (across < M ? 1 : 3), true);
        if (ly > 5) return ABYSSAL_ROCK;
        return AIR;
    }
    const bool corrWall = (!inRoom(lx) && inRoom(lz) && std::abs(lz % C - M) == 2) || (inRoom(lx) && !inRoom(lz) && std::abs(lx % C - M) == 2);
    if (corrWall) return ly <= 5 ? aged(ly == 1 ? 2 : 1) : (ItemId)ABYSSAL_ROCK;
    return ABYSSAL_ROCK;
}

void World::stampBronzeHalls(Chunk& c) {
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize;
    for (int rx = floorDiv(x0, kHallRegion) - 1; rx <= floorDiv(x0 + 15, kHallRegion); ++rx)
        for (int rz = floorDiv(z0, kHallRegion) - 1; rz <= floorDiv(z0 + 15, kHallRegion); ++rz) {
            int ox, oz;
            if (!bronzeHallsIn(rx, rz, ox, oz)) continue;
            if (ox + kHallSize + 6 < x0 || ox - 6 > x0 + 15 || oz + kHallSize + 4 < z0 || oz - 4 > z0 + 15) continue;
            const uint32_t seed = hash3(rx, 31, rz, seed_ ^ 0xB80Fu);
            for (int x = 0; x < kChunkSize; ++x)
                for (int z = 0; z < kChunkSize; ++z) {
                    const int lx = x0 + x - ox, lz = z0 + z - oz;
                    if (lx >= -2 && lz >= -2 && lx < kHallSize + 2 && lz < kHallSize + 2)
                        for (int y = kHallFloor - 3; y <= kHallFloor + kHallHeight + 1; ++y) c.set(x, y, z, ABYSSAL_ROCK);  // a solid shell
                    if (lx >= 0 && lz >= 0 && lx < kHallSize && lz < kHallSize) {
                        for (int ly = 0; ly < kHallHeight; ++ly) {
                            c.set(x, kHallFloor + ly, z, bronzeHallsBlock(lx, ly, lz, seed));
                        }
                        // Lava two below the grates.
                        if (bronzeAgingBase(c.get(x, kHallFloor, z)) == 3) {
                            c.set(x, kHallFloor - 1, z, LAVA);
                            c.set(x, kHallFloor - 2, z, ABYSSAL_ROCK);
                        }
                    }
                    // The shaft: north-west of the grid, 2 x 2, from the first hall up to the surface, vines on its wall.
                    const int sx = lx - 2, sz = lz - 2;
                    if (sx >= 0 && sx < 4 && sz >= 0 && sz < 4) {
                        int top = kGenHeight + kDeepShift - 1;
                        while (top > kHallFloor && (c.get(x, top, z) == AIR || isLeaves(c.get(x, top, z)) || isPlant(c.get(x, top, z)))) --top;
                        const bool wall = sx == 0 || sz == 0 || sx == 3 || sz == 3;
                        for (int y = kHallFloor + kHallHeight - 1; y <= top + 1; ++y) {
                            if (!wall) c.set(x, y, z, sx == 1 && y <= top ? (ItemId)VINES : (ItemId)AIR);
                            else if (y <= top) c.set(x, y, z, bronzeVariant(1, y % 3 == 0 ? 2 : 1, false));
                        }
                        // The marker: a ring of cut bronze round the shaft and chiseled corners.
                        if (wall) {
                            c.set(x, top + 1, z, (sx == 0 || sx == 3) && (sz == 0 || sz == 3) ? (ItemId)CHISELED_BRONZE : (ItemId)CUT_BRONZE);
                            if ((sx == 0 || sx == 3) && (sz == 0 || sz == 3)) c.set(x, top + 2, z, (ItemId)(BRONZE_BULB + 1));
                        }
                    }
                }
        }
}

void World::decorateCaves(Chunk& c, const Column cols[kChunkSize][kChunkSize]) {
    // 1.6.0 Caves and Tunes: what lives in the caves of new worlds. Mossy caves (moss floors,
    // mossy rock, vines from the ceiling) come in regions; glowing fungus grows deep down;
    // gravel lies on cave floors; lava and tar gather in pools.
    const int cx = c.cx, cz = c.cz;
    auto rock = [](ItemId b) { return b == STONE || b == DEEPSTONE; };
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z, top = cols[x][z].height - 6;
            const bool lush = detail_.perlin2(wx / 64.0f - 410.0f, wz / 64.0f + 95.0f) > 0.18f;
            const bool gravelPatch = detail_.perlin2(wx / 9.0f + 77.0f, wz / 9.0f - 31.0f) > 0.35f;
            // Pools come in two sizes: small pockets anywhere in the deeper caves, and wide lakes
            // on the floors of the deep chambers. Kept rare so caves stay walkable.
            const float pocket = detail_.perlin2(wx / 5.0f - 260.0f, wz / 5.0f + 610.0f);
            const float lake = detail_.perlin2(wx / 26.0f + 140.0f, wz / 26.0f - 75.0f);
            const bool tarPools = detail_.perlin2(wx / 150.0f + 33.0f, wz / 150.0f) > 0.2f;
            for (int y = 6; y < top && y < kGenHeight - 2; ++y) {
                if (c.get(x, y, z) != AIR) continue;
                const uint32_t r = hash3(wx, y, wz, seed_ ^ 0xCA7Eu) % 1000;
                const ItemId below = c.get(x, y - 1, z), above = c.get(x, y + 1, z);
                if (rock(below)) {
                    // A cave floor.
                    // (The water and tar lakes up to kCaveLakeLevel stay as they are.)
                    const bool inside = x > 0 && x < 15 && z > 0 && z < 15 && y > kCaveLakeLevel + 1;
                    const bool bigLake = inside && y <= 24 && lake > 0.42f;
                    const bool smallPool = inside && y < (tarPools ? 40 : 32) && pocket > 0.6f;
                    if (bigLake || smallPool) {
                        // The floor turns to liquid (two deep in a lake) on a bed of rock.
                        const ItemId liquid = tarPools ? (ItemId)TAR : (ItemId)LAVA;
                        const int deep = bigLake ? 2 : 1;
                        for (int d = 1; d <= deep; ++d) c.set(x, y - d, z, liquid);
                        c.set(x, y - deep - 1, z, tarPools ? (ItemId)TAR_BLOCK : (ItemId)DEEPSTONE);
                    } else if (lush) {
                        c.set(x, y - 1, z, r < 650 ? (ItemId)MOSS_BLOCK : (ItemId)MOSSY_DEEPSTONE);
                    } else if (gravelPatch) {
                        c.set(x, y - 1, z, GRAVEL);
                    } else if (y < 22 && r < 25) {
                        c.set(x, y - 1, z, FUNGAL_DEEPSTONE);
                    }
                }
                if (rock(above)) {
                    // A cave ceiling: mossy in the mossy caves, with vines hanging down.
                    if (lush) {
                        if (r < 300) c.set(x, y + 1, z, MOSSY_DEEPSTONE);
                        if (r % 7 < 3) {
                            const int len = 1 + (int)(r % 5);
                            for (int k = 0; k < len && y - k > 1 && c.get(x, y - k, z) == AIR; ++k) c.set(x, y - k, z, VINES);
                        }
                    } else if (y < 22 && r < 15) {
                        c.set(x, y + 1, z, FUNGAL_DEEPSTONE);
                    }
                }
            }
            // Ore showing in the cave walls now and then, by depth: coal high up, iron and bronze
            // in the middle, steel lower, ruby rarely and only deep. Small veins of 1 to 3.
            for (int y = 6; y < top; ++y) {
                if (!rock(c.get(x, y, z))) continue;
                const bool open = (x > 0 && c.get(x - 1, y, z) == AIR) || (x < 15 && c.get(x + 1, y, z) == AIR) ||
                                  (z > 0 && c.get(x, y, z - 1) == AIR) || (z < 15 && c.get(x, y, z + 1) == AIR) ||
                                  c.get(x, y + 1, z) == AIR || c.get(x, y - 1, z) == AIR;
                if (!open) continue;
                const uint32_t r = hash3(wx, y, wz, seed_ ^ 0x0E5Eu);
                if (r % 1000 >= 5) continue;
                const uint32_t pick = (r >> 10) % 100;
                ItemId ore = y > 40 ? (ItemId)COAL_ORE
                             : y > 26 ? (pick < 55 ? (ItemId)IRON_ORE : (ItemId)COAL_ORE)
                             : y > 14 ? (pick < 45 ? (ItemId)BRONZE_ORE : pick < 80 ? (ItemId)IRON_ORE : (ItemId)STEEL_ORE)
                                      : (pick < 50 ? (ItemId)STEEL_ORE : pick < 62 ? (ItemId)RUBY_ORE : (ItemId)IRON_ORE);
                c.set(x, y, z, ore);
                const int more = (int)((r >> 20) % 3);
                for (int k = 0; k < more; ++k) {
                    const int dx = (int)((r >> (22 + k * 3)) % 3) - 1, dy = (int)((r >> (23 + k * 3)) % 3) - 1;
                    if (x + dx >= 0 && x + dx < 16 && rock(c.get(x + dx, y + dy, z))) c.set(x + dx, y + dy, z, ore);
                }
            }
            // Mossy walls in the mossy caves: rock beside the cave air.
            if (lush)
                for (int y = 6; y < top; ++y)
                    if (rock(c.get(x, y, z)) && hash3(wx, y, wz, seed_ ^ 0x11Fu) % 3 == 0 &&
                        ((x > 0 && c.get(x - 1, y, z) == AIR) || (x < 15 && c.get(x + 1, y, z) == AIR) ||
                         (z > 0 && c.get(x, y, z - 1) == AIR) || (z < 15 && c.get(x, y, z + 1) == AIR)))
                        c.set(x, y, z, MOSSY_DEEPSTONE);
        }
}

bool World::nearestMineshaft(int x, int z, int& outX, int& outY, int& outZ) {
    long best = -1;
    if (dim_ == Dimension::Nether) {
        const int br = floorDiv(x, kCrimsonShaftRegion), bz = floorDiv(z, kCrimsonShaftRegion);
        for (int rx = br - 4; rx <= br + 4; ++rx)
            for (int rz = bz - 4; rz <= bz + 4; ++rz) {
                int ox, oy, oz;
                if (!crimsonShaftIn(rx, rz, ox, oy, oz)) continue;
                ox += 48, oy += 2, oz += 48;  // the central room
                const long d = (long)(ox - x) * (ox - x) + (long)(oz - z) * (oz - z);
                if (best < 0 || d < best) best = d, outX = ox, outY = oy, outZ = oz;
            }
        if (best >= 0) outY += yShift();
    return best >= 0;
    }
    if (dim_ != Dimension::Overworld || !config_.overworld.structures) return false;
    const int region = 288, maxSize = 97;
    const int br = floorDiv(x, region), bz = floorDiv(z, region);
    for (int rx = br - 3; rx <= br + 3; ++rx)
        for (int rz = bz - 3; rz <= bz + 3; ++rz) {
            const uint64_t key = ((uint64_t)(uint32_t)rx << 32) | (uint32_t)rz;
            auto it = mineshafts_.find(key);
            if (it == mineshafts_.end()) {
                // The same choices as stampStructures makes for a mineshaft.
                std::array<int, 4> found{0, 0, 0, 0};
                const uint32_t h = hash3(rx, (int)WorldStructure::Mineshaft * 977 + 13, rz, seed_ ^ 0x57A7u);
                if ((int)(h % 100) < 65) {
                    const int ox = rx * region + (int)((h >> 8) % (uint32_t)(region - maxSize));
                    const int oz = rz * region + (int)((h >> 16) % (uint32_t)(region - maxSize));
                    const Structure s = buildWorldStructure(WorldStructure::Mineshaft, h * 2654435761u).rotated((int)(h >> 28));
                    const int oy = 16 + (int)((h >> 4) % 14);
                    if (column(ox + s.sx / 2, oz + s.sz / 2).height >= oy + s.sy + 8) found = {1, ox + s.sx / 2, oy + 2, oz + s.sz / 2};
                }
                if (mineshafts_.size() > 256) mineshafts_.clear();
                it = mineshafts_.emplace(key, found).first;
            }
            if (!it->second[0]) continue;
            const long d = (long)(it->second[1] - x) * (it->second[1] - x) + (long)(it->second[3] - z) * (it->second[3] - z);
            if (best < 0 || d < best) best = d, outX = it->second[1], outY = it->second[2], outZ = it->second[3];
        }
    if (best >= 0) outY += yShift();
    return best >= 0;
}

void World::stampStructures(Chunk& c) {
    if (!config_.overworld.structures) return;
    struct Kind {
        WorldStructure type;
        int region;      // one chance per region of this many blocks
        int percent;     // chance that the region has one
        int maxSize;     // blueprint size bound, to know which regions can reach this chunk
    };
    static const Kind kinds[] = {
        {WorldStructure::Ruin, 160, 70, 8},
        {WorldStructure::Shipwreck, 128, 85, 14},
        {WorldStructure::Dungeon, 128, 45, 10},
        {WorldStructure::Mineshaft, 288, 65, 97},
        {WorldStructure::DesertTemple, 240, 65, 15},
        {WorldStructure::Igloo, 200, 60, 9},
        {WorldStructure::Watchtower, 224, 50, 7},
        {WorldStructure::Village, kVillageRegion, 80, kVillageSize},
        {WorldStructure::DrownedRuins, 176, 60, kDrownedRuinsSize},
        {WorldStructure::BrokenPortal, 240, 60, kBrokenPortalSize},
        {WorldStructure::RuinedVillage, 400, 45, kVillageSize},  // 1.5.9 (generation 3)
        {WorldStructure::CaveDungeon, 72, 55, 11},               // 1.6.0 (generation 4)
        {WorldStructure::Camp, 112, 70, 11},                     // 1.7.2 build 3 (generation 5)
        {WorldStructure::RuinedStation, kStationRegion, 45, 23},            // 2.4.0 (generation 7)
    };
    const int sea = config_.overworld.seaLevel;
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize, x1 = x0 + kChunkSize - 1, z1 = z0 + kChunkSize - 1;
    for (const Kind& k : kinds) {
        if (k.type == WorldStructure::RuinedVillage && config_.generation < 3) continue;
        if (k.type == WorldStructure::CaveDungeon && config_.generation < 4) continue;
        if (k.type == WorldStructure::Camp && config_.generation < 5) continue;
        if (k.type == WorldStructure::RuinedStation && (config_.generation < 7 || !kFeatures240)) continue;
        for (int rx = floorDiv(x0 - k.maxSize, k.region); rx <= floorDiv(x1, k.region); ++rx)
            for (int rz = floorDiv(z0 - k.maxSize, k.region); rz <= floorDiv(z1, k.region); ++rz) {
                uint32_t h = hash3(rx, (int)k.type * 977 + 13, rz, seed_ ^ 0x57A7u);
                if ((int)(h % 100) >= k.percent) continue;
                // Where in the region, and how it stands.
                int ox = rx * k.region + (int)((h >> 8) % (uint32_t)(k.region - k.maxSize));
                int oz = rz * k.region + (int)((h >> 16) % (uint32_t)(k.region - k.maxSize));
                const uint64_t key = ((uint64_t)(uint32_t)rx << 34) ^ ((uint64_t)(uint32_t)rz << 4) ^ (uint64_t)k.type;
                auto it = blueprints_.find(key);
                if (it == blueprints_.end()) {
                    if (blueprints_.size() > 48) blueprints_.clear();
                    Structure s = buildWorldStructure(k.type, h * 2654435761u);
                    // Villages keep their layout (their doors face the paths).
                    const int turns = k.type == WorldStructure::Village || k.type == WorldStructure::RuinedVillage || k.type == WorldStructure::RuinedStation ? 0 : (int)(h >> 28);
                    it = blueprints_.emplace(key, std::make_shared<Structure>(s.rotated(turns))).first;
                }
                const Structure& s = *it->second;
                if (ox > x1 || oz > z1 || ox + s.sx <= x0 || oz + s.sz <= z0) continue;
                // The height decides whether it belongs here at all.
                Column middle = column(ox + s.sx / 2, oz + s.sz / 2);
                int oy;
                const BiomeDef* mb = middle.biome;
                const bool desert = mb && (mb->cactusDensity > 0 || mb->id == "desert");
                const bool snowy = mb && (mb->freeze || mb->id == "snowy_taiga" || mb->id == "ice_plains");
                const bool surface = k.type == WorldStructure::Ruin || k.type == WorldStructure::DesertTemple ||
                                     k.type == WorldStructure::Igloo || k.type == WorldStructure::Watchtower ||
                                     k.type == WorldStructure::Village || k.type == WorldStructure::BrokenPortal ||
                                     k.type == WorldStructure::RuinedVillage || k.type == WorldStructure::Camp || k.type == WorldStructure::RuinedStation;
                if (surface) {
                    // Camps stand mostly in the Dappled Forest; elsewhere only now and then.
                    if (k.type == WorldStructure::Camp && !(mb && mb->id == "dappled_forest") && (h >> 5) % 6 != 0) continue;
                    if (middle.height <= sea + 1 || !mb || mb->peakBlock) continue;
                    if (k.type == WorldStructure::DesertTemple && !desert) continue;
                    if (k.type == WorldStructure::Igloo && !snowy) continue;
                    if ((k.type == WorldStructure::Watchtower || k.type == WorldStructure::Village || k.type == WorldStructure::RuinedVillage) &&
                        (desert || snowy))
                        continue;
                    oy = middle.height + (k.type == WorldStructure::Ruin ? 0 : 1);
                    // Some portal ruins are half buried.
                    if (k.type == WorldStructure::BrokenPortal) oy = middle.height - ((h >> 6) % 3 == 0 ? 3 : 0);
                } else if (k.type == WorldStructure::Shipwreck) {
                    if (middle.height > sea - 5) continue;  // needs deep water
                    oy = middle.height;
                } else if (k.type == WorldStructure::DrownedRuins) {
                    if (middle.height > sea - 4) continue;  // under a lake or the sea
                    oy = middle.height - 1;
                } else if (k.type == WorldStructure::Dungeon) {
                    oy = 10 + (int)((h >> 4) % 24);
                    if (middle.height < oy + s.sy + 6) continue;
                } else if (k.type == WorldStructure::CaveDungeon) {
                    // Deep in the rock, under solid ground, often beside a cave.
                    oy = 8 + (int)((h >> 4) % 30);
                    if (middle.height < oy + s.sy + 10) continue;
                } else {
                    oy = 16 + (int)((h >> 4) % 14);
                    if (middle.height < oy + s.sy + 8) continue;
                }
                // 2.4.0 (generation 7): a village no longer lays a round flat plate of grass over the land (on
                // a hill it stood up as a raised disc with cliffs). Open ground keeps the land as it is, a path
                // is laid into the ground where it lies; only houses, the well and the fields stand on the plan.
                const bool village = config_.generation >= 7 && (k.type == WorldStructure::Village || k.type == WorldStructure::RuinedVillage);
                auto openColumn = [&](int x, int z, bool& path) {
                    path = false;
                    for (int y = 0; y < s.sy; ++y) {
                        const ItemId id = s.at(x, y, z);
                        if (id == Structure::kVoid || id == AIR || isPlant(id)) continue;
                        if (y == 0 && (id == GRASS || id == GRAVEL)) { path = id == GRAVEL; continue; }
                        return false;
                    }
                    return true;
                };
                if (village)
                    for (int z = std::max(0, z0 - oz); z < s.sz && oz + z <= z1; ++z)
                        for (int x = std::max(0, x0 - ox); x < s.sx && ox + x <= x1; ++x) {
                            bool path;
                            if (!openColumn(x, z, path) || !path) continue;
                            const int lx = ox + x - x0, lz = oz + z - z0;
                            int g = std::min(kGenHeight - 2, oy + 4);
                            while (g > 1 && (c.get(lx, g, lz) == AIR || isPlant(c.get(lx, g, lz)) || isLeaves(c.get(lx, g, lz)) || isLog(c.get(lx, g, lz)))) --g;
                            if (std::abs(g - (oy - 1)) <= 5 && c.get(lx, g, lz) != WATER) {
                                c.set(lx, g, lz, GRAVEL);
                                if (isPlant(c.get(lx, g + 1, lz))) c.set(lx, g + 1, lz, AIR);
                            }
                        }
                // Copy the part inside this chunk.
                for (int y = 0; y < s.sy; ++y) {
                    const int wy = oy + y;
                    if (wy < 1 || wy >= kGenHeight) continue;
                    for (int z = std::max(0, z0 - oz); z < s.sz && oz + z <= z1; ++z)
                        for (int x = std::max(0, x0 - ox); x < s.sx && ox + x <= x1; ++x) {
                            ItemId id = s.at(x, y, z);
                            if (id == Structure::kVoid) continue;
                            bool pathCol;
                            if (village && openColumn(x, z, pathCol)) continue;
                            const int lx = ox + x - x0, lz = oz + z - z0;
                            if (c.get(lx, wy, lz) == BEDROCK || c.get(lx, wy, lz) == WORLD_END) continue;
                            if (id == AIR && wy <= sea && (k.type == WorldStructure::Shipwreck || k.type == WorldStructure::DrownedRuins)) id = WATER;
                            if (id == AIR && k.type == WorldStructure::Ruin && wy > middle.height + 4) continue;
                            // 1.6.0: the old mineshafts' planks have rotted in places.
                            if (id == PLANKS && k.type == WorldStructure::Mineshaft && config_.generation >= 4 &&
                                hash3(ox + x, wy, oz + z, seed_ ^ 0x207Fu) % 5 < 2)
                                id = ROTTEN_PLANKS;
                            // A buried portal ruin keeps the earth around its lower half.
                            if (id == AIR && k.type == WorldStructure::BrokenPortal && (wy <= middle.height || wy > middle.height + 9)) continue;
                            c.set(lx, wy, lz, id);
                            // Buildings stand on a foundation where the ground dips.
                            if (surface && y == 0)
                                for (int d = 1; d <= 4 && wy - d > 0; ++d) {
                                    ItemId below = c.get(lx, wy - d, lz);
                                    if (below != AIR && below != WATER && !isPlant(below)) break;
                                    c.set(lx, wy - d, lz, k.type == WorldStructure::Village || k.type == WorldStructure::RuinedVillage ? (ItemId)DIRT : (ItemId)COBBLESTONE);
                                }
                        }
                }
            }
    }
}

void World::placeOres(Chunk& c, const OreDef& ore, uint32_t salt) {
    // Small blobs that random-walk through stone; kept inside the chunk.
    for (int v = 0; v < ore.veins; ++v) {
        uint32_t h = hash3(c.cx, v, c.cz, seed_ ^ salt);
        int x = 1 + (int)(h % 14), z = 1 + (int)((h >> 4) % 14);
        int y = ore.minY + (int)((h >> 8) % (uint32_t)std::max(1, ore.maxY - ore.minY));
        for (int i = 0; i < ore.size; ++i) {
            // 1.6.0: the ores reach into the deepstone too; hardium is found only there.
            const ItemId here = c.get(x, y, z);
            if (ore.block == HARDIUM_ORE ? (here == DEEPSTONE || here == ABYSSAL_ROCK) : (here == STONE || here == DEEPSTONE || here == ABYSSAL_ROCK)) c.set(x, y, z, ore.block);
            uint32_t step = hash3(v, i, c.cx ^ c.cz, seed_ ^ salt ^ h);
            switch (step % 6) {
                case 0: x = std::min(x + 1, kChunkSize - 1); break;
                case 1: x = std::max(x - 1, 0); break;
                case 2: z = std::min(z + 1, kChunkSize - 1); break;
                case 3: z = std::max(z - 1, 0); break;
                case 4: y = std::min(y + 1, ore.maxY); break;
                default: y = std::max(y - 1, 1); break;
            }
        }
    }
}

void World::generateNether(Chunk& c) {
    const NetherDef& nd = config_.nether;
    const float floorThreshold = 0.5f - nd.floorAmount;
    const uint32_t oreEvery = nd.oreChance > 0 ? (uint32_t)std::max(1.0f, 1.0f / nd.oreChance) : 0;
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            int wx = c.cx * kChunkSize + x, wz = c.cz * kChunkSize + z;
            float soul = detail_.perlin2(wx / 20.0f, wz / 20.0f);
            for (int y = 0; y < kGenHeight; ++y) {
                ItemId id;
                uint32_t h = hash3(wx, y, wz, seed_ ^ 0xBEEFu);
                if (y == 0) {
                    id = WORLD_END;
                } else if (y < 5 && (int)(h & 3) < 4 - y) {
                    id = BEDROCK;
                } else if (y == kGenHeight - 1 || (y > kGenHeight - 6 && (int)(h & 3) < y - (kGenHeight - 6))) {
                    id = nd.ceiling;
                } else {
                    // Big open caverns: 3D noise, denser towards the floor and the ceiling.
                    float n = caves_.perlin3(wx / nd.caveScale, y / 20.0f, wz / nd.caveScale) +
                              0.5f * terrain_.perlin3(wx / 12.0f, y / 10.0f, wz / 12.0f);
                    float edge = std::max(0.0f, (24 - y) / 24.0f) + std::max(0.0f, (y - 96) / 28.0f);
                    if (n + edge * 1.2f > nd.openness) id = (oreEvery && h % oreEvery == 0) ? nd.ore : nd.fill;
                    else id = y <= nd.lavaLevel ? nd.liquid : (ItemId)AIR;
                }
                c.set(x, y, z, id);
            }
            // Floor blocks in open areas, a little gravel near the lava shore.
            const bool meadow = nd.fill == NETHERRACK && netherGrassAt(wx, wz);
            const bool wasteland = nd.fill == NETHERRACK && wastelandAt(wx, wz);
            const int cb = cornerBiomeAt(wx, wz);  // 2.2.0 preview
            if (cb == 1) {
                // Gilded Quarry: the rock in worked bands with carved rows, and rich veins of ore.
                for (int y = 6; y < kGenHeight - 6; ++y) {
                    if (c.get(x, y, z) != nd.fill) continue;
                    const uint32_t q = hash3(wx, y, wz, seed_ ^ 0x6A11u);
                    if (y % 7 == 0) c.set(x, y, z, CARVED_CORNERSTONE);
                    else if (q % 70 == 0) c.set(x, y, z, BRONZE_ORE);
                    else if (q % 90 == 1) c.set(x, y, z, POWERSTONE_ORE);
                    else if (q % 240 == 2) c.set(x, y, z, RUBY_ORE);
                }
            } else if (cb == 2) {
                // Prismatide Coast: the lava sea turns into shallow brine lagoons over siltstone.
                for (int y = 1; y <= nd.lavaLevel; ++y) {
                    const ItemId b = c.get(x, y, z);
                    if (b == nd.liquid) c.set(x, y, z, y >= nd.lavaLevel - 1 ? (ItemId)AETHER_BRINE : (ItemId)SILTSTONE);
                    else if (b == nd.fill && y > nd.lavaLevel - 6) c.set(x, y, z, SILTSTONE);
                }
                const uint32_t q = hash3(wx, 991, wz, seed_ ^ 0xC0A5u);
                if (q % 160 == 0 && c.get(x, nd.lavaLevel, z) == AETHER_BRINE) {
                    // A hexagonal crystal pillar rising out of the lagoon.
                    const int tall = 3 + (int)((q >> 9) % 6);
                    for (int k = 0; k <= tall && nd.lavaLevel + k < kGenHeight - 4; ++k)
                        if (c.get(x, nd.lavaLevel + k, z) == AIR || c.get(x, nd.lavaLevel + k, z) == AETHER_BRINE) c.set(x, nd.lavaLevel + k, z, PRISMATIDE_PILLAR);
                }
            }
            for (int y = nd.lavaLevel + 1; y < kGenHeight - 2; ++y) {
                if (c.get(x, y, z) != nd.fill || c.get(x, y + 1, z) != AIR) continue;
                if (cb != 0) {
                    const uint32_t r = hash3(wx, y, wz, seed_ ^ 0x22B1u);
                    const bool room = c.get(x, y + 2, z) == AIR;
                    if (cb == 1) {
                        // Gilded Quarry: terraces cut in steps of four blocks; gilded cobble paths and
                        // carved plazas in whole patches (not block by block), bare cornerstone between.
                        int top = y;
                        const int step = (y / 4) * 4 + 3;
                        if (step > y && step < kGenHeight - 4) {
                            bool clear = true;
                            for (int k = y + 1; k <= step + 2 && clear; ++k) clear = c.get(x, k, z) == AIR;
                            if (clear) {
                                for (int k = y + 1; k <= step; ++k) c.set(x, k, z, nd.fill);
                                top = step;
                            }
                        }
                        const float paths = detail_.perlin2(wx / 11.0f + 300, wz / 11.0f - 120);
                        const float plaza = detail_.perlin2(wx / 17.0f - 510, wz / 17.0f + 77);
                        const ItemId ground = std::fabs(paths) < 0.09f ? (ItemId)GILDED_COBBLE : plaza > 0.32f ? (ItemId)CARVED_CORNERSTONE : (ItemId)NETHERRACK;
                        c.set(x, top, z, ground);
                        if (top + 2 >= kGenHeight || c.get(x, top + 1, z) != AIR || c.get(x, top + 2, z) != AIR) continue;
                        if (ground == NETHERRACK && r % 7 == 0) c.set(x, top + 1, z, COPPER_MOSS);  // moss grows on the bare rock
                        else if (ground == CARVED_CORNERSTONE && (wx % 6 == 0) && (wz % 6 == 0)) {
                            // Amber lamps stand on the plazas in a regular grid.
                            c.set(x, top + 1, z, CARVED_CORNERSTONE);
                            c.set(x, top + 2, z, AMBER_RESIN);
                        }
                        y = top;
                    } else if (cb == 2) {
                        // Prismatide Coast: salt along the lagoons, siltstone further up; reeds and pads.
                        const bool shore = y <= nd.lavaLevel + 3;
                        c.set(x, y, z, shore ? (ItemId)LUMINOUS_SALT : r % 9 == 0 ? (ItemId)LUMINOUS_SALT : (ItemId)SILTSTONE);
                        if (!room) continue;
                        if (shore && r % 5 == 0 && c.get(x, y + 3, z) == AIR) {
                            c.set(x, y + 1, z, GLASS_REEDS);
                            if ((r >> 8) % 2) c.set(x, y + 2, z, GLASS_REEDS);
                        } else if (r % 120 == 7) {
                            c.set(x, y + 1, z, SPORE_PAD);
                        } else if (r % 300 == 11 && c.get(x, y + 8, z) == AIR) {
                            const int tall = 2 + (int)((r >> 12) % 5);
                            for (int k = 1; k <= tall; ++k) c.set(x, y + k, z, PRISMATIDE_PILLAR);
                        }
                    } else {
                        // Overgrown Monolith: mossy cornerstone and grass, starberries and rare truffles.
                        c.set(x, y, z, r % 2 ? (ItemId)MOSSY_CORNERSTONE : (ItemId)NETHER_GRASS);
                        if (!room) continue;
                        if (r % 37 == 0) c.set(x, y + 1, z, STARBERRY_BUSH);
                        else if (r % 6 == 0) c.set(x, y + 1, z, NETHER_SPROUTS);
                    }
                    continue;
                }
                if (wasteland) {
                    // 1.5.1: the Powerstone Wasteland: dry grass, Grasser flowers, debris and broken pillars.
                    const uint32_t r = hash3(wx, y, wz, seed_ ^ 0x9A57u);
                    const bool room = c.get(x, y + 2, z) == AIR;
                    c.set(x, y, z, r % 120 == 0 ? (ItemId)DEBRIS_GRAVEL : (ItemId)WASTELAND_GRASS);
                    if (!room) continue;
                    if (r % 400 == 7 && y + 6 < kGenHeight && c.get(x, y + 5, z) == AIR) {
                        const int tall = 2 + (int)((r >> 12) % 3);
                        for (int k = 1; k <= tall; ++k)
                            c.set(x, y + k, z, k == tall ? (ItemId)CHISELED_POWERSTONE_BRICKS : (r >> (k + 3)) & 1 ? (ItemId)CRACKED_POWERSTONE_BRICKS : (ItemId)POWERSTONE_BRICKS);
                    } else if (r % 9 < 2) {
                        c.set(x, y + 1, z, DRY_GRASS);
                    } else if (r % 37 == 3) {
                        c.set(x, y + 1, z, GRASSER_FLOWER);
                    }
                } else if (meadow && soul <= floorThreshold) {
                    // The grassy Nether: nether grass over netherrack, with sprouts here and there.
                    c.set(x, y, z, NETHER_GRASS);
                    if (hash3(wx, y, wz, seed_ ^ 0x5B0Du) % 5 == 0 && c.get(x, y + 2, z) == AIR) c.set(x, y + 1, z, NETHER_SPROUTS);
                } else if (soul > floorThreshold) {
                    c.set(x, y, z, nd.floor);
                    if (c.get(x, y - 1, z) == nd.fill) c.set(x, y - 1, z, nd.floor);
                } else if (y <= nd.lavaLevel + 3 && soul < -0.35f) {
                    c.set(x, y, z, nd.shore);
                }
            }
        }

    // Glowstone clusters hanging from cave ceilings.
    uint32_t r = hash3(c.cx, 0, c.cz, seed_ ^ 0x6105u);
    int clusters = nd.glowstoneClusters > 0 ? (int)(r % (uint32_t)(nd.glowstoneClusters + 1)) : 0;
    for (int i = 0; i < clusters; ++i) {
        uint32_t ri = hash3(c.cx, i, c.cz, seed_ ^ 0x6105u);
        int x = 3 + (int)(ri % 10), z = 3 + (int)((ri >> 5) % 10);
        int y = kGenHeight - 8;
        while (y > 40 && !(c.get(x, y, z) == AIR && isSolid(c.get(x, y + 1, z)))) --y;
        if (y <= 40) continue;
        for (int k = 0; k < 40; ++k) {
            uint32_t q = hash3(c.cx * 97 + i, k, c.cz, seed_);
            int dx = (int)(q % 5) - 2, dz = (int)((q >> 3) % 5) - 2, dy = -(int)((q >> 6) % 5);
            if (c.get(x + dx, y + dy, z + dz) == AIR) c.set(x + dx, y + dy, z + dz, POWERSTONE_LIGHT);
        }
    }

    // 2.2.0 preview: the giant stonewood trees of the Overgrown Monolith: a 2 x 2 trunk of
    // petrified bark, a wide crown of glowcap leaves with glowing strands hanging from it, and
    // the rare void truffle between the roots.
    if (kFeatures220 && nd.fill == NETHERRACK) {
        const uint32_t tr = hash3(c.cx, 4242, c.cz, seed_ ^ 0x570Eu);
        const int tx = 5 + (int)(tr % 5), tz = 5 + (int)((tr >> 4) % 5);
        const int twx = c.cx * kChunkSize + tx, twz = c.cz * kChunkSize + tz;
        if (tr % 3 == 0 && cornerBiomeAt(twx, twz) == 3)
            for (int y = nd.lavaLevel + 2; y < kGenHeight - 26; ++y) {
                const ItemId g = c.get(tx, y, tz);
                if (g != MOSSY_CORNERSTONE && g != NETHER_GRASS) continue;
                const int height = 12 + (int)((tr >> 8) % 8);
                bool room = true;
                for (int k = 1; k <= height + 2 && room; ++k)
                    for (int dx = 0; dx < 2 && room; ++dx)
                        for (int dz = 0; dz < 2 && room; ++dz) {
                            const ItemId b = c.get(tx + dx, y + k, tz + dz);
                            room = b == AIR || isPlant(b);
                        }
                if (!room) continue;
                for (int k = 1; k <= height; ++k)
                    for (int dx = 0; dx < 2; ++dx)
                        for (int dz = 0; dz < 2; ++dz) c.set(tx + dx, y + k, tz + dz, STONEWOOD_LOG);
                const int top = y + height;
                auto inChunk = [](int v) { return v >= 0 && v < kChunkSize; };
                for (int dy = -2; dy <= 2; ++dy) {
                    const int rad = dy == 2 ? 3 : dy == -2 ? 4 : 5;
                    for (int dx = -rad; dx <= rad + 1; ++dx)
                        for (int dz = -rad; dz <= rad + 1; ++dz) {
                            const float ox = dx - 0.5f, oz = dz - 0.5f;
                            if (ox * ox + oz * oz > rad * rad + 1) continue;
                            const int lx = tx + dx, lz = tz + dz;
                            if (!inChunk(lx) || !inChunk(lz)) continue;
                            if (c.get(lx, top + dy, lz) == AIR) c.set(lx, top + dy, lz, GLOWCAP_LEAVES);
                        }
                }
                // Glowing strands hanging from the crown's edge.
                for (int k = 0; k < 10; ++k) {
                    const uint32_t q = hash3(c.cx * 31 + k, top, c.cz, seed_ ^ 0x9A1Eu);
                    const int lx = tx + (int)(q % 11) - 5, lz = tz + (int)((q >> 4) % 11) - 5;
                    if (!inChunk(lx) || !inChunk(lz)) continue;
                    const int len = 2 + (int)((q >> 8) % 5);
                    for (int d = 1; d <= len && c.get(lx, top - 3 - d + 1, lz) == AIR; ++d) c.set(lx, top - 3 - d + 1, lz, GLOWCAP_LEAVES);
                }
                // A truffle by the roots now and then.
                if ((tr >> 20) % 3 == 0 && inChunk(tx + 2) && c.get(tx + 2, y + 1, tz) == AIR) c.set(tx + 2, y + 1, tz, VOID_TRUFFLE);
                break;
            }
    }

    // Netherperk trees in the meadows: a red stem and a glowing orange crown.
    if (nd.fill == NETHERRACK)
        for (int x = 3; x <= 12; ++x)
            for (int z = 3; z <= 12; ++z) {
                const int wx = c.cx * kChunkSize + x, wz = c.cz * kChunkSize + z;
                uint32_t r = hash3(wx, 77, wz, seed_ ^ 0x9E7Bu);
                if (r % 1000 >= 14 || !netherGrassAt(wx, wz)) continue;
                for (int y = nd.lavaLevel + 2; y < kGenHeight - 16; ++y) {
                    if (c.get(x, y, z) != NETHER_GRASS) continue;
                    const int stem = 4 + (int)((r >> 10) % 4);
                    bool room = true;
                    for (int k = 1; k <= stem + 3 && room; ++k) room = c.get(x, y + k, z) == AIR || c.get(x, y + k, z) == NETHER_SPROUTS;
                    if (!room) continue;
                    for (int k = 1; k <= stem; ++k) c.set(x, y + k, z, NETHERPERK_LOG);
                    const int top = y + stem;
                    for (int dy = -1; dy <= 2; ++dy) {
                        int rad = dy == 2 ? 1 : dy == -1 ? 3 : 3 - (dy == 1);
                        for (int dx = -rad; dx <= rad; ++dx)
                            for (int dz = -rad; dz <= rad; ++dz) {
                                if (dx * dx + dz * dz > rad * rad + 1) continue;
                                if (dy == -1 && dx * dx + dz * dz < (rad - 1) * (rad - 1)) continue;  // hollow underside
                                if (c.get(x + dx, top + dy, z + dz) == AIR) c.set(x + dx, top + dy, z + dz, NETHERPERK_CAP);
                            }
                    }
                    break;
                }
            }

    // Fortresses of nether bricks.
    if (!config_.overworld.structures) return;
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize;
    const WorldStructure fortressKind = config_.generation >= 2 ? WorldStructure::GreatFortress : WorldStructure::Fortress;
    for (int rx = floorDiv(x0 - 90, kFortressRegion); rx <= floorDiv(x0 + 15, kFortressRegion); ++rx)
        for (int rz = floorDiv(z0 - 90, kFortressRegion); rz <= floorDiv(z0 + 15, kFortressRegion); ++rz) {
            int ox, oy, oz;
            uint32_t h;
            if (!fortressIn(rx, rz, ox, oy, oz, h)) continue;
            stampBlueprint(c, blueprint((int)fortressKind, rx, rz, h), ox, oy, oz, true);
        }
    // 1.5.7: crimson mineshafts tunnel through the rock.
    for (int rx = floorDiv(x0 - 96, kCrimsonShaftRegion); rx <= floorDiv(x0 + 15, kCrimsonShaftRegion); ++rx)
        for (int rz = floorDiv(z0 - 96, kCrimsonShaftRegion); rz <= floorDiv(z0 + 15, kCrimsonShaftRegion); ++rz) {
            int ox, oy, oz;
            if (!crimsonShaftIn(rx, rz, ox, oy, oz)) continue;
            stampBlueprint(c, blueprint((int)WorldStructure::CrimsonMineshaft, rx, rz, hash3(rx, 1577, rz, seed_ ^ 0xC21Du)), ox, oy, oz, false);
        }
    // 1.5.1: forgotten villages lie in hollows of the wasteland, on cornerstone foundations.
    if (nd.fill != NETHERRACK) return;
    const int n = kForgottenVillageSize, half = n / 2;
    for (int rx = floorDiv(x0 - n, kForgottenRegion); rx <= floorDiv(x0 + 15, kForgottenRegion); ++rx)
        for (int rz = floorDiv(z0 - n, kForgottenRegion); rz <= floorDiv(z0 + 15, kForgottenRegion); ++rz) {
            int vx, vy, vz;
            if (!forgottenVillageIn(rx, rz, vx, vy, vz)) continue;
            const uint32_t h = hash3(rx, 7777, rz, seed_ ^ 0xF06Fu);
            const Structure& s0 = blueprint((int)WorldStructure::ForgottenVillage, rx, rz, h);
            // 2.4.0 (generation 7): open ground of the plan is left out, so the village sits in the land
            // instead of on a round plate standing on pillars of netherrack.
            Structure open;
            if (config_.generation >= 7) {
                open = s0;
                for (int z = 0; z < open.sz; ++z)
                    for (int x = 0; x < open.sx; ++x) {
                        bool built = false;
                        for (int y = 0; y < open.sy && !built; ++y) {
                            const ItemId id = open.at(x, y, z);
                            built = !(id == Structure::kVoid || id == AIR || isPlant(id) || (y == 0 && (id == WASTELAND_GRASS || id == DEBRIS_GRAVEL || id == GRAVEL)));
                        }
                        if (!built)
                            for (int y = 0; y < open.sy; ++y) open.at(x, y, z) = Structure::kVoid;
                    }
            }
            const Structure& s = config_.generation >= 7 ? open : s0;
            const int ox = vx - half, oy = vy - 1, oz = vz - half;
            stampBlueprint(c, s, ox, oy, oz, false);
            // Foundations of cornerstone down to the ground, so no house hangs over the lava.
            for (int z = std::max(0, z0 - oz); z < s.sz && oz + z < z0 + kChunkSize; ++z)
                for (int x = std::max(0, x0 - ox); x < s.sx && ox + x < x0 + kChunkSize; ++x) {
                    if (s.at(x, 0, z) == Structure::kVoid) continue;
                    const int lx = ox + x - x0, lz = oz + z - z0;
                    for (int y = oy - 1; y > 4; --y) {
                        const ItemId b = c.get(lx, y, lz);
                        if (isSolid(b) && b != NETHER_SPROUTS) break;
                        c.set(lx, y, lz, NETHERRACK);
                    }
                }
        }
}

int World::cornerBiomeAt(int x, int z) const {
    if (!kFeatures220 || dim_ != Dimension::Nether || config_.nether.fill != NETHERRACK || wastelandAt(x, z)) return 0;
    const float a = humidity_.perlin2(x / 170.0f + 1300, z / 170.0f - 900);
    const float b = humidity_.perlin2(x / 170.0f - 700, z / 170.0f + 500);
    if (a > 0.18f) return 1;
    if (a < -0.22f) return 2;
    if (b > 0.2f) return 3;
    return 0;
}

bool World::wastelandAt(int x, int z) const {
    return dim_ == Dimension::Nether && config_.nether.fill == NETHERRACK && humidity_.perlin2(x / 150.0f + 400, z / 150.0f - 200) > 0.12f;
}

bool World::forgottenVillageIn(int rx, int rz, int& cx, int& cy, int& cz) const {
    if (dim_ != Dimension::Nether || !config_.overworld.structures || config_.nether.fill != NETHERRACK) return false;
    const uint32_t h = hash3(rx, 7777, rz, seed_ ^ 0xF06Fu);
    if (h % 100 >= 60) return false;
    const int margin = kForgottenVillageSize / 2 + 4;
    cx = rx * kForgottenRegion + margin + (int)((h >> 8) % (uint32_t)(kForgottenRegion - 2 * margin));
    cz = rz * kForgottenRegion + margin + (int)((h >> 16) % (uint32_t)(kForgottenRegion - 2 * margin));
    if (!wastelandAt(cx, cz)) return false;
    cy = config_.nether.lavaLevel + 8 + (int)((h >> 4) % 8);
    return true;
}

// ---------------------------------------------------------------- 1.1.0 lands

bool World::netherGrassAt(int x, int z) const { return temperature_.perlin2(x / 96.0f, z / 96.0f) > 0.05f; }

bool World::crimsonShaftIn(int rx, int rz, int& ox, int& oy, int& oz) const {
    if (dim_ != Dimension::Nether || !config_.overworld.structures || config_.generation < 2) return false;
    const uint32_t h = hash3(rx, 1577, rz, seed_ ^ 0xC21Du);
    if (h % 100 >= 50) return false;
    ox = rx * kCrimsonShaftRegion + 8 + (int)((h >> 8) % (uint32_t)(kCrimsonShaftRegion - 96 - 16));
    oz = rz * kCrimsonShaftRegion + 8 + (int)((h >> 16) % (uint32_t)(kCrimsonShaftRegion - 96 - 16));
    oy = config_.nether.lavaLevel + 4 + (int)((h >> 4) % 18);
    return true;
}

bool World::fortressIn(int rx, int rz, int& ox, int& oy, int& oz, uint32_t& h) const {
    h = hash3(rx, 4441, rz, seed_ ^ 0xF0F7u);
    // The region around the world's centre always has one, so the first trip finds it.
    if (!(rx == 0 && rz == 0) && h % 100 >= 55) return false;
    const int size = config_.generation >= 2 ? 85 : 53;
    ox = rx * kFortressRegion + 8 + (int)((h >> 8) % (uint32_t)(kFortressRegion - size - 16));
    oz = rz * kFortressRegion + 8 + (int)((h >> 16) % (uint32_t)(kFortressRegion - size - 16));
    oy = 40 + (int)((h >> 4) % 14);
    return true;
}

void World::arenaCentre(int rx, int rz, int& x, int& z) const {
    uint32_t h = hash3(rx, 7331, rz, seed_ ^ 0xA2E4u);
    x = rx * kArenaRegion + kArenaRegion / 2 + (int)(h % 161) - 80;
    z = rz * kArenaRegion + kArenaRegion / 2 + (int)((h >> 9) % 161) - 80;
}

float World::arenaInfluence(int x, int z) const {
    int cx, cz;
    arenaCentre(floorDiv(x, kArenaRegion), floorDiv(z, kArenaRegion), cx, cz);
    float d = std::sqrt((float)(x - cx) * (x - cx) + (float)(z - cz) * (z - cz));
    return std::clamp(1.0f - (d - 30) / 110.0f, 0.0f, 1.0f);
}

const Structure& World::blueprint(int kind, int rx, int rz, uint32_t h) {
    const uint64_t key = ((uint64_t)(uint32_t)rx << 34) ^ ((uint64_t)(uint32_t)rz << 4) ^ (uint64_t)kind;
    auto it = blueprints_.find(key);
    if (it == blueprints_.end()) {
        if (blueprints_.size() > 48) blueprints_.clear();
        Structure s = buildWorldStructure((WorldStructure)kind, h * 2654435761u);
        it = blueprints_.emplace(key, std::make_shared<Structure>(std::move(s))).first;
    }
    return *it->second;
}

void World::stampBlueprint(Chunk& c, const Structure& s, int ox, int oy, int oz, bool pillars) {
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize, x1 = x0 + kChunkSize - 1, z1 = z0 + kChunkSize - 1;
    if (ox > x1 || oz > z1 || ox + s.sx <= x0 || oz + s.sz <= z0) return;
    for (int y = 0; y < s.sy; ++y) {
        const int wy = oy + y;
        if (wy < 1 || wy >= kGenHeight - 1) continue;
        for (int z = std::max(0, z0 - oz); z < s.sz && oz + z <= z1; ++z)
            for (int x = std::max(0, x0 - ox); x < s.sx && ox + x <= x1; ++x) {
                ItemId id = s.at(x, y, z);
                if (id == Structure::kVoid) continue;
                const int lx = ox + x - x0, lz = oz + z - z0;
                if (c.get(lx, wy, lz) == BEDROCK || c.get(lx, wy, lz) == WORLD_END) continue;
                c.set(lx, wy, lz, id);
                // Fortress pillars reach down to the ground (or the lava sea).
                if (pillars && y == 0 && id == NETHER_BRICK && (ox + x) % 6 == 0 && (oz + z) % 6 == 0)
                    for (int d = 1; d <= 50 && wy - d > 0; ++d) {
                        ItemId below = c.get(lx, wy - d, lz);
                        if (isSolid(below) && below != NETHER_BRICK) break;
                        c.set(lx, wy - d, lz, NETHER_BRICK);
                    }
            }
    }
}

World::VoidBiome World::voidBiomeAt(int x, int z) const {
    float lava = std::max(arenaInfluence(x, z) * 1.4f, (terrain_.perlin2(x / 260.0f + 40, z / 260.0f) + 0.6f) * 0.55f);
    if (lava > 0.6f) return VoidBiome::Lava;
    const bool purple = temperature_.perlin2(x / 180.0f + 50, z / 180.0f) > -0.05f;
    // 1.6.0: inside the purple lands, broken pieces of a forgotten overworld drift.
    if (purple && config_.generation >= 4 && detail_.perlin2(x / 150.0f - 620.0f, z / 150.0f + 410.0f) > 0.28f) return VoidBiome::Forgotten;
    if (config_.generation >= 5) {
        // 1.7.2 build 3: the expansion's lands, each from its own slow noise.
        const float e = humidity_.perlin2(x / 170.0f + 77.0f, z / 170.0f - 33.0f);
        if (purple && e > 0.22f) return VoidBiome::CrimsoniumForest;
        if (!purple && e > 0.2f) return VoidBiome::ShardDesert;
        if (e < -0.3f) return VoidBiome::Shattered;
    }
    return purple ? VoidBiome::Purple : VoidBiome::Crimson;
}

int World::voidHeight(int x, int z) const {
    float lava = std::max(arenaInfluence(x, z) * 1.4f, (terrain_.perlin2(x / 260.0f + 40, z / 260.0f) + 0.6f) * 0.55f);
    float purple = temperature_.perlin2(x / 180.0f + 50, z / 180.0f);
    auto smooth = [](float a, float b, float v) {
        float t = std::clamp((v - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3 - 2 * t);
    };
    float hp = 68 + terrain_.perlin2(x / 48.0f, z / 48.0f) * 12 + detail_.perlin2(x / 12.0f, z / 12.0f) * 3;
    float hc = 64 + terrain_.perlin2(x / 70.0f + 9, z / 70.0f) * 5 + std::sin(x / 9.0f + detail_.perlin2(x / 30.0f, z / 30.0f) * 4) * 1.5f;
    if (config_.crimsonDesert >= 2) {
        // 1.5.1: sharp-crested dunes across the wind, and wide hollows between them.
        const float dune = 1 - std::fabs(std::sin((x * 0.8f + z * 0.55f) / 9.0f + detail_.perlin2(x / 40.0f + 70, z / 40.0f) * 3));
        const float hollow = std::clamp((-detail_.perlin2(x / 55.0f + 300, z / 55.0f) - 0.3f) * 10, 0.0f, 1.0f);
        hc += dune * dune * 3.2f - hollow * 4.0f;
    }
    float hl = 54 + terrain_.perlin2(x / 40.0f, z / 40.0f + 3) * 7;
    float wl = smooth(0.45f, 0.7f, lava), wp = smooth(-0.15f, 0.05f, purple);
    float h = (1 - wl) * (wp * hp + (1 - wp) * hc) + wl * hl;
    // The arena itself sits on flat ground just above the lava.
    float a = std::clamp((arenaInfluence(x, z) - 0.75f) * 4, 0.0f, 1.0f);
    h = h * (1 - a) + (kVoidLavaLevel + 1) * a;
    return std::clamp((int)h, 8, kGenHeight - 20);
}

std::string World::placeName(int x, int z) const {
    switch (dim_) {
        case Dimension::Nether:
            if (const int cb = cornerBiomeAt(x, z)) return cb == 1 ? "Zlatavý lom" : cb == 2 ? "Prizmatické pobřeží" : "Zarostlý monolit";
            return wastelandAt(x, z) ? "Powerstonová pustina" : netherGrassAt(x, z) ? "Louka úhelného kamene" : "Dimenze úhelného kamene";
        case Dimension::Celestial: return "Nebeská říše";
        case Dimension::Void:
            switch (voidBiomeAt(x, z)) {
                case VoidBiome::Purple: return "Fialový les";
                case VoidBiome::Crimson: return "Karmínová poušť";
                case VoidBiome::Forgotten: return "Zapomenutý Overworld";
                case VoidBiome::CrimsoniumForest: return "Crimsoniový les";
                case VoidBiome::Shattered: return "Roztříštěná prázdnota";
                case VoidBiome::ShardDesert: return "Poušť prázdnotných střepů";
                default: return "Lávová pustina";
            }
        default: {
            const BiomeDef* b = biomeAt(x, z);
            return b ? b->name : "";
        }
    }
}

bool World::ruinedStationIn(int rx, int rz, int& cx, int& cz) const {
    // The same choice stampStructures makes for WorldStructure::RuinedStation.
    if (dim_ != Dimension::Overworld || !config_.overworld.structures || config_.generation < 7 || !kFeatures240) return false;
    const uint32_t h = hash3(rx, (int)WorldStructure::RuinedStation * 977 + 13, rz, seed_ ^ 0x57A7u);
    if ((int)(h % 100) >= 45) return false;
    const int ox = rx * kStationRegion + (int)((h >> 8) % (uint32_t)(kStationRegion - 23));
    const int oz = rz * kStationRegion + (int)((h >> 16) % (uint32_t)(kStationRegion - 23));
    Column middle = column(ox + 11, oz + 3);
    if (!middle.biome || middle.biome->peakBlock || middle.height <= config_.overworld.seaLevel + 1) return false;
    cx = ox + 11, cz = oz + 3;
    return true;
}

bool World::villageIn(int rx, int rz, int& cx, int& cy, int& cz) const {
    // The same choice stampStructures makes for WorldStructure::Village.
    if (dim_ != Dimension::Overworld || !config_.overworld.structures) return false;
    uint32_t h = hash3(rx, (int)WorldStructure::Village * 977 + 13, rz, seed_ ^ 0x57A7u);
    if ((int)(h % 100) >= 80) return false;
    const int ox = rx * kVillageRegion + (int)((h >> 8) % (uint32_t)(kVillageRegion - kVillageSize));
    const int oz = rz * kVillageRegion + (int)((h >> 16) % (uint32_t)(kVillageRegion - kVillageSize));
    Column middle = column(ox + kVillageSize / 2, oz + kVillageSize / 2);
    const BiomeDef* mb = middle.biome;
    if (!mb || mb->peakBlock || middle.height <= config_.overworld.seaLevel + 1) return false;
    if (mb->cactusDensity > 0 || mb->id == "desert" || mb->freeze || mb->id == "snowy_taiga" || mb->id == "ice_plains") return false;
    cx = ox + kVillageSize / 2;
    cy = middle.height + 2;
    cz = oz + kVillageSize / 2;
    return true;
}

void World::celestialSpot(int rx, int rz, int& px, int& pz) const {
    const int region = kCelestialRegion;
    const uint32_t hh = hash3(rx, 1552, rz, seed_ ^ 0xCE1Eu);
    px = rx * region + 16 + (int)(hh % (uint32_t)(region - 32)), pz = rz * region + 16 + (int)((hh >> 12) % (uint32_t)(region - 32));
    if (config_.generation >= 6) {
        // 1.9.0: always in the middle part of a chunk, so the frame is really built there (near a
        // chunk edge it used to be skipped, and the compass pointed at nothing).
        px = floorDiv(px, kChunkSize) * kChunkSize + 4 + (int)((hh >> 24) % 8);
        pz = floorDiv(pz, kChunkSize) * kChunkSize + 4 + (int)((hh >> 4) % 8);
    }
}

void World::templeCentre(int rx, int rz, int& tx, int& tz) const {
    if (rx == 0 && rz == 0) {
        tx = 96, tz = 40;  // close to where players arrive
        return;
    }
    const uint32_t h = hash3(rx, 1996, rz, seed_ ^ 0x7E3Bu);
    tx = rx * kTempleRegion + 40 + (int)(h % (uint32_t)(kTempleRegion - 80));
    tz = rz * kTempleRegion + 40 + (int)((h >> 12) % (uint32_t)(kTempleRegion - 80));
}

// 1.9.5: the Celestial Temple: a round island of white rock with a floor of bricks, a ring of
// pillars crowned with crystals and a raised dais in the middle where the Sky Guardian waits.
void World::stampCelestialTemple(Chunk& c) {
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize, R = kTempleRadius, Y = kTempleY;
    for (int rx = floorDiv(x0 - R, kTempleRegion); rx <= floorDiv(x0 + 15 + R, kTempleRegion); ++rx)
        for (int rz = floorDiv(z0 - R, kTempleRegion); rz <= floorDiv(z0 + 15 + R, kTempleRegion); ++rz) {
            int tx, tz;
            templeCentre(rx, rz, tx, tz);
            if (tx + R < x0 || tx - R > x0 + 15 || tz + R < z0 || tz - R > z0 + 15) continue;
            for (int lx = 0; lx < kChunkSize; ++lx)
                for (int lz = 0; lz < kChunkSize; ++lz) {
                    const int dx = x0 + lx - tx, dz = z0 + lz - tz;
                    const float d = std::sqrt((float)(dx * dx + dz * dz));
                    if (d > R + 0.5f) continue;
                    for (int y = Y + 1; y < Y + 14; ++y) c.set(lx, y, lz, AIR);
                    // The island under the floor narrows downwards.
                    const int depth = 2 + (int)((R - d) * 0.7f);
                    for (int k = 1; k <= depth; ++k) c.set(lx, Y - k, lz, (Y - k) % 4 == 0 ? SKY_STONE : PALE_STONE);
                    c.set(lx, Y, lz, d > R - 1.5f ? PALE_STONE : (dx + dz) % 2 == 0 ? CELESTIAL_STONE_BRICKS : PALE_STONE);
                    if (d < 3.5f) c.set(lx, Y + 1, lz, d < 1.5f ? PALE_CRYSTAL : CELESTIAL_STONE_BRICKS);  // the dais
                    // Twelve pillars on a ring.
                    const float a = std::atan2((float)dz, (float)dx);
                    const float sector = std::fmod(a + kPi * 2, kPi / 6);
                    if (d > R - 4.5f && d < R - 2.5f && (sector < 0.12f || sector > kPi / 6 - 0.12f)) {
                        for (int y = Y + 1; y <= Y + 7; ++y) c.set(lx, y, lz, y == Y + 7 ? PALE_CRYSTAL : PALE_STONE);
                    }
                }
        }
}

bool World::nearestCelestialFrame(int x, int z, int& outX, int& outZ) const {
    if (dim_ != Dimension::Void || config_.generation < 4) return false;
    const int rx0 = floorDiv(x, kCelestialRegion), rz0 = floorDiv(z, kCelestialRegion);
    long best = -1;
    for (int rx = rx0 - 4; rx <= rx0 + 4; ++rx)
        for (int rz = rz0 - 4; rz <= rz0 + 4; ++rz) {
            int px, pz;
            celestialSpot(rx, rz, px, pz);
            if (voidBiomeAt(px, pz) != VoidBiome::Forgotten) continue;
            const long d = (long)(px - x) * (px - x) + (long)(pz - z) * (pz - z);
            if (best < 0 || d < best) best = d, outX = px, outZ = pz;
        }
    return best >= 0;
}

bool World::nearestStructure(int x, int z, int& outX, int& outY, int& outZ) const {
    long best = -1;
    if (dim_ == Dimension::Nether) {
        const int brx = floorDiv(x, kFortressRegion), brz = floorDiv(z, kFortressRegion);
        for (int rx = brx - 4; rx <= brx + 4; ++rx)
            for (int rz = brz - 4; rz <= brz + 4; ++rz) {
                int ox, oy, oz;
                uint32_t h;
                if (!fortressIn(rx, rz, ox, oy, oz, h)) continue;
                const int half = config_.generation >= 2 ? 42 : 26;  // the great hall in the middle
                long d = (long)(ox + half - x) * (ox + half - x) + (long)(oz + half - z) * (oz + half - z);
                if (best < 0 || d < best) {
                    best = d;
                    outX = ox + half;
                    outY = oy + 1;
                    outZ = oz + half;
                }
            }
    } else if (dim_ == Dimension::Overworld) {
        // Villages.
        const int brx = floorDiv(x, kVillageRegion), brz = floorDiv(z, kVillageRegion);
        for (int rx = brx - 3; rx <= brx + 3; ++rx)
            for (int rz = brz - 3; rz <= brz + 3; ++rz) {
                int cx, cy, cz;
                if (!villageIn(rx, rz, cx, cy, cz)) continue;
                long d = (long)(cx - x) * (cx - x) + (long)(cz - z) * (cz - z);
                if (best < 0 || d < best) {
                    best = d;
                    outX = cx;
                    outY = cy;
                    outZ = cz;
                }
            }
    } else if (dim_ == Dimension::Celestial && config_.generation >= 6) {
        // 1.9.5: the Celestial Temples.
        const int brx = floorDiv(x, kTempleRegion), brz = floorDiv(z, kTempleRegion);
        for (int rx = brx - 1; rx <= brx + 1; ++rx)
            for (int rz = brz - 1; rz <= brz + 1; ++rz) {
                int tx, tz;
                templeCentre(rx, rz, tx, tz);
                const long d = (long)(tx - x) * (tx - x) + (long)(tz - z) * (tz - z);
                if (best < 0 || d < best) best = d, outX = tx, outY = kTempleY + 1, outZ = tz;
            }
    } else if (dim_ == Dimension::Void) {
        const int brx = floorDiv(x, kArenaRegion), brz = floorDiv(z, kArenaRegion);
        for (int rx = brx - 1; rx <= brx + 1; ++rx)
            for (int rz = brz - 1; rz <= brz + 1; ++rz) {
                int cx, cz;
                arenaCentre(rx, rz, cx, cz);
                long d = (long)(cx - x) * (cx - x) + (long)(cz - z) * (cz - z);
                if (best < 0 || d < best) {
                    best = d;
                    outX = cx;
                    outY = kVoidLavaLevel + 2;
                    outZ = cz;
                }
            }
    }
    if (best >= 0) outY += yShift();
    return best >= 0;
}

ItemId World::forgottenBlock(int wx, int y, int wz, int h) const {
    // 1.6.0: the Forgotten Overworld: the land is cut into pieces of 8 x 8 columns. Some are
    // gone (a drop to the void floor far below), the others float at their own height on a
    // ragged underside: souled grass, a few layers of souled dirt and then souled stone.
    if (y == 0) return WORLD_END;
    if (y < 3) return (hash3(wx, y, wz, seed_ ^ 0x701Du) & 1) ? (ItemId)BEDROCK : (ItemId)VOID_STONE;
    if (y < 26) return VOID_STONE;
    const uint32_t cell = hash3(floorDiv(wx, 8), 0, floorDiv(wz, 8), seed_ ^ 0xF06Fu);
    if (cell % 100 < 28) return AIR;  // a missing piece
    const int top = forgottenTop(wx, wz, h);
    const int bottom = top - 10 - (int)(hash3(wx, 1, wz, seed_ ^ 0xF07Fu) % 5) - (int)(cell % 4);
    if (y > top || y < bottom) return AIR;
    if (y == top) return SOULED_GRASS;
    if (y >= top - 3) return SOULED_DIRT;
    return SOULED_STONE;
}

int World::forgottenTop(int wx, int wz, int h) const {
    const uint32_t cell = hash3(floorDiv(wx, 8), 0, floorDiv(wz, 8), seed_ ^ 0xF06Fu);
    return h + (int)((cell >> 8) % 9) - 4;  // each piece at its own height
}

// ---------------------------------------------------------------- 1.7.0 the Celestial Reach

int World::celestialTop(int x, int z) const {
    // Islands where a slow and a quick noise together rise over a threshold; a big one always
    // floats around the arrival point (0, 0).
    float m = detail_.perlin2(x / 52.0f + 911.0f, z / 52.0f - 377.0f) * 0.65f + detail_.perlin2(x / 17.0f - 51.0f, z / 17.0f + 83.0f) * 0.35f;
    const float d = std::sqrt((float)(x * x + z * z));
    if (d < 14) m = std::max(m, 0.16f + (14 - d) / 14 * 0.45f);
    if (m < 0.16f) return -1;
    const float base = 74 + detail_.perlin2(x / 95.0f + 300.0f, z / 95.0f - 120.0f) * 16;
    return (int)(base + (m - 0.16f) * 9);
}

void World::generateCelestial(Chunk& c) {
    // Floating islands of celestial soul grass over an empty sky: nothing below, a fall is a
    // fall. Pale trees, broken pillars of a lost kingdom, and chains hanging under the rock.
    const int cx = c.cx, cz = c.cz;
    int tops[kChunkSize][kChunkSize];
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            const int top = celestialTop(wx, wz);
            tops[x][z] = top;
            if (top < 0) continue;
            float m = detail_.perlin2(wx / 52.0f + 911.0f, wz / 52.0f - 377.0f) * 0.65f + detail_.perlin2(wx / 17.0f - 51.0f, wz / 17.0f + 83.0f) * 0.35f;
            const float d = std::sqrt((float)(wx * wx + wz * wz));
            if (d < 14) m = std::max(m, 0.16f + (14 - d) / 14 * 0.45f);
            const int depth = 3 + (int)((m - 0.16f) * 55 + caves_.perlin2(wx / 7.0f, wz / 7.0f) * 3);
            const bool newRock = config_.generation >= 6;
            for (int k = 0; k <= depth && top - k > 1; ++k) {
                ItemId b = k == 0 ? CELESTIAL_GRASS : k < 3 ? CELESTIAL_DIRT : CELESTIAL_STONE;
                if (newRock && k >= 3) {
                    // 1.9.0: white rock with blue bands, crystal veins and celestium in the deep part.
                    const int yy = top - k;
                    b = (yy + (int)(detail_.perlin2(wx / 9.0f, wz / 9.0f) * 3)) % 7 < 2 ? SKY_STONE : PALE_STONE;
                    const uint32_t v = hash3(wx, yy, wz, seed_ ^ 0x5EEDu);
                    if (k > 5 && v % 61 == 0) b = CELESTIUM_ORE;
                    else if (detail_.perlin3(wx / 6.0f, yy / 6.0f, wz / 6.0f) > 0.62f) b = PALE_CRYSTAL;
                }
                c.set(x, top - k, z, b);
            }
            if (newRock && hash3(wx, 5, wz, seed_ ^ 0xC2u) % 37 == 0)
                // Crystal spikes hang from the underside of the islands.
                for (int k = 1; k < 2 + (int)(hash3(wx, 6, wz, seed_) % 4) && top - depth - k > 1; ++k) c.set(x, top - depth - k, z, PALE_CRYSTAL);
            // A chain hanging from the underside now and then.
            const uint32_t r = hash3(wx, 7, wz, seed_ ^ 0xC3A1u);
            if (r % 113 == 0)
                for (int k = 1; k < 4 + (int)((r >> 8) % 6) && top - depth - k > 1; ++k) c.set(x, top - depth - k, z, VOID_FENCE);
        }
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int top = tops[x][z];
            if (top < 0) continue;
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            const uint32_t r = hash3(wx, 0, wz, seed_ ^ 0xCE1Au);
            if (x >= 2 && x <= 13 && z >= 2 && z <= 13 && r % 83 == 0 && wx * wx + wz * wz > 9) {
                // A celestial tree: pale trunk, a round crown of see-through sky leaves.
                const int h = 4 + (int)((r >> 6) % 3);
                for (int k = 1; k <= h; ++k) c.set(x, top + k, z, CELESTIAL_LOG);
                for (int dx = -2; dx <= 2; ++dx)
                    for (int dz = -2; dz <= 2; ++dz)
                        for (int dy = h - 1; dy <= h + 2; ++dy) {
                            const int rr = dy >= h + 1 ? 1 : 2;
                            if (std::abs(dx) > rr || std::abs(dz) > rr || (rr == 2 && std::abs(dx) == 2 && std::abs(dz) == 2)) continue;
                            if (c.get(x + dx, top + dy, z + dz) == AIR) c.set(x + dx, top + dy, z + dz, CELESTIAL_LEAVES);
                        }
            } else if (r % 509 == 0) {
                // A broken pillar of the lost kingdom (1.9.0: of white rock with a crystal on top).
                const int h = 2 + (int)((r >> 5) % 5);
                for (int k = 1; k <= h; ++k) c.set(x, top + k, z, config_.generation >= 6 && k % 3 == 0 ? PALE_STONE : CELESTIAL_STONE_BRICKS);
                if (config_.generation >= 6) c.set(x, top + h + 1, z, PALE_CRYSTAL);
            } else if (config_.generation >= 6 && r % 1201 == 7 && x >= 3 && x <= 12 && z >= 3 && z <= 12) {
                // 1.9.0: a bridge remnant: a short broken deck of bricks and planks on two posts.
                const bool alongX = (r >> 9) & 1;
                for (int k = -3; k <= 3; ++k) {
                    const int bx = x + (alongX ? k : 0), bz = z + (alongX ? 0 : k);
                    if (hash3(bx, 9, bz, seed_) % 4 == 0) continue;  // missing planks
                    c.set(bx, top + 2, bz, std::abs(k) == 3 ? CELESTIAL_STONE_BRICKS : CELESTIAL_PLANKS);
                }
                for (int k : {-3, 3}) c.set(x + (alongX ? k : 0), top + 1, z + (alongX ? 0 : k), CELESTIAL_STONE_BRICKS);
            }
        }
    if (config_.generation >= 5) stampCelestialBridges(c);
    if (config_.generation >= 6) stampCelestialTemple(c);
}

// 1.8.0: bridges in the Celestial Reach. The sky is cut into a grid of kBridgeCell cells; each cell
// may have an anchor, a point on an island near its middle. Neighbouring anchors (east and south)
// are joined by a bridge when the line between them crosses open sky, never runs into higher
// ground and does not climb too steeply. Everything depends on the seed and the position only,
// so each chunk draws its own part of a bridge and the parts meet exactly at chunk borders.
namespace {
constexpr int kBridgeCell = 56;
struct BridgeSeg {
    float ax, az, bx, bz;
    int ay, by;
};
}  // namespace

void World::stampCelestialBridges(Chunk& c) {
    const int cx = c.cx, cz = c.cz;
    auto anchor = [&](int gx, int gz, int& ax, int& az, int& ay) {
        // The island ground closest to a point near the cell's middle (searched on a coarse grid).
        const uint32_t h = hash3(gx, 41, gz, seed_ ^ 0xB71Du);
        const int mx = gx * kBridgeCell + kBridgeCell / 2 + (int)(h % 9) - 4, mz = gz * kBridgeCell + kBridgeCell / 2 + (int)((h >> 8) % 9) - 4;
        int best = 1 << 30;
        ay = -1;
        for (int dx = -20; dx <= 20; dx += 4)
            for (int dz = -20; dz <= 20; dz += 4) {
                const int d = dx * dx + dz * dz, top = d < best ? celestialTop(mx + dx, mz + dz) : -1;
                if (top < 0) continue;
                best = d, ax = mx + dx, az = mz + dz, ay = top;
            }
        return ay >= 0;
    };
    std::vector<BridgeSeg> segs;
    const int g0x = floorDiv(cx * kChunkSize, kBridgeCell) - 1, g0z = floorDiv(cz * kChunkSize, kBridgeCell) - 1;
    const int g1x = floorDiv(cx * kChunkSize + kChunkSize - 1, kBridgeCell), g1z = floorDiv(cz * kChunkSize + kChunkSize - 1, kBridgeCell);
    for (int gx = g0x; gx <= g1x; ++gx)
        for (int gz = g0z; gz <= g1z; ++gz) {
            int ax, az, ay;
            if (!anchor(gx, gz, ax, az, ay)) continue;
            for (int dir = 0; dir < 2; ++dir) {
                const int ngx = gx + (dir == 0), ngz = gz + (dir == 1);
                // Not every pair is joined: about two in three.
                if (hash3(gx * 2 + dir, 43, gz, seed_ ^ 0x8B1Du) % 3 == 0) continue;
                int bx, bz, by;
                if (!anchor(ngx, ngz, bx, bz, by)) continue;
                const float len = std::sqrt((float)((bx - ax) * (bx - ax) + (bz - az) * (bz - az)));
                // The deck runs level, just over the highest ground on the way, and is only built
                // when both ends are close below it (no long climbs, no towers).
                bool gap = false;
                int deck = std::max(ay, by);
                for (float t = 0; t <= 1.0f; t += 1.0f / len) {
                    const int top = celestialTop((int)std::lround(ax + (bx - ax) * t), (int)std::lround(az + (bz - az) * t));
                    if (top < 0) gap = true;
                    deck = std::max(deck, top);
                }
                const bool blocked = deck - ay > 3 || deck - by > 3;
                if (gap && !blocked) segs.push_back({(float)ax, (float)az, (float)bx, (float)bz, deck + 1, deck + 1});
            }
        }
    if (segs.empty()) return;
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const float wx = (float)(cx * kChunkSize + x), wz = (float)(cz * kChunkSize + z);
            for (const BridgeSeg& s : segs) {
                const float dx = s.bx - s.ax, dz = s.bz - s.az, len2 = dx * dx + dz * dz;
                const float t = std::clamp(((wx - s.ax) * dx + (wz - s.az) * dz) / len2, 0.0f, 1.0f);
                const float px = s.ax + dx * t - wx, pz = s.az + dz * t - wz;
                const float dist = std::sqrt(px * px + pz * pz);
                if (dist > 1.8f) continue;
                const int deck = (int)std::lround(s.ay + (s.by - s.ay) * t);
                const bool edge = dist > 1.1f;
                const int along = (int)(t * std::sqrt(len2));
                if (c.get(x, deck, z) == AIR) c.set(x, deck, z, edge ? CELESTIAL_STONE_BRICKS : CELESTIAL_PLANKS);
                // Railings of brick slabs along both edges, with a pillar every eight blocks.
                if (edge && c.get(x, deck + 1, z) == AIR) {
                    const bool post = along % 8 == 0;
                    c.set(x, deck + 1, z, post ? (ItemId)CELESTIAL_STONE_BRICKS : slabId(21, 0));
                    if (post && c.get(x, deck + 2, z) == AIR) c.set(x, deck + 2, z, slabId(21, 0));
                }
                // Chains hang below the posts, the way the islands' own chains do.
                if (edge && along % 8 == 0)
                    for (int k = 1; k <= 3 && deck - k > 1; ++k)
                        if (c.get(x, deck - k, z) == AIR) c.set(x, deck - k, z, VOID_FENCE);
            }
        }
}

void World::generateVoid(Chunk& c) {
    // The Crimson Void: void stone everywhere; a purple forest of eye trees, a red sand
    // desert and lava lands with the boss arena.
    const int cx = c.cx, cz = c.cz;
    VoidBiome biomes[kChunkSize][kChunkSize];
    int heights[kChunkSize][kChunkSize];
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            const VoidBiome b = voidBiomeAt(wx, wz);
            int h = voidHeight(wx, wz);
            // 1.7.2 build 3: the Shattered Void is torn: steps and spikes of rock; the shard desert
            // rolls in long dark dunes.
            if (b == VoidBiome::Shattered) h += (int)(std::fabs(detail_.perlin2(wx / 14.0f + 51.0f, wz / 14.0f)) * 14);
            if (b == VoidBiome::ShardDesert) h += (int)((1 - std::fabs(std::sin((wx * 0.5f + wz * 0.8f) / 11.0f))) * 4);
            h = std::min(h, kGenHeight - 24);
            biomes[x][z] = b;
            heights[x][z] = h;
            for (int y = 0; y < kGenHeight; ++y) {
                ItemId id = AIR;
                uint32_t r = hash3(wx, y, wz, seed_ ^ 0x701Du);
                if (y == 0) id = WORLD_END;
                else if (y < 3 && (r & 1)) id = BEDROCK;
                else if (y < h) id = VOID_STONE;
                else if (y == h) id = b == VoidBiome::Purple ? VOID_GRASS : b == VoidBiome::Crimson ? RED_SAND : VOID_STONE;
                else if (b == VoidBiome::Lava && y <= kVoidLavaLevel) id = LAVA;
                if (b == VoidBiome::Crimson && y < h && y >= h - 4) id = RED_SAND;
                if (b == VoidBiome::Lava && y == h && r % 9 == 0) id = OBSIDIAN;
                if (b == VoidBiome::Forgotten) id = forgottenBlock(wx, y, wz, h);
                if (b == VoidBiome::CrimsoniumForest) {
                    if (y == h) id = VOID_GRASS;
                    else if (id == VOID_STONE && y > h - 12 && r % 170 == 0) id = CRIMSONIUM_ORE;  // the rare mineral, close under the floor
                } else if (b == VoidBiome::Shattered && (id == VOID_STONE || y == h)) {
                    id = SHATTERED_STONE;
                    // Clefts that split the rock from the top down.
                    if (y > h - 9 && y <= h && std::fabs(caves_.perlin2(wx / 9.0f + 300.0f, wz / 9.0f)) < 0.07f) id = AIR;
                    else if (y <= h && r % 97 == 0) id = VOID_SHARD_BLOCK;
                } else if (b == VoidBiome::ShardDesert && y <= h && y > h - 6) {
                    id = y > h - 4 ? (ItemId)DARK_SAND : (ItemId)DARK_SANDSTONE;
                }
                if (id == VOID_STONE) {
                    // Caves, and the richest ores of the game deep down.
                    if (y > 4 && y < h - 5 && std::fabs(caves_.perlin3(wx / 26.0f, y / 14.0f, wz / 26.0f)) < 0.05f) id = AIR;
                    else if (y < 48 && r % 280 == 0) id = RUBY_ORE;
                    else if (y < 56 && r % 230 == 7) id = STEEL_ORE;
                }
                c.set(x, y, z, id);
            }
        }
    // Trees, roots and obsidian spikes (inside the chunk, so nothing gets cut in half).
    for (int x = 2; x <= 13; ++x)
        for (int z = 2; z <= 13; ++z) {
            const int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z, h = heights[x][z];
            if (h + 12 >= kGenHeight) continue;
            uint32_t r = hash3(wx, 0, wz, seed_ ^ 0xE7E5u);
            if (biomes[x][z] == VoidBiome::Forgotten) {
                // Oaks and grass of the lost world on its floating pieces.
                const int t = forgottenTop(wx, wz, h);
                if (t + 12 < kGenHeight && c.get(x, t, z) == SOULED_GRASS && c.get(x, t + 1, z) == AIR) {
                    if (r % 1000 < 16) placeTree(c, x, t + 1, z, r, LOG, LEAVES);
                    else if (r % 1000 < 90) c.set(x, t + 1, z, TALL_GRASS);
                }
                continue;
            }
            if (c.get(x, h + 1, z) != AIR) continue;
            if (biomes[x][z] == VoidBiome::CrimsoniumForest && c.get(x, h, z) == VOID_GRASS) {
                // Dense red trees of crimsonium wood; crimson roots between them.
                if (r % 1000 < 45) placeTree(c, x, h + 1, z, r, CRIMSONIUM_LOG, CRIMSONIUM_LEAVES);
                else if (r % 1000 < 110) c.set(x, h + 1, z, CRIMSON_ROOTS);
                continue;
            }
            if (biomes[x][z] == VoidBiome::ShardDesert && c.get(x, h, z) == DARK_SAND) {
                if (r % 1000 < 18) c.set(x, h + 1, z, SHARD_CLUSTER);
                continue;
            }
            if (biomes[x][z] == VoidBiome::Shattered && c.get(x, h, z) == SHATTERED_STONE) {
                // Spikes of broken rock, some tipped with crystal.
                if (r % 1000 < 12) {
                    const int tall = 3 + (int)((r >> 12) % 6);
                    for (int k = 1; k <= tall && h + k < kGenHeight - 2; ++k) c.set(x, h + k, z, k == tall && (r >> 9) % 3 == 0 ? (ItemId)VOID_SHARD_BLOCK : (ItemId)SHATTERED_STONE);
                }
                continue;
            }
            if (biomes[x][z] == VoidBiome::Purple && c.get(x, h, z) == VOID_GRASS) {
                if (r % 1000 < 22) {
                    // Like an oak, but the trunk has an eye in the middle.
                    placeTree(c, x, h + 1, z, r, VOID_LOG, VOID_LEAVES);
                    c.set(x, h + 2, z, EYE_LOG);
                } else if (r % 1000 < 70) {
                    // 1.5.9 (generation 3): the Cornerstone Dimension's sprouts do not belong on void
                    // grass; new worlds grow the void's own crimson roots there, and fewer of them.
                    if (config_.generation < 3) c.set(x, h + 1, z, NETHER_SPROUTS);
                    else if (r % 1000 < 40) c.set(x, h + 1, z, CRIMSON_ROOTS);
                }
            } else if (biomes[x][z] == VoidBiome::Crimson && c.get(x, h, z) == RED_SAND && r % 1000 < 25) {
                c.set(x, h + 1, z, CRIMSON_ROOTS);
            } else if (biomes[x][z] == VoidBiome::Crimson && config_.crimsonDesert >= 2 && c.get(x, h, z) == RED_SAND && r % 1000 < 40) {
                // 1.5.1: the desert's landmarks, few and far between.
                const uint32_t kind = r % 1000;
                auto putIf = [&](int lx, int ly, int lz, ItemId id) {
                    if (lx >= 0 && lx < kChunkSize && lz >= 0 && lz < kChunkSize && ly > 0 && ly < kGenHeight) c.set(lx, ly, lz, id);
                };
                if (kind < 31) {
                    // A boulder of crimson stone, sunk a little into the sand.
                    for (int dx = 0; dx <= 1; ++dx)
                        for (int dz = 0; dz <= 1; ++dz)
                            for (int dy = 0; dy <= 1 + (int)((r >> (dx + dz * 2 + 8)) & 1); ++dy) putIf(x + dx, h + dy, z + dz, CRIMSON_STONE);
                } else if (kind < 34) {
                    // A spire of crimson stone, thick at its foot.
                    const int tall = 4 + (int)((r >> 12) % 5);
                    for (int k = 0; k <= tall; ++k) putIf(x, h + k, z, CRIMSON_STONE);
                    for (int k = 0; k <= tall / 2; ++k) {
                        putIf(x + 1, h + k, z, CRIMSON_STONE);
                        putIf(x, h + k, z + 1, CRIMSON_STONE);
                    }
                    putIf(x - 1, h, z, CRIMSON_STONE);
                } else if (kind < 37) {
                    // A crystal formation: rubies and void stone breaking out of crimson rock.
                    for (int dx = -1; dx <= 1; ++dx)
                        for (int dz = -1; dz <= 1; ++dz) putIf(x + dx, h, z + dz, (dx + dz) % 2 ? (ItemId)CRIMSON_STONE : (ItemId)VOID_STONE);
                    putIf(x, h + 1, z, RUBY_ORE);
                    putIf(x + 1, h + 1, z, CRIMSON_STONE);
                    putIf(x, h + 2, z, VOID_STONE);
                    if ((r >> 14) & 1) putIf(x - 1, h + 1, z + 1, RUBY_ORE);
                } else {
                    // A half-buried ruin: a broken pillar and a stretch of wall of void bricks.
                    const int tall = 2 + (int)((r >> 12) % 3);
                    for (int k = -2; k <= tall; ++k) putIf(x, h + k, z, k == tall ? (ItemId)VOID_STONE : (ItemId)VOID_STONE_BRICKS);
                    const int len = std::min(4, kChunkSize - 1 - x);
                    for (int k = 1; k <= len; ++k)
                        for (int dy = -1; dy <= (int)((r >> (k + 16)) & 1); ++dy) putIf(x + k, h + dy, z + 1, VOID_STONE_BRICKS);
                }
            } else if (biomes[x][z] == VoidBiome::Lava && h > kVoidLavaLevel && r % 1000 < 4 && arenaInfluence(wx, wz) < 0.6f) {
                int tall = 3 + (int)((r >> 12) % 6);
                for (int k = 1; k <= tall; ++k) c.set(x, h + k, z, OBSIDIAN);
                c.set(x, h + tall + 1, z, EYE_LOG);
            }
        }
    // 1.7.2 build 3: the expansion's own structures. Each fits inside one chunk (so it is never cut
    // at a border) and is chosen from the chunk's position and the seed alone.
    if (config_.generation >= 5) {
        const uint32_t r = hash3(cx, 91, cz, seed_ ^ 0x5E7Au);
        const VoidBiome b = biomes[8][8];
        const int h0 = heights[8][8];
        auto put = [&](int x, int y, int z, ItemId id) {
            if (x >= 0 && x < kChunkSize && z >= 0 && z < kChunkSize && y > 2 && y < kGenHeight - 1) c.set(x, y, z, id);
        };
        const int roll = (int)(r % 100);
        if (b == VoidBiome::CrimsoniumForest && roll < 9 && h0 + 10 < kGenHeight) {
            // A forest shrine: a floor of shattered bricks, four crimsonium pillars, a slab roof
            // with a crystal on top, the shrine's chest in the middle.
            for (int x = 5; x <= 11; ++x)
                for (int z = 5; z <= 11; ++z) {
                    for (int y = h0 + 1; y <= h0 + 6; ++y) put(x, y, z, AIR);
                    put(x, h0, z, SHATTERED_BRICKS);
                    put(x, h0 + 5, z, slabId(37, 0));
                }
            for (int x : {5, 11})
                for (int z : {5, 11})
                    for (int y = h0 + 1; y <= h0 + 4; ++y) put(x, y, z, CRIMSONIUM_LOG);
            put(8, h0 + 6, 8, VOID_SHARD_BLOCK);
            put(8, h0 + 1, 8, LOOT_CHEST_SHRINE);
            put(7, h0 + 1, 8, VOID_SHARD_BLOCK);
            put(9, h0 + 1, 8, VOID_SHARD_BLOCK);
        } else if (b == VoidBiome::Shattered && roll < 10) {
            // A rock arch from one side of the chunk to the other.
            for (int x = 2; x <= 13; ++x) {
                const int top = h0 + 3 + (int)(std::sin((x - 2) / 11.0f * 3.14159f) * 7);
                for (int z = 7; z <= 9; ++z) {
                    put(x, top, z, SHATTERED_STONE);
                    put(x, top - 1, z, (x * 7 + z) % 9 == 0 ? (ItemId)VOID_SHARD_BLOCK : (ItemId)SHATTERED_STONE);
                    if (x <= 3 || x >= 12)
                        for (int y = heights[x][z]; y < top; ++y) put(x, y, z, SHATTERED_STONE);  // the legs
                }
            }
        } else if (b == VoidBiome::Shattered && roll < 17 && h0 + 16 < kGenHeight) {
            // A crumbling tower of shattered bricks, open to the sky, its chest on the floor.
            const int tall = 7 + (int)((r >> 9) % 7);
            for (int x = 6; x <= 10; ++x)
                for (int z = 6; z <= 10; ++z) {
                    put(x, h0, z, SHATTERED_BRICKS);
                    const bool wall = x == 6 || x == 10 || z == 6 || z == 10;
                    for (int y = h0 + 1; y <= h0 + tall; ++y) {
                        const bool broken = y > h0 + tall - 3 && hash3(x * 7 + z, y, cz, seed_) % 3 == 0;
                        put(x, y, z, wall && !broken ? (ItemId)SHATTERED_BRICKS : (ItemId)AIR);
                    }
                }
            put(8, h0 + 1, 6, AIR);
            put(8, h0 + 2, 6, AIR);  // the doorway
            put(8, h0 + 1, 9, LOOT_CHEST_VOID);
        } else if (b == VoidBiome::ShardDesert && roll < 8) {
            // A buried ruin: the tops of walls sticking out of the dunes.
            for (int x = 4; x <= 12; ++x)
                for (int z = 4; z <= 12; ++z) {
                    if (x != 4 && x != 12 && z != 4 && z != 12) continue;
                    const int up = (int)(hash3(x, z, cx * 31 + cz, seed_) % 4);
                    for (int y = h0 - 3; y <= h0 - 1 + up; ++y) put(x, y, z, (x + z) % 3 ? (ItemId)DARK_SANDSTONE : (ItemId)VOID_STONE_BRICKS);
                }
            put(8, h0 - 2, 8, LOOT_CHEST_CHAMBER);
        } else if (b == VoidBiome::ShardDesert && roll < 13 && h0 > 24) {
            // A hidden chamber under the sand; only a crystal on a small pillar gives it away.
            const int floor = h0 - 13;
            for (int x = 5; x <= 11; ++x)
                for (int z = 5; z <= 11; ++z)
                    for (int y = floor; y <= floor + 5; ++y) {
                        const bool shell = x == 5 || x == 11 || z == 5 || z == 11 || y == floor || y == floor + 5;
                        put(x, y, z, shell ? (ItemId)DARK_SANDSTONE : (ItemId)AIR);
                    }
            put(8, floor + 1, 8, LOOT_CHEST_CHAMBER);
            put(6, floor + 1, 6, VOID_SHARD_BLOCK);
            put(10, floor + 1, 10, VOID_SHARD_BLOCK);
            put(8, h0 + 1, 8, SHATTERED_BRICKS);
            put(8, h0 + 2, 8, VOID_SHARD_BLOCK);
        }
    }
    // 1.6.0: the frames of the portal to the Celestial Reach lie in the Forgotten Overworld, flat
    // in the ground like the End's: twelve frame blocks round a dark hole (opens in 1.7.0).
    if (config_.generation >= 4) {
        const int region = kCelestialRegion;
        for (int rx = floorDiv(cx * kChunkSize, region); rx <= floorDiv(cx * kChunkSize + 15, region); ++rx)
            for (int rz = floorDiv(cz * kChunkSize, region); rz <= floorDiv(cz * kChunkSize + 15, region); ++rz) {
                int px, pz;
                celestialSpot(rx, rz, px, pz);
                const int lx = px - cx * kChunkSize, lz = pz - cz * kChunkSize;
                if (lx < 3 || lx > 12 || lz < 3 || lz > 12 || voidBiomeAt(px, pz) != VoidBiome::Forgotten) continue;
                int top = forgottenTop(px, pz, heights[lx][lz]);
                if (config_.generation >= 6) {
                    // 1.9.0: on a raised platform over the highest ground round it, never buried
                    // by a higher piece of land and never missing over a hole.
                    for (int dx = -3; dx <= 3; ++dx)
                        for (int dz = -3; dz <= 3; ++dz) top = std::max(top, forgottenTop(px + dx, pz + dz, heights[lx + dx][lz + dz]) + 1);
                    top = std::min(top, kGenHeight - 10);
                    for (int dx = -3; dx <= 3; ++dx)
                        for (int dz = -3; dz <= 3; ++dz) {
                            for (int y = top + 1; y < top + 8; ++y) c.set(lx + dx, y, lz + dz, AIR);
                            if (std::abs(dx) == 3 || std::abs(dz) == 3) c.set(lx + dx, top, lz + dz, SOULED_STONE);  // the rim
                            for (int y = top - 1; y > 1 && y > top - 12 && c.get(lx + dx, y, lz + dz) == AIR; --y)
                                c.set(lx + dx, y, lz + dz, SOULED_STONE);  // the pillar down to the ground
                        }
                } else if (c.get(lx, top, lz) == AIR) {
                    continue;  // on a missing piece: none here
                }
                for (int dx = -2; dx <= 2; ++dx)
                    for (int dz = -2; dz <= 2; ++dz) {
                        const bool corner = std::abs(dx) == 2 && std::abs(dz) == 2, inside = std::abs(dx) <= 1 && std::abs(dz) <= 1;
                        for (int y = top + 1; y < top + 6; ++y) c.set(lx + dx, y, lz + dz, AIR);
                        if (corner) c.set(lx + dx, top, lz + dz, SOULED_STONE);
                        else if (inside) c.set(lx + dx, top, lz + dz, AIR), c.set(lx + dx, top - 1, lz + dz, VOID_STONE);
                        else c.set(lx + dx, top, lz + dz, CELESTIAL_FRAME);
                        c.set(lx + dx, top - 2, lz + dz, SOULED_STONE);
                    }
            }
    }
    // Dungeons underground and towers on the surface (away from the arena).
    {
        const int bx0 = cx * kChunkSize, bz0 = cz * kChunkSize;
        struct VoidKind {
            WorldStructure type;
            int region, percent, size;
        };
        static const VoidKind kinds[] = {{WorldStructure::VoidDungeon, 112, 60, 11}, {WorldStructure::VoidTower, 224, 70, 11},
                                         {WorldStructure::VoidMineshaft, 320, 55, 97}, {WorldStructure::VoidWatchtower, 192, 60, 7}};
        for (const VoidKind& k : kinds) {
            // 1.5.9 kinds only in worlds of generation 3.
            if ((k.type == WorldStructure::VoidMineshaft || k.type == WorldStructure::VoidWatchtower) && config_.generation < 3) continue;
            for (int rx = floorDiv(bx0 - k.size, k.region); rx <= floorDiv(bx0 + 15, k.region); ++rx)
                for (int rz = floorDiv(bz0 - k.size, k.region); rz <= floorDiv(bz0 + 15, k.region); ++rz) {
                    uint32_t h = hash3(rx, (int)k.type * 31 + 7, rz, seed_ ^ 0x701Fu);
                    if ((int)(h % 100) >= k.percent) continue;
                    int ox = rx * k.region + 4 + (int)((h >> 8) % (uint32_t)(k.region - k.size - 8));
                    int oz = rz * k.region + 4 + (int)((h >> 16) % (uint32_t)(k.region - k.size - 8));
                    int mx = ox + k.size / 2, mz = oz + k.size / 2;
                    if (arenaInfluence(mx, mz) > 0.2f) continue;
                    int oy;
                    if (k.type == WorldStructure::VoidDungeon) {
                        oy = 18 + (int)((h >> 4) % 20);
                    } else if (k.type == WorldStructure::VoidMineshaft) {
                        // Under solid void land only (not the lava lands), well below the surface.
                        if (voidBiomeAt(mx, mz) == VoidBiome::Lava) continue;
                        oy = 16 + (int)((h >> 4) % 10);
                        if (voidHeight(mx, mz) < oy + 16) continue;
                    } else {
                        if (voidBiomeAt(mx, mz) == VoidBiome::Lava || voidBiomeAt(mx, mz) == VoidBiome::Forgotten) continue;
                        // Watchtowers keep to the crimson lands; the old towers stand anywhere.
                        if (k.type == WorldStructure::VoidWatchtower && voidBiomeAt(mx, mz) != VoidBiome::Crimson) continue;
                        oy = voidHeight(mx, mz);
                    }
                    stampBlueprint(c, blueprint((int)k.type, rx, rz, h), ox, oy, oz, false);
                }
        }
    }
    // The boss arena.
    const int x0 = cx * kChunkSize, z0 = cz * kChunkSize;
    for (int rx = floorDiv(x0 - 60, kArenaRegion); rx <= floorDiv(x0 + 60, kArenaRegion); ++rx)
        for (int rz = floorDiv(z0 - 60, kArenaRegion); rz <= floorDiv(z0 + 60, kArenaRegion); ++rz) {
            int ax, az;
            arenaCentre(rx, rz, ax, az);
            const int half = kArenaRadius + 2;
            stampBlueprint(c, blueprint((int)WorldStructure::BossArena, rx, rz, hash3(rx, 1, rz, seed_)), ax - half,
                           kVoidLavaLevel - 2, az - half, false);
        }
}

void World::placePine(Chunk& c, int x, int y, int z, uint32_t h, ItemId trunkBlock, ItemId leafBlock) {
    // A spruce: a tall trunk and a cone of needle rings that alternate wide and narrow.
    int trunk = 6 + (int)((h >> 8) % 4);
    if (y + trunk + 2 >= kGenHeight) return;
    if (c.get(x, y - 1, z) == GRASS || c.get(x, y - 1, z) == SNOW_BLOCK) c.set(x, y - 1, z, DIRT);
    const int bottom = 2 + (int)((h >> 12) % 2);
    for (int dy = bottom; dy <= trunk + 1; ++dy) {
        int fromTop = trunk + 1 - dy;
        int r = fromTop == 0 ? 0 : fromTop % 2 == 1 ? 1 : std::min(2, 1 + fromTop / 3);
        for (int dx = -r; dx <= r; ++dx)
            for (int dz = -r; dz <= r; ++dz) {
                if (std::abs(dx) == r && std::abs(dz) == r && r > 0) continue;  // round the rings
                if (c.get(x + dx, y + dy, z + dz) == AIR) c.set(x + dx, y + dy, z + dz, leafBlock);
            }
    }
    for (int dy = 0; dy < trunk; ++dy) c.set(x, y + dy, z, trunkBlock);
}

void World::placeTree(Chunk& c, int x, int y, int z, uint32_t h, ItemId trunkBlock, ItemId leafBlock) {
    int trunk = 4 + (int)((h >> 8) % 3);
    if (y + trunk + 2 >= kGenHeight) return;
    if (c.get(x, y - 1, z) == GRASS) c.set(x, y - 1, z, DIRT);
    // Crown in four layers like the classic oak: two wide (5x5) and two narrow (3x3).
    // Corners are cut so the crown reads as a rounded shape instead of a box: the
    // upper wide layer always loses them, the lower one and the narrow layer only
    // sometimes, and the top is a plus.
    for (int dy = trunk - 2; dy <= trunk + 1; ++dy) {
        int r = dy >= trunk ? 1 : 2;
        for (int dx = -r; dx <= r; ++dx)
            for (int dz = -r; dz <= r; ++dz) {
                bool corner = std::abs(dx) == r && std::abs(dz) == r;
                uint32_t m = h ^ (uint32_t)(((dy + 8) * 5 + dx + 2) * 5 + dz + 2) * 0x9E3779B9u;
                m ^= m >> 15;
                m *= 0x2C1B3C6Du;
                bool bit = (m >> 16) & 1;
                if (corner && (dy == trunk + 1 || dy == trunk - 1 || bit)) continue;
                if (c.get(x + dx, y + dy, z + dz) == AIR) c.set(x + dx, y + dy, z + dz, leafBlock);
            }
    }
    for (int dy = 0; dy < trunk; ++dy) c.set(x, y + dy, z, trunkBlock);
}

int World::ensureChunksAround(int cx, int cz, int radius, int budget) {
    std::vector<std::pair<int, int>> missing;
    for (int dx = -radius; dx <= radius; ++dx)
        for (int dz = -radius; dz <= radius; ++dz)
            if (dx * dx + dz * dz <= radius * radius && !getChunk(cx + dx, cz + dz))
                missing.emplace_back(dx, dz);
    std::sort(missing.begin(), missing.end(), [](auto& a, auto& b) {
        return a.first * a.first + a.second * a.second < b.first * b.first + b.second * b.second;
    });
    int made = 0;
    for (auto& [dx, dz] : missing) {
        if (made >= budget) break;
        generateChunk(cx + dx, cz + dz);
        ++made;
    }
    return made;
}

void World::unloadFarChunks(int cx, int cz, int radius) {
    bool regionsTouched = false;
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        int dx = it->second->cx - cx, dz = it->second->cz - cz;
        if (dx * dx + dz * dz > radius * radius) {
            if (it->second->modified) saveChunk(*it->second);
            it = chunks_.erase(it);
            regionsTouched = true;
        } else {
            ++it;
        }
    }
    if (regionsTouched) flushRegions();  // what leaves memory is on disk right away
}

std::string World::chunkPath(int cx, int cz) const {
    return storageDir_ + "/c." + std::to_string(cx) + "." + std::to_string(cz) + ".bin";
}

// 1.5.7 chunk file: "VCC4", the uncompressed size (4 bytes, little endian), then the VCC3 runs
// below packed with zlib: worlds take a fraction of the space.
// Chunk file: "VCC3", then runs of (count - 1, id low byte, id high byte) over the 32768
// blocks in Chunk::index order. Older files: "VCC2" (1.3.0) and "VCC1" have (count - 1, id)
// byte pairs, and VCC1 still holds the Nether blocks removed in 1.2.0.
namespace {
// One chunk as bytes: "VCC4", the size of the runs, then the runs packed with zlib.
std::vector<uint8_t> encodeChunk(const Chunk& c) {
    const size_t total = sizeof c.blocks / sizeof c.blocks[0];
    std::vector<uint8_t> runs;
    for (size_t i = 0; i < total;) {
        size_t run = 1;
        while (i + run < total && run < 256 && c.blocks[i + run] == c.blocks[i]) ++run;
        runs.push_back((uint8_t)(run - 1));
        runs.push_back((uint8_t)(c.blocks[i] & 0xFF));
        runs.push_back((uint8_t)(c.blocks[i] >> 8));
        i += run;
    }
    uLongf packedSize = compressBound((uLong)runs.size());
    std::vector<uint8_t> out(8 + packedSize);
    out[0] = 'V', out[1] = 'C', out[2] = 'C', out[3] = '5';  // 2.2.0: VCC5, chunks 192 blocks high
    for (int k = 0; k < 4; ++k) out[4 + k] = (uint8_t)(runs.size() >> (8 * k));
    if (compress2(out.data() + 8, &packedSize, runs.data(), (uLong)runs.size(), 6) != Z_OK) return {};
    out.resize(8 + packedSize);
    return out;
}

// Reads any chunk format into c.blocks: VCC4, VCC3 (runs of 3 bytes), VCC2/VCC1 (2 bytes).
bool decodeChunk(const std::vector<uint8_t>& data, Chunk& c, bool* legacyHeight = nullptr) {
    if (data.size() < 4 || data[0] != 'V' || data[1] != 'C' || data[2] != 'C' || data[3] < '1' || data[3] > '5') return false;
    int version = data[3] - '0';
    const bool tall = version == 5;  // VCC5: 192 blocks high; older chunks were 128 high, x << 11 | z << 7 | y
    if (legacyHeight) *legacyHeight = !tall;
    std::vector<uint8_t> unpacked;
    const uint8_t* p = data.data() + 4;
    size_t n = data.size() - 4;
    if (version >= 4) {
        if (data.size() < 8) return false;
        const uLongf want = (uLongf)(data[4] | data[5] << 8 | data[6] << 16 | (uint32_t)data[7] << 24);
        if (want >= (1u << 20)) return false;
        unpacked.resize(want);
        uLongf got = want;
        if (uncompress(unpacked.data(), &got, data.data() + 8, (uLong)(data.size() - 8)) != Z_OK || got != want) return false;
        p = unpacked.data(), n = unpacked.size(), version = 3;
    }
    const size_t total = tall ? sizeof c.blocks / sizeof c.blocks[0] : (size_t)kChunkSize * kChunkSize * kGenHeight;
    const size_t recSize = version >= 3 ? 3 : 2;
    std::vector<ItemId> flat(tall ? 0 : total);
    ItemId* dst = tall ? c.blocks : flat.data();
    size_t pos = 0;
    for (size_t at = 0; pos < total && at + recSize <= n; at += recSize) {
        const size_t count = std::min<size_t>(p[at] + 1, total - pos);
        ItemId id = version >= 3 ? (ItemId)(p[at + 1] | p[at + 2] << 8) : p[at + 1];
        if (version == 1) id = upgradeLegacyBlock(id);  // 1.3.0 reused the ids of removed Nether blocks
        std::fill(dst + pos, dst + pos + count, id);
        pos += count;
    }
    if (pos != total) return false;
    if (!tall) {
        std::fill(c.blocks, c.blocks + sizeof c.blocks / sizeof c.blocks[0], (ItemId)AIR);
        for (int x = 0; x < kChunkSize; ++x)
            for (int z = 0; z < kChunkSize; ++z)
                for (int y = 0; y < kGenHeight; ++y) c.blocks[Chunk::index(x, y, z)] = flat[(x << 11) | (z << 7) | y];
    }
    return true;
}

std::vector<uint8_t> readAll(const std::string& path) {
    std::vector<uint8_t> out;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    uint8_t buf[8192];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return out;
}

uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
}  // namespace

std::string World::regionPath(int rx, int rz) const {
    return storageDir_ + "/r." + std::to_string(rx) + "." + std::to_string(rz) + ".vcr";
}

// Region file: "VCR1", 1024 entries of (offset, length) as little-endian uint32 (chunk
// (cx & 31) + (cz & 31) * 32), then the chunks' bytes.
World::Region& World::region(int rx, int rz) const {
    const uint64_t key = ((uint64_t)(uint32_t)rx << 32) | (uint32_t)rz;
    auto it = regions_.find(key);
    if (it != regions_.end()) return it->second;
    Region& r = regions_[key];
    const std::vector<uint8_t> file = readAll(regionPath(rx, rz));
    if (file.size() >= 4 + 1024 * 8 && file[0] == 'V' && file[1] == 'C' && file[2] == 'R' && file[3] == '1')
        for (int i = 0; i < 1024; ++i) {
            const uint32_t off = le32(&file[4 + i * 8]), len = le32(&file[8 + i * 8]);
            if (len && (size_t)off + len <= file.size()) r.blobs[i].assign(file.begin() + off, file.begin() + off + len);
        }
    return r;
}

void World::flushRegions() const {
    if (storageDir_.empty()) return;
    std::error_code ec;
    for (auto& [key, r] : regions_) {
        if (!r.dirty) continue;
        const int rx = (int32_t)(key >> 32), rz = (int32_t)(uint32_t)key;
        std::vector<uint8_t> out = {'V', 'C', 'R', '1'};
        out.resize(4 + 1024 * 8, 0);
        for (int i = 0; i < 1024; ++i) {
            if (r.blobs[i].empty()) continue;
            const uint32_t off = (uint32_t)out.size(), len = (uint32_t)r.blobs[i].size();
            for (int k = 0; k < 4; ++k) out[4 + i * 8 + k] = (uint8_t)(off >> (8 * k)), out[8 + i * 8 + k] = (uint8_t)(len >> (8 * k));
            out.insert(out.end(), r.blobs[i].begin(), r.blobs[i].end());
        }
        std::filesystem::create_directories(storageDir_, ec);
        const std::string path = regionPath(rx, rz), tmp = path + ".tmp";
        FILE* f = std::fopen(tmp.c_str(), "wb");
        if (!f) continue;
        bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
        ok = std::fclose(f) == 0 && ok;
        if (ok) {
            std::filesystem::rename(tmp, path, ec);
            r.dirty = false;
        } else {
            std::filesystem::remove(tmp, ec);
        }
    }
    // The old one-file-per-chunk copies are not needed once their region is written.
    for (const std::string& f : legacyFiles_) std::filesystem::remove(f, ec);
    legacyFiles_.clear();
    if (regions_.size() > 64)  // keep memory small: drop the regions already on disk
        for (auto it = regions_.begin(); it != regions_.end();) it = it->second.dirty ? std::next(it) : regions_.erase(it);
}

bool World::loadChunk(Chunk& c) const {
    if (storageDir_.empty()) return false;
    const std::vector<uint8_t>& blob = region(floorDiv(c.cx, 32), floorDiv(c.cz, 32)).blobs[(c.cx & 31) + (c.cz & 31) * 32];
    bool legacyHeight = false;
    bool ok = !blob.empty() && decodeChunk(blob, c, &legacyHeight);
    if (!ok) {
        // Before 1.5.7 every chunk had a file of its own.
        const std::string legacy = chunkPath(c.cx, c.cz);
        ok = decodeChunk(readAll(legacy), c, &legacyHeight);
        if (ok) {
            if (legacyHeight && deep_) const_cast<World*>(this)->deepenOverworld(c), legacyHeight = false;
            Region& r = region(floorDiv(c.cx, 32), floorDiv(c.cz, 32));
            r.blobs[(c.cx & 31) + (c.cz & 31) * 32] = encodeChunk(c);
            r.dirty = true;
            legacyFiles_.push_back(legacy);
        }
    }
    if (!ok) return false;
    // 2.2.0: a chunk saved before the deeper Overworld is lifted, with the new deep layers below it.
    if (legacyHeight && deep_) const_cast<World*>(this)->deepenOverworld(c);
    c.modified = true;  // keep saving it, it no longer matches the generator
    return true;
}

void World::saveChunk(Chunk& c) const {
    if (storageDir_.empty()) return;
    Region& r = region(floorDiv(c.cx, 32), floorDiv(c.cz, 32));
    std::vector<uint8_t> blob = encodeChunk(c);
    if (blob.empty()) return;
    r.blobs[(c.cx & 31) + (c.cz & 31) * 32] = std::move(blob);
    r.dirty = true;
}

void World::saveModifiedChunks() {
    for (auto& [k, c] : chunks_)
        if (c->modified) saveChunk(*c);
    flushRegions();
}

const char* seedPresetName(int preset) {
    static const char* kNames[kSeedPresets] = {"Žádná", "Tajgový les", "Vesnice", "Led", "Pláně", "Strakatý les", "Velký strakatý les s tajgou",
                                               "Důl poblíž"};
    return kNames[std::clamp(preset, 0, kSeedPresets - 1)];
}

bool findPresetSpot(int preset, const GenConfig& config, uint32_t seed, int& outX, int& outZ) {
    if (preset <= 0) return false;
    World w(seed, Dimension::Overworld, config);
    auto all = [&](const char* id, int x, int z, int r) {
        for (int dx = -r; dx <= r; dx += 8)
            for (int dz = -r; dz <= r; dz += 8) {
                const BiomeDef* b = w.biomeAt(x + dx, z + dz);
                if (!b || b->id != id) return false;
            }
        return true;
    };
    auto any = [&](const char* id, int x, int z, int r) {
        for (int dx = -r; dx <= r; dx += 16)
            for (int dz = -r; dz <= r; dz += 16) {
                const BiomeDef* b = w.biomeAt(x + dx, z + dz);
                if (b && b->id == id) return true;
            }
        return false;
    };
    if (preset == 2 || preset == 7) {
        // A village or a mineshaft: the nearest one, and the spawn a little way from it.
        int x, y, z;
        const bool ok = preset == 2 ? w.nearestStructure(0, 0, x, y, z) : w.nearestMineshaft(0, 0, x, y, z);
        if (!ok) return false;
        outX = x + (preset == 2 ? 14 : 6), outZ = z + 6;
        return true;
    }
    // Rings of growing radius, 32 blocks apart.
    for (int r = 0; r <= 4000; r += 32)
        for (int k = 0, steps = std::max(1, r / 8); k < steps; ++k) {
            const float a = k * 2 * 3.14159265f / steps;
            const int x = (int)(std::cos(a) * r), z = (int)(std::sin(a) * r);
            bool ok = false;
            switch (preset) {
                case 1: ok = all("snowy_taiga", x, z, 24); break;
                case 3: ok = all("ice_plains", x, z, 24); break;
                case 4: ok = all("plains", x, z, 24); break;
                case 5: ok = all("dappled_forest", x, z, 24); break;
                case 6: ok = all("dappled_forest", x, z, 32) && any("snowy_taiga", x, z, 320); break;
                default: break;
            }
            if (ok && w.surfaceHeight(x, z) > w.seaLevel()) {
                outX = x, outZ = z;
                return true;
            }
        }
    return false;
}

}  // namespace vc

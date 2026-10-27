#include "World.h"

#include <zlib.h>

#include "WorldStructures.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
      temperature_(seed * 7 + 11), humidity_(seed * 13 + 5) {}

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
    if (config_.flat || config_.biomes.empty()) {
        col.height = config_.flat ? 3 : config_.overworld.seaLevel + 2;
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
    col.height = std::clamp((int)(base + continent * 10 + hills * variation), 4, kChunkHeight - 10);
    return col;
}

int World::surfaceHeight(int x, int z) const { return column(x, z).height; }

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
        if (dim_ == Dimension::Nether) generateNether(*chunk);
        else if (dim_ == Dimension::Void) generateVoid(*chunk);
        else generateOverworld(*chunk);
    }
    chunk->recalcHeightmap();
    Chunk* raw = chunk.get();
    chunks_[key(cx, cz)] = std::move(chunk);
    markDirty(cx - 1, cz); markDirty(cx + 1, cz);
    markDirty(cx, cz - 1); markDirty(cx, cz + 1);
    return raw;
}

void World::generateOverworld(Chunk& c) {
    const int cx = c.cx, cz = c.cz;
    const int sea = config_.overworld.seaLevel;
    if (config_.biomes.empty() && !config_.flat) return;
    Column cols[kChunkSize][kChunkSize];
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            int wx = cx * kChunkSize + x, wz = cz * kChunkSize + z;
            if (config_.flat) {
                // Superflat: bedrock, two dirt layers and grass, like the classic flat world.
                c.set(x, 0, z, WORLD_END);
                c.set(x, 1, z, DIRT);
                c.set(x, 2, z, DIRT);
                c.set(x, 3, z, GRASS);
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
            for (int y = 0; y < kChunkHeight; ++y) {
                ItemId id = AIR;
                if (y == 0) id = WORLD_END;
                else if (y < 4 && (hash3(wx, y, wz, seed_) & 1)) id = BEDROCK;
                else if (y < h - b.fillerDepth) id = STONE;
                else if (y < h) id = under;
                else if (y == h) id = top;
                else if (y <= sea) id = ((b.freeze || col.temperature < 0.12f) && y == sea) ? (ItemId)ICE : (ItemId)WATER;
                // High mountains: hardened stone above the peak height, snow on the very top.
                if (b.peakBlock && y > b.peakHeight && y <= h && id != BEDROCK) id = b.peakBlock;
                if (y == h && h > b.snowHeight) id = SNOW_BLOCK;
                // 1.6.0 (generation 4): the deep layer of deepstone, with a wavy top.
                const bool gen4 = config_.generation >= 4;
                if (gen4 && id == STONE && y < 12 + (int)(detail_.perlin2(wx / 23.0f + 41.0f, wz / 23.0f) * 3))
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
                    else placeTree(c, x, h + 1, z, r, b.trunk, b.leaves);
                    tree = true;
                }
            }
            // Cacti on sand, 1 to 3 blocks tall, with air around them like in the desert of the classic game.
            if (!tree && b.cactusDensity > 0 && c.get(x, h, z) == SAND && h + 4 < kChunkHeight && x >= 1 && x <= 14 &&
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
            if (!tree && cold && h + 1 < kChunkHeight && isOpaque(c.get(x, h, z)) && c.get(x, h + 1, z) == AIR) {
                c.set(x, h + 1, z, SNOW_LAYER);
                continue;
            }
            // Tall grass and flowers grow on grass with air above (not under leaves of this column's tree).
            const ItemId ground = c.get(x, h, z);
            if (!tree && h + 1 < kChunkHeight && ground == GRASS && c.get(x, h + 1, z) == AIR) {
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
        hardium.veins = 2;
        hardium.size = 4;
        hardium.minY = 3;
        hardium.maxY = 10;
        placeOres(c, hardium, 0x4A2Du);
    }
    stampStructures(c);
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
            for (int y = 6; y < top && y < kChunkHeight - 2; ++y) {
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
    };
    const int sea = config_.overworld.seaLevel;
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize, x1 = x0 + kChunkSize - 1, z1 = z0 + kChunkSize - 1;
    for (const Kind& k : kinds) {
        if (k.type == WorldStructure::RuinedVillage && config_.generation < 3) continue;
        if (k.type == WorldStructure::CaveDungeon && config_.generation < 4) continue;
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
                    const int turns = k.type == WorldStructure::Village || k.type == WorldStructure::RuinedVillage ? 0 : (int)(h >> 28);
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
                                     k.type == WorldStructure::RuinedVillage;
                if (surface) {
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
                // Copy the part inside this chunk.
                for (int y = 0; y < s.sy; ++y) {
                    const int wy = oy + y;
                    if (wy < 1 || wy >= kChunkHeight) continue;
                    for (int z = std::max(0, z0 - oz); z < s.sz && oz + z <= z1; ++z)
                        for (int x = std::max(0, x0 - ox); x < s.sx && ox + x <= x1; ++x) {
                            ItemId id = s.at(x, y, z);
                            if (id == Structure::kVoid) continue;
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
            if (ore.block == HARDIUM_ORE ? here == DEEPSTONE : (here == STONE || here == DEEPSTONE)) c.set(x, y, z, ore.block);
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
            for (int y = 0; y < kChunkHeight; ++y) {
                ItemId id;
                uint32_t h = hash3(wx, y, wz, seed_ ^ 0xBEEFu);
                if (y == 0) {
                    id = WORLD_END;
                } else if (y < 5 && (int)(h & 3) < 4 - y) {
                    id = BEDROCK;
                } else if (y == kChunkHeight - 1 || (y > kChunkHeight - 6 && (int)(h & 3) < y - (kChunkHeight - 6))) {
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
            for (int y = nd.lavaLevel + 1; y < kChunkHeight - 2; ++y) {
                if (c.get(x, y, z) != nd.fill || c.get(x, y + 1, z) != AIR) continue;
                if (wasteland) {
                    // 1.5.1: the Powerstone Wasteland: dry grass, Grasser flowers, debris and broken pillars.
                    const uint32_t r = hash3(wx, y, wz, seed_ ^ 0x9A57u);
                    const bool room = c.get(x, y + 2, z) == AIR;
                    c.set(x, y, z, r % 120 == 0 ? (ItemId)DEBRIS_GRAVEL : (ItemId)WASTELAND_GRASS);
                    if (!room) continue;
                    if (r % 400 == 7 && y + 6 < kChunkHeight && c.get(x, y + 5, z) == AIR) {
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
        int y = kChunkHeight - 8;
        while (y > 40 && !(c.get(x, y, z) == AIR && isSolid(c.get(x, y + 1, z)))) --y;
        if (y <= 40) continue;
        for (int k = 0; k < 40; ++k) {
            uint32_t q = hash3(c.cx * 97 + i, k, c.cz, seed_);
            int dx = (int)(q % 5) - 2, dz = (int)((q >> 3) % 5) - 2, dy = -(int)((q >> 6) % 5);
            if (c.get(x + dx, y + dy, z + dz) == AIR) c.set(x + dx, y + dy, z + dz, POWERSTONE_LIGHT);
        }
    }

    // Netherperk trees in the meadows: a red stem and a glowing orange crown.
    if (nd.fill == NETHERRACK)
        for (int x = 3; x <= 12; ++x)
            for (int z = 3; z <= 12; ++z) {
                const int wx = c.cx * kChunkSize + x, wz = c.cz * kChunkSize + z;
                uint32_t r = hash3(wx, 77, wz, seed_ ^ 0x9E7Bu);
                if (r % 1000 >= 14 || !netherGrassAt(wx, wz)) continue;
                for (int y = nd.lavaLevel + 2; y < kChunkHeight - 16; ++y) {
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
            const Structure& s = blueprint((int)WorldStructure::ForgottenVillage, rx, rz, h);
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
        if (wy < 1 || wy >= kChunkHeight - 1) continue;
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
    return std::clamp((int)h, 8, kChunkHeight - 20);
}

std::string World::placeName(int x, int z) const {
    switch (dim_) {
        case Dimension::Nether:
            return wastelandAt(x, z) ? "Powerstonová pustina" : netherGrassAt(x, z) ? "Louka úhelného kamene" : "Dimenze úhelného kamene";
        case Dimension::Void:
            switch (voidBiomeAt(x, z)) {
                case VoidBiome::Purple: return "Fialový les";
                case VoidBiome::Crimson: return "Karmínová poušť";
                case VoidBiome::Forgotten: return "Zapomenutý Overworld";
                default: return "Lávová pustina";
            }
        default: {
            const BiomeDef* b = biomeAt(x, z);
            return b ? b->name : "";
        }
    }
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
            const int h = voidHeight(wx, wz);
            biomes[x][z] = b;
            heights[x][z] = h;
            for (int y = 0; y < kChunkHeight; ++y) {
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
            if (h + 12 >= kChunkHeight) continue;
            uint32_t r = hash3(wx, 0, wz, seed_ ^ 0xE7E5u);
            if (biomes[x][z] == VoidBiome::Forgotten) {
                // Oaks and grass of the lost world on its floating pieces.
                const int t = forgottenTop(wx, wz, h);
                if (t + 12 < kChunkHeight && c.get(x, t, z) == SOULED_GRASS && c.get(x, t + 1, z) == AIR) {
                    if (r % 1000 < 16) placeTree(c, x, t + 1, z, r, LOG, LEAVES);
                    else if (r % 1000 < 90) c.set(x, t + 1, z, TALL_GRASS);
                }
                continue;
            }
            if (c.get(x, h + 1, z) != AIR) continue;
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
                    if (lx >= 0 && lx < kChunkSize && lz >= 0 && lz < kChunkSize && ly > 0 && ly < kChunkHeight) c.set(lx, ly, lz, id);
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
    // 1.6.0: the frames of the portal to the Celestial Reach lie in the Forgotten Overworld, flat
    // in the ground like the End's: twelve frame blocks round a dark hole (opens in 1.7.0).
    if (config_.generation >= 4) {
        const int region = 240;
        for (int rx = floorDiv(cx * kChunkSize, region); rx <= floorDiv(cx * kChunkSize + 15, region); ++rx)
            for (int rz = floorDiv(cz * kChunkSize, region); rz <= floorDiv(cz * kChunkSize + 15, region); ++rz) {
                const uint32_t hh = hash3(rx, 1552, rz, seed_ ^ 0xCE1Eu);
                const int px = rx * region + 16 + (int)(hh % (uint32_t)(region - 32)), pz = rz * region + 16 + (int)((hh >> 12) % (uint32_t)(region - 32));
                const int lx = px - cx * kChunkSize, lz = pz - cz * kChunkSize;
                if (lx < 3 || lx > 12 || lz < 3 || lz > 12 || voidBiomeAt(px, pz) != VoidBiome::Forgotten) continue;
                const int top = forgottenTop(px, pz, heights[lx][lz]);
                if (c.get(lx, top, lz) == AIR) continue;  // on a missing piece: none here
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
    if (y + trunk + 2 >= kChunkHeight) return;
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
    if (y + trunk + 2 >= kChunkHeight) return;
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
    out[0] = 'V', out[1] = 'C', out[2] = 'C', out[3] = '4';
    for (int k = 0; k < 4; ++k) out[4 + k] = (uint8_t)(runs.size() >> (8 * k));
    if (compress2(out.data() + 8, &packedSize, runs.data(), (uLong)runs.size(), 6) != Z_OK) return {};
    out.resize(8 + packedSize);
    return out;
}

// Reads any chunk format into c.blocks: VCC4, VCC3 (runs of 3 bytes), VCC2/VCC1 (2 bytes).
bool decodeChunk(const std::vector<uint8_t>& data, Chunk& c) {
    if (data.size() < 4 || data[0] != 'V' || data[1] != 'C' || data[2] != 'C' || data[3] < '1' || data[3] > '4') return false;
    int version = data[3] - '0';
    std::vector<uint8_t> unpacked;
    const uint8_t* p = data.data() + 4;
    size_t n = data.size() - 4;
    if (version == 4) {
        if (data.size() < 8) return false;
        const uLongf want = (uLongf)(data[4] | data[5] << 8 | data[6] << 16 | (uint32_t)data[7] << 24);
        if (want >= (1u << 20)) return false;
        unpacked.resize(want);
        uLongf got = want;
        if (uncompress(unpacked.data(), &got, data.data() + 8, (uLong)(data.size() - 8)) != Z_OK || got != want) return false;
        p = unpacked.data(), n = unpacked.size(), version = 3;
    }
    const size_t total = sizeof c.blocks / sizeof c.blocks[0], recSize = version >= 3 ? 3 : 2;
    size_t pos = 0;
    for (size_t at = 0; pos < total && at + recSize <= n; at += recSize) {
        const size_t count = std::min<size_t>(p[at] + 1, total - pos);
        ItemId id = version >= 3 ? (ItemId)(p[at + 1] | p[at + 2] << 8) : p[at + 1];
        if (version == 1) id = upgradeLegacyBlock(id);  // 1.3.0 reused the ids of removed Nether blocks
        std::fill(c.blocks + pos, c.blocks + pos + count, id);
        pos += count;
    }
    return pos == total;
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
    bool ok = !blob.empty() && decodeChunk(blob, c);
    if (!ok) {
        // Before 1.5.7 every chunk had a file of its own.
        const std::string legacy = chunkPath(c.cx, c.cz);
        ok = decodeChunk(readAll(legacy), c);
        if (ok) {
            Region& r = region(floorDiv(c.cx, 32), floorDiv(c.cz, 32));
            r.blobs[(c.cx & 31) + (c.cz & 31) * 32] = encodeChunk(c);
            r.dirty = true;
            legacyFiles_.push_back(legacy);
        }
    }
    if (!ok) return false;
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

}  // namespace vc

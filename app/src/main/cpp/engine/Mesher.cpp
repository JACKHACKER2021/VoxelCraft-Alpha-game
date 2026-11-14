#include "Mesher.h"

#include <algorithm>
#include <vector>

#include "BlockShapes.h"
#include "Blocks.h"
#include "Trains.h"
#include <cmath>

namespace vc {

namespace {

constexpr float kTile = kTileUv;

// 2.1.0 preview: rails as real 3D track: wooden sleepers across the track and two steel rails on top,
// following the track's centre line (straight, a smooth quarter circle round a curve, or up a slope).
struct RailPath {
    int in, out, slope;  // slope: the side it rises towards, -1 flat
};
Vec3 railPoint(const RailPath& r, float t, Vec3& tan) {
    RailPos p;
    p.in = r.in, p.out = r.out, p.t = t, p.slopeHigh = r.slope;
    const Vec3 v = TrainSystem::pointOn(p, &tan);  // cell (0, 0, 0); the rail head at 1/16
    tan = Vec3(tan.x, 0, tan.z);
    const float l = std::sqrt(tan.x * tan.x + tan.z * tan.z);
    if (l > 0) tan = tan * (1 / l);
    return Vec3(v.x, v.y - 1 / 16.0f, v.z);
}
void orientedBox(std::vector<Vertex>& out, Vec3 a, Vec3 b, float halfW, float y0, float y1, int tile, float light, float glow) {
    // A box from a to b (its centre line on the ground), halfW to each side, from y0 to y1 above it.
    Vec3 d = b - a;
    Vec3 side(-d.z, 0, d.x);
    const float sl = std::sqrt(side.x * side.x + side.z * side.z);
    if (sl < 1e-5f) return;
    side = side * (halfW / sl);
    const Vec3 c[8] = {a - side + Vec3(0, y0, 0), a + side + Vec3(0, y0, 0), b + side + Vec3(0, y0, 0), b - side + Vec3(0, y0, 0),
                       a - side + Vec3(0, y1, 0), a + side + Vec3(0, y1, 0), b + side + Vec3(0, y1, 0), b - side + Vec3(0, y1, 0)};
    static const int faces[6][4] = {{4, 5, 6, 7}, {3, 2, 1, 0}, {0, 1, 5, 4}, {2, 3, 7, 6}, {1, 2, 6, 5}, {3, 0, 4, 7}};
    static const float shade[6] = {1.0f, 0.5f, 0.8f, 0.8f, 0.65f, 0.65f};
    const float tu = (tile % kAtlasCols) * kTileUv, tv = (tile / kAtlasCols) * kTileUv;
    static const float uv[4][2] = {{0.2f, 0.8f}, {0.8f, 0.8f}, {0.8f, 0.2f}, {0.2f, 0.2f}};
    for (int f = 0; f < 6; ++f) {
        Vertex q[4];
        for (int k = 0; k < 4; ++k) {
            const Vec3& p = c[faces[f][k]];
            q[k] = {p.x, p.y, p.z, tu + uv[k][0] * kTileUv, tv + uv[k][1] * kTileUv, light * shade[f], glow * shade[f]};
        }
        out.insert(out.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
    }
}
float kDxF(int side) { static const float d[4] = {0, 1, 0, -1}; return d[side & 3]; }
float kDzF(int side) { static const float d[4] = {-1, 0, 1, 0}; return d[side & 3]; }
void meshRail(std::vector<Vertex>& out, const RailPath& r, int wx, int y, int wz, float light, float glow, bool dull) {
    const bool curve = ((r.in + 2) & 3) != r.out;
    const int segments = curve ? 8 : 1;
    const Vec3 base((float)wx, (float)y, (float)wz);
    const int sleeperTile = tileFor(SPRUCE_LOG, F_NORTH), railTile = tileFor(dull ? STONE : STEEL_BLOCK, F_TOP);
    // Sleepers: four per block along the track, across it.
    const int sleepers = curve ? 3 : 4;
    for (int i = 0; i < sleepers; ++i) {
        Vec3 tan;
        const float t = (i + 0.5f) / sleepers;
        const Vec3 m = railPoint(r, t, tan);
        const Vec3 half = tan * 0.09f;
        orientedBox(out, base + m - half, base + m + half, 0.56f, 0.0f, 0.07f, sleeperTile, light * 0.9f, glow);
    }
    // Two rails, 0.44 to each side of the centre line, in short straight pieces round a curve.
    for (float sideOff : {-0.44f, 0.44f}) {
        Vec3 tan0;
        Vec3 p0 = railPoint(r, 0, tan0);
        p0 = p0 + Vec3(-tan0.z, 0, tan0.x) * sideOff;
        for (int sgm = 1; sgm <= segments; ++sgm) {
            Vec3 tan1;
            Vec3 p1 = railPoint(r, (float)sgm / segments, tan1);
            p1 = p1 + Vec3(-tan1.z, 0, tan1.x) * sideOff;
            orientedBox(out, base + p0, base + p1, 0.045f, 0.07f, 0.17f, railTile, light, glow);
            p0 = p1;
        }
    }
}

struct FaceDef {
    int dx, dy, dz;
    float shade;          // classic per-direction shading
    float corners[4][3];  // counter-clockwise when seen from outside
};

const FaceDef kFaces[6] = {
    /* top    */ {0, 1, 0, 1.0f, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}},
    /* bottom */ {0, -1, 0, 0.5f, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}},
    /* north  */ {0, 0, -1, 0.8f, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}},
    /* south  */ {0, 0, 1, 0.8f, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}},
    /* east   */ {1, 0, 0, 0.6f, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}},
    /* west   */ {-1, 0, 0, 0.6f, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}},
};

bool faceVisible(ItemId self, ItemId neighbour) {
    if (neighbour == AIR) return true;
    if (isLiquid(self)) return false;  // liquids only show against air
    if (isOpaque(neighbour)) return false;
    // Leaves have see-through pixels, so the faces between two leaf blocks must be drawn
    // ("fancy leaves"); hiding them left tree crowns as hollow shells with holes.
    if (isLeaves(self)) return true;
    return neighbour != self;          // glass next to glass: hidden
}

// The chunk and its eight neighbours, for lookups one block past the edges.
class Neighbourhood {
public:
    Neighbourhood(const World& world, const Chunk& chunk) {
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz) chunks_[dx + 1][dz + 1] = world.getChunk(chunk.cx + dx, chunk.cz + dz);
    }
    // x and z relative to the middle chunk, -16..31.
    ItemId get(int x, int y, int z) const {
        if (y < 0 || y >= kChunkHeight) return AIR;
        int ix = x < 0 ? 0 : x >= kChunkSize ? 2 : 1, iz = z < 0 ? 0 : z >= kChunkSize ? 2 : 1;
        const Chunk* c = chunks_[ix][iz];
        return c ? c->get(x - (ix - 1) * kChunkSize, y, z - (iz - 1) * kChunkSize) : (ItemId)AIR;
    }
    int height(int x, int z) const {
        int ix = x < 0 ? 0 : x >= kChunkSize ? 2 : 1, iz = z < 0 ? 0 : z >= kChunkSize ? 2 : 1;
        const Chunk* c = chunks_[ix][iz];
        return c ? c->heightmap[(x - (ix - 1) * kChunkSize) * kChunkSize + (z - (iz - 1) * kChunkSize)] : 0;
    }

private:
    const Chunk* chunks_[3][3];
};

// Blocks that darken the corners next to them (ambient occlusion).
inline bool occludes(ItemId id) { return isOpaque(id) || isLeaves(id); }

// Approximate sky light for every cell of the chunk plus a one block border:
// 1 in the open, less under leaves (each leaf block above takes some light,
// so tree crowns get depth and shade the ground and trunk below them) and
// dark under opaque blocks.
class SkyLight {
public:
    SkyLight(const Neighbourhood& n) {
        for (int x = -1; x <= kChunkSize; ++x)
            for (int z = -1; z <= kChunkSize; ++z) {
                float* col = column(x, z);
                const int top = n.height(x, z);
                int leaves = 0;
                for (int y = kChunkHeight - 1; y >= 0; --y) {
                    if (isLeaves(n.get(x, y, z))) ++leaves;
                    col[y] = y < top ? 0.45f : 1.0f - 0.13f * std::min(leaves, 4);
                }
            }
    }
    float at(int x, int y, int z) const { return y >= kChunkHeight ? 1.0f : y < 0 ? 0.45f : column(x, z)[y]; }

private:
    float* column(int x, int z) { return &light_[((x + 1) * (kChunkSize + 2) + (z + 1)) * kChunkHeight]; }
    const float* column(int x, int z) const { return &light_[((x + 1) * (kChunkSize + 2) + (z + 1)) * kChunkHeight]; }
    std::vector<float> light_ = std::vector<float>((kChunkSize + 2) * (kChunkSize + 2) * kChunkHeight);
};

// Light from glowing blocks (lamps, lava, lit furnaces...) for the chunk and 15 blocks around it:
// each block passes on one level less, through everything that is not opaque.
class BlockLight {
public:
    static constexpr int kMargin = 15, kSide = kChunkSize + 2 * kMargin;

    explicit BlockLight(const Neighbourhood& n, const World& world, const Chunk& chunk) {
        // Most chunks have no glowing blocks at all: then there is nothing to do.
        std::vector<int> queue;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz) {
                const Chunk* c = world.getChunk(chunk.cx + dx, chunk.cz + dz);
                if (!c) continue;
                if (c->lights < 0) {
                    c->lights = 0;
                    for (ItemId b : c->blocks)
                        if (lightLevel(b) > 0) {
                            c->lights = 1;
                            break;
                        }
                }
                if (!c->lights) continue;
                for (int x = 0; x < kChunkSize; ++x)
                    for (int z = 0; z < kChunkSize; ++z)
                        for (int y = 0; y < kChunkHeight; ++y) {
                            const int level = lightLevel(c->get(x, y, z));
                            if (!level) continue;
                            const int rx = x + dx * kChunkSize + kMargin, rz = z + dz * kChunkSize + kMargin;
                            if (rx < 0 || rz < 0 || rx >= kSide || rz >= kSide) continue;
                            if (light_.empty()) light_.assign((size_t)kSide * kSide * kChunkHeight, 0);
                            uint8_t& l = light_[cell(rx, y, rz)];
                            if (level > l) {
                                l = (uint8_t)level;
                                queue.push_back(cell(rx, y, rz));
                            }
                        }
            }
        // Spread, one level less per step.
        for (size_t q = 0; q < queue.size(); ++q) {
            const int i = queue[q];
            const int level = light_[(size_t)i];
            if (level <= 1) continue;
            const int y = i % kChunkHeight, rz = (i / kChunkHeight) % kSide, rx = i / kChunkHeight / kSide;
            const int next[6][3] = {{rx + 1, y, rz}, {rx - 1, y, rz}, {rx, y + 1, rz}, {rx, y - 1, rz}, {rx, y, rz + 1}, {rx, y, rz - 1}};
            for (const auto& p : next) {
                if (p[0] < 0 || p[2] < 0 || p[0] >= kSide || p[2] >= kSide || p[1] < 0 || p[1] >= kChunkHeight) continue;
                const int j = cell(p[0], p[1], p[2]);
                if (light_[(size_t)j] >= level - 1) continue;
                const int wx = p[0] - kMargin, wz = p[2] - kMargin;
                // The neighbourhood reaches 16 blocks past the chunk; light goes no further.
                if (wx < -kChunkSize || wz < -kChunkSize || wx >= 2 * kChunkSize || wz >= 2 * kChunkSize) continue;
                if (isOpaque(n.get(wx, p[1], wz))) continue;
                light_[(size_t)j] = (uint8_t)(level - 1);
                queue.push_back(j);
            }
        }
    }
    // 0..1 at a cell relative to the chunk (-15..30).
    float at(int x, int y, int z) const {
        if (light_.empty() || y < 0 || y >= kChunkHeight) return 0;
        const int rx = x + kMargin, rz = z + kMargin;
        if (rx < 0 || rz < 0 || rx >= kSide || rz >= kSide) return 0;
        const float l = light_[cell(rx, y, rz)] / 15.0f;
        return l * l * 1.15f;  // falls off faster than linear, like a real lamp
    }

private:
    static int cell(int rx, int y, int rz) { return (rx * kSide + rz) * kChunkHeight + y; }
    std::vector<uint8_t> light_;
};

}  // namespace

bool canMesh(const World& world, int cx, int cz) {
    return world.getChunk(cx - 1, cz) && world.getChunk(cx + 1, cz) &&
           world.getChunk(cx, cz - 1) && world.getChunk(cx, cz + 1);
}

void buildChunkMesh(const World& world, const Chunk& chunk, ChunkMesh& out) {
    out.opaque.clear();
    out.translucent.clear();
    const int baseX = chunk.cx * kChunkSize, baseZ = chunk.cz * kChunkSize;
    const bool nether = world.dimension() == Dimension::Nether;
    const Neighbourhood n(world, chunk);
    const SkyLight sky(n);
    const BlockLight glow(n, world, chunk);
    // 1.7.2 build 2: grass takes the colour of its biome (0 green, 1 orange, 2 dark), worked out
    // once per column and only where there is grass.
    int8_t grassKind[kChunkSize][kChunkSize];
    std::fill(&grassKind[0][0], &grassKind[0][0] + kChunkSize * kChunkSize, (int8_t)-1);
    auto grassTile = [&](int x, int z, int tile) {
        if (tile != T_GRASS_TOP && tile != T_GRASS_SIDE) return tile;
        int8_t& k = grassKind[x][z];
        if (k < 0) {
            const BiomeDef* b = world.config().oldWorld ? nullptr : world.biomeAt(baseX + x, baseZ + z);
            k = !b ? 0 : b->id == "dappled_forest" ? 1 : (b->id == "snowy_taiga" || b->treeShape == 1) ? 2 : 0;
        }
        if (k == 1) return tile == T_GRASS_TOP ? (int)T_GRASS_TOP_ORANGE : (int)T_GRASS_SIDE_ORANGE;
        if (k == 2) return tile == T_GRASS_TOP ? (int)T_GRASS_TOP_DARK : (int)T_GRASS_SIDE_DARK;
        return tile;
    };

    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkHeight; ++y) {
                ItemId id = chunk.get(x, y, z);
                if (id == AIR || id == BARRIER) continue;  // 1.9.5: barriers are invisible
                int wx = baseX + x, wz = baseZ + z;
                auto& dst = isTranslucent(id) ? out.translucent : out.opaque;

                if (isCrossingSignal(id) || isCrossingGate(id)) {
                    // 2.1.0 preview build 3: a level-crossing signal (post, crossed boards, two lamps) or
                    // a crossing gate (post, lamp, striped arm: up when open, across the road when closed).
                    const float light = nether ? 0.75f : sky.at(x, y, z), glowHere = glow.at(x, y, z);
                    const bool signal = isCrossingSignal(id);
                    const int k = signal ? id - CROSSING_SIGNAL : id - CROSSING_GATE;
                    const int f = signal ? k / 3 : k / 2, st = signal ? k % 3 : k % 2;
                    const Vec3 mid((float)wx + 0.5f, (float)y, (float)wz + 0.5f);
                    const Vec3 fw(kDxF(f), 0, kDzF(f)), sd(kDxF(f + 1), 0, kDzF(f + 1));
                    const int postTile = tileFor(STEEL_BLOCK, F_TOP), dark = tileFor(OBSIDIAN, F_TOP);
                    orientedBox(out.opaque, mid - sd * 0.3f, mid + sd * 0.3f, 0.3f, 0, 0.08f, dark, light, glowHere);  // base plate
                    orientedBox(out.opaque, mid - sd * 0.06f, mid + sd * 0.06f, 0.06f, 0.08f, signal ? 2.45f : 1.55f, postTile, light, glowHere);
                    if (signal) {
                        // The crossed boards, built from short pieces along the two diagonals.
                        for (int arm = 0; arm < 2; ++arm)
                            for (int i = 0; i < 9; ++i) {
                                const float u = -0.48f + i * 0.12f, h = 1.75f + (arm ? 0.48f - u : u + 0.48f) * 0.7f;
                                const Vec3 c = mid + sd * u + fw * 0.08f;
                                orientedBox(out.opaque, c - sd * 0.065f, c + sd * 0.065f, 0.025f, h - 0.06f, h + 0.06f, T_SIGNAL_BOARD, light, glowHere);
                            }
                        // The lamps, one each side; lit ones glow (they take turns while a train is near).
                        for (int side = 0; side < 2; ++side) {
                            const bool lit = (st == 1 && side == 0) || (st == 2 && side == 1);
                            const Vec3 c = mid + sd * (side ? 0.3f : -0.3f) + fw * 0.1f;
                            orientedBox(out.opaque, mid + fw * 0.1f, c, 0.03f, 1.33f, 1.37f, dark, light, glowHere);  // the bracket
                            orientedBox(out.opaque, c - sd * 0.11f, c + sd * 0.11f, 0.07f, 1.22f, 1.48f, lit ? T_SIGNAL_LAMP_ON : T_SIGNAL_LAMP_OFF,
                                        lit ? 1.0f : light, lit ? 1.0f : glowHere);
                        }
                    } else {
                        const bool closed = st == 1;
                        const Vec3 lampAt = mid + fw * 0.08f;
                        orientedBox(out.opaque, lampAt - sd * 0.08f, lampAt + sd * 0.08f, 0.05f, 1.3f, 1.48f, closed ? T_SIGNAL_LAMP_ON : T_SIGNAL_LAMP_OFF,
                                    closed ? 1.0f : light, closed ? 1.0f : glowHere);
                        const int armTile = T_BUFFER_PLATE;
                        if (closed) orientedBox(out.opaque, mid + sd * 0.07f, mid + sd * 3.4f, 0.05f, 1.05f, 1.17f, armTile, light, glowHere);
                        else orientedBox(out.opaque, mid + sd * 0.07f, mid + sd * 0.19f, 0.05f, 1.05f, 4.2f, armTile, light, glowHere);
                    }
                    continue;
                }
                if (isRail(id)) {
                    // 2.1.0 preview: 3D track (see meshRail).
                    const float light = nether ? 0.75f : sky.at(x, y, z), glowHere = glow.at(x, y, z);
                    if (isTurntable(id))
                        orientedBox(out.opaque, Vec3((float)wx, (float)y, wz + 0.5f), Vec3(wx + 1.0f, (float)y, wz + 0.5f), 0.5f, 0, 0.05f, T_TURNTABLE, light, glowHere);
                    int a, b2;
                    if (TrainSystem::railSides(id, a, b2)) {
                        meshRail(out.opaque, {a, b2, isRailSlope(id) ? id - RAIL_SLOPE : -1}, wx, y, wz, light, glowHere, false);
                    } else if (isRailSwitch(id)) {
                        const int k = railSwitchIndex(id), f = k / 2, st = k % 2;
                        meshRail(out.opaque, {f, (f + 2) & 3, -1}, wx, y, wz, light, glowHere, st == 1);         // straight
                        meshRail(out.opaque, {(f + 1) & 3, (f + 2) & 3, -1}, wx, y, wz, light, glowHere, st == 0);  // the branch
                        // The lever beside the points, leaning the way the switch is set.
                        const Vec3 at((float)wx + 0.5f + kDxF(f + 3) * 0.62f, (float)y, (float)wz + 0.5f + kDzF(f + 3) * 0.62f);
                        orientedBox(out.opaque, at - Vec3(0.06f, 0, 0), at + Vec3(0.06f, 0, 0), 0.06f, 0, 0.12f, tileFor(STONE, F_TOP), light, glowHere);
                        const Vec3 tip = at + Vec3(kDxF(st ? f + 1 : f) * 0.12f, 0, kDzF(st ? f + 1 : f) * 0.12f);
                        orientedBox(out.opaque, at, tip, 0.02f, 0.12f, 0.42f, tileFor(POWERSTONE_BLOCK, F_TOP), light, glowHere);
                    }
                    continue;
                }
                if (isShaped(id)) {
                    // Doors, trapdoors, fences, chests, cactus: a few textured boxes.
                    Box boxes[10];
                    const int count = blockBoxes(world, wx, y, wz, id, false, boxes);
                    for (int b = 0; b < count; ++b) {
                        const Box& bx = boxes[b];
                        const float lo[3] = {bx.x0, bx.y0, bx.z0}, hi[3] = {bx.x1, bx.y1, bx.z1};
                        for (int f = 0; f < 6; ++f) {
                            const FaceDef& fd = kFaces[f];
                            const int axis = fd.dx ? 0 : fd.dy ? 1 : 2, dir = fd.dx + fd.dy + fd.dz;
                            const bool onEdge = dir > 0 ? hi[axis] >= 0.999f : lo[axis] <= 0.001f;
                            ItemId nb = n.get(x + fd.dx, y + fd.dy, z + fd.dz);
                            if (onEdge && isOpaque(nb)) continue;
                            float light = nether ? fd.shade * 0.7f
                                                 : fd.shade * (onEdge ? sky.at(x + fd.dx, y + fd.dy, z + fd.dz) : sky.at(x, y, z));
                            float block = isEmissive(id) ? 1.0f
                                                         : fd.shade * (onEdge ? glow.at(x + fd.dx, y + fd.dy, z + fd.dz) : glow.at(x, y, z));
                            int tile = tileFor(id, (Face)f);
                            if (isCable(id)) {
                                // The wire itself (the item icon keeps the cross).
                                tile = cableOn(id) ? T_CABLE_LINE_ON : T_CABLE_LINE;
                                if (cableOn(id)) light = block = 1;
                            }
                            float tu = (tile % kAtlasCols) * kTile, tv = (tile / kAtlasCols) * kTile;
                            Vertex q[4];
                            for (int i = 0; i < 4; ++i) {
                                const float* c = fd.corners[i];
                                float p[3];
                                for (int k = 0; k < 3; ++k) p[k] = c[k] > 0.5f ? hi[k] : lo[k];
                                // Texture coordinates from the position on the face, so a small box shows
                                // the matching part of the tile.
                                // 1.7.2 build 2: a wall torch's stick sits at the wall, but its texture is the
                                // standing torch's: map the box back to the middle of the tile (it showed the
                                // empty edge of the tile, so the torch looked like a flat plate).
                                float tp[3] = {p[0], p[1], p[2]};
                                if (isTorch(id) && id != TORCH) tp[0] += 7 / 16.0f - bx.x0, tp[2] += 7 / 16.0f - bx.z0, tp[1] -= 3 / 16.0f;
                                float u = axis == 0 ? tp[2] : tp[0], v = axis == 1 ? tp[2] : 1 - std::min(tp[1], 1.0f);
                                if (f == 2 || f == 5) u = 1 - u;  // north and west faces are seen mirrored
                                if (f == F_TOP && isBed(id)) {
                                    // The bed's top turns with it: the pillow lies across the head end.
                                    const int facing = (id - BED) / 2;
                                    const float x = p[0], z = p[2];
                                    if (facing == 1) u = z, v = 1 - x;
                                    else if (facing == 2) u = 1 - x, v = 1 - z;
                                    else if (facing == 3) u = 1 - z, v = x;
                                }
                                if (isTorch(id) && id != TORCH) {
                                    // 1.5.9: a wall torch leans out from its wall, its foot against it.
                                    static const float away[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};
                                    const float k = (p[1] - 3 / 16.0f) * 0.4f;
                                    p[0] += away[id - TORCH_WALL][0] * k;
                                    p[2] += away[id - TORCH_WALL][1] * k;
                                }
                                q[i] = {wx + p[0], y + p[1], wz + p[2], tu + u * kTile, tv + v * kTile, light, block};
                            }
                            out.opaque.insert(out.opaque.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
                        }
                    }
                    if (id == CAMPFIRE) {
                        // 1.9.0: the flames between the logs, two crossed glowing planes.
                        const int tile = T_FIRE;
                        const float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile, u1 = u0 + kTile, v1 = v0 + kTile;
                        const float e = 0.2f, y0 = y + 0.1f, y1 = y + 0.95f;
                        const float planes[2][4] = {{e, e, 1 - e, 1 - e}, {1 - e, e, e, 1 - e}};
                        for (const auto& p : planes) {
                            Vertex a{wx + p[0], y0, wz + p[1], u0, v1, 1, 1}, b{wx + p[2], y0, wz + p[3], u1, v1, 1, 1};
                            Vertex c{wx + p[2], y1, wz + p[3], u1, v0, 1, 1}, d{wx + p[0], y1, wz + p[1], u0, v0, 1, 1};
                            out.opaque.insert(out.opaque.end(), {a, b, c, a, c, d, a, c, b, a, d, c});
                        }
                    }
                    continue;
                }

                if (isPlant(id)) {
                    // Two crossed planes, each with both sides so they show from every angle.
                    float light = nether ? 0.75f : 0.95f * sky.at(x, y, z);
                    float block = glow.at(x, y, z) * 0.95f;
                    if (id == FIRE) light = block = 1;  // 1.9.0: fire glows
                    int tile = tileFor(id, F_SOUTH);
                    float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile, u1 = u0 + kTile, v1 = v0 + kTile;
                    const float e = 0.15f;  // pulled in a little from the block corners
                    const float planes[2][4] = {{e, e, 1 - e, 1 - e}, {1 - e, e, e, 1 - e}};
                    for (const auto& p : planes) {
                        Vertex a{wx + p[0], (float)y, wz + p[1], u0, v1, light, block}, b{wx + p[2], (float)y, wz + p[3], u1, v1, light, block};
                        Vertex c{wx + p[2], y + 1.0f, wz + p[3], u1, v0, light, block + kWaveSway},
                            d{wx + p[0], y + 1.0f, wz + p[1], u0, v0, light, block + kWaveSway};  // the tops sway
                        dst.insert(dst.end(), {a, b, c, a, c, d});  // one side
                        dst.insert(dst.end(), {a, c, b, a, d, c});  // the other side
                    }
                    continue;
                }

                for (int f = 0; f < 6; ++f) {
                    const FaceDef& fd = kFaces[f];
                    const int lx = x + fd.dx, ly = y + fd.dy, lz = z + fd.dz;
                    if (ly < 0) continue;
                    ItemId nb = n.get(lx, ly, lz);
                    if (!faceVisible(id, nb)) continue;
                    // Liquid surfaces sit slightly lower, like in the original.
                    float topCut = (isLiquid(id) && f != 1 && n.get(x, y + 1, z) != id) ? 0.1f : 0.0f;
                    // 1.9.0 build 3: flowing water and lava get lower with every block away from the
                    // source, and the surface slopes: each corner is the average of the liquid cells
                    // that share it (full where liquid lies on top).
                    auto cornerCut = [&](int ox, int oz) {
                        float sum = 0;
                        int cells = 0;
                        for (int i = ox - 1; i <= ox; ++i)
                            for (int j = oz - 1; j <= oz; ++j) {
                                if (n.get(x + i, y, z + j) != id) continue;
                                if (n.get(x + i, y + 1, z + j) == id) return 0.0f;
                                // Water drops by a fifth of a block per step; lava over its longer reach.
                                const float step = id == WATER ? 0.16f : 0.13f;
                                sum += std::min(0.9f, 0.1f + world.liquidLevel(wx + i, y, wz + j) * step);
                                ++cells;
                            }
                        return cells ? sum / cells : topCut;
                    };

                    float light;
                    // Glowing blocks shine in the dark; the light of lamps falls on the faces around them.
                    const float block = isEmissive(id) ? 1.0f : glow.at(lx, ly, lz) * fd.shade;
                    if (isEmissive(id)) light = 1.0f;
                    else if (nether) light = fd.shade * 0.7f;  // no sky in the Nether: a dim, warm ambient
                    else light = fd.shade * sky.at(lx, ly, lz);

                    // 1.5.5: leaves sway; the open surface of water and lava ripples.
                    const int wave = isLeaves(id) ? 1 : isLiquid(id) && f == F_TOP && n.get(x, y + 1, z) != id ? 2 : 0;
                    int tile = id == GRASS ? grassTile(x, z, tileFor(id, (Face)f)) : tileFor(id, (Face)f);
                    float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile;
                    const float uv[4][2] = {{u0, v0 + kTile}, {u0 + kTile, v0 + kTile}, {u0 + kTile, v0}, {u0, v0}};

                    // Ambient occlusion: each corner looks at the two blocks beside it and the one
                    // diagonal to it in the layer in front of the face.
                    float ao[4] = {1, 1, 1, 1};
                    if (!isEmissive(id) && !isLiquid(id)) {
                        static const float kAo[4] = {0.5f, 0.68f, 0.84f, 1.0f};
                        for (int i = 0; i < 4; ++i) {
                            const float* c = fd.corners[i];
                            int o[3] = {0, 0, 0}, axis = 0;
                            int s1[3] = {0, 0, 0}, s2[3] = {0, 0, 0};
                            for (int k = 0; k < 3; ++k) {
                                bool normalAxis = (k == 0 && fd.dx) || (k == 1 && fd.dy) || (k == 2 && fd.dz);
                                if (normalAxis) continue;
                                o[k] = c[k] > 0.5f ? 1 : -1;
                                (axis++ == 0 ? s1 : s2)[k] = o[k];
                            }
                            bool a = occludes(n.get(lx + s1[0], ly + s1[1], lz + s1[2]));
                            bool b = occludes(n.get(lx + s2[0], ly + s2[1], lz + s2[2]));
                            bool d = occludes(n.get(lx + o[0], ly + o[1], lz + o[2]));
                            ao[i] = kAo[a && b ? 0 : 3 - (a + b + d)];
                        }
                    }

                    Vertex q[4];
                    for (int i = 0; i < 4; ++i) {
                        const float* c = fd.corners[i];
                        const float cut = c[1] > 0 && topCut > 0 ? cornerCut(c[0] > 0.5f ? 1 : 0, c[2] > 0.5f ? 1 : 0) : 0;
                        q[i] = {wx + c[0], y + c[1] - cut, wz + c[2], uv[i][0], uv[i][1], light * ao[i], block * (0.4f + 0.6f * ao[i])};
                        if (wave == 1 || (wave == 2 && c[1] > 0)) q[i].block += wave == 1 ? kWaveSway : kWaveWater;
                    }
                    // Split the quad along the diagonal that keeps the shading smooth.
                    if (ao[0] + ao[2] >= ao[1] + ao[3]) dst.insert(dst.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
                    else dst.insert(dst.end(), {q[1], q[2], q[3], q[1], q[3], q[0]});
                }
            }
}

}  // namespace vc

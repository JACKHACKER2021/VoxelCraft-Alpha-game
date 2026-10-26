#include "Drops.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include "BlockShapes.h"
#include "Blocks.h"
#include "ItemModel.h"

namespace vc {

namespace {

constexpr float kTile = kTileUv;
constexpr float kHalf = 0.125f;  // a dropped item is a quarter of a block

bool solidAt(const World& w, const Vec3& p) {
    return isSolid(w.getBlock((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z)));
}

}  // namespace

void DropSystem::spawn(int dim, const Vec3& pos, const Vec3& vel, const ItemStack& stack) {
    if (stack.empty()) return;
    // Merge into a resting drop of the same item nearby first.
    for (DroppedItem& d : drops_)
        if (d.dim == dim && d.stack.id == stack.id && d.stack.damage == stack.damage &&
            d.stack.count + stack.count <= maxStack(stack.id) && (d.pos - pos).length() < 1.0f) {
            d.stack.count = (uint8_t)(d.stack.count + stack.count);
            d.age = std::min(d.age, kPickupDelay * 0.5f);
            return;
        }
    drops_.push_back({dim, pos, vel, stack, 0});
}

int DropSystem::update(const World& world, int dim, const Vec3& player, Inventory& inventory, float dt) {
    int picked = 0;
    for (size_t i = 0; i < drops_.size();) {
        DroppedItem& d = drops_[i];
        if (d.dim != dim) {
            ++i;
            continue;
        }
        d.age += dt;
        // Fall, slide a little, rest on blocks; float up out of a block it is stuck in.
        if (solidAt(world, d.pos)) {
            d.pos.y += dt * 2;
            d.vel = {0, 0, 0};
        } else {
            d.vel.y = std::max(d.vel.y - 16 * dt, -20.0f);
            Vec3 next = d.pos + d.vel * dt;
            if (solidAt(world, Vec3(next.x, d.pos.y, d.pos.z))) d.vel.x = 0, next.x = d.pos.x;
            if (solidAt(world, Vec3(d.pos.x, d.pos.y, next.z))) d.vel.z = 0, next.z = d.pos.z;
            if (solidAt(world, Vec3(next.x, next.y - kHalf, next.z))) {
                next.y = std::floor(next.y - kHalf) + 1 + kHalf;
                d.vel.y = 0;
                d.vel.x *= std::max(0.0f, 1 - dt * 10);
                d.vel.z *= std::max(0.0f, 1 - dt * 10);
            }
            d.pos = next;
        }
        // Picked up when the player is close and there is room for at least part of it.
        Vec3 diff = d.pos - (player + Vec3(0, 0.9f, 0));
        if (d.age >= kPickupDelay && std::fabs(diff.x) < 1.4f && std::fabs(diff.z) < 1.4f && std::fabs(diff.y) < 1.6f) {
            int left = inventory.add(d.stack);
            picked += d.stack.count - left;
            d.stack.count = (uint8_t)left;
        }
        if (d.stack.empty() || d.age > kLifetime || d.pos.y < -10) {
            drops_.erase(drops_.begin() + (long)i);
            continue;
        }
        ++i;
    }
    return picked;
}

int DropSystem::itemCount(int dim) const {
    int n = 0;
    for (const DroppedItem& d : drops_)
        if (d.dim == dim) n += d.stack.count;
    return n;
}

void DropSystem::buildVertices(int dim, float time, std::vector<Vertex>& out) const {
    out.clear();
    for (const DroppedItem& d : drops_) {
        if (d.dim != dim) continue;
        // Bob and spin like classic dropped items.
        const float spin = time * 1.6f + d.pos.x * 3.1f, cs = std::cos(spin), sn = std::sin(spin);
        const Vec3 c = d.pos + Vec3(0, std::sin(time * 2.5f + d.pos.z) * 0.05f + 0.05f, 0);
        auto corner = [&](float x, float y, float z) {
            return Vec3(c.x + x * cs - z * sn, c.y + y, c.z + x * sn + z * cs);
        };
        auto quad = [&](Vec3 a, Vec3 b, Vec3 cc, Vec3 dd, int tile, float light, float s0 = 0, float s1 = 1, float t0 = 0,
                        float t1 = 1) {
            const float tu = (tile % kAtlasCols) * kTile, tv = (tile / kAtlasCols) * kTile;
            const float u0 = tu + s0 * kTile, u1 = tu + s1 * kTile, v0 = tv + t0 * kTile, v1 = tv + t1 * kTile;
            Vertex va{a.x, a.y, a.z, u0, v1, light}, vb{b.x, b.y, b.z, u1, v1, light};
            Vertex vc{cc.x, cc.y, cc.z, u1, v0, light}, vd{dd.x, dd.y, dd.z, u0, v0, light};
            out.insert(out.end(), {va, vb, vc, va, vc, vd});
        };
        const float h = kHalf;
        const bool flat = isItemOnly(d.stack.id) || isPlant(d.stack.id);
        const int copies = d.stack.count > 16 ? 3 : d.stack.count > 1 ? 2 : 1;  // a pile shows more than one
        for (int k = 0; k < copies; ++k) {
            const float ox = k * 0.07f, oy = k * 0.05f, oz = -k * 0.06f;
            auto P = [&](float x, float y, float z) { return corner(x + ox, y + oy, z + oz); };
            if (flat) {
                // Items lie as small 3D models, standing up and spinning.
                appendItemModel(tileFor(d.stack.id, F_SOUTH), 0.95f, 1 / 16.0f,
                                [&](const Vec3& q) { return P(q.x * h * 3, q.y * h * 3 + h * 0.5f, q.z * h * 3); }, out);
                continue;
            }
            const ItemId id = d.stack.id;
            Box boxes[kItemBoxes];
            const int n = itemBoxes(id, boxes);
            for (int i = 0; i < n; ++i) {
                const Box& bx = boxes[i];
                const float x0 = -h + 2 * h * bx.x0, x1 = -h + 2 * h * bx.x1, y0 = -h + 2 * h * bx.y0;
                const float y1 = -h + 2 * h * bx.y1, z0 = -h + 2 * h * bx.z0, z1 = -h + 2 * h * bx.z1;
                const float ty0 = 1 - bx.y1, ty1 = 1 - bx.y0;
                quad(P(x0, y1, z1), P(x1, y1, z1), P(x1, y1, z0), P(x0, y1, z0), tileFor(id, F_TOP), 1.0f, bx.x0, bx.x1, bx.z0, bx.z1);
                quad(P(x0, y0, z0), P(x1, y0, z0), P(x1, y0, z1), P(x0, y0, z1), tileFor(id, F_BOTTOM), 0.5f, bx.x0, bx.x1, bx.z0, bx.z1);
                quad(P(x1, y0, z0), P(x0, y0, z0), P(x0, y1, z0), P(x1, y1, z0), tileFor(id, F_NORTH), 0.8f, 1 - bx.x1, 1 - bx.x0, ty0, ty1);
                quad(P(x0, y0, z1), P(x1, y0, z1), P(x1, y1, z1), P(x0, y1, z1), tileFor(id, F_SOUTH), 0.8f, bx.x0, bx.x1, ty0, ty1);
                quad(P(x1, y0, z1), P(x1, y0, z0), P(x1, y1, z0), P(x1, y1, z1), tileFor(id, F_EAST), 0.6f, 1 - bx.z1, 1 - bx.z0, ty0, ty1);
                quad(P(x0, y0, z0), P(x0, y0, z1), P(x0, y1, z1), P(x0, y1, z0), tileFor(id, F_WEST), 0.6f, bx.z0, bx.z1, ty0, ty1);
            }
        }
    }
}

bool DropSystem::save(const std::string& folder) const {
    std::ostringstream o;
    for (const DroppedItem& d : drops_)
        o << d.dim << ' ' << d.pos.x << ' ' << d.pos.y << ' ' << d.pos.z << ' ' << (int)d.stack.id << ' '
          << (int)d.stack.count << ' ' << d.stack.damage << ' ' << d.age << '\n';
    std::string path = folder + "/drops.dat";
    std::ofstream out(path + ".tmp", std::ios::trunc);
    out << o.str();
    out.close();
    if (!out) return false;
    return std::rename((path + ".tmp").c_str(), path.c_str()) == 0;
}

void DropSystem::load(const std::string& folder) {
    drops_.clear();
    std::ifstream in(folder + "/drops.dat");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        DroppedItem d;
        int id, count;
        if (!(ls >> d.dim >> d.pos.x >> d.pos.y >> d.pos.z >> id >> count >> d.stack.damage >> d.age)) continue;
        if (id <= 0 || id >= kItemIdLimit || count <= 0 || count > maxStack((ItemId)id) || (d.dim != 0 && d.dim != 1)) continue;
        d.stack.id = (ItemId)id;
        d.stack.count = (uint8_t)count;
        drops_.push_back(d);
    }
}

}  // namespace vc

#include "Boats.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "Blocks.h"

namespace vc {

namespace {

constexpr float kTile = kTileUv;

bool solid(const World& w, float x, float y, float z) {
    return isSolid(w.getBlock((int)std::floor(x), (int)std::floor(y), (int)std::floor(z)));
}

// Water surface height at (x, z) near y, or -1 when there is no water there.
float waterTop(const World& w, float x, float y, float z) {
    int bx = (int)std::floor(x), bz = (int)std::floor(z), by = (int)std::floor(y);
    for (int yy = by + 1; yy >= by - 1; --yy)
        if (w.getBlock(bx, yy, bz) == WATER && w.getBlock(bx, yy + 1, bz) != WATER) return yy + 0.9f;
    return -1;
}

}  // namespace

int BoatSystem::spawn(int dim, const Vec3& pos, float yaw) {
    Boat b;
    b.dim = dim;
    b.pos = pos;
    b.yaw = yaw;
    boats_.push_back(b);
    return (int)boats_.size() - 1;
}

void BoatSystem::remove(int index) {
    if (index >= 0 && index < (int)boats_.size()) boats_.erase(boats_.begin() + index);
}

void BoatSystem::update(const World& world, int dim, float dt, int ridden, float moveForward, float moveSide, float heading) {
    for (int i = 0; i < (int)boats_.size(); ++i) {
        Boat& b = boats_[i];
        if (b.dim != dim) continue;
        b.damage = std::max(0.0f, b.damage - dt * 0.5f);
        float top = waterTop(world, b.pos.x, b.pos.y, b.pos.z);
        const bool afloat = top >= 0;
        if (i == ridden) {
            // Steering: the boat turns smoothly towards where the player looks.
            float target = heading, diff = std::remainder(target - b.yaw, 2 * 3.14159265f);
            b.yaw += diff * std::min(1.0f, dt * 4);
            const float speed = afloat ? 6.0f : 1.2f;
            Vec3 fwd(std::sin(b.yaw), 0, -std::cos(b.yaw)), side(std::cos(b.yaw), 0, std::sin(b.yaw));
            Vec3 want = fwd * (moveForward * speed) + side * (moveSide * speed * 0.6f);
            b.vel.x += (want.x - b.vel.x) * std::min(1.0f, dt * 3);
            b.vel.z += (want.z - b.vel.z) * std::min(1.0f, dt * 3);
        } else {
            b.vel.x *= std::max(0.0f, 1 - dt * 2);
            b.vel.z *= std::max(0.0f, 1 - dt * 2);
        }
        // Float on the surface, or fall onto the ground.
        if (afloat) {
            b.pos.y += (top - 0.35f - b.pos.y) * std::min(1.0f, dt * 6);
            b.vel.y = 0;
        } else {
            b.vel.y = std::max(b.vel.y - 20 * dt, -20.0f);
        }
        Vec3 next = b.pos + b.vel * dt;
        // Hitting the shore or a wall stops it on that axis.
        auto blocked = [&](const Vec3& p) {
            for (float ox : {-kHalfWidth, kHalfWidth})
                for (float oz : {-kHalfWidth, kHalfWidth})
                    if (solid(world, p.x + ox, p.y + 0.15f, p.z + oz) || solid(world, p.x + ox, p.y + kHeight, p.z + oz)) return true;
            return false;
        };
        if (blocked(Vec3(next.x, b.pos.y, b.pos.z))) next.x = b.pos.x, b.vel.x = 0;
        if (blocked(Vec3(next.x, b.pos.y, next.z))) next.z = b.pos.z, b.vel.z = 0;
        if (!afloat && solid(world, next.x, next.y, next.z)) {
            next.y = std::floor(next.y) + 1;
            b.vel.y = 0;
        }
        b.pos = next;
    }
}

int BoatSystem::pick(int dim, const Vec3& origin, const Vec3& dir, float maxDist, float& dist) const {
    int best = -1;
    dist = maxDist;
    for (int i = 0; i < (int)boats_.size(); ++i) {
        const Boat& b = boats_[i];
        if (b.dim != dim) continue;
        // A round box is good enough: the boat turns, so use its larger half size.
        const float r = kHalfLength;
        const float lo[3] = {b.pos.x - r, b.pos.y, b.pos.z - r}, hi[3] = {b.pos.x + r, b.pos.y + kHeight, b.pos.z + r};
        const float o[3] = {origin.x, origin.y, origin.z}, d[3] = {dir.x, dir.y, dir.z};
        float t0 = 0, t1 = maxDist;
        bool hit = true;
        for (int k = 0; k < 3 && hit; ++k) {
            if (std::fabs(d[k]) < 1e-9f) {
                hit = o[k] >= lo[k] && o[k] <= hi[k];
                continue;
            }
            float a = (lo[k] - o[k]) / d[k], c = (hi[k] - o[k]) / d[k];
            if (a > c) std::swap(a, c);
            t0 = std::max(t0, a);
            t1 = std::min(t1, c);
            hit = t0 <= t1;
        }
        if (hit && t0 < dist) {
            dist = t0;
            best = i;
        }
    }
    return best;
}

void BoatSystem::buildVertices(int dim, std::vector<Vertex>& out) const {
    out.clear();
    const int tile = tileFor(PLANKS, F_TOP);
    const float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile;
    for (const Boat& b : boats_) {
        if (b.dim != dim) continue;
        const float cs = std::cos(b.yaw), sn = std::sin(b.yaw);
        // Local x across, z along the boat (forward is -z), y up.
        auto P = [&](float x, float y, float z) { return Vec3(b.pos.x + x * cs - z * sn, b.pos.y + y, b.pos.z + x * sn + z * cs); };
        auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1, float light) {
            const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
            static const int faces[6][4][3] = {
                {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
                {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
                {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}};
            static const float shade[6] = {1.0f, 0.5f, 0.8f, 0.8f, 0.6f, 0.6f};
            for (int f = 0; f < 6; ++f) {
                Vertex q[4];
                const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
                for (int k = 0; k < 4; ++k) {
                    Vec3 p = P(faces[f][k][0] ? hi[0] : lo[0], faces[f][k][1] ? hi[1] : lo[1], faces[f][k][2] ? hi[2] : lo[2]);
                    q[k] = {p.x, p.y, p.z, u0 + uv[k][0] * kTile, v0 + uv[k][1] * kTile, shade[f] * light};
                }
                out.insert(out.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
            }
        };
        const float w = kHalfWidth, l = kHalfLength, t = 0.1f, h = kHeight;
        box(-w, 0, -l, w, t, l, 0.95f);              // bottom
        box(-w, t, -l, -w + t, h, l, 0.95f);         // left side
        box(w - t, t, -l, w, h, l, 0.95f);           // right side
        box(-w + t, t, -l, w - t, h, -l + t, 0.95f); // bow
        box(-w + t, t, l - t, w - t, h, l, 0.95f);   // stern
        box(-w + t, 0.3f, -0.08f, w - t, 0.38f, 0.08f, 0.9f);  // seat
    }
}

bool BoatSystem::save(const std::string& folder) const {
    std::ostringstream o;
    for (const Boat& b : boats_) o << "boat " << b.dim << ' ' << b.pos.x << ' ' << b.pos.y << ' ' << b.pos.z << ' ' << b.yaw << '\n';
    std::string path = folder + "/entities.dat";
    std::ofstream out(path + ".tmp", std::ios::trunc);
    out << o.str();
    out.close();
    return out && std::rename((path + ".tmp").c_str(), path.c_str()) == 0;
}

void BoatSystem::load(const std::string& folder) {
    boats_.clear();
    std::ifstream in(folder + "/entities.dat");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string kind;
        Boat b;
        if (ls >> kind >> b.dim >> b.pos.x >> b.pos.y >> b.pos.z >> b.yaw && kind == "boat" && (b.dim == 0 || b.dim == 1))
            boats_.push_back(b);
    }
}

}  // namespace vc

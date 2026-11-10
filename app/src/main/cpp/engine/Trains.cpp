#include "Trains.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace vc {

namespace {

#include "TrainModels.inc"

constexpr int kDx[4] = {0, 1, 0, -1}, kDz[4] = {-1, 0, 1, 0};
constexpr float kTile = kTileUv;
constexpr float kHalfPi = 1.57079633f;

// Solid parts of a car in model units (x across, y up from the rail head, z along; the front is -z):
// a floor, side walls with a door in the middle, end walls and a roof. Players walk in by the doors.
struct CarBox {
    float x0, y0, z0, x1, y1, z1;
};
int carBoxes(CarKind kind, CarBox out[16]) {
    int n = 0;
    const float w = 0.72f, t = 0.08f, len = 2.0f;
    out[n++] = {-w, 0.2f, -len, w, 0.52f, len};  // the floor (and the frame under it)
    if (kind == CarKind::Freight) {
        // Low walls all round and the load in the middle: climb on top of it.
        out[n++] = {-w, 0.52f, -len, -w + t, 1.36f, len};
        out[n++] = {w - t, 0.52f, -len, w, 1.36f, len};
        out[n++] = {-w, 0.52f, -len, w, 1.36f, -len + t};
        out[n++] = {-w, 0.52f, len - t, w, 1.36f, len};
        out[n++] = {-0.64f, 0.52f, -1.64f, 0.64f, 1.58f, 1.66f};
        return n;
    }
    // Passenger car: walls the whole length with a door in the middle of each side. Locomotive:
    // the cab at the back (z < -0.4) with a door each side, the engine and boiler in front.
    const float cab1 = kind == CarKind::Locomotive ? -0.4f : len;
    const float door0 = kind == CarKind::Locomotive ? -1.95f : -0.38f, door1 = kind == CarKind::Locomotive ? -1.5f : 0.38f;
    for (float side : {-1.0f, 1.0f}) {
        const float x0 = side < 0 ? -w : w - t, x1 = side < 0 ? -w + t : w;
        if (door0 > -len) out[n++] = {x0, 0.52f, -len, x1, 2.2f, door0};
        out[n++] = {x0, 0.52f, door1, x1, 2.2f, cab1};
        out[n++] = {x0, 1.75f, door0, x1, 2.2f, door1};  // above the door
    }
    out[n++] = {-w, 0.52f, -len, w, 2.2f, -len + t};  // the back end
    out[n++] = {-w, 2.2f, -len, w, 2.36f, cab1};       // the roof
    if (kind == CarKind::Locomotive) {
        out[n++] = {-w, 0.52f, -0.4f, w, 1.6f, len};          // the engine
        out[n++] = {-0.57f, 1.6f, -0.4f, 0.57f, 2.2f, len};   // the boiler
    } else {
        out[n++] = {-w, 0.52f, len - t, w, 2.2f, len};  // the front end
    }
    return n;
}

Vec3 sideMid(int side) { return Vec3(0.5f + kDx[side] * 0.5f, 0, 0.5f + kDz[side] * 0.5f); }

}  // namespace

bool TrainSystem::railSides(ItemId id, int& a, int& b) {
    if (id == RAIL_NS) return a = 0, b = 2, true;
    if (id == RAIL_EW) return a = 1, b = 3, true;
    if (id >= RAIL_CURVE && id < RAIL_CURVE + 4) {
        a = id - RAIL_CURVE;
        b = (a + 1) & 3;
        return true;
    }
    return false;
}

bool TrainSystem::nextSide(ItemId id, int in, int& out) {
    int a, b;
    if (railSides(id, a, b)) {
        if (in == a) return out = b, true;
        if (in == b) return out = a, true;
        return false;
    }
    if (isRailSwitch(id)) {
        // Straight between f and f + 2, the branch between f + 2 and f + 1 (see railSwitchId).
        const int k = railSwitchIndex(id), f = k / 2, st = k % 2;
        const int s0 = f, s2 = (f + 2) & 3, s1 = (f + 1) & 3;
        if (in == s0 || in == s1) return out = s2, true;
        if (in == s2) return out = st ? s1 : s0, true;
        return false;
    }
    if (isBufferStop(id)) {
        const int g = id - BUFFER_STOP;  // the plate's side: trains come in from the other one
        if (in == ((g + 2) & 3)) return out = g, true;
        return false;
    }
    return false;
}

void TrainSystem::flip(RailPos& p) {
    std::swap(p.in, p.out);
    p.t = 1 - p.t;
}

Vec3 TrainSystem::pointOn(const RailPos& p, Vec3* tangent) {
    const Vec3 a = sideMid(p.in), b = sideMid(p.out);
    Vec3 local, tan;
    if (((p.in + 2) & 3) == p.out) {
        local = a + (b - a) * p.t;
        tan = b - a;
    } else {
        // A quarter circle round the corner the two sides share, radius half a block.
        const Vec3 corner = a + b - Vec3(0.5f, 0, 0.5f);
        const Vec3 u = a - corner, v = b - corner;
        const float ang = p.t * kHalfPi;
        local = corner + u * std::cos(ang) + v * std::sin(ang);
        tan = v * std::cos(ang) - u * std::sin(ang);
    }
    if (tangent) {
        const float l = std::sqrt(tan.x * tan.x + tan.z * tan.z);
        *tangent = l > 0 ? Vec3(tan.x / l, 0, tan.z / l) : Vec3(0, 0, -1);
    }
    return Vec3(p.x + local.x, p.y + 1 / 16.0f, p.z + local.z);
}

bool TrainSystem::advance(const World& world, RailPos& p, float dist, float& moved) {
    moved = 0;
    for (int guard = 0; guard < 4096 && dist > 1e-5f; ++guard) {
        const ItemId here = world.getBlock(p.x, p.y, p.z);
        const bool curve = ((p.in + 2) & 3) != p.out;
        const float len = curve ? kHalfPi * 0.5f : 1.0f;
        const float end = isBufferStop(here) ? 0.5f : 1.0f;  // a buffer stop: the middle is the end
        const float left = (end - p.t) * len;
        if (dist <= left) {
            p.t += dist / len;
            moved += dist;
            return true;
        }
        moved += std::max(0.0f, left);
        dist -= std::max(0.0f, left);
        if (isBufferStop(here)) {
            p.t = end;
            return false;
        }
        // Into the next cell, if a rail there takes us.
        const int nx = p.x + kDx[p.out], nz = p.z + kDz[p.out], nin = (p.out + 2) & 3;
        const ItemId next = world.getBlock(nx, p.y, nz);
        int nout;
        if (!(isRail(next) || isBufferStop(next)) || !nextSide(next, nin, nout)) {
            p.t = 1;
            return false;
        }
        p.x = nx, p.z = nz, p.in = nin, p.out = nout, p.t = 0;
    }
    return true;
}

void TrainSystem::refresh(TrainCar& car) const {
    Vec3 tan;
    car.pos = pointOn(car.at, &tan);
    car.yaw = std::atan2(-tan.x, tan.z);  // the models' front is +z
}

bool TrainSystem::place(const World& world, int dim, CarKind kind, int x, int y, int z, int look) {
    const ItemId here = world.getBlock(x, y, z);
    if (!isRail(here)) return false;
    // Behind the last car of a train that ends close by.
    for (Train& tr : trains_) {
        if (tr.dim != dim || tr.cars.empty() || tr.cars.size() >= 12) continue;
        const TrainCar& last = tr.cars.back();
        const float dx = last.pos.x - (x + 0.5f), dz = last.pos.z - (z + 0.5f);
        if (dx * dx + dz * dz > (kPitch * 1.6f) * (kPitch * 1.6f)) continue;
        TrainCar car;
        car.kind = kind;
        car.at = last.at;
        flip(car.at);
        float moved;
        if (!advance(world, car.at, kPitch, moved)) return false;
        flip(car.at);
        refresh(car);
        tr.cars.push_back(car);
        return true;
    }
    // A new train, heading the way the player looks if the rail goes that way.
    int a, b;
    RailPos at;
    at.x = x, at.y = y, at.z = z, at.t = 0.5f;
    if (railSides(here, a, b)) {
        at.out = (b == look || ((a + 2) & 3) == look) ? b : a;
        at.in = at.out == a ? b : a;
        if (at.out != look && at.in == look) std::swap(at.in, at.out);
    } else {
        const int k = railSwitchIndex(here), f = k / 2;
        at.in = (f + 2) & 3;
        at.out = f;
        if (look == at.in) flip(at);
    }
    Train tr;
    tr.dim = dim;
    TrainCar car;
    car.kind = kind;
    car.at = at;
    refresh(car);
    tr.cars.push_back(car);
    trains_.push_back(tr);
    return true;
}

void TrainSystem::update(const World& world, int dim, float dt, Vec3& playerPos, float& playerYaw, bool playerSeated) {
    for (Train& tr : trains_) {
        if (tr.dim != dim || tr.cars.empty()) continue;
        const bool hasEngine = tr.cars.front().kind == CarKind::Locomotive;
        const float target = hasEngine ? tr.throttle * kMaxSpeed : 0;
        if (tr.brake) {
            const float s = std::max(0.0f, std::fabs(tr.speed) - kBrake * dt);
            tr.speed = tr.speed < 0 ? -s : s;
        } else if (tr.speed < target) {
            tr.speed = std::min(target, tr.speed + kAccel * dt);
        } else if (tr.speed > target) {
            tr.speed = std::max(target, tr.speed - kAccel * dt);
        }
        if (std::fabs(tr.speed) < 1e-4f) continue;
        // The player standing in or on a car rides along: remember where in the car they are.
        int carried = -1;
        Vec3 local;
        float yawBefore = 0;
        if (!playerSeated) {
            for (size_t i = 0; i < tr.cars.size(); ++i) {
                const TrainCar& c = tr.cars[i];
                const Vec3 d = playerPos - c.pos;
                const float cs = std::cos(-c.yaw), sn = std::sin(-c.yaw);
                const Vec3 l(d.x * cs - d.z * sn, d.y, d.x * sn + d.z * cs);
                if (std::fabs(l.x) < 0.75f * kScale && std::fabs(l.z) < 2.05f * kScale && l.y > 0.2f * kScale && l.y < 2.6f * kScale) {
                    carried = (int)i, local = l, yawBefore = c.yaw;
                    break;
                }
            }
        }
        // The leading car (the back one when reversing) moves first; the rest follow by as much.
        const bool fwd = tr.speed > 0;
        const float want = std::fabs(tr.speed) * dt;
        TrainCar& lead = fwd ? tr.cars.front() : tr.cars.back();
        if (!fwd) flip(lead.at);
        float moved;
        const bool ok = advance(world, lead.at, want, moved);
        if (!fwd) flip(lead.at);
        for (TrainCar& c : tr.cars) {
            if (&c == &lead) continue;
            if (!fwd) flip(c.at);
            float m2;
            advance(world, c.at, moved, m2);
            if (!fwd) flip(c.at);
        }
        if (!ok) tr.speed = 0, tr.throttle = 0;  // the end of the track: the train stops
        for (TrainCar& c : tr.cars) refresh(c);
        if (carried >= 0) {
            const TrainCar& c = tr.cars[(size_t)carried];
            const float cs = std::cos(c.yaw), sn = std::sin(c.yaw);
            playerPos = c.pos + Vec3(local.x * cs - local.z * sn, local.y, local.x * sn + local.z * cs);
            playerYaw += c.yaw - yawBefore;
        }
    }
}

bool TrainSystem::collide(int dim, Vec3& pos, Vec3& vel) const {
    const float r = 0.3f, h = 1.8f;
    bool onCar = false;
    for (const Train& tr : trains_) {
        if (tr.dim != dim) continue;
        for (const TrainCar& c : tr.cars) {
            Vec3 d = pos - c.pos;
            if (d.x * d.x + d.z * d.z > 64 || d.y < -3 || d.y > 6) continue;
            const float cs = std::cos(-c.yaw), sn = std::sin(-c.yaw);
            Vec3 l(d.x * cs - d.z * sn, d.y, d.x * sn + d.z * cs);
            CarBox boxes[16];
            const int n = carBoxes(c.kind, boxes);
            bool moved = false;
            for (int i = 0; i < n; ++i) {
                const CarBox& b0 = boxes[i];
                const float x0 = b0.x0 * kScale, x1 = b0.x1 * kScale, y0 = b0.y0 * kScale, y1 = b0.y1 * kScale;
                const float z0 = b0.z0 * kScale, z1 = b0.z1 * kScale;
                if (l.y >= y1 || l.y + h <= y0) continue;
                const float cx = std::max(x0, std::min(l.x, x1)), cz = std::max(z0, std::min(l.z, z1));
                const float ex = l.x - cx, ez = l.z - cz;
                if (ex * ex + ez * ez >= r * r) continue;
                // Step up onto it if it is low enough under the feet, else push out sideways.
                // The floor is a high step (0.8 of a block): it can be walked up onto like a stair.
                if (y1 - l.y <= (i == 0 ? 0.95f : 0.55f)) {
                    l.y = y1;
                    if (vel.y < 0) vel.y = 0;
                    onCar = true;
                } else if (l.y + h - y0 < 0.4f && vel.y > 0) {
                    l.y = y0 - h;  // bumped the head
                    vel.y = 0;
                } else {
                    const float pushL = l.x + r - x0, pushR = x1 - (l.x - r), pushF = l.z + r - z0, pushB = z1 - (l.z - r);
                    const float m = std::min(std::min(pushL, pushR), std::min(pushF, pushB));
                    if (m == pushL) l.x -= pushL;
                    else if (m == pushR) l.x += pushR;
                    else if (m == pushF) l.z -= pushF;
                    else l.z += pushB;
                }
                moved = true;
            }
            if (moved) {
                const float c2 = std::cos(c.yaw), s2 = std::sin(c.yaw);
                pos = c.pos + Vec3(l.x * c2 - l.z * s2, l.y, l.x * s2 + l.z * c2);
            }
        }
    }
    return onCar;
}

int TrainSystem::pick(int dim, const Vec3& origin, const Vec3& dir, float maxDist, float& dist) const {
    int best = -1;
    dist = maxDist;
    for (size_t ti = 0; ti < trains_.size(); ++ti) {
        const Train& tr = trains_[ti];
        if (tr.dim != dim) continue;
        for (size_t ci = 0; ci < tr.cars.size(); ++ci) {
            const TrainCar& c = tr.cars[ci];
            const float cs = std::cos(-c.yaw), sn = std::sin(-c.yaw);
            const Vec3 d = origin - c.pos;
            const Vec3 o(d.x * cs - d.z * sn, d.y, d.x * sn + d.z * cs), v(dir.x * cs - dir.z * sn, dir.y, dir.x * sn + dir.z * cs);
            const float lo[3] = {-0.75f * kScale, 0.2f * kScale, -2.1f * kScale}, hi[3] = {0.75f * kScale, 2.4f * kScale, 2.1f * kScale};
            const float oo[3] = {o.x, o.y, o.z}, vv[3] = {v.x, v.y, v.z};
            float t0 = 0, t1 = dist;
            bool hit = true;
            for (int k = 0; k < 3 && hit; ++k) {
                if (std::fabs(vv[k]) < 1e-6f) {
                    if (oo[k] < lo[k] || oo[k] > hi[k]) hit = false;
                    continue;
                }
                float a = (lo[k] - oo[k]) / vv[k], b = (hi[k] - oo[k]) / vv[k];
                if (a > b) std::swap(a, b);
                t0 = std::max(t0, a), t1 = std::min(t1, b);
                if (t0 > t1) hit = false;
            }
            if (hit && t0 < dist) dist = t0, best = (int)(ti * 64 + ci);
        }
    }
    return best;
}

bool TrainSystem::nearest(int dim, const Vec3& pos, float range, int& train, int& car) const {
    float best = range * range;
    train = car = -1;
    for (size_t ti = 0; ti < trains_.size(); ++ti) {
        if (trains_[ti].dim != dim) continue;
        for (size_t ci = 0; ci < trains_[ti].cars.size(); ++ci) {
            const Vec3 d = trains_[ti].cars[ci].pos - pos;
            const float d2 = d.x * d.x + d.z * d.z + d.y * d.y * 0.25f;
            if (d2 < best) best = d2, train = (int)ti, car = (int)ci;
        }
    }
    return train >= 0;
}

int TrainSystem::seatCount(CarKind kind) const {
    return kind == CarKind::Locomotive ? kTrainSeatCount_locomotive : kind == CarKind::Passenger ? kTrainSeatCount_passenger : 0;
}

Vec3 TrainSystem::seatPos(const TrainCar& car, int seat) const {
    const float(*seats)[3] = car.kind == CarKind::Locomotive ? kTrainSeats_locomotive : kTrainSeats_passenger;
    const float* s = seats[std::max(0, std::min(seat, seatCount(car.kind) - 1))];
    const float lx = s[0] * kScale, ly = (s[1] - 0.3f) * kScale, lz = s[2] * kScale;
    const float cs = std::cos(car.yaw), sn = std::sin(car.yaw);
    return car.pos + Vec3(lx * cs - lz * sn, ly, lx * sn + lz * cs);
}

void TrainSystem::remove(int train, int car) {
    if (train < 0 || train >= (int)trains_.size()) return;
    Train& tr = trains_[(size_t)train];
    if (car <= 0) {
        trains_.erase(trains_.begin() + train);
        return;
    }
    if (car < (int)tr.cars.size()) tr.cars.resize((size_t)car);
}

void TrainSystem::removeCar(int train, int car) {
    if (train < 0 || train >= (int)trains_.size()) return;
    Train& tr = trains_[(size_t)train];
    if (car < 0 || car >= (int)tr.cars.size()) return;
    tr.cars.erase(tr.cars.begin() + car);
    if (tr.cars.empty()) trains_.erase(trains_.begin() + train);
    else if (car == 0) tr.speed = tr.throttle = 0;  // the engine is gone: the wagons stop
}

void TrainSystem::buildVertices(int dim, std::vector<Vertex>& out) const {
    const int tile = T_TRAIN_PALETTE;
    const float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile;
    for (const Train& tr : trains_) {
        if (tr.dim != dim) continue;
        for (const TrainCar& c : tr.cars) {
            const int16_t* data = c.kind == CarKind::Locomotive ? kTrain_locomotive : c.kind == CarKind::Passenger ? kTrain_passenger : kTrain_freight;
            const size_t count = (c.kind == CarKind::Locomotive ? sizeof(kTrain_locomotive) : c.kind == CarKind::Passenger ? sizeof(kTrain_passenger) : sizeof(kTrain_freight)) /
                                 sizeof(int16_t) / 5;
            const float cs = std::cos(c.yaw), sn = std::sin(c.yaw), k = kScale / 2048.0f;
            for (size_t i = 0; i < count; ++i) {
                const int16_t* v = data + i * 5;
                const float lx = v[0] * k, ly = v[1] * k, lz = v[2] * k;
                const int ci = v[3];
                // The colour: the middle of its cell in the palette tile (4 x 4 cells).
                const float u = u0 + ((ci % 4) * 4 + 2) / 16.0f * kTile, vv = v0 + ((ci / 4) * 4 + 2) / 16.0f * kTile;
                out.push_back({c.pos.x + lx * cs - lz * sn, c.pos.y + ly, c.pos.z + lx * sn + lz * cs, u, vv, v[4] / 255.0f});
            }
        }
    }
}

bool TrainSystem::save(const std::string& folder) const {
    std::ostringstream o;
    for (const Train& tr : trains_) {
        o << "train " << tr.dim << ' ' << tr.cars.size() << '\n';
        for (const TrainCar& c : tr.cars)
            o << "car " << (int)c.kind << ' ' << c.at.x << ' ' << c.at.y << ' ' << c.at.z << ' ' << c.at.in << ' ' << c.at.out << ' ' << c.at.t << '\n';
    }
    std::ofstream out(folder + "/trains.dat", std::ios::trunc);
    out << o.str();
    return (bool)out;
}

void TrainSystem::load(const std::string& folder) {
    trains_.clear();
    std::ifstream in(folder + "/trains.dat");
    std::string word;
    while (in >> word) {
        if (word == "train") {
            Train tr;
            size_t n = 0;
            in >> tr.dim >> n;
            for (size_t i = 0; i < n && in >> word && word == "car"; ++i) {
                TrainCar c;
                int kind = 0;
                in >> kind >> c.at.x >> c.at.y >> c.at.z >> c.at.in >> c.at.out >> c.at.t;
                c.kind = (CarKind)std::max(0, std::min(kind, 2));
                c.at.in &= 3, c.at.out &= 3;
                refresh(c);
                tr.cars.push_back(c);
            }
            if (!tr.cars.empty()) trains_.push_back(tr);
        }
    }
}

}  // namespace vc

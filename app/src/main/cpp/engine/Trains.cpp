#include "Trains.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace vc {

namespace {

#include "TrainModels.inc"

// Build 12: a cell has eight ports: the middles of its sides (even: 0 N, 2 E, 4 S, 6 W) and its corners
// (odd: 1 NE, 3 SE, 5 SW, 7 NW). Leaving by port p goes to the neighbour that way, in by port p + 4.
constexpr int kPx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, kPz[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
constexpr float kTile = kTileUv;
constexpr float kHalfPi = 1.57079633f;

// Solid parts of a car in model units (x across, y up from the rail head, z along; the front is -z):
// a floor, side walls with a door in the middle, end walls and a roof. Players walk in by the doors.
struct CarBox {
    float x0, y0, z0, x1, y1, z1;
};
int carBoxes(CarKind kind, CarBox out[16], float doors = 0) {
    int n = 0;
    if (isSubwayCar(kind)) {
        // 2.3.0: the subway: a long body with two doorways each side (z -1.4..-0.6 and 0.6..1.4);
        // their leaves block the way until they slide open.
        const float w = 0.92f, t = 0.08f, len = 2.55f, top = 2.45f;
        out[n++] = {-w, 0.2f, -len, w, 0.5f, len};
        for (float side : {-1.0f, 1.0f}) {
            const float x0 = side < 0 ? -w : w - t, x1 = side < 0 ? -w + t : w;
            if (doors > 0.6f) {
                out[n++] = {x0, 0.5f, -len, x1, top, -1.4f};
                out[n++] = {x0, 0.5f, -0.6f, x1, top, 0.6f};
                out[n++] = {x0, 0.5f, 1.4f, x1, top, len};
                out[n++] = {x0, 1.85f, -1.4f, x1, top, -0.6f};
                out[n++] = {x0, 1.85f, 0.6f, x1, top, 1.4f};
            } else {
                out[n++] = {x0, 0.5f, -len, x1, top, len};
            }
        }
        out[n++] = {-w, 0.5f, -len, w, top, -len + t};
        out[n++] = {-w, 0.5f, len - t, w, top, len};
        out[n++] = {-w, top, -len, w, top + 0.08f, len};
        return n;
    }
    const float w = 0.72f, t = 0.08f, len = 2.0f;
    out[n++] = {-w, 0.2f, -len, w, 0.52f, len};  // the floor (and the frame under it)
    if (kind == CarKind::Drill) {
        // Build 5: the machine house fills the car; the drill sticks out in front.
        out[n++] = {-w, 0.52f, -1.9f, w, 1.95f, 1.95f};
        out[n++] = {-0.72f, 0.34f, 1.95f, 0.72f, 1.78f, 3.15f};
        return n;
    }
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

Vec3 portPos(int p) { return Vec3(0.5f + kPx[p & 7] * 0.5f, 0, 0.5f + kPz[p & 7] * 0.5f); }

// The shape of the piece between two ports: 0 straight through the middle (a side to the opposite side,
// or a corner to the opposite corner), 1 a quarter circle (two neighbouring sides), 2 a bend of 45
// degrees (a side to a far corner, through the middle).
int pathShape(int in, int out) {
    const int d = (out - in) & 7;
    if (d == 4) return 0;
    if ((in & 1) == 0 && (out & 1) == 0) return 1;
    return 2;
}
}  // namespace

bool TrainSystem::railSides(ItemId id, int& a, int& b) {
    // The two ports of a plain piece (see kPx).
    if (isTurntable(id)) {
        a = id == TURNTABLE ? 0 : 2;
        b = a + 4;
        return true;
    }
    if (isRailSlope(id) || isRailGentle(id)) {  // up the slope towards side d, down towards the opposite side
        a = (isRailSlope(id) ? id - RAIL_SLOPE : (id - RAIL_GENTLE) / 4) * 2;
        b = (a + 4) & 7;
        return true;
    }
    if (id == RAIL_NS) return a = 0, b = 4, true;
    if (id == RAIL_EW) return a = 2, b = 6, true;
    if (id >= RAIL_CURVE && id < RAIL_CURVE + 4) {
        a = (id - RAIL_CURVE) * 2;
        b = (a + 2) & 7;
        return true;
    }
    if (isRailDiag(id)) return a = id == RAIL_DIAG ? 1 : 7, b = a + 4 & 7, true;
    if (isRailBend(id)) {
        const int k = id - RAIL_BEND, c = (k / 2) * 2;
        a = c;
        b = (c + 4 + (k % 2 ? 1 : -1)) & 7;
        return true;
    }
    return false;
}

ItemId TrainSystem::railFor(int a, int b) {
    // The plain piece between ports a and b, or AIR when no piece joins them.
    if (a == b) return AIR;
    if (a > b) std::swap(a, b);
    const int d = b - a;
    if (d == 4) {
        if (a == 0) return RAIL_NS;
        if (a == 2) return RAIL_EW;
        return a == 1 ? (ItemId)RAIL_DIAG : (ItemId)(RAIL_DIAG + 1);
    }
    if ((a & 1) == 0 && (b & 1) == 0) {
        if (d == 2) return (ItemId)(RAIL_CURVE + a / 2);
        if (d == 6) return (ItemId)(RAIL_CURVE + 3);  // west to north
        return AIR;
    }
    if (d != 3 && d != 5) return AIR;
    const int c = (a & 1) == 0 ? a : b, corner = c == a ? b : a;
    const int s = corner == ((c + 5) & 7) ? 1 : 0;
    return (ItemId)(RAIL_BEND + (c / 2) * 2 + s);
}

bool TrainSystem::nextSide(ItemId id, int in, int& out) {
    int a, b;
    if (railSides(id, a, b)) {
        if (in == a) return out = b, true;
        if (in == b) return out = a, true;
        return false;
    }
    if (isRailSwitch(id)) {
        // Straight between f and f + 2, the branch between f + 2 and f + 1 (see railSwitchId), in sides.
        const int k = railSwitchIndex(id), f = k / 2, st = k % 2;
        const int s0 = f * 2, s2 = ((f + 2) & 3) * 2, s1 = railSwitchBranch(id) * 2;
        if (in == s0 || in == s1) return out = s2, true;
        if (in == s2) return out = st ? s1 : s0, true;
        return false;
    }
    if (isRailSwitch8(id)) {
        const int e = railSwitch8Entry(id), straight = (e + 4) & 7, branch = railSwitch8Branch(id);
        if (in == e) return out = railSwitch8State(id) ? branch : straight, true;
        if (in == straight || in == branch) return out = e, true;
        return false;
    }
    if (isBufferStop(id)) {
        const int g = (id - BUFFER_STOP) * 2;  // the plate's side: trains come in from the other one
        if (in == ((g + 4) & 7)) return out = g, true;
        return false;
    }
    return false;
}

void TrainSystem::flip(RailPos& p) {
    std::swap(p.in, p.out);
    p.t = 1 - p.t;
}

void TrainSystem::setSlope(RailPos& p, ItemId id) {
    p.slopeHigh = -1, p.slopeLo = 0, p.slopeSpan = 1;
    if (isRailSlope(id)) p.slopeHigh = (id - RAIL_SLOPE) * 2;
    if (isRailGentle(id)) {
        const int k = id - RAIL_GENTLE;
        p.slopeHigh = (k / 4) * 2, p.slopeLo = (k % 4) * 0.25f, p.slopeSpan = 0.25f;
    }
}

float TrainSystem::pathLength(int in, int out) {
    switch (pathShape(in, out)) {
        case 0: return (in & 1) ? 1.41421f : 1.0f;
        case 1: return kHalfPi * 0.5f;
        default: return 1.2f;  // the 45-degree bend
    }
}

Vec3 TrainSystem::pointOn(const RailPos& p, Vec3* tangent) {
    const Vec3 a = portPos(p.in), b = portPos(p.out);
    Vec3 local, tan;
    switch (pathShape(p.in, p.out)) {
        case 0:
            local = a + (b - a) * p.t;
            tan = b - a;
            break;
        case 1: {
            // A quarter circle round the corner the two sides share, radius half a block.
            const Vec3 corner = a + b - Vec3(0.5f, 0, 0.5f);
            const Vec3 u = a - corner, v = b - corner;
            const float ang = p.t * kHalfPi;
            local = corner + u * std::cos(ang) + v * std::sin(ang);
            tan = v * std::cos(ang) - u * std::sin(ang);
            break;
        }
        default: {
            // Build 12: the 45-degree bend, a smooth curve from one port to the other pulled through the middle.
            const Vec3 m(0.5f, 0, 0.5f);
            const float t = p.t, u = 1 - t;
            local = a * (u * u) + m * (2 * u * t) + b * (t * t);
            tan = (m - a) * (2 * u) + (b - m) * (2 * t);
            break;
        }
    }
    if (tangent) {
        const float l = std::sqrt(tan.x * tan.x + tan.z * tan.z);
        *tangent = l > 0 ? Vec3(tan.x / l, 0, tan.z / l) : Vec3(0, 0, -1);
    }
    // On a slope the rail rises from 0 at the low side to 1 at the high side (`slopeHigh`).
    float rise = 0;
    if (p.slopeHigh >= 0) rise = p.slopeLo + p.slopeSpan * (p.out == p.slopeHigh ? p.t : 1 - p.t);
    if (tangent && p.slopeHigh >= 0) {
        const float up = (p.out == p.slopeHigh ? 1.0f : -1.0f) * p.slopeSpan;
        const Vec3 t3(tangent->x, up, tangent->z);
        *tangent = t3 * (1.0f / std::sqrt(1 + up * up));
    }
    return Vec3(p.x + local.x, p.y + 1 / 16.0f + rise, p.z + local.z);
}

bool TrainSystem::advance(const World& world, RailPos& p, float dist, float& moved) {
    moved = 0;
    for (int guard = 0; guard < 4096 && dist > 1e-5f; ++guard) {
        const ItemId here = world.getBlock(p.x, p.y, p.z);
        const float len = p.slopeHigh >= 0 ? std::sqrt(1 + p.slopeSpan * p.slopeSpan) : pathLength(p.in, p.out);
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
        // Into the next cell, if a rail there takes us: one block up after climbing a slope, or
        // one block down onto a slope that falls away from us.
        const int nx = p.x + kPx[p.out], nz = p.z + kPz[p.out], nin = (p.out + 4) & 7;
        int ny = p.y + (p.slopeHigh >= 0 && p.out == p.slopeHigh && p.slopeLo + p.slopeSpan > 0.99f ? 1 : 0);
        ItemId next = world.getBlock(nx, ny, nz);
        int nout;
        bool ok = (isRail(next) || isBufferStop(next)) && nextSide(next, nin, nout);
        if (!ok) {
            const ItemId below = world.getBlock(nx, ny - 1, nz);
            const bool topFull = isRailSlope(below) || (isRailGentle(below) && (below - RAIL_GENTLE) % 4 == 3);
            int hi, lo;
            if (topFull && railSides(below, hi, lo) && hi == nin && nextSide(below, nin, nout)) ok = true, next = below, --ny;
        }
        if (!ok) {
            p.t = 1;
            return false;
        }
        p.x = nx, p.y = ny, p.z = nz, p.in = nin, p.out = nout, p.t = 0;
        setSlope(p, next);
    }
    return true;
}

void TrainSystem::refresh(TrainCar& car) const {
    Vec3 tan;
    car.pos = pointOn(car.at, &tan);
    car.yaw = std::atan2(-tan.x, tan.z);  // the models' front is +z
}

void TrainSystem::pose(const World& world, TrainCar& car) const {
    // The ends of the car lie on the track 2 blocks ahead and behind the middle: the car tilts to
    // the line between them and sits at their average height, so it climbs a slope smoothly
    // instead of standing level and cutting into the hill.
    constexpr float kHalf = 2.0f;
    RailPos front = car.at, back = car.at;
    float moved = 0;
    advance(world, front, kHalf, moved);
    const float fm = moved;
    flip(back);
    advance(world, back, kHalf, moved);
    const float bm = moved;
    const Vec3 f = pointOn(front), b = pointOn(back);
    const float horiz = std::sqrt((f.x - b.x) * (f.x - b.x) + (f.z - b.z) * (f.z - b.z));
    if (fm + bm < 0.5f || horiz < 0.2f) {
        car.pitch = 0;
        return;
    }
    car.pitch = std::atan2(f.y - b.y, horiz);
    car.pos.y = (f.y * bm + b.y * fm) / (fm + bm);  // the height at the middle between both ends
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
        // 2.3.0: the subway's cars are longer: the gap between middles grows with them.
        const float pitch = kPitch + (carHalf(kind) - 2.25f + carHalf(last.kind) - 2.25f) * kScale;
        if (!advance(world, car.at, pitch, moved)) return false;
        flip(car.at);
        refresh(car);
        pose(world, car);
        tr.cars.push_back(car);
        return true;
    }
    // A new train, heading the way the player looks if the rail goes that way.
    int a, b;
    RailPos at;
    at.x = x, at.y = y, at.z = z, at.t = 0.5f;
    look *= 2;  // the side the player looks at, as a port
    if (railSides(here, a, b)) {
        // Out by the end closer to where the player looks.
        auto off = [&](int q) { const int d = (q - look) & 7; return std::min(d, 8 - d); };
        at.out = off(b) <= off(a) ? b : a;
        at.in = at.out == a ? b : a;
    } else if (isRailSwitch8(here)) {
        at.in = (railSwitch8Entry(here) + 4) & 7;
        at.out = railSwitch8Entry(here);
        auto off = [&](int q) { const int d = (q - look) & 7; return std::min(d, 8 - d); };
        if (off(at.in) < off(at.out)) flip(at);
    } else {
        const int k = railSwitchIndex(here), f = k / 2;
        at.in = ((f + 2) & 3) * 2;
        at.out = f * 2;
        if (look == at.in) flip(at);
    }
    setSlope(at, here);
    Train tr;
    tr.dim = dim;
    TrainCar car;
    car.kind = kind;
    car.at = at;
    refresh(car);
    pose(world, car);
    tr.cars.push_back(car);
    trains_.push_back(tr);
    return true;
}

void TrainSystem::update(const World& world, int dim, float dt, Vec3& playerPos, float& playerYaw, bool playerSeated) {
    for (Train& tr : trains_) {
        if (tr.dim != dim || tr.cars.empty()) continue;
        // Build 5: the locomotive may be anywhere in the train (behind a drill car that leads).
        bool hasEngine = false, subway = false;
        for (const TrainCar& c : tr.cars) hasEngine = hasEngine || isEngineCar(c.kind), subway = subway || c.kind == CarKind::SubwayLocomotive;
        // 2.3.0: the subway's doors open when it stands with no throttle, and shut before it leaves.
        if (subway) {
            const bool open = std::fabs(tr.speed) < 0.05f && std::fabs(tr.throttle) < 0.01f;
            tr.doors = open ? std::min(1.0f, tr.doors + dt * 1.25f) : std::max(0.0f, tr.doors - dt * 1.5f);
        } else {
            tr.doors = 0;
        }
        const float top = subway ? kSubwaySpeed : kMaxSpeed, accel = subway ? kSubwayAccel : kAccel;
        const float target = hasEngine && tr.doors <= 0.0f ? tr.throttle * top : 0;
        if (tr.brake) {
            const float s = std::max(0.0f, std::fabs(tr.speed) - kBrake * dt);
            tr.speed = tr.speed < 0 ? -s : s;
        } else if (tr.speed < target) {
            tr.speed = std::min(target, tr.speed + accel * dt);
        } else if (tr.speed > target && tr.boosted <= 0) {
            tr.speed = std::max(target, tr.speed - accel * dt);
        }
        // Build 3: a booster under the rail pushes a moving train on, up to half again its top speed.
        if (std::fabs(tr.speed) > 0.1f) {
            const TrainCar& c0 = tr.cars.front();
            if (world.getBlock(c0.at.x, c0.at.y - 1, c0.at.z) == TRAIN_BOOSTER) {
                const float top2 = top * 1.5f, s2 = std::min(top2, std::fabs(tr.speed) + 8.0f * dt);
                tr.speed = tr.speed < 0 ? -s2 : s2;
                tr.boosted = 3.0f;
            }
        }
        if (tr.boosted > 0) tr.boosted -= dt;
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
                if (std::fabs(l.x) < (isSubwayCar(c.kind) ? 0.95f : 0.75f) * kScale && std::fabs(l.z) < (carHalf(c.kind) - 0.2f) * kScale && l.y > 0.2f * kScale && l.y < 2.6f * kScale) {
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
        // Build 3: the car's nose must stay over the track, so look half a car further ahead: the
        // train stops with its end at the buffer stop instead of running past it into the air.
        const float half = carHalf(lead.kind) * kScale;
        RailPos probe = lead.at;
        float ahead;
        advance(world, probe, half + want, ahead);
        const float allowed = std::max(0.0f, std::min(want, ahead - half));
        float moved;
        advance(world, lead.at, allowed, moved);
        const bool ok = allowed >= want - 1e-4f;
        if (!fwd) flip(lead.at);
        for (TrainCar& c : tr.cars) {
            if (&c == &lead) continue;
            if (!fwd) flip(c.at);
            float m2;
            advance(world, c.at, moved, m2);
            if (!fwd) flip(c.at);
        }
        // Build 5: a drill car in front does not stop at the end of the track: it asks for the
        // next piece of tunnel (Session cuts it and lays a rail) and crawls on slowly meanwhile.
        const bool drilling = fwd && !ok && tr.cars.front().kind == CarKind::Drill && !isBufferStop(world.getBlock(probe.x, probe.y, probe.z)) &&
                              (tr.rails > 0 || infiniteRails);  // 2.3.0: it lays the rails it carries
        if (drilling) {
            tr.speed = std::min(tr.speed, 2.0f);
            if ((tr.drillTimer -= dt) <= 0) {
                tr.drillTimer = 0.35f;
                drillRequests_.push_back({probe.x, probe.y, probe.z, probe.out, (int)(&tr - trains_.data())});
            }
        } else if (!ok) {
            tr.speed = 0, tr.throttle = 0;  // the end of the track: the train stops
        }
        for (TrainCar& c : tr.cars) refresh(c), pose(world, c);
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
            const int n = carBoxes(c.kind, boxes, tr.doors);
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
            const float hw = isSubwayCar(c.kind) ? 0.95f : 0.75f, hl = carHalf(c.kind) - 0.15f;
            const float lo[3] = {-hw * kScale, 0.2f * kScale, -hl * kScale}, hi[3] = {hw * kScale, 2.4f * kScale, hl * kScale};
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
    switch (kind) {
        case CarKind::Locomotive: return kTrainSeatCount_locomotive;
        case CarKind::Passenger: return kTrainSeatCount_passenger;
        case CarKind::SubwayLocomotive: return kTrainSeatCount_subway_locomotive;
        case CarKind::SubwayCar: return kTrainSeatCount_subway_car;
        default: return 0;
    }
}

Vec3 TrainSystem::seatPos(const TrainCar& car, int seat) const {
    const float(*seats)[3] = car.kind == CarKind::Locomotive ? kTrainSeats_locomotive : car.kind == CarKind::SubwayLocomotive ? kTrainSeats_subway_locomotive
                             : car.kind == CarKind::SubwayCar ? kTrainSeats_subway_car : kTrainSeats_passenger;
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

namespace {
struct Mesh {
    const int16_t* data;
    size_t count;
};
#define VC_MESH(arr) Mesh{arr, sizeof(arr) / sizeof(int16_t) / 7}
Mesh carMesh(CarKind k) {
    switch (k) {
        case CarKind::Locomotive: return VC_MESH(kTrain_locomotive);
        case CarKind::Passenger: return VC_MESH(kTrain_passenger);
        case CarKind::Drill: return VC_MESH(kTrain_drill);
        case CarKind::SubwayLocomotive: return VC_MESH(kTrain_subway_locomotive);
        case CarKind::SubwayCar: return VC_MESH(kTrain_subway_car);
        default: return VC_MESH(kTrain_freight);
    }
}
}  // namespace

void TrainSystem::buildVertices(int dim, std::vector<Vertex>& out) const {
    for (const Train& tr : trains_) {
        if (tr.dim != dim) continue;
        for (const TrainCar& c : tr.cars) {
            const float cs = std::cos(c.yaw), sn = std::sin(c.yaw), k = kScale / 2048.0f;
            const float pc = std::cos(c.pitch), ps = std::sin(c.pitch);
            auto emit = [&](const Mesh& m, float dz) {
                for (size_t i = 0; i < m.count; ++i) {
                    const int16_t* v = m.data + i * 7;
                    const float lx = v[0] * k, ly0 = v[1] * k, lz0 = v[2] * k + dz;
                    const float ly = ly0 * pc + lz0 * ps, lz = lz0 * pc - ly0 * ps;  // build 5: tilted on slopes
                    // Build 4: each colour has its own pixel-art tile; u, v are inside it (half a pixel in from the edges).
                    const int tile = trainTexTile(v[3]);
                    const float u = ((tile % kAtlasCols) + (0.5f + v[5] / 255.0f * 15.0f) / 16.0f) * kTile;
                    const float vv = ((tile / kAtlasCols) + (0.5f + v[6] / 255.0f * 15.0f) / 16.0f) * kTile;
                    out.push_back({c.pos.x + lx * cs - lz * sn, c.pos.y + ly, c.pos.z + lx * sn + lz * cs, u, vv, v[4] / 255.0f});
                }
            };
            emit(carMesh(c.kind), 0);
            // 2.3.0: the subway's door leaves slide apart along the side as the doors open.
            const float slide = 0.38f * kScale * tr.doors;
            if (c.kind == CarKind::SubwayLocomotive) emit(VC_MESH(kTrainDoorsNeg_subway_locomotive), -slide), emit(VC_MESH(kTrainDoorsPos_subway_locomotive), slide);
            if (c.kind == CarKind::SubwayCar) emit(VC_MESH(kTrainDoorsNeg_subway_car), -slide), emit(VC_MESH(kTrainDoorsPos_subway_car), slide);
        }
    }
}

bool TrainSystem::save(const std::string& folder) const {
    std::ostringstream o;
    for (const Train& tr : trains_) {
        o << "train " << tr.dim << ' ' << tr.cars.size() << ' ' << tr.rails << '\n';
        for (const TrainCar& c : tr.cars)
            o << "car8 " << (int)c.kind << ' ' << c.at.x << ' ' << c.at.y << ' ' << c.at.z << ' ' << c.at.in << ' ' << c.at.out << ' ' << c.at.t << ' ' << c.at.slopeHigh << ' ' << c.at.slopeLo << ' ' << c.at.slopeSpan << '\n';
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
            if (in.peek() == ' ') in >> tr.rails;  // 2.3.0
            for (size_t i = 0; i < n && in >> word && (word == "car" || word == "car8"); ++i) {
                const bool ports = word == "car8";  // build 12: eight ports; "car" lines count four sides
                TrainCar c;
                int kind = 0;
                in >> kind >> c.at.x >> c.at.y >> c.at.z >> c.at.in >> c.at.out >> c.at.t;
                // 2.1.0: the slope is saved after the rest (older files end the line here).
                if (in.peek() == ' ') in >> c.at.slopeHigh;
                if (ports && in.peek() == ' ') in >> c.at.slopeLo >> c.at.slopeSpan;
                c.kind = (CarKind)std::max(0, std::min(kind, 5));
                if (!ports) c.at.in = (c.at.in & 3) * 2, c.at.out = (c.at.out & 3) * 2, c.at.slopeHigh = c.at.slopeHigh >= 0 ? c.at.slopeHigh * 2 : -1;
                c.at.in &= 7, c.at.out &= 7;
                refresh(c);
                tr.cars.push_back(c);
            }
            if (!tr.cars.empty()) trains_.push_back(tr);
        }
    }
}

}  // namespace vc

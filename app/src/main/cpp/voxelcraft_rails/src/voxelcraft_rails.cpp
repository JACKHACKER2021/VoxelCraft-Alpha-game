#include "voxelcraft_rails.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace vcrails {

static constexpr float kPi = 3.14159265358979f;

// ======================================================================= headings
Heading opposite(Heading h) { return Heading((int(h) + 2) & 3); }

void headingStep(Heading h, int& dx, int& dz) {
    static const int DX[4] = {0, 1, 0, -1}, DZ[4] = {-1, 0, 1, 0};
    dx = DX[int(h)]; dz = DZ[int(h)];
}

// local heading (rot 0) -> world heading for a block rotated by 'rot'
static Heading rotHeading(Heading local, int rot) { return Heading((int(local) - rot + 4) & 3); }

// ======================================================================= rail geometry
RailPorts railPorts(const RailBlock& b) {
    RailPorts p{};
    switch (b.shape) {
        case RailShape::Straight:   p = {{Heading::North, 0}, {Heading::South, 0}, false}; break;
        case RailShape::CurveRight: p = {{Heading::North, 0}, {Heading::East,  0}, false}; break;
        case RailShape::CurveLeft:  p = {{Heading::North, 0}, {Heading::West,  0}, false}; break;
        case RailShape::Stop:       p = {{Heading::North, 0}, {Heading::South, 0}, true};  break;
        case RailShape::SlopeUp:    p = {{Heading::North, 0}, {Heading::South, 1}, false}; break;
        case RailShape::SlopeDown:  p = {{Heading::North, 1}, {Heading::South, 0}, false}; break;
        case RailShape::SwitchRight:
            p = {{Heading::North, 0}, {Heading::South, 0}, false, {Heading::East, 0}, true}; break;
        case RailShape::SwitchLeft:
            p = {{Heading::North, 0}, {Heading::South, 0}, false, {Heading::West, 0}, true}; break;
    }
    if (p.hasAlt) p.alt.edge = rotHeading(p.alt.edge, b.rot & 3);
    p.in.edge  = rotHeading(p.in.edge,  b.rot & 3);
    p.out.edge = rotHeading(p.out.edge, b.rot & 3);
    return p;
}

float railLength(RailShape s) {
    switch (s) {
        case RailShape::CurveLeft:
        case RailShape::CurveRight: return kPi / 4.0f;      // quarter circle r=0.5
        case RailShape::SlopeUp:
        case RailShape::SlopeDown:  return std::sqrt(2.0f);
        default:                    return 1.0f;
    }
}

RailShape effectiveShape(const RailBlock& b, bool branch) {
    if (b.shape == RailShape::SwitchRight) return branch ? RailShape::CurveRight : RailShape::Straight;
    if (b.shape == RailShape::SwitchLeft)  return branch ? RailShape::CurveLeft  : RailShape::Straight;
    return b.shape;
}

float railMaxT(RailShape s) { return s == RailShape::Stop ? 0.80f : 1.0f; }

static void sampleLocal(RailShape s, float t, Vec3& p, Vec3& d) {
    const float inv = 0.70710678f;
    switch (s) {
        case RailShape::Straight:
        case RailShape::Stop:
        case RailShape::SwitchRight:   // switches are sampled via effectiveShape()
        case RailShape::SwitchLeft:
            p = {0, kRailTop, -0.5f + t}; d = {0, 0, 1}; break;
        case RailShape::CurveRight: {
            float th = kPi - t * kPi / 2;
            p = {0.5f + 0.5f * std::cos(th), kRailTop, -0.5f + 0.5f * std::sin(th)};
            d = {std::sin(th), 0, -std::cos(th)}; break;
        }
        case RailShape::CurveLeft: {
            float th = t * kPi / 2;
            p = {-0.5f + 0.5f * std::cos(th), kRailTop, -0.5f + 0.5f * std::sin(th)};
            d = {-std::sin(th), 0, std::cos(th)}; break;
        }
        case RailShape::SlopeUp: {
            const float off = kRailTop - kRailBed;             // rail head above bed, along the normal
            p = {0, kRailBed + t + off * inv, -0.5f + t - off * inv};
            d = {0, inv, inv}; break;
        }
        case RailShape::SlopeDown: {                            // SlopeUp rotated 180 deg about Y
            Vec3 q, qd; sampleLocal(RailShape::SlopeUp, 1.0f - t, q, qd);
            p = {-q.x, q.y, -q.z};
            d = {qd.x, -qd.y, qd.z};                            // travelling downhill towards +Z
            break;
        }
    }
}

void sampleRail(const RailBlock& b, float t, Vec3& pos, Vec3& dir, bool branch) {
    Vec3 p, d; sampleLocal(effectiveShape(b, branch), t, p, d);
    int r = b.rot & 3;
    static const int C[4] = {1, 0, -1, 0}, S[4] = {0, 1, 0, -1};
    int c = C[r], s = S[r];
    pos = {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
    dir = {d.x * c + d.z * s, d.y, -d.x * s + d.z * c};
}

// ======================================================================= recipes
const std::vector<RecipeSpec>& recipeTable() {
    static const char* K  = "S=voxelcraft:steel_ingot;P=voxelcraft:spruce_planks";
    static const char* KR = "R=vcrails:rail_straight;S=voxelcraft:steel_ingot;P=voxelcraft:spruce_planks";
    static const char* KW = "S=voxelcraft:steel_ingot;P=voxelcraft:spruce_planks;W=vcrails:train_wheel";
    static const std::vector<RecipeSpec> t = {
        {"vcrails:rail_straight",    "vcrails:rail_straight",    16, {"S.S", "SPS", "S.S"}, K},
        {"vcrails:rail_curve_right", "vcrails:rail_curve_right",  2, {"RR.", ".PR", nullptr}, KR},
        {"vcrails:rail_curve_left",  "vcrails:rail_curve_left",   2, {".RR", "RP.", nullptr}, KR},
        {"vcrails:rail_switch_right","vcrails:rail_switch_right", 1, {"RR.", "RSP", nullptr}, KR},
        {"vcrails:rail_switch_left", "vcrails:rail_switch_left",  1, {".RR", "PSR", nullptr}, KR},
        {"vcrails:rail_stop",        "vcrails:rail_stop",         2, {"PPP", "SRS", nullptr}, KR},
        {"vcrails:rail_slope_up",    "vcrails:rail_slope_up",     2, {"..R", ".RS", "PPP"}, KR},
        {"vcrails:rail_slope_down",  "vcrails:rail_slope_down",   2, {"R..", "SR.", "PPP"}, KR},
        {"vcrails:train_wheel",      "vcrails:train_wheel",       4, {".S.", "SPS", ".S."}, K},
        {"vcrails:train_locomotive", "vcrails:train_locomotive",  1, {"SSS", "SPS", "WSW"}, KW},
        {"vcrails:train_passenger",  "vcrails:train_passenger",   1, {"PSP", "P.P", "WSW"}, KW},
        {"vcrails:train_freight",    "vcrails:train_freight",     1, {"P.P", "PPP", "WSW"}, KW},
    };
    return t;
}

// ======================================================================= seats
const std::vector<SeatSlot>& seatsFor(CarType t) {
    static const std::vector<SeatSlot> loco = {{{0.0f, 0.82f, -1.2f}}};
    static const std::vector<SeatSlot> pass = [] {
        std::vector<SeatSlot> v;
        for (int side = -1; side <= 1; side += 2)
            for (float z : {-0.725f, -0.275f, 0.275f, 0.725f}) v.push_back({{side * 0.5f, 0.86f, z}});
        return v;
    }();
    static const std::vector<SeatSlot> none;
    return t == CarType::Locomotive ? loco : (t == CarType::Passenger ? pass : none);
}

// ======================================================================= Train
static Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float len(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

Train::Train(std::vector<CarType> consist) {
    for (CarType t : consist) {
        Car c; c.type = t; c.passengers.assign(seatsFor(t).size(), 0);
        cars_.push_back(std::move(c));
    }
}

static Vec3 blockCentre(int x, int y, int z) { return {x + 0.5f, float(y), z + 0.5f}; }

bool Train::place(const IRailWorld& w, int x, int y, int z, bool reversed) {
    if (!w.getRail(x, y, z, blk_)) return false;
    cx_ = x; cy_ = y; cz_ = z; rev_ = reversed; branch_ = blk_.diverge;
    t_ = reversed ? railMaxT(blk_.shape) : 0.0f;
    speed_ = 0; atBuffer_ = false; trail_.clear();
    Vec3 p, d; sampleRail(blk_, t_, p, d, branch_);
    if (reversed) d = d * -1.0f;
    Vec3 head = blockCentre(cx_, cy_, cz_) + p;
    // synthetic straight run-in behind the head so cars have somewhere to stand
    float total = (cars_.size() - 1) * kCarPitch + 4.0f;
    for (float s = 0; s <= total; s += 0.05f) trail_.push_back(head - d * s);
    layoutCars();
    return true;
}

bool Train::findNext(const IRailWorld& w, bool& ok) {
    RailPorts p = railPorts(blk_);
    RailPort exit = rev_ ? p.in : ((branch_ && p.hasAlt) ? p.alt : p.out);
    int dx, dz; headingStep(exit.edge, dx, dz);
    int level = cy_ + exit.height;
    for (int dy : {0, 1, -1}) {
        int nx = cx_ + dx, ny = cy_ + dy, nz = cz_ + dz;
        RailBlock nb;
        if (!w.getRail(nx, ny, nz, nb)) continue;
        RailPorts np = railPorts(nb);
        Heading need = opposite(exit.edge);
        if (np.in.edge == need && ny + np.in.height == level) {
            cx_ = nx; cy_ = ny; cz_ = nz; blk_ = nb; rev_ = false; branch_ = nb.diverge; t_ = 0; ok = true; return true;
        }
        if (!np.deadEnd && np.out.edge == need && ny + np.out.height == level) {
            cx_ = nx; cy_ = ny; cz_ = nz; blk_ = nb; rev_ = true; branch_ = false; t_ = railMaxT(nb.shape); ok = true; return true;
        }
        if (np.hasAlt && np.alt.edge == need && ny + np.alt.height == level) {
            cx_ = nx; cy_ = ny; cz_ = nz; blk_ = nb; rev_ = true; branch_ = true; t_ = 1.0f; ok = true; return true;
        }
    }
    ok = false; return false;
}

bool Train::advance(float dist, const IRailWorld& w) {
    while (dist > 1e-6f) {
        float L = railLength(effectiveShape(blk_, branch_)), maxT = railMaxT(blk_.shape);
        float remaining = rev_ ? t_ * L : (maxT - t_) * L;
        if (dist < remaining) { t_ += (rev_ ? -1.0f : 1.0f) * dist / L; dist = 0; break; }
        dist -= remaining;
        t_ = rev_ ? 0.0f : maxT;
        if (blk_.shape == RailShape::Stop && !rev_) { atBuffer_ = true; speed_ = 0; return false; }
        bool ok = false;
        if (!findNext(w, ok)) { atBuffer_ = true; speed_ = 0; return false; }
    }
    return true;
}

void Train::recordHead() {
    Vec3 p, d; sampleRail(blk_, t_, p, d, branch_);
    Vec3 head = blockCentre(cx_, cy_, cz_) + p;
    if (trail_.empty() || len(head - trail_.front()) >= 0.02f) trail_.push_front(head);
    size_t maxPts = size_t(((cars_.size() - 1) * kCarPitch + 4.0f) / 0.02f) + 64;
    while (trail_.size() > maxPts) trail_.pop_back();
}

static Vec3 trailAt(const std::deque<Vec3>& tr, float d) {
    if (tr.size() < 2) return tr.empty() ? Vec3{} : tr.front();
    if (d <= 0) {                                       // ahead of the head: extrapolate
        Vec3 dir = tr[0] - tr[1]; float l = len(dir);
        return l > 1e-6f ? tr[0] + dir * (-d / l) : tr[0];
    }
    float acc = 0;
    for (size_t i = 0; i + 1 < tr.size(); ++i) {
        float seg = len(tr[i] - tr[i + 1]);
        if (acc + seg >= d) return tr[i] + (tr[i + 1] - tr[i]) * ((d - acc) / (seg > 1e-6f ? seg : 1.0f));
        acc += seg;
    }
    return tr.back();
}

void Train::layoutCars() {
    for (size_t i = 0; i < cars_.size(); ++i) {
        float centre = i * kCarPitch;
        Vec3 front = trailAt(trail_, centre - 1.3f), rear = trailAt(trail_, centre + 1.3f);
        Car& c = cars_[i];
        c.pos = (front + rear) * 0.5f;
        Vec3 d = front - rear; float h = std::sqrt(d.x * d.x + d.z * d.z);
        if (h > 1e-5f) c.yaw = std::atan2(d.x, d.z);
        c.pitch = std::atan2(d.y, h > 1e-5f ? h : 1.0f);
    }
}

void Train::update(float dt, const IRailWorld& w) {
    float target = brake_ ? 0.0f : throttle_ * kMaxSpeed;
    float step = (brake_ ? 2.5f * kAccel : kAccel) * dt;
    if (speed_ < target) speed_ = std::min(target, speed_ + step);
    else                 speed_ = std::max(target, speed_ - step);
    if (speed_ < 1e-4f) { speed_ = 0; layoutCars(); return; }
    atBuffer_ = false;
    float dist = speed_ * dt;
    while (dist > 1e-6f) {                               // small sub-steps keep curves smooth
        float s = std::min(dist, 0.05f);
        bool ok = advance(s, w);
        recordHead();
        dist -= s;
        for (Car& c : cars_) c.wheelAngle += s / kWheelRadius;
        if (!ok) break;
    }
    layoutCars();
}

int Train::board(uint32_t id, CarType preferred) {
    if (id == 0) return -1;
    for (int pass = 0; pass < 2; ++pass)
        for (size_t i = 0; i < cars_.size(); ++i) {
            if (pass == 0 && cars_[i].type != preferred) continue;
            for (auto& slot : cars_[i].passengers)
                if (slot == 0) { slot = id; return int(i); }
        }
    return -1;
}

bool Train::alight(uint32_t id, Vec3* exitPos) {
    for (Car& c : cars_)
        for (auto& slot : c.passengers)
            if (slot == id) {
                slot = 0;
                if (exitPos) {
                    float s = std::sin(c.yaw), co = std::cos(c.yaw);
                    *exitPos = c.pos + Vec3{1.2f * co, 0.0f, -1.2f * s};      // right side of the car
                }
                return true;
            }
    return false;
}

bool Train::seatWorldPos(uint32_t id, Vec3& out) const {
    for (const Car& c : cars_) {
        const auto& seats = seatsFor(c.type);
        for (size_t i = 0; i < c.passengers.size(); ++i)
            if (c.passengers[i] == id) {
                Vec3 l = seats[i].local; float s = std::sin(c.yaw), co = std::cos(c.yaw);
                out = c.pos + Vec3{l.x * co + l.z * s, l.y, -l.x * s + l.z * co};
                return true;
            }
    }
    return false;
}

// ======================================================================= mod registration
static const VcHostApi* g_host = nullptr;

static void logf(int lvl, const char* msg) { if (g_host && g_host->log) g_host->log(lvl, msg); else std::fprintf(stderr, "[vcrails] %s\n", msg); }

static int registerAll(const VcHostApi* h) {
    int errors = 0;
    auto chk = [&](int r, const char* what) { if (r != 0) { logf(2, what); ++errors; } };

    struct B { const char* id; const char* name; const char* model; };
    const B blocks[] = {
        {"vcrails:rail_straight",    "Rail",                 "models/rail_straight.glb"},
        {"vcrails:rail_curve_left",  "Rail Curve (Left)",    "models/rail_curve_left.glb"},
        {"vcrails:rail_curve_right", "Rail Curve (Right)",   "models/rail_curve_right.glb"},
        {"vcrails:rail_switch_right","Rail Switch (Right)",   "models/rail_switch_right.glb"},
        {"vcrails:rail_switch_left", "Rail Switch (Left)",    "models/rail_switch_left.glb"},
        {"vcrails:rail_stop",        "Rail Buffer Stop",     "models/rail_stop.glb"},
        {"vcrails:rail_slope_up",    "Rail Slope Up",        "models/rail_slope_up.glb"},
        {"vcrails:rail_slope_down",  "Rail Slope Down",      "models/rail_slope_down.glb"},
    };
    for (const B& b : blocks) {
        bool sw = std::string(b.id).find("switch") != std::string::npos;
        VcBlockDef d{b.id, b.name, b.model, uint32_t(VC_BLOCK_NON_SOLID | VC_BLOCK_ROTATABLE | VC_BLOCK_ATTACH_RAIL | (sw ? VC_BLOCK_USE_TOGGLE : 0)), 0.7f};
        chk(h->register_block(&d), b.id);
    }

    const VcItemDef items[] = {
        {"vcrails:train_wheel",     "Train Wheel",        64},
        {"vcrails:train_locomotive","Locomotive",          1},
        {"vcrails:train_passenger", "Passenger Wagon",     1},
        {"vcrails:train_freight",   "Freight Wagon",       1},
    };
    for (const VcItemDef& i : items) chk(h->register_item(&i), i.id);

    const VcEntityDef ents[] = {
        {"vcrails:train_locomotive", "models/train_locomotive.glb",       kCarWidth, 2.45f, kCarLength, 1, "wheel_"},
        {"vcrails:train_passenger",  "models/train_passenger_wagon.glb",  kCarWidth, 2.45f, kCarLength, 8, "wheel_"},
        {"vcrails:train_freight",    "models/train_freight_wagon.glb",    kCarWidth, 1.40f, kCarLength, 0, "wheel_"},
    };
    for (const VcEntityDef& e : ents) chk(h->register_entity(&e), e.id);

    for (const RecipeSpec& r : recipeTable()) {
        VcRecipeDef d{r.id, r.out, r.count, {r.rows[0], r.rows[1], r.rows[2]}, r.keys};
        chk(h->register_recipe(&d), r.id);
    }
    return errors;
}

} // namespace vcrails

// ======================================================================= exported C entry points
VC_EXPORT int vc_mod_init(const VcHostApi* host) {
    if (!host || host->abi_version != VC_MOD_ABI_VERSION) return -1;
    vcrails::g_host = host;
    int errs = vcrails::registerAll(host);
    vcrails::logf(0, errs ? "Rails & Trains loaded with errors" : "Rails & Trains loaded");
    return errs;
}
VC_EXPORT void        vc_mod_shutdown(void)     { vcrails::g_host = nullptr; }
VC_EXPORT const char* vc_mod_name(void)         { return "vcrails"; }
VC_EXPORT uint32_t    vc_mod_abi_version(void)  { return VC_MOD_ABI_VERSION; }
VC_EXPORT int vc_rails_toggle_switch(int shape, int* diverge) {
    if (!diverge) return 0;
    vcrails::RailBlock b; b.shape = vcrails::RailShape(shape); b.diverge = *diverge != 0;
    if (!vcrails::toggleSwitch(b)) return 0;
    *diverge = b.diverge ? 1 : 0; return 1;
}

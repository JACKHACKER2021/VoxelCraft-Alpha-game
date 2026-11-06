#include "Trains.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "Blocks.h"
#include "Json.h"

namespace vc {

namespace {

constexpr float kTile = kTileUv;
const int kDx[4] = {0, 1, 0, -1};  // north, east, south, west
const int kDz[4] = {-1, 0, 1, 0};

inline int opp(int d) { return (d + 2) & 3; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float yawOf(float dx, float dz) { return std::atan2(dx, -dz); }

constexpr float kCurveLength = 0.78f;
inline float pieceLength(PieceKind k) { return k == PieceKind::Curve ? kCurveLength : 1.0f; }

int tileOfBlock(ItemId id) { return tileFor(id, F_NORTH); }

// One oriented box in world space: `origin` + local axes turned by `yaw` (forward is -z).
void emitBox(std::vector<Vertex>& out, const Vec3& origin, float yaw, float x0, float y0, float z0, float x1, float y1, float z1,
             int tile, float light) {
    const float cs = std::cos(yaw), sn = std::sin(yaw);
    const float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile;
    auto P = [&](float x, float y, float z) { return Vec3(origin.x + x * cs - z * sn, origin.y + y, origin.z + x * sn + z * cs); };
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    static const int faces[6][4][3] = {
        {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
        {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
        {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}};
    static const float shade[6] = {1.0f, 0.5f, 0.8f, 0.8f, 0.6f, 0.6f};
    static const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    for (int f = 0; f < 6; ++f) {
        Vertex q[4];
        for (int k = 0; k < 4; ++k) {
            Vec3 p = P(faces[f][k][0] ? hi[0] : lo[0], faces[f][k][1] ? hi[1] : lo[1], faces[f][k][2] ? hi[2] : lo[2]);
            q[k] = {p.x, p.y, p.z, u0 + uv[k][0] * kTile, v0 + uv[k][1] * kTile, shade[f] * light};
        }
        out.insert(out.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Track geometry

void TrainSystem::ports(PieceKind kind, int dir, bool out[4]) {
    out[0] = out[1] = out[2] = out[3] = false;
    dir &= 3;
    switch (kind) {
        case PieceKind::Straight: out[dir] = out[opp(dir)] = true; break;
        case PieceKind::Curve: out[dir] = out[(dir + 1) & 3] = true; break;
        case PieceKind::Cross: out[0] = out[1] = out[2] = out[3] = true; break;
        case PieceKind::Stop: out[dir] = true; break;
    }
}

namespace {
// The port a train leaves through after entering through `entry` (-1 = it cannot leave).
int exitPort(const TrackPiece& p, int entry) {
    switch (p.kind) {
        case PieceKind::Straight:
        case PieceKind::Cross: return opp(entry);
        case PieceKind::Curve: {
            int a = p.dir & 3, b = (p.dir + 1) & 3;
            return entry == a ? b : entry == b ? a : -1;
        }
        case PieceKind::Stop: return -1;
    }
    return -1;
}
bool hasPort(const TrackPiece& p, int port) {
    bool o[4];
    TrainSystem::ports(p.kind, p.dir, o);
    return o[port & 3];
}
}  // namespace

TrainSystem::TrainSystem() { resetLook(); }

std::vector<std::string>& TrainSystem::packConfigs() {
    static std::vector<std::string> configs;
    return configs;
}

const TrackPiece* TrainSystem::piece(int dim, int x, int y, int z) const {
    auto it = pieces_.find({dim, x, y, z});
    return it == pieces_.end() ? nullptr : &it->second;
}

const TrackPiece* TrainSystem::pieceFor(int dim, const TrackState& s) const {
    const TrackPiece* p = piece(dim, s.x, s.y, s.z);
    return p && p->kind != PieceKind::Stop && hasPort(*p, s.entry) ? p : nullptr;
}

bool TrainSystem::placePiece(const World& world, int dim, int x, int y, int z, PieceKind kind, int dir, std::string* why) {
    auto fail = [&](const char* m) {
        if (why) *why = m;
        return false;
    };
    if (isSolid(world.getBlock(x, y, z))) return fail("Místo je obsazené");
    if (!isSolid(world.getBlock(x, y - 1, z))) return fail("Pod kolejí musí být pevný blok");
    TrackPiece p;
    p.dim = dim, p.x = x, p.y = y, p.z = z, p.kind = kind, p.dir = dir & 3;
    pieces_[{dim, x, y, z}] = p;
    return true;
}

bool TrainSystem::removePiece(int dim, int x, int y, int z) { return pieces_.erase({dim, x, y, z}) > 0; }

Vec3 TrainSystem::worldPos(int dim, const TrackState& s, float* yaw) const {
    Vec3 c(s.x + 0.5f, s.y + kRailHeight, s.z + 0.5f);
    const TrackPiece* p = pieceFor(dim, s);
    if (!p) {
        if (yaw) *yaw = 0;
        return c;
    }
    const int out = exitPort(*p, s.entry);
    Vec3 a(c.x + kDx[s.entry] * 0.5f, c.y, c.z + kDz[s.entry] * 0.5f);
    Vec3 b(c.x + kDx[out] * 0.5f, c.y, c.z + kDz[out] * 0.5f);
    float t = std::clamp(s.t, 0.0f, 1.0f);
    if (p->kind == PieceKind::Curve) {
        // Quadratic curve from edge to edge with the cell centre as control point.
        float u = 1 - t;
        Vec3 r(u * u * a.x + 2 * u * t * c.x + t * t * b.x, c.y, u * u * a.z + 2 * u * t * c.z + t * t * b.z);
        float dx = 2 * u * (c.x - a.x) + 2 * t * (b.x - c.x), dz = 2 * u * (c.z - a.z) + 2 * t * (b.z - c.z);
        if (yaw) *yaw = yawOf(dx, dz);
        return r;
    }
    if (yaw) *yaw = yawOf(b.x - a.x, b.z - a.z);
    return Vec3(lerpf(a.x, b.x, t), c.y, lerpf(a.z, b.z, t));
}

// Moves `s` forward by `dist` along the track. `moved` is how far it really got.
bool TrainSystem::step(int dim, TrackState& s, float dist, float& moved) const {
    moved = 0;
    for (int guard = 0; guard < 64 && dist > 1e-5f; ++guard) {
        const TrackPiece* p = pieceFor(dim, s);
        if (!p) return false;
        const float len = pieceLength(p->kind);
        const float remaining = (1 - s.t) * len;
        if (dist < remaining) {
            s.t += dist / len;
            moved += dist;
            return true;
        }
        dist -= remaining;
        moved += remaining;
        s.t = 1;
        const int out = exitPort(*p, s.entry);
        if (out < 0) return false;
        const int nx = s.x + kDx[out], nz = s.z + kDz[out], in = opp(out);
        const TrackPiece* n = piece(dim, nx, s.y, nz);
        if (!n || n->kind == PieceKind::Stop || !hasPort(*n, in)) return false;  // end of the line or a buffer
        s.x = nx, s.z = nz, s.entry = in, s.t = 0;
    }
    return true;
}

bool TrainSystem::advance(Train& t, TrackState& s, float dist, float* moved) const {
    float m = 0;
    bool ok = step(t.dim, s, dist, m);
    if (moved) *moved = m;
    return ok;
}

// The same place seen from the other direction (entering through the exit port).
TrackState TrainSystem::flipped(int dim, const TrackState& s) const {
    TrackState r = s;
    const TrackPiece* p = pieceFor(dim, s);
    if (p) {
        int out = exitPort(*p, s.entry);
        if (out >= 0) r.entry = out;
    }
    r.t = 1 - s.t;
    return r;
}

// ---------------------------------------------------------------------------------------------
// Trains

int TrainSystem::spawnTrain(int dim, int x, int y, int z, int dir, const std::vector<CarKind>& kinds, std::string* why) {
    auto fail = [&](const char* m) {
        if (why) *why = m;
        return -1;
    };
    if (kinds.empty()) return fail("Chybí vozy");
    TrackState head;
    head.x = x, head.y = y, head.z = z, head.entry = opp(dir & 3), head.t = 0.5f;
    if (!pieceFor(dim, head)) {
        // The piece may be a curve or have another axis: take whichever port suits it best.
        const TrackPiece* p = piece(dim, x, y, z);
        if (!p || p->kind == PieceKind::Stop) return fail("Tady není kolej");
        bool ok = false;
        for (int e = 0; e < 4 && !ok; ++e)
            if (hasPort(*p, e) && exitPort(*p, e) >= 0) head.entry = e, ok = true;
        if (!ok) return fail("Tady není kolej");
    }
    Train t;
    t.dim = dim;
    TrackState s = head;
    for (size_t i = 0; i < kinds.size(); ++i) {
        Car c;
        c.kind = kinds[i];
        c.st = s;
        t.cars.push_back(c);
        // The next car stands one spacing further back.
        TrackState back = flipped(dim, s);
        float moved = 0;
        bool ok = step(dim, back, kCarSpacing, moved);
        s = flipped(dim, back);
        if (!ok || moved < kCarSpacing - 1e-3f) {
            if (i + 1 < kinds.size() && why) *why = "Kolej je krátká, vlak má méně vozů";
            break;
        }
    }
    refreshCars(t);
    trains_.push_back(t);
    return (int)trains_.size() - 1;
}

void TrainSystem::removeTrain(int index) {
    if (index >= 0 && index < (int)trains_.size()) trains_.erase(trains_.begin() + index);
}

int TrainSystem::nearestTrain(int dim, const Vec3& p, float maxDist) const {
    int best = -1;
    float bd = maxDist;
    for (int i = 0; i < (int)trains_.size(); ++i) {
        if (trains_[i].dim != dim) continue;
        for (const Car& c : trains_[i].cars) {
            float d = (c.pos - p).length();
            if (d < bd) bd = d, best = i;
        }
    }
    return best;
}

void TrainSystem::setTarget(int index, float speed) {
    if (index < 0 || index >= (int)trains_.size()) return;
    Train& t = trains_[index];
    t.target = std::clamp(speed, 0.0f, kMaxSpeed);
    if (t.target > 0) t.blocked = false;
}

void TrainSystem::reverse(int index) {
    if (index < 0 || index >= (int)trains_.size()) return;
    Train& t = trains_[index];
    for (Car& c : t.cars) c.st = flipped(t.dim, c.st);
    t.reversed = !t.reversed;
    t.speed = 0;
    t.blocked = false;
    refreshCars(t);
}

Vec3 TrainSystem::frontPosition(int index) const {
    if (index < 0 || index >= (int)trains_.size() || trains_[index].cars.empty()) return {};
    const Train& t = trains_[index];
    return (t.reversed ? t.cars.back() : t.cars.front()).pos;
}

void TrainSystem::refreshCars(Train& t) const {
    for (Car& c : t.cars) {
        float yaw = 0;
        c.pos = worldPos(t.dim, c.st, &yaw);
        c.yaw = t.reversed ? yaw + kPi : yaw;
    }
}

void TrainSystem::update(int dim, float dt, const std::function<bool(int, int, int)>& supported) {
    time_ += dt;
    // Once a second: pieces whose ground was dug away disappear.
    static float sinceCheck = 0;
    sinceCheck += dt;
    if (sinceCheck > 1.0f && supported) {
        sinceCheck = 0;
        for (auto it = pieces_.begin(); it != pieces_.end();) {
            if (it->first.dim == dim && !supported(it->first.x, it->first.y, it->first.z))
                it = pieces_.erase(it);
            else
                ++it;
        }
    }
    for (Train& t : trains_) {
        if (t.dim != dim || t.cars.empty()) continue;
        // Speed towards the wanted speed; a train in front of a buffer stands still.
        if (t.speed < t.target) t.speed = std::min(t.target, t.speed + kAccel * dt);
        else if (t.speed > t.target) t.speed = std::max(t.target, t.speed - kAccel * 1.5f * dt);
        if (t.speed > 1e-4f) {
            const float dist = t.speed * dt;
            const int lead = t.reversed ? (int)t.cars.size() - 1 : 0;
            float moved = 0;
            bool ok = step(t.dim, t.cars[lead].st, dist, moved);
            for (int i = 0; i < (int)t.cars.size(); ++i) {
                if (i == lead) continue;
                float m2 = 0;
                step(t.dim, t.cars[i].st, moved, m2);
            }
            if (!ok) {
                t.speed = 0;
                t.target = 0;
                t.blocked = true;
            }
        }
        refreshCars(t);
    }
}

void TrainSystem::update(const World& world, int dim, float dt) {
    update(dim, dt, [&](int x, int y, int z) {
        // Pieces in chunks that are not loaded stay as they are.
        if (!world.getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize))) return true;
        return isSolid(world.getBlock(x, y - 1, z));
    });
}

// ---------------------------------------------------------------------------------------------
// Look

void TrainSystem::setMaterial(const std::string& name, int tile) { materials_[materialIndex(name)] = tile; }

int TrainSystem::materialIndex(const std::string& name) {
    for (size_t i = 0; i < materialNames_.size(); ++i)
        if (materialNames_[i] == name) return (int)i;
    materialNames_.push_back(name);
    materials_.push_back(tileOfBlock(PLANKS));
    return (int)materialNames_.size() - 1;
}

void TrainSystem::resetLook() {
    materialNames_.clear();
    materials_.clear();
    struct M { const char* name; ItemId block; };
    static const M defaults[] = {
        {"tie", SPRUCE_PLANKS},        {"rail", STEEL_BLOCK},          {"buffer", STEEL_BLOCK},
        {"wheel", WOOL_BLACK},         {"steam_body", WOOL_BLACK},     {"steam_boiler", WOOL_BLACK},
        {"steam_dark", WOOL_BLACK},    {"steam_trim", STEEL_BLOCK},    {"boxcar_body", PLANKS},
        {"boxcar_roof", SPRUCE_PLANKS}, {"boxcar_trim", STEEL_BLOCK},  {"passenger_body", (ItemId)(WOOL_ORANGE + 13)},
        {"passenger_roof", (ItemId)(WOOL_ORANGE + 6)}, {"passenger_window", GLASS}, {"passenger_trim", STEEL_BLOCK},
    };
    for (const M& m : defaults) setMaterial(m.name, tileOfBlock(m.block));
    defaultModels();
}

void TrainSystem::defaultModels() {
    auto B = [&](std::vector<ModelBox>& v, float x0, float y0, float z0, float x1, float y1, float z1, const char* mat) {
        v.push_back({x0, y0, z0, x1, y1, z1, materialIndex(mat)});
    };
    auto wheels = [&](std::vector<ModelBox>& v, std::initializer_list<float> zs) {
        for (float z : zs) {
            B(v, -0.5f, 0.0f, z - 0.12f, -0.4f, 0.26f, z + 0.12f, "wheel");
            B(v, 0.4f, 0.0f, z - 0.12f, 0.5f, 0.26f, z + 0.12f, "wheel");
        }
    };
    for (auto& m : models_) m.clear();
    // Steam engine: boiler in front, cab behind, chimney on top.
    auto& s = models_[(int)CarKind::Steam];
    B(s, -0.42f, 0.12f, -0.9f, 0.42f, 0.3f, 0.9f, "steam_dark");
    B(s, -0.3f, 0.3f, -0.9f, 0.3f, 0.86f, 0.2f, "steam_boiler");
    B(s, -0.1f, 0.86f, -0.75f, 0.1f, 1.2f, -0.55f, "steam_dark");
    B(s, -0.16f, 0.86f, -0.25f, 0.16f, 1.0f, 0.0f, "steam_trim");
    B(s, -0.42f, 0.3f, 0.2f, 0.42f, 1.25f, 0.9f, "steam_body");
    B(s, -0.48f, 1.25f, 0.15f, 0.48f, 1.33f, 0.95f, "steam_dark");
    B(s, -0.3f, 0.12f, -0.9f, 0.3f, 0.26f, -0.8f, "steam_trim");
    wheels(s, {-0.55f, 0.0f, 0.55f});
    // Boxcar: a closed wooden box with a sliding door strip on each side.
    auto& b = models_[(int)CarKind::Boxcar];
    B(b, -0.45f, 0.12f, -0.9f, 0.45f, 0.2f, 0.9f, "boxcar_trim");
    B(b, -0.45f, 0.2f, -0.88f, 0.45f, 1.1f, 0.88f, "boxcar_body");
    B(b, -0.5f, 1.1f, -0.92f, 0.5f, 1.18f, 0.92f, "boxcar_roof");
    B(b, 0.45f, 0.25f, -0.22f, 0.48f, 1.0f, 0.22f, "boxcar_trim");
    B(b, -0.48f, 0.25f, -0.22f, -0.45f, 1.0f, 0.22f, "boxcar_trim");
    wheels(b, {-0.55f, 0.55f});
    // Passenger car: coloured body with a row of windows.
    auto& p = models_[(int)CarKind::Passenger];
    B(p, -0.45f, 0.12f, -0.9f, 0.45f, 0.2f, 0.9f, "passenger_trim");
    B(p, -0.45f, 0.2f, -0.88f, 0.45f, 1.0f, 0.88f, "passenger_body");
    B(p, -0.5f, 1.0f, -0.92f, 0.5f, 1.1f, 0.92f, "passenger_roof");
    for (float z : {-0.6f, 0.0f, 0.6f}) {
        B(p, 0.45f, 0.55f, z - 0.22f, 0.47f, 0.88f, z + 0.22f, "passenger_window");
        B(p, -0.47f, 0.55f, z - 0.22f, -0.45f, 0.88f, z + 0.22f, "passenger_window");
    }
    wheels(p, {-0.55f, 0.55f});
}

bool TrainSystem::loadConfig(const std::string& text, const std::function<int(const std::string&)>& blockTile, std::string& error) {
    Json j;
    if (!Json::parse(text, j, error)) return false;
    if (!j.isObject()) {
        error = "trains.json musí být objekt";
        return false;
    }
    const Json& mats = j["materials"];
    if (mats.isObject()) {
        for (const std::string& k : mats.keys()) {
            int tile = blockTile ? blockTile(mats[k].str("")) : -1;
            if (tile < 0) {
                error = "neznámý blok pro materiál \"" + k + "\"";
                continue;  // the others still apply; the last problem is reported
            }
            setMaterial(k, tile);
        }
    }
    static const char* names[3] = {"steam", "boxcar", "passenger"};
    for (int i = 0; i < 3; ++i) {
        const Json& arr = j["models"][names[i]];
        if (!arr.isArray() || arr.size() == 0) continue;
        std::vector<ModelBox> boxes;
        for (size_t n = 0; n < arr.size(); ++n) {
            const Json& e = arr[n];
            float c[6];
            if (e["from"].size() == 3 && e["to"].size() == 3) {
                for (int k = 0; k < 3; ++k) c[k] = (float)e["from"][k].num(), c[3 + k] = (float)e["to"][k].num();
            } else if (e["origin"].size() == 3 && e["size"].size() == 3) {  // Bedrock/Blockbench style
                for (int k = 0; k < 3; ++k) c[k] = (float)e["origin"][k].num(), c[3 + k] = c[k] + (float)e["size"][k].num();
            } else {
                error = std::string("model ") + names[i] + ": box " + std::to_string(n) + " má špatné rozměry";
                continue;
            }
            for (int k = 0; k < 3; ++k)
                if (c[k] > c[3 + k]) std::swap(c[k], c[3 + k]);
            boxes.push_back({c[0], c[1], c[2], c[3], c[4], c[5], materialIndex(e["material"].str("steam_body"))});
        }
        if (!boxes.empty()) models_[i] = boxes;
    }
    return error.empty();
}

// ---------------------------------------------------------------------------------------------
// Rendering

void TrainSystem::buildVertices(int dim, const Vec3& eye, float maxDist, std::vector<Vertex>& out) const {
    out.clear();
    const float md2 = maxDist * maxDist;
    auto near = [&](float x, float z) { return (x - eye.x) * (x - eye.x) + (z - eye.z) * (z - eye.z) < md2; };
    const int tie = materials_[0], rail = materials_[1], buffer = materials_[2];
    const float light = 0.95f;

    // One stretch of straight track: `len` long, centred at `c`, heading `yaw`.
    auto stretch = [&](const Vec3& c, float yaw, float len) {
        const float h = len * 0.5f;
        emitBox(out, c, yaw, -0.30f, 0.06f, -h, -0.22f, kRailHeight, h, rail, light);
        emitBox(out, c, yaw, 0.22f, 0.06f, -h, 0.30f, kRailHeight, h, rail, light);
        int n = std::max(1, (int)std::lround(len / 0.25f));
        for (int i = 0; i < n; ++i) {
            float z = -h + (i + 0.5f) * len / n;
            emitBox(out, c, yaw, -0.4f, 0.0f, z - 0.07f, 0.4f, 0.06f, z + 0.07f, tie, light);
        }
    };
    for (const auto& kv : pieces_) {
        const TrackPiece& p = kv.second;
        if (p.dim != dim || !near(p.x + 0.5f, p.z + 0.5f)) continue;
        const Vec3 c(p.x + 0.5f, (float)p.y, p.z + 0.5f);
        switch (p.kind) {
            case PieceKind::Straight: stretch(c, yawOf((float)kDx[p.dir], (float)kDz[p.dir]), 1.0f); break;
            case PieceKind::Cross:
                stretch(c, 0.0f, 1.0f);
                stretch(c, kPi * 0.5f, 1.0f);
                break;
            case PieceKind::Curve: {
                const int a = p.dir & 3, b = (p.dir + 1) & 3;
                Vec3 A(0.5f * kDx[a], 0, 0.5f * kDz[a]), B(0.5f * kDx[b], 0, 0.5f * kDz[b]);
                const int n = 4;
                Vec3 prev = A;
                for (int i = 1; i <= n; ++i) {
                    float t = (float)i / n, u = 1 - t;
                    Vec3 q(u * u * A.x + t * t * B.x, 0, u * u * A.z + t * t * B.z);  // control point = centre (0, 0)
                    Vec3 d = q - prev;
                    float len = d.length();
                    if (len > 1e-4f) stretch(Vec3(c.x + (prev.x + q.x) * 0.5f, c.y, c.z + (prev.z + q.z) * 0.5f), yawOf(d.x, d.z), len + 0.03f);
                    prev = q;
                }
                break;
            }
            case PieceKind::Stop: {
                // Rail stub from the connected side, buffer beam at the far edge.
                const float yaw = yawOf((float)kDx[p.dir], (float)kDz[p.dir]);
                const float dx = (float)kDx[p.dir], dz = (float)kDz[p.dir];
                stretch(Vec3(c.x + dx * 0.05f, c.y, c.z + dz * 0.05f), yaw, 0.9f);
                const Vec3 end(c.x - dx * 0.46f, c.y, c.z - dz * 0.46f);
                emitBox(out, end, yaw, -0.4f, 0.0f, -0.04f, 0.4f, 0.55f, 0.04f, buffer, light);
                emitBox(out, end, yaw, -0.34f, 0.2f, 0.04f, -0.2f, 0.4f, 0.12f, buffer, light);
                emitBox(out, end, yaw, 0.2f, 0.2f, 0.04f, 0.34f, 0.4f, 0.12f, buffer, light);
                break;
            }
        }
    }
    for (const Train& t : trains_) {
        if (t.dim != dim) continue;
        for (const Car& c : t.cars) {
            if (!near(c.pos.x, c.pos.z)) continue;
            for (const ModelBox& m : models_[(int)c.kind])
                emitBox(out, c.pos, c.yaw, m.x0, m.y0, m.z0, m.x1, m.y1, m.z1,
                        materials_[std::clamp(m.material, 0, (int)materials_.size() - 1)], light);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// World files

bool TrainSystem::save(const std::string& folder) const {
    std::ostringstream o;
    for (const auto& kv : pieces_) {
        const TrackPiece& p = kv.second;
        o << "piece " << p.dim << ' ' << p.x << ' ' << p.y << ' ' << p.z << ' ' << (int)p.kind << ' ' << p.dir << '\n';
    }
    for (const Train& t : trains_) {
        o << "train " << t.dim << ' ' << (t.reversed ? 1 : 0) << ' ' << t.cars.size() << '\n';
        for (const Car& c : t.cars)
            o << "car " << (int)c.kind << ' ' << c.st.x << ' ' << c.st.y << ' ' << c.st.z << ' ' << c.st.entry << ' ' << c.st.t << '\n';
    }
    std::string path = folder + "/trains.dat";
    std::ofstream out(path + ".tmp", std::ios::trunc);
    out << o.str();
    out.close();
    return out && std::rename((path + ".tmp").c_str(), path.c_str()) == 0;
}

void TrainSystem::load(const std::string& folder) {
    pieces_.clear();
    trains_.clear();
    std::ifstream in(folder + "/trains.dat");
    std::string line;
    int carsLeft = 0;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string kind;
        ls >> kind;
        if (kind == "piece") {
            TrackPiece p;
            int k = 0;
            if (ls >> p.dim >> p.x >> p.y >> p.z >> k >> p.dir && k >= 0 && k <= 3 && (p.dim >= 0 && p.dim <= 3)) {
                p.kind = (PieceKind)k;
                p.dir &= 3;
                pieces_[{p.dim, p.x, p.y, p.z}] = p;
            }
        } else if (kind == "train") {
            Train t;
            int rev = 0, n = 0;
            if (ls >> t.dim >> rev >> n && (t.dim >= 0 && t.dim <= 3) && n > 0 && n < 64) {
                t.reversed = rev != 0;
                trains_.push_back(t);
                carsLeft = n;
            }
        } else if (kind == "car" && carsLeft > 0 && !trains_.empty()) {
            Car c;
            int k = 0;
            if (ls >> k >> c.st.x >> c.st.y >> c.st.z >> c.st.entry >> c.st.t && k >= 0 && k <= 2 && c.st.entry >= 0 && c.st.entry < 4) {
                c.kind = (CarKind)k;
                trains_.back().cars.push_back(c);
            }
            --carsLeft;
        }
    }
    for (Train& t : trains_) refreshCars(t);
    trains_.erase(std::remove_if(trains_.begin(), trains_.end(), [](const Train& t) { return t.cars.empty(); }), trains_.end());
}

}  // namespace vc

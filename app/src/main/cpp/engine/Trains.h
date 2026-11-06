#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Math.h"
#include "Mesher.h"
#include "World.h"

namespace vc {

// VoxelCraft Trains (2.1.0): a small railway system, written for the engine from scratch.
//
// Track is a layer of pieces on the ground (one piece per block cell, flat for now):
//   straight  two ports, opposite sides             (dir 0/2 = north-south, 1/3 = east-west)
//   curve     two ports on neighbouring sides       (dir = the first port, the second is dir+1)
//   cross     four ports, a train drives straight through
//   stop      one port: a buffer at the end of the line, the train stands in front of it
// Directions use the same numbering as doors: 0 north (-z), 1 east (+x), 2 south (+z), 3 west (-x).
//
// A train is a row of cars (steam engine, boxcar, passenger car). Every car sits on the track
// with its own state (cell, entry port, progress 0..1) and all cars move the same distance each
// tick, so the spacing stays exact and the train follows curves without any extra maths.
// Reversing flips every state and swaps head and tail.
enum class PieceKind : uint8_t { Straight = 0, Curve = 1, Cross = 2, Stop = 3 };
enum class CarKind : uint8_t { Steam = 0, Boxcar = 1, Passenger = 2 };

struct TrackPiece {
    int dim = 0, x = 0, y = 0, z = 0;
    PieceKind kind = PieceKind::Straight;
    int dir = 0;
};

// Where a car stands: inside `cell`, having entered through port `entry`, `t` of the way across.
struct TrackState {
    int x = 0, y = 0, z = 0;
    int entry = 0;
    float t = 0;
};

struct Car {
    CarKind kind = CarKind::Steam;
    TrackState st;
    Vec3 pos;      // world position (centre of the bottom), filled by update()
    float yaw = 0; // like Player::yaw
};

struct Train {
    int dim = 0;
    std::vector<Car> cars;  // cars[0] is the front while !reversed
    bool reversed = false;  // the tail leads
    float speed = 0;        // current speed in blocks per second (always >= 0)
    float target = 0;       // wanted speed, set with /train go
    bool blocked = false;   // standing in front of a buffer or the end of the track
};

// A box of a model: local x across, y up, z along the car (forward is -z), in blocks.
struct ModelBox {
    float x0, y0, z0, x1, y1, z1;
    int material;  // index into TrainSystem::materials
};

class TrainSystem {
public:
    static constexpr float kCarSpacing = 4.5f;   // distance between car centres (Minecraft train scale: cars ~4.4 blocks long)
    static constexpr float kMaxSpeed = 14.0f;    // blocks per second
    static constexpr float kAccel = 3.0f;
    static constexpr float kRailHeight = 0.12f;  // the track is this thick

    TrainSystem();

    // --- Track ---------------------------------------------------------------
    // Fails (false) when the cell is not free or has nothing solid below, `why` says what.
    bool placePiece(const World& world, int dim, int x, int y, int z, PieceKind kind, int dir, std::string* why = nullptr);
    bool removePiece(int dim, int x, int y, int z);
    const TrackPiece* piece(int dim, int x, int y, int z) const;
    size_t pieceCount() const { return pieces_.size(); }

    // --- Trains --------------------------------------------------------------
    // Puts a train on the track at the cell, facing `dir`, one car per entry of `kinds`
    // (front first). Returns the index of the new train, -1 when there is no track.
    int spawnTrain(int dim, int x, int y, int z, int dir, const std::vector<CarKind>& kinds, std::string* why = nullptr);
    void removeTrain(int index);
    int addCar(int trainIndex, CarKind kind, std::string* why = nullptr);
    bool removeCar(int trainIndex, CarKind* removedKind = nullptr);
    int nearestTrain(int dim, const Vec3& p, float maxDist) const;
    void setTarget(int index, float speed);  // 0 = brake
    void reverse(int index);
    Vec3 frontPosition(int index) const;     // where a rider sits

    // Moves all trains of `dim`. `supported(x, y, z)` says whether a piece at that cell still has
    // ground below it (pieces over broken blocks are dropped).
    void update(int dim, float dt, const std::function<bool(int, int, int)>& supported);
    void update(const World& world, int dim, float dt);

    // --- Collision -----------------------------------------------------------
    // Returns true if the given AABB overlaps any car in .
    // If ignoreTrainIdx >= 0, that train is ignored (e.g. the train the player is riding).
    bool collides(int dim, const Vec3& pMin, const Vec3& pMax, int ignoreTrainIdx = -1) const;
    // Returns push vector if  overlaps a train car, to push out players hit by moving trains.
    Vec3 collisionPush(int dim, const Vec3& p, float radius, float height, int ignoreTrainIdx = -1) const;

    // Geometry for the renderer: textured quads in the terrain atlas, like boats.
    void buildVertices(int dim, const Vec3& eye, float maxDist, std::vector<Vertex>& out) const;

    // --- Look ------------------------------------------------------------------
    // Materials are atlas tiles by name ("tie", "rail", "steam_body", ...). A pack can replace
    // them and the car models with a trains.json (see loadConfig).
    void setMaterial(const std::string& name, int tile);
    int materialIndex(const std::string& name);
    const std::vector<ModelBox>& model(CarKind k) const { return models_[(int)k]; }
    // `json` is the text of trains.json. `blockTile(key)` turns "namespace:block" or a vanilla
    // block key into an atlas tile (-1 = unknown). Returns false and fills `error` on bad input.
    bool loadConfig(const std::string& json, const std::function<int(const std::string&)>& blockTile, std::string& error);
    void resetLook();
    // trains.json texts of the enabled packs, filled by Game::rebuildFromPacks, applied by Session.
    static std::vector<std::string>& packConfigs();

    // --- World files -------------------------------------------------------------
    bool save(const std::string& folder) const;
    void load(const std::string& folder);

    const std::vector<Train>& trains() const { return trains_; }
    std::vector<Train>& trains() { return trains_; }

    // Exposed for tests.
    static void ports(PieceKind kind, int dir, bool out[4]);
    bool advance(Train& t, TrackState& s, float dist, float* moved) const;  // false = ran out of track
    Vec3 worldPos(int dim, const TrackState& s, float* yaw) const;

private:
    struct Key {
        int dim, x, y, z;
        bool operator==(const Key& o) const { return dim == o.dim && x == o.x && y == o.y && z == o.z; }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            size_t h = (size_t)(k.x * 73856093) ^ (size_t)(k.y * 19349663) ^ (size_t)(k.z * 83492791) ^ (size_t)(k.dim * 2654435761u);
            return h;
        }
    };
    const TrackPiece* pieceFor(int dim, const TrackState& s) const;
    bool step(int dim, TrackState& s, float dist, float& moved) const;
    TrackState flipped(int dim, const TrackState& s) const;
    void refreshCars(Train& t) const;
    void defaultModels();

    std::unordered_map<Key, TrackPiece, KeyHash> pieces_;
    std::vector<Train> trains_;
    std::vector<std::string> materialNames_;
    std::vector<int> materials_;  // atlas tile per material
    std::vector<ModelBox> models_[3];
    float time_ = 0;
};

}  // namespace vc

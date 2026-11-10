#pragma once
#include <string>
#include <vector>

#include "Math.h"
#include "Mesher.h"
#include "World.h"

namespace vc {

// 2.1.0 preview (the rails experiment): trains. A train is a locomotive with wagons behind it that
// runs along the rail blocks (straight rails, curves, switches) and stops at a buffer stop or where
// the track ends. The models come from assets_src/trains (tools/train_models.py), drawn bigger than
// the originals so people fit inside: the cars are solid (floor, walls, roof) and have a door in
// each side, so players can walk in. The driver sits in the locomotive and drives with the throttle
// and the brake.
enum class CarKind { Locomotive = 0, Passenger = 1, Freight = 2 };

// A place on the track: a rail cell, the side the train came in by and the side it leaves by
// (0 north, 1 east, 2 south, 3 west), and how far through the cell it is (0..1).
struct RailPos {
    int x = 0, y = 0, z = 0;
    int in = 2, out = 0;
    float t = 0.5f;
};

struct TrainCar {
    CarKind kind = CarKind::Locomotive;
    RailPos at;      // the middle of the car
    Vec3 pos;        // world position of the middle at rail-head level (updated from `at`)
    float yaw = 0;   // the model's front (+z) points along the travel direction
};

struct Train {
    int dim = 0;
    std::vector<TrainCar> cars;  // cars[0] leads when going forward
    float speed = 0;     // blocks per second, negative backwards
    float throttle = 0;  // -1..1: the speed the driver asks for (a fraction of kMaxSpeed)
    bool brake = false;
};

class TrainSystem {
public:
    static constexpr float kScale = 1.6f;                 // model units to blocks
    static constexpr float kPitch = 4.45f * kScale;       // from the middle of one car to the next
    static constexpr float kMaxSpeed = 12.0f, kAccel = 2.5f, kBrake = 7.0f;

    // Puts a car on the rail at (x, y, z): behind the last car of a train whose end is close,
    // or as a new train heading the way `look` points (0 north...). False when it does not fit.
    bool place(const World& world, int dim, CarKind kind, int x, int y, int z, int look);
    // Moves the trains of `dim`. The player standing in or on a car is carried with it.
    void update(const World& world, int dim, float dt, Vec3& playerPos, float& playerYaw, bool playerSeated);
    // Pushes the player (feet at `pos`, 0.3 radius, 1.8 high) out of the cars; true when they stand on one.
    bool collide(int dim, Vec3& pos, Vec3& vel) const;
    // The car the ray hits first, -1 for none (index = train * 64 + car).
    int pick(int dim, const Vec3& origin, const Vec3& dir, float maxDist, float& dist) const;
    // The nearest car within `range` of `pos`: train and car index; false when none.
    bool nearest(int dim, const Vec3& pos, float range, int& train, int& car) const;
    // Seats: where a sitting player's feet go, -1 seats for a car without any.
    int seatCount(CarKind kind) const;
    Vec3 seatPos(const TrainCar& car, int seat) const;
    void remove(int train, int car);  // the car and everything behind it is taken off
    void removeCar(int train, int car);  // just that car (the train ends when it was the last one)
    void buildVertices(int dim, std::vector<Vertex>& out) const;

    std::vector<Train>& trains() { return trains_; }
    const std::vector<Train>& trains() const { return trains_; }
    bool save(const std::string& folder) const;
    void load(const std::string& folder);

    // Track geometry (also used by tests).
    static bool railSides(ItemId id, int& a, int& b);                    // the two ends of a plain piece
    static bool nextSide(ItemId id, int in, int& out);                    // where a train entering by `in` leaves
    static Vec3 pointOn(const RailPos& p, Vec3* tangent = nullptr);       // world position (rail head) and direction
    static bool advance(const World& world, RailPos& p, float dist, float& moved);  // false when it hit the end
    static void flip(RailPos& p);

private:
    std::vector<Train> trains_;
    void refresh(TrainCar& car) const;
};

}  // namespace vc

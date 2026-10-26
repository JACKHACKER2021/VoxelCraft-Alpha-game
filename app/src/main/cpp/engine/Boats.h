#pragma once
#include <string>
#include <vector>

#include "Math.h"
#include "Mesher.h"
#include "World.h"

namespace vc {

// Rowing boats: placed from the boat item, they float on water and the player
// rides them with the D-pad (forward is where the camera looks).
struct Boat {
    int dim = 0;
    Vec3 pos;           // centre of the bottom
    float yaw = 0;      // heading, radians (like Player::yaw)
    Vec3 vel;
    float damage = 0;   // seconds of hitting it (it breaks at kBreakTime)
};

class BoatSystem {
public:
    static constexpr float kHalfLength = 0.7f, kHalfWidth = 0.45f, kHeight = 0.55f;
    static constexpr float kBreakTime = 0.6f;

    int spawn(int dim, const Vec3& pos, float yaw);
    void remove(int index);
    // Moves all boats of `dim`: floating, falling, the ridden one steered by `moveForward`
    // (-1..1) and `moveSide` towards `heading`.
    void update(const World& world, int dim, float dt, int ridden, float moveForward, float moveSide, float heading);
    // The boat the ray hits first (and how far), -1 for none.
    int pick(int dim, const Vec3& origin, const Vec3& dir, float maxDist, float& dist) const;
    void buildVertices(int dim, std::vector<Vertex>& out) const;

    std::vector<Boat>& boats() { return boats_; }
    const std::vector<Boat>& boats() const { return boats_; }
    bool save(const std::string& folder) const;
    void load(const std::string& folder);

private:
    std::vector<Boat> boats_;
};

}  // namespace vc

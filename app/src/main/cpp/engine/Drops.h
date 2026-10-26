#pragma once
#include <string>
#include <vector>

#include "Items.h"
#include "Math.h"
#include "Mesher.h"
#include "World.h"

namespace vc {

// Items lying in the world: what did not fit into a full inventory, what the
// player threw away. They fall, rest on the ground, are picked up when the
// player walks close and there is room, and are saved with the world, so no
// item ever disappears silently. After kLifetime seconds they despawn like in
// the classic game.
struct DroppedItem {
    int dim = 0;  // 0 overworld, 1 nether
    Vec3 pos, vel;
    ItemStack stack;
    float age = 0;
};

class DropSystem {
public:
    static constexpr float kLifetime = 300;     // seconds
    static constexpr float kPickupDelay = 1.0f;  // thrown items are not caught again right away

    void spawn(int dim, const Vec3& pos, const Vec3& vel, const ItemStack& stack);
    // Physics for the drops of `dim`, then picks up what reaches `player` (feet position).
    // Returns the number of items that went into the inventory.
    int update(const World& world, int dim, const Vec3& player, Inventory& inventory, float dt);
    // Small textured cubes (blocks) or flat icons (items) for the drops of `dim`.
    void buildVertices(int dim, float time, std::vector<Vertex>& out) const;

    const std::vector<DroppedItem>& drops() const { return drops_; }
    int itemCount(int dim) const;
    bool save(const std::string& folder) const;
    void load(const std::string& folder);

private:
    std::vector<DroppedItem> drops_;
};

}  // namespace vc

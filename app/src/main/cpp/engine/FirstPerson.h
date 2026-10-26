#pragma once
#include <cstdint>
#include <vector>

#include "Mesher.h"
#include "Blocks.h"

namespace vc {

// The player's right hand in first person and what it holds. HandAnimator only
// keeps timers; Session feeds it real events (digging, a block really placed, the
// selected slot), so the hand never shows something the player does not have.
class HandAnimator {
public:
    static constexpr float kSwingTime = 0.3f;   // one punch / mining stroke
    static constexpr float kPlaceTime = 0.22f;  // short push forward when a block is placed
    static constexpr float kEquipSpeed = 7.0f;  // lowering + raising when the held item changes

    // `held` is the item in the selected slot right now (AIR for an empty hand).
    void update(float dt, float walkSpeed, bool onGround, bool mining, ItemId held, int slot);
    void swing();  // a punch: restarts only when the previous one is past half way
    void place();  // a block was placed
    void use();    // an item was used (eating, flint and steel)

    // State for drawing and tests.
    ItemId shown() const { return shown_; }       // what the hand displays now
    float swingProgress() const { return swing_; }  // 0..1, 1 = at rest
    float placeProgress() const { return place_; }
    float equip() const { return equip_; }          // 0 = raised, 1 = fully lowered
    float bob() const { return bobAmount_; }
    float bobPhase() const { return bobPhase_; }
    float time() const { return time_; }

private:
    ItemId shown_ = 0;
    int shownSlot_ = -1;
    bool lowering_ = false;
    float swing_ = 1, place_ = 1, equip_ = 0;
    float bobPhase_ = 0, bobAmount_ = 0, time_ = 0;
};

// Geometry of the hand in view space (x right, y up, looking along -z), ready for a
// projection of its own. Skin-textured parts go to `skin`, blocks and items (terrain
// atlas) to `atlas`. `light` darkens the hand in caves and at night.
void buildFirstPerson(const HandAnimator& hand, float light, std::vector<Vertex>& skin, std::vector<Vertex>& atlas);

}  // namespace vc
